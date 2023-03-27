/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AlarmDB } from "resource://gre/modules/AlarmDB.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const DEBUG = Services.prefs.getBoolPref("dom.alarm.debug", false);
function debug(aMsg) {
  console.log(`AlarmService: ${aMsg}`);
}

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "systemmessenger", function() {
  return Cc["@mozilla.org/systemmessage-service;1"].getService(
    Ci.nsISystemMessageService
  );
});

XPCOMUtils.defineLazyGetter(lazy, "powerManagerService", function() {
  return Cc["@mozilla.org/power/powermanagerservice;1"].getService(
    Ci.nsIPowerManagerService
  );
});

XPCOMUtils.defineLazyGetter(lazy, "timeService", function() {
  return Cc["@mozilla.org/sidl-native/time;1"].getService(Ci.nsITime);
});

/**
 * AlarmService provides an API to schedule alarms using the device's RTC.
 *
 * AlarmService is primarily used by the Alarm API,
 * which uses IPC to communicate with the service.
 *
 * AlarmService can also be used by Gecko code by importing the module and then
 * using AlarmService.add() and AlarmService.remove(). Only Gecko code running
 * in the parent process should do this.
 */

export const AlarmService = {
  lastChromeId: 0,

  init: function init() {
    DEBUG && debug("init()");

    Services.obs.addObserver(this, "b2g-sw-registration-done");
    Services.obs.addObserver(this, "profile-change-teardown");

    this._currentTimezoneOffset = new Date().getTimezoneOffset();

    let alarmHalService = (this._alarmHalService = Cc[
      "@mozilla.org/alarmHalService;1"
    ].getService(Ci.nsIAlarmHalService));

    alarmHalService.setAlarmFiredCb(this._onAlarmFired.bind(this));

    lazy.timeService.addObserver(
      lazy.timeService.TIME_CHANGED,
      {
        notify: aTimeInfo => {
          DEBUG &&
            debug(
              `TIME_CHANGED ${aTimeInfo?.reason} ${aTimeInfo?.delta} ${aTimeInfo?.timezone}`
            );
          lazy.systemmessenger.broadcastMessage("system-time-change", {
            reason: aTimeInfo?.reason,
            delta: aTimeInfo?.delta,
            timezone: aTimeInfo?.timezone,
          });
        },
      },
      {
        resolve: () => DEBUG && debug("resolve: addObserver on TIME_CHANGED"),
        reject: () => DEBUG && debug("reject: addObserver on TIME_CHANGED"),
      }
    );

    let timezoneChangeCallback = this._onTimezoneChanged.bind(this);
    lazy.timeService.addObserver(
      lazy.timeService.TIMEZONE_CHANGED,
      {
        notify: timezoneChangeCallback,
      },
      {
        resolve: () =>
          DEBUG && debug("resolve: addObserver on TIMEZONE_CHANGED"),
        reject: () =>
          DEBUG && debug("resolve: addObserver on TIMEZONE_CHANGED"),
      }
    );

    // Add the messages to be listened to.
    this._messages = ["Alarm:GetAll", "Alarm:Add", "Alarm:Remove"];
    this._messages.forEach(
      function addMessage(msgName) {
        Services.ppmm.addMessageListener(msgName, this);
      }.bind(this)
    );

    // Set the indexeddb database.
    this._db = new AlarmDB();
    this._db.init();

    // Variable to save alarms waiting to be set.
    this._alarmQueue = [];

    // Wait until `b2g-sw-registration-done` to restore alarms from DB,
    // to make sure service workers are ready to catch fired alarms.
  },

  // Getter/setter to access the current alarm set in system.
  _alarm: null,
  get _currentAlarm() {
    return this._alarm;
  },
  set _currentAlarm(aAlarm) {
    this._alarm = aAlarm;
    if (!aAlarm) {
      return;
    }

    let alarmTimeInMs = this._getAlarmTime(aAlarm);
    let ns = (alarmTimeInMs % 1000) * 1000000;
    if (!this._alarmHalService.setAlarm(alarmTimeInMs / 1000, ns)) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    DEBUG && debug("receiveMessage(): " + aMessage.name);
    let json = aMessage.json;
    let mm = aMessage.target;

    switch (aMessage.name) {
      case "Alarm:GetAll":
        this._db.getAll(
          json.url,
          function getAllSuccessCb(aAlarms) {
            DEBUG &&
              debug(
                "Callback after getting alarms from database: " +
                  JSON.stringify(aAlarms)
              );

            this._sendAsyncMessage(mm, "GetAll", true, aAlarms);
          }.bind(this),
          function getAllErrorCb(aErrorMsg) {
            this._sendAsyncMessage(mm, "GetAll", false, aErrorMsg);
          }.bind(this)
        );
        break;

      case "Alarm:Add":
        // Prepare a record for the new alarm to be added.
        let newAlarm = {
          date: json.date,
          ignoreTimezone: json.ignoreTimezone,
          data: json.data,
          url: json.url,
        };

        this.add(
          newAlarm,
          null,
          // Receives the alarm ID as the last argument.
          this._sendAsyncMessage.bind(this, mm, "Add", true),
          // Receives the error message as the last argument.
          this._sendAsyncMessage.bind(this, mm, "Add", false)
        );
        break;

      case "Alarm:Remove":
        this.remove(json.id, json.url);
        break;

      default:
        throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    }
  },

  _sendAsyncMessage: function _sendAsyncMessage(
    aMessageManager,
    aAction,
    aSuccess,
    aData
  ) {
    DEBUG && debug(`_sendAsyncMessage ${aAction} ${aSuccess}`);

    if (!aMessageManager) {
      DEBUG && debug("Invalid message manager: null");
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    let json = null;
    switch (aAction) {
      case "Add":
        json = aSuccess
          ? { id: aData, success: true }
          : { errorMsg: aData, success: false };
        break;

      case "GetAll":
        json = aSuccess
          ? { alarms: aData, success: true }
          : { errorMsg: aData, success: false };
        break;

      default:
        throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    }

    let msgName = `Alarm:${aAction}:Return`;
    DEBUG && debug(`sendAsyncMessage ${msgName} ${JSON.stringify(json)}`);
    aMessageManager.sendAsyncMessage(msgName, json);
  },

  _removeAlarmFromDb: function _removeAlarmFromDb(aId, aUrl, aRemoveSuccessCb) {
    DEBUG && debug("_removeAlarmFromDb()");

    // If the aRemoveSuccessCb is undefined or null, set a dummy callback for
    // it which is needed for _db.remove().
    if (!aRemoveSuccessCb) {
      aRemoveSuccessCb = function removeSuccessCb() {
        DEBUG && debug("Remove alarm from DB successfully.");
      };
    }

    // Is this a chrome alarm?
    if (aId < 0) {
      aRemoveSuccessCb();
      return;
    }

    this._db.remove(aId, aUrl, aRemoveSuccessCb, function removeErrorCb(
      aErrorMsg
    ) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    });
  },

  /**
   * Create a copy of the alarm that does not expose internal fields to
   * receivers and sticks to the public |respectTimezone| API rather than the
   * boolean |ignoreTimezone| field.
   */
  _publicAlarm: function _publicAlarm(aAlarm) {
    let alarm = {
      id: aAlarm.id,
      date: aAlarm.date,
      ignoreTimezone: aAlarm.ignoreTimezone,
      data: aAlarm.data,
    };

    return alarm;
  },

  _fireSystemMessage: function _fireSystemMessage(aAlarm) {
    DEBUG && debug("_fireSystemMessage: " + JSON.stringify(aAlarm));
    try {
      // Use try here in case newURI fails on invalid url.
      let origin = Services.io.newURI(aAlarm.url).prePath;
      DEBUG && debug("sendMessage to " + origin);
      lazy.systemmessenger.sendMessage(
        "alarm",
        this._publicAlarm(aAlarm),
        origin
      );
    } catch (err) {
      DEBUG && debug("sendMessage failed. " + err);
    }
  },

  _notifyAlarmObserver: function _notifyAlarmObserver(aAlarm) {
    DEBUG && debug("_notifyAlarmObserver()");

    let wakeLock = lazy.powerManagerService.newWakeLock("cpu");

    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      () => {
        DEBUG && debug("_notifyAlarmObserver - timeout()");
        if (aAlarm.url) {
          this._fireSystemMessage(aAlarm);
        } else if (typeof aAlarm.alarmFiredCb === "function") {
          aAlarm.alarmFiredCb(this._publicAlarm(aAlarm));
        }

        wakeLock.unlock();
      },
      0,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _onAlarmFired: function _onAlarmFired() {
    DEBUG && debug("_onAlarmFired()");

    if (this._currentAlarm) {
      let currentAlarmTime = this._getAlarmTime(this._currentAlarm);

      // If a alarm fired before the actual time that the current
      // alarm should occur, we reset this current alarm.
      if (currentAlarmTime > Date.now()) {
        let currentAlarm = this._currentAlarm;
        this._currentAlarm = currentAlarm;

        this._debugCurrentAlarm();
        return;
      }

      this._removeAlarmFromDb(this._currentAlarm.id, null);
      // We need to clear the current alarm before notifying because chrome
      // alarms may add a new alarm during their callback, and we do not want
      // to clobber it.
      let firingAlarm = this._currentAlarm;
      this._currentAlarm = null;
      this._notifyAlarmObserver(firingAlarm);
    }

    // Reset the next alarm from the queue.
    let alarmQueue = this._alarmQueue;
    while (alarmQueue.length) {
      let nextAlarm = alarmQueue.shift();
      let nextAlarmTime = this._getAlarmTime(nextAlarm);

      // If the next alarm has been expired, directly notify the observer.
      // it instead of setting it.
      if (nextAlarmTime <= Date.now()) {
        this._removeAlarmFromDb(nextAlarm.id, null);
        this._notifyAlarmObserver(nextAlarm);
      } else {
        this._currentAlarm = nextAlarm;
        break;
      }
    }

    this._debugCurrentAlarm();
  },

  _onTimezoneChanged: function _onTimezoneChanged() {
    DEBUG && debug("_onTimezoneChanged()");
    this._currentTimezoneOffset = new Date().getTimezoneOffset();
    this._restoreAlarmsFromDb();
  },

  _restoreAlarmsFromDb: function _restoreAlarmsFromDb() {
    DEBUG && debug("_restoreAlarmsFromDb()");

    this._db.getAll(
      null,
      function getAllSuccessCb(aAlarms) {
        DEBUG &&
          debug(
            "Callback after getting alarms from database: " +
              JSON.stringify(aAlarms)
          );

        // Clear any alarms set or queued in the cache if coming from db.
        let alarmQueue = this._alarmQueue;
        if (this._currentAlarm) {
          alarmQueue.unshift(this._currentAlarm);
          this._currentAlarm = null;
        }
        for (let i = 0; i < alarmQueue.length; ) {
          if (alarmQueue[i].id < 0) {
            ++i;
            continue;
          }
          alarmQueue.splice(i, 1);
        }

        // Only restore the alarm that's not yet expired; otherwise, remove it
        // from the database and notify the observer.
        aAlarms.forEach(
          function addAlarm(aAlarm) {
            if (
              "url" in aAlarm &&
              aAlarm.url &&
              this._getAlarmTime(aAlarm) > Date.now()
            ) {
              alarmQueue.push(aAlarm);
            } else {
              this._removeAlarmFromDb(aAlarm.id, null);
              this._notifyAlarmObserver(aAlarm);
            }
          }.bind(this)
        );

        // Set the next alarm from the queue.
        if (alarmQueue.length) {
          alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
          this._currentAlarm = alarmQueue.shift();
        }

        this._debugCurrentAlarm();
      }.bind(this),
      function getAllErrorCb(aErrorMsg) {
        throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
      }
    );
  },

  _getAlarmTime: function _getAlarmTime(aAlarm) {
    // Avoid casting a Date object to a Date again to
    // preserve milliseconds. See bug 810973.
    let alarmTime;
    if (aAlarm.date instanceof Date) {
      alarmTime = aAlarm.date.getTime();
    } else {
      alarmTime = new Date(aAlarm.date).getTime();
    }

    // For an alarm specified with "ignoreTimezone", it must be fired respect
    // to the user's timezone.  Supposing an alarm was set at 7:00pm at Tokyo,
    // it must be gone off at 7:00pm respect to Paris' local time when the user
    // is located at Paris.  We can adjust the alarm UTC time by calculating
    // the difference of the orginal timezone and the current timezone.
    if (aAlarm.ignoreTimezone) {
      alarmTime +=
        (this._currentTimezoneOffset - aAlarm.timezoneOffset) * 60000;
    }
    return alarmTime;
  },

  _sortAlarmByTimeStamps: function _sortAlarmByTimeStamps(aAlarm1, aAlarm2) {
    return this._getAlarmTime(aAlarm1) - this._getAlarmTime(aAlarm2);
  },

  _debugCurrentAlarm: function _debugCurrentAlarm() {
    DEBUG && debug("Current alarm: " + JSON.stringify(this._currentAlarm));
    DEBUG && debug("Alarm queue: " + JSON.stringify(this._alarmQueue));
  },

  /**
   *
   * Add a new alarm. This will set the RTC to fire at the selected date and
   * notify the caller. Notifications are delivered via System Messages if the
   * alarm is added on behalf of a app. Otherwise aAlarmFiredCb is called.
   *
   * @param object aNewAlarm
   *        Should contain the following literal properties:
   *          - |date| date: when the alarm should timeout.
   *          - |ignoreTimezone| boolean: See [1] for the details.
   *          - |url| string: Url of app on whose behalf the alarm
   *                                  is added.
   *          - |data| object [optional]: Data that can be stored in DB.
   * @param function aAlarmFiredCb
   *        Callback function invoked when the alarm is fired.
   *        It receives a single argument, the alarm object.
   *        May be null.
   * @param function aSuccessCb
   *        Callback function to receive an alarm ID (number).
   * @param function aErrorCb
   *        Callback function to receive an error message (string).
   * @returns void
   *
   * Notes:
   * [1] https://wiki.mozilla.org/WebAPI/AlarmAPI#Proposed_API
   */

  add(aNewAlarm, aAlarmFiredCb, aSuccessCb, aErrorCb) {
    DEBUG && debug("add " + JSON.stringify(aNewAlarm));

    aSuccessCb = aSuccessCb || function() {};
    aErrorCb = aErrorCb || function() {};

    if (!aNewAlarm) {
      aErrorCb("alarm is null");
      return;
    }

    if (!aNewAlarm.date) {
      aErrorCb("alarm.date is null");
      return;
    }

    aNewAlarm.timezoneOffset = this._currentTimezoneOffset;

    if ("url" in aNewAlarm) {
      this._db.add(
        aNewAlarm,
        function addSuccessCb(aNewId) {
          DEBUG && debug("Callback after adding alarm in database.");
          this.processNewAlarm(aNewAlarm, aNewId, aAlarmFiredCb, aSuccessCb);
        }.bind(this),
        function addErrorCb(aErrorMsg) {
          aErrorCb(aErrorMsg);
        }
      );
    } else {
      // alarms without urls are managed by chrome code. For them we use
      // negative IDs.
      this.processNewAlarm(
        aNewAlarm,
        --this.lastChromeId,
        aAlarmFiredCb,
        aSuccessCb
      );
    }
  },

  processNewAlarm(aNewAlarm, aNewId, aAlarmFiredCb, aSuccessCb) {
    DEBUG &&
      debug("processNewAlarm " + JSON.stringify(aNewAlarm) + " " + aNewId);
    aNewAlarm.id = aNewId;

    // Now that the alarm has been added to the database, we can tack on
    // the non-serializable callback to the in-memory object.
    aNewAlarm.alarmFiredCb = aAlarmFiredCb;

    // If the new alarm already expired at this moment, we directly
    // notify this alarm
    let newAlarmTime = this._getAlarmTime(aNewAlarm);
    if (newAlarmTime < Date.now()) {
      DEBUG && debug("The new alarm already expired.");
      aSuccessCb(aNewId);
      this._removeAlarmFromDb(aNewAlarm.id, null);
      this._notifyAlarmObserver(aNewAlarm);
      return;
    }

    // If there is no alarm being set in system, set the new alarm.
    if (this._currentAlarm == null) {
      DEBUG && debug("No alarm, set a new alarm.");
      this._currentAlarm = aNewAlarm;
      this._debugCurrentAlarm();
      aSuccessCb(aNewId);
      return;
    }

    // If the new alarm is earlier than the current alarm, swap them and
    // push the previous alarm back to the queue.
    let alarmQueue = this._alarmQueue;
    let currentAlarmTime = this._getAlarmTime(this._currentAlarm);
    if (newAlarmTime < currentAlarmTime) {
      alarmQueue.unshift(this._currentAlarm);
      this._currentAlarm = aNewAlarm;
      this._debugCurrentAlarm();
      aSuccessCb(aNewId);
      return;
    }

    // Push the new alarm in the queue.
    alarmQueue.push(aNewAlarm);
    alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
    this._debugCurrentAlarm();
    aSuccessCb(aNewId);
  },

  /*
   * Remove the alarms associated with the url.
   *
   * @param string aUrl
   *        Url for application which added the alarm.
   * @returns void
   */
  removeByHost(aUrl) {
    DEBUG && debug("removeByHost(" + aUrl + ")");

    // Passing null to AlarmDB.getAll() returns all alarms directly.
    if (!aUrl) {
      return;
    }

    this._db.getAll(
      aUrl,
      function getAllSuccessCb(aAlarms) {
        aAlarms.forEach(function removeAlarm(aAlarm) {
          this.remove(aAlarm.id, aUrl);
        }, this);
      }.bind(this),
      function getAllErrorCb(aErrorMsg) {
        throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
      }
    );
  },

  /*
   * Remove the alarm associated with an ID.
   *
   * @param number aAlarmId
   *        The ID of the alarm to be removed.
   * @param string aUrl
   *        Url for application which added the alarm. (Optional)
   * @returns void
   */
  remove(aAlarmId, aUrl) {
    DEBUG && debug("remove(" + aAlarmId + ", " + aUrl + ")");

    this._removeAlarmFromDb(
      aAlarmId,
      aUrl,
      function removeSuccessCb() {
        DEBUG && debug("Callback after removing alarm from database.");

        // If there are no alarms set, nothing to do.
        if (!this._currentAlarm) {
          DEBUG && debug("No alarms set.");
          return;
        }

        // Check if the alarm to be removed is in the queue and whether it
        // belongs to the requesting app.
        let alarmQueue = this._alarmQueue;
        if (
          this._currentAlarm.id != aAlarmId ||
          this._currentAlarm.url != aUrl
        ) {
          for (let i = 0; i < alarmQueue.length; i++) {
            if (alarmQueue[i].id == aAlarmId && alarmQueue[i].url == aUrl) {
              alarmQueue.splice(i, 1);
              break;
            }
          }
          this._debugCurrentAlarm();
          return;
        }

        // The alarm to be removed is the current alarm reset the next alarm
        // from the queue if any.
        if (alarmQueue.length) {
          this._currentAlarm = alarmQueue.shift();
          this._debugCurrentAlarm();
          return;
        }

        // No alarm waiting to be set in the queue.
        this._currentAlarm = null;
        this._debugCurrentAlarm();
      }.bind(this)
    );
  },

  observe(aSubject, aTopic, aData) {
    DEBUG && debug("observe(): " + aTopic);

    switch (aTopic) {
      case "b2g-sw-registration-done":
        Services.obs.removeObserver(this, "b2g-sw-registration-done");
        DEBUG && debug("restore alarms from db.");
        this._restoreAlarmsFromDb();
        break;
      case "profile-change-teardown":
        this.uninit();
        break;
    }
  },

  uninit: function uninit() {
    DEBUG && debug("uninit()");

    Services.obs.removeObserver(this, "profile-change-teardown");

    this._messages.forEach(
      function(aMsgName) {
        Services.ppmm.removeMessageListener(aMsgName, this);
      }.bind(this)
    );

    if (this._db) {
      this._db.close();
    }
    this._db = null;

    this._alarmHalService = null;
  },
};

AlarmService.init();
