/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/systemlibs.js");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const RIL = ChromeUtils.import("resource://gre/modules/ril_consts.js");

const { libcutils } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const GONK_MOBILECONNECTIONSERVICE_CID = Components.ID(
  "{0c9c1a96-2c72-4c55-9e27-0ca73eb16f63}"
);
const MOBILECONNECTIONINFO_CID = Components.ID(
  "{8162b3c0-664b-45f6-96cd-f07b4e193b0e}"
);
const MOBILEDEVICEIDENTITIES_CID = Components.ID(
  "{f38786c0-786a-11e5-a837-0800200c9a66}"
);
const MOBILENETWORKINFO_CID = Components.ID(
  "{a6c8416c-09b4-46d1-bf29-6520d677d085}"
);
const MOBILECELLINFO_CID = Components.ID(
  "{0635d9ab-997e-4cdf-84e7-c1883752dff3}"
);
const MOBILECALLFORWARDINGOPTIONS_CID = Components.ID(
  "{e0cf4463-ee63-4b05-ab2e-d94bf764836c}"
);
const MOBILESIGNALSTRENGTH_CID = Components.ID(
  "{0ef33667-389b-4864-930c-0622d2d1833c}"
);
const NEIGHBORINGCELLINFO_CID = Components.ID(
  "{6078cbf1-f34c-44fa-96f8-11a88d4bfdd3}"
);
const GSMCELLINFO_CID = Components.ID("{e3cf3aa0-f992-48fe-967b-ec98a28c8535}");
const WCDMACELLINFO_CID = Components.ID(
  "{62e2c83c-b535-4068-9762-8039fac48106}"
);
const CDMACELLINFO_CID = Components.ID(
  "{40f491f0-dd8b-42fd-af32-aef5b002749a}"
);
const LTECELLINFO_CID = Components.ID("{715e2c76-3b08-41e4-8ea5-e60c5ce6393e}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID = "network-active-changed";
const NS_DATA_CALL_ERROR_TOPIC_ID = "data-call-error";

const TOPIC_MOZSETTINGS_CHANGED = "mozsettings-changed";
const kPrefRilDataServiceId = "ril.data.defaultServiceId";
const kPrefSupportPrimarySimSwitch = "ril.support.primarysim.switch";

const UNKNOWN_VALUE = Ci.nsICellInfo.UNKNOWN_VALUE;
const SIGNAL_UNKNOWN_VALUE = Ci.nsIMobileSignalStrength.SIGNAL_UNKNOWN_VALUE;

const SIGNAL_STRENGTH_NONE_OR_UNKNOWN = 0;
const SIGNAL_STRENGTH_POOR = 1;
const SIGNAL_STRENGTH_MODERATE = 2;
const SIGNAL_STRENGTH_GOOD = 3;
const SIGNAL_STRENGTH_GREAT = 4;

var CFConditionMap = [];
CFConditionMap[Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL] =
  Ci.nsIImsUt.CDIV_CF_UNCONDITIONAL;
CFConditionMap[Ci.nsIMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY] =
  Ci.nsIImsUt.CDIV_CF_BUSY;
CFConditionMap[Ci.nsIMobileConnection.CALL_FORWARD_REASON_NO_REPLY] =
  Ci.nsIImsUt.CDIV_CF_NO_REPLY;
CFConditionMap[Ci.nsIMobileConnection.CALL_FORWARD_REASON_NOT_REACHABLE] =
  Ci.nsIImsUt.CDIV_CF_NOT_REACHABLE;
CFConditionMap[Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING] =
  Ci.nsIImsUt.CDIV_CF_ALL;
CFConditionMap[
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING
] = Ci.nsIImsUt.CDIV_CF_ALL_CONDITIONAL;

var CFReasonMap = [];
CFReasonMap[Ci.nsIImsUt.CDIV_CF_UNCONDITIONAL] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL;
CFReasonMap[Ci.nsIImsUt.CDIV_CF_BUSY] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY;
CFReasonMap[Ci.nsIImsUt.CDIV_CF_NO_REPLY] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_NO_REPLY;
CFReasonMap[Ci.nsIImsUt.CDIV_CF_NOT_REACHABLE] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_NOT_REACHABLE;
CFReasonMap[Ci.nsIImsUt.CDIV_CF_ALL] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING;
CFReasonMap[Ci.nsIImsUt.CDIV_CF_ALL_CONDITIONAL] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING;

var CBProgramMap = [];
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING] =
  Ci.nsIImsUt.CB_BAOC;
CBProgramMap[
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL
] = Ci.nsIImsUt.CB_BOIC;
CBProgramMap[
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME
] = Ci.nsIImsUt.CB_BOIC_EXHC;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_INCOMING] =
  Ci.nsIImsUt.CB_BAIC;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_INCOMING_ROAMING] =
  Ci.nsIImsUt.CB_BIC_WR;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_SERVICE] =
  Ci.nsIImsUt.CB_BA_ALL;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_SERVICE] =
  Ci.nsIImsUt.CB_BA_MO;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_INCOMING_SERVICE] =
  Ci.nsIImsUt.CB_BA_MT;
CBProgramMap[Ci.nsIMobileConnection.CALL_BARRING_ANONYMOUS_INCOMING] =
  Ci.nsIImsUt.CB_BIC_ACR;

// TODO: Customization for rsrp/rssnr range.
const rsrp_thresh = [-140, -128, -118, -108, -98, -44];

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionMessenger",
  "@mozilla.org/ril/system-messenger-helper;1",
  "nsIMobileConnectionMessenger"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkManager",
  "@mozilla.org/network/manager;1",
  "nsINetworkManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDataCallManager",
  "@mozilla.org/datacall/manager;1",
  "nsIDataCallManager"
);

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gTelephonyUtils",
  "resource://gre/modules/TelephonyUtils.jsm",
  "TelephonyUtils"
);

XPCOMUtils.defineLazyGetter(lazy, "gRadioInterfaceLayer", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}
  return ril;
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gImsRegService",
  "@mozilla.org/mobileconnection/imsregservice;1",
  "nsIImsRegService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gCustomizationInfo",
  "@kaiostech.com/customizationinfo;1",
  "nsICustomizationInfo"
);

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gPhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm",
  "PhoneNumberUtils"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gImsServiceManager",
  "@mozilla.org/b2g/imsservicemanager;1",
  "nsIImsServiceManager"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;
function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref;
  try {
    debugPref =
      debugPref || Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
  } catch (e) {
    debugPref = false;
  }
  DEBUG = debugPref || RIL_DEBUG.DEBUG_RIL;
}
updateDebugFlag();

function debug(s) {
  dump("MobileConnectionService: " + s + "\n");
}

function MobileNetworkInfo() {
  this.shortName = null;
  this.longName = null;
  this.mcc = null;
  this.mnc = null;
  this.state = null;
}
MobileNetworkInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileNetworkInfo]),
  classID: MOBILENETWORKINFO_CID,
};

