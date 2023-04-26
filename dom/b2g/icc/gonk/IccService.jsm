/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL", function() {
  return ChromeUtils.import("resource://gre/modules/ril_consts.js");
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const GONK_ICCSERVICE_CID = Components.ID(
  "{df854256-9554-11e4-a16c-c39e8d106c26}"
);

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const B2G_SW_REGISTRATION_DONE_TOPIC_ID = "b2g-sw-registration-done";

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRadioInterfaceLayer",
  "@mozilla.org/ril;1",
  "nsIRadioInterfaceLayer"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIGonkMobileConnectionService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccMessenger",
  "@mozilla.org/ril/system-messenger-helper;1",
  "nsIIccMessenger"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gStkCmdFactory",
  "@mozilla.org/icc/stkcmdfactory;1",
  "nsIStkCmdFactory"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;
function debug(s) {
  console.log("IccService: " + s + "\n");
}

function IccInfo() {}
IccInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIIccInfo]),

  // nsIIccInfo

  iccType: null,
  iccid: null,
  mcc: null,
  mnc: null,
  spn: null,
  imsi: null,
  gid1: null,
  gid2: null,
  isDisplayNetworkNameRequired: false,
  isDisplaySpnRequired: false,
};

function GsmIccInfo() {}
GsmIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIGsmIccInfo, Ci.nsIIccInfo]),

  // nsIGsmIccInfo

  msisdn: null,
};

function CdmaIccInfo() {}
CdmaIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICdmaIccInfo, Ci.nsIIccInfo]),

  // nsICdmaIccInfo

  mdn: null,
  prlVersion: 0,
};

function IsimIccInfo() {}
IsimIccInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIIsimIccInfo]),

  // nsIIsimIccInfo

  impi: null,
  impus: [],
};

function IccContact(aContact) {
  this.id = aContact.contactId || null;
  this._names = [];
  this._numbers = [];
  this._emails = [];

  this._names.push(aContact.alphaId);
  this._numbers.push(aContact.number);

  let anrLen = aContact.anr ? aContact.anr.length : 0;
  for (let i = 0; i < anrLen; i++) {
    this._numbers.push(aContact.anr[i]);
  }

  if (aContact.email) {
    this._emails.push(aContact.email);
  }
}
IccContact.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIIccContact]),

  _names: null,
  _numbers: null,
  _emails: null,

  // nsIIccContact

  id: null,

  getNames() {
    if (!this._names) {
      return null;
    }

    return this._names.slice();
  },

  getNumbers() {
    if (!this._numbers) {
      return null;
    }

    return this._numbers.slice();
  },

  getEmails() {
    if (!this._emails) {
      return null;
    }

    return this._emails.slice();
  },
};

