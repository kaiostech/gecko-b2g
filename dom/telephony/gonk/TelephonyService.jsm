/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-unused-vars */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

const RIL = ChromeUtils.import("resource://gre/modules/ril_consts.js");

const RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    maxLogLevelPref: "dom.telephony.loglevel",
    prefix: "Telephony",
  });
});

const GONK_TELEPHONYSERVICE_CID = Components.ID(
  "{67d26434-d063-4d28-9f48-5b3189788155}"
);
const TELEPHONYCALLINFO_CID = Components.ID(
  "{d9e8b358-a02c-4cf3-9fc7-816c2e8d46e4}"
);

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefDefaultServiceId = "dom.telephony.defaultServiceId";
const kPrefTwoDigitShortCodes = "ussd.exceptioncode";
const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefAlwaysTryImsForEcc = "ril.alwaysTryImsForEcc";
const kPrefImsEnabled = "b2g.ims.enabled";

const nsITelephonyAudioService = Ci.nsITelephonyAudioService;
const nsITelephonyService = Ci.nsITelephonyService;
const nsITelephonyCallInfo = Ci.nsITelephonyCallInfo;
const nsIMobileConnection = Ci.nsIMobileConnection;

const CALL_WAKELOCK_TIMEOUT = 5000;

// In CDMA, RIL only hold one call index. We need to fake a second call index
// in TelephonyService for 3-way calling.
const CDMA_FIRST_CALL_INDEX = 1;
const CDMA_SECOND_CALL_INDEX = 2;

const DIAL_ERROR_INVALID_STATE_ERROR = "InvalidStateError";
const DIAL_ERROR_OTHER_CONNECTION_IN_USE = "OtherConnectionInUse";
const DIAL_ERROR_BAD_NUMBER = RIL.GECKO_CALL_ERROR_BAD_NUMBER;
const DIAL_ERROR_RADIO_NOT_AVAILABLE = RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE;

const TONES_GAP_DURATION = 70;

const EMERGENCY_CALL_DEFAULT_CLIENT_ID = 0;

// Consts for MMI.
// MMI procedure as defined in TS.22.030 6.5.2
const MMI_PROCEDURE_ACTIVATION = "*";
const MMI_PROCEDURE_DEACTIVATION = "#";
const MMI_PROCEDURE_INTERROGATION = "*#";
const MMI_PROCEDURE_REGISTRATION = "**";
const MMI_PROCEDURE_ERASURE = "##";

// MMI call forwarding service codes as defined in TS.22.030 Annex B
const MMI_SC_CFU = "21";
const MMI_SC_CF_BUSY = "67";
const MMI_SC_CF_NO_REPLY = "61";
const MMI_SC_CF_NOT_REACHABLE = "62";
const MMI_SC_CF_ALL = "002";
const MMI_SC_CF_ALL_CONDITIONAL = "004";

// MMI service codes for PIN/PIN2/PUK/PUK2 management as defined in TS.22.030
// sec 6.6
const MMI_SC_PIN = "04";
const MMI_SC_PIN2 = "042";
const MMI_SC_PUK = "05";
const MMI_SC_PUK2 = "052";

// MMI service code for IMEI presentation as defined in TS.22.030 sec 6.7
const MMI_SC_IMEI = "06";

// MMI call waiting service code
const MMI_SC_CALL_WAITING = "43";

// MMI service code for registration new password as defined in TS 22.030 6.5.4
const MMI_SC_CHANGE_PASSWORD = "03";
const MMI_ZZ_BARRING_SERVICE = "330";

// MMI call barring service codes
const MMI_SC_BAOC = "33";
const MMI_SC_BAOIC = "331";
const MMI_SC_BAOICxH = "332";
const MMI_SC_BAIC = "35";
const MMI_SC_BAICr = "351";
const MMI_SC_BA_ALL = "330";
const MMI_SC_BA_MO = "333";
const MMI_SC_BA_MT = "353";

// MMI called line presentation service codes
const MMI_SC_CLIP = "30";
const MMI_SC_CLIR = "31";

// MMI service code key strings.
const MMI_KS_SC_CALL_BARRING = "scCallBarring";
const MMI_KS_SC_CALL_FORWARDING = "scCallForwarding";
const MMI_KS_SC_CLIP = "scClip";
const MMI_KS_SC_CLIR = "scClir";
// const MMI_KS_SC_PWD = "scPwd";
const MMI_KS_SC_CALL_WAITING = "scCallWaiting";
const MMI_KS_SC_PIN = "scPin";
const MMI_KS_SC_PIN2 = "scPin2";
const MMI_KS_SC_PUK = "scPuk";
const MMI_KS_SC_PUK2 = "scPuk2";
const MMI_KS_SC_CHANGE_PASSWORD = "scChangePassword";
const MMI_KS_SC_IMEI = "scImei";
const MMI_KS_SC_USSD = "scUssd";
const MMI_KS_SC_CALL = "scCall";

// MMI error messages key strings.
const MMI_ERROR_KS_ERROR = "emMmiError";
const MMI_ERROR_KS_NOT_SUPPORTED = "emMmiErrorNotSupported";
const MMI_ERROR_KS_INVALID_ACTION = "emMmiErrorInvalidAction";
const MMI_ERROR_KS_MISMATCH_PIN = "emMmiErrorMismatchPin";
const MMI_ERROR_KS_MISMATCH_PASSWORD = "emMmiErrorMismatchPassword";
const MMI_ERROR_KS_BAD_PIN = "emMmiErrorBadPin";
const MMI_ERROR_KS_BAD_PUK = "emMmiErrorBadPuk";
const MMI_ERROR_KS_INVALID_PIN = "emMmiErrorInvalidPin";
const MMI_ERROR_KS_INVALID_PASSWORD = "emMmiErrorInvalidPassword";
const MMI_ERROR_KS_NEEDS_PUK = "emMmiErrorNeedsPuk";
const MMI_ERROR_KS_SIM_BLOCKED = "emMmiErrorSimBlocked";

// MMI status message.
const MMI_SM_KS_PASSWORD_CHANGED = "smPasswordChanged";
const MMI_SM_KS_PIN_CHANGED = "smPinChanged";
const MMI_SM_KS_PIN2_CHANGED = "smPin2Changed";
const MMI_SM_KS_PIN_UNBLOCKED = "smPinUnblocked";
const MMI_SM_KS_PIN2_UNBLOCKED = "smPin2Unblocked";
const MMI_SM_KS_SERVICE_ENABLED = "smServiceEnabled";
const MMI_SM_KS_SERVICE_ENABLED_FOR = "smServiceEnabledFor";
const MMI_SM_KS_SERVICE_DISABLED = "smServiceDisabled";
const MMI_SM_KS_SERVICE_REGISTERED = "smServiceRegistered";
const MMI_SM_KS_SERVICE_ERASED = "smServiceErased";
const MMI_SM_KS_SERVICE_INTERROGATED = "smServiceInterrogated";
const MMI_SM_KS_SERVICE_NOT_PROVISIONED = "smServiceNotProvisioned";
const MMI_SM_KS_CLIR_PERMANENT = "smClirPermanent";
const MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_ON = "smClirDefaultOnNextCallOn";
const MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_OFF = "smClirDefaultOnNextCallOff";
const MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_ON = "smClirDefaultOffNextCallOn";
const MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_OFF = "smClirDefaultOffNextCallOff";
const MMI_SM_KS_CALL_CONTROL = "smCallControl";

// MMI Service class
const MMI_KS_SERVICE_CLASS_VOICE = "serviceClassVoice";
const MMI_KS_SERVICE_CLASS_DATA = "serviceClassData";
const MMI_KS_SERVICE_CLASS_FAX = "serviceClassFax";
const MMI_KS_SERVICE_CLASS_SMS = "serviceClassSms";
const MMI_KS_SERVICE_CLASS_DATA_SYNC = "serviceClassDataSync";
const MMI_KS_SERVICE_CLASS_DATA_ASYNC = "serviceClassDataAsync";
const MMI_KS_SERVICE_CLASS_PACKET = "serviceClassPacket";
const MMI_KS_SERVICE_CLASS_PAD = "serviceClassPad";

// States of USSD Session : DONE -> ONGOING [-> CANCELLING] -> DONE
const USSD_SESSION_DONE = "DONE";
const USSD_SESSION_ONGOING = "ONGOING";
const USSD_SESSION_CANCELLING = "CANCELLING";

const MMI_PROC_TO_CF_ACTION = {};
MMI_PROC_TO_CF_ACTION[MMI_PROCEDURE_ACTIVATION] =
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ENABLE;
MMI_PROC_TO_CF_ACTION[MMI_PROCEDURE_DEACTIVATION] =
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_DISABLE;
MMI_PROC_TO_CF_ACTION[MMI_PROCEDURE_INTERROGATION] =
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_QUERY_STATUS;
MMI_PROC_TO_CF_ACTION[MMI_PROCEDURE_REGISTRATION] =
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_REGISTRATION;
MMI_PROC_TO_CF_ACTION[MMI_PROCEDURE_ERASURE] =
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ERASURE;

const MMI_SC_TO_CF_REASON = {};
MMI_SC_TO_CF_REASON[MMI_SC_CFU] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL;
MMI_SC_TO_CF_REASON[MMI_SC_CF_BUSY] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY;
MMI_SC_TO_CF_REASON[MMI_SC_CF_NO_REPLY] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_NO_REPLY;
MMI_SC_TO_CF_REASON[MMI_SC_CF_NOT_REACHABLE] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_NOT_REACHABLE;
MMI_SC_TO_CF_REASON[MMI_SC_CF_ALL] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING;
MMI_SC_TO_CF_REASON[MMI_SC_CF_ALL_CONDITIONAL] =
  Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING;

const MMI_SC_TO_LOCK_TYPE = {};
MMI_SC_TO_LOCK_TYPE[MMI_SC_PIN] = Ci.nsIIcc.CARD_LOCK_TYPE_PIN;
MMI_SC_TO_LOCK_TYPE[MMI_SC_PIN2] = Ci.nsIIcc.CARD_LOCK_TYPE_PIN2;
MMI_SC_TO_LOCK_TYPE[MMI_SC_PUK] = Ci.nsIIcc.CARD_LOCK_TYPE_PUK;
MMI_SC_TO_LOCK_TYPE[MMI_SC_PUK2] = Ci.nsIIcc.CARD_LOCK_TYPE_PUK2;

const MMI_PROC_TO_CLIR_ACTION = {};
MMI_PROC_TO_CLIR_ACTION[MMI_PROCEDURE_ACTIVATION] =
  Ci.nsIMobileConnection.CLIR_INVOCATION;
MMI_PROC_TO_CLIR_ACTION[MMI_PROCEDURE_DEACTIVATION] =
  Ci.nsIMobileConnection.CLIR_SUPPRESSION;

const MMI_SC_TO_CB_PROGRAM = {};
MMI_SC_TO_CB_PROGRAM[MMI_SC_BAOC] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BAOIC] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BAOICxH] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BAIC] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_INCOMING;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BAICr] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_INCOMING_ROAMING;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BA_ALL] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_SERVICE;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BA_MO] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_SERVICE;
MMI_SC_TO_CB_PROGRAM[MMI_SC_BA_MT] =
  Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_INCOMING_SERVICE;

const CF_ACTION_TO_STATUS_MESSAGE = {};
CF_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ENABLE
] = MMI_SM_KS_SERVICE_ENABLED;
CF_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_DISABLE
] = MMI_SM_KS_SERVICE_DISABLED;
CF_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_REGISTRATION
] = MMI_SM_KS_SERVICE_REGISTERED;
CF_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ERASURE
] = MMI_SM_KS_SERVICE_ERASED;

const LOCK_TYPE_TO_STATUS_MESSAGE = {};
LOCK_TYPE_TO_STATUS_MESSAGE[
  Ci.nsIIcc.CARD_LOCK_TYPE_PIN
] = MMI_SM_KS_PIN_CHANGED;
LOCK_TYPE_TO_STATUS_MESSAGE[
  Ci.nsIIcc.CARD_LOCK_TYPE_PIN2
] = MMI_SM_KS_PIN2_CHANGED;
LOCK_TYPE_TO_STATUS_MESSAGE[
  Ci.nsIIcc.CARD_LOCK_TYPE_PUK
] = MMI_SM_KS_PIN_UNBLOCKED;
LOCK_TYPE_TO_STATUS_MESSAGE[
  Ci.nsIIcc.CARD_LOCK_TYPE_PUK2
] = MMI_SM_KS_PIN2_UNBLOCKED;

const CLIR_ACTION_TO_STATUS_MESSAGE = {};
CLIR_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CLIR_INVOCATION
] = MMI_SM_KS_SERVICE_ENABLED;
CLIR_ACTION_TO_STATUS_MESSAGE[
  Ci.nsIMobileConnection.CLIR_SUPPRESSION
] = MMI_SM_KS_SERVICE_DISABLED;

const MMI_KS_SERVICE_CLASS_MAPPING = {};
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
] = MMI_KS_SERVICE_CLASS_VOICE;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA
] = MMI_KS_SERVICE_CLASS_DATA;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_FAX
] = MMI_KS_SERVICE_CLASS_FAX;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_SMS
] = MMI_KS_SERVICE_CLASS_SMS;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_SYNC
] = MMI_KS_SERVICE_CLASS_DATA_SYNC;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_ASYNC
] = MMI_KS_SERVICE_CLASS_DATA_ASYNC;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_PACKET
] = MMI_KS_SERVICE_CLASS_PACKET;
MMI_KS_SERVICE_CLASS_MAPPING[
  Ci.nsIMobileConnection.ICC_SERVICE_CLASS_PAD
] = MMI_KS_SERVICE_CLASS_PAD;