function MobileSignalStrength(aClientId) {
  this._clientId = aClientId;
}
MobileSignalStrength.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileSignalStrength]),
  classID: MOBILESIGNALSTRENGTH_CID,

  /**
   * A utility function to dump debug message.
   */
  _debug(aMessage) {
    dump("MobileSignalStrength[" + this._clientId + "]: " + aMessage + "\n");
  },

  // nsIMobileSignalStrength
  level: 0,

  gsmSignalStrength: 99,
  gsmBitErrorRate: 99,

  cdmaDbm: -1,
  cdmaEcio: -1,
  cdmaEvdoDbm: -1,
  cdmaEvdoEcio: -1,
  cdmaEvdoSNR: -1,

  lteSignalStrength: 99,
  lteRsrp: SIGNAL_UNKNOWN_VALUE,
  lteRsrq: SIGNAL_UNKNOWN_VALUE,
  lteRssnr: SIGNAL_UNKNOWN_VALUE,
  lteCqi: SIGNAL_UNKNOWN_VALUE,
  lteTimingAdvance: SIGNAL_UNKNOWN_VALUE,

  tdscdmaRscp: SIGNAL_UNKNOWN_VALUE,

  _validateInfo(aSignalStrength) {
    let ss = aSignalStrength;

    ss.gsmSignalStrength = ss.gsmSignalStrength > 0 ? ss.gsmSignalStrength : 99;

    ss.cdmaDbm = ss.cdmaDbm > 0 ? -ss.cdmaDbm : -120;
    ss.cdmaEcio = ss.cdmaEcio > 0 ? -ss.cdmaEcio : -160;
    ss.cdmaEvdoDbm = ss.cdmaEvdoDbm > 0 ? -ss.cdmaEvdoDbm : -120;
    ss.cdmaEvdoEcio = ss.cdmaEvdoEcio >= 0 ? -ss.cdmaEvdoEcio : -1;
    ss.cdmaEvdoSNR =
      ss.cdmaEvdoSNR > 0 && ss.cdmaEvdoSNR <= 8 ? ss.cdmaEvdoSNR : -1;

    ss.lteSignalStrength =
      ss.lteSignalStrength >= 0 ? ss.lteSignalStrength : 99;
    ss.lteRsrp =
      ss.lteRsrp >= 44 && ss.lteRsrp <= 140
        ? -ss.lteRsrp
        : SIGNAL_UNKNOWN_VALUE;
    ss.lteRsrq =
      ss.lteRsrq >= 3 && ss.lteRsrq <= 20 ? -ss.lteRsrq : SIGNAL_UNKNOWN_VALUE;
    ss.lteRssnr =
      ss.lteRssnr >= -200 && ss.lteRssnr <= 300
        ? ss.lteRssnr
        : SIGNAL_UNKNOWN_VALUE;

    ss.tdscdmaRscp =
      ss.tdscdmaRscp >= 25 && ss.tdscdmaRscp <= 120
        ? -ss.tdscdmaRscp
        : SIGNAL_UNKNOWN_VALUE;

    return ss;
  },

  _updateLevel(aRadioTech) {
    return new Promise((aResolve, aReject) => {
      let isGsm = true;
      if (
        aRadioTech == RIL.NETWORK_CREG_TECH_IS95A ||
        aRadioTech == RIL.NETWORK_CREG_TECH_IS95B ||
        aRadioTech == RIL.NETWORK_CREG_TECH_1XRTT ||
        aRadioTech == RIL.NETWORK_CREG_TECH_EVDO0 ||
        aRadioTech == RIL.NETWORK_CREG_TECH_EVDOA ||
        aRadioTech == RIL.NETWORK_CREG_TECH_EVDOB ||
        aRadioTech == RIL.NETWORK_CREG_TECH_EHRPD
      ) {
        isGsm = false;
      }

      let level;
      if (isGsm) {
        level = this._getLteLevel();
        if (level == SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
          level = this._getTdscdmaLevel();
          if (level == SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
            level = this._getGsmLevel();
          }
        }
      } else {
        let cdmaLevel = this._getCdmaLevel();
        let evdoLevel = this._getCdmaEvdoLevel();
        if (evdoLevel == SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
          level = cdmaLevel;
        } else if (cdmaLevel == SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
          level = evdoLevel;
        } else {
          level = cdmaLevel < evdoLevel ? cdmaLevel : evdoLevel;
        }
      }

      this.level = level;

      aResolve();
    });
  },

  _getLteLevel() {
    let levelRsrp = -1;
    let levelRssnr = -1;
    let levelRssi = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;

    if (this.lteRsrp > rsrp_thresh[5]) {
      levelRsrp = -1;
    } else if (this.lteRsrp > rsrp_thresh[4]) {
      levelRsrp = SIGNAL_STRENGTH_GREAT;
    } else if (this.lteRsrp > rsrp_thresh[3]) {
      levelRsrp = SIGNAL_STRENGTH_GOOD;
    } else if (this.lteRsrp > rsrp_thresh[2]) {
      levelRsrp = SIGNAL_STRENGTH_MODERATE;
    } else if (this.lteRsrp > rsrp_thresh[1]) {
      levelRsrp = SIGNAL_STRENGTH_POOR;
    } else if (this.lteRsrp > rsrp_thresh[0]) {
      levelRsrp = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (this.lteRssnr > 300) {
      levelRssnr = -1;
    } else if (this.lteRssnr >= 130) {
      levelRssnr = SIGNAL_STRENGTH_GREAT;
    } else if (this.lteRssnr >= 45) {
      levelRssnr = SIGNAL_STRENGTH_GOOD;
    } else if (this.lteRssnr >= 10) {
      levelRssnr = SIGNAL_STRENGTH_MODERATE;
    } else if (this.lteRssnr >= -30) {
      levelRssnr = SIGNAL_STRENGTH_POOR;
    } else if (this.lteRssnr >= -200) {
      levelRssnr = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (DEBUG) {
      this._debug(
        "getLteLevel - lteRsrp  : " +
          this.lteRsrp +
          ". lteRssnr : " +
          this.lteRssnr +
          ". levelRsrp : " +
          levelRsrp +
          ". levelRssnr : " +
          levelRssnr
      );
    }

    if (levelRsrp != -1 && levelRssnr != -1) {
      return levelRsrp < levelRssnr ? levelRssnr : levelRsrp;
    }

    if (levelRssnr != -1) {
      return levelRssnr;
    }

    if (levelRsrp != -1) {
      return levelRsrp;
    }

    if (this.lteSignalStrength > 63) {
      levelRssi = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    } else if (this.lteSignalStrength >= 12) {
      levelRssi = SIGNAL_STRENGTH_GREAT;
    } else if (this.lteSignalStrength >= 8) {
      levelRssi = SIGNAL_STRENGTH_GOOD;
    } else if (this.lteSignalStrength >= 5) {
      levelRssi = SIGNAL_STRENGTH_MODERATE;
    } else if (this.lteSignalStrength >= 0) {
      levelRssi = SIGNAL_STRENGTH_POOR;
    }

    if (DEBUG) {
      this._debug(
        "getLteLevel - rssi : " +
          this.lteSignalStrength +
          ". levelRssi :" +
          levelRssi
      );
    }
    return levelRssi;
  },

  _getGsmLevel() {
    let level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;

    if (this.gsmSignalStrength <= 2 || this.gsmSignalStrength == 99) {
      level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    } else if (this.gsmSignalStrength >= 12) {
      level = SIGNAL_STRENGTH_GREAT;
    } else if (this.gsmSignalStrength >= 8) {
      level = SIGNAL_STRENGTH_GOOD;
    } else if (this.gsmSignalStrength >= 5) {
      level = SIGNAL_STRENGTH_MODERATE;
    } else {
      level = SIGNAL_STRENGTH_POOR;
    }

    if (DEBUG) {
      this._debug("getGsmLevel = " + level);
    }
    return level;
  },

  _getCdmaLevel() {
    let level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    let levelDbm;
    let levelEcio;

    if (this.cdmaDbm >= -75) {
      levelDbm = SIGNAL_STRENGTH_GREAT;
    } else if (this.cdmaDbm >= -85) {
      levelDbm = SIGNAL_STRENGTH_GOOD;
    } else if (this.cdmaDbm >= -95) {
      levelDbm = SIGNAL_STRENGTH_MODERATE;
    } else if (this.cdmaDbm >= -100) {
      levelDbm = SIGNAL_STRENGTH_POOR;
    } else {
      levelDbm = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (this.cdmaEcio >= -90) {
      levelEcio = SIGNAL_STRENGTH_GREAT;
    } else if (this.cdmaEcio >= -110) {
      levelEcio = SIGNAL_STRENGTH_GOOD;
    } else if (this.cdmaEcio >= -130) {
      levelEcio = SIGNAL_STRENGTH_MODERATE;
    } else if (this.cdmaEcio >= -150) {
      levelEcio = SIGNAL_STRENGTH_POOR;
    } else {
      levelEcio = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (levelDbm > levelEcio) {
      level = levelEcio;
    } else {
      level = levelDbm;
    }

    if (DEBUG) {
      this._debug("getCdmaLevel = " + level);
    }
    return level;
  },

  _getCdmaEvdoLevel() {
    let level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    let levelEvdoDbm;
    let levelEvdoSnr;

    if (this.cdmaEvdoDbm >= -65) {
      levelEvdoDbm = SIGNAL_STRENGTH_GREAT;
    } else if (this.cdmaEvdoDbm >= -75) {
      levelEvdoDbm = SIGNAL_STRENGTH_GOOD;
    } else if (this.cdmaEvdoDbm >= -90) {
      levelEvdoDbm = SIGNAL_STRENGTH_MODERATE;
    } else if (this.cdmaEvdoDbm >= -105) {
      levelEvdoDbm = SIGNAL_STRENGTH_POOR;
    } else {
      levelEvdoDbm = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (this.cdmaEvdoSNR >= 7) {
      levelEvdoSnr = SIGNAL_STRENGTH_GREAT;
    } else if (this.cdmaEvdoSNR >= 5) {
      levelEvdoSnr = SIGNAL_STRENGTH_GOOD;
    } else if (this.cdmaEvdoSNR >= 3) {
      levelEvdoSnr = SIGNAL_STRENGTH_MODERATE;
    } else if (this.cdmaEvdoSNR >= 1) {
      levelEvdoSnr = SIGNAL_STRENGTH_POOR;
    } else {
      levelEvdoSnr = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    }

    if (levelEvdoDbm > levelEvdoSnr) {
      level = levelEvdoSnr;
    } else {
      level = levelEvdoDbm;
    }

    if (DEBUG) {
      this._debug("getCdmaEvdoLevel = " + level);
    }
    return level;
  },

  _getTdscdmaLevel() {
    let level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    if (this.tdscdmaRscp > -25 || this.tdscdmaRscp == SIGNAL_UNKNOWN_VALUE) {
      level = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
    } else if (this.tdscdmaRscp >= -49) {
      level = SIGNAL_STRENGTH_GREAT;
    } else if (this.tdscdmaRscp >= -73) {
      level = SIGNAL_STRENGTH_GOOD;
    } else if (this.tdscdmaRscp >= -97) {
      level = SIGNAL_STRENGTH_MODERATE;
    } else if (this.tdscdmaRscp >= -110) {
      level = SIGNAL_STRENGTH_POOR;
    }

    if (DEBUG) {
      this._debug("getTdscdmaLevel = " + level);
    }
    return level;
  },
};

function MobileCellInfo() {
  this.gsmLocationAreaCode = -1;
  this.gsmCellId = -1;
  this.cdmaBaseStationId = -1;
  this.cdmaBaseStationLatitude = -2147483648;
  this.cdmaBaseStationLongitude = -2147483648;
  this.cdmaSystemId = -1;
  this.cdmaNetworkId = -1;
  this.cdmaRoamingIndicator = -1;
  this.cdmaSystemIsInPRL = false;
  this.cdmaDefaultRoamingIndicator = -1;
}
MobileCellInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileCellInfo]),
  classID: MOBILECELLINFO_CID,
};

function MobileConnectionInfo() {
  this.state = null;
  this.connected = false;
  this.emergencyCallsOnly = false;
  this.roaming = false;
  this.network = null;
  this.cell = null;
  this.type = null;
  this.reasonDataDenied = 0;
}
MobileConnectionInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileConnectionInfo]),
  classID: MOBILECONNECTIONINFO_CID,

  state: null,
  connected: false,
  emergencyCallsOnly: false,
  roaming: false,
  network: null,
  cell: null,
  type: null,
  reasonDataDenied: 0,
};

function MobileDeviceIdentities() {}
MobileDeviceIdentities.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileDeviceIdentities]),
  classID: MOBILEDEVICEIDENTITIES_CID,

  imei: null,
  imeisv: null,
  esn: null,
  meid: null,
};

function MobileCallForwardingOptions(aOptions) {
  for (let key in aOptions) {
    this[key] = aOptions[key];
  }
}
MobileCallForwardingOptions.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileCallForwardingOptions]),
  classID: MOBILECALLFORWARDINGOPTIONS_CID,

  // nsIMobileForwardingOptions

  active: false,
  action: Ci.nsIMobileConnection.CALL_FORWARD_ACTION_UNKNOWN,
  reason: Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNKNOWN,
  number: null,
  timeSeconds: -1,
  serviceClass: Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE,
};

function NeighboringCellInfo(aOptions) {
  this.networkType = aOptions.networkType;
  this.gsmLocationAreaCode =
    aOptions.gsmLocationAreaCode !== undefined &&
    aOptions.gsmLocationAreaCode >= 0 &&
    aOptions.gsmLocationAreaCode <= 65535
      ? aOptions.gsmLocationAreaCode
      : UNKNOWN_VALUE;
  this.gsmCellId =
    aOptions.gsmCellId !== undefined &&
    aOptions.gsmCellId >= 0 &&
    aOptions.gsmCellId <= 65535
      ? aOptions.gsmCellId
      : UNKNOWN_VALUE;
  this.wcdmaPsc =
    aOptions.wcdmaPsc !== undefined &&
    aOptions.wcdmaPsc >= 0 &&
    aOptions.wcdmaPsc <= 511
      ? aOptions.wcdmaPsc
      : UNKNOWN_VALUE;
  this.signalStrength =
    aOptions.signalStrength !== undefined &&
    ((aOptions.signalStrength >= 0 && aOptions.signalStrength <= 31) ||
      (aOptions.signalStrength >= -120 && aOptions.signalStrength <= -25))
      ? aOptions.signalStrength
      : UNKNOWN_VALUE;
}
NeighboringCellInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINeighboringCellInfo]),
  classID: NEIGHBORINGCELLINFO_CID,

  isValid() {
    return !(
      this.gsmLocationAreaCode == UNKNOWN_VALUE &&
      this.gsmCellId == UNKNOWN_VALUE &&
      this.wcdmaPsc == UNKNOWN_VALUE &&
      this.signalStrength == UNKNOWN_VALUE
    );
  },

  // nsINeighboringCellInfo

  networkType: null,
  gsmLocationAreaCode: UNKNOWN_VALUE,
  gsmCellId: UNKNOWN_VALUE,
  wcdmaPsc: UNKNOWN_VALUE,
  signalStrength: UNKNOWN_VALUE,
};