function IccService() {
  this._iccs = [];

  let numClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    this._iccs.push(new Icc(i));
  }

  this._updateDebugFlag();

  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  Services.obs.addObserver(this, B2G_SW_REGISTRATION_DONE_TOPIC_ID);

  this._b2gSWRegistered = false;
  this._cachedStkCmds = {};
}
IccService.prototype = {
  classID: GONK_ICCSERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIIccService,
    Ci.nsIGonkIccService,
    Ci.nsIObserver,
  ]),

  // An array of Icc instances.
  _iccs: null,

  _b2gSWRegistered: false,
  _cachedStkCmds: null, // {iccid: []}

  _updateDebugFlag() {
    try {
      DEBUG =
        DEBUG || Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  /**
   * nsIIccService interface.
   */
  getIccByServiceId(aServiceId) {
    let icc = this._iccs[aServiceId];
    if (!icc) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    return icc;
  },

  /**
   * nsIGonkIccService interface.
   */
  notifyStkCommand(aServiceId, aStkcommand) {
    if (DEBUG) {
      debug("notifyStkCommand for service Id: " + aServiceId);
    }

    let icc = this.getIccByServiceId(aServiceId);

    if (!icc.iccInfo || !icc.iccInfo.iccid) {
      debug("Warning: got STK command when iccid is invalid.");
      return;
    }

    if (this._b2gSWRegistered) {
      lazy.gIccMessenger.notifyStkProactiveCommand(
        icc.iccInfo.iccid,
        aStkcommand
      );
    } else {
      // Cache command since SW registration is not ready yet.
      // TODO we should create getter api instead of delay broadcast.
      if (!this._cachedStkCmds[icc.iccInfo.iccid]) {
        this._cachedStkCmds[icc.iccInfo.iccid] = [];
      }
      this._cachedStkCmds[icc.iccInfo.iccid].push(aStkcommand);
    }

    icc._deliverListenerEvent("notifyStkCommand", [aStkcommand]);
  },

  notifyStkSessionEnd(aServiceId) {
    if (DEBUG) {
      debug("notifyStkSessionEnd for service Id: " + aServiceId);
    }

    this.getIccByServiceId(aServiceId)._deliverListenerEvent(
      "notifyStkSessionEnd"
    );
  },

  notifyCardStateChanged(aServiceId, aCardState, aPin2CardState) {
    if (DEBUG) {
      debug(
        "notifyCardStateChanged for service Id: " +
          aServiceId +
          ", CardState: " +
          aCardState +
          ", Pin2CardState: " +
          aPin2CardState
      );
    }

    this.getIccByServiceId(aServiceId)._updateCardState(
      aCardState,
      aPin2CardState
    );
  },

  notifyIccInfoChanged(aServiceId, aIccInfo) {
    if (DEBUG) {
      debug(
        "notifyIccInfoChanged for service Id: " +
          aServiceId +
          ", IccInfo: " +
          JSON.stringify(aIccInfo)
      );
    }

    this.getIccByServiceId(aServiceId)._updateIccInfo(aIccInfo);
  },

  notifyImsiChanged(aServiceId, aImsi) {
    if (DEBUG) {
      debug(
        "notifyImsiChanged for service Id: " + aServiceId + ", Imsi: " + aImsi
      );
    }

    let icc = this.getIccByServiceId(aServiceId);
    icc.imsi = aImsi || null;
    if (icc.iccInfo) {
      icc.iccInfo.imsi = icc.imsi;
    }
    icc._deliverListenerEvent("notifyIccInfoChanged");
  },

  notifyIsimInfoChanged(aServiceId, aIsimInfo) {
    if (DEBUG) {
      debug(
        "notifyIsimInfoChanged for service Id: " +
          aServiceId +
          ", IsimInfo: " +
          JSON.stringify(aIsimInfo)
      );
    }

    this.getIccByServiceId(aServiceId)._updateIsimIccInfo(aIsimInfo);
  },

  /**
   * nsIObserver interface.
   */
  observe(_aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          this._updateDebugFlag();
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        Services.obs.removeObserver(this, B2G_SW_REGISTRATION_DONE_TOPIC_ID);
        break;
      case B2G_SW_REGISTRATION_DONE_TOPIC_ID:
        this._b2gSWRegistered = true;
        this._flushCachedStkCmds();
        Services.obs.removeObserver(this, B2G_SW_REGISTRATION_DONE_TOPIC_ID);
        break;
    }
  },

  _flushCachedStkCmds() {
    for (let iccid in this._cachedStkCmds) {
      let cmds = this._cachedStkCmds[iccid];
      cmds.forEach(stkCmd => {
        lazy.gIccMessenger.notifyStkProactiveCommand(iccid, stkCmd);
      });
    }
    this._cachedStkCmds = {};
  },
};

