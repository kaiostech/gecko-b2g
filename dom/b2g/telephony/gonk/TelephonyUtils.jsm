/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["TelephonyUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "TelephonyService",
  "@mozilla.org/telephony/telephonyservice;1",
  "nsITelephonyService"
);

function getCurrentCalls(aFilter) {
  if (aFilter === undefined) {
    aFilter = call => true;
  }

  let calls = [];

  // nsITelephonyService.enumerateCalls is synchronous.
  lazy.TelephonyService.enumerateCalls({
    QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyListener]),
    enumerateCallStateComplete() {},
    enumerateCallState(call) {
      if (aFilter(call)) {
        calls.push(call);
      }
    },
  });

  return calls;
}

const TelephonyUtils = {
  /**
   * Check whether there are any calls.
   *
   * @param aClientId [optional] If provided, only check on aClientId
   * @return boolean
   */
  hasAnyCalls(aClientId) {
    let calls = getCurrentCalls(call => {
      if (aClientId !== undefined && call.clientId !== aClientId) {
        return false;
      }
      return true;
    });

    return calls.length !== 0;
  },

  /**
   * Check whether there are any connected calls.
   *
   * @param aClientId [optional] If provided, only check on aClientId
   * @return boolean
   */
  hasConnectedCalls(aClientId) {
    let calls = getCurrentCalls(call => {
      if (aClientId !== undefined && call.clientId !== aClientId) {
        return false;
      }
      return call.callState === Ci.nsITelephonyService.CALL_STATE_CONNECTED;
    });

    return calls.length !== 0;
  },

  /**
   * Return a promise which will be resolved when there are no calls.
   *
   * @param aClientId [optional] only check on aClientId if provided
   * @return Promise
   */
  waitForNoCalls(aClientId) {
    if (!this.hasAnyCalls(aClientId)) {
      return Promise.resolve();
    }

    let self = this;
    return new Promise(resolve => {
      let listener = {
        QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyListener]),

        enumerateCallStateComplete() {},
        enumerateCallState() {},
        callStateChanged() {
          if (!self.hasAnyCalls(aClientId)) {
            lazy.TelephonyService.unregisterListener(this);
            resolve();
          }
        },
        supplementaryServiceNotification() {},
        notifyError() {},
        notifyCdmaCallWaiting() {},
        notifyConferenceError() {},
      };

      lazy.TelephonyService.registerListener(listener);
    });
  },
};
