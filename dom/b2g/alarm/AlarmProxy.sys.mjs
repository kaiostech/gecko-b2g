/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const REQUEST_CPU_LOCK_TIMEOUT = 10 * 1000; // 10 seconds.
const DEBUG = Services.prefs.getBoolPref("dom.alarm.debug", false);
function debug(aMsg) {
  console.log(`AlarmProxy: ${aMsg}`);
}

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

/*
 * AlarmProxy is a helper of passing requests from AlarmManager.cpp to
 * AlarmService.sys.mjs, receiving results from AlarmService.sys.mjs and send it
 * back to AlarmManager.cpp.
 * Note that the life cycle of an AlarmProxy is expected to be within
 * one single request, so there is no count or uuid to distinguish each request.
 */

export function AlarmProxy() {
  DEBUG && debug("AlarmProxy constructor.");
}

AlarmProxy.prototype = {
  add: function add(aUrl, aOptions, aCallback) {
    DEBUG && debug("add " + JSON.stringify(aOptions) + " " + aUrl);

    let date = aOptions.date;
    let ignoreTimezone = !!aOptions.ignoreTimezone;
    let data = aOptions.data;

    if (!date) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    if (data && aUrl) {
      // Run eval() in the sand box with the principal of the calling
      // web page to ensure no cross-origin object is involved. A "Permission
      // Denied" error will be thrown in case of privilege violation.
      // We used JSON.stringify in the past, but the toJSON privileged function is denied now.
      try {
        let sandbox = new Cu.Sandbox(aUrl);
        sandbox.data = data;
        data = Cu.evalInSandbox("eval(data)", sandbox);
      } catch (e) {
        DEBUG && debug(e);
        aCallback.onAdd(
          Cr.NS_ERROR_INVALID_ARG,
          Cu.cloneInto("data examination in sandbox failed", {})
        );
        return;
      }
    }

    let cpuLock = lazy.gPowerManagerService.newWakeLock("cpu");
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    // Start a timer to prevent from non-responding request.
    timer.initWithCallback(
      () => {
        DEBUG && debug("Request timeout! Release the cpu lock");
        cpuLock.unlock();
      },
      REQUEST_CPU_LOCK_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    let sendMsgName = "Alarm:Add";
    let recvMsgName = `${sendMsgName}:Return`;
    Services.cpmm.addMessageListener(recvMsgName, function alarmAdd(aMessage) {
      DEBUG && debug("Receive " + JSON.stringify(aMessage));
      Services.cpmm.removeMessageListener(recvMsgName, alarmAdd);
      timer.cancel();
      cpuLock.unlock();

      let json = aMessage.json ? aMessage.json : {};
      if (json.success) {
        aCallback.onAdd(Cr.NS_OK, Cu.cloneInto(json.id, {}));
      } else {
        aCallback.onAdd(Cr.NS_ERROR_FAILURE, Cu.cloneInto(json.errorMsg, {}));
      }
    });

    Services.cpmm.sendAsyncMessage(sendMsgName, {
      date,
      ignoreTimezone,
      data,
      url: aUrl,
    });
  },

  remove: function remove(aUrl, aId) {
    DEBUG && debug("remove " + aId + " " + aUrl);

    Services.cpmm.sendAsyncMessage("Alarm:Remove", {
      id: aId,
      url: aUrl,
    });
  },

  getAll: function getAll(aUrl, aCallback) {
    DEBUG && debug("getAll " + aUrl);

    let sendMsgName = "Alarm:GetAll";
    let recvMsgName = `${sendMsgName}:Return`;
    Services.cpmm.addMessageListener(recvMsgName, function alarmGetAll(
      aMessage
    ) {
      Services.cpmm.removeMessageListener(recvMsgName, alarmGetAll);

      let json = aMessage.json ? aMessage.json : {};
      if (json.success) {
        let alarms = [];
        json.alarms.forEach(function trimAlarmInfo(aAlarm) {
          let alarm = {
            id: aAlarm.id,
            date: aAlarm.date,
            ignoreTimezone: aAlarm.ignoreTimezone,
            data: aAlarm.data,
          };
          alarms.push(alarm);
        });

        aCallback.onGetAll(Cr.NS_OK, Cu.cloneInto(alarms, {}));
      } else {
        aCallback.onGetAll(
          Cr.NS_ERROR_FAILURE,
          Cu.cloneInto(json.errorMsg, {})
        );
      }
    });

    Services.cpmm.sendAsyncMessage(sendMsgName, {
      url: aUrl,
    });
  },

  contractID: "@mozilla.org/alarm/proxy;1",

  classID: Components.ID("{736b46dc-9bea-4f75-a567-0bec41980f0c}"),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAlarmProxy,
    Ci.nsISupportsWeakReference,
  ]),
};