var ALWAYS_TRY_IMS_FOR_EMERGENCY = false;
var IMS_ENABLED = false;

var DEBUG;
function debug(s) {
  dump("TelephonyService: " + s + "\n");
}

XPCOMUtils.defineLazyGetter(lazy, "gRadioInterfaceLayer", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}
  return ril;
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gTelephonyMessenger",
  "@mozilla.org/ril/system-messenger-helper;1",
  "nsITelephonyMessenger"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gAudioService",
  "@mozilla.org/telephony/audio-service;1",
  "nsITelephonyAudioService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gGonkMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIGonkMobileConnectionService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gImsRegService",
  "@mozilla.org/mobileconnection/imsregservice;1",
  "nsIImsRegService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gImsPhoneService",
  "@b2g/telephony/ims/imsphoneservice;1",
  "nsIImsPhoneService"
);

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gPhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm",
  "PhoneNumberUtils"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  DialNumberUtils: "resource://gre/modules/DialNumberUtils.jsm",
});

function TelephonyCallInfo(aCall) {
  this.clientId = aCall.clientId;
  this.callIndex = aCall.callIndex;
  this.callState = aCall.state;
  this.voiceQuality =
    aCall.voiceQuality || nsITelephonyService.CALL_VOICE_QUALITY_NORMAL;
  this.disconnectedReason = aCall.disconnectedReason || "";

  this.number = aCall.number;
  this.numberPresentation = aCall.numberPresentation;
  this.name = aCall.name;
  this.namePresentation = aCall.namePresentation;

  this.isOutgoing = aCall.isOutgoing;
  this.isEmergency = aCall.isEmergency;
  this.isConference = aCall.isConference;
  this.isSwitchable = aCall.isSwitchable;
  this.isMergeable = aCall.isMergeable;
  this.isConferenceParent = aCall.isConferenceParent || false;
  this.isMarkable = aCall.isMarkable || false;

  this.isVt = aCall.isVt || false;
  this.capabilities =
    aCall.capabilities || Ci.nsITelephonyCallInfo.CAPABILITY_SUPPORTS_NONE;
  this.videoCallState =
    aCall.videoCallState || Ci.nsITelephonyCallInfo.STATE_AUDIO_ONLY;
  this.radioTech = aCall.radioTech || Ci.nsITelephonyCallInfo.RADIO_TECH_CS;
  this.rttMode = aCall.rttMode || Ci.nsITelephonyService.RTT_MODE_OFF;
  this.vowifiCallQuality =
    aCall.vowifiCallQuality || nsITelephonyCallInfo.VOWIFI_QUALITY_NONE;
  this.verStatus = aCall.verStatus || nsITelephonyCallInfo.VER_NONE;
}
TelephonyCallInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyCallInfo]),
  classID: TELEPHONYCALLINFO_CID,

  // nsITelephonyCallInfo

  clientId: 0,
  callIndex: 0,
  callState: nsITelephonyService.CALL_STATE_UNKNOWN,
  voiceQuality: nsITelephonyService.CALL_VOICE_QUALITY_NORMAL,
  disconnectedReason: "",

  number: "",
  numberPresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,
  name: "",
  namePresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,

  isOutgoing: true,
  isEmergency: false,
  isConference: false,
  isSwitchable: true,
  isMergeable: true,
  isVt: false,
  capabilities: Ci.nsITelephonyCallInfo.CAPABILITY_SUPPORTS_NONE,
  videoCallState: Ci.nsITelephonyCallInfo.STATE_AUDIO_ONLY,
  radioTech: Ci.nsITelephonyCallInfo.RADIO_TECH_CS,
  rttMode: Ci.nsITelephonyService.RTT_MODE_OFF,
  vowifiCallQuality: nsITelephonyCallInfo.VOWIFI_QUALITY_NONE,
};

function Call(aClientId, aCallIndex) {
  this.clientId = aClientId;
  this.callIndex = aCallIndex;
}
Call.prototype = {
  clientId: 0,
  callIndex: 0,
  state: nsITelephonyService.CALL_STATE_UNKNOWN,
  number: "",
  numberPresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,
  name: "",
  namePresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,
  isOutgoing: true,
  isEmergency: false,
  isConference: false,
  isSwitchable: true,
  isMergeable: true,
  started: null,
  isConferenceParent: false,
  isMarkable: false,

  isVt: false,
  voiceQuality: nsITelephonyService.CALL_VOICE_QUALITY_NORMAL,
  capabilities: Ci.nsITelephonyCallInfo.CAPABILITY_SUPPORTS_NONE,
  videoCallState: Ci.nsITelephonyCallInfo.STATE_AUDIO_ONLY,
  radioTech: Ci.nsITelephonyCallInfo.RADIO_TECH_CS,
  vowifiCallQuality: nsITelephonyCallInfo.VOWIFI_QUALITY_NONE,
  verStatus: nsITelephonyCallInfo.VER_NONE,

  videoCallProvider: null,
  isRtt: false,

  getVideoCallProvider() {
    if (!this.videoCallProvider) {
      this.videoCallProvider = new VideoCallProvider(
        this.clientId,
        this.callIndex
      );
    }

    return this.videoCallProvider;
  },

  isImsCall() {
    return (
      this.radioTech === Ci.nsITelephonyCallInfo.RADIO_TECH_PS ||
      this.radioTech === Ci.nsITelephonyCallInfo.RADIO_TECH_WIFI
    );
  },
};

function MobileConnectionListener() {}
MobileConnectionListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileConnectionListener]),

  // nsIMobileConnectionListener
  notifyVoiceChanged() {},
  notifyDataChanged() {},
  notifyDataError(_message) {},
  notifyCFStateChanged(
    _action,
    _reason,
    _number,
    _timeSeconds,
    _serviceClass
  ) {},
  notifyEmergencyCbModeChanged(_active, _timeoutMs) {},
  notifyOtaStatusChanged(_status) {},
  notifyRadioStateChanged() {},
  notifyClirModeChanged(_mode) {},
  notifyLastKnownNetworkChanged() {},
  notifyLastKnownHomeNetworkChanged() {},
  notifyNetworkSelectionModeChanged() {},
  notifyDeviceIdentitiesChanged() {},
  notifySignalStrengthChanged() {},
  notifyModemRestart(_reason) {},
};

function VideoCallProvider(aClientId, aCallIndex) {
  this._clientId = aClientId;
  this._callIndex = aCallIndex;
  this._listeners = [];
}
VideoCallProvider.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIVideocallProvider]),
  _clientId: null,
  _callIndex: null,
  _listeners: null,

  // nsIVideoCallProvider
  /*eslint no-unused-vars: ["error", { "varsIgnorePattern": "aCameraId" }]*/
  setCamera(aCameraId) {},
  /*eslint no-unused-vars: ["error", { "varsIgnorePattern": "aProducer|aWidth|aHeight" }]*/
  setPreviewSurface(aProducer, aWidth, aHeight) {},
  setDisplaySurface(aProducer, aWidth, aHeight) {},
  setDeviceOrientation(aRotation) {},
  setZoom(aValue) {},
  sendSessionModifyRequest(aFromProfile, aToProfile) {},
  sendSessionModifyResponse(aResponseProfile) {},
  requestCameraCapabilities() {},

  registerCallback(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterCallback(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },
};

