/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL", function () {
  let obj = ChromeUtils.import("resource://gre/modules/ril_consts.js");
  return obj;
});

/**
 * RILSystemMessenger
 */
const RILSystemMessenger = function () {};
RILSystemMessenger.prototype = {
  /**
   * Hook of Broadcast function
   *
   * @param aType
   *        The type of the message to be sent.
   * @param aMessage
   *        The message object to be broadcasted.
   */
  broadcastMessage(aType, aMessage, aOrigin = null) {
    // Function stub to be replaced by the owner of this messenger.
  },

  /**
   * Hook of the function to create MozStkCommand message.
   * @param  aStkProactiveCmd
   *         nsIStkProactiveCmd instance.
   *
   * @return a JS object which complies the dictionary of MozStkCommand defined
   *         in MozStkCommandEvent.webidl
   */
  createCommandMessage(aStkProactiveCmd) {
    // Function stub to be replaced by the owner of this messenger.
  },

  /**
   * Wrapper to send "telephony-new-call" system message.
   */
  notifyNewCall() {
    this.broadcastMessage("telephony-new-call", {});
  },

  /**
   * Wrapper to send "telephony-call-ended" system message.
   */
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
    let radioTech = "cs";
    switch (aRadioTech) {
      case Ci.nsITelephonyCallInfo.RADIO_TECH_PS:
        radioTech = "ps";
        break;
      case Ci.nsITelephonyCallInfo.RADIO_TECH_WIFI:
        radioTech = "wifi";
        break;
      case Ci.nsITelephonyCallInfo.RADIO_TECH_CS:
      /* falls through */
      default:
        radioTech = "cs";
        break;
    }

    let verStatus = "none";
    switch (aVerStatus) {
      case Ci.nsITelephonyCallInfo.VER_FAIL:
        verStatus = "fail";
        break;
      case Ci.nsITelephonyCallInfo.VER_PASS:
        verStatus = "pass";
        break;
      case Ci.nsITelephonyCallInfo.VER_NONE:
      /* falls through */
      default:
        verStatus = "none";
        break;
    }

    let data = {
      serviceId: aServiceId,
      number: aNumber,
      emergency: aEmergency,
      duration: aDuration,
      direction: aOutgoing ? "outgoing" : "incoming",
      hangUpLocal: aHangUpLocal,
      isVt: aIsVt,
      radioTech,
      isRtt: aIsRtt,
      verStatus,
    };

    if (aCdmaWaitingNumber != null) {
      data.secondNumber = aCdmaWaitingNumber;
    }

    this.broadcastMessage("telephony-call-ended", data);
  },

  /**
   * Wrapper to send "telephony-hac-mode-changed" system message.
   */
  notifyHacModeChanged(aEnable) {
    this.broadcastMessage("telephony-hac-mode-changed", { hacMode: aEnable });
  },

  /**
   * Wrapper to send "telephony-tty-mode-changed" system message.
   */
  notifyTtyModeChanged(aMode) {
    let ttyMode = ["off", "full", "hco", "vco"][aMode];
    if (!ttyMode) {
      throw new Error("Invalid TTY Mode: " + aMode);
    }

    this.broadcastMessage("telephony-tty-mode-changed", { ttyMode });
  },

  _convertSmsMessageClass(aMessageClass) {
    return lazy.RIL.GECKO_SMS_MESSAGE_CLASSES[aMessageClass] || null;
  },

  _convertSmsDelivery(aDelivery) {
    return ["received", "sending", "sent", "error"][aDelivery] || null;
  },

  _convertSmsDeliveryStatus(aDeliveryStatus) {
    return (
      [
        lazy.RIL.GECKO_SMS_DELIVERY_STATUS_NOT_APPLICABLE,
        lazy.RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS,
        lazy.RIL.GECKO_SMS_DELIVERY_STATUS_PENDING,
        lazy.RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
      ][aDeliveryStatus] || null
    );
  },

  /**
   * Wrapper to send 'sms-received', 'sms-delivery-success', 'sms-sent',
   * 'sms-failed', 'sms-delivery-error' system message.
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
    let msgType = [
      "sms-received",
      "sms-sent",
      "sms-delivery-success",
      "sms-failed",
      "sms-delivery-error",
    ][aNotificationType];

    if (!msgType) {
      throw new Error("Invalid Notification Type: " + aNotificationType);
    }

    this.broadcastMessage(msgType, {
      iccId: aIccId,
      type: "sms",
      id: aId,
      threadId: aThreadId,
      delivery: this._convertSmsDelivery(aDelivery),
      deliveryStatus: this._convertSmsDeliveryStatus(aDeliveryStatus),
      sender: aSender,
      receiver: aReceiver,
      body: aBody,
      messageClass: this._convertSmsMessageClass(aMessageClass),
      timestamp: aTimestamp,
      sentTimestamp: aSentTimestamp,
      deliveryTimestamp: aDeliveryTimestamp,
      read: aRead,
    });
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
    this.broadcastMessage("data-sms-received", {
      serviceId: aServiceId,
      iccId: aIccId,
      type: "sms",
      sender: aSender,
      receiver: aReceiver,
      originatorPort: aDestinationPort,
      destinationPort: aDestinationPort,
      data: aDatas,
    });
  },

  _convertCbGsmGeographicalScope(aGeographicalScope) {
    return lazy.RIL.CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[aGeographicalScope] || null;
  },

  _convertCbMessageClass(aMessageClass) {
    return lazy.RIL.GECKO_SMS_MESSAGE_CLASSES[aMessageClass] || null;
  },

  _convertCbEtwsWarningType(aWarningType) {
    return lazy.RIL.CB_ETWS_WARNING_TYPE_NAMES[aWarningType] || null;
  },

  /**
   * Wrapper to send 'cellbroadcast-received' system message.
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
    // Align the same layout to MozCellBroadcastMessage
    let data = {
      serviceId: aServiceId,
      gsmGeographicalScope: this._convertCbGsmGeographicalScope(
        aGsmGeographicalScope
      ),
      messageCode: aMessageCode,
      messageId: aMessageId,
      language: aLanguage,
      body: aBody,
      messageClass: this._convertCbMessageClass(aMessageClass),
      timestamp: aTimestamp,
      cdmaServiceCategory: null,
      etws: null,
      updateNumber: aUpdateNumber,
    };

    if (aHasEtwsInfo) {
      data.etws = {
        warningType: this._convertCbEtwsWarningType(aEtwsWarningType),
        emergencyUserAlert: aEtwsEmergencyUserAlert,
        popup: aEtwsPopup,
      };
    }

    if (
      aCdmaServiceCategory !=
      Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID
    ) {
      data.cdmaServiceCategory = aCdmaServiceCategory;
    }

    this.broadcastMessage("cellbroadcast-received", data);
  },

  /**
   * Wrapper to send 'ussd-received' system message.
   */
  notifyUssdReceived(aServiceId, aMessage, aSessionEnded) {
    this.broadcastMessage("ussd-received", {
      serviceId: aServiceId,
      message: aMessage,
      sessionEnded: aSessionEnded,
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Display Info.
   */
  notifyCdmaInfoRecDisplay(aServiceId, aDisplay) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      display: aDisplay,
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Called Party
   * Number Info.
   */
  notifyCdmaInfoRecCalledPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      calledNumber: {
        type: aType,
        plan: aPlan,
        number: aNumber,
        pi: aPi,
        si: aSi,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Calling Party
   * Number Info.
   */
  notifyCdmaInfoRecCallingPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      callingNumber: {
        type: aType,
        plan: aPlan,
        number: aNumber,
        pi: aPi,
        si: aSi,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Connected Party
   * Number Info.
   */
  notifyCdmaInfoRecConnectedPartyNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      connectedNumber: {
        type: aType,
        plan: aPlan,
        number: aNumber,
        pi: aPi,
        si: aSi,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Signal Info.
   */
  notifyCdmaInfoRecSignal(aServiceId, aType, aAlertPitch, aSignal) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      signal: {
        type: aType,
        alertPitch: aAlertPitch,
        signal: aSignal,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Redirecting
   * Number Info.
   */
  notifyCdmaInfoRecRedirectingNumber(
    aServiceId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi,
    aReason
  ) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      redirect: {
        type: aType,
        plan: aPlan,
        number: aNumber,
        pi: aPi,
        si: aSi,
        reason: aReason,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Line Control Info.
   */
  notifyCdmaInfoRecLineControl(
    aServiceId,
    aPolarityIncluded,
    aToggle,
    aReverse,
    aPowerDenial
  ) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      lineControl: {
        polarityIncluded: aPolarityIncluded,
        toggle: aToggle,
        reverse: aReverse,
        powerDenial: aPowerDenial,
      },
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with CLIR Info.
   */
  notifyCdmaInfoRecClir(aServiceId, aCause) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      clirCause: aCause,
    });
  },

  /**
   * Wrapper to send 'cdma-info-rec-received' system message with Audio Control Info.
   */
  notifyCdmaInfoRecAudioControl(aServiceId, aUpLink, aDownLink) {
    this.broadcastMessage("cdma-info-rec-received", {
      clientId: aServiceId,
      audioControl: {
        upLink: aUpLink,
        downLink: aDownLink,
      },
    });
  },

  /**
   * Wrapper to send 'icc-stkcommand' system message with Audio Control Info.
   */
  notifyStkProactiveCommand(aIccId, aCommand) {
    this.broadcastMessage("icc-stkcommand", {
      iccId: aIccId,
      command: this.createCommandMessage(aCommand),
    });
  },
};

const EXPORTED_SYMBOLS = ["RILSystemMessenger"];