function CellInfo(aOptions) {
  this.type = aOptions.type;
  this.registered = aOptions.registered;
  this.timestampType = aOptions.timestampType;
  this.timestamp = aOptions.timestamp;
}
CellInfo.prototype = {
  // nsICellInfo

  type: null,
  registered: false,
  timestampType: Ci.nsICellInfo.TIMESTAMP_TYPE_UNKNOWN,
  timestamp: 0,
};

function GsmCellInfo(aOptions) {
  CellInfo.call(this, aOptions);

  // Cell Identity
  this.mcc =
    aOptions.mcc !== undefined && aOptions.mcc >= 0 && aOptions.mcc <= 999
      ? aOptions.mcc
      : UNKNOWN_VALUE;
  this.mnc =
    aOptions.mnc !== undefined && aOptions.mnc >= 0 && aOptions.mnc <= 999
      ? aOptions.mnc
      : UNKNOWN_VALUE;
  this.lac =
    aOptions.lac !== undefined && aOptions.lac >= 0 && aOptions.lac <= 65535
      ? aOptions.lac
      : UNKNOWN_VALUE;
  this.cid =
    aOptions.cid !== undefined && aOptions.cid >= 0 && aOptions.cid <= 65535
      ? aOptions.cid
      : UNKNOWN_VALUE;

  // Signal Strength
  this.signalStrength =
    aOptions.signalStrength !== undefined &&
    aOptions.signalStrength >= 0 &&
    aOptions.signalStrength <= 31
      ? aOptions.signalStrength
      : UNKNOWN_VALUE;
  this.bitErrorRate =
    aOptions.bitErrorRate !== undefined &&
    aOptions.bitErrorRate >= 0 &&
    aOptions.bitErrorRate <= 7
      ? aOptions.bitErrorRate
      : UNKNOWN_VALUE;
}
GsmCellInfo.prototype = {
  __proto__: CellInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICellInfo, Ci.nsIGsmCellInfo]),
  classID: GSMCELLINFO_CID,

  isValid() {
    return !(
      this.mcc == UNKNOWN_VALUE &&
      this.mnc == UNKNOWN_VALUE &&
      this.lac == UNKNOWN_VALUE &&
      this.cid == UNKNOWN_VALUE &&
      this.signalStrength == UNKNOWN_VALUE &&
      this.bitErrorRate == UNKNOWN_VALUE
    );
  },

  // nsIGsmCellInfo

  mcc: UNKNOWN_VALUE,
  mnc: UNKNOWN_VALUE,
  lac: UNKNOWN_VALUE,
  cid: UNKNOWN_VALUE,
  signalStrength: UNKNOWN_VALUE,
  bitErrorRate: UNKNOWN_VALUE,
};

function WcdmaCellInfo(aOptions) {
  CellInfo.call(this, aOptions);

  // Cell Identity
  this.mcc =
    aOptions.mcc !== undefined && aOptions.mcc >= 0 && aOptions.mcc <= 999
      ? aOptions.mcc
      : UNKNOWN_VALUE;
  this.mnc =
    aOptions.mnc !== undefined && aOptions.mnc >= 0 && aOptions.mnc <= 999
      ? aOptions.mnc
      : UNKNOWN_VALUE;
  this.lac =
    aOptions.lac !== undefined && aOptions.lac >= 0 && aOptions.lac <= 65535
      ? aOptions.lac
      : UNKNOWN_VALUE;
  this.cid =
    aOptions.cid !== undefined && aOptions.cid >= 0 && aOptions.cid <= 268435455
      ? aOptions.cid
      : UNKNOWN_VALUE;
  this.psc =
    aOptions.psc !== undefined && aOptions.psc >= 0 && aOptions.psc <= 511
      ? aOptions.psc
      : UNKNOWN_VALUE;

  // Signal Strength
  this.signalStrength =
    aOptions.signalStrength !== undefined &&
    aOptions.signalStrength >= 0 &&
    aOptions.signalStrength <= 31
      ? aOptions.signalStrength
      : UNKNOWN_VALUE;
  this.bitErrorRate =
    aOptions.bitErrorRate !== undefined &&
    aOptions.bitErrorRate >= 0 &&
    aOptions.bitErrorRate <= 7
      ? aOptions.bitErrorRate
      : UNKNOWN_VALUE;
}
WcdmaCellInfo.prototype = {
  __proto__: CellInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICellInfo, Ci.nsIWcdmaCellInfo]),
  classID: WCDMACELLINFO_CID,

  isValid() {
    return !(
      this.mcc == UNKNOWN_VALUE &&
      this.mnc == UNKNOWN_VALUE &&
      this.lac == UNKNOWN_VALUE &&
      this.cid == UNKNOWN_VALUE &&
      this.psc == UNKNOWN_VALUE &&
      this.signalStrength == UNKNOWN_VALUE &&
      this.bitErrorRate == UNKNOWN_VALUE
    );
  },

  // nsIWcdmaCellInfo

  mcc: UNKNOWN_VALUE,
  mnc: UNKNOWN_VALUE,
  lac: UNKNOWN_VALUE,
  cid: UNKNOWN_VALUE,
  psc: UNKNOWN_VALUE,
  signalStrength: UNKNOWN_VALUE,
  bitErrorRate: UNKNOWN_VALUE,
};

function LteCellInfo(aOptions) {
  CellInfo.call(this, aOptions);

  // Cell Identity
  this.mcc =
    aOptions.mcc !== undefined && aOptions.mcc >= 0 && aOptions.mcc <= 999
      ? aOptions.mcc
      : UNKNOWN_VALUE;
  this.mnc =
    aOptions.mnc !== undefined && aOptions.mnc >= 0 && aOptions.mnc <= 999
      ? aOptions.mnc
      : UNKNOWN_VALUE;
  this.cid =
    aOptions.cid !== undefined && aOptions.cid >= 0 && aOptions.cid <= 268435455
      ? aOptions.cid
      : UNKNOWN_VALUE;
  this.pcid =
    aOptions.pcid !== undefined && aOptions.pcid >= 0 && aOptions.pcid <= 503
      ? aOptions.pcid
      : UNKNOWN_VALUE;
  this.tac =
    aOptions.tac !== undefined && aOptions.tac >= 0 && aOptions.tac <= 65535
      ? aOptions.tac
      : UNKNOWN_VALUE;

  // Signal Strength
  this.signalStrength =
    aOptions.signalStrength !== undefined &&
    aOptions.signalStrength >= 0 &&
    aOptions.signalStrength <= 31
      ? aOptions.signalStrength
      : UNKNOWN_VALUE;
  this.rsrp =
    aOptions.rsrp !== undefined && aOptions.rsrp >= 44 && aOptions.rsrp <= 140
      ? aOptions.rsrp
      : UNKNOWN_VALUE;
  this.rsrq =
    aOptions.rsrq !== undefined && aOptions.rsrq >= 3 && aOptions.rsrq <= 20
      ? aOptions.rsrq
      : UNKNOWN_VALUE;
  this.rssnr =
    aOptions.rssnr !== undefined &&
    aOptions.rssnr >= -200 &&
    aOptions.rssnr <= 300
      ? aOptions.rssnr
      : UNKNOWN_VALUE;
  this.cqi =
    aOptions.cqi !== undefined && aOptions.cqi >= 0 && aOptions.cqi <= 15
      ? aOptions.cqi
      : UNKNOWN_VALUE;
  this.timingAdvance =
    aOptions.timingAdvance !== undefined &&
    aOptions.timingAdvance >= 0 &&
    aOptions.timingAdvance <= 2147483646
      ? aOptions.timingAdvance
      : UNKNOWN_VALUE;
}
LteCellInfo.prototype = {
  __proto__: CellInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICellInfo, Ci.nsILteCellInfo]),
  classID: LTECELLINFO_CID,

  isValid() {
    return !(
      this.mcc == UNKNOWN_VALUE &&
      this.mnc == UNKNOWN_VALUE &&
      this.cid == UNKNOWN_VALUE &&
      this.pcid == UNKNOWN_VALUE &&
      this.tac == UNKNOWN_VALUE &&
      this.signalStrength == UNKNOWN_VALUE &&
      this.rsrp == UNKNOWN_VALUE &&
      this.rsrq == UNKNOWN_VALUE &&
      this.rssnr == UNKNOWN_VALUE &&
      this.cqi == UNKNOWN_VALUE &&
      this.timingAdvance == UNKNOWN_VALUE
    );
  },

  // nsILteCellInfo

  mcc: UNKNOWN_VALUE,
  mnc: UNKNOWN_VALUE,
  cid: UNKNOWN_VALUE,
  pcid: UNKNOWN_VALUE,
  tac: UNKNOWN_VALUE,
  signalStrength: UNKNOWN_VALUE,
  rsrp: UNKNOWN_VALUE,
  rsrq: UNKNOWN_VALUE,
  rssnr: UNKNOWN_VALUE,
  cqi: UNKNOWN_VALUE,
  timingAdvance: UNKNOWN_VALUE,
};

function CdmaCellInfo(aOptions) {
  CellInfo.call(this, aOptions);

  // Cell Identity
  this.networkId =
    aOptions.networkId !== undefined &&
    aOptions.networkId >= 0 &&
    aOptions.networkId <= 65535
      ? aOptions.networkId
      : UNKNOWN_VALUE;
  this.systemId =
    aOptions.systemId !== undefined &&
    aOptions.systemId >= 0 &&
    aOptions.systemId <= 32767
      ? aOptions.systemId
      : UNKNOWN_VALUE;
  this.baseStationId =
    aOptions.baseStationId !== undefined &&
    aOptions.baseStationId >= 0 &&
    aOptions.baseStationId <= 65535
      ? aOptions.baseStationId
      : UNKNOWN_VALUE;
  this.longitude =
    aOptions.longitude !== undefined &&
    aOptions.longitude >= -2592000 &&
    aOptions.longitude <= 2592000
      ? aOptions.longitude
      : UNKNOWN_VALUE;
  this.latitude =
    aOptions.latitude !== undefined &&
    aOptions.latitude >= -1296000 &&
    aOptions.latitude <= 1296000
      ? aOptions.latitude
      : UNKNOWN_VALUE;

  // Signal Strength
  this.cdmaEcio =
    aOptions.cdmaEcio !== undefined && aOptions.cdmaEcio >= 0
      ? aOptions.cdmaEcio
      : UNKNOWN_VALUE;
  this.evdoDbm =
    aOptions.evdoDbm !== undefined && aOptions.evdoDbm >= 0
      ? aOptions.evdoDbm
      : UNKNOWN_VALUE;
  this.evdoEcio =
    aOptions.evdoEcio !== undefined && aOptions.evdoEcio >= 0
      ? aOptions.evdoEcio
      : UNKNOWN_VALUE;
  this.evdoSnr =
    aOptions.evdoSnr !== undefined &&
    aOptions.evdoSnr >= 0 &&
    aOptions.evdoSnr <= 8
      ? aOptions.evdoSnr
      : UNKNOWN_VALUE;
}
CdmaCellInfo.prototype = {
  __proto__: CellInfo.prototype,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICellInfo, Ci.nsICdmaCellInfo]),
  classID: CDMACELLINFO_CID,

  isValid() {
    return !(
      this.networkId == UNKNOWN_VALUE &&
      this.systemId == UNKNOWN_VALUE &&
      this.baseStationId == UNKNOWN_VALUE &&
      this.longitude == UNKNOWN_VALUE &&
      this.latitude == UNKNOWN_VALUE &&
      this.cdmaDbm == UNKNOWN_VALUE &&
      this.cdmaEcio == UNKNOWN_VALUE &&
      this.evdoDbm == UNKNOWN_VALUE &&
      this.evdoEcio == UNKNOWN_VALUE &&
      this.evdoSnr == UNKNOWN_VALUE
    );
  },

  // nsICdmaCellInfo

  networkId: UNKNOWN_VALUE,
  systemId: UNKNOWN_VALUE,
  baseStationId: UNKNOWN_VALUE,
  longitude: UNKNOWN_VALUE,
  latitude: UNKNOWN_VALUE,
  cdmaDbm: UNKNOWN_VALUE,
  cdmaEcio: UNKNOWN_VALUE,
  evdoDbm: UNKNOWN_VALUE,
  evdoEcio: UNKNOWN_VALUE,
  evdoSnr: UNKNOWN_VALUE,
};

