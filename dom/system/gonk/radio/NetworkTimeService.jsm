/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Sntp } = ChromeUtils.import("resource://gre/modules/Sntp.jsm");

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gTime",
  "@mozilla.org/sidl-native/time;1",
  "nsITime"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkManager",
  "@mozilla.org/network/manager;1",
  "nsINetworkManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

const NETWORKTIMESERVICE_CID = Components.ID(
  "{08e5d35e-40fc-4404-ad42-b6c5efa59d68}"
);

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";
const kNetworkActiveChangedTopic = "network-active-changed";

const OBSERVER_TOPICS_ARRAY = [
  NS_XPCOM_SHUTDOWN_OBSERVER_ID,
  kNetworkActiveChangedTopic,
];

const kSettingsClockAutoUpdateEnabled = "time.clock.automatic-update.enabled";
const kSettingsClockAutoUpdateAvailable =
  "time.clock.automatic-update.available";
const kSettingsTimezoneAutoUpdateEnabled =
  "time.timezone.automatic-update.enabled";
const kSettingsTimezoneAutoUpdateAvailable =
  "time.timezone.automatic-update.available";
const kSettingsDataDefaultServiceId = "ril.data.defaultServiceId";

const SETTING_KEYS_ARRAY = [
  kSettingsClockAutoUpdateEnabled,
  kSettingsClockAutoUpdateAvailable,
  kSettingsTimezoneAutoUpdateEnabled,
  kSettingsTimezoneAutoUpdateAvailable,
  kSettingsDataDefaultServiceId,
];

const NETWORK_TYPE_WIFI = Ci.nsINetworkInfo.NETWORK_TYPE_WIFI;
const NETWORK_TYPE_MOBILE = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE;

const INVALID_UPTIME = undefined;

var DEBUG;
function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref;
  try {
    debugPref =
      debugPref || Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
  } catch (e) {
    debugPref = false;
  }
  DEBUG = RIL_DEBUG.DEBUG_RIL || debugPref;
}
updateDebugFlag();