function Icc(aClientId) {
  this._clientId = aClientId;
  this._radioInterface = lazy.gRadioInterfaceLayer.getRadioInterface(aClientId);
  this._listeners = [];
}
Icc.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIIcc]),

  _clientId: 0,
  _radioInterface: null,
  _listeners: null,

  _updateCardState(aCardState, aPin2CardState) {
    if (this.cardState != aCardState) {
      this.cardState = aCardState;
    }

    if (this.pin2CardState != aPin2CardState) {
      this.pin2CardState = aPin2CardState;
    }

    this._deliverListenerEvent("notifyCardStateChanged");
  },

  // An utility function to copy objects.
  _updateInfo(aSrcInfo, aDestInfo) {
    for (let key in aSrcInfo) {
      aDestInfo[key] = aSrcInfo[key];
    }
  },

  /**
   * A utility function to compare objects. The srcInfo may contain
   * "rilMessageType", should ignore it.
   */
  _isInfoChanged(srcInfo, destInfo) {
    if (!destInfo) {
      return true;
    }

    for (let key in srcInfo) {
      if (key === "rilMessageType") {
        continue;
      }
      if (srcInfo[key] !== destInfo[key]) {
        return true;
      }
    }

    return false;
  },

  /**
   * We need to consider below cases when update iccInfo:
   * 1. Should clear iccInfo to null if there is no card detected.
   * 2. Need to create corresponding object based on iccType.
   */
  _updateIccInfo(aIccInfo) {
    let oldSpn = this.iccInfo ? this.iccInfo.spn : null;

    // Card is not detected, clear iccInfo to null.
    if (!aIccInfo || !aIccInfo.iccid) {
      if (this.iccInfo) {
        if (DEBUG) {
          debug("Card is not detected, clear iccInfo to null.");
        }
        this.imsi = null;
        this.iccInfo = null;
        this._deliverListenerEvent("notifyIccInfoChanged");
      }
      return;
    }

    if (!this._isInfoChanged(aIccInfo, this.iccInfo)) {
      return;
    }

    // If iccInfo is null, new corresponding object based on iccType.
    if (!this.iccInfo || this.iccInfo.iccType != aIccInfo.iccType) {
      if (aIccInfo.iccType === "ruim" || aIccInfo.iccType === "csim") {
        this.iccInfo = new CdmaIccInfo();
      } else if (aIccInfo.iccType === "sim" || aIccInfo.iccType === "usim") {
        this.iccInfo = new GsmIccInfo();
      } else {
        this.iccInfo = new IccInfo();
      }
    }

    this._updateInfo(aIccInfo, this.iccInfo);
    this.iccInfo.imsi = this.imsi || null;

    this._deliverListenerEvent("notifyIccInfoChanged");

    // Update lastKnownSimMcc.
    if (aIccInfo.mcc) {
      try {
        Services.prefs.setCharPref(
          "ril.lastKnownSimMcc",
          aIccInfo.mcc.toString()
        );
      } catch (e) {}
    }

    // Update lastKnownHomeNetwork.
    if (aIccInfo.mcc && aIccInfo.mnc) {
      let lastKnownHomeNetwork = aIccInfo.mcc + "-" + aIccInfo.mnc;
      // Append spn information if available.
      if (aIccInfo.spn) {
        lastKnownHomeNetwork += "-" + aIccInfo.spn;
      }

      lazy.gMobileConnectionService.notifyLastHomeNetworkChanged(
        this._clientId,
        lastKnownHomeNetwork
      );
    }

    // If spn becomes available, we should check roaming again.
    if (!oldSpn && aIccInfo.spn) {
      lazy.gMobileConnectionService.notifySpnAvailable(this._clientId);
    }
  },

  _deliverListenerEvent(aName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (!this._listeners.includes(listener)) {
        continue;
      }
      let handler = listener[aName];
      if (typeof handler != "function") {
        throw new Error("No handler for " + aName);
      }
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        if (DEBUG) {
          debug("listener for " + aName + " threw an exception: " + e);
        }
      }
    }
  },

  _modifyCardLock(aOperation, aOptions, aCallback) {
    this._radioInterface.sendWorkerMessage(aOperation, aOptions, aResponse => {
      if (aResponse.errorMsg) {
        let retryCount =
          aResponse.remainingRetries !== undefined
            ? aResponse.remainingRetries
            : -1;
        debug(
          "_modifyCardLock aOperation:" +
            aOperation +
            " errorMsg:" +
            aResponse.errorMsg +
            " , retryCount:" +
            retryCount
        );
        aCallback.notifyCardLockError(aResponse.errorMsg, retryCount);
        return;
      }

      aCallback.notifySuccess();
    });
  },

  /**
   * Helper to match the MVNO pattern with IMSI.
   *
   * Note: Characters 'x' and 'X' in MVNO are skipped and not compared.
   *       E.g., if the aMvnoData passed is '310260x10xxxxxx', then the
   *       function returns true only if imsi has the same first 6 digits, 8th
   *       and 9th digit.
   *
   * @param aMvnoData
   *        MVNO pattern.
   * @param aImsi
   *        IMSI of this ICC.
   *
   * @return true if matched.
   */
  _isImsiMatches(aMvnoData, aImsi) {
    // This should not be an error, but a mismatch.
    if (aMvnoData.length > aImsi.length) {
      return false;
    }

    for (let i = 0; i < aMvnoData.length; i++) {
      let c = aMvnoData[i];
      if (c !== "x" && c !== "X" && c !== aImsi[i]) {
        return false;
      }
    }
    return true;
  },

  /**
   * nsIIsimIccInfo interface.
   */
  isimInfo: null,
  _updateIsimIccInfo(aIsimInfo) {
    if (!this.isimInfo) {
      this.isimInfo = new IsimIccInfo();
    }

    this.isimInfo.impi = aIsimInfo.impi;
    this.isimInfo.impus = aIsimInfo.impus.slice();

    this._deliverListenerEvent("notifyIsimInfoChanged");
  },

  /**
   * nsIIcc interface.
   */
  iccInfo: null,
  cardState: Ci.nsIIcc.CARD_STATE_UNKNOWN,
  pin2CardState: Ci.nsIIcc.CARD_STATE_UNKNOWN,
  imsi: null,

  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },

  getCardLockEnabled(aLockType, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "iccGetCardLockEnabled",
      { lockType: aLockType },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifySuccessWithBoolean(aResponse.enabled);
      }
    );
  },

  unlockCardLock(aLockType, aPassword, aNewPin, aCallback) {
    this._modifyCardLock(
      "iccUnlockCardLock",
      { lockType: aLockType, password: aPassword, newPin: aNewPin },
      aCallback
    );
  },

  setCardLockEnabled(aLockType, aPassword, aEnabled, aCallback) {
    this._modifyCardLock(
      "iccSetCardLockEnabled",
      { lockType: aLockType, password: aPassword, enabled: aEnabled },
      aCallback
    );
  },

  changeCardLockPassword(aLockType, aPassword, aNewPassword, aCallback) {
    this._modifyCardLock(
      "iccChangeCardLockPassword",
      { lockType: aLockType, password: aPassword, newPassword: aNewPassword },
      aCallback
    );
  },

  matchMvno(aMvnoType, aMvnoData, aCallback) {
    if (!aMvnoData) {
      aCallback.notifyError(lazy.RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    switch (aMvnoType) {
      case Ci.nsIIcc.CARD_MVNO_TYPE_IMSI:
        let imsi = this.imsi;
        if (!imsi) {
          aCallback.notifyError(lazy.RIL.GECKO_ERROR_GENERIC_FAILURE);
          break;
        }
        aCallback.notifySuccessWithBoolean(
          this._isImsiMatches(aMvnoData, imsi)
        );
        break;
      case Ci.nsIIcc.CARD_MVNO_TYPE_SPN:
        let spn = this.iccInfo && this.iccInfo.spn;
        if (!spn) {
          aCallback.notifyError(lazy.RIL.GECKO_ERROR_GENERIC_FAILURE);
          break;
        }
        aCallback.notifySuccessWithBoolean(spn == aMvnoData);
        break;
      case Ci.nsIIcc.CARD_MVNO_TYPE_GID:
        let gid = this.iccInfo && this.iccInfo.gid1;
        let mvnoDataLength = aMvnoData.length;
        if (!gid) {
          aCallback.notifyError(lazy.RIL.GECKO_ERROR_GENERIC_FAILURE);
        } else if (mvnoDataLength > gid.length) {
          aCallback.notifySuccessWithBoolean(false);
        } else {
          let result =
            gid.substring(0, mvnoDataLength).toLowerCase() ==
            aMvnoData.toLowerCase();
          aCallback.notifySuccessWithBoolean(result);
        }
        break;
      default:
        aCallback.notifyError(lazy.RIL.GECKO_ERROR_MODE_NOT_SUPPORTED);
        break;
    }
  },

  getServiceStateEnabled(aService, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getIccServiceState",
      { service: aService },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifySuccessWithBoolean(aResponse.result);
      }
    );
  },

  iccOpenChannel(aAid, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "iccOpenChannel",
      { aid: aAid },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifyOpenChannelSuccess(aResponse.channel);
      }
    );
  },

  iccExchangeAPDU(aChannel, aCla, aIns, aP1, aP2, aP3, aData, aCallback) {
    if (!aData) {
      if (DEBUG) {
        debug("data is not set , aP3 : " + aP3);
      }
    }

    let apdu = {
      cla: aCla,
      command: aIns,
      p1: aP1,
      p2: aP2,
      p3: aP3,
      data: aData,
    };

    this._radioInterface.sendWorkerMessage(
      "iccExchangeAPDU",
      { channel: aChannel, apdu },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifyExchangeAPDUResponse(
          aResponse.sw1,
          aResponse.sw2,
          aResponse.simResponse
        );
      }
    );
  },

  iccCloseChannel(aChannel, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "iccCloseChannel",
      { channel: aChannel },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifyCloseChannelSuccess();
      }
    );
  },

  sendStkResponse(aCommand, aResponse) {
    let response = lazy.gStkCmdFactory.createResponseMessage(aResponse);
    response.command = lazy.gStkCmdFactory.createCommandMessage(aCommand);
    this._radioInterface.sendWorkerMessage("sendStkTerminalResponse", response);
  },

  sendStkMenuSelection(aItemIdentifier, aHelpRequested) {
    this._radioInterface.sendWorkerMessage("sendStkMenuSelection", {
      itemIdentifier: aItemIdentifier,
      helpRequested: aHelpRequested,
    });
  },

  sendStkTimerExpiration(aTimerId, aTimerValue) {
    this._radioInterface.sendWorkerMessage("sendStkTimerExpiration", {
      timer: {
        timerId: aTimerId,
        timerValue: aTimerValue,
      },
    });
  },

  sendStkEventDownload(aEvent) {
    this._radioInterface.sendWorkerMessage("sendStkEventDownload", {
      event: lazy.gStkCmdFactory.createEventMessage(aEvent),
    });
  },

  getMaxContactCount(aContactType) {
    this._radioInterface.sendWorkerMessage(
      "getMaxContactCount",
      { contactType: aContactType },
      aResponse => {
        if (aResponse.errorMsg) {
          if (DEBUG) {
            debug("getMaxContactCount failed " + aResponse.errorMsg);
          }
          return;
        }
        this._totalRecords = aResponse.totalRecords;
        if (DEBUG) {
          debug("_totalRecords = " + this._totalRecords);
        }
      }
    );
  },

  readContacts(aContactType, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "readICCContacts",
      { contactType: aContactType },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        let iccContacts = [];

        aResponse.contacts.forEach(c => iccContacts.push(new IccContact(c)));
        aCallback.notifyRetrievedIccContacts(iccContacts, iccContacts.length);
      }
    );
  },

  updateContact(aContactType, aContact, aPin2, aCallback) {
    let iccContact = { contactId: aContact.id };

    let names = aContact.getNames();
    if (names && names[0]) {
      iccContact.alphaId = names[0];
    }

    let numbers = aContact.getNumbers();
    if (numbers && numbers[0]) {
      iccContact.number = numbers[0];
      let anrArray = numbers.slice(1);
      let length = anrArray.length;
      if (length > 0) {
        iccContact.anr = [];
        for (let i = 0; i < length; i++) {
          iccContact.anr.push(anrArray[i]);
        }
      }
    }

    let emails = aContact.getEmails();
    if (emails && emails[0]) {
      iccContact.email = emails[0];
    }

    this._radioInterface.sendWorkerMessage(
      "updateICCContact",
      { contactType: aContactType, contact: iccContact, pin2: aPin2 },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }
        aCallback.notifyUpdatedIccContact(new IccContact(aResponse.contact));
      }
    );
  },

  getIccAuthentication(aAppType, aAuthType, aData, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getIccAuthentication",
      { appType: aAppType, authType: aAuthType, data: aData },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        aCallback.notifyAuthResponse(aResponse.responseData);
      }
    );
  },
};

var EXPORTED_SYMBOLS = ["IccService"];