function MobileConnectionProvider(aClientId, aRadioInterface, aImsRegHandler) {
  this._clientId = aClientId;
  this._radioInterface = aRadioInterface;
  if (aImsRegHandler) {
    this._imsRegHandler = aImsRegHandler.QueryInterface(
      Ci.nsIGonkImsRegHandler
    );
  } else {
    this._imsRegHandler = null;
  }

  this._imsUt = null;
  this._tokenUtMap = [];
  this._operatorInfo = new MobileNetworkInfo();
  // An array of nsIMobileConnectionListener instances.
  this._listeners = [];

  this.voice = new MobileConnectionInfo();
  this.data = new MobileConnectionInfo();
  this.signalStrength = new MobileSignalStrength(this._clientId);
  this.deviceIdentities = new MobileDeviceIdentities();
}
MobileConnectionProvider.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIMobileConnection,
    Ci.nsIImsUtCallback,
  ]),

  _clientId: null,
  _radioInterface: null,
  _operatorInfo: null,
  _listeners: null,
  _imsRegHandler: null,
  _imsUt: null,

  /**
   * The networks that are currently trying to be selected (or "automatic").
   * This helps ensure that only one network per client is selected at a time.
   */
  _selectingNetwork: null,

  /**
   * The two radio states below stand for the user expectation and the hardware
   * status, respectively. |radioState| will be updated based on their values.
   */
  _expectedRadioState: RIL.GECKO_RADIOSTATE_UNKNOWN,
  _hardwareRadioState: RIL.GECKO_RADIOSTATE_UNKNOWN,

  voice: null,
  data: null,
  networkSelectionMode: Ci.nsIMobileConnection.NETWORK_SELECTION_MODE_UNKNOWN,
  radioState: Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN,
  lastKnownNetwork: null,
  lastKnownHomeNetwork: null,
  deviceIdentities: null,
  isInEmergencyCbMode: false,
  signalStrength: null,

  /**
   * Set flag to indicate there's setPreferredNetworkType needed after modem restart
   */
  _preferredNetworkType:
    Ci.nsIMobileConnection.PREFERRED_NETWORK_TYPE_LTE_WCDMA_GSM_CDMA_EVDO,
  _needRestorePreferredNetworkType: false,

  // Maps tokens we send out with UT message to the message callback.
  _tokenUtMap: [],

  /**
   * A utility function to dump debug message.
   */
  _debug(aMessage) {
    //dump("MobileConnectionProvider[" + this._clientId + "]: " + aMessage + "\n");
    console.log(
      "MobileConnectionProvider[" + this._clientId + "]: " + aMessage + "\n"
    );
  },

  /**
   * Function to power off radio and set modem off
   * before device power down or reboot.
   */
  shutdownRequest() {
    return new Promise((aResolve, aReject) => {
      this.setRadioEnabled(false, {
        notifySuccess: () => {
          aResolve();
        },
        notifyError: errorMsg => {
          if (DEBUG) {
            this._debug("setRadioEnabled notifyError:" + errorMsg);
          }
          aResolve();
        },
      });
    })
      .then(() => {
        return new Promise((aResolve, aReject) => {
          this._radioInterface.sendWorkerMessage(
            "shutdownRequest",
            null,
            aResponse => {
              aResolve();
            }
          );
        });
      })
      .catch(aError => {
        this._debug("shutdownRequest Error:" + aError);
      });
  },

  /**
   * A utility function to get supportedNetworkTypes from system property.
   */
  _getSupportedNetworkTypes() {
    let key = "persist.ril." + this._clientId + ".network_types";
    let supportedNetworkTypes = libcutils.property_get(key, "").split(",");

    // If mozRIL system property is not available, fallback to AOSP system
    // property for support network types.
    if (supportedNetworkTypes.length === 1 && supportedNetworkTypes[0] === "") {
      key = "ro.telephony.default_network";
      let indexString = libcutils.property_get(key, "");
      let index = parseInt(indexString, 10);
      if (DEBUG) {
        this._debug("Fallback to " + key + ": " + index);
      }

      let networkTypes = RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO[index];
      supportedNetworkTypes = networkTypes
        ? networkTypes.replace(/-auto/g, "").split("/")
        : RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT.split(",");
    }

    let enumNetworkTypes = [];
    for (let type of supportedNetworkTypes) {
      // If the value in system property is not valid, use the default one which
      // is defined in ril_consts.js.
      if (!RIL.GECKO_SUPPORTED_NETWORK_TYPES.includes(type)) {
        if (DEBUG) {
          this._debug("Unknown network type: " + type);
        }
        RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT.split(",").forEach(aType => {
          enumNetworkTypes.push(
            RIL.GECKO_SUPPORTED_NETWORK_TYPES.indexOf(aType)
          );
        });
        break;
      }
      enumNetworkTypes.push(RIL.GECKO_SUPPORTED_NETWORK_TYPES.indexOf(type));
    }
    if (DEBUG) {
      this._debug("Supported Network Types: " + enumNetworkTypes);
    }

    return enumNetworkTypes;
  },

  /**
   * Helper for guarding us against invalid mode for clir.
   */
  _isValidClirMode(aMode) {
    switch (aMode) {
      case Ci.nsIMobileConnection.CLIR_DEFAULT:
      case Ci.nsIMobileConnection.CLIR_INVOCATION:
      case Ci.nsIMobileConnection.CLIR_SUPPRESSION:
        return true;
      default:
        return false;
    }
  },

  /**
   * Fix the roaming. RIL can report roaming in some case it is not
   * really the case. See bug 787967
   *
   * See bug 12354, remove check at _updateConnectionInfo to avoid wrong status
   * update, it looks like andorid only left this check for CDMA case.
   * http://androidxref.com/6.0.1_r10/xref/frameworks/opt/telephony/src/java/com
   * /android/internal/telephony/cdma/CdmaServiceStateTracker.java#1573
   */
  _checkRoamingBetweenOperators(aNetworkInfo) {
    let icc = lazy.gIccService.getIccByServiceId(this._clientId);
    let iccInfo = icc ? icc.iccInfo : null;
    let operator = aNetworkInfo.network;
    let state = aNetworkInfo.state;

    if (
      !iccInfo ||
      !operator ||
      state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED
    ) {
      return false;
    }

    let spn = iccInfo.spn && iccInfo.spn.toLowerCase();
    let longName = operator.longName && operator.longName.toLowerCase();
    let shortName = operator.shortName && operator.shortName.toLowerCase();

    let equalsLongName = longName && spn == longName;
    let equalsShortName = shortName && spn == shortName;
    let equalsMcc = iccInfo.mcc == operator.mcc;

    let newRoaming =
      aNetworkInfo.roaming &&
      !(equalsMcc && (equalsLongName || equalsShortName));
    if (newRoaming === aNetworkInfo.roaming) {
      return false;
    }

    aNetworkInfo.roaming = newRoaming;
    return true;
  },

  /**
   * The design of this updating function is to update the attribute in
   * |aDestInfo| *only if* new data (e.g. aSrcInfo) contains the same attribute.
   * Thus, for the attribute in |aDestInfo| that isn't showed in |aSrcInfo|, it
   * should just keep the original value unchanged.
   */
  _updateConnectionInfo(aDestInfo, aSrcInfo) {
    let isUpdated = false;
    for (let key in aSrcInfo) {
      if (key === "network" || key === "cell") {
        // nsIMobileNetworkInfo and nsIMobileCellInfo are handled explicitly below.
        continue;
      }

      if (aDestInfo[key] !== aSrcInfo[key]) {
        isUpdated = true;
        aDestInfo[key] = aSrcInfo[key];
      }
    }

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (aDestInfo.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      aDestInfo.cell = null;
      aDestInfo.network = null;
    } else {
      aDestInfo.network = this._operatorInfo;

      // If no new cell data is passed, we should just keep the original cell
      // data unchanged.
      if (aSrcInfo.cell) {
        if (!aDestInfo.cell) {
          aDestInfo.cell = new MobileCellInfo();
        }

        isUpdated =
          this._updateInfo(aDestInfo.cell, aSrcInfo.cell) || isUpdated;
      }
    }

    return isUpdated;
  },

  /**
   * The design of this updating function is to update the attribute in
   * |aDestInfo| *only if* new data (e.g. aSrcInfo) contains the same attribute.
   * Thus, for the attribute in |aDestInfo| that isn't showed in |aSrcInfo|, it
   * should just keep the original value unchanged.
   */
  _updateInfo(aDestInfo, aSrcInfo) {
    let isUpdated = false;
    for (let key in aSrcInfo) {
      if (aDestInfo[key] !== aSrcInfo[key]) {
        isUpdated = true;
        aDestInfo[key] = aSrcInfo[key];
      }
    }
    return isUpdated;
  },

  _rulesToCallForwardingOptions(aRules) {
    return aRules.map(rule => new MobileCallForwardingOptions(rule));
  },

  _dispatchNotifyError(aCallback, aErrorMsg) {
    Services.tm.currentThread.dispatch(
      () => aCallback.notifyError(aErrorMsg),
      Ci.nsIThread.DISPATCH_NORMAL
    );
  },

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

  deliverListenerEvent(aName, aArgs) {
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
          this._debug("listener for " + aName + " threw an exception: " + e);
        }
      }
    }
  },

  updateVoiceInfo(aNewInfo, aBatch = false) {
    let isUpdated = this._updateConnectionInfo(this.voice, aNewInfo);
    if (isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyVoiceChanged");
    }
  },

  updateDataInfo(aNewInfo, aBatch = false) {
    // For the data connection, the `connected` flag indicates whether
    // there's an active data call. We get correct `connected` state here.
    let active = lazy.gNetworkManager?.activeNetworkInfo?.QueryInterface(
      Ci.nsIRilNetworkInfo
    );

    aNewInfo.connected = false;
    if (
      active &&
      active.type === Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE &&
      active.serviceId === this._clientId
    ) {
      aNewInfo.connected = true;
    }

    let isUpdated = this._updateConnectionInfo(this.data, aNewInfo);
    if (isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyDataChanged");
    }

    if (isUpdated) {
      this._ensureDataRegistration();
    }
  },

  _dataRegistrationFailed: false,
  _ensureDataRegistration() {
    let isDataRegistered =
      this.data &&
      this.data.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED;
    let isVoiceRegistered =
      this.voice &&
      this.voice.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED;

    if (isVoiceRegistered && isDataRegistered) {
      if (DEBUG) {
        this._debug("Voice and data registered.");
      }
      this._dataRegistrationFailed = false;
      return;
    }

    if (
      isVoiceRegistered &&
      !isDataRegistered &&
      this._clientId == lazy.gDataCallManager.dataDefaultServiceId
    ) {
      // We have been here before, no more recovery.
      if (this._dataRegistrationFailed) {
        if (DEBUG) {
          this._debug(
            "Voice and data not consistent: " +
              this.voice.state +
              " != " +
              this.data.state +
              "."
          );
        }
        return;
      }

      if (DEBUG) {
        this._debug(
          "Voice and data not consistent: " +
            this.voice.state +
            " != " +
            this.data.state +
            ", try to recover."
        );
      }

      this._dataRegistrationFailed = true;
      // If there is any ongoing call, wait for them to disconnect.
      if (lazy.gTelephonyUtils.hasAnyCalls(this._clientId)) {
        lazy.gTelephonyUtils.waitForNoCalls(this._clientId).then(() => {
          if (this._dataRegistrationFailed) {
            this._recoverDataRegistration();
          }
        });
        return;
      }

      this._recoverDataRegistration();
    }
  },

  /**
   * To recover data registration, get the current preferred network type first,
   * then set it to a temporary preferred network type, and last set back to the
   * previous preferred network type. This is will cause deregistration and
   * registration on both voice and data networks.
   */
  _recoverDataRegistration() {
    if (DEBUG) {
      this._debug("Trying to recover data registration...");
    }

    let currentPreferredNetworkType;

    let resetPreferredNetworkType = () => {
      this.setPreferredNetworkType(currentPreferredNetworkType, {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIMobileConnectionCallback,
        ]),
        notifySuccess: () => {},
        notifyError: aErrorMsg => {},
      });
    };

    let setTemporaryPreferredNetworkType = () => {
      this.setPreferredNetworkType(
        Ci.nsIMobileConnection.PREFERRED_NETWORK_TYPE_WCDMA_GSM_CDMA_EVDO,
        {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess: () => resetPreferredNetworkType(),
          notifyError: aErrorMsg => resetPreferredNetworkType(),
        }
      );
    };

    this.getPreferredNetworkType({
      QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileConnectionCallback]),
      notifyGetPreferredNetworkTypeSuccess: networkType => {
        currentPreferredNetworkType = networkType;
        setTemporaryPreferredNetworkType();
      },
      notifyError: aErrorMsg => {
        currentPreferredNetworkType =
          Ci.nsIMobileConnection.PREFERRED_NETWORK_TYPE_LTE_WCDMA_GSM_CDMA_EVDO;
        setTemporaryPreferredNetworkType();
      },
    });
  },

  updateOperatorInfo(aNewInfo, aBatch = false) {
    let isUpdated = this._updateInfo(this._operatorInfo, aNewInfo);

    // Update lastKnownNetwork
    if (this._operatorInfo.mcc && this._operatorInfo.mnc) {
      let network = this._operatorInfo.mcc + "-" + this._operatorInfo.mnc;
      if (this.lastKnownNetwork !== network) {
        if (DEBUG) {
          this._debug("lastKnownNetwork now is " + network);
        }

        this.lastKnownNetwork = network;
        this.deliverListenerEvent("notifyLastKnownNetworkChanged");
      }
    }

    // If the voice is unregistered, no need to send notification.
    if (
      this.voice.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
      isUpdated &&
      !aBatch
    ) {
      this.deliverListenerEvent("notifyVoiceChanged");
    }

    // If the data is unregistered, no need to send notification.
    if (
      this.data.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
      isUpdated &&
      !aBatch
    ) {
      this.deliverListenerEvent("notifyDataChanged");
    }
  },

  updateSignalInfo(aNewSignalInfo) {
    let newSignalInfo = this.signalStrength._validateInfo(aNewSignalInfo);
    if (this._updateInfo(this.signalStrength, newSignalInfo)) {
      if (DEBUG) {
        this._debug("updateSignalInfo : " + JSON.stringify(aNewSignalInfo));
      }
      let radioTech = RIL.NETWORK_CREG_TECH_UNKNOWN;
      if (this.data.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
        radioTech = this.data.radioTech;
      } else if (
        this.voice.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED
      ) {
        radioTech = this.voice.radioTech;
      }

      this.signalStrength._updateLevel(radioTech).then(() => {
        this.deliverListenerEvent("notifySignalStrengthChanged");
      });
    }
  },

  updateRadioState(aMessage, aCallback = null) {
    switch (aMessage.msgType) {
      case "ExpectedRadioState":
        this._expectedRadioState = aMessage.msgData;
        break;
      case "HardwareRadioState":
        this._hardwareRadioState = aMessage.msgData;
        break;
      default:
        if (DEBUG) {
          this._debug("updateRadioState: Invalid message type");
        }
        return;
    }

    if (
      aMessage.msgType === "ExpectedRadioState" &&
      aCallback &&
      this._hardwareRadioState === this._expectedRadioState
    ) {
      // Early resolved
      aCallback.notifySuccess();
      return;
    }

    let newState;
    switch (this._expectedRadioState) {
      case RIL.GECKO_RADIOSTATE_ENABLED:
        newState =
          this._hardwareRadioState === this._expectedRadioState
            ? Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED
            : Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLING;
        break;

      case RIL.GECKO_RADIOSTATE_DISABLED:
        newState =
          this._hardwareRadioState === this._expectedRadioState
            ? Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLED
            : Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLING;
        break;

      default:
        /* RIL.GECKO_RADIOSTATE_UNKNOWN */
        switch (this._hardwareRadioState) {
          case RIL.GECKO_RADIOSTATE_ENABLED:
            newState = Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED;
            break;
          case RIL.GECKO_RADIOSTATE_DISABLED:
            newState = Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLED;
            break;
          default:
            /* RIL.GECKO_RADIOSTATE_UNKNOWN */
            newState = Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN;
        }
    }

    // This update is triggered by underlying layers and the state is UNKNOWN
    if (
      aMessage.msgType === "HardwareRadioState" &&
      aMessage.msgData === RIL.GECKO_RADIOSTATE_UNKNOWN
    ) {
      // TODO: Find a better way than just setting the radio state to UNKNOWN
      newState = Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN;
    }

    if (
      newState === Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLING ||
      newState === Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLING
    ) {
      let action = this._expectedRadioState === RIL.GECKO_RADIOSTATE_ENABLED;
      this._radioInterface.sendWorkerMessage(
        "setRadioEnabled",
        { enabled: action },
        function(aResponse) {
          if (!aCallback) {
            return false;
          }
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }
          aCallback.notifySuccess();
          return false;
        }
      );
    }

    if (DEBUG) {
      this._debug("Current Radio State is '" + newState + "'");
    }
    if (this.radioState === newState) {
      return;
    }

    this.radioState = newState;
    if (
      newState === Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED &&
      this._needRestorePreferredNetworkType
    ) {
      this.setPreferredNetworkType(this._preferredNetworkType, {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIMobileConnectionCallback,
        ]),
        notifySuccess: () => {},
        notifyError: aErrorMsg => {},
      });
      this._needRestorePreferredNetworkType = false;
    }

    this.deliverListenerEvent("notifyRadioStateChanged");
  },

  updateDeviceIdentities(aImei, aImeisv, aEsn, aMeid) {
    let newDeviceIdentities = new MobileDeviceIdentities();
    newDeviceIdentities.imei = aImei;
    newDeviceIdentities.imeisv = aImeisv;
    newDeviceIdentities.esn = aEsn;
    newDeviceIdentities.meid = aMeid;
    if (this._updateInfo(this.deviceIdentities, newDeviceIdentities)) {
      if (DEBUG) {
        this._debug(
          "updateDeviceIdentities : " + JSON.stringify(this.deviceIdentities)
        );
      }
      this.deliverListenerEvent("notifyDeviceIdentitiesChanged");
    }
  },

  notifyCFStateChanged(aAction, aReason, aNumber, aTimeSeconds, aServiceClass) {
    this.deliverListenerEvent("notifyCFStateChanged", [
      aAction,
      aReason,
      aNumber,
      aTimeSeconds,
      aServiceClass,
    ]);
  },

  getSupportedNetworkTypes(aTypes) {
    aTypes.value = this._getSupportedNetworkTypes().slice();
    return aTypes.value.length;
  },

  getNetworks(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getAvailableNetworks",
      null,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        let networks = aResponse.networks;
        for (let i = 0; i < networks.length; i++) {
          let info = new MobileNetworkInfo();
          this._updateInfo(info, networks[i]);
          networks[i] = info;
        }

        aCallback.notifyGetNetworksSuccess(networks.length, networks);
        return false;
      }.bind(this)
    );
  },

  selectNetwork(aNetwork, aCallback) {
    if (
      !aNetwork ||
      isNaN(parseInt(aNetwork.mcc, 10)) ||
      isNaN(parseInt(aNetwork.mnc, 10))
    ) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_ARGUMENTS);
      return;
    }

    if (this._selectingNetwork) {
      this._dispatchNotifyError(aCallback, "AlreadySelectingANetwork");
      return;
    }

    let options = { mcc: aNetwork.mcc, mnc: aNetwork.mnc };
    this._selectingNetwork = options;
    this._radioInterface.sendWorkerMessage(
      "selectNetwork",
      options,
      function(aResponse) {
        this._selectingNetwork = null;
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }.bind(this)
    );
  },

  selectNetworkAutomatically(aCallback) {
    if (this._selectingNetwork) {
      this._dispatchNotifyError(aCallback, "AlreadySelectingANetwork");
      return;
    }

    this._selectingNetwork = "automatic";
    this._radioInterface.sendWorkerMessage(
      "selectNetworkAuto",
      null,
      function(aResponse) {
        this._selectingNetwork = null;
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }.bind(this)
    );
  },

  setPreferredNetworkType(aType, aCallback) {
    if (this.radioState !== Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._preferredNetworkType = aType;
    this._radioInterface.sendWorkerMessage(
      "setPreferredNetworkType",
      { type: aType },
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }
    );
  },

  getPreferredNetworkType(aCallback) {
    if (this.radioState !== Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._radioInterface.sendWorkerMessage(
      "getPreferredNetworkType",
      null,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifyGetPreferredNetworkTypeSuccess(aResponse.type);
        return false;
      }
    );
  },

  setRoamingPreference(aMode, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "setRoamingPreference",
      { mode: aMode },
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }
    );
  },

  getRoamingPreference(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "queryRoamingPreference",
      null,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifyGetRoamingPreferenceSuccess(aResponse.mode);
        return false;
      }
    );
  },

  setVoicePrivacyMode(aEnabled, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "setVoicePrivacyMode",
      { enabled: aEnabled },
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }
    );
  },

  getVoicePrivacyMode(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "queryVoicePrivacyMode",
      null,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccessWithBoolean(aResponse.enabled);
        return false;
      }
    );
  },

  setCallForwarding(
    aAction,
    aReason,
    aNumber,
    aTimeSeconds,
    aServiceClass,
    aCallback
  ) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let condition = CFConditionMap[aReason];
        let requestId = this._imsUt.updateCallForward(
          aAction,
          condition,
          aNumber,
          aServiceClass,
          aTimeSeconds
        );

        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "setCallForwarding",
            action: aAction,
            reason: aReason,
            number: aNumber,
            timeseconds: aTimeSeconds,
            serviceclass: aServiceClass,
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error setCallForwarding requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt is null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      let options = {
        action: aAction,
        reason: aReason,
        number: aNumber,
        timeSeconds: aTimeSeconds,
        serviceClass: aServiceClass,
      };

      this._radioInterface.sendWorkerMessage(
        "setCallForward",
        options,
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          this.notifyCFStateChanged(
            aResponse.action,
            aResponse.reason,
            aResponse.number,
            aResponse.timeSeconds,
            aResponse.serviceClass
          );
          aCallback.notifySuccess();
          return false;
        }.bind(this)
      );
    }
  },

  getCallForwarding(aReason, aServiceClass, aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        // Send the request.
        let condition = CFConditionMap[aReason];
        let requestId = this._imsUt.queryCallForward(condition, "");

        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "getCallForwarding",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error getCallForwarding requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      this._radioInterface.sendWorkerMessage(
        "queryCallForwardStatus",
        { reason: aReason, serviceClass: aServiceClass },
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          let infos = this._rulesToCallForwardingOptions(aResponse.rules);
          aCallback.notifyGetCallForwardingSuccess(infos.length, infos);
          return false;
        }.bind(this)
      );
    }
  },

  setCallBarring(aProgram, aEnabled, aPassword, aServiceClass, aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let cbType = CBProgramMap[aProgram];
        let requestId = this._imsUt.updateCallBarring(
          cbType,
          aEnabled,
          [],
          aServiceClass,
          aPassword
        );

        if (requestId >= 0) {
          let token = {
            request: "setCallBarring",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error setCallBarring requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      let options = {
        program: aProgram,
        enabled: aEnabled,
        password: aPassword,
        serviceClass: aServiceClass,
      };

      this._radioInterface.sendWorkerMessage(
        "setCallBarring",
        options,
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          aCallback.notifySuccess();
          return false;
        }
      );
    }
  },

  getCallBarring(aProgram, aPassword, aServiceClass, aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let cbType = CBProgramMap[aProgram];
        let requestId = this._imsUt.queryCallBarring(cbType, aServiceClass);
        if (DEBUG) {
          this._debug("queryCallBarring with id: " + requestId);
        }

        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "getCallBarring",
            program: aProgram,
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error getCallBarring requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      let options = {
        program: aProgram,
        password: aPassword,
        serviceClass: aServiceClass,
      };

      this._radioInterface.sendWorkerMessage(
        "queryCallBarringStatus",
        options,
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          aCallback.notifyGetCallBarringSuccess(
            aResponse.program,
            aResponse.enabled,
            aResponse.serviceClass
          );
          return false;
        }
      );
    }
  },

  changeCallBarringPassword(aPin, aNewPin, aCallback) {
    let options = {
      pin: aPin,
      newPin: aNewPin,
    };

    this._radioInterface.sendWorkerMessage(
      "changeCallBarringPassword",
      options,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifySuccess();
        return false;
      }
    );
  },

  setCallWaiting(aEnabled, aServiceClass, aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let requestId = this._imsUt.updateCallWaiting(aEnabled, aServiceClass);
        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "setCallWaiting",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error setCallBarring requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      let options = {
        enabled: aEnabled,
        serviceClass: aServiceClass,
      };

      this._radioInterface.sendWorkerMessage(
        "setCallWaiting",
        options,
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          aCallback.notifySuccess();
          return false;
        }
      );
    }
  },

  getCallWaiting(aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let requestId = this._imsUt.queryCallWaiting();
        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "getCallWaiting",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error getCallBarring requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      this._radioInterface.sendWorkerMessage("queryCallWaiting", null, function(
        aResponse
      ) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifyGetCallWaitingSuccess(aResponse.serviceClass);
        return false;
      });
    }
  },

  setCallingLineIdRestriction(aMode, aCallback) {
    if (!this._isValidClirMode(aMode)) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_ARGUMENTS);
      return;
    }

    if (DEBUG) {
      this._debug("setCallingLineIdRestriction: " + aMode);
    }

    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let requestId = this._imsUt.updateCLIR(aMode);
        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "setCallingLineIdRestriction",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug(
            "Error setCallingLineIdRestriction requestId =" + requestId
          );
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      if (
        this.radioState !== Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED
      ) {
        this._dispatchNotifyError(
          aCallback,
          RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE
        );
        return;
      }

      this._radioInterface.sendWorkerMessage(
        "setCLIR",
        { clirMode: aMode },
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return false;
          }

          this.deliverListenerEvent("notifyClirModeChanged", [aResponse.mode]);
          aCallback.notifySuccess();
          return false;
        }.bind(this)
      );
    }
  },

  getCallingLineIdRestriction(aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      if (!this._imsUt) {
        this.initUt();
      }

      if (this._imsUt) {
        let requestId = this._imsUt.queryCLIR();
        if (requestId >= 0) {
          // Store the token for the callback notify.
          let token = {
            request: "getCallingLineIdRestriction",
            callback: aCallback,
          };
          this._tokenUtMap[requestId] = token;
        } else {
          this._debug("Error getCallBarring requestId =" + requestId);
          this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
        }
      } else {
        this._debug("Error _imsUt null.");
        this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
    } else {
      if (
        this.radioState !== Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED
      ) {
        this._dispatchNotifyError(
          aCallback,
          RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE
        );
        return;
      }

      this._radioInterface.sendWorkerMessage("getCLIR", null, function(
        aResponse
      ) {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return false;
        }

        aCallback.notifyGetClirStatusSuccess(aResponse.n, aResponse.m);
        return false;
      });
    }
  },

  exitEmergencyCbMode(aCallback) {
    if (this._imsRegHandler && this._imsRegHandler.isImsRegistered) {
      try {
        let imsEcbm = lazy.gImsServiceManager.getEcbm(this._clientId);
        if (imsEcbm) {
          imsEcbm.exitEmergencyCallbackMode();
          aCallback.notifySuccess();
          return;
        }
      } catch (e) {
        if (DEBUG) {
          this._debug("getEcbm failed");
        }
      }
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
    } else {
      this._radioInterface.sendWorkerMessage(
        "sendExitEmergencyCbModeRequest",
        null,
        function(aResponse) {
          if (aResponse.errorMsg) {
            aCallback.notifyError(aResponse.errorMsg);
            return;
          }

          aCallback.notifySuccess();
        }
      );
    }
  },

  setRadioEnabled(aEnabled, aCallback) {
    if (DEBUG) {
      this._debug("setRadioEnabled: " + aEnabled);
    }

    // Before sending a equest to |ril_worker.js|, we should check radioState.
    switch (this.radioState) {
      case Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN:
      case Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED:
      case Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLED:
        break;
      default:
        aCallback.notifyError("InvalidStateError");
        return;
    }

    let message = {
      msgType: "ExpectedRadioState",
      msgData: aEnabled
        ? RIL.GECKO_RADIOSTATE_ENABLED
        : RIL.GECKO_RADIOSTATE_DISABLED,
    };
    this.updateRadioState(message, aCallback);
  },

  getCellInfoList(aCallback) {
    this._radioInterface.sendWorkerMessage("getCellInfoList", null, function(
      aResponse
    ) {
      if (aResponse.errorMsg) {
        aCallback.notifyGetCellInfoListFailed(aResponse.errorMsg);
        return;
      }

      let cellInfoList = [];
      let count = aResponse.result.length;
      for (let i = 0; i < count; i++) {
        let srcCellInfo = aResponse.result[i];
        let cellInfo;
        switch (srcCellInfo.type) {
          case RIL.CELL_INFO_TYPE_GSM:
            cellInfo = new GsmCellInfo(srcCellInfo);
            break;
          case RIL.CELL_INFO_TYPE_WCDMA:
            cellInfo = new WcdmaCellInfo(srcCellInfo);
            break;
          case RIL.CELL_INFO_TYPE_LTE:
            cellInfo = new LteCellInfo(srcCellInfo);
            break;
          case RIL.CELL_INFO_TYPE_CDMA:
            cellInfo = new CdmaCellInfo(srcCellInfo);
            break;
        }

        if (!cellInfo || !cellInfo.isValid()) {
          continue;
        }

        cellInfoList.push(cellInfo);
      }
      aCallback.notifyGetCellInfoList(cellInfoList.length, cellInfoList);
    });
  },

  getNeighboringCellIds(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getNeighboringCellIds",
      null,
      function(aResponse) {
        if (aResponse.errorMsg) {
          aCallback.notifyGetNeighboringCellIdsFailed(aResponse.errorMsg);
          return;
        }

        let neighboringCellIds = [];
        let count = aResponse.result.length;
        for (let i = 0; i < count; i++) {
          let srcCellInfo = aResponse.result[i];
          let cellInfo = new NeighboringCellInfo(srcCellInfo);
          if (cellInfo && cellInfo.isValid()) {
            neighboringCellIds.push(cellInfo);
          }
        }
        aCallback.notifyGetNeighboringCellIds(
          neighboringCellIds.length,
          neighboringCellIds
        );
      }
    );
  },

  getIdentities(aCallback) {
    let deviceId = new MobileDeviceIdentities();
    deviceId.imei = this.deviceIdentities.imei;
    deviceId.imeisv = this.deviceIdentities.imeisv;
    deviceId.esn = this.deviceIdentities.esn;
    deviceId.meid = this.deviceIdentities.emid;
    aCallback.notifyGetDeviceIdentitiesRequestSuccess(deviceId);
  },

  stopNetworkScan(aCallback) {
    this._radioInterface.sendWorkerMessage("stopNetworkScan", null, function(
      aResponse
    ) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    });
  },

  initUt() {
    if (!this._imsUt) {
      if (this._imsRegHandler && this._imsRegHandler) {
        let imsMMTel = this._imsRegHandler.imsMMTelFeature;
        if (imsMMTel) {
          this._imsUt = imsMMTel.getUtInterface();
          if (this._imsUt) {
            if (DEBUG) {
              this._debug("set imsut callback this");
            }
            this._imsUt.setCallback(this);
          } else if (DEBUG) {
            this._debug("_imsUt not ready.");
          }
        } else if (DEBUG) {
          this._debug("imsMMTel not ready.");
        }
      } else if (DEBUG) {
        this._debug("_imsRegHandler not ready.");
      }
    }
  },

  //nsIImsUtCallback implement.
  onUtConfigurationUpdated(aId) {
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    if (DEBUG) {
      this._debug(
        "onUtConfigurationUpdated. aId= " +
          aId +
          ", aReqeust = " +
          token.request
      );
    }

    if (token.request === "setCallForwarding") {
      this.notifyCFStateChanged(
        token.action,
        token.reason,
        token.number,
        token.timeseconds,
        token.serviceclass
      );
      token.callback.notifySuccess();
    } else {
      token.callback.notifySuccess();
    }
    delete this._tokenUtMap[aId];
  },

  onUtConfigurationUpdateFailed(aId, aError) {
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    if (DEBUG) {
      this._debug(
        "onUtConfigurationUpdateFailed. aId=" +
          aId +
          ", aReqeust=" +
          token.reqeust +
          ", aError.code=" +
          aError.code +
          ", aError.extraCode=" +
          aError.extraCode +
          ", aError.extraMessage=" +
          aError.extraMessage
      );
    }
    // If ImsReasonInfo object does not have a String error code, use a
    // Gecko default error string.
    let errorMsg =
      aError.extraMessage ||
      RIL.RIL_IMSCALL_FAILCAUSE_TO_GECKO_CALL_ERROR[aError.code];

    this._dispatchNotifyError(token.callback, errorMsg);
    delete this._tokenUtMap[aId];
  },

  onCallForwardQueried(aId, aCfInfos) {
    if (DEBUG) {
      this._debug(
        "onCallForwardQueried. aId=" +
          aId +
          " , aCfInfos=" +
          JSON.stringify(aCfInfos)
      );
    }
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    // For mapping from the nsIImsCallForwardInfo to nsIMobileCallForwardingOptions format.
    let cfInfos = [];
    for (let i = 0; i < aCfInfos.length; i++) {
      let cfInfo = {};
      cfInfo.active = aCfInfos[i].status == Ci.nsIImsUt.CF_STATUS_ACTIVE;
      cfInfo.action = Ci.nsIMobileConnection.CALL_FORWARD_ACTION_QUERY_STATUS;
      cfInfo.reason = CFReasonMap[aCfInfos[i].condition];
      cfInfo.number = aCfInfos[i].number;
      cfInfo.timeSeconds = aCfInfos[i].timeSeconds;
      cfInfo.serviceClass = aCfInfos[i].serviceClass;
      cfInfos.push(cfInfo);
    }

    let infos = this._rulesToCallForwardingOptions(cfInfos);
    token.callback.notifyGetCallForwardingSuccess(infos.length, infos);
    delete this._tokenUtMap[aId];
  },

  onCallBarringQueried(aId, aCbInfo) {
    if (DEBUG) {
      this._debug(
        "onCallBarringQueried. aId=" +
          aId +
          " , aCbInfo=" +
          JSON.stringify(aCbInfo)
      );
    }
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    let enabled = aCbInfo.status == Ci.nsIImsUt.STATUS_ENABLED;

    let serviceClass = enabled
      ? Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
      : Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE;

    token.callback.notifyGetCallBarringSuccess(
      token.program,
      enabled,
      serviceClass
    );
    delete this._tokenUtMap[aId];
  },

  onCallWaitingQueried(aId, aCwInfo) {
    if (DEBUG) {
      this._debug(
        "onCallWaitingQueried. aId=" +
          aId +
          " , aCwInfo=" +
          JSON.stringify(aCwInfo)
      );
    }
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    let serviceClass =
      aCwInfo.status == Ci.nsIImsUt.STATUS_ENABLED
        ? Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
        : Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE;
    token.callback.notifyGetCallWaitingSuccess(serviceClass);
    delete this._tokenUtMap[aId];
  },

  onClirQueried(aId, aClirStatus) {
    if (DEBUG) {
      this._debug(
        "onClirQueried. aId=" +
          aId +
          " , aClirStatus=" +
          JSON.stringify(aClirStatus)
      );
    }
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    token.callback.notifyGetClirStatusSuccess(
      aClirStatus.clirOutgoingStatus,
      aClirStatus.clirInterrogationStatus
    );
    delete this._tokenUtMap[aId];
  },

  utConfigurationQueryFailed(aId, aError) {
    let token = this._tokenUtMap[aId];
    if (!token) {
      if (DEBUG) {
        this._debug("Ignore orphan ut aId: " + aId);
      }
      return;
    }

    if (DEBUG) {
      this._debug(
        "utConfigurationQueryFailed. aId=" +
          aId +
          ", aRequest= " +
          token.request +
          ", aError.code=" +
          aError.code +
          ", aError.extraCode=" +
          aError.extraCode +
          ", aError.extraMessage=" +
          aError.extraMessage
      );
    }
    // If ImsReasonInfo object does not have a String error code, use a
    // Gecko default error string.
    let errorMsg =
      aError.extraMessage ||
      RIL.RIL_IMSCALL_FAILCAUSE_TO_GECKO_CALL_ERROR[aError.code];

    this._dispatchNotifyError(token.callback, errorMsg);
  },

  // Helper functions
};