function NetworkTimeService() {
  if (DEBUG) {
    this.debug("NetworkTimeService constructor");
  }
  this._lastNitzData = [];
  this._suggestedTimeRequests = [];
  this._clockAutoUpdateEnabled = false;
  this._timezoneAutoUpdateEnabled = false;
  this._dataDefaultServiceId = -1;

  this._sntpTimeoutInSecs = Services.prefs.getIntPref("network.sntp.timeout");

  this._sntp = new Sntp(
    this.onSntpDataAvailable.bind(this),
    Services.prefs.getIntPref("network.sntp.maxRetryCount"),
    Services.prefs.getIntPref("network.sntp.refreshPeriod"),
    this._sntpTimeoutInSecs,
    Services.prefs.getCharPref("network.sntp.pools").split(";"),
    Services.prefs.getIntPref("network.sntp.port")
  );

  if (lazy.gSettingsManager) {
    // Read the "time.clock.automatic-update.enabled" setting to see if
    // we need to adjust the system clock time by NITZ or SNTP.
    // Read the "time.timezone.automatic-update.enabled" setting to see if
    // we need to adjust the system timezone by NITZ.
    lazy.gSettingsManager.getBatch(
      [
        kSettingsClockAutoUpdateEnabled,
        kSettingsTimezoneAutoUpdateEnabled,
        kSettingsDataDefaultServiceId,
      ],
      {
        resolve: settings => {
          settings.forEach(info => {
            this.handle(info.name, JSON.parse(info.value));
          });
        },
        reject: () => {},
      }
    );
  }

  // Set "time.clock.automatic-update.available" to false when starting up.
  this._setClockAutoUpdateAvailable(false);

  // Set "time.timezone.automatic-update.available" to false when starting up.
  this._setTimezoneAutoUpdateAvailable(false);

  this._initObservers();

  if (DEBUG) {
    this.debug("NetworkTimeService constructor end");
  }
}
NetworkTimeService.prototype = {
  classID: NETWORKTIMESERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINetworkTimeService,
    Ci.nsIObserver,
    Ci.nsISidlDefaultResponse,
    Ci.nsISettingsObserver,
    Ci.nsITimeObserver,
  ]),

  // Remember the last NITZ message so that we can set the time based on
  // the network immediately when users enable network-based time.
  _lastNitzData: null,

  // Flag to determine whether to update system clock automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _clockAutoUpdateEnabled: null,

  // Flag to determine whether to update system timezone automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _timezoneAutoUpdateEnabled: null,

  // Object that handles SNTP.
  _sntp: null,

  _suggestedTimeRequests: null,

  _dataDefaultServiceId: null,

  _sntpTimeoutInSecs: null,

  _sntpTimer: null,

  debug(aMessage) {
    console.log("NetworkTimeService: " + aMessage);
  },

  setTelephonyTime(aSlotId, aNitzData) {
    // // Got the NITZ info received from the ril_worker.
    this._setClockAutoUpdateAvailable(true);
    this._setTimezoneAutoUpdateAvailable(true);

    // Cache the latest NITZ message whenever receiving it.
    this._lastNitzData[aSlotId] = aNitzData;

    // Set the received NITZ clock if the setting is enabled.
    if (this._clockAutoUpdateEnabled) {
      this.setClockByNitz(aNitzData);
    }
    // Set the received NITZ timezone if the setting is enabled.
    if (this._timezoneAutoUpdateEnabled) {
      this.setTimezoneByNitz(aNitzData);
    }
  },

  getSuggestedNetworkTime(aCallback) {
    if (this._lastNitzData[0]) {
      this._getClockByNitz(this._lastNitzData[0])
        .then(suggestion => {
          aCallback.onSuggestedNetworkTimeResponse(suggestion);
        })
        .catch(() => {
          this.debug("getSuggestedNetworkTime rejected");
        });
      return;
    }

    // SNTP
    if (
      lazy.gNetworkManager.activeNetworkInfo &&
      lazy.gNetworkManager.activeNetworkInfo.state ==
        Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
    ) {
      if (!this._sntp.isExpired()) {
        let offset = this._sntp.getOffset();
        aCallback.onSuggestedNetworkTimeResponse(Date.now() + offset);
      } else {
        this._suggestedTimeRequests.push(aCallback);
        if (this._suggestedTimeRequests.length == 1) {
          this._requestSntp();
        }
      }
      return;
    }

    // No network time is available, return now.
    aCallback.onSuggestedNetworkTimeResponse(Date.now());
  },

  handle(aName, aResult) {
    switch (aName) {
      case kSettingsClockAutoUpdateEnabled:
        this._clockAutoUpdateEnabled = aResult;
        if (!this._clockAutoUpdateEnabled) {
          break;
        }

        // Set the latest cached NITZ time if it's available.
        if (this._lastNitzData[0]) {
          this.setClockByNitz(this._lastNitzData[0]);
        } else if (
          lazy.gNetworkManager.activeNetworkInfo &&
          lazy.gNetworkManager.activeNetworkInfo.state ==
            Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          // Set the latest cached SNTP time if it's available.
          if (!this._sntp.isExpired()) {
            this._setClockBySntp(this._sntp.getOffset());
          } else {
            // Or refresh the SNTP.
            this._sntp.request();
          }
        }
        break;

      case kSettingsTimezoneAutoUpdateEnabled:
        this._timezoneAutoUpdateEnabled = aResult;

        if (this._timezoneAutoUpdateEnabled) {
          // Apply the latest cached NITZ for timezone if it's available.
          if (this._timezoneAutoUpdateEnabled && this._lastNitzData[0]) {
            this.setTimezoneByNitz(this._lastNitzData[0], true);
          }
        }
        break;

      case kSettingsDataDefaultServiceId:
        this._dataDefaultServiceId = aResult;
        break;

      default:
        break;
    }
  },

  // nsIObserver interface methods.
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this._deinitObservers();
        break;

      case kNetworkActiveChangedTopic:
        if (!aSubject) {
          return;
        }
        let networkInfo = aSubject.QueryInterface(Ci.nsINetworkInfo);
        if (
          !networkInfo ||
          networkInfo.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          return;
        }

        // SNTP can only update when we have mobile or Wifi connections.
        if (
          networkInfo.type != NETWORK_TYPE_WIFI &&
          networkInfo.type != NETWORK_TYPE_MOBILE
        ) {
          return;
        }

        // If the network comes from RIL, make sure the RIL service is matched.
        if (
          networkInfo.type == NETWORK_TYPE_MOBILE &&
          aSubject instanceof Ci.nsIRilNetworkInfo
        ) {
          networkInfo = aSubject.QueryInterface(Ci.nsIRilNetworkInfo);
          if (networkInfo.serviceId != this._dataDefaultServiceId) {
            return;
          }
        }

        if (this._sntp.isExpired()) {
          this.debug("sntp expired, request");
          this._requestSntp();
        }
        break;

      default:
        break;
    }
  },

  // nsISidlDefaultResponse
  resolve() {
    if (DEBUG) {
      this.debug("SIDL op success");
    }
  },

  reject() {
    this.debug("SIDL op error");
  },

  // nsISettingsObserver
  observeSetting(aSettingInfo) {
    if (aSettingInfo) {
      this.handleSettingsChange(
        aSettingInfo.name,
        JSON.parse(aSettingInfo.value)
      );
    }
  },

  // nsITimeObserver
  notify(aTimeInfo) {
    switch (aTimeInfo.reason) {
      case Ci.nsITime.TIME_CHANGED:
        let offset = parseInt(aTimeInfo.delta, 10);
        this._sntp.updateOffset(offset);
        break;
    }
  },

  handleSettingsChange(aName, aResult) {
    // Don't allow any content processes to modify the setting
    // "time.clock.automatic-update.available" except for the chrome process.
    if (aName === kSettingsClockAutoUpdateAvailable) {
      let isClockAutoUpdateAvailable =
        this._lastNitzData[0] !== null || this._sntp.isAvailable();
      if (aResult !== isClockAutoUpdateAvailable) {
        if (DEBUG) {
          this.debug(
            "Content processes cannot modify 'time.clock.automatic-update.available'. Restore!"
          );
        }
        // Restore the setting to the current value.
        this._setClockAutoUpdateAvailable(isClockAutoUpdateAvailable);
      }
      return;
    }

    // Don't allow any content processes to modify the setting
    // "time.timezone.automatic-update.available" except for the chrome
    // process.
    if (aName === kSettingsTimezoneAutoUpdateAvailable) {
      let isTimezoneAutoUpdateAvailable = this._lastNitzData[0] !== null;
      if (aResult !== isTimezoneAutoUpdateAvailable) {
        if (DEBUG) {
          this.debug(
            "Content processes cannot modify 'time.timezone.automatic-update.available'. Restore!"
          );
        }
        // Restore the setting to the current value.
        this._setTimezoneAutoUpdateAvailable(isTimezoneAutoUpdateAvailable);
      }
      return;
    }
    this.handle(aName, aResult);
  },

  _setClockAutoUpdateAvailable(aAvailable) {
    this._updateSetting(kSettingsClockAutoUpdateAvailable, aAvailable);
  },

  _setTimezoneAutoUpdateAvailable(aAvailable) {
    this._updateSetting(kSettingsTimezoneAutoUpdateAvailable, aAvailable);
  },

  /**
   * Set the system clock by NITZ.
   * @param aNitzData nsINitzData
   */
  setClockByNitz(aNitzData) {
    if (DEBUG) {
      this.debug("setClockByNitz: " + aNitzData.time);
    }
    this._getClockByNitz(aNitzData)
      .then(nitzTime => {
        if (nitzTime == INVALID_UPTIME) {
          this.debug("setClockByNitz nitzTime is invalid, skip!");
          return;
        }
        lazy.gTime.setTime(nitzTime, this);
      })
      .catch(() => {});
  },

  _getClockByNitz(aNitzData) {
    let self = this;
    // To set the system clock time. Note that there could be a time diff
    // between when the NITZ was received and when the time is actually set.
    return new Promise((aResolve, _aReject) => {
      let callback = {
        QueryInterface: ChromeUtils.generateQI([Ci.nsITimeGetElapsedRealTime]),

        resolve(systemUpTime) {
          let upTimeDiff = systemUpTime
            ? systemUpTime - aNitzData.receiveTime
            : -1;
          if (DEBUG) {
            self.debug("_getClockByNitz: " + upTimeDiff);
          }

          if (upTimeDiff < 0) {
            self.debug("_getClockByNitz upTimeDiff is invalid!");
            aResolve(INVALID_UPTIME);
          }

          aResolve(aNitzData.time + upTimeDiff);
        },

        reject() {
          aResolve(INVALID_UPTIME);
        },
      };

      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.initWithCallback(
        () => {
          if (DEBUG) {
            self.debug("500ms timeout, force reject");
          }
          callback.reject();
        },
        500,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
      lazy.gTime.getElapsedRealTime(callback);
    });
  },

  /**
   * Set the system time zone by NITZ.
   */
  setTimezoneByNitz(aNitzData, aForceUpdate = false) {
    // To set the sytem timezone. Note that we need to convert the time zone
    // value to a UTC repesentation string in the format of "UTC(+/-)hh:mm".
    // Ex, time zone -480 is "UTC+08:00"; time zone 630 is "UTC-10:30".

    // DST can only be 0|1|2 hr according to 3GPP TS 22.042, besides, there is
    // no need to unapply the DST correction because FE should be able to get
    // the correct time zone offset by using "time.timezone.dst" value.
    this._updateSetting("time.timezone.dst", aNitzData.dst);

    if (aForceUpdate || aNitzData.timeZone != new Date().getTimezoneOffset()) {
      let absTimeZoneInMinutes = Math.abs(aNitzData.timeZone);
      let timeZoneStr = "UTC";
      timeZoneStr += aNitzData.timeZone > 0 ? "-" : "+";
      timeZoneStr += ("0" + Math.floor(absTimeZoneInMinutes / 60)).slice(-2);
      timeZoneStr += ":";
      timeZoneStr += ("0" + (absTimeZoneInMinutes % 60)).slice(-2);
      this._updateSetting("time.timezone", timeZoneStr);
    }
  },

  onSntpDataAvailable(aOffset) {
    this._cancelSntpTimer();
    this._setClockBySntp(aOffset);
    this._notifyRequesters(aOffset);
  },

  _notifyRequesters(aOffset) {
    if (this._suggestedTimeRequests.length) {
      let suggestion = Date.now() + aOffset;
      for (let index = 0; index < this._suggestedTimeRequests.length; index++) {
        this._suggestedTimeRequests[index].onSuggestedNetworkTimeResponse(
          suggestion
        );
      }

      this._suggestedTimeRequests = [];
    }
  },

  /**
   * Set the system clock by SNTP.
   */
  _setClockBySntp(aOffset) {
    // Got the SNTP info.
    this._setClockAutoUpdateAvailable(true);
    if (!this._clockAutoUpdateEnabled) {
      return;
    }
    if (this._lastNitzData[0]) {
      if (DEBUG) {
        this.debug("SNTP: NITZ available, discard SNTP");
      }
      return;
    }
    lazy.gTime.setTime(Date.now() + aOffset, this);
  },

  _updateSetting(aKey, aValue) {
    if (lazy.gSettingsManager) {
      if (DEBUG) {
        this.debug(
          "Update setting key: " + aKey + ", value: " + JSON.stringify(aValue)
        );
      }

      lazy.gSettingsManager.set(
        [
          {
            name: aKey,
            value: JSON.stringify(aValue),
          },
        ],
        {
          resolve() {},
          reject() {},
        }
      );
    }
  },

  _initObservers() {
    this._initTopicObservers();
    this._initSettingsObservers();
    lazy.gTime.addObserver(Ci.nsITime.TIME_CHANGED, this, this);
  },

  _deinitObservers() {
    this._deinitTopicObservers();
    this._deinitSettingsObservers();
    lazy.gTime.removeObserver(Ci.nsITime.TIME_CHANGED, this, this);
  },

  _initTopicObservers() {
    this.debug("init observers: " + OBSERVER_TOPICS_ARRAY);
    OBSERVER_TOPICS_ARRAY.forEach(topic => {
      Services.obs.addObserver(this, topic);
    });
  },

  _deinitTopicObservers() {
    OBSERVER_TOPICS_ARRAY.forEach(topic => {
      Services.obs.removeObserver(this, topic);
    });
  },

  _initSettingsObservers() {
    this.debug("_initSettingsObservers: " + SETTING_KEYS_ARRAY);
    SETTING_KEYS_ARRAY.forEach(setting => {
      this._addSettingsObserver(setting);
    });
  },

  _deinitSettingsObservers() {
    SETTING_KEYS_ARRAY.forEach(setting => {
      this._removeSettingsObserver(setting);
    });
  },

  _addSettingsObserver(aKey) {
    lazy.gSettingsManager.addObserver(aKey, this, {
      resolve: () => {
        if (DEBUG) {
          this.debug("Add SettingObserve " + aKey + " success");
        }
      },
      reject: () => {
        this.debug("Add SettingObserve " + aKey + " failed");
      },
    });
  },

  _removeSettingsObserver(aKey) {
    lazy.gSettingsManager.removeObserver(aKey, this, {
      resolve: () => {
        if (DEBUG) {
          this.debug("Remove SettingObserve " + aKey + " success");
        }
      },
      reject: () => {
        this.debug("Remove SettingObserve " + aKey + " failed");
      },
    });
  },

  _requestSntp() {
    this._sntp.request();
    this._cancelSntpTimer();

    this._sntpTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._sntpTimer.initWithCallback(
      () => {
        this._notifyRequesters(0);
        this._sntpTimer = null;
      },
      this._sntpTimeoutInSecs * 1000,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _cancelSntpTimer() {
    if (this._sntpTimer) {
      if (DEBUG) {
        this.debug("cancel sntp timer");
      }
      this._sntpTimer.cancel();
      this._sntpTimer = null;
    }
  },
};

var EXPORTED_SYMBOLS = ["NetworkTimeService"];