function TelephonyService() {
  this._numClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
  this._listeners = [];

  this._isDialing = false;
  this._cachedDialRequest = null;
  this._currentCalls = {};
  this._audioStates = [];
  this._ussdSessions = [];
  this._twoDigitShortCodes = [];
  this._mobileConnListeners = [];

  this._initConstByPrefs();
  this._initImsPhones();

  this._cdmaCallWaitingNumber = null;

  this._headsetState = lazy.gAudioService.headsetState;
  lazy.gAudioService.registerListener(this);
  this._applyTtyMode();

  // this.defaultServiceId = this._getDefaultServiceId();
  this.defaultServiceId = 0;

  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  Services.prefs.addObserver(kPrefDefaultServiceId, this);
  Services.prefs.addObserver(kPrefTwoDigitShortCodes, this);

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  for (let i = 0; i < this._numClients; ++i) {
    this._audioStates[i] = nsITelephonyAudioService.PHONE_STATE_NORMAL;
    this._ussdSessions[i] = USSD_SESSION_DONE;
    this._currentCalls[i] = {};
    this._enumerateCallsForClient(i);
  }

  this._updateTwoDigitShortCodes();

  // access mobile connection in next tick.
  Services.tm.currentThread.dispatch(() => {
    for (let i = 0; i < this._numClients; ++i) {
      let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(i);
      let listener = new MobileConnectionListener();
      listener.notifyRadioStateChanged = () => {
        if (
          connection.radioState ===
            Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN ||
          connection.radioState ===
            Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED
        ) {
          this._sendToRilWorker(i, "getCurrentCalls", null, null);
        }
      };
      connection.registerListener(listener);
      this._mobileConnListeners[i] = listener;
    }
  }, Ci.nsIThread.DISPATCH_NORMAL);
}
TelephonyService.prototype = {
  classID: GONK_TELEPHONYSERVICE_CID,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsITelephonyService,
    Ci.nsIGonkTelephonyService,
    Ci.nsITelephonyAudioListener,
    Ci.nsIObserver,
  ]),

  // The following attributes/functions are used for acquiring/releasing the
  // CPU wake lock when the RIL handles the incoming call. Note that we need
  // a timer to bound the lock's life cycle to avoid exhausting the battery.
  _callRingWakeLock: null,
  _callRingWakeLockTimer: null,

  /**
   * USSD session flags.
   * Only one USSD session may exist at a time, and the session is assumed
   * to exist until:
   *    a) There's a call to cancelUSSD()
   *    b) Receiving a session end unsolicited event.
   */
  _ussdSessions: null,
  _imsPhones: null,

  _twoDigitShortCodes: null,

  _mobileConnListeners: null,

  _acquireCallRingWakeLock() {
    if (!this._callRingWakeLock) {
      if (DEBUG) {
        debug("Acquiring a CPU wake lock for handling incoming call.");
      }
      this._callRingWakeLock = lazy.gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._callRingWakeLockTimer) {
      if (DEBUG) {
        debug("Creating a timer for releasing the CPU wake lock.");
      }
      this._callRingWakeLockTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }
    if (DEBUG) {
      debug("Setting the timer for releasing the CPU wake lock.");
    }
    this._callRingWakeLockTimer.initWithCallback(
      this._releaseCallRingWakeLock.bind(this),
      CALL_WAKELOCK_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _releaseCallRingWakeLock() {
    if (DEBUG) {
      debug("Releasing the CPU wake lock for handling incoming call.");
    }
    if (this._callRingWakeLockTimer) {
      this._callRingWakeLockTimer.cancel();
    }
    if (this._callRingWakeLock) {
      this._callRingWakeLock.unlock();
      this._callRingWakeLock = null;
    }
  },

  _getClient(aClientId) {
    return lazy.gRadioInterfaceLayer.getRadioInterface(aClientId);
  },

  _sendToRilWorker(aClientId, aType, aMessage, aCallback) {
    this._getClient(aClientId).sendWorkerMessage(aType, aMessage, aCallback);
  },

  _isGsmTechGroup(aType) {
    switch (aType) {
      case null: // Handle unknown as gsm.
      case "gsm":
      case "gprs":
      case "edge":
      case "umts":
      case "hsdpa":
      case "hsupa":
      case "hspa":
      case "hspa+":
      case "lte":
      case "iwlan":
        return true;
      default:
        return false;
    }
  },

  _isCdmaClient(aClientId) {
    let type = lazy.gGonkMobileConnectionService.getItemByServiceId(aClientId)
      .voice.type;
    return !this._isGsmTechGroup(type);
  },

  _isEmergencyOnly(aClientId) {
    return lazy.gGonkMobileConnectionService.getItemByServiceId(aClientId).voice
      .emergencyCallsOnly;
  },

  _isRadioOn(aClientId) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    return (
      connection.radioState === nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED
    );
  },

  _isVoWifi(aClientId) {
    let connInfo = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    ).data;
    return (
      RIL.GECKO_RADIO_TECH.indexOf(connInfo.type) ===
      RIL.NETWORK_CREG_TECH_IWLAN
    );
  },

  // An array of nsITelephonyListener instances.
  _listeners: null,
  _notifyAllListeners(aMethodName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (!this._listeners.includes(listener)) {
        // Listener has been unregistered in previous run.
        continue;
      }

      let handler = listener[aMethodName];
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        debug("listener for " + aMethodName + " threw an exception: " + e);
      }
    }
  },

  _computeAudioStateForClient(aClientId) {
    let indexes = Object.keys(this._currentCalls[aClientId]);
    if (!indexes.length) {
      return nsITelephonyAudioService.PHONE_STATE_NORMAL;
    }

    let firstCall = this._currentCalls[aClientId][indexes[0]];
    if (
      indexes.length === 1 &&
      firstCall.state === nsITelephonyService.CALL_STATE_INCOMING
    ) {
      return nsITelephonyAudioService.PHONE_STATE_RINGTONE;
    }

    return nsITelephonyAudioService.PHONE_STATE_IN_CALL;
  },

  _updateAudioState(aClientId) {
    this._audioStates[aClientId] = this._computeAudioStateForClient(aClientId);

    if (
      this._audioStates.some(
        state => state === nsITelephonyAudioService.PHONE_STATE_IN_CALL
      )
    ) {
      lazy.gAudioService.setPhoneState(
        nsITelephonyAudioService.PHONE_STATE_IN_CALL
      );
    } else if (
      this._audioStates.some(
        state => state === nsITelephonyAudioService.PHONE_STATE_RINGTONE
      )
    ) {
      lazy.gAudioService.setPhoneState(
        nsITelephonyAudioService.PHONE_STATE_RINGTONE
      );
    } else {
      lazy.gAudioService.setPhoneState(
        nsITelephonyAudioService.PHONE_STATE_NORMAL
      );
    }
  },

  _formatInternationalNumber(aNumber, aToa) {
    if (aNumber && aToa == RIL.TOA_INTERNATIONAL && aNumber[0] != "+") {
      return "+" + aNumber;
    }

    return aNumber;
  },

  _convertRILCallState(aState) {
    switch (aState) {
      case RIL.CALL_STATE_UNKNOWN:
        return nsITelephonyService.CALL_STATE_UNKNOWN;
      case RIL.CALL_STATE_ACTIVE:
        return nsITelephonyService.CALL_STATE_CONNECTED;
      case RIL.CALL_STATE_HOLDING:
        return nsITelephonyService.CALL_STATE_HELD;
      case RIL.CALL_STATE_DIALING:
        return nsITelephonyService.CALL_STATE_DIALING;
      case RIL.CALL_STATE_ALERTING:
        return nsITelephonyService.CALL_STATE_ALERTING;
      case RIL.CALL_STATE_INCOMING:
      case RIL.CALL_STATE_WAITING:
        return nsITelephonyService.CALL_STATE_INCOMING;
      default:
        throw new Error("Unknown rilCallState: " + aState);
    }
  },

  _initConstByPrefs() {
    this._updateDebugFlag();
    this._updateAlwaysTryImsForEcc();
    this._updateImsEnable();
  },

  _updateDebugFlag() {
    try {
      DEBUG =
        RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  _updateAlwaysTryImsForEcc() {
    try {
      ALWAYS_TRY_IMS_FOR_EMERGENCY =
        Services.prefs.getBoolPref(kPrefAlwaysTryImsForEcc) ||
        ALWAYS_TRY_IMS_FOR_EMERGENCY;
    } catch (e) {
      ALWAYS_TRY_IMS_FOR_EMERGENCY = true;
    }
  },

  _updateImsEnable() {
    try {
      IMS_ENABLED = Services.prefs.getBoolPref(kPrefImsEnabled) || IMS_ENABLED;
    } catch (e) {
      IMS_ENABLED = false;
    }
  },

  _getDefaultServiceId() {
    let id = Services.prefs.getIntPref(kPrefDefaultServiceId);
    let numRil = Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);

    if (id >= numRil || id < 0) {
      id = 0;
    }

    return id;
  },

  _currentCalls: null,
  _enumerateCallsForClient(aClientId) {
    if (DEBUG) {
      debug("Enumeration of calls for client " + aClientId);
    }

    this._sendToRilWorker(aClientId, "getCurrentCalls", null, response => {
      if (response.errorMsg) {
        return;
      }

      // Clear all.
      this._currentCalls[aClientId] = {};

      for (let i in response.calls) {
        let call = (this._currentCalls[aClientId][i] = new Call(aClientId, i));
        this._updateCallFromRil(call, response.calls[i], aClientId);
      }
    });
  },

  /**
   * Checks whether to temporarily suppress caller id for the call.
   *
   * @param aMmi
   *        MMI full object.
   */
  _isTemporaryCLIR(aMmi) {
    return aMmi && aMmi.serviceCode === MMI_SC_CLIR && aMmi.dialNumber;
  },

  /**
   * Map MMI procedure to CLIR MODE.
   *
   * @param aProcedure
   *        MMI procedure
   */
  _procedureToCLIRMode(aProcedure) {
    // In temporary mode, MMI_PROCEDURE_ACTIVATION means allowing CLI
    // presentation, i.e. CLIR_SUPPRESSION. See TS 22.030, Annex B.
    switch (aProcedure) {
      case MMI_PROCEDURE_ACTIVATION:
        return Ci.nsIMobileConnection.CLIR_SUPPRESSION;
      case MMI_PROCEDURE_DEACTIVATION:
        return Ci.nsIMobileConnection.CLIR_INVOCATION;
      default:
        return Ci.nsIMobileConnection.CLIR_DEFAULT;
    }
  },

  _appliedTtyMode: nsITelephonyService.TTY_MODE_OFF,
  _applyTtyMode() {
    // Apply TTY mode preference only if headset is available.
    // Otherwise, set to OFF.
    let ttyMode =
      this._headsetState == nsITelephonyAudioService.HEADSET_STATE_HEADSET ||
      this._headsetState == nsITelephonyAudioService.HEADSET_STATE_HEADPHONE
        ? lazy.gAudioService.ttyMode
        : nsITelephonyService.TTY_MODE_OFF;

    // Apply only if it's different from what has been applied.
    if (this._appliedTtyMode == ttyMode) {
      return;
    }

    this._appliedTtyMode = ttyMode;
    lazy.gAudioService.applyTtyMode(ttyMode);

    for (let clientId = 0; clientId < this._numClients; clientId++) {
      this._sendToRilWorker(clientId, "setTtyMode", { ttyMode }, aResponse => {
        if (aResponse.errorMsg) {
          debug("Failed to set TTY Mode, error: " + aResponse.errorMsg);
        }
      });
    }
    lazy.gTelephonyMessenger.notifyTtyModeChanged(ttyMode);
    // TODO: send request to IMS stack (if existing) for setup IMS TTY bearer.
  },

  /**
   * nsITelephonyService interface.
   */

  defaultServiceId: 0,

  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index < 0) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.splice(index, 1);
  },

  enumerateCalls(aListener) {
    if (DEBUG) {
      debug("Requesting enumeration of calls for callback");
    }

    for (let cid = 0; cid < this._numClients; ++cid) {
      let calls = this._currentCalls[cid];
      if (!calls) {
        continue;
      }
      for (let i = 0, indexes = Object.keys(calls); i < indexes.length; ++i) {
        let call = calls[indexes[i]];
        let callInfo = new TelephonyCallInfo(call);
        aListener.enumerateCallState(callInfo);
      }
    }
    aListener.enumerateCallStateComplete();
  },

  _hasCalls(aClientId) {
    return Object.keys(this._currentCalls[aClientId]).length !== 0;
  },

  _hasCallsOnOtherClient(aClientId) {
    for (let cid = 0; cid < this._numClients; ++cid) {
      if (cid !== aClientId && this._hasCalls(cid)) {
        return true;
      }
    }
    return false;
  },

  // All calls in the conference is regarded as one conference call.
  _numCallsOnLine(aClientId, aRadioTech = -1) {
    let numCalls = 0;
    let hasConference = false;

    for (let cid in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][cid];
      if (aRadioTech !== -1 && call.radioTech !== aRadioTech) {
        continue;
      }
      if (call.isConference) {
        hasConference = true;
      } else {
        numCalls++;
      }
    }

    return hasConference ? numCalls + 1 : numCalls;
  },

  /**
   * Is there an active call?
   */
  _isActive(aClientId) {
    for (let index in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][index];
      if (call.state === nsITelephonyService.CALL_STATE_CONNECTED) {
        return true;
      }
    }
    return false;
  },

  /**
   * Select a proper client for dialing emergency call.
   *
   * @return clientId
   */
  _getClientIdForEmergencyCall() {
    // Select the client with sim card first.
    for (let cid = 0; cid < this._numClients; ++cid) {
      let icc = lazy.gIccService.getIccByServiceId(cid);
      let cardState = icc ? icc.cardState : Ci.nsIIcc.CARD_STATE_UNKONWN;
      if (
        cardState !== Ci.nsIIcc.CARD_STATE_UNDETECTED &&
        cardState !== Ci.nsIIcc.CARD_STATE_UNKNOWN
      ) {
        return cid;
      }
    }

    // Use the defualt client if no card presents.
    return EMERGENCY_CALL_DEFAULT_CLIENT_ID;
  },

  /**
   * Dial number. Perform call setup or SS procedure accordingly.
   *
   * @see 3GPP TS 22.030 Figure 3.5.3.2
   */
  dial(aClientId, aNumber, aIsDialEmergency, aType, aRttMode, aCallback) {
    if (DEBUG) {
      debug(
        "Dialing " +
          (aIsDialEmergency ? "emergency " : "") +
          aNumber +
          ", clientId: " +
          aClientId +
          ", aRttMode: " +
          aRttMode
      );
    }

    // We don't try to be too clever here, as the phone is probably in the
    // locked state. Let's just check if it's a number without normalizing
    if (!aIsDialEmergency) {
      aNumber = lazy.gPhoneNumberUtils.normalize(aNumber);
    }

    // Validate the number.
    // Note: isPlainPhoneNumber also accepts USSD and SS numbers
    if (!lazy.gPhoneNumberUtils.isPlainPhoneNumber(aNumber)) {
      if (DEBUG) {
        debug("Error: Number '" + aNumber + "' is not viable. Drop.");
      }
      aCallback.notifyError(DIAL_ERROR_BAD_NUMBER);
      return;
    }
    let isEmergencyNumber = lazy.DialNumberUtils.isEmergency(
      aNumber,
      aClientId
    );

    // DialEmergency accepts only emergency number.
    if (aIsDialEmergency && !isEmergencyNumber) {
      if (DEBUG) {
        debug("Error: Dial a non-emergency by dialEmergency. Drop.");
      }
      aCallback.notifyError(DIAL_ERROR_BAD_NUMBER);
      return;
    }

    if (isEmergencyNumber) {
      this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
      return;
    }

    // In cdma, we should always handle the request as a call.
    if (this._isCdmaClient(aClientId)) {
      this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
      return;
    }

    let mmi = lazy.DialNumberUtils.parseMMI(aNumber);
    if (mmi) {
      if (this._isTemporaryCLIR(mmi)) {
        this._dialCall(
          aClientId,
          mmi.dialNumber,
          aRttMode,
          this._procedureToCLIRMode(mmi.procedure),
          aCallback
        );
      } else {
        this._dialMMI(aClientId, mmi, aCallback);
      }
    } else if (aNumber[aNumber.length - 1] === "#") {
      // # string
      this._dialMMI(aClientId, { fullMMI: aNumber }, aCallback);
    } else if (aNumber.length <= 2) {
      // short string
      if (this._hasCalls(aClientId)) {
        this._dialInCallMMI(aClientId, aNumber, aRttMode, aCallback);
      } else if (
        (aNumber.length === 2 && aNumber[0] === "1") ||
        this._isTwoDigitShortCode(aNumber)
      ) {
        this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
      } else {
        this._dialMMI(aClientId, { fullMMI: aNumber }, aCallback);
      }
    } else {
      this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
    }
  },

  // Handling of supplementary services within a call as 3GPP TS 22.030 6.5.5
  _dialInCallMMI(aClientId, aNumber, aRttMode, aCallback) {
    let mmiCallback = {
      notifyError: () => aCallback.notifyDialMMIError(MMI_ERROR_KS_ERROR),
      notifySuccess: () =>
        aCallback.notifyDialMMISuccess(MMI_SM_KS_CALL_CONTROL),
    };

    if (aNumber === "0") {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      this._hangUpBackground(aClientId, mmiCallback);
    } else if (aNumber === "1") {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      this._hangUpForeground(aClientId, mmiCallback);
    } else if (aNumber[0] === "1" && aNumber.length === 2) {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      if (this._isImsClient(aClientId)) {
        // IMS call does not suppor 1X.
        mmiCallback.notifyError();
      } else {
        this.hangUpCall(aClientId, parseInt(aNumber[1]), mmiCallback);
      }
    } else if (aNumber === "2") {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      this._switchActiveCall(aClientId, mmiCallback);
    } else if (aNumber[0] === "2" && aNumber.length === 2) {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      if (this._isImsClient(aClientId)) {
        // IMS call does not suppor 2X.
        mmiCallback.notifyError();
      } else {
        this._separateCallGsm(aClientId, parseInt(aNumber[1]), mmiCallback);
      }
    } else if (aNumber === "3") {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      this._conferenceCallGsm(aClientId, mmiCallback);
    } else if (aNumber === "4") {
      aCallback.notifyDialMMI(MMI_KS_SC_CALL);
      this._explicitCallTransfer(aClientId, mmiCallback);
    } else if (this._isTwoDigitShortCode(aNumber)) {
      this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
    } else {
      this._dialMMI(aClientId, { fullMMI: aNumber }, aCallback);
    }
  },

  _dialCall(
    aClientId,
    aNumber,
    aRttMode,
    aClirMode = Ci.nsIMobileConnection.CLIR_DEFAULT,
    aCallback
  ) {
    if (this._isDialing) {
      if (DEBUG) {
        debug("Error: Already has a dialing call.");
      }
      aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
      return;
    }

    // We can only have at most two calls on the same line (client).
    if (this._numCallsOnLine(aClientId) >= 2) {
      if (DEBUG) {
        debug("Error: Already has more than 2 calls on line.");
      }
      aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
      return;
    }

    // For DSDS, if there is aleady a call on SIM 'aClientId', we cannot place
    // any new call on other SIM.
    if (this._hasCallsOnOtherClient(aClientId)) {
      if (DEBUG) {
        debug("Error: Already has a call on other sim.");
      }
      aCallback.notifyError(DIAL_ERROR_OTHER_CONNECTION_IN_USE);
      return;
    }

    let isEmergency = lazy.DialNumberUtils.isEmergency(aNumber, aClientId);

    if (!isEmergency) {
      if (!this._isRadioOn(aClientId) && !this._isVoWifi(aClientId)) {
        if (DEBUG) {
          debug("Error: Dial a normal call when radio off. Drop");
        }
        aCallback.notifyError(DIAL_ERROR_RADIO_NOT_AVAILABLE);
        return;
      }
    }

    if (isEmergency) {
      if (aClientId === -1) {
        // Automatically select a proper clientId for emergency call.
        if (DEBUG) {
          debug("Error: No client has been assigned.");
        }
        aClientId = this._getClientIdForEmergencyCall();
      }

      // Radio is off. Turn it on first.
      if (!this._isRadioOn(aClientId) && !this._isVoWifi(aClientId)) {
        let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
          aClientId
        );
        let listener = new MobileConnectionListener();

        listener.notifyRadioStateChanged = () => {
          if (this._isRadioOn(aClientId)) {
            this._dialCall(aClientId, aNumber, aRttMode, undefined, aCallback);
            connection.unregisterListener(listener);
          }
        };
        connection.registerListener(listener);

        // Turn on radio.
        connection.setRadioEnabled(true, {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess: () => {},
          notifyError: aErrorMsg => {
            connection.unregisterListener(listener);
            aCallback.notifyError(DIAL_ERROR_RADIO_NOT_AVAILABLE);
          },
        });

        return;
      }
    }

    let options = {
      isEmergency,
      number: aNumber,
      rttMode: aRttMode,
      clirMode: aClirMode,
    };

    // No active call. Dial it out directly.
    if (!this._isActive(aClientId)) {
      this._sendDialCallRequest(aClientId, options, aCallback);
      return;
    }

    // CDMA 3-way calling.
    if (this._isCdmaClient(aClientId)) {
      this._dialCdmaThreeWayCall(aClientId, options, aCallback);
      return;
    }

    // GSM. Hold the active call before dialing.
    if (DEBUG) {
      debug("There is an active call. Hold it first before dial.");
    }

    if (this._cachedDialRequest) {
      if (DEBUG) {
        debug("Error: There already is a pending dial request.");
      }
      aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
      return;
    }

    let callback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyCallback]),

      notifySuccess: () => {
        this._cachedDialRequest = {
          clientId: aClientId,
          options,
          callback: aCallback,
        };
      },

      notifyError: aErrorMsg => {
        if (DEBUG) {
          debug("Error: Fail to automatically hold the active call.");
        }
        aCallback.notifyError(aErrorMsg);
      },
    };

    if (this._isImsClient(aClientId)) {
      this._switchImsActiveCall(
        aClientId,
        Ci.nsITelephonyService.CALL_TYPE_VOICE,
        Ci.nsITelephonyService.RTT_MODE_OFF,
        callback
      );
    } else {
      this._switchActiveCall(aClientId, callback);
    }
  },

  _dialCdmaThreeWayCall(aClientId, aOptions, aCallback) {
    this._sendToRilWorker(
      aClientId,
      "cdmaFlash",
      { number: aOptions.number },
      response => {
        if (response.errorMsg) {
          aCallback.notifyError(response.errorMsg);
          return;
        }

        // RIL doesn't hold the 2nd call. We create one by ourselves.
        aCallback.notifyDialCallSuccess(
          aClientId,
          CDMA_SECOND_CALL_INDEX,
          aOptions.number,
          aOptions.isEmergency,
          nsITelephonyService.RTT_MODE_OFF,
          nsITelephonyService.CALL_VOICE_QUALITY_NORMAL,
          nsITelephonyCallInfo.STATE_AUDIO_ONLY,
          nsITelephonyCallInfo.CAPABILITY_SUPPORTS_NONE,
          nsITelephonyCallInfo.RADIO_TECH_CS
        );

        let childCall = (this._currentCalls[aClientId][
          CDMA_SECOND_CALL_INDEX
        ] = new Call(aClientId, CDMA_SECOND_CALL_INDEX));

        childCall.parentId = CDMA_FIRST_CALL_INDEX;
        childCall.state = nsITelephonyService.CALL_STATE_DIALING;
        childCall.number = aOptions.number;
        childCall.isOutgoing = true;
        childCall.isEmergency = aOptions.isEmergency;
        childCall.isConference = false;
        childCall.isSwitchable = false;
        childCall.isMergeable = true;

        // Manual update call state according to the request response.
        this._handleCallStateChanged(aClientId, [childCall]);

        childCall.state = nsITelephonyService.CALL_STATE_CONNECTED;

        let parentCall = this._currentCalls[aClientId][childCall.parentId];
        parentCall.childId = CDMA_SECOND_CALL_INDEX;
        parentCall.state = nsITelephonyService.CALL_STATE_HELD;
        parentCall.isSwitchable = false;
        parentCall.isMergeable = true;
        this._handleCallStateChanged(aClientId, [childCall, parentCall]);
      }
    );
  },

  _sendDialCallRequest(aClientId, aOptions, aCallback) {
    if (DEBUG) {
      debug("_sendDialCallRequest: " + aOptions);
    }
    this._isDialing = true;

    if (
      this._isImsClient(aClientId) ||
      this._useImsForEmergency(aOptions.isEmergency, aClientId)
    ) {
      this._dialImsCall(aClientId, aOptions, aCallback);
      return;
    }

    if (DEBUG) {
      debug("dial cs call");
    }

    this._sendToRilWorker(aClientId, "dial", aOptions, response => {
      this._isDialing = false;

      if (response.errorMsg) {
        this._sendToRilWorker(aClientId, "getFailCause", null, response => {
          aCallback.notifyError(response.failCause);
        });
      } else {
        this._ongoingDial = {
          clientId: aClientId,
          isEmergency: aOptions.isEmergency,
          rttMode: aOptions.rttMode,
          callback: aCallback,
          clirMode: aOptions.clirMode,
        };
      }
    });
  },

  /**
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _dialMMI(aClientId, aMmi, aCallback) {
    let mmiServiceCode = aMmi
      ? this._serviceCodeToKeyString(aMmi.serviceCode)
      : MMI_KS_SC_USSD;

    aCallback.notifyDialMMI(mmiServiceCode);

    if (
      mmiServiceCode !== MMI_KS_SC_IMEI &&
      !this._isRadioOn(aClientId) &&
      !this._isVoWifi(aClientId)
    ) {
      aCallback.notifyDialMMIError(DIAL_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    // We check if the MMI service code is supported and in that case we
    // trigger the appropriate RIL request if possible.
    switch (mmiServiceCode) {
      // Call Forwarding
      case MMI_KS_SC_CALL_FORWARDING:
        this._callForwardingMMI(aClientId, aMmi, aCallback);
        break;

      // Change the current ICC PIN number.
      case MMI_KS_SC_PIN:
      // Change the current ICC PIN2 number.
      // falls through
      case MMI_KS_SC_PIN2:
        this._iccChangeLockMMI(aClientId, aMmi, aCallback);
        break;

      // Unblock ICC PUK.
      case MMI_KS_SC_PUK:
      // Unblock ICC PUN2.
      // falls through
      case MMI_KS_SC_PUK2:
        this._iccUnlockMMI(aClientId, aMmi, aCallback);
        break;

      // IMEI
      case MMI_KS_SC_IMEI:
        this._getImeiMMI(aClientId, aMmi, aCallback);
        break;

      // CLIP
      case MMI_KS_SC_CLIP:
        this._clipMMI(aClientId, aMmi, aCallback);
        break;

      // CLIR (non-temporary ones)
      case MMI_KS_SC_CLIR:
        this._clirMMI(aClientId, aMmi, aCallback);
        break;

      // Change call barring password
      case MMI_KS_SC_CHANGE_PASSWORD:
        this._callBarringPasswordMMI(aClientId, aMmi, aCallback);
        break;

      // Call barring
      case MMI_KS_SC_CALL_BARRING:
        this._callBarringMMI(aClientId, aMmi, aCallback);
        break;

      // Call waiting
      case MMI_KS_SC_CALL_WAITING:
        this._callWaitingMMI(aClientId, aMmi, aCallback);
        break;

      // Handle unknown MMI code as USSD.
      default:
        if (this._ussdSessions[aClientId] == USSD_SESSION_ONGOING) {
          // Cancel the previous ussd session first.
          this._cancelUSSDInternal(aClientId, aResponse => {
            // Fail to cancel ussd session, report error instead of sending ussd
            // request.
            if (aResponse.errorMsg) {
              aCallback.notifyDialMMIError(aResponse.errorMsg);
              return;
            }
            this._sendUSSDInternal(
              aClientId,
              aMmi.fullMMI,
              this._defaultMMICallbackHandler.bind(this, aCallback)
            );
          });
          return;
        }
        this._sendUSSDInternal(
          aClientId,
          aMmi.fullMMI,
          this._defaultMMICallbackHandler.bind(this, aCallback)
        );
        break;
    }
  },

  /**
   * Handle call forwarding MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _callForwardingMMI(aClientId, aMmi, aCallback) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    let action = MMI_PROC_TO_CF_ACTION[aMmi.procedure];
    let reason = MMI_SC_TO_CF_REASON[aMmi.serviceCode];
    let serviceClass = this._siToServiceClass(aMmi.sib);

    if (action === Ci.nsIMobileConnection.CALL_FORWARD_ACTION_QUERY_STATUS) {
      connection.getCallForwarding(reason, serviceClass, {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIMobileConnectionCallback,
        ]),
        notifyGetCallForwardingSuccess(aCount, aResults) {
          aCallback.notifyDialMMISuccessWithCallForwardingOptions(
            MMI_SM_KS_SERVICE_INTERROGATED,
            aCount,
            aResults
          );
        },
        notifyError(aErrorMsg) {
          aCallback.notifyDialMMIError(aErrorMsg);
        },
      });
    } else {
      let number = aMmi.sia || "";
      let timeSeconds = aMmi.sic;
      // 3GPP TS 22.030 6.5.2
      // a call forwarding request with a single * would be
      // interpreted as registration if containing a forwarded-to
      // number, or an activation if not
      if (aMmi.procedure === MMI_PROCEDURE_ACTIVATION) {
        if (!number) {
          action = Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ENABLE;
        } else {
          action = Ci.nsIMobileConnection.CALL_FORWARD_ACTION_REGISTRATION;
        }
      }
      connection.setCallForwarding(
        action,
        reason,
        number,
        timeSeconds,
        serviceClass,
        {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess() {
            aCallback.notifyDialMMISuccess(CF_ACTION_TO_STATUS_MESSAGE[action]);
          },
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        }
      );
    }
  },

  /**
   * Handle icc change lock MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _iccChangeLockMMI(aClientId, aMmi, aCallback) {
    let errorMsg = this._getIccLockMMIError(aMmi);
    if (errorMsg) {
      aCallback.notifyDialMMIError(errorMsg);
      return;
    }

    let icc = lazy.gIccService.getIccByServiceId(aClientId);
    let lockType = MMI_SC_TO_LOCK_TYPE[aMmi.serviceCode];

    icc.changeCardLockPassword(lockType, aMmi.sia, aMmi.sib, {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIIccCallback]),
      notifySuccess() {
        aCallback.notifyDialMMISuccess(LOCK_TYPE_TO_STATUS_MESSAGE[lockType]);
      },
      notifyCardLockError(aErrorMsg, aRetryCount) {
        if (aRetryCount <= 0) {
          if (lockType === Ci.nsIIcc.CARD_LOCK_TYPE_PIN) {
            aErrorMsg = MMI_ERROR_KS_NEEDS_PUK;
          }

          aCallback.notifyDialMMIError(aErrorMsg);
          return;
        }

        aCallback.notifyDialMMIErrorWithInfo(MMI_ERROR_KS_BAD_PIN, aRetryCount);
      },
    });
  },

  /**
   * Handle icc unlock lock MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _iccUnlockMMI(aClientId, aMmi, aCallback) {
    let errorMsg = this._getIccLockMMIError(aMmi);
    if (errorMsg) {
      aCallback.notifyDialMMIError(errorMsg);
      return;
    }

    let icc = lazy.gIccService.getIccByServiceId(aClientId);
    let lockType = MMI_SC_TO_LOCK_TYPE[aMmi.serviceCode];

    icc.unlockCardLock(lockType, aMmi.sia, aMmi.sib, {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIIccCallback]),
      notifySuccess() {
        aCallback.notifyDialMMISuccess(LOCK_TYPE_TO_STATUS_MESSAGE[lockType]);
      },
      notifyCardLockError(aErrorMsg, aRetryCount) {
        if (aRetryCount <= 0) {
          if (lockType === Ci.nsIIcc.CARD_LOCK_TYPE_PUK) {
            aErrorMsg = MMI_ERROR_KS_SIM_BLOCKED;
          }

          aCallback.notifyDialMMIError(aErrorMsg);
          return;
        }

        aCallback.notifyDialMMIErrorWithInfo(MMI_ERROR_KS_BAD_PUK, aRetryCount);
      },
    });
  },

  /**
   * Handle IMEI MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _getImeiMMI(aClientId, aMmi, aCallback) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    if (!connection.deviceIdentities || !connection.deviceIdentities.imei) {
      aCallback.notifyDialMMIError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    aCallback.notifyDialMMISuccess(connection.deviceIdentities.imei);
  },

  /**
   * Handle CLIP MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _clipMMI(aClientId, aMmi, aCallback) {
    if (aMmi.procedure !== MMI_PROCEDURE_INTERROGATION) {
      aCallback.notifyDialMMIError(MMI_ERROR_KS_NOT_SUPPORTED);
      return;
    }

    this._sendToRilWorker(aClientId, "queryCLIP", {}, aResponse => {
      if (aResponse.errorMsg) {
        aCallback.notifyDialMMIError(aResponse.errorMsg);
        return;
      }

      // aResponse.provisioned informs about the called party receives the
      // calling party's address information:
      // 0 for CLIP not provisioned
      // 1 for CLIP provisioned
      // 2 for unknown
      switch (aResponse.provisioned) {
        case 0:
          aCallback.notifyDialMMISuccess(MMI_SM_KS_SERVICE_DISABLED);
          break;
        case 1:
          aCallback.notifyDialMMISuccess(MMI_SM_KS_SERVICE_ENABLED);
          break;
        default:
          aCallback.notifyDialMMIError(MMI_ERROR_KS_ERROR);
          break;
      }
    });
  },

  /**
   * Handle CLIR MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _clirMMI(aClientId, aMmi, aCallback) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    switch (aMmi.procedure) {
      case MMI_PROCEDURE_INTERROGATION:
        connection.getCallingLineIdRestriction({
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifyGetClirStatusSuccess(aN, aM) {
            let errorMsg;
            let statusMessage;
            // TS 27.007 +CLIR parameter 'm'.
            switch (aM) {
              // CLIR not provisioned.
              case 0:
                statusMessage = MMI_SM_KS_SERVICE_NOT_PROVISIONED;
                break;
              // CLIR provisioned in permanent mode.
              case 1:
                statusMessage = MMI_SM_KS_CLIR_PERMANENT;
                break;
              // Unknown (e.g. no network, etc.).
              case 2:
                errorMsg = MMI_ERROR_KS_ERROR;
                break;
              // CLIR temporary mode presentation restricted.
              case 3:
                // TS 27.007 +CLIR parameter 'n'.
                switch (aN) {
                  // Default.
                  case 0:
                  // CLIR invocation.
                  // falls through
                  case 1:
                    statusMessage = MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_ON;
                    break;
                  // CLIR suppression.
                  case 2:
                    statusMessage = MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_OFF;
                    break;
                  default:
                    errorMsg = RIL.GECKO_ERROR_GENERIC_FAILURE;
                    break;
                }
                break;
              // CLIR temporary mode presentation allowed.
              case 4:
                // TS 27.007 +CLIR parameter 'n'.
                switch (aN) {
                  // Default.
                  case 0:
                  // CLIR suppression.
                  // falls through
                  case 2:
                    statusMessage = MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_OFF;
                    break;
                  // CLIR invocation.
                  case 1:
                    statusMessage = MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_ON;
                    break;
                  default:
                    errorMsg = RIL.GECKO_ERROR_GENERIC_FAILURE;
                    break;
                }
                break;
              default:
                errorMsg = RIL.GECKO_ERROR_GENERIC_FAILURE;
                break;
            }

            if (errorMsg) {
              aCallback.notifyDialMMIError(errorMsg);
              return;
            }

            aCallback.notifyDialMMISuccess(statusMessage);
          },
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      case MMI_PROCEDURE_ACTIVATION:
      case MMI_PROCEDURE_DEACTIVATION: {
        let clirMode = MMI_PROC_TO_CLIR_ACTION[aMmi.procedure];
        connection.setCallingLineIdRestriction(clirMode, {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess() {
            aCallback.notifyDialMMISuccess(
              CLIR_ACTION_TO_STATUS_MESSAGE[clirMode]
            );
          },
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      }
      default:
        aCallback.notifyDialMMIError(MMI_ERROR_KS_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Handle change call barring password MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _callBarringPasswordMMI(aClientId, aMmi, aCallback) {
    if (
      aMmi.procedure !== MMI_PROCEDURE_REGISTRATION &&
      aMmi.procedure !== MMI_PROCEDURE_ACTIVATION
    ) {
      aCallback.notifyDialMMIError(MMI_ERROR_KS_INVALID_ACTION);
      return;
    }

    if (aMmi.sia !== "" && aMmi.sia !== MMI_ZZ_BARRING_SERVICE) {
      aCallback.notifyDialMMIError(MMI_ERROR_KS_NOT_SUPPORTED);
      return;
    }

    let validPassword = aSi => /^[0-9]{4}$/.test(aSi);
    if (
      !validPassword(aMmi.sib) ||
      !validPassword(aMmi.sic) ||
      !validPassword(aMmi.pwd)
    ) {
      aCallback.notifyDialMMIError(MMI_ERROR_KS_INVALID_PASSWORD);
      return;
    }

    if (aMmi.sic !== aMmi.pwd) {
      aCallback.notifyDialMMIError(MMI_ERROR_KS_MISMATCH_PASSWORD);
      return;
    }

    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    connection.changeCallBarringPassword(aMmi.sib, aMmi.sic, {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIMobileConnectionCallback]),
      notifySuccess() {
        aCallback.notifyDialMMISuccess(MMI_SM_KS_PASSWORD_CHANGED);
      },
      notifyError(aErrorMsg) {
        aCallback.notifyDialMMIError(aErrorMsg);
      },
    });
  },

  /**
   * Handle call barring MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _callBarringMMI(aClientId, aMmi, aCallback) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );
    let program = MMI_SC_TO_CB_PROGRAM[aMmi.serviceCode];
    let password = aMmi.sia || "";
    let serviceClass = this._siToServiceClass(aMmi.sib);

    switch (aMmi.procedure) {
      case MMI_PROCEDURE_INTERROGATION:
        connection.getCallBarring(program, password, serviceClass, {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifyGetCallBarringSuccess: function(
            aProgram,
            aEnabled,
            aServiceClass
          ) {
            if (!aEnabled) {
              aCallback.notifyDialMMISuccess(MMI_SM_KS_SERVICE_DISABLED);
              return;
            }

            let services = this._serviceClassToStringArray(aServiceClass);
            aCallback.notifyDialMMISuccessWithStrings(
              MMI_SM_KS_SERVICE_ENABLED_FOR,
              services.length,
              services
            );
          }.bind(this),
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      case MMI_PROCEDURE_ACTIVATION:
      case MMI_PROCEDURE_DEACTIVATION: {
        let enabled = aMmi.procedure === MMI_PROCEDURE_ACTIVATION;
        connection.setCallBarring(program, enabled, password, serviceClass, {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess() {
            aCallback.notifyDialMMISuccess(
              enabled ? MMI_SM_KS_SERVICE_ENABLED : MMI_SM_KS_SERVICE_DISABLED
            );
          },
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      }
      default:
        aCallback.notifyDialMMIError(MMI_ERROR_KS_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Handle call waiting MMI code.
   *
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   */
  _callWaitingMMI(aClientId, aMmi, aCallback) {
    let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
      aClientId
    );

    switch (aMmi.procedure) {
      case MMI_PROCEDURE_INTERROGATION:
        connection.getCallWaiting({
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifyGetCallWaitingSuccess: function(aServiceClass) {
            if (
              aServiceClass === Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE
            ) {
              aCallback.notifyDialMMISuccess(MMI_SM_KS_SERVICE_DISABLED);
              return;
            }

            let services = this._serviceClassToStringArray(aServiceClass);
            aCallback.notifyDialMMISuccessWithStrings(
              MMI_SM_KS_SERVICE_ENABLED_FOR,
              services.length,
              services
            );
          }.bind(this),
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      case MMI_PROCEDURE_ACTIVATION:
      case MMI_PROCEDURE_DEACTIVATION: {
        let enabled = aMmi.procedure === MMI_PROCEDURE_ACTIVATION;
        let serviceClass = this._siToServiceClass(aMmi.sia);
        connection.setCallWaiting(enabled, serviceClass, {
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIMobileConnectionCallback,
          ]),
          notifySuccess() {
            aCallback.notifyDialMMISuccess(
              enabled ? MMI_SM_KS_SERVICE_ENABLED : MMI_SM_KS_SERVICE_DISABLED
            );
          },
          notifyError(aErrorMsg) {
            aCallback.notifyDialMMIError(aErrorMsg);
          },
        });
        break;
      }
      default:
        aCallback.notifyDialMMIError(MMI_ERROR_KS_NOT_SUPPORTED);
        break;
    }
  },

  _serviceCodeToKeyString(aServiceCode) {
    switch (aServiceCode) {
      case MMI_SC_CFU:
      case MMI_SC_CF_BUSY:
      case MMI_SC_CF_NO_REPLY:
      case MMI_SC_CF_NOT_REACHABLE:
      case MMI_SC_CF_ALL:
      case MMI_SC_CF_ALL_CONDITIONAL:
        return MMI_KS_SC_CALL_FORWARDING;
      case MMI_SC_PIN:
        return MMI_KS_SC_PIN;
      case MMI_SC_PIN2:
        return MMI_KS_SC_PIN2;
      case MMI_SC_PUK:
        return MMI_KS_SC_PUK;
      case MMI_SC_PUK2:
        return MMI_KS_SC_PUK2;
      case MMI_SC_IMEI:
        return MMI_KS_SC_IMEI;
      case MMI_SC_CLIP:
        return MMI_KS_SC_CLIP;
      case MMI_SC_CLIR:
        return MMI_KS_SC_CLIR;
      case MMI_SC_BAOC:
      case MMI_SC_BAOIC:
      case MMI_SC_BAOICxH:
      case MMI_SC_BAIC:
      case MMI_SC_BAICr:
      case MMI_SC_BA_ALL:
      case MMI_SC_BA_MO:
      case MMI_SC_BA_MT:
        return MMI_KS_SC_CALL_BARRING;
      case MMI_SC_CALL_WAITING:
        return MMI_KS_SC_CALL_WAITING;
      case MMI_SC_CHANGE_PASSWORD:
        return MMI_KS_SC_CHANGE_PASSWORD;
      default:
        return MMI_KS_SC_USSD;
    }
  },

  /**
   * Helper for translating basic service group to service class parameter.
   */
  _siToServiceClass(aSi) {
    if (!aSi) {
      return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE;
    }

    let serviceCode = parseInt(aSi, 10);
    switch (serviceCode) {
      case 10:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_SMS +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_FAX +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
        );
      case 11:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE;
      case 12:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_SMS +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_FAX
        );
      case 13:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_FAX;
      case 16:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_SMS;
      case 19:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_FAX +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
        );
      case 21:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_PAD +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_ASYNC
        );
      case 22:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_PACKET +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_SYNC
        );
      case 25:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_ASYNC;
      case 26:
        return (
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_DATA_SYNC +
          Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE
        );
      case 99:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_PACKET;
      default:
        return Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE;
    }
  },

  _serviceClassToStringArray(aServiceClass) {
    let services = [];
    for (
      let mask = Ci.nsIMobileConnection.ICC_SERVICE_CLASS_VOICE;
      mask <= Ci.nsIMobileConnection.ICC_SERVICE_CLASS_MAX;
      mask <<= 1
    ) {
      if (mask & aServiceClass) {
        services.push(MMI_KS_SERVICE_CLASS_MAPPING[mask]);
      }
    }
    return services;
  },

  _getIccLockMMIError(aMmi) {
    // As defined in TS.122.030 6.6.2 to change the ICC PIN we should expect
    // an MMI code of the form **04*OLD_PIN*NEW_PIN*NEW_PIN#, where old PIN
    // should be entered as the SIA parameter and the new PIN as SIB and
    // SIC.
    if (aMmi.procedure !== MMI_PROCEDURE_REGISTRATION) {
      return MMI_ERROR_KS_INVALID_ACTION;
    }

    if (!aMmi.sia || !aMmi.sib || !aMmi.sic) {
      return MMI_ERROR_KS_ERROR;
    }

    if (
      aMmi.sia.length < 4 ||
      aMmi.sia.length > 8 ||
      aMmi.sib.length < 4 ||
      aMmi.sib.length > 8 ||
      aMmi.sic.length < 4 ||
      aMmi.sic.length > 8
    ) {
      return MMI_ERROR_KS_INVALID_PIN;
    }

    if (aMmi.sib != aMmi.sic) {
      return MMI_ERROR_KS_MISMATCH_PIN;
    }

    return null;
  },

  /**
   * The default callback handler for call operations.
   *
   * @param aCallback
   *        An callback object including notifySuccess() and notifyError(aMsg)
   * @param aResponse
   *        The response from ril_worker.
   */
  _defaultCallbackHandler(aCallback, aResponse) {
    if (aResponse.errorMsg) {
      aCallback.notifyError(aResponse.errorMsg);
    } else {
      aCallback.notifySuccess();
    }
  },

  _defaultMMICallbackHandler(aCallback, aResponse) {
    if (aResponse.errorMsg) {
      aCallback.notifyDialMMIError(aResponse.errorMsg);
    } else {
      aCallback.notifyDialMMISuccess("");
    }
  },

  _getCallsWithState(aClientId, aState) {
    let calls = [];
    for (let i in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][i];
      if (call.state === aState) {
        calls.push(call);
      }
    }
    return calls;
  },

  /**
   * Update call information from RIL.
   *
   * @return Boolean to indicate whether the data is changed.
   */
  _updateCallFromRil(aCall, aRilCall, aClientId) {
    aRilCall.state = this._convertRILCallState(aRilCall.state);
    aRilCall.number = this._formatInternationalNumber(
      aRilCall.number,
      aRilCall.toa
    );

    let change = false;
    const key = [
      "state",
      "number",
      "numberPresentation",
      "name",
      "namePresentation",
      "rttMode",
      "isVt",
      "isConferenceParent",
      "voiceQuality",
      "radioTech",
      "capabilities",
      "vowifiCallQuality",
      "isMarkable",
      "verStatus",
    ];

    for (let k of key) {
      if (k === "isVt") {
        aCall[k] |= aRilCall[k];
      } else if (aCall[k] != aRilCall[k]) {
        aCall[k] = aRilCall[k];
        change = true;
      }
    }

    aCall.isOutgoing = !aRilCall.isMT;
    aCall.isEmergency = lazy.DialNumberUtils.isEmergency(
      aCall.number,
      aClientId
    );
    if (
      aCall.rttMode == nsITelephonyService.RTT_MODE_FULL &&
      aCall.state == nsITelephonyService.CALL_STATE_CONNECTED
    ) {
      aCall.isRtt = true;
    }

    if (
      !aCall.started &&
      aCall.state == nsITelephonyService.CALL_STATE_CONNECTED
    ) {
      aCall.started = new Date().getTime();
    }

    return change;
  },

  /**
   * Identify the conference group.
   * @return [conference state, array of calls in group]
   *
   * TODO: handle multi-sim case.
   */
  _detectConference(aClientId) {
    // There are some difficuties to identify the conference by |.isMpty| from RIL
    // so we don't rely on this flag.
    //  - |.isMpty| becomes false when the conference call is put on hold.
    //  - |.isMpty| may remain true when other participants left the conference.

    // All the calls in the conference should have the same state and it is
    // either CONNECTED or HELD. That means, if we find a group of call with
    // the same state and its size is larger than 2, it must be a conference.
    let connectedCalls = this._getCallsWithState(
      aClientId,
      nsITelephonyService.CALL_STATE_CONNECTED
    );
    let heldCalls = this._getCallsWithState(
      aClientId,
      nsITelephonyService.CALL_STATE_HELD
    );

    if (connectedCalls.length >= 2) {
      return [nsITelephonyService.CALL_STATE_CONNECTED, connectedCalls];
    } else if (heldCalls.length >= 2) {
      return [nsITelephonyService.CALL_STATE_HELD, heldCalls];
    }

    return [nsITelephonyService.CALL_STATE_UNKNOWN, null];
  },

  /**
   * Update the isConference flag of all Calls.
   *
   * @return [conference state, array of calls being updated]
   */
  _updateConference(aClientId) {
    let [newConferenceState, conferenceCalls] = this._detectConference(
      aClientId
    );
    if (DEBUG) {
      debug("Conference state: " + newConferenceState);
    }

    let changedCalls = [];
    let conference = new Set(conferenceCalls);

    for (let i in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][i];
      let isConference = conference.has(call);
      if (call.isConference != isConference) {
        call.isConference = isConference;
        changedCalls.push(call);
      }
    }

    return [newConferenceState, changedCalls];
  },

  sendTones(aClientId, aDtmfChars, aPauseDuration, aToneDuration, aCallback) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let tones = aDtmfChars;
    let isIms = this._isImsClient(aClientId);
    let self = this;

    let playTone = tone => {
      if (DEBUG) {
        debug("sendTones playTone: " + tone);
      }
      let run = function() {
        if (DEBUG) {
          debug("sendTones run: " + tone);
        }
        timer.initWithCallback(
          () => {
            self.stopTone(aClientId);
            timer.initWithCallback(
              () => {
                if (tones.length === 1) {
                  if (DEBUG) {
                    debug("sendTones success");
                  }
                  aCallback.notifySuccess();
                } else {
                  tones = tones.substr(1);
                  playTone(tones[0]);
                }
              },
              TONES_GAP_DURATION,
              Ci.nsITimer.TYPE_ONE_SHOT
            );
          },
          aToneDuration,
          Ci.nsITimer.TYPE_ONE_SHOT
        );
      };

      if (isIms) {
        let callback = {
          notifySuccess() {
            if (DEBUG) {
              debug("sendTones notifySuccess");
            }
            run();
          },

          notifyError(aError) {
            if (DEBUG) {
              debug("sendTones notifyError: " + aError);
            }
            aCallback.notifyError(aError);
          },
        };
        this._sendImsTone(aClientId, tone, callback);
      } else {
        this._sendToRilWorker(
          aClientId,
          "startTone",
          { dtmfChar: tone },
          response => {
            if (response.errorMsg) {
              aCallback.notifyError(response.errorMsg);
              return;
            }
            run();
          }
        );
      }
    };

    timer.initWithCallback(
      () => {
        playTone(tones[0]);
      },
      aPauseDuration,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  startTone(aClientId, aDtmfChar) {
    if (this._isImsClient(aClientId)) {
      this._startImsTone(aClientId, aDtmfChar);
      return;
    }
    this._sendToRilWorker(aClientId, "startTone", { dtmfChar: aDtmfChar });
  },

  stopTone(aClientId) {
    if (this._isImsClient(aClientId)) {
      this._stopImsTone(aClientId);
      return;
    }
    this._sendToRilWorker(aClientId, "stopTone");
  },

  answerCall(aClientId, aCallIndex, aType, aRttMode, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (DEBUG) {
      lazy.console.log("answerCall state: " + call.state);
    }
    if (!call || call.state != nsITelephonyService.CALL_STATE_INCOMING) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let callNum = Object.keys(this._currentCalls[aClientId]).length;
    if (callNum !== 1) {
      this._switchActiveCall(aClientId, aCallback);
    } else {
      if (DEBUG) {
        debug("answerCall isImsCall: " + call.isImsCall());
      }
      if (call.isImsCall()) {
        this._answerImsPhone(aClientId, aCallIndex, aType, aRttMode, aCallback);
      } else {
        if (DEBUG) {
          lazy.console.log("answerCall sendToRil");
        }
        this._sendToRilWorker(
          aClientId,
          "answerCall",
          null,
          this._defaultCallbackHandler.bind(this, aCallback)
        );
      }
    }
  },

  rejectCall(aClientId, aCallIndex, aReason, aCallback) {
    if (DEBUG) {
      debug("rejectCall aCallIndex : " + aCallIndex);
    }
    if (this._isCdmaClient(aClientId)) {
      this._hangUpBackground(aClientId, aCallback);
      return;
    }

    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || call.state != nsITelephonyService.CALL_STATE_INCOMING) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let callNum = Object.keys(this._currentCalls[aClientId]).length;
    if (callNum !== 1) {
      this._hangUpBackground(aClientId, aCallback);
    } else {
      call.hangUpLocal = true;
      if (call.isImsCall()) {
        this._rejectImsCall(aClientId, aCallIndex, aReason, aCallback);
      } else {
        this._sendToRilWorker(
          aClientId,
          "udub",
          null,
          this._defaultCallbackHandler.bind(this, aCallback)
        );
      }
    }
  },

  hangUpAllCalls(aClientId, aCallback) {
    if (!this._hasCalls(aClientId)) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let calls = [];
    for (let cid in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][cid];
      if (
        call.state !== nsITelephonyService.CALL_STATE_UNKNOWN &&
        call.state !== nsITelephonyService.CALL_STATE_DISCONNECTED
      ) {
        calls.push(call);
      }
    }

    if (calls.length <= 0) {
      // No valid call to disconnect, report error back.
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let disconnectCalls = [...new Set(calls)];
    disconnectCalls.forEach(call => {
      call.hangUpLocal = true;
      if (this._isImsClient(aClientId)) {
        this._hangupImsCall(aClientId, call.callIndex, aCallback);
      } else {
        // TODO: partner can change below code to their own specific API for
        // preformance improvement, but move _sendToRilWorker out of forEach block.
        // [Remember to disable test_hangup_all_calls.js if API changes.]
        this._sendToRilWorker(
          aClientId,
          "hangUpCall",
          { callIndex: call.callIndex },
          this._defaultCallbackHandler.bind(this, aCallback)
        );
      }
    });
  },

  _hangUpForeground(aClientId, aCallback) {
    let calls = this._getCallsWithState(
      aClientId,
      nsITelephonyService.CALL_STATE_CONNECTED
    );
    calls.forEach(call => (call.hangUpLocal = true));

    if (this._isImsClient(aClientId)) {
      this._hangUpImsForeground(aClientId, aCallback);
    } else {
      this._sendToRilWorker(
        aClientId,
        "hangUpForeground",
        null,
        this._defaultCallbackHandler.bind(this, aCallback)
      );
    }
  },

  _hangUpBackground(aClientId, aCallback) {
    // When both a held and a waiting call exist, the request shall apply to
    // the waiting call.
    let waitingCalls = this._getCallsWithState(
      aClientId,
      nsITelephonyService.CALL_STATE_INCOMING
    );
    let heldCalls = this._getCallsWithState(
      aClientId,
      nsITelephonyService.CALL_STATE_HELD
    );

    if (waitingCalls.length) {
      waitingCalls.forEach(call => (call.hangUpLocal = true));
    } else {
      heldCalls.forEach(call => (call.hangUpLocal = true));
    }

    if (this._isImsClient(aClientId)) {
      this._hangUpImsBackground(aClientId, aCallback);
    } else {
      this._sendToRilWorker(
        aClientId,
        "hangUpBackground",
        null,
        this._defaultCallbackHandler.bind(this, aCallback)
      );
    }
  },

  hangUpCall(aClientId, aCallIndex, aReason, aCallback) {
    // Should release both, child and parent, together. Since RIL holds only
    // the parent call, we send 'parentId' to RIL.
    aCallIndex =
      this._currentCalls[aClientId][aCallIndex].parentId || aCallIndex;

    if (DEBUG) {
      debug("hangUpCall aCallIndex : " + aCallIndex);
    }

    let call = this._currentCalls[aClientId][aCallIndex];

    call.hangUpLocal = true;
    if (call.isImsCall()) {
      this._hangupImsCall(aClientId, aCallIndex, aReason, aCallback);
    } else {
      this._sendToRilWorker(
        aClientId,
        "hangUpCall",
        { callIndex: aCallIndex },
        this._defaultCallbackHandler.bind(this, aCallback)
      );
    }
  },

  _switchCall(aClientId, aCallIndex, aCallback, aRequiredState) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    if (this._isCdmaClient(aClientId)) {
      this._switchCallCdma(aClientId, aCallIndex, aCallback);
    } else {
      this._switchCallGsm(aClientId, aCallIndex, aCallback, aRequiredState);
    }
  },

  _switchCallGsm(aClientId, aCallIndex, aCallback, aRequiredState) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (call.state != aRequiredState) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    this._switchActiveCall(aClientId, aCallback);
  },

  _switchActiveCall(
    aClientId,
    aCallback,
    aCallType = Ci.nsITelephonyService.CALL_TYPE_VOICE,
    aRttMode = Ci.nsITelephonyService.RTT_MODE_OFF
  ) {
    if (this._isImsClient(aClientId)) {
      this._switchImsActiveCall(aClientId, aCallType, aRttMode, aCallback);
    } else {
      this._sendToRilWorker(
        aClientId,
        "switchActiveCall",
        null,
        this._defaultCallbackHandler.bind(this, aCallback)
      );
    }
  },

  _switchImsActiveCall(aClientId, aCallType, aRttMode, aCallback) {
    if (DEBUG) {
      debug("_switchImsActiveCall");
    }
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    let imsCallback = this._createSimpleImsCallback(aCallback);
    imsPhone.switchActiveCall(aCallType, aRttMode, imsCallback);
  },

  _switchCallCdma(aClientId, aCallIndex, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call.isSwitchable) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    this._sendToRilWorker(
      aClientId,
      "cdmaFlash",
      null,
      this._defaultCallbackHandler.bind(this, aCallback)
    );
  },

  _explicitCallTransfer(aClientId, aCallback) {
    if (this._isImsClient(aClientId)) {
      let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
      imsPhone.consultativeTransfer(this._createSimpleImsCallback(aCallback));
    } else {
      this._sendToRilWorker(
        aClientId,
        "explicitCallTransfer",
        null,
        this._defaultCallbackHandler.bind(this, aCallback)
      );
    }
  },

  holdCall(aClientId, aCallIndex, aCallback) {
    this._switchCall(
      aClientId,
      aCallIndex,
      aCallback,
      nsITelephonyService.CALL_STATE_CONNECTED
    );
  },

  resumeCall(aClientId, aCallIndex, aCallback) {
    this._switchCall(
      aClientId,
      aCallIndex,
      aCallback,
      nsITelephonyService.CALL_STATE_HELD
    );
  },

  _conferenceCallGsm(aClientId, aCallback) {
    if (this._isImsClient(aClientId)) {
      debug("ims conferenceCall with clientId: " + aClientId);
      let self = this;
      let callback = {
        QueryInterface: ChromeUtils.generateQI([Ci.nsIImsPhoneCallback]),
        notifySuccess() {
          aCallback.notifySuccess();
        },
        notifyError(aError) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          self._notifyAllListeners("notifyConferenceError", [
            RIL.GECKO_CONF_CALL_ERROR_ADD,
            aError,
          ]);
        },
      };

      let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
      imsPhone.conferenceCall(callback);

      return;
    }

    this._sendToRilWorker(aClientId, "conferenceCall", null, response => {
      if (response.errorMsg) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        // TODO: Bug 1124993. Deprecate it. Use callback response is enough.
        this._notifyAllListeners("notifyConferenceError", [
          RIL.GECKO_CONF_CALL_ERROR_ADD,
          response.errorMsg,
        ]);
        return;
      }

      aCallback.notifySuccess();
    });
  },

  _conferenceCallCdma(aClientId, aCallback) {
    for (let index in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][index];
      if (!call.isMergeable) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        return;
      }
    }

    this._sendToRilWorker(aClientId, "cdmaFlash", null, response => {
      if (response.errorMsg) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        // TODO: Bug 1124993. Deprecate it. Use callback response is enough.
        this._notifyAllListeners("notifyConferenceError", [
          RIL.GECKO_CONF_CALL_ERROR_ADD,
          response.errorMsg,
        ]);
        return;
      }

      let calls = [];
      for (let index in this._currentCalls[aClientId]) {
        let call = this._currentCalls[aClientId][index];
        call.state = nsITelephonyService.CALL_STATE_CONNECTED;
        call.isConference = true;
        calls.push(call);
      }
      this._handleCallStateChanged(aClientId, calls);

      aCallback.notifySuccess();
    });
  },

  conferenceCall(aClientId, aCallback) {
    if (Object.keys(this._currentCalls[aClientId]).length < 2) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    debug("conferenceCall");

    if (this._isCdmaClient(aClientId)) {
      this._conferenceCallCdma(aClientId, aCallback);
    } else {
      this._conferenceCallGsm(aClientId, aCallback);
    }
  },

  _separateCallGsm(aClientId, aCallIndex, aCallback) {
    this._sendToRilWorker(
      aClientId,
      "separateCall",
      { callIndex: aCallIndex },
      response => {
        if (response.errorMsg) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          // TODO: Bug 1124993. Deprecate it. Use callback response is enough.
          this._notifyAllListeners("notifyConferenceError", [
            RIL.GECKO_CONF_CALL_ERROR_REMOVE,
            response.errorMsg,
          ]);
          return;
        }

        aCallback.notifySuccess();
      }
    );
  },

  _removeCdmaSecondCall(aClientId) {
    let childCall = this._currentCalls[aClientId][CDMA_SECOND_CALL_INDEX];
    let parentCall = this._currentCalls[aClientId][CDMA_FIRST_CALL_INDEX];

    this._disconnectCalls(aClientId, [childCall]);

    parentCall.isConference = false;
    parentCall.isSwitchable = true;
    parentCall.isMergeable = true;
    this._handleCallStateChanged(aClientId, [childCall, parentCall]);
  },

  // See 3gpp2, S.R0006-522-A v1.0. Table 4, XID 6S.
  // Release the third party. Optionally apply a warning tone. Connect the
  // controlling subscriber and the second party. Go to the 2-way state.
  _separateCallCdma(aClientId, aCallIndex, aCallback) {
    this._sendToRilWorker(aClientId, "cdmaFlash", null, response => {
      if (response.errorMsg) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        // TODO: Bug 1124993. Deprecate it. Use callback response is enough.
        this._notifyAllListeners("notifyConferenceError", [
          RIL.GECKO_CONF_CALL_ERROR_REMOVE,
          response.errorMsg,
        ]);
        return;
      }

      this._removeCdmaSecondCall(aClientId);
      aCallback.notifySuccess();
    });
  },

  separateCall(aClientId, aCallIndex, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isConference) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    if (this._isCdmaClient(aClientId)) {
      this._separateCallCdma(aClientId, aCallIndex, aCallback);
    } else {
      this._separateCallGsm(aClientId, aCallIndex, aCallback);
    }
  },

  hangUpConference(aClientId, aCallback) {
    if (DEBUG) {
      debug("hangUpConference");
    }
    // In cdma, ril only maintains one call index.
    if (this._isCdmaClient(aClientId)) {
      this._sendToRilWorker(
        aClientId,
        "hangUpCall",
        { callIndex: CDMA_FIRST_CALL_INDEX },
        this._defaultCallbackHandler.bind(this, aCallback)
      );
      return;
    }

    if (this._isImsClient(aClientId)) {
      this._hangupImsConference(aClientId, aCallback);
      return;
    }

    // Find a conference call, and send the corresponding request to RIL worker.
    for (let index in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][index];
      if (!call.isConference) {
        continue;
      }

      let command =
        call.state === nsITelephonyService.CALL_STATE_CONNECTED
          ? "hangUpForeground"
          : "hangUpBackground";
      this._sendToRilWorker(
        aClientId,
        command,
        null,
        this._defaultCallbackHandler.bind(this, aCallback)
      );
      return;
    }

    // There is no conference call.
    if (DEBUG) {
      debug(
        "hangUpConference: No conference call in modem[" + aClientId + "]."
      );
    }
    aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
  },

  _hangupImsConference(aClientId, aCallback) {
    for (let index in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][index];
      if (!call.isConferenceParent) {
        continue;
      }

      let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
      if (imsPhone) {
        imsPhone.hangupCall(
          index,
          nsITelephonyService.CALL_FAIL_NONE,
          this._createSimpleImsCallback(aCallback)
        );
        return;
      }
    }

    // There is no conference call.
    if (DEBUG) {
      debug(
        "hangUpConference: No conference call in modem[" + aClientId + "]."
      );
    }
    aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
  },

  _switchConference(aClientId, aCallback) {
    // Cannot hold/resume a conference in cdma.
    if (this._isCdmaClient(aClientId)) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    this._switchActiveCall(aClientId, aCallback);
  },

  holdConference(aClientId, aCallback) {
    this._switchConference(aClientId, aCallback);
  },

  resumeConference(aClientId, aCallback) {
    this._switchConference(aClientId, aCallback);
  },

  sendUSSD(aClientId, aUssd, aCallback) {
    this._sendUSSDInternal(
      aClientId,
      aUssd,
      this._defaultCallbackHandler.bind(this, aCallback)
    );
  },

  _sendUSSDInternal(aClientId, aUssd, aCallback) {
    this._ussdSessions[aClientId] = USSD_SESSION_ONGOING;
    this._sendToRilWorker(aClientId, "sendUSSD", { ussd: aUssd }, aResponse => {
      if (aResponse.errorMsg) {
        this._ussdSessions[aClientId] = USSD_SESSION_DONE;
      }
      aCallback(aResponse);
    });
  },

  cancelUSSD(aClientId, aCallback) {
    this._cancelUSSDInternal(
      aClientId,
      this._defaultCallbackHandler.bind(this, aCallback)
    );
  },

  _cancelUSSDInternal(aClientId, aCallback) {
    this._ussdSessions[aClientId] = USSD_SESSION_CANCELLING;
    this._sendToRilWorker(aClientId, "cancelUSSD", {}, aResponse => {
      if (aResponse.errorMsg) {
        this._ussdSessions[aClientId] = USSD_SESSION_ONGOING;
      }
      aCallback(aResponse);
    });
  },

  getVideoCallProvider(aClientId, aCallIndex) {
    if (DEBUG) {
      debug("getVideoCallProvider aCallIndex: " + aCallIndex);
    }

    let call = this._currentCalls[aClientId][aCallIndex];
    return call.getVideoCallProvider();
  },

  sendRttModify(aClientId, aCallIndex, aRttMode, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call.isImsCall()) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.sendRttModifyRequest(
      aCallIndex,
      aRttMode,
      this._createSimpleImsCallback(aCallback)
    );
  },

  sendRttModifyResponse(aClientId, aCallIndex, aStatus, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call.isImsCall()) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.sendRttModifyResponse(
      aCallIndex,
      aStatus,
      this._createSimpleImsCallback(aCallback)
    );
  },

  sendRttMessage(aClientId, aCallIndex, aMessage, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call.isImsCall() || !call.isRtt) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.sendRttMessage(
      aCallIndex,
      aMessage,
      this._createSimpleImsCallback(aCallback)
    );
  },

  get hacMode() {
    return lazy.gAudioService.hacMode;
  },

  set hacMode(aEnabled) {
    lazy.gAudioService.hacMode = aEnabled;
  },

  get microphoneMuted() {
    return lazy.gAudioService.microphoneMuted;
  },

  set microphoneMuted(aMuted) {
    lazy.gAudioService.microphoneMuted = aMuted;
  },

  get speakerEnabled() {
    return lazy.gAudioService.speakerEnabled;
  },

  set speakerEnabled(aEnabled) {
    lazy.gAudioService.speakerEnabled = aEnabled;
  },

  get ttyMode() {
    return lazy.gAudioService.ttyMode;
  },

  set ttyMode(aMode) {
    lazy.gAudioService.ttyMode = aMode;
    this._applyTtyMode();
  },

  /**
   * nsIGonkTelephonyService interface.
   */

  _notifyCallEnded(aCall) {
    let duration =
      "started" in aCall && typeof aCall.started == "number"
        ? new Date().getTime() - aCall.started
        : 0;

    // Don't save conference parent into call log.
    if (!aCall.isConferenceParent) {
      lazy.gTelephonyMessenger.notifyCallEnded(
        aCall.clientId,
        aCall.number,
        this._cdmaCallWaitingNumber,
        aCall.isEmergency,
        duration,
        aCall.isOutgoing,
        aCall.hangUpLocal,
        aCall.isVt,
        aCall.radioTech,
        aCall.isRtt,
        aCall.verStatus
      );
    }

    // Clear cache of this._cdmaCallWaitingNumber after call disconnected.
    this._cdmaCallWaitingNumber = null;
  },

  /**
   * Disconnect calls by updating their states. Sometimes, it may cause other
   * calls being disconnected as well.
   *
   * @return Array a list of calls we need to fire callStateChange
   */
  _disconnectCalls(
    aClientId,
    aCalls,
    aFailCause = RIL.GECKO_CALL_ERROR_NORMAL_CALL_CLEARING
  ) {
    if (DEBUG) {
      debug("_disconnectCalls: " + JSON.stringify(aCalls));
    }

    // In addition to the disconnected call itself, its decedent calls should be
    // treated as disconnected calls as well.
    let disconnectedCalls = aCalls.slice();
    for (let call in aCalls) {
      while (call.childId) {
        call = this._currentCalls[aClientId][call.childId];
        disconnectedCalls.push(call);
      }
    }

    // Store unique value in the list.
    disconnectedCalls = [...new Set(disconnectedCalls)];

    let callsForStateChanged = [];

    disconnectedCalls.forEach(call => {
      call.state = nsITelephonyService.CALL_STATE_DISCONNECTED;
      call.disconnectedReason = aFailCause;

      let parentCall = this._currentCalls[aClientId][call.parentId];
      if (parentCall) {
        delete parentCall.childId;
      }

      this._notifyCallEnded(call);

      callsForStateChanged.push(call);

      delete this._currentCalls[aClientId][call.callIndex];
    });

    return callsForStateChanged;
  },

  /**
   * Handle an incoming call.
   *
   * Not much is known about this call at this point, but it's enough
   * to start bringing up the Phone app already.
   */
  notifyCallRing() {
    // We need to acquire a CPU wake lock to avoid the system falling into
    // the sleep mode when the RIL handles the incoming call.
    this._acquireCallRingWakeLock();

    lazy.gTelephonyMessenger.notifyNewCall();
  },

  /**
   * Handle current calls reported from RIL.
   *
   * @param aCalls call from RIL, which contains:
   *        state, callIndex, toa, isMT, number, numberPresentation, name,
   *        namePresentation.
   */
  notifyCurrentCalls(aClientId, aCalls) {
    // Check whether there is a removed call.
    let hasRemovedCalls = () => {
      let newIndexes = new Set(Object.keys(aCalls));
      for (let i in this._currentCalls[aClientId]) {
        if (!newIndexes.has(i)) {
          return true;
        }
      }
      return false;
    };

    // If there are removedCalls, we should fetch the failCause first.
    if (!hasRemovedCalls()) {
      this._handleCurrentCalls(aClientId, aCalls);
    } else {
      // TODO need a better way for this.
      if (this._isImsClient(aClientId)) {
        this._getImsLastFailCause(aClientId, cause => {
          this._handleCurrentCalls(aClientId, aCalls, cause);
        });
        return;
      }
      this._sendToRilWorker(aClientId, "getFailCause", null, response => {
        this._handleCurrentCalls(aClientId, aCalls, response.failCause);
      });
    }
  },

  _handleCurrentCalls(
    aClientId,
    aCalls,
    aFailCause = RIL.GECKO_CALL_ERROR_NORMAL_CALL_CLEARING
  ) {
    if (DEBUG) {
      debug(
        "handleCurrentCalls: " +
          JSON.stringify(aCalls) +
          ", failCause: " +
          aFailCause
      );
    }

    let changedCalls = new Set();
    let removedCalls = new Set();

    let allIndexes = new Set([
      ...Object.keys(this._currentCalls[aClientId]),
      ...Object.keys(aCalls),
    ]);

    for (let i of allIndexes) {
      let call = this._currentCalls[aClientId][i];
      let rilCall = aCalls[i];

      // Determine the change of call.
      if (call && !rilCall) {
        // removed.
        removedCalls.add(call);
      } else if (call && rilCall) {
        // changed.
        if (this._updateCallFromRil(call, rilCall, aClientId)) {
          changedCalls.add(call);
        }
      } else {
        // !call && rilCall. added.
        this._currentCalls[aClientId][i] = call = new Call(aClientId, i);
        this._updateCallFromRil(call, rilCall, aClientId);
        changedCalls.add(call);

        // Handle ongoingDial.
        if (
          this._ongoingDial &&
          this._ongoingDial.clientId === aClientId &&
          call.state !== nsITelephonyService.CALL_STATE_INCOMING
        ) {
          this._ongoingDial.callback.notifyDialCallSuccess(
            aClientId,
            i,
            call.number,
            this._ongoingDial.isEmergency,
            this._ongoingDial.rttMode,
            call.voiceQuality,
            call.capabilities,
            call.videoCallState,
            call.radioTech
          );
          this._ongoingDial = null;
        }
      }
    }

    // This is the first poll call state after request dial.
    // We expect ongoing call to appear in call list.
    // If not, ongoing call is disappeared somehow.
    if (this._ongoingDial) {
      if (DEBUG) {
        debug(
          "_ongoingDial does not appear in call list, drop it after request getFailCause."
        );
      }
      let callback = this._ongoingDial.callback;
      this._sendToRilWorker(aClientId, "getFailCause", null, response => {
        callback.notifyError(response.failCause);
      });
      this._ongoingDial = null;
    }

    // For correct conference detection, we should mark removedCalls as
    // DISCONNECTED first.
    let disconnectedCalls = this._disconnectCalls(
      aClientId,
      [...removedCalls],
      aFailCause
    );
    disconnectedCalls.forEach(call => changedCalls.add(call));

    // Detect conference and update isConference flag.
    /*eslint no-unused-vars: ["error", { "varsIgnorePattern": "newConferenceState" }]*/
    let [_newConferenceState, conferenceChangedCalls] = this._updateConference(
      aClientId
    );
    conferenceChangedCalls.forEach(call => changedCalls.add(call));

    this._handleCallStateChanged(aClientId, [...changedCalls]);

    this._updateAudioState(aClientId);

    // Handle cached dial request.
    if (this._cachedDialRequest && !this._isActive(aClientId)) {
      if (DEBUG) {
        debug("All calls held. Perform the cached dial request.");
      }

      let request = this._cachedDialRequest;
      this._sendDialCallRequest(
        request.clientId,
        request.options,
        request.callback
      );
      this._cachedDialRequest = null;
    }
  },

  /**
   * Handle call state changes.
   */
  _handleCallStateChanged(_aClientId, aCalls) {
    if (DEBUG) {
      debug("handleCallStateChanged: " + JSON.stringify(aCalls));
    }

    if (aCalls.length === 0) {
      return;
    }

    if (
      aCalls.some(call => call.state == nsITelephonyService.CALL_STATE_DIALING)
    ) {
      lazy.gTelephonyMessenger.notifyNewCall();
    }

    let allInfo = aCalls.map(call => new TelephonyCallInfo(call));
    this._notifyAllListeners("callStateChanged", [allInfo.length, allInfo]);
  },

  notifyCdmaCallWaiting(aClientId, aCall) {
    // We need to acquire a CPU wake lock to avoid the system falling into
    // the sleep mode when the RIL handles the incoming call.
    this._acquireCallRingWakeLock();

    let call = this._currentCalls[aClientId][CDMA_SECOND_CALL_INDEX];
    if (call) {
      // TODO: Bug 977503 - B2G RIL: [CDMA] update callNumber when a waiting
      // call comes after a 3way call.
      this._removeCdmaSecondCall(aClientId);
    }

    this._cdmaCallWaitingNumber = aCall.number;

    this._notifyAllListeners("notifyCdmaCallWaiting", [
      aClientId,
      aCall.number,
      aCall.numberPresentation,
      aCall.name,
      aCall.namePresentation,
    ]);
  },

  notifySupplementaryService(
    aClientId,
    aNotificationType,
    aCode,
    aIndex,
    aType,
    aNumber
  ) {
    if (DEBUG) {
      debug(
        "notifySupplementaryService for " +
          aClientId +
          ": " +
          " (notificationType : " +
          aNotificationType +
          " code : " +
          aCode +
          " index : " +
          aIndex +
          " type : " +
          aType +
          " number : " +
          aNumber +
          ")"
      );
    }
    this._notifyAllListeners("supplementaryServiceNotification", [
      aClientId,
      aNotificationType,
      aCode,
      aIndex,
      aType,
      aNumber,
    ]);
  },

  notifySrvccState(aClientId, aState) {
    if (DEBUG) {
      debug(
        "notifySrvccState for " +
          aClientId +
          ": " +
          " (aState : " +
          aState +
          ")"
      );
    }
    this._notifyAllListeners("notifySrvccState", [aClientId, aState]);
  },

  notifyUssdReceived(aClientId, aMessage, aSessionEnded) {
    if (DEBUG) {
      debug(
        "notifyUssdReceived for " +
          aClientId +
          ": " +
          aMessage +
          " (sessionEnded : " +
          aSessionEnded +
          ")"
      );
    }

    let oldSession = this._ussdSessions[aClientId];
    this._ussdSessions[aClientId] = aSessionEnded
      ? USSD_SESSION_DONE
      : USSD_SESSION_ONGOING;

    // We suppress the empty message only when the session is not changed and
    // is not alive. See Bug 1057455, 1198676.
    // Moreover, we should allow a notification initiated by network
    // in which further response is not required.
    // See |5.2.2 Actions at the UE| in 3GPP TS 22.090:
    // "
    //  The network may explicitly indicate to the UE that a response
    //  from the user is required. ...
    //  If the network does not indicate that a response is required,
    //  then the normal MMI procedures on the UE continue to apply.
    // "
    if (
      oldSession != USSD_SESSION_ONGOING &&
      this._ussdSessions[aClientId] != USSD_SESSION_ONGOING &&
      !aMessage
    ) {
      return;
    }

    lazy.gTelephonyMessenger.notifyUssdReceived(
      aClientId,
      aMessage,
      aSessionEnded
    );
  },

  /**
   * Handle ringback tone changes from RIL.
   */
  notifyRingbackTone(aClientId, aPlayRingbackTone) {
    this._notifyAllListeners("notifyRingbackTone", [aPlayRingbackTone]);
  },

  /**
   * nsITelephonyAudioListener interface.
   */
  _headsetState: nsITelephonyAudioService.HEADSET_STATE_OFF,
  notifyHeadsetStateChanged(aState) {
    this._headsetState = aState;
    this._applyTtyMode();
  },

  /**
   * Handle TTY mode received event from stack.
   *
   */
  notifyTtyModeReceived(aClientId, aMode) {
    this._notifyAllListeners("notifyTtyModeReceived", [aMode]);
  },

  /**
   * Handle service coverage losing event from stack.
   */
  notifyTelephonyCoverageLosing(aClientId, aType) {
    this._notifyAllListeners("notifyTelephonyCoverageLosing", [aType]);
  },

  /**
   * Handle Rtt modify request recieved from RIL
   */
  notifyRttModifyRequestReceived(aClientId, aCallIndex, aRttMode) {
    this._notifyAllListeners("notifyRttModifyRequestReceived", [
      aClientId,
      aCallIndex,
      aRttMode,
    ]);
  },

  /**
   * Handle Rtt modify reponse received from RIL
   */
  notifyRttModifyResponseReceived(aClientId, aCallIndex, aStatus) {
    this._notifyAllListeners("notifyRttModifyResponseReceived", [
      aClientId,
      aCallIndex,
      aStatus,
    ]);
  },

  /**
   * Handle Rtt message received
   */
  notifyRttMessageReceived(aClientId, aCallIndex, aMessage) {
    this._notifyAllListeners("notifyRttMessageReceived", [
      aClientId,
      aCallIndex,
      aMessage,
    ]);
  },

  /**
   * nsIObserver interface.
   */

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          this._updateDebugFlag();
        } else if (aData === kPrefDefaultServiceId) {
          this.defaultServiceId = this._getDefaultServiceId();
        } else if (aData === kPrefTwoDigitShortCodes) {
          this._updateTwoDigitShortCodes();
        } else if (aData == kPrefAlwaysTryImsForEcc) {
          this._updateAlwaysTryImsForEcc();
        }
        break;

      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        // Release the CPU wake lock for handling the incoming call.
        this._releaseCallRingWakeLock();
        lazy.gAudioService.unregisterListener(this);
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
        Services.prefs.removeObserver(kPrefDefaultServiceId, this);
        Services.prefs.removeObserver(kPrefTwoDigitShortCodes, this);

        for (let i = 0; i < this._mobileConnListeners.length; i++) {
          let connection = lazy.gGonkMobileConnectionService.getItemByServiceId(
            i
          );
          connection.unregisterListener(this._mobileConnListeners[i]);
        }
        this._mobileConnListeners = [];
        break;
    }
  },

  // IMS part
  _isImsClient(aClientId) {
    if (DEBUG) {
      debug("_isIms: " + aClientId);
    }
    let imsHandler = lazy.gImsRegService.getHandlerByServiceId(aClientId);
    if (!imsHandler) {
      if (DEBUG) {
        debug("_isIms no imsHandler ");
      }
      return false;
    }

    if (DEBUG) {
      debug("_isIms enabled: " + imsHandler.enabled);
      debug("_isIms capability: " + imsHandler.capability);
    }

    let isIms =
      imsHandler.enabled &&
      imsHandler.capability > Ci.nsIImsRegHandler.IMS_CAPABILITY_UNKNOWN;

    if (DEBUG) {
      debug("_isIms call ims: " + isIms);
    }
    return isIms;
  },

  _dialImsCall(aClientId, aOptions, aCallback) {
    let imsPhone = this._imsPhones[aClientId];
    let dialRequest = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIImsPhoneDialRequest]),
      serviceId: aClientId,
      number: aOptions.number,
      isEmergency: aOptions.isEmergency,
      type: aOptions.type,
      rttMode: aOptions.rttMode,
      clirMode: aOptions.clirMode,
    };

    let self = this;
    let imsCallback = this._createImsCallback(
      () => {
        self._isDialing = false;
        if (DEBUG) {
          lazy.console.log("_dialImsCall success");
        }
        self._ongoingDial = {
          clientId: aClientId,
          isEmergency: aOptions.isEmergency,
          rttMode: aOptions.rttMode,
          clirMode: aOptions.clirMode,
          callback: aCallback,
        };
      },
      aError => {
        self._isDialing = false;
        if (DEBUG) {
          lazy.console.log("_dialImsCall error: " + aError);
        }
        aCallback.notifyError(aError);
      }
    );

    imsPhone.dial(dialRequest, imsCallback);
  },

  _initImsPhones() {
    this._imsPhones = [];
    for (let index = 0; index < this._numClients; index++) {
      if (!IMS_ENABLED) {
        return;
      }
      let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(index);
      this._imsPhones.push(imsPhone);
    }
  },

  _answerImsPhone(aClientId, aCallIndex, aType, aRttMode, aCallback) {
    if (DEBUG) {
      lazy.console.log("_answerImsPhone: " + aCallIndex);
    }
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.answerCall(
      aCallIndex,
      aType,
      aRttMode,
      this._createSimpleImsCallback(aCallback)
    );
  },

  _rejectImsCall(aClientId, aCallIndex, aReason, aCallback) {
    if (DEBUG) {
      lazy.console.log("_rejectImsCall: " + aCallIndex);
    }
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    if (DEBUG) {
      lazy.console.log("_rejectImsCall with phone: " + imsPhone);
    }
    imsPhone.rejectCall(
      aCallIndex,
      aReason,
      this._createSimpleImsCallback(aCallback)
    );
  },

  _hangupImsCall(aClientId, aCallIndex, aReason, aCallback) {
    if (DEBUG) {
      lazy.console.log("_hangupImsCall: " + aCallIndex);
    }
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.hangupCall(
      aCallIndex,
      aReason,
      this._createSimpleImsCallback(aCallback)
    );
  },

  _getImsLastFailCause(aClientId, aCallback) {
    let callback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIImsGetFailCauseCallback]),
      notifyFailCause(aCause) {
        aCallback(aCause);
      },
    };
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.getFailCause(callback);
  },

  _sendImsTone(aClientId, aDtmfChar, aCallback) {
    if (DEBUG) {
      debug("_sendImsTone");
    }

    let callback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIImsPhoneCallback]),
      notifySuccess() {
        if (DEBUG) {
          debug("sendImsTone notifySuccess");
        }
        aCallback.notifySuccess();
      },

      notifyError(aError) {
        debug("sendImsTone notifyError: " + aError);
        aCallback.notifyError(aError);
      },
    };
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.sendDtmf(aDtmfChar, callback);
  },

  _startImsTone(aClientId, aDtmfChar) {
    if (DEBUG) {
      debug("_startTone");
    }

    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.startDtmf(aDtmfChar);
  },

  _stopImsTone(aClientId) {
    if (DEBUG) {
      debug("_stopImsTone");
    }
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.stopDtmf();
  },

  _createSimpleImsCallback(aCallback) {
    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIImsPhoneCallback]),
      notifySuccess() {
        aCallback.notifySuccess();
      },

      notifyError(aError) {
        aCallback.notifyError(aError);
      },
    };
  },

  _createImsCallback(aSuccessCb, aErrorCb) {
    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIImsPhoneCallback]),
      notifySuccess() {
        aSuccessCb();
      },

      notifyError(aError) {
        aErrorCb(aError);
      },
    };
  },

  _hangUpImsForeground(aClientId, aCallback) {
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.hangupForeground(this._createSimpleImsCallback(aCallback));
  },

  _hangUpImsBackground(aClientId, aCallback) {
    let imsPhone = lazy.gImsPhoneService.getPhoneByServiceId(aClientId);
    imsPhone.hangupBackground(this._createSimpleImsCallback(aCallback));
  },

  _updateTwoDigitShortCodes() {
    let twoDigitShortCode = Services.prefs.getCharPref(
      kPrefTwoDigitShortCodes,
      ""
    );
    if (twoDigitShortCode) {
      this._twoDigitShortCodes = twoDigitShortCode.split(",");
    }
  },

  _isTwoDigitShortCode(aNumber) {
    return this._twoDigitShortCodes.includes(aNumber);
  },

  _useImsForEmergency(aIsEmergency, aClientId) {
    if (!aIsEmergency) {
      return false;
    }

    // This is a workaround for CS + PS call issue.
    // We are not capable to handle CS + PS calls in same slot imultaneously.
    // TBD. to create CS, PS call trackers spearately and a call manager to
    // manage call trackers.
    if (
      this._numCallsOnLine(aClientId, Ci.nsITelephonyCallInfo.RADIO_TECH_CS)
    ) {
      if (DEBUG) {
        debug("don't use IMS for ECC due to CS call is ongoing");
      }
      return false;
    }

    return ALWAYS_TRY_IMS_FOR_EMERGENCY && lazy.gImsRegService.isServiceReady();
  },
};

var EXPORTED_SYMBOLS = ["TelephonyService"];
