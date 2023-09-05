/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RSM", function () {
  let obj = ChromeUtils.import("resource://gre/modules/RILSystemMessenger.jsm");
  return obj;
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gStkCmdFactory",
  "@mozilla.org/icc/stkcmdfactory;1",
  "nsIStkCmdFactory"
);

const RILSYSTEMMESSENGERHELPER_CID = Components.ID(
  "{19d9a4ea-580d-11e4-8f6c-37ababfaaea9}"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSystemMessenger",
  "@mozilla.org/systemmessage-service;1",
  "nsISystemMessageService"
);

var DEBUG = true;
function debug(s) {
  dump("-@- RILSystemMessenger: " + s + "\n");
}

// Read debug setting from pref.
try {
  let debugPref = Services.prefs.getBoolPref("ril.debugging.enabled");
  DEBUG = DEBUG || debugPref;
} catch (e) {}

/**
 * RILSystemMessengerHelper
 */
function RILSystemMessengerHelper() {
  if (DEBUG) {
    debug("constructor");
  }
  debug("RSM: " + JSON.stringify(lazy.RSM));

  this.messenger = new lazy.RSM.RILSystemMessenger();
  this.messenger.broadcastMessage = (aType, aMessage, aOrigin = null) => {
    if (DEBUG) {
      debug(
        "broadcastMessage: aType: " +
          aType +
          ", aMessage: " +
          JSON.stringify(aMessage)
      );
    }

    if (aOrigin) {
      lazy.gSystemMessenger.sendMessage(aType, aMessage, aOrigin);
    } else {
      lazy.gSystemMessenger.broadcastMessage(aType, aMessage);
    }
  };

  this.messenger.createCommandMessage = aStkProactiveCmd => {
    return lazy.gStkCmdFactory.createCommandMessage(aStkProactiveCmd);
  };
}
RILSystemMessengerHelper.prototype = {
  classID: RILSYSTEMMESSENGERHELPER_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsITelephonyMessenger,
    Ci.nsISmsMessenger,
    Ci.nsICellbroadcastMessenger,
    Ci.nsIMobileConnectionMessenger,
    Ci.nsIIccMessenger,
  ]),

  /**
   * RILSystemMessenger instance.
   */
  messenger: null,

  /**
   * nsITelephonyMessenger API
   */
  notifyNewCall() {
    this.messenger.notifyNewCall();
  },

  notifyCallEnded(
    aServiceId,
    aNumber,
    aCdmaWaitingNumber,
    aEmergency,
    aDuration,
    aOutgoing,
    aHangUpLocal,
    aIsVt,
    aRadioTech,
    aIsRtt,
    aVerStatus
  ) {
    this.messenger.notifyCallEnded(
      aServiceId,
      aNumber,
      aCdmaWaitingNumber,
      aEmergency,
      aDuration,
      aOutgoing,
      aHangUpLocal,
      aIsVt,
      aRadioTech,
      aIsRtt,
      aVerStatus
    );
  },

  notifyHacModeChanged(aEnable) {
    this.messenger.notifyHacModeChanged(aEnable);
  },

  notifyTtyModeChanged(aMode) {
    this.messenger.notifyTtyModeChanged(aMode);
  },

  notifyUssdReceived(aServiceId, aMessage, aSessionEnded) {
    this.messenger.notifyUssdReceived(aServiceId, aMessage, aSessionEnded);
  },

  /**
   * nsISmsMessenger API
   */
  notifySms(
    aNotificationType,
    aId,
    aThreadId,
    aIccId,
    aDelivery,
    aDeliveryStatus,
    aSender,
    aReceiver,
    aBody,
    aMessageClass,
    aTimestamp,
    aSentTimestamp,
    aDeliveryTimestamp,
    aRead
  ) {
    this.messenger.notifySms(
      aNotificationType,
      aId,
      aThreadId,
      aIccId,
      aDelivery,
      aDeliveryStatus,
      aSender,
      aReceiver,
      aBody,
      aMessageClass,
      aTimestamp,
      aSentTimestamp,
      aDeliveryTimestamp,
      aRead
    );
  },

  notifyDataSms(
    aServiceId,
    aIccId,
    aSender,
    aReceiver,
    aOriginatorPort,
    aDestinationPort,
    aDatas
  ) {
    this.messenger.notifyDataSms(
      aServiceId,
      aIccId,
      aSender,
      aReceiver,
      aOriginatorPort,
      aDestinationPort,
      aDatas
    );
  },

  /**
   * nsICellbroadcastMessenger API
   */
  notifyCbMessageReceived(
    aServiceId,
    aGsmGeographicalScope,
    aMessageCode,
    aMessageId,
    aLanguage,
    aBody,
    aMessageClass,
    aTimestamp,
    aCdmaServiceCategory,
    aHasEtwsInfo,
    aEtwsWarningType,
    aEtwsEmergencyUserAlert,
    aEtwsPopup,
    aUpdateNumber
  ) {
    this.messenger.notifyCbMessageReceived(
      aServiceId,
      aGsmGeographicalScope,
      aMessageCode,
      aMessageId,
      aLanguage,
      aBody,
      aMessageClass,
      aTimestamp,
      aCdmaServiceCategory,
      aHasEtwsInfo,
      aEtwsWarningType,
      aEtwsEmergencyUserAlert,
      aEtwsPopup,
      aUpdateNumber
    );
  },

  /**
   * nsIMobileConnectionMessenger API
   */
  notifyCdmaInfoRecDisplay(aServiceId, aDisplay) {
    this.messenger.notifyCdmaInfoRecDisplay(aServiceId, aDisplay);
  },

  notifyCdmaInfoRecCalledPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.messenger.notifyCdmaInfoRecCalledPartyNumber(
      aServiceId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecCallingPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.messenger.notifyCdmaInfoRecCallingPartyNumber(
      aServiceId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecConnectedPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.messenger.notifyCdmaInfoRecConnectedPartyNumber(
      aServiceId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecSignal(aServiceId, aType, aAlertPitch, aSignal) {
    this.messenger.notifyCdmaInfoRecSignal(
      aServiceId,
      aType,
      aAlertPitch,
      aSignal
    );
  },

  notifyCdmaInfoRecRedirectingNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi,
    aReason
  ) {
    this.messenger.notifyCdmaInfoRecRedirectingNumber(
      aServiceId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi,
      aReason
    );
  },

  notifyCdmaInfoRecLineControl(
    aServiceId,
    aPolarityIncluded,
    aToggle,
    aReverse,
    aPowerDenial
  ) {
    this.messenger.notifyCdmaInfoRecLineControl(
      aServiceId,
      aPolarityIncluded,
      aToggle,
      aReverse,
      aPowerDenial
    );
  },

  notifyCdmaInfoRecClir(aServiceId, aCause) {
    this.messenger.notifyCdmaInfoRecClir(aServiceId, aCause);
  },

  notifyCdmaInfoRecAudioControl(aServiceId, aUpLink, aDownLink) {
    this.messenger.notifyCdmaInfoRecAudioControl(
      aServiceId,
      aUpLink,
      aDownLink
    );
  },

  /**
   * nsIIccMessenger API
   */
  notifyStkProactiveCommand(aIccId, aCommand) {
    this.messenger.notifyStkProactiveCommand(aIccId, aCommand);
  },
};

var EXPORTED_SYMBOLS = ["RILSystemMessengerHelper"];