function MobileConnectionService() {
  this._providers = [];

  let numClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
  // FIXME
  debug("numClients: " + numClients);
  if (numClients < 1) {
    numClients = 1;
  }
  for (let i = 0; i < numClients; i++) {
    let radioInterface = lazy.gRadioInterfaceLayer.getRadioInterface(i);

    let imsRegHandler = null;
    if (lazy.gImsRegService) {
      imsRegHandler = lazy.gImsRegService.getHandlerByServiceId(i);
    }

    let provider = new MobileConnectionProvider(
      i,
      radioInterface,
      imsRegHandler
    );
    this._providers.push(provider);
  }

  // A preference to determine if device support switching primary SIM feature.
  this._supportPrimarySimSwitch = Services.prefs.getBoolPref(
    kPrefSupportPrimarySimSwitch
  );

  this._updateSupportedNetworkTypes();

  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID);
  Services.obs.addObserver(this, NS_DATA_CALL_ERROR_TOPIC_ID);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  if (this.numItems == 2 && this._supportPrimarySimSwitch) {
    Services.obs.addObserver(this, TOPIC_MOZSETTINGS_CHANGED);
  }

  debug("init complete");
}
MobileConnectionService.prototype = {
  classID: GONK_MOBILECONNECTIONSERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIGonkMobileConnectionService,
    Ci.nsIMobileConnectionService,
    Ci.nsIObserver,
  ]),

  // An array of MobileConnectionProvider instances.
  _providers: null,

  // A flag to indicate if device support primary SIM switch.
  _supportPrimarySimSwitch: false,

  _shutdown() {
    Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
    Services.obs.removeObserver(this, NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID);
    Services.obs.removeObserver(this, NS_DATA_CALL_ERROR_TOPIC_ID);
    Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    if (this.numItems == 2 && this._supportPrimarySimSwitch) {
      Services.obs.removeObserver(this, TOPIC_MOZSETTINGS_CHANGED);
    }
  },

  _updateDebugFlag() {
    try {
      DEBUG =
        RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  _updateSupportedNetworkTypes() {
    // Update "persist.ril.[clientId].network_types" if these keys do not exist.
    for (let i = 0; i < this.numItems; i++) {
      let key = "persist.ril." + i + ".network_types";
      let supportedNetworkTypes = libcutils.property_get(key, "");

      if (!supportedNetworkTypes) {
        key = "ro.moz.ril." + i + ".network_types";
        libcutils.property_set(
          "persist.ril." + i + ".network_types",
          // This should be customizable regarding default value.
          libcutils.property_get(
            key,
            i == 0
              ? RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT
              : RIL.GECKO_SUPPORTED_NETWORK_TYPES[0]
          )
        );
      }
    }
  },

  _swapSupportedNetworkTypes(aNewPrimarySlot) {
    let roKeyPrimary = "ro.moz.ril.0.network_types";
    let roKeySlave = "ro.moz.ril.1.network_types";

    let persistKeyPrimary = "persist.ril." + aNewPrimarySlot + ".network_types";
    let persistTypesPrimary = libcutils.property_get(persistKeyPrimary, "");

    let persistKeySlave =
      "persist.ril." + (aNewPrimarySlot ^ 1) + ".network_types";
    let persistTypesSlave = libcutils.property_get(persistKeySlave, "");

    if (!persistTypesPrimary || !persistTypesSlave) {
      if (DEBUG) {
        debug("Don't expect empty values for both slots");
      }
      return;
    }

    // Update persist value for according to aNewPrimarySlot, Default value
    // should be customizable.
    libcutils.property_set(
      persistKeyPrimary,
      libcutils.property_get(
        roKeyPrimary,
        RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT
      )
    );
    libcutils.property_set(
      persistKeySlave,
      libcutils.property_get(roKeySlave, RIL.GECKO_SUPPORTED_NETWORK_TYPES[0])
    );
  },

  /**
   * nsIMobileConnectionService interface.
   */
  get numItems() {
    return this._providers.length;
  },

  getItemByServiceId(aServiceId) {
    let provider = this._providers[aServiceId];
    if (!provider) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    return provider;
  },

  shutdownRadio(aCallback) {
    let shutdownPromses = [];
    try {
      shutdownPromses.push(
        this.getItemByServiceId(
          lazy.gDataCallManager.dataDefaultServiceId
        ).shutdownRequest()
      );
      for (let i = 0; i < this._providers.length; i++) {
        if (i == lazy.gDataCallManager.dataDefaultServiceId) {
          continue;
        }
        shutdownPromses.push(this.getItemByServiceId(i).shutdownRequest());
      }
      Promise.all(shutdownPromses).then(() => {
        if (aCallback) {
          aCallback.notifySuccess();
        }
      });
    } catch (aError) {
      if (aCallback) {
        aCallback.notifySuccess();
      }
    }
  },

  /**
   * nsIGonkMobileConnectionService interface.
   */
  notifyVoiceInfoChanged(aClientId, aVoiceInfo) {
    if (DEBUG) {
      debug(
        "notifyVoiceInfoChanged for " +
          aClientId +
          ": " +
          JSON.stringify(aVoiceInfo)
      );
    }

    this.getItemByServiceId(aClientId).updateVoiceInfo(aVoiceInfo);
  },

  notifyDataInfoChanged(aClientId, aDataInfo) {
    if (DEBUG) {
      debug(
        "notifyDataInfoChanged for " +
          aClientId +
          ": " +
          JSON.stringify(aDataInfo)
      );
    }

    this.getItemByServiceId(aClientId).updateDataInfo(aDataInfo);
  },

  notifyDataError(aClientId, aMessage) {
    if (DEBUG) {
      debug("notifyDataError for " + aClientId + ": " + aMessage);
    }

    this.getItemByServiceId(aClientId).deliverListenerEvent("notifyDataError", [
      aMessage,
    ]);
  },

  notifyEmergencyCallbackModeChanged(aClientId, aActive, aTimeoutMs) {
    if (DEBUG) {
      debug(
        "notifyEmergencyCbModeChanged for " +
          aClientId +
          ": " +
          JSON.stringify({ active: aActive, timeoutMs: aTimeoutMs })
      );
    }

    let provider = this.getItemByServiceId(aClientId);
    provider.isInEmergencyCbMode = aActive;
    provider.deliverListenerEvent("notifyEmergencyCbModeChanged", [
      aActive,
      aTimeoutMs,
    ]);
  },

  notifyOtaStatusChanged(aClientId, aStatus) {
    if (DEBUG) {
      debug("notifyOtaStatusChanged for " + aClientId + ": " + aStatus);
    }

    this.getItemByServiceId(
      aClientId
    ).deliverListenerEvent("notifyOtaStatusChanged", [aStatus]);
  },

  notifyRadioStateChanged(aClientId, aRadioState) {
    if (DEBUG) {
      debug("notifyRadioStateChanged for " + aClientId + ": " + aRadioState);
    }

    let message = {
      msgType: "HardwareRadioState",
      msgData: aRadioState,
    };
    this.getItemByServiceId(aClientId).updateRadioState(message);
  },

  notifyNetworkInfoChanged(aClientId, aNetworkInfo) {
    if (DEBUG) {
      debug(
        "notifyNetworkInfoChanged for " +
          aClientId +
          ": " +
          JSON.stringify(aNetworkInfo)
      );
    }

    let provider = this.getItemByServiceId(aClientId);
    let operatorMessage = aNetworkInfo[RIL.NETWORK_INFO_OPERATOR];
    let voiceMessage = aNetworkInfo[RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE];
    let dataMessage = aNetworkInfo[RIL.NETWORK_INFO_DATA_REGISTRATION_STATE];
    let selectionMessage =
      aNetworkInfo[RIL.NETWORK_INFO_NETWORK_SELECTION_MODE];

    // Batch the *InfoChanged messages together
    if (operatorMessage) {
      provider.updateOperatorInfo(operatorMessage, true);
    }

    if (voiceMessage) {
      provider.updateVoiceInfo(voiceMessage, true);
      lazy.gPhoneNumberUtils.updateCountryNameProperty(aClientId);
    }

    if (dataMessage) {
      provider.updateDataInfo(dataMessage, true);
    }

    if (
      provider.voice.roaming &&
      !this._isOperatorConsideredRoaming(aClientId, provider) &&
      (this._isSameNamedOperators(aClientId, provider) ||
        this._isOperatorConsideredNonRoaming(aClientId, provider))
    ) {
      provider.voice.roaming = false;
    }

    if (
      provider.data.roaming &&
      !this._isOperatorConsideredRoaming(aClientId, provider) &&
      (this._isSameNamedOperators(aClientId, provider) ||
        this._isOperatorConsideredNonRoaming(aClientId, provider))
    ) {
      provider.data.roaming = false;
    }

    if (selectionMessage) {
      this.notifyNetworkSelectModeChanged(aClientId, selectionMessage.mode);
    }

    if (voiceMessage || operatorMessage) {
      provider.deliverListenerEvent("notifyVoiceChanged");
    }

    if (dataMessage || operatorMessage) {
      provider.deliverListenerEvent("notifyDataChanged");
    }
  },

  _isOperatorConsideredRoaming(aClientId, provider) {
    let operatorNumeric =
      provider._operatorInfo.mcc + provider._operatorInfo.mnc;
    let numericArray = [];

    if (lazy.gCustomizationInfo) {
      numericArray = lazy.gCustomizationInfo.getCustomizedValue(
        aClientId,
        "roamingOperatorStringArray",
        []
      );
    }

    if (numericArray.length == 0 || operatorNumeric == 0) {
      return false;
    }

    // This will check operatorNumeric from the beginning to see if it contains the numeric.
    for (let numeric of numericArray) {
      if (operatorNumeric.includes(numeric)) {
        return true;
      }
    }
    return false;
  },

  _isSameNamedOperators(aClientId, provider) {
    let icc = lazy.gIccService.getIccByServiceId(aClientId);
    let iccInfo = icc ? icc.iccInfo : null;

    if (!iccInfo) {
      return false;
    }

    let equalsMcc = iccInfo.mcc == provider._operatorInfo.mcc;
    if (!equalsMcc) {
      return false;
    }
    let spn = iccInfo && iccInfo.spn;
    let onsl = provider._operatorInfo.longName;
    let onss = provider._operatorInfo.shortName;

    let equalsOnsl = onsl != null && spn == onsl;
    let equalsOnss = onss != null && spn == onss;

    return equalsMcc && (equalsOnsl || equalsOnss);
  },

  _isOperatorConsideredNonRoaming(aClientId, provider) {
    let operatorNumeric =
      provider._operatorInfo.mcc + provider._operatorInfo.mnc;
    let numericArray = [];

    if (lazy.gCustomizationInfo) {
      numericArray = lazy.gCustomizationInfo.getCustomizedValue(
        aClientId,
        "nonRoamingOperatorStringArray",
        []
      );
    }

    if (numericArray.length == 0 || operatorNumeric == 0) {
      return false;
    }

    // This will check operatorNumeric from the beginning to see if it contains the numeric.
    for (let numeric of numericArray) {
      if (operatorNumeric.includes(numeric)) {
        return true;
      }
    }
    return false;
  },

  notifySignalStrengthChanged(aClientId, aSignal) {
    if (DEBUG) {
      debug(
        "notifySignalStrengthChanged for " +
          aClientId +
          ": " +
          JSON.stringify(aSignal)
      );
    }

    this.getItemByServiceId(aClientId).updateSignalInfo(aSignal);
  },

  notifyOperatorChanged(aClientId, aOperator) {
    if (DEBUG) {
      debug(
        "notifyOperatorChanged for " +
          aClientId +
          ": " +
          JSON.stringify(aOperator)
      );
    }

    this.getItemByServiceId(aClientId).updateOperatorInfo(aOperator);
  },

  notifyNetworkSelectModeChanged(aClientId, aMode) {
    if (DEBUG) {
      debug("notifyNetworkSelectModeChanged for " + aClientId + ": " + aMode);
    }

    let provider = this.getItemByServiceId(aClientId);
    if (provider.networkSelectionMode === aMode) {
      return;
    }

    provider.networkSelectionMode = aMode;
    provider.deliverListenerEvent("notifyNetworkSelectionModeChanged");
  },

  notifySpnAvailable(aClientId) {
    if (DEBUG) {
      debug("notifySpnAvailable for " + aClientId);
    }

    let provider = this.getItemByServiceId(aClientId);

    // Update voice roaming state
    provider.updateVoiceInfo({});

    // Update data roaming state
    provider.updateDataInfo({});
  },

  notifyLastHomeNetworkChanged(aClientId, aNetwork) {
    if (DEBUG) {
      debug("notifyLastHomeNetworkChanged for " + aClientId + ": " + aNetwork);
    }

    let provider = this.getItemByServiceId(aClientId);
    if (provider.lastKnownHomeNetwork === aNetwork) {
      return;
    }

    provider.lastKnownHomeNetwork = aNetwork;
    provider.deliverListenerEvent("notifyLastKnownHomeNetworkChanged");
  },

  notifyCFStateChanged(
    aClientId,
    aAction,
    aReason,
    aNumber,
    aTimeSeconds,
    aServiceClass
  ) {
    if (DEBUG) {
      debug("notifyCFStateChanged for " + aClientId);
    }

    let provider = this.getItemByServiceId(aClientId);
    provider.notifyCFStateChanged(
      aAction,
      aReason,
      aNumber,
      aTimeSeconds,
      aServiceClass
    );
  },

  notifyCdmaInfoRecDisplay(aClientId, aDisplay) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecDisplay(
      aClientId,
      aDisplay
    );
  },

  notifyCdmaInfoRecCalledPartyNumber(
    aClientId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecCalledPartyNumber(
      aClientId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecCallingPartyNumber(
    aClientId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecCallingPartyNumber(
      aClientId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecConnectedPartyNumber(
    aClientId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi
  ) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecConnectedPartyNumber(
      aClientId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi
    );
  },

  notifyCdmaInfoRecSignal(aClientId, aType, aAlertPitch, aSignal) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecSignal(
      aClientId,
      aType,
      aAlertPitch,
      aSignal
    );
  },

  notifyCdmaInfoRecRedirectingNumber(
    aClientId,
    aType,
    aPlan,
    aNumber,
    aPi,
    aSi,
    aReason
  ) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecRedirectingNumber(
      aClientId,
      aType,
      aPlan,
      aNumber,
      aPi,
      aSi,
      aReason
    );
  },

  notifyCdmaInfoRecLineControl(
    aClientId,
    aPolarityIncluded,
    aToggle,
    aReverse,
    aPowerDenial
  ) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecLineControl(
      aClientId,
      aPolarityIncluded,
      aToggle,
      aReverse,
      aPowerDenial
    );
  },

  notifyCdmaInfoRecClir(aClientId, aCause) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecClir(aClientId, aCause);
  },

  notifyCdmaInfoRecAudioControl(aClientId, aUpLink, aDownLink) {
    lazy.gMobileConnectionMessenger.notifyCdmaInfoRecAudioControl(
      aClientId,
      aUpLink,
      aDownLink
    );
  },

  notifyDeviceIdentitiesChanged(aClientId, aImei, aImeisv, aEsn, aMeid) {
    this.getItemByServiceId(aClientId).updateDeviceIdentities(
      aImei,
      aImeisv,
      aEsn,
      aMeid
    );
  },

  notifyModemRestart(aClientId, aReason) {
    if (DEBUG) {
      debug(
        "notifyModemRestart: clientId=" + aClientId + ", reason=" + aReason
      );
    }

    let provider = this.getItemByServiceId(aClientId);
    if (provider._expectedRadioState === RIL.GECKO_RADIOSTATE_ENABLED) {
      provider._needRestorePreferredNetworkType = true;
    }

    provider.deliverListenerEvent("notifyModemRestart", [aReason]);
  },

  /**
   * nsIObserver interface.
   */
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID:
        for (let i = 0; i < this.numItems; i++) {
          let provider = this._providers[i];
          // Update connected flag only.
          provider.updateDataInfo({});
        }
        break;
      case NS_DATA_CALL_ERROR_TOPIC_ID:
        try {
          if (aSubject instanceof Ci.nsIRilNetworkInfo) {
            let rilInfo = aSubject.QueryInterface(Ci.nsIRilNetworkInfo);
            this.notifyDataError(rilInfo.serviceId, aData);
          }
        } catch (e) {}
        break;
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          this._updateDebugFlag();
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this._shutdown();
        break;
      case TOPIC_MOZSETTINGS_CHANGED:
        if ("wrappedJSObject" in aSubject) {
          aSubject = aSubject.wrappedJSObject;
        }
        this.handle(aSubject.key, aSubject.value);
        break;
    }
  },

  handle(aName, aResult) {
    switch (aName) {
      case kPrefRilDataServiceId:
        aResult = aResult || 0;
        if (DEBUG) {
          debug("'ril.data.defaultServiceId' is now " + aResult);
        }
        this._swapSupportedNetworkTypes(aResult);
        break;
    }
  },
};

var EXPORTED_SYMBOLS = ["MobileConnectionService"];
