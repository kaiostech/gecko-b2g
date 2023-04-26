/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_CONTEXT = "chrome";
MARIONETTE_HEAD_JS = "head.js";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TelephonyService",
  "@mozilla.org/telephony/telephonyservice;1",
  "nsIGonkTelephonyService"
);

ChromeUtils.defineModuleGetter(
  this,
  "TelephonyUtils",
  "resource://gre/modules/TelephonyUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "RIL", function() {
  let obj = ChromeUtils.import("resource://gre/modules/ril_consts.js", null);
  return obj;
});

const number = "0912345678";

function dial() {
  return new Promise(resolve => {
    TelephonyService.dial(0, number, false, {
      QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyDialCallback]),
      notifyDialCallSuccess() {
        resolve();
      },
    });
  });
}

function waitForStateChanged(aPredicate) {
  return new Promise(resolve => {
    let listener = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyListener]),

      callStateChanged(length, allInfo) {
        if (aPredicate(allInfo)) {
          resolve(allInfo);
          TelephonyService.unregisterListener(listener);
        }
      },

      supplementaryServiceNotification() {},
      notifyError() {},
      notifyCdmaCallWaiting() {},
      notifyConferenceError() {},
    };

    TelephonyService.registerListener(listener);
  });
}

function test_noCall() {
  log("== test_noCall ==");
  is(TelephonyUtils.hasAnyCalls(), false, "hasAnyCalls");
  is(TelephonyUtils.hasConnectedCalls(), false, "hasConnectedCalls");
  return TelephonyUtils.waitForNoCalls();
}

function test_oneCall() {
  log("== test_oneCall ==");

  return dial()
    .then(() => {
      is(TelephonyUtils.hasAnyCalls(), true, "hasAnyCalls");
      is(TelephonyUtils.hasConnectedCalls(), false, "hasConnectedCalls");
    })
    .then(() =>
      waitForStateChanged(aAllInfo => {
        return (
          aAllInfo[0].callState === Ci.nsITelephonyService.CALL_STATE_ALERTING
        );
      })
    )
    .then(() => {
      let p = waitForStateChanged(aAllInfo => {
        return (
          aAllInfo[0].callState === Ci.nsITelephonyService.CALL_STATE_CONNECTED
        );
      });
      emulator.runCmd("telephony accept " + number);
      return p;
    })
    .then(() => {
      is(TelephonyUtils.hasAnyCalls(), true, "hasAnyCalls");
      is(TelephonyUtils.hasConnectedCalls(), true, "hasConnectedCalls");
    })
    .then(() => {
      let p = TelephonyUtils.waitForNoCalls();
      emulator.runCmd("telephony cancel " + number);
      return p;
    });
}

startTest(function() {
  return Promise.resolve()
    .then(test_noCall)
    .then(test_oneCall)
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
