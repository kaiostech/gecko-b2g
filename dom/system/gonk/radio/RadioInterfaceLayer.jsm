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

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

const { libcutils } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);

ChromeUtils.import("resource://gre/modules/systemlibs.js");

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "SIM", function() {
  return ChromeUtils.import("resource://gre/modules/simIOHelper.js");
});

XPCOMUtils.defineLazyGetter(lazy, "RIL", function() {
  return ChromeUtils.import("resource://gre/modules/ril_consts.js");
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

var TelephonyRequestQueue = ChromeUtils.import(
  "resource://gre/modules/TelephonyRequestQueue.jsm"
);

const { setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

// Ril quirk to always turn the radio off for the client without SIM card
// except hw default client.
var RILQUIRKS_RADIO_OFF_WO_CARD =
  libcutils.property_get("ro.moz.ril.radio_off_wo_card", "false") == "true";

const RADIOINTERFACELAYER_CID = Components.ID(
  "{2d831c8d-6017-435b-a80c-e5d422810cea}"
);
const RADIOINTERFACE_CID = Components.ID(
  "{6a7c91f0-a2b3-4193-8562-8969296c0b54}"
);

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefAppCBConfigurationEnabled = "dom.app_cb_configuration";

const PROP_MULTISIM_CONFIG = "persist.radio.multisim.config";

// Timeout value for emergency callback mode.
const EMERGENCY_CB_MODE_TIMEOUT_MS = 5 * 60 * 1000;
const RADIO_POWER_OFF_TIMEOUT = 30000;
const HW_DEFAULT_CLIENT_ID = 0;

// const INVALID_UPTIME = undefined;

const ICC_MAX_LINEAR_FIXED_RECORDS = 0xfe;

// set to true in ril_consts_debug.js to see debug messages
var DEBUG = RIL_DEBUG.DEBUG_RIL;

function updateDebugFlag() {
  // Read debug setting from pref
  DEBUG =
    RIL_DEBUG.DEBUG_RIL ||
    Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, false);
}
updateDebugFlag();

function debug(s) {
  dump("-*- RadioInterfaceLayer: " + s + "\n");
}

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/gonkiccservice;1",
  "nsIGonkIccService"
);

// XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
//                                    "@mozilla.org/mobilemessage/mobilemessageservice;1",
//                                    "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSmsService",
  "@mozilla.org/sms/gonksmsservice;1",
  "nsIGonkSmsService"
);

// XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
//                                    "@mozilla.org/parentprocessmessagemanager;1",
//                                    "nsIMessageBroadcaster");

// XPCOMUtils.defineLazyServiceGetter(this, "gSystemWorkerManager",
//                                    "@mozilla.org/telephony/system-worker-manager;1",
//                                    "nsISystemWorkerManager");

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gTelephonyService",
  "@mozilla.org/telephony/telephonyservice;1",
  "nsIGonkTelephonyService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIGonkMobileConnectionService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gCellBroadcastService",
  "@mozilla.org/cellbroadcast/cellbroadcastservice;1",
  "nsIGonkCellBroadcastService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gStkCmdFactory",
  "@mozilla.org/icc/stkcmdfactory;1",
  "nsIStkCmdFactory"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDataCallManager",
  "@mozilla.org/datacall/manager;1",
  "nsIDataCallManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDataCallInterfaceService",
  "@mozilla.org/datacall/interfaceservice;1",
  "nsIGonkDataCallInterfaceService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRilWorkerService",
  "@mozilla.org/rilworkerservice;1",
  "nsIRilWorkerService"
);

if (Ci.nsIOemHook) {
  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "gOemHookService",
    "@mozilla.org/b2g/oemhookservice;1",
    "nsIOemHookService"
  );
}

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkTimeService",
  "@b2g/time/networktimeservice;1",
  "nsINetworkTimeService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gVoicemailService",
  "@mozilla.org/voicemail/gonkvoicemailservice;1",
  "nsIGonkVoicemailService"
);

XPCOMUtils.defineLazyGetter(lazy, "gRadioEnabledController", function() {
  let _ril = null;
  let _pendingMessages = []; // For queueing "setRadioEnabled" message.
  let _isProcessingPending = false;
  let _timer = null;
  let _request = null;
  let _deactivatingDeferred = {};
  let _initializedCardState = {};
  let _allCardStateInitialized = !RILQUIRKS_RADIO_OFF_WO_CARD;

  return {
    init(ril) {
      _ril = ril;
    },

    receiveCardState(clientId) {
      if (_allCardStateInitialized) {
        return;
      }

      if (DEBUG) {
        debug("RadioControl: receive cardState from " + clientId);
      }
      _initializedCardState[clientId] = true;
      if (
        Object.keys(_initializedCardState).length == _ril.numRadioInterfaces
      ) {
        _allCardStateInitialized = true;
        this._startProcessingPending();
      }
    },

    setRadioEnabled(clientId, data, callback) {
      if (DEBUG) {
        debug("setRadioEnabled: " + clientId + ": " + JSON.stringify(data));
      }
      let message = {
        clientId,
        data,
        callback,
      };
      _pendingMessages.push(message);
      this._startProcessingPending();
    },

    notifyRadioStateChanged(clientId, radioState) {
      lazy.gMobileConnectionService.notifyRadioStateChanged(
        clientId,
        radioState
      );
    },

    _startProcessingPending() {
      if (!_isProcessingPending) {
        if (DEBUG) {
          debug("RadioControl: start dequeue");
        }
        _isProcessingPending = true;
        this._processNextMessage();
      }
    },

    _processNextMessage() {
      if (_pendingMessages.length === 0 || !_allCardStateInitialized) {
        if (DEBUG) {
          debug("RadioControl: stop dequeue");
        }
        _isProcessingPending = false;
        return;
      }

      let msg = _pendingMessages.shift();
      this._handleMessage(msg);
    },

    _getNumCards() {
      let numCards = 0;
      for (let i = 0, N = _ril.numRadioInterfaces; i < N; ++i) {
        if (_ril.getRadioInterface(i).isCardPresent()) {
          numCards++;
        }
      }
      return numCards;
    },

    _isRadioAbleToEnableAtClient(clientId, numCards) {
      if (!RILQUIRKS_RADIO_OFF_WO_CARD) {
        return true;
      }

      // We could only turn on the radio for clientId if
      // 1. a SIM card is presented or
      // 2. it is the default clientId and there is no any SIM card at any client.

      if (_ril.getRadioInterface(clientId).isCardPresent()) {
        return true;
      }

      numCards = numCards == null ? this._getNumCards() : numCards;
      if (clientId === HW_DEFAULT_CLIENT_ID && numCards === 0) {
        return true;
      }

      return false;
    },

    _handleMessage(message) {
      if (DEBUG) {
        debug("RadioControl: handleMessage: " + JSON.stringify(message));
      }
      let clientId = message.clientId || 0;

      if (message.data.enabled) {
        if (this._isRadioAbleToEnableAtClient(clientId)) {
          this._setRadioEnabledInternal(message);
        } else {
          // Not really do it but respond success.
          message.callback(message.data);
        }

        this._processNextMessage();
      } else {
        _request = this._setRadioEnabledInternal.bind(this, message);

        // In 2G network, modem takes 35+ seconds to process deactivate data
        // call request if device has active voice call (please see bug 964974
        // for more details). Therefore we should hangup all active voice calls
        // first. And considering some DSDS architecture, toggling one radio may
        // toggle both, so we send hangUpAll to all clients.
        let hangUpCallback = {
          QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyCallback]),
          notifySuccess() {},
          notifyError() {},
        };

        lazy.gTelephonyService.enumerateCalls({
          QueryInterface: ChromeUtils.generateQI([Ci.nsITelephonyListener]),
          enumerateCallState(aInfo) {
            lazy.gTelephonyService.hangUpCall(
              aInfo.clientId,
              aInfo.callIndex,
              hangUpCallback
            );
          },
          enumerateCallStateComplete() {},
        });

        // In some DSDS architecture with only one modem, toggling one radio may
        // toggle both. Therefore, for safely turning off, we should first
        // explicitly deactivate all data calls from all clients.
        this._deactivateDataCalls().then(() => {
          if (DEBUG) {
            debug("RadioControl: deactivation done");
          }
          this._executeRequest();
        });

        this._createTimer();
      }
    },

    _setRadioEnabledInternal(message) {
      let clientId = message.clientId || 0;
      let radioInterface = _ril.getRadioInterface(clientId);

      radioInterface.sendRilRequest(
        "setRadioEnabled",
        message.data,
        function(response) {
          if (response.errorMsg) {
            // If request fails, set current radio state to unknown, since we will
            // handle it in |mobileConnectionService|.
            this.notifyRadioStateChanged(
              clientId,
              Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN
            );
          }
          debug("_setRadioEnabledInternal callback.");
          return message.callback(response);
        }.bind(this)
      );
    },

    _deactivateDataCalls() {
      if (DEBUG) {
        debug("RadioControl: deactivating data calls...");
      }
      _deactivatingDeferred = {};

      let promise = Promise.resolve();
      for (let i = 0, N = _ril.numRadioInterfaces; i < N; ++i) {
        promise = promise.then(this._deactivateDataCallsForClient(i));
      }

      return promise;
    },

    _deactivateDataCallsForClient(clientId) {
      return function() {
        let deferred = (_deactivatingDeferred[clientId] = PromiseUtils.defer());
        let dataCallHandler = lazy.gDataCallManager.getDataCallHandler(
          clientId
        );

        dataCallHandler.deactivateAllDataCalls(
          lazy.RIL.DATACALL_DEACTIVATE_RADIO_SHUTDOWN,
          function() {
            deferred.resolve();
          }
        );

        return deferred.promise;
      };
    },

    _createTimer() {
      if (!_timer) {
        _timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      }
      _timer.initWithCallback(
        this._executeRequest.bind(this),
        RADIO_POWER_OFF_TIMEOUT,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    },

    _cancelTimer() {
      if (_timer) {
        _timer.cancel();
      }
    },

    _executeRequest() {
      if (typeof _request === "function") {
        if (DEBUG) {
          debug("RadioControl: executeRequest");
        }
        this._cancelTimer();
        _request();
        _request = null;
      }
      this._processNextMessage();
    },
  };
});

// Initialize shared preference "ril.numRadioInterfaces" according to system
// property.
try {
  Services.prefs.setIntPref(
    kPrefRilNumRadioInterfaces,
    (function() {
      try {
        let configuration = libcutils.property_get(PROP_MULTISIM_CONFIG, "");
        switch (configuration) {
          case "dsds":
          // falls through
          case "dsda":
            return 2;
          case "tsts":
            return 3;
          default:
            return 1;
        }
      } catch (e) {}

      return 1;
    })()
  );
} catch (e) {}

function DataCall(aAttributes) {
  for (let key in aAttributes) {
    if (key === "pdpType") {
      // Convert pdp type into constant int value.
      this[key] = lazy.RIL.RIL_DATACALL_PDP_TYPES.indexOf(aAttributes[key]);
      continue;
    }

    this[key] = aAttributes[key];
  }
}
DataCall.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDataCall]),

  failCause: Ci.nsIDataCallInterface.DATACALL_FAIL_NONE,
  suggestedRetryTime: -1,
  cid: -1,
  active: -1,
  pdpType: -1,
  ifname: null,
  addreses: null,
  dnses: null,
  gateways: null,
  pcscf: null,
  mtu: -1,
};

function DataProfile(aAttributes) {
  for (let key in aAttributes) {
    if (key === "carrier_enabled") {
      this.enabled = aAttributes[key];
      break;
    }
    this[key] = aAttributes[key];
  }
}
DataProfile.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDataProfile]),

  profileId: -1,
  apn: null,
  protocol: null,
  authType: -1,
  user: null,
  password: null,
  type: -1,
  maxConnsTime: -1,
  maxConns: -1,
  waitTime: -1,
  enabled: false,
  supportedApnTypesBitmap: -1,
  roamingProtocol: null,
  bearerBitmap: -1,
  mtu: -1,
  mvnoType: null,
  mvnoMatchData: null,
  modemCognitive: false,
};

function RadioInterfaceLayer() {
  let numIfaces = this.numRadioInterfaces;
  if (DEBUG) {
    debug(numIfaces + " interfaces");
  }

  this.radioInterfaces = [];
  for (let clientId = 0; clientId < numIfaces; clientId++) {
    this.radioInterfaces.push(new RadioInterface(clientId));
  }

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);

  lazy.gRadioEnabledController.init(this);
}
RadioInterfaceLayer.prototype = {
  classID: RADIOINTERFACELAYER_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIRadioInterfaceLayer,
    Ci.nsIObserver,
  ]),

  /**
   * nsIObserver interface methods.
   */

  observe(subject, topic, data) {
    switch (topic) {
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        for (let radioInterface of this.radioInterfaces) {
          radioInterface.shutdown();
        }
        this.radioInterfaces = null;
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;

      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (data === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
    }
  },

  /**
   * nsIRadioInterfaceLayer interface methods.
   */

  getRadioInterface(clientId) {
    return this.radioInterfaces[clientId];
  },

  setMicrophoneMuted(muted) {
    for (let clientId = 0; clientId < this.numRadioInterfaces; clientId++) {
      let radioInterface = this.radioInterfaces[clientId];
      radioInterface.sendRilRequest("setMute", { muted });
    }
  },
};

XPCOMUtils.defineLazyGetter(
  RadioInterfaceLayer.prototype,
  "numRadioInterfaces",
  function() {
    try {
      //Cameron config the default value to 1.
      return Services.prefs.getIntPref(kPrefRilNumRadioInterfaces, 1);
    } catch (e) {}

    return 1;
  }
);

function RadioInterface(aClientId) {
  this.token = 1;
  this.clientId = aClientId;
  this.radioState = Ci.nsIRilIndicationResult.RADIOSTATE_UNKNOWN;
  this.radioTech = Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_UNKNOWN;
  this._waitingRadioTech = false;
  this._attachDataRegistration = {};
  this.pendingToReportSmsMemoryStatus = false;
  this._isCdma = false;
  this.deviceIdentities = null;
  this.basebandVersion = null;
  this._processingNetworkInfo = false;
  this._needRepollNetworkInfo = false;
  this._pendingNetworkInfo = { rilMessageType: "networkinfochanged" };
  this.pendingNetworkType = null;
  this.cardState = lazy.RIL.GECKO_CARDSTATE_UNINITIALIZED;
  this.pin2CardState = lazy.RIL.GECKO_CARDSTATE_UNINITIALIZED;
  this.operator = {
    rilMessageType: "operatorchange",
    longName: null,
    shortName: null,
  };
  this._initIccInfo();
  this.networkSelectionMode = lazy.RIL.GECKO_NETWORK_SELECTION_UNKNOWN;
  this.voiceRegistrationState = {};
  this.dataRegistrationState = {};
  this.aid = null;
  this.appType = null;

  this.cellBroadcastDisabled = false;

  this._receivedSmsCbPagesMap = null;

  this.oemHook = null;

  /**
   * Cell Broadcast Search Lists.
   */
  let cbmmi = this.cellBroadcastConfigs && this.cellBroadcastConfigs.MMI;
  this.cellBroadcastConfigs = {
    MMI: cbmmi || null,
  };
  this.mergedCellBroadcastConfig = null;

  if (lazy.gRilWorkerService) {
    this.rilworker = lazy.gRilWorkerService.getRilWorker(this.clientId);
    if (this.rilworker) {
      this.rilworker.initRil(this);
    }
  }

  // For sim io context.
  this.simIOcontext = new lazy.SIM.Context(this);

  this.tokenCallbackMap = {};
  this.telephonyRequestQueue = new TelephonyRequestQueue.TelephonyRequestQueue(
    this
  );
  // For IccIO
  this.tokenOptionsMap = {};

  /**
   * CallForward options
   */
  this._callForwardOptions = {};

  /**
   * Radio capabilities.
   */
  this._radioCapability = {};

  this._pendingSentSmsMap = {};
  this._pendingSmsRequest = {};

  this._isInEmergencyCbMode = false;

  this._exitEmergencyCbModeTimer = Cc["@mozilla.org/timer;1"].createInstance(
    Ci.nsITimer
  );
}

// OemHook.
let radioInterface_queryInterfaces = [Ci.nsIRadioInterface, Ci.nsIRilCallback];
if (Ci.nsIOemHookCallback) {
  radioInterface_queryInterfaces.push(Ci.nsIOemHookCallback);
}

RadioInterface.prototype = {
  classID: RADIOINTERFACE_CID,
  QueryInterface: ChromeUtils.generateQI(radioInterface_queryInterfaces),

  rilworker: null,

  oemHook: null,

  // This gets incremented each time we send out a message.
  token: 1,

  // Maps tokens we send out with messages to the message callback.
  tokenCallbackMap: null,

  // Telephony request control queue.
  telephonyRequestQueue: null,

  // Maps tokens we stroe the command type for mapping the handler function.
  tokenOptionsMap: null,

  /**
   * One of the RADIO_STATE_* constants.
   */
  radioState: null,

  /**
   * Set when radio is ready but radio tech is unknown. That is, we are
   * waiting for REQUEST_VOICE_RADIO_TECH
   */

  radioTech: null,

  _waitingRadioTech: false,

  _attachDataRegistration: null,

  /**
   * True when the request to report SMS Memory Status is pending.
   */
  pendingToReportSmsMemoryStatus: false,

  /**
   * True if we are on a CDMA phone.
   */
  _isCdma: false,

  /**
   * Device Identities including IMEI, IMEISV, ESN and MEID.
   */
  deviceIdentities: null,

  /**
   * String containing the baseband version.
   */
  basebandVersion: null,

  /**
   * Whether or not the multiple requests in requestNetworkInfo() are currently
   * being processed
   */
  _processingNetworkInfo: false,

  /**
   * Multiple requestNetworkInfo() in a row before finishing the first
   * request, hence we need to fire requestNetworkInfo() again after
   * gathering all necessary stuffs. This is to make sure that ril_worker
   * gets precise network information.
   */
  _needRepollNetworkInfo: false,

  /**
   * Pending messages to be send in batch from requestNetworkInfo()
   */
  _pendingNetworkInfo: {},

  /**
   * Marker object.
   */
  pendingNetworkType: null,

  /**
   * Card state
   */
  cardState: null,

  /**
   * Pin2 Card state
   */
  pin2CardState: null,

  /**
   * List of strings identifying the network operator.
   */
  operator: null,

  // Icc card
  iccInfoPrivate: null,

  iccInfo: null,

  /**
   * Application identification for apps in ICC.
   */
  aid: null,

  /**
   * Application type for apps in ICC.
   */
  appType: null,

  networkSelectionMode: null,

  voiceRegistrationState: null,
  dataRegistrationState: null,

  // For SIM IO
  simIOcontext: null,

  /**
   * Global Cell Broadcast switch.
   */
  cellBroadcastDisabled: false,

  /**
   * Parsed Cell Broadcast search lists.
   * cellBroadcastConfigs.MMI should be preserved over rild reset.
   */
  cellBroadcastConfigs: null,
  mergedCellBroadcastConfig: null,

  _receivedSmsCbPagesMap: null,

  /**
   * CallForward options
   */
  _callForwardOptions: null,

  /**
   * Radio capability.
   */
  _radioCapability: null,

  /**
   * Outgoing messages waiting for SMS-STATUS-REPORT.
   */
  _pendingSentSmsMap: null,

  /**
   * Pending send message request.
   */
  _pendingSmsRequest: null,

  /**
   * True if we are in emergency callback mode.
   */
  _isInEmergencyCbMode: null,

  /**
   * ECM timeout id.
   */
  _exitEmergencyCbModeTimer: null,

  debug(s) {
    dump("-*- RadioInterface[" + this.clientId + "]: " + s + "\n");
  },

  _initIccInfo() {
    /**
     * ICC information that is not exposed to Gaia.
     */
    this.iccInfoPrivate = {};

    /**
     * ICC information, such as MSISDN, MCC, MNC, SPN...etc.
     */
    let iccInfoReseted = false;
    if (this.iccInfo && Object.keys(this.iccInfo).length > 0) {
      iccInfoReseted = true;
    }

    this.iccInfo = {};
    if (iccInfoReseted) {
      this.simIOcontext.ICCUtilsHelper.handleICCInfoChange();
    }
  },

  isCardPresent() {
    let icc = lazy.gIccService.getIccByServiceId(this.clientId);
    let cardState = icc ? icc.cardState : Ci.nsIIcc.CARD_STATE_UNKNOWN;
    return (
      cardState !== Ci.nsIIcc.CARD_STATE_UNDETECTED &&
      cardState !== Ci.nsIIcc.CARD_STATE_UNKNOWN
    );
  },

  /* eslint-disable complexity */
  handleUnsolicitedMessage(message) {
    if (DEBUG) {
      this.debug("message.rilMessageType:" + message.rilMessageType);
    }

    switch (message.rilMessageType) {
      case "rilconnected":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RIL_CONNECTED");
        }
        this.handleRilConnected(message);
        break;
      case "callRing":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_CALL_RING");
        }
        lazy.gTelephonyService.notifyCallRing();
        break;
      case "callStateChanged":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED");
        }
        this.handleCallStateChanged();
        break;
      case "cdmaCallWaiting":
        lazy.gTelephonyService.notifyCdmaCallWaiting(
          this.clientId,
          message.waitingCall
        );
        break;
      case "suppSvcNotification":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_SUPP_SVC_NOTIFICATION");
        }
        lazy.gTelephonyService.notifySupplementaryService(
          this.clientId,
          message.suppSvc.notificationType,
          message.suppSvc.code,
          message.suppSvc.index,
          message.suppSvc.type,
          message.suppSvc.number
        );
        break;
      case "srvccStateNotify":
        let srvccState = message.srvccState;
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_SRVCC_STATE_NOTIFY srvccState = " +
              JSON.stringify(srvccState)
          );
        }
        lazy.gTelephonyService.notifySrvccState(this.clientId, srvccState);
        break;
      case "ussdreceived":
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_ON_USSD typeCode = " +
              message.typeCode +
              " , message = " +
              message.message
          );
        }
        // Per ril.h the USSD session is assumed to persist if
        // the type code is "1", otherwise the current session
        // (if any) is assumed to have terminated.
        let sessionEnded = message.typeCode !== 1;
        lazy.gTelephonyService.notifyUssdReceived(
          this.clientId,
          message.message,
          sessionEnded
        );
        break;
      case "datacalllistchanged":
        let dataCalls = message
          .getDataCallLists()
          .map(dataCall => new DataCall(dataCall));
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_DATA_CALL_LIST_CHANGED :" +
              JSON.stringify(dataCalls)
          );
        }

        lazy.gDataCallInterfaceService.notifyDataCallListChanged(
          this.clientId,
          dataCalls.length,
          dataCalls
        );
        break;
      case "ringbackTone":
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_RINGBACK_TONE : playRingbackTone = " +
              message.playRingbackTone
          );
        }
        lazy.gTelephonyService.notifyRingbackTone(
          this.clientId,
          message.playRingbackTone
        );
        break;
      //====== For networkinfochange not a unsol command=====
      case "networkinfochanged":
        lazy.gMobileConnectionService.notifyNetworkInfoChanged(
          this.clientId,
          message
        );
        break;
      case "networkselectionmodechange":
        lazy.gMobileConnectionService.notifyNetworkSelectModeChanged(
          this.clientId,
          message.mode
        );
        break;
      case "voiceregistrationstatechange":
        lazy.gMobileConnectionService.notifyVoiceInfoChanged(
          this.clientId,
          message
        );
        break;
      case "dataregistrationstatechange":
        lazy.gMobileConnectionService.notifyDataInfoChanged(
          this.clientId,
          message
        );
        break;
      case "operatorchange":
        lazy.gMobileConnectionService.notifyOperatorChanged(
          this.clientId,
          message
        );
        break;
      //==================================
      case "signalstrengthchange":
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_SIGNAL_STRENGTH,  signalStrength = " +
              JSON.stringify(message.signalStrength)
          );
        }
        this.handleSignalStrength(message);
        break;
      case "otastatuschange":
        lazy.gMobileConnectionService.notifyOtaStatusChanged(
          this.clientId,
          message.status
        );
        break;
      case "radiostatechange":
        // gRadioEnabledController should know the radio state for each client,
        // so notify gRadioEnabledController here.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, radioStateChanged: " +
              message.radioState
          );
        }
        this.handleRadioStateChange(message.radioState);
        lazy.gRadioEnabledController.notifyRadioStateChanged(
          this.clientId,
          message.radioState
        );
        // After the modem assert, the data calls should be deactivated.
        if (
          message.radioState != Ci.nsIRilIndicationResult.RADIOSTATE_ENABLED
        ) {
          lazy.gRadioEnabledController._deactivateDataCalls();
        }
        break;
      case "sms-received":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_NEW_SMS");
        }
        this.handleSmsReceived(message);
        break;
      case "smsOnSim":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM");
        }
        this.handleSmsOnSim(message);
        break;
      case "smsstatusreport":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_SMS_STATUS_REPORT");
        }
        this.handleSmsStatusReport(message);
        break;
      case "cellbroadcast-received":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_NEW_CBS");
        }
        this.handleCellbroadcastMessageReceived(message);
        break;
      case "nitzTimeReceived":
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_NITZ_TIME_RECEIVED dateString = " +
              message.dateString +
              " ,receiveTimeInMS = " +
              message.receiveTimeInMS
          );
        }
        this.handleNitzTime(message);
        break;
      //==== For icc info not a Unsol command ===
      case "iccinfochange":
        if (DEBUG) {
          this.debug("iccinfochange message=" + JSON.stringify(message));
        }
        lazy.gIccService.notifyIccInfoChanged(
          this.clientId,
          message.iccid ? message : null
        );
        break;
      case "iccmbdn":
        if (DEBUG) {
          this.debug("iccmbdn message=" + JSON.stringify(message));
        }
        this.handleIccMbdn(message);
        break;
      case "iccmwis":
        if (DEBUG) {
          this.debug("iccmwis message=" + JSON.stringify(message));
        }
        this.handleIccMwis(message.mwi);
        break;
      case "isiminfochange":
        if (DEBUG) {
          this.debug("isiminfochange message=" + JSON.stringify(message));
        }
        lazy.gIccService.notifyIsimInfoChanged(
          this.clientId,
          message.impi ? message : null
        );
        break;
      //================================================
      case "stkcommand":
        lazy.gIccService.notifyStkCommand(
          this.clientId,
          lazy.gStkCmdFactory.createCommand(message)
        );
        break;
      case "stksessionend":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_STK_SESSION_END");
        }
        lazy.gIccService.notifyStkSessionEnd(this.clientId);
        break;
      case "stkProactiveCommand":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_STK_PROACTIVE_COMMAND");
        }
        this.handleStkProactiveCommand(message);
        break;
      case "stkEventNotify":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_STK_EVENT_NOTIFY");
        }
        this.handleStkProactiveCommand(message);
        break;
      case "cdma-info-rec-received":
        this.handleCdmaInformationRecords(message.records);
        break;
      // Cameron TODO complete the modem reset.
      case "modemReset":
        let reason = message.reason;
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_MODEM_RESTART reason = " +
              JSON.stringify(reason)
          );
        }

        lazy.gMobileConnectionService.notifyModemRestart(
          this.clientId,
          message.reason
        );
        break;
      case "networkStateChanged":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED");
        }
        this.handleNetworkStateChanged();
        break;
      case "voiceRadioTechChanged":
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_VOICE_RADIO_TECH_CHANGED radioTech = " +
              message.radioTech
          );
        }
        this.handleVoiceRadioTechnology(message);
        break;
      case "simSmsStorageFull":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_SIM_SMS_STORAGE_FULL");
        }
        break;
      case "simStatusChanged":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED");
        }
        this.sendRilRequest("getICCStatus", null);
        break;
      case "resendIncallMute":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_RESEND_INCALL_MUTE");
        }
        break;
      case "cellInfoList":
        let cellInfoLists = message.getCellInfo();
        // Gecko do not handle this UNSL command.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_CELL_INFO_LIST cellInfoLists = " +
              JSON.stringify(cellInfoLists)
          );
        }
        break;
      case "simRefresh":
        let refreshResult = message.refreshResult;
        // Gecko do not handle this UNSL command.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_SIM_REFRESH refreshResult = " +
              JSON.stringify(refreshResult)
          );
        }
        this.handleSimRefresh(refreshResult);
        break;
      case "restrictedStateChanged":
        let restrictedState = message.restrictedState;
        // Gecko do not handle this UNSL command.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_RESTRICTED_STATE_CHANGED restrictedState = " +
              JSON.stringify(restrictedState)
          );
        }
        break;
      case "enterEmergencyCbMode":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE");
        }
        this.handleChangedEmergencyCbMode(true);
        break;
      case "exitEmergencyCbMode":
        if (DEBUG) {
          this.debug("RILJ: [UNSL]< RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE");
        }
        this.handleChangedEmergencyCbMode(false);
        break;
      case "subscriptionStatusChanged":
        let activate = message.activate;
        // Gecko do not handle this UNSL command.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED activate = " +
              JSON.stringify(activate)
          );
        }
        break;
      case "hardwareConfigChanged":
        // Gecko do not handle this UNSL command.
        let HWConfigs = message.getHardwardConfig();
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_HARDWARE_CONFIG_CHANGED HWConfigs = " +
              JSON.stringify(HWConfigs)
          );
        }
        break;
      case "radioCapabilityIndication":
        // Gecko do not handle this UNSL command.
        let rc = message.rc;
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_RADIO_CAPABILITY rc = " +
              JSON.stringify(rc)
          );
        }
        break;
      case "lceData":
        // Gecko do not handle this UNSL command.
        let lce = message.lce;
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_LCEDATA_RECV lce = " + JSON.stringify(lce)
          );
        }
        break;
      case "pcoData":
        // Gecko do not handle this UNSL command.
        let pco = message.pco;
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_PCO_DATA pco = " + JSON.stringify(pco)
          );
        }

        let connHandler = lazy.gDataCallManager.getDataCallHandler(
          this.clientId
        );
        connHandler.updatePcoData(
          pco.cid,
          pco.bearerProto,
          pco.pcoId,
          pco.getContents()
        );
        break;
      case "imsNetworkStateChanged":
        // Gecko do not handle this UNSL command.
        if (DEBUG) {
          this.debug(
            "RILJ: [UNSL]< RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED "
          );
        }
        break;
      default:
        throw new Error(
          "Don't know about this message type: " + message.rilMessageType
        );
    }
  },
  /* eslint-enable complexity */

  handleRilConnected(newState) {
    this.sendRilRequest("setRadioEnabled", { enabled: false });
    this.sendRilRequest("setCellInfoListRate", null);
    // Always ensure that we are not in emergency callback mode when init.
    this.exitEmergencyCbMode();
    //Cameron handle the CDMA later.
    //mRil->setCdmaSubscriptionSource(mRil.mCdmaSubscription, null);
  },

  handleRadioStateChange(newState) {
    if (DEBUG) {
      this.debug(
        "Radio state changed from '" +
          this.radioState +
          "' to '" +
          newState +
          "'"
      );
    }

    if (this.radioState == newState) {
      return;
    }

    if (newState !== Ci.nsIRilIndicationResult.RADIOSTATE_UNKNOWN) {
      // Retrieve device identities once radio is available.
      this.sendRilRequest("getDeviceIdentity", null);
      this.sendRilRequest("getRadioCapability", null);
    }

    if (newState == Ci.nsIRilIndicationResult.RADIOSTATE_ENABLED) {
      // This value is defined in RIL v7, we will retrieve radio tech by another
      // request. We leave _isCdma untouched, and it will be set once we get the
      // radio technology.
      this._waitingRadioTech = true;
      this.sendRilRequest("getVoiceRadioTechnology", null);
    }

    if (newState != Ci.nsIRilIndicationResult.RADIOSTATE_ENABLED) {
      this._attachDataRegistration.result = false;
    }

    if (
      (this.radioState == Ci.nsIRilIndicationResult.RADIOSTATE_UNKNOWN ||
        this.radioState == Ci.nsIRilIndicationResult.RADIOSTATE_DISABLED) &&
      newState == Ci.nsIRilIndicationResult.RADIOSTATE_ENABLED
    ) {
      // The radio became available, let's get its info.
      if (!this._waitingRadioTech) {
        this.sendRilRequest("getDeviceIdentity", null);
      }
      this.sendRilRequest("getBasebandVersion", null);

      this.updateCellBroadcastConfig();

      this.sendRilRequest("setSuppServiceNotifications", { enable: true });

      if (
        libcutils.property_get("ro.moz.ril.data_reg_on_demand", "false") ===
          "true" &&
        !this._attachDataRegistration.result
      ) {
        this.setDataRegistration({
          attach: this._attachDataRegistration.attach,
        });
      } else if (
        libcutils.property_get("ro.moz.ril.subscription_control", "false") ===
          "true" &&
        this._attachDataRegistration.attach
      ) {
        this.setDataRegistration({ attach: true });
      }

      if (this.pendingToReportSmsMemoryStatus) {
        this.sendRilRequest("reportSmsMemoryStatus", {
          isAvailable: this.smsStorageAvailable,
        });
      }
    }

    this.radioState = newState;

    // If the radio is up and on, so let's query the card state.
    // On older RILs only if the card is actually ready, though.
    // If _waitingRadioTech is set, we don't need to get icc status now.
    if (
      newState == Ci.nsIRilIndicationResult.RADIOSTATE_UNKNOWN ||
      newState == Ci.nsIRilIndicationResult.RADIOSTATE_DISABLED ||
      this._waitingRadioTech
    ) {
      return;
    }

    this.sendRilRequest("getICCStatus", null);
  },

  _mergeCellBroadcastConfigs(list, from, to) {
    if (!list) {
      return [from, to];
    }

    for (let i = 0, f1, t1; i < list.length; ) {
      f1 = list[i++];
      t1 = list[i++];
      if (to == f1) {
        // ...[from]...[to|f1]...(t1)
        list[i - 2] = from;
        return list;
      }

      if (to < f1) {
        // ...[from]...(to)...[f1] or ...[from]...(to)[f1]
        if (i > 2) {
          // Not the first range pair, merge three arrays.
          return list
            .slice(0, i - 2)
            .concat([from, to])
            .concat(list.slice(i - 2));
        }
        return [from, to].concat(list);
      }

      if (from > t1) {
        // ...[f1]...(t1)[from] or ...[f1]...(t1)...[from]
        continue;
      }

      // Have overlap or merge-able adjacency with [f1]...(t1). Replace it
      // with [min(from, f1)]...(max(to, t1)).

      if (from < f1) {
        // [from]...[f1]...(t1) or [from][f1]...(t1)
        // Save minimum from value.
        list[i - 2] = from;
      }

      if (to <= t1) {
        // [from]...[to](t1) or [from]...(to|t1)
        // Can't have further merge-able adjacency. Return.
        return list;
      }

      // Try merging possible next adjacent range.
      let j = i;
      for (let f2, t2; j < list.length; ) {
        f2 = list[j++];
        t2 = list[j++];
        if (to > t2) {
          // [from]...[f2]...[t2]...(to) or [from]...[f2]...[t2](to)
          // Merge next adjacent range again.
          continue;
        }

        if (to < t2) {
          if (to < f2) {
            // [from]...(to)[f2] or [from]...(to)...[f2]
            // Roll back and give up.
            j -= 2;
          } else if (to < t2) {
            // [from]...[to|f2]...(t2), or [from]...[f2]...[to](t2)
            // Merge to [from]...(t2) and give up.
            to = t2;
          }
        }

        break;
      }

      // Save maximum to value.
      list[i - 1] = to;

      if (j != i) {
        // Remove merged adjacent ranges.
        let ret = list.slice(0, i);
        if (j < list.length) {
          ret = ret.concat(list.slice(j));
        }
        return ret;
      }

      return list;
    }

    // Append to the end.
    list.push(from);
    list.push(to);

    return list;
  },

  _isCellBroadcastConfigReady() {
    if (!("MMI" in this.cellBroadcastConfigs)) {
      return false;
    }

    // CBMI should be ready in GSM.
    if (
      !this._isCdma &&
      (!("CBMI" in this.cellBroadcastConfigs) ||
        !("CBMID" in this.cellBroadcastConfigs) ||
        !("CBMIR" in this.cellBroadcastConfigs))
    ) {
      return false;
    }

    return true;
  },

  /**
   * Merge all members of cellBroadcastConfigs into mergedCellBroadcastConfig.
   */
  _mergeAllCellBroadcastConfigs() {
    if (
      (Services.prefs.getBoolPref(kPrefAppCBConfigurationEnabled) || false) &&
      !this._isCellBroadcastConfigReady()
    ) {
      if (DEBUG) {
        this.debug("cell broadcast configs not ready, waiting ...");
      }
      return;
    }

    // Prepare cell broadcast config. CBMI* are only used in GSM.
    let usedCellBroadcastConfigs = { MMI: this.cellBroadcastConfigs.MMI };
    if (
      (Services.prefs.getBoolPref(kPrefAppCBConfigurationEnabled) || false) &&
      !this._isCdma
    ) {
      usedCellBroadcastConfigs.CBMI = this.cellBroadcastConfigs.CBMI;
      usedCellBroadcastConfigs.CBMID = this.cellBroadcastConfigs.CBMID;
      usedCellBroadcastConfigs.CBMIR = this.cellBroadcastConfigs.CBMIR;
    }

    if (DEBUG) {
      this.debug(
        "Cell Broadcast search lists: " +
          JSON.stringify(usedCellBroadcastConfigs)
      );
    }

    let list = null;
    for (let key in usedCellBroadcastConfigs) {
      let ll = usedCellBroadcastConfigs[key];
      if (ll == null) {
        continue;
      }

      for (let i = 0; i < ll.length; i += 2) {
        list = this._mergeCellBroadcastConfigs(list, ll[i], ll[i + 1]);
      }
    }

    if (DEBUG) {
      this.debug(
        "Cell Broadcast search lists(merged): " + JSON.stringify(list)
      );
    }
    this.mergedCellBroadcastConfig = list;
    this.updateCellBroadcastConfig();
  },

  setCellBroadcastDisabled(options) {
    this.cellBroadcastDisabled = options.disabled;

    // Return the response and let ril handle the rest thing.
    options.errorMsg = Ci.nsIRilResponseResult.RADIO_ERROR_NONE;
    this.handleRilResponse(options);

    // If |this.mergedCellBroadcastConfig| is null, either we haven't finished
    // reading required SIM files, or no any channel is ever configured.  In
    // the former case, we'll call |this.updateCellBroadcastConfig()| later
    // with correct configs; in the latter case, we don't bother resetting CB
    // to disabled again.
    if (this.mergedCellBroadcastConfig) {
      this.updateCellBroadcastConfig();
    }
  },

  setCellBroadcastSearchList(options) {
    let getSearchListStr = function(aSearchList) {
      if (typeof aSearchList === "string" || aSearchList instanceof String) {
        return aSearchList;
      }

      // TODO: Set search list for CDMA/GSM individually. Bug 990926
      let prop = this._isCdma ? "cdma" : "gsm";

      return aSearchList && aSearchList[prop];
    }.bind(this);

    try {
      let str = getSearchListStr(options.searchList);
      this.debug("setCellBroadcastSearchList  str: " + str);
      this.cellBroadcastConfigs.MMI = this._convertCellBroadcastSearchList(str);
      options.errorMsg = Ci.nsIRilResponseResult.RADIO_ERROR_NONE;
    } catch (e) {
      if (DEBUG) {
        this.debug("Invalid Cell Broadcast search list: " + e);
      }
      options.errorMsg = lazy.RIL.GECKO_ERROR_UNSPECIFIED_ERROR;
    }

    this.handleRilResponse(options);
    if (options.errorMsg) {
      return;
    }

    this._mergeAllCellBroadcastConfigs();
  },

  /**
   * Convert Cell Broadcast settings string into search list.
   */
  _convertCellBroadcastSearchList(searchListStr) {
    let parts = searchListStr && searchListStr.split(",");
    if (!parts) {
      return null;
    }

    let list = null;
    let result, from, to;
    for (let range of parts) {
      // Match "12" or "12-34". The result will be ["12", "12", null] or
      // ["12-34", "12", "34"].
      result = range.match(/^(\d+)(?:-(\d+))?$/);
      if (!result) {
        throw new Error("Invalid format");
      }

      from = parseInt(result[1], 10);
      to = result[2] ? parseInt(result[2], 10) + 1 : from + 1;
      if (
        !lazy.RIL.CB_NON_MMI_SETTABLE_RANGES &&
        !this._checkCellBroadcastMMISettable(from, to)
      ) {
        throw new Error("Invalid range");
      }

      if (list == null) {
        list = [];
      }
      list.push(from);
      list.push(to);
    }

    return list;
  },

  /**
   * Check whether search list from settings is settable by MMI, that is,
   * whether the range is bounded in any entries of CB_NON_MMI_SETTABLE_RANGES.
   */
  _checkCellBroadcastMMISettable(from, to) {
    if (to <= from || from >= 65536 || from < 0) {
      return false;
    }

    if (!this._isCdma) {
      // GSM not settable ranges.
      for (let i = 0, f, t; i < lazy.RIL.CB_NON_MMI_SETTABLE_RANGES.length; ) {
        f = lazy.RIL.CB_NON_MMI_SETTABLE_RANGES[i++];
        t = lazy.RIL.CB_NON_MMI_SETTABLE_RANGES[i++];
        if (from < t && to > f) {
          // Have overlap.
          return false;
        }
      }
    }

    return true;
  },

  updateCellBroadcastConfig() {
    let activate =
      !this.cellBroadcastDisabled &&
      this.mergedCellBroadcastConfig != null &&
      this.mergedCellBroadcastConfig.length > 0;
    if (activate) {
      this.setSmsBroadcastConfig(this.mergedCellBroadcastConfig);
    } else {
      // It's unnecessary to set config first if we're deactivating.
      this.setSmsBroadcastActivation(false);
    }
  },

  setSmsBroadcastActivation(activate) {
    if (this._isCdma) {
      // TODO complete the CDMA part.
    } else {
      this.sendRilRequest("setGsmBroadcastActivation", { activate }, null);
    }
  },

  setSmsBroadcastConfig(config) {
    if (this._isCdma) {
      // TODO mark the CDMA feature.
      /*this.sendRilRequest("setCdmaSmsBroadcastConfig"
                          , {config: config}
                          , null);*/
    } else {
      this.sendRilRequest("setGsmBroadcastConfig", { config }, null);
    }
  },

  handleStkProactiveCommand(message) {
    let berTlv;
    try {
      berTlv = this.simIOcontext.BerTlvHelper.decode(message.cmd);
      if (DEBUG) {
        this.debug(
          "handleStkProactiveCommand berTlv: " + JSON.stringify(berTlv)
        );
      }
    } catch (e) {
      if (DEBUG) {
        this.debug("handleStkProactiveCommand  error: " + e);
      }
      this.processSendStkTerminalResponse({
        resultCode: lazy.RIL.STK_RESULT_CMD_DATA_NOT_UNDERSTOOD,
      });
      return;
    }

    let ctlvs = berTlv.value;
    let ctlv = this.simIOcontext.StkProactiveCmdHelper.searchForTag(
      lazy.RIL.COMPREHENSIONTLV_TAG_COMMAND_DETAILS,
      ctlvs
    );

    if (!ctlv) {
      this.processSendStkTerminalResponse({
        resultCode: lazy.RIL.STK_RESULT_CMD_DATA_NOT_UNDERSTOOD,
      });
      throw new Error("Can't find COMMAND_DETAILS ComprehensionTlv");
    }

    let cmdDetails = ctlv.value;
    if (DEBUG) {
      this.debug(
        "commandNumber = " +
          cmdDetails.commandNumber +
          " typeOfCommand = " +
          cmdDetails.typeOfCommand.toString(16) +
          " commandQualifier = " +
          cmdDetails.commandQualifier
      );
    }

    // STK_CMD_MORE_TIME need not to propagate event to chrome.
    if (cmdDetails.typeOfCommand == lazy.RIL.STK_CMD_MORE_TIME) {
      this.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: lazy.RIL.STK_RESULT_OK,
      });
      return;
    }

    this.simIOcontext.StkCommandParamsFactory.createParam(
      cmdDetails,
      ctlvs,
      aResult => {
        cmdDetails.options = aResult;
        cmdDetails.rilMessageType = "stkcommand";
        this.handleUnsolicitedMessage(cmdDetails);
        //this.sendChromeMessage(cmdDetails);
      }
    );
  },

  handleNetworkStateChanged() {
    this.sendRilRequest("getICCStatus", null);
    //this.getICCStatus();
    this.requestNetworkInfo();
  },

  handleCallStateChanged() {
    this.sendRilRequest("getCurrentCalls", null);
  },

  /**
   * Request various states about the network.
   */
  requestNetworkInfo() {
    if (this._processingNetworkInfo) {
      if (DEBUG) {
        /*this.debug("Network info requested, but we're already " +
                           "requesting network info.");*/
      }
      this._needRepollNetworkInfo = true;
      return;
    }

    //if (DEBUG) console.log("Requesting network info");

    this._processingNetworkInfo = true;
    this.sendRilRequest("getVoiceRegistrationState", null);
    //this.getVoiceRegistrationState();
    this.sendRilRequest("getDataRegistrationState", null);
    //this.getDataRegistrationState(); //TODO only GSM
    this.sendRilRequest("getOperator", null);
    //this.getOperator();
    this.sendRilRequest("getNetworkSelectionMode", null);
    //this.getNetworkSelectionMode();
    this.sendRilRequest("getSignalStrength", null);
    //this.getSignalStrength();
  },

  setDataRegistration(attach) {
    let deferred = PromiseUtils.defer();
    this.sendWorkerMessage("setDataRegistration", { attach }, function(
      response
    ) {
      // Always resolve to proceed with the following steps.
      deferred.resolve(response.errorMsg ? response.errorMsg : null);
    });
    /*
    this.workerMessenger.send("setDataRegistration",
                              {attach: attach},
                              (function(response) {
      // Always resolve to proceed with the following steps.
      deferred.resolve(response.errorMsg ? response.errorMsg : null);
    }).bind(this));*/

    return deferred.promise;
  },

  /**
   * TODO: Bug 911713 - B2G NetworkManager: Move policy control logic to
   *                    NetworkManager
   */
  updateRILNetworkInterface() {
    let connHandler = lazy.gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.updateRILNetworkInterface();
  },

  /**
   * handle received SMS.
   */
  handleSmsReceived(aMessage) {
    let pdu = {};
    let pduLength = aMessage.getPdu(pdu);
    let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
    GsmPDUHelper.initWith(pdu.value);

    let [message, result] = GsmPDUHelper.processReceivedSms(pduLength);

    if (DEBUG) {
      this.debug(
        "RILJ: [UNSL]< RIL_UNSOL_RESPONSE_NEW_SMS :" +
          JSON.stringify(message) +
          ", result: " +
          result
      );
    }
    this._notifyNewSmsMessage(message);
  },

  /**
   * handle received SMS on sim.
   */
  handleSmsOnSim(aMessage) {
    this.simIOcontext.SimRecordHelper.readSMS(
      aMessage.recordNumber,
      function onsuccess(message) {
        if (message && message.simStatus === 3) {
          //New Unread SMS
          this._notifyNewSmsMessage(message);
        }
      }.bind(this),
      function onerror(errorMsg) {
        if (DEBUG) {
          this.debug(
            "Failed to Read NEW SMS on SIM #" +
              aMessage.recordNumber +
              ", errorMsg: " +
              errorMsg
          );
        }
      }
    );
  },

  _notifyNewSmsMessage(aMessage) {
    let header = aMessage.header;
    // Concatenation Info:
    // - segmentRef: a modulo 256 counter indicating the reference number for a
    //               particular concatenated short message. '0' is a valid number.
    // - The concatenation info will not be available in |header| if
    //   segmentSeq or segmentMaxSeq is 0.
    // See 3GPP TS 23.040, 9.2.3.24.1 Concatenated Short Messages.
    let segmentRef =
      header && header.segmentRef !== undefined ? header.segmentRef : 1;
    let segmentSeq = (header && header.segmentSeq) || 1;
    let segmentMaxSeq = (header && header.segmentMaxSeq) || 1;
    // Application Ports:
    // The port number ranges from 0 to 49151.
    // see 3GPP TS 23.040, 9.2.3.24.3/4 Application Port Addressing.
    let originatorPort =
      header && header.originatorPort !== undefined
        ? header.originatorPort
        : Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID;
    let destinationPort =
      header && header.destinationPort !== undefined
        ? header.destinationPort
        : Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID;

    let gonkSms = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIGonkSmsMessage]),
      smsc: aMessage.SMSC || null,
      sentTimestamp: aMessage.sentTimestamp,
      sender: aMessage.sender,
      pid: aMessage.pid,
      encoding: aMessage.encoding,
      messageClass: lazy.RIL.GECKO_SMS_MESSAGE_CLASSES.indexOf(
        aMessage.messageClass
      ),
      language: aMessage.language || null,
      segmentRef,
      segmentSeq,
      segmentMaxSeq,
      originatorPort,
      destinationPort,
      imsMessage: false,
      // MWI info:
      mwiPresent: !!aMessage.mwi,
      mwiDiscard: aMessage.mwi ? aMessage.mwi.discard : false,
      mwiMsgCount: aMessage.mwi ? aMessage.mwi.msgCount : 0,
      mwiActive: aMessage.mwi ? aMessage.mwi.active : false,
      // CDMA related attributes:
      cdmaMessageType: aMessage.messageType || 0,
      cdmaTeleservice: aMessage.teleservice || 0,
      cdmaServiceCategory: aMessage.serviceCategory || 0,
      body: aMessage.body || null,
    };

    lazy.gSmsService.notifyMessageReceived(
      this.clientId,
      gonkSms,
      aMessage.data || [],
      aMessage.data ? aMessage.data.length : 0
    );
  },

  /**
   * handle received SMS.
   */
  handleSmsStatusReport(aMessage) {
    let pdu = {};
    let pduLength = aMessage.getPdu(pdu);
    let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
    GsmPDUHelper.initWith(pdu.value);

    let [message, result] = GsmPDUHelper.processReceivedSms(pduLength);
    if (!message) {
      if (DEBUG) {
        this.debug("invalid SMS-STATUS-REPORT");
      }
      return result;
    }

    let pendingSms = this._pendingSentSmsMap[message.messageRef];
    if (!pendingSms) {
      if (DEBUG) {
        this.debug("no pending SMS-SUBMIT message");
      }
      return lazy.RIL.PDU_FCS_OK;
    }

    let status = message.status;

    // 3GPP TS 23.040 9.2.3.15 `The MS shall interpret any reserved values as
    // "Service Rejected"(01100011) but shall store them exactly as received.
    if (
      status >= 0x80 ||
      (status >= lazy.RIL.PDU_ST_0_RESERVED_BEGIN &&
        status < lazy.RIL.PDU_ST_0_SC_SPECIFIC_BEGIN) ||
      (status >= lazy.RIL.PDU_ST_1_RESERVED_BEGIN &&
        status < lazy.RIL.PDU_ST_1_SC_SPECIFIC_BEGIN) ||
      (status >= lazy.RIL.PDU_ST_2_RESERVED_BEGIN &&
        status < lazy.RIL.PDU_ST_2_SC_SPECIFIC_BEGIN) ||
      (status >= lazy.RIL.PDU_ST_3_RESERVED_BEGIN &&
        status < lazy.RIL.PDU_ST_3_SC_SPECIFIC_BEGIN)
    ) {
      status = lazy.RIL.PDU_ST_3_SERVICE_REJECTED;
    }

    // Pending. Waiting for next status report.
    if (status >>> 5 == 0x01) {
      if (DEBUG) {
        this.debug("SMS-STATUS-REPORT: delivery still pending");
      }
      return lazy.RIL.PDU_FCS_OK;
    }

    let _deliveryStatus =
      status >>> 5 === 0x00
        ? lazy.RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS
        : lazy.RIL.GECKO_SMS_DELIVERY_STATUS_ERROR;

    let response = {
      rilMessageType: "updateSMSDeliveryStatus",
      deliveryStatus: _deliveryStatus,
      rilMessageToken: pendingSms.rilMessageToken,
      errorMsg: 0,
    };
    delete this._pendingSentSmsMap[message.messageRef];
    this.handleRilResponse(response);

    //TODO: Find better place to send ack.
    this.sendRilRequest("ackSMS", { result: lazy.RIL.PDU_FCS_OK });
    return lazy.RIL.PDU_FCS_OK;
  },

  /**
   * Read UICC Phonebook contacts.
   *
   * @param contactType
   *        One of GECKO_CARDCONTACT_TYPE_*.
   * @param requestId
   *        Request id from RadioInterfaceLayer.
   */
  readICCContacts(options) {
    if (!this.appType) {
      options.errorMsg = lazy.RIL.CONTACT_ERR_REQUEST_NOT_SUPPORTED;
      this.handleRilResponse(options);
      return;
    }
    this.simIOcontext.ICCContactHelper.readICCContacts(
      this.appType,
      options.contactType,
      function onsuccess(contacts) {
        for (let i = 0; i < contacts.length; i++) {
          let contact = contacts[i];
          let pbrIndex = contact.pbrIndex || 0;
          let recordIndex =
            pbrIndex * ICC_MAX_LINEAR_FIXED_RECORDS + contact.recordId;
          contact.contactId = this.iccInfo.iccid + recordIndex;
        }
        // Reuse 'options' to get 'requestId' and 'contactType'.
        options.contacts = contacts;
        this.handleRilResponse(options);
      }.bind(this),
      function onerror(errorMsg) {
        options.errorMsg = errorMsg;
        this.handleRilResponse(options);
      }.bind(this)
    );
  },

  getMaxContactCount(options) {
    this.simIOcontext.ICCContactHelper.getMaxContactCount(
      this.appType,
      options.contactType,
      aTotalRecords => {
        options.totalRecords = aTotalRecords;
        this.handleRilResponse(options);
      },
      aError => {
        options.errorMsg = aError;
        this.handleRilResponse(options);
      }
    );
  },

  /**
   * Update UICC Phonebook.
   *
   * @param contactType   One of GECKO_CARDCONTACT_TYPE_*.
   * @param contact       The contact will be updated.
   * @param pin2          PIN2 is required for updating FDN.
   * @param requestId     Request id from RadioInterfaceLayer.
   */
  updateICCContact(options) {
    let onsuccess = function onsuccess(updatedContact) {
      let recordIndex =
        updatedContact.pbrIndex * ICC_MAX_LINEAR_FIXED_RECORDS +
        updatedContact.recordId;
      updatedContact.contactId = this.iccInfo.iccid + recordIndex;
      options.contact = updatedContact;
      // Reuse 'options' to get 'requestId' and 'contactType'.
      this.handleRilResponse(options);
    }.bind(this);

    let onerror = function onerror(errorMsg) {
      options.errorMsg = errorMsg;
      this.handleRilResponse(options);
    }.bind(this);

    if (!this.appType || !options.contact) {
      onerror(lazy.RIL.CONTACT_ERR_REQUEST_NOT_SUPPORTED);
      return;
    }

    let contact = options.contact;
    let iccid = this.iccInfo.iccid;
    let isValidRecordId = false;
    if (
      typeof contact.contactId === "string" &&
      contact.contactId.startsWith(iccid)
    ) {
      let recordIndex = contact.contactId.substring(iccid.length);
      contact.pbrIndex = Math.floor(recordIndex / ICC_MAX_LINEAR_FIXED_RECORDS);
      contact.recordId = recordIndex % ICC_MAX_LINEAR_FIXED_RECORDS;
      isValidRecordId = contact.recordId > 0 && contact.recordId < 0xff;
    }

    if (DEBUG) {
      this.simIOcontext.debug("Update ICC Contact " + JSON.stringify(contact));
    }

    let ICCContactHelper = this.simIOcontext.ICCContactHelper;
    // If contact has 'recordId' property, updates corresponding record.
    // If not, inserts the contact into a free record.
    if (isValidRecordId) {
      ICCContactHelper.updateICCContact(
        this.appType,
        options.contactType,
        contact,
        options.pin2,
        onsuccess,
        onerror
      );
    } else {
      ICCContactHelper.addICCContact(
        this.appType,
        options.contactType,
        contact,
        options.pin2,
        onsuccess,
        onerror
      );
    }
  },

  getIccServiceState(options) {
    if (options.service === lazy.RIL.GECKO_CARDSERVICE_FDN) {
      let ICCUtilsHelper = this.simIOcontext.ICCUtilsHelper;
      options.result = ICCUtilsHelper.isICCServiceAvailable("FDN");
    } else {
      options.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
    }
    this.handleRilResponse(options);
  },

  /**
   * Handle the NITZ message.
   * The data contained in the NITZ message is
   * in the form "yy/mm/dd,hh:mm:ss(+/-)tz,dt"
   * for example: 12/02/16,03:36:08-20,00,310410
   * See also bug 714352 - Listen for NITZ updates from rild.
   */

  handleNitzTime(message) {
    let dateString = message.dateString;
    let year = parseInt(dateString.substr(0, 2), 10);
    let month = parseInt(dateString.substr(3, 2), 10);
    let day = parseInt(dateString.substr(6, 2), 10);
    let hours = parseInt(dateString.substr(9, 2), 10);
    let minutes = parseInt(dateString.substr(12, 2), 10);
    let seconds = parseInt(dateString.substr(15, 2), 10);
    // Note that |tz| is in 15-min units.
    let tz = parseInt(dateString.substr(17, 3), 10);
    // Note that |dst| is in 1-hour units and is already applied in |tz|.
    let dst = parseInt(dateString.substr(21, 2), 10);

    let timeInMS = Date.UTC(
      year + lazy.RIL.PDU_TIMESTAMP_YEAR_OFFSET,
      month - 1,
      day,
      hours,
      minutes,
      seconds
    );

    if (isNaN(timeInMS)) {
      this.debug("NITZ failed to convert date");
      return;
    }

    let nitzData = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsINitzData]),
      time: timeInMS,
      timeZone: -(tz * 15),
      dst,
      receiveTime: message.receiveTimeInMS,
    };

    if (DEBUG) {
      this.debug("setTelephonyTime: " + nitzData);
    }
    lazy.gNetworkTimeService.setTelephonyTime(this.clientId, nitzData);
    this.debug("setTelephonyTime end");
  },

  handleIccMbdn(message) {
    lazy.gVoicemailService.notifyInfoChanged(
      this.clientId,
      message.number,
      message.alphaId
    );
  },

  handleIccMwis(mwi) {
    // Note: returnNumber and returnMessage is not available from UICC.
    lazy.gVoicemailService.notifyStatusChanged(
      this.clientId,
      mwi.active,
      mwi.msgCount,
      null,
      null
    );
  },

  _convertCbGsmGeographicalScope(aGeographicalScope) {
    return aGeographicalScope != null
      ? aGeographicalScope
      : Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_INVALID;
  },

  _convertCbMessageClass(aMessageClass) {
    let index = lazy.RIL.GECKO_SMS_MESSAGE_CLASSES.indexOf(aMessageClass);
    return index != -1
      ? index
      : Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL;
  },

  _convertCbEtwsWarningType(aWarningType) {
    return aWarningType != null
      ? aWarningType
      : Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID;
  },

  _processReceivedSmsCbPage(original) {
    if (original.numPages <= 1) {
      if (original.body) {
        original.fullBody = original.body;
        delete original.body;
      } else if (original.data) {
        original.fullData = original.data;
        delete original.data;
      }
      return original;
    }

    // Hash = <serial>:<mcc>:<mnc>:<lac>:<cid>
    let hash =
      original.serial + ":" + this.iccInfo.mcc + ":" + this.iccInfo.mnc + ":";
    switch (original.geographicalScope) {
      case lazy.RIL.CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE_IMMEDIATE:
      case lazy.RIL.CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE:
        hash +=
          this.voiceRegistrationState.cell.gsmLocationAreaCode +
          ":" +
          this.voiceRegistrationState.cell.gsmCellId;
        break;
      case lazy.RIL.CB_GSM_GEOGRAPHICAL_SCOPE_LOCATION_AREA_WIDE:
        hash += this.voiceRegistrationState.cell.gsmLocationAreaCode + ":";
        break;
      default:
        hash += ":";
        break;
    }

    let index = original.pageIndex;

    let options = this._receivedSmsCbPagesMap[hash];
    if (!options) {
      options = original;
      this._receivedSmsCbPagesMap[hash] = options;

      options.receivedPages = 0;
      options.pages = [];
    } else if (options.pages[index]) {
      // Duplicated page?
      if (DEBUG) {
        this.debug(
          "Got duplicated page no." +
            index +
            " of a multipage SMSCB: " +
            JSON.stringify(original)
        );
      }
      return null;
    }

    if (options.encoding == lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      options.pages[index] = original.data;
      delete original.data;
    } else {
      options.pages[index] = original.body;
      delete original.body;
    }
    options.receivedPages++;
    if (options.receivedPages < options.numPages) {
      if (DEBUG) {
        this.debug(
          "Got page no." +
            index +
            " of a multipage SMSCB: " +
            JSON.stringify(options)
        );
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsCbPagesMap[hash];

    // Rebuild full body
    if (options.encoding == lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all pages by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.numPages; i++) {
        fullDataLen += options.pages[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d = 0, i = 1; i <= options.numPages; i++) {
        let data = options.pages[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.pages.join("");
    }

    if (DEBUG) {
      this.debug("Got full multipage SMSCB: " + JSON.stringify(options));
    }

    return options;
  },

  handleCellbroadcastMessageReceived(aMessage) {
    let data = {};
    let dataLength = aMessage.getNewBroadcastSms(data);

    let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
    GsmPDUHelper.initWith(data.value);

    if (DEBUG) {
      this.debug("Receive Cellbroadcast pdu: " + JSON.stringify(data));
    }

    let message = GsmPDUHelper.readCbMessage(dataLength);
    if (DEBUG) {
      this.debug("Receive Cellbroadcast Message: " + JSON.stringify(message));
    }

    message = this._processReceivedSmsCbPage(message);
    if (!message) {
      return;
    }
    if (DEBUG) {
      this.debug("New Cellbroadcast Message: " + JSON.stringify(message));
    }

    let etwsInfo = message.etws;
    let hasEtwsInfo = etwsInfo != null;
    let serviceCategory = message.serviceCategory
      ? message.serviceCategory
      : Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID;

    let gonkCellBroadcastMessage = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIGonkCellBroadcastMessage]),
      gsmGeographicalScope: this._convertCbGsmGeographicalScope(
        message.geographicalScope
      ),
      messageCode: message.messageCode,
      serialNumber: message.serial,
      messageId: message.messageId,
      language: message.language,
      body: message.fullBody,
      messageClass: this._convertCbMessageClass(message.messageClass),
      timeStamp: Date.now(),
      cdmaServiceCategory: serviceCategory,
      hasEtwsInfo,
      etwsWarningType: hasEtwsInfo
        ? this._convertCbEtwsWarningType(etwsInfo.warningType)
        : Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID,
      etwsEmergencyUserAlert: hasEtwsInfo ? etwsInfo.emergencyUserAlert : false,
      etwsPopup: hasEtwsInfo ? etwsInfo.popup : false,
    };

    if (message.geoFencingTrigger) {
      gonkCellBroadcastMessage.geoFencingTriggerType =
        message.geoFencingTrigger.type;
      gonkCellBroadcastMessage.cellBroadcastIdentifiers =
        message.geoFencingTrigger.cbIdentifiers;
    }
    if (message.geometries) {
      gonkCellBroadcastMessage.geometries = message.geometries;
      gonkCellBroadcastMessage.maximumWaitingTimeSec =
        message.maximumWaitingTimeSec;
    }

    if (DEBUG) {
      this.debug(
        "New gonkCellBroadcastMessage: " +
          JSON.stringify(gonkCellBroadcastMessage)
      );
    }

    lazy.gCellBroadcastService.notifyMessageReceived(
      this.clientId,
      gonkCellBroadcastMessage
    );
  },

  handleCdmaInformationRecords(aRecords) {
    if (DEBUG) {
      this.debug("cdma-info-rec-received: " + JSON.stringify(aRecords));
    }

    let clientId = this.clientId;

    aRecords.forEach(function(aRecord) {
      if (aRecord.display) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecDisplay(
          clientId,
          aRecord.display
        );
        return;
      }

      if (aRecord.calledNumber) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecCalledPartyNumber(
          clientId,
          aRecord.calledNumber.type,
          aRecord.calledNumber.plan,
          aRecord.calledNumber.number,
          aRecord.calledNumber.pi,
          aRecord.calledNumber.si
        );
        return;
      }

      if (aRecord.callingNumber) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecCallingPartyNumber(
          clientId,
          aRecord.callingNumber.type,
          aRecord.callingNumber.plan,
          aRecord.callingNumber.number,
          aRecord.callingNumber.pi,
          aRecord.callingNumber.si
        );
        return;
      }

      if (aRecord.connectedNumber) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecConnectedPartyNumber(
          clientId,
          aRecord.connectedNumber.type,
          aRecord.connectedNumber.plan,
          aRecord.connectedNumber.number,
          aRecord.connectedNumber.pi,
          aRecord.connectedNumber.si
        );
        return;
      }

      if (aRecord.signal) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecSignal(
          clientId,
          aRecord.signal.type,
          aRecord.signal.alertPitch,
          aRecord.signal.signal
        );
        return;
      }

      if (aRecord.redirect) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecRedirectingNumber(
          clientId,
          aRecord.redirect.type,
          aRecord.redirect.plan,
          aRecord.redirect.number,
          aRecord.redirect.pi,
          aRecord.redirect.si,
          aRecord.redirect.reason
        );
        return;
      }

      if (aRecord.lineControl) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecLineControl(
          clientId,
          aRecord.lineControl.polarityIncluded,
          aRecord.lineControl.toggle,
          aRecord.lineControl.reverse,
          aRecord.lineControl.powerDenial
        );
        return;
      }

      if (aRecord.clirCause) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecClir(
          clientId,
          aRecord.clirCause
        );
        return;
      }

      if (aRecord.audioControl) {
        lazy.gMobileConnectionService.notifyCdmaInfoRecAudioControl(
          clientId,
          aRecord.audioControl.upLink,
          aRecord.audioControl.downLink
        );
      }
    });
  },

  // Flag to determine whether to update system clock automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _clockAutoUpdateEnabled: null,

  // Flag to determine whether to update system timezone automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _timezoneAutoUpdateEnabled: null,

  // Remember the last NITZ message so that we can set the time based on
  // the network immediately when users enable network-based time.
  _lastNitzMessage: null,

  // Cell Broadcast settings values.
  _cellBroadcastSearchList: null,

  handleError(aErrorMessage) {
    if (DEBUG) {
      this.debug("There was an error while reading RIL settings.");
    }
  },

  // nsIRadioInterface

  // TODO: Bug 928861 - B2G NetworkManager: Provide a more generic function
  //                    for connecting
  setupDataCallByType(networkType) {
    let connHandler = lazy.gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.setupDataCallByType(networkType);
  },

  // TODO: Bug 928861 - B2G NetworkManager: Provide a more generic function
  //                    for connecting
  deactivateDataCallByType(networkType) {
    let connHandler = lazy.gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.deactivateDataCallByType(networkType);
  },

  // TODO: Bug 904514 - [meta] NetworkManager enhancement
  getDataCallStateByType(networkType) {
    let connHandler = lazy.gDataCallManager.getDataCallHandler(this.clientId);
    return connHandler.getDataCallStateByType(networkType);
  },

  // nsIRilCallback

  handleRilIndication(response) {
    if (DEBUG) {
      this.debug(
        this.clientId,
        "Received message from ril: " + JSON.stringify(response)
      );
    }

    if (!response) {
      return;
    }

    this.handleUnsolicitedMessage(response);
  },

  /* eslint-disable complexity */
  handleRilResponse(response) {
    if (DEBUG) {
      this.debug(
        "Received message from worker handleRilResponse:" +
          JSON.stringify(response.rilMessageType)
      );
    }

    if (!response) {
      return;
    }

    // For backward avaliable. Intention for not change the origianl parameter naming for the upperlayer.
    let result = {};
    result.rilMessageType = response.rilMessageType;
    result.rilMessageToken = response.rilMessageToken;

    if (typeof response.errorMsg == "string") {
      result.errorMsg = response.errorMsg;
    } else {
      result.errorMsg = lazy.RIL.RIL_ERROR_TO_GECKO_ERROR[response.errorMsg];
    }

    // Handle solic reponse by function then call the call back.
    switch (response.rilMessageType) {
      case "getICCStatus":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_SIM_STATUS cardStatus = " +
                JSON.stringify(response.cardStatus)
            );
          }
          this.handleIccStatus(response);
          result.cardState = this.cardState;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_SIM_STATUS error = " +
              +result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "iccUnlockCardLock":
        if (DEBUG) {
          this.debug("iccUnlockCardLock command error.");
        }
        break;
      case "iccSetCardLockEnabled":
        if (DEBUG) {
          this.debug("iccSetCardLockEnabled command error.");
        }
        break;
      case "iccGetCardLockEnabled":
        if (DEBUG) {
          this.debug("iccGetCardLockEnabled command error.");
        }
        break;
      case "iccChangeCardLockPassword":
        if (DEBUG) {
          this.debug("iccChangeCardLockPassword command error.");
        }
        break;
      case "iccGetCardLockRetryCount":
        if (DEBUG) {
          this.debug("iccGetCardLockRetryCount command error.");
        }
        break;
      case "queryICCFacilityLock":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_FACILITY_LOCK response = " +
                JSON.stringify(response.serviceClass)
            );
          }
          //0 - Available & Disabled, 1-Available & Enabled, 2-Unavailable.
          result.serviceClass = response.serviceClass;
          result.enabled = result.serviceClass == 1;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_FACILITY_LOCK error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setICCFacilityLock":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_FACILITY_LOCK"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_FACILITY_LOCK error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                "remainingRetries = " +
                response.remainingRetries
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "enterICCPIN":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PIN"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PIN error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }

        break;
      case "enterICCPUK":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PUK"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PUK error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "enterICCPIN2":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PIN2"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PIN2 error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "enterICCPUK2":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PUK2"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ENTER_SIM_PUK2 error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "changeICCPIN":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CHANGE_SIM_PIN"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CHANGE_SIM_PIN error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "changeICCPIN2":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CHANGE_SIM_PIN2"
            );
          }
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CHANGE_SIM_PIN2 error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                ", remainingRetries = " +
                JSON.stringify(response.remainingRetries)
            );
          }
          result.remainingRetries = response.remainingRetries;
        }
        break;
      case "enterDepersonalization":
        break;
      case "getCurrentCalls":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_CURRENT_CALLS"
            );
          }
          result.calls = this.handleCurrentCalls(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_CURRENT_CALLS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "dial":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_DIAL"
            );
          }
          //this.handleRequestDial();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DIAL error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getIMSI":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_IMSI imsi = " +
                response.imsi
            );
          }
          this.iccInfoPrivate.imsi = result.imsi = response.imsi;
          lazy.gIccService.notifyImsiChanged(
            this.clientId,
            this.iccInfoPrivate.imsi
          );
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_IMSI error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "hangUpCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_HANGUP"
            );
          }
          //this.handleHangUpCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_HANGUP error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "hangUpBackground":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND"
            );
          }
          //this.handleHangUpBackgroundCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "hangUpForeground":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND"
            );
          }
          //this.handleHangUpForegroundCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "switchActiveCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE"
            );
          }
          //this.handleSwitchActiveCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "conferenceCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CONFERENCE"
            );
          }
          //this.handleConferenceCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_CONFERENCE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "udub":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_UDUB"
            );
          }
          //this.handleRejectCall();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_UDUB error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getFailCause":
        this._handleLastCallFailCause(response, result);
        break;
      case "explicitCallTransfer":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_EXPLICIT_CALL_TRANSFER"
            );
          }
          //this.handleExplicitCallTransfer();
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_EXPLICIT_CALL_TRANSFER error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getSignalStrength":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SIGNAL_STRENGTH signalStrength = " +
                JSON.stringify(response.signalStrength)
            );
          }
          this.handleSignalStrength(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SIGNAL_STRENGTH error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getVoiceRegistrationState":
        this._receivedNetworkInfo(
          lazy.RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE
        );
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_VOICE_REGISTRATION_STATE voiceRegStatus = " +
                JSON.stringify(response.voiceRegStatus)
            );
          }

          this.handleVoiceRegistrationState(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_VOICE_REGISTRATION_STATE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getDataRegistrationState":
        this._receivedNetworkInfo(
          lazy.RIL.NETWORK_INFO_DATA_REGISTRATION_STATE
        );
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_DATA_REGISTRATION_STATE dataRegStatus = " +
                JSON.stringify(response.dataRegStatus)
            );
          }
          this.handleDataRegistrationState(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_VOICE_REGISTRATION_STATE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getOperator":
        this._receivedNetworkInfo(lazy.RIL.NETWORK_INFO_OPERATOR);
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_OPERATOR operator = " +
                JSON.stringify(response.operator)
            );
          }

          this.handleOperator(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_OPERATOR error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getNetworkSelectionMode":
        this._receivedNetworkInfo(lazy.RIL.NETWORK_INFO_NETWORK_SELECTION_MODE);
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE nwModeManual = " +
                response.nwModeManual
            );
          }
          this.handleNetworkSelectionMode(response.nwModeManual);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setRadioEnabled":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_RADIO_POWER"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_RADIO_POWER error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setupDataCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SETUP_DATA_CALL dcResponse = " +
                JSON.stringify(response.dcResponse)
            );
          }
          result.dcResponse = response.dcResponse;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SETUP_DATA_CALL error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "iccIO":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SIM_IO IccIoResult = " +
                JSON.stringify(response.iccIo)
            );
          }
          this.handleIccIoResult(response);
          result.iccIo = response.iccIo;
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SIM_IO error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")" +
                " , IccIoResult =" +
                JSON.stringify(response.iccIo)
            );
          }
          result.iccIo = response.iccIo;
          this.handleIccIoErrorResult(result);
        }
        break;
      case "sendUSSD":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_SEND_USSD"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SEND_USSD error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "cancelUSSD":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CANCEL_USSD"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_CANCEL_USSD error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getCLIR":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_CLIR n = " +
                JSON.stringify(response.n) +
                " , m = " +
                JSON.stringify(response.m)
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_CLIR error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setCLIR":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_SET_CLIR"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_CLIR error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "queryCallForwardStatus":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_CALL_FORWARD_STATUS"
            );
          }
          result = this.handleQueryCallForwardStatus(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_CALL_FORWARD_STATUS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setCallForward":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_CALL_FORWARD"
            );
          }
          this._setCallForwardToSim(this._callForwardOptions);
          result = response;
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_CALL_FORWARD error = " +
                result.errorMsg +
                " (" +
                response.errorMsg +
                ")"
            );
          }
          this._callForwardOptions = {};
        }
        break;
      case "queryCallWaiting":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_CALL_WAITING"
            );
          }
          result.serviceClass = response.enable
            ? response.serviceClass
            : lazy.RIL.ICC_SERVICE_CLASS_NONE;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_CALL_WAITING error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setCallWaiting":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_CALL_WAITING"
            );
          }
          result.serviceClass = response.enable
            ? response.serviceClass
            : lazy.RIL.ICC_SERVICE_CLASS_NONE;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_CALL_WAITING error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "ackSMS":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SMS_ACKNOWLEDGE"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_ACKNOWLEDGE_GSM_SMS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "answerCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_ANSWER"
            );
          }
          //this.handleAnswerCall();
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_ANSWER error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "deactivateDataCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_DEACTIVATE_DATA_CALL"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DEACTIVATE_DATA_CALL error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "changeCallBarringPassword":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CHANGE_BARRING_PASSWORD"
            );
          }
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_CHANGE_BARRING_PASSWORD error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "selectNetworkAuto":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC"
            );
          }
          this.handleNetworkSelectionMode(false);
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "selectNetwork":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL"
            );
          }
          this.handleNetworkSelectionMode(true);
          result = response;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getAvailableNetworks":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_AVAILABLE_NETWORKS"
            );
          }
          result.networks = this.handleAvailableNetworks(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_AVAILABLE_NETWORKS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "sendTone":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_DTMF"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DTMF error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "startTone":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_DTMF_START"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DTMF_START error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "stopTone":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_DTMF_STOP"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DTMF_STOP error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getBasebandVersion":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_BASEBAND_VERSION basebandVersion = " +
                response.basebandVersion
            );
          }
          this.handleBasebandVersion(response);
          result.basebandVersion = response.basebandVersion;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_BASEBAND_VERSION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "separateCall":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SEPARATE_CONNECTION"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SEPARATE_CONNECTION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setMute":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < RIL_REQUEST_SET_MUTE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_MUTE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "queryCLIP":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_CLIP provisioned = " +
                response.provisioned
            );
          }
          result.provisioned = response.provisioned;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_CLIP error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getDataCallList":
        if (response.errorMsg == 0) {
          let dataCallLists = response.getDataCallLists();
          if (DEBUG) {
            this.debug(
              "RILJ: [UNSL]< RIL_REQUEST_DATA_CALL_LIST :" +
                JSON.stringify(dataCallLists)
            );
          }
          result.datacalls = dataCallLists;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "writeSmsToSIM":
        break;
      case "setPreferredNetworkType":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getPreferredNetworkType":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE type = " +
                response.type
            );
          }
          result.type = response.type;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getNeighboringCellIds":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_NEIGHBORING_CELL_IDS"
            );
          }
          result = this.handleNeighboringCellIds(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_NEIGHBORING_CELL_IDS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "queryTtyMode":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_QUERY_TTY_MODE ttymode = " +
                response.ttyMode
            );
          }
          result.ttyMode = response.ttyMode;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_QUERY_TTY_MODE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setRoamingPreference":
        break;
      case "queryRoamingPreference":
        break;
      case "setTtyMode":
        break;
      case "setVoicePrivacyMode":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE "
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "queryVoicePrivacyMode":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE enable = " +
                response.enhancedVoicePrivacy
            );
          }
          result.enabled = response.enhancedVoicePrivacy;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "cdmaFlash":
        break;
      case "sendSMS":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + response.rilMessageToken + "] < REQUEST_SEND_SMS"
            );
          }
          let message = this._pendingSmsRequest[response.rilMessageToken];
          delete this._pendingSmsRequest[response.rilMessageToken];
          result = this.handleSmsSendResult(response, message);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < REQUEST_SEND_SMS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "acknowledgeCdmaSms":
        break;
      case "setCellBroadcastDisabled":
        // This is not a ril reponse.
        break;
      case "setCellBroadcastSearchList":
        // This is not a ril reponse.
        break;
      case "setGsmBroadcastActivation":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GSM_BROADCAST_ACTIVATION"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GSM_BROADCAST_ACTIVATION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setGsmBroadcastConfig":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GSM_SET_BROADCAST_CONFIG"
            );
          }
          this.setSmsBroadcastActivation(true);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GSM_SET_BROADCAST_CONFIG error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setCdmaSmsBroadcastConfig":
        break;
      case "getCdmaSubscription":
        break;
      case "getDeviceIdentity":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_DEVICE_IDENTITY imei = " +
                response.imei +
                " , imeisv = " +
                response.imeisv +
                " ,esn = " +
                response.esn +
                " , meid = " +
                response.meid
            );
          }
          result.deviceIdentities = this.handleDeviceIdentity(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_DEVICE_IDENTITY error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "sendExitEmergencyCbModeRequest":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getSmscAddress":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_SMSC_ADDRESS smsc= " +
                response.smsc
            );
          }

          result = this.handleSmscAddress(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_SMSC_ADDRESS error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setSmscAddress":
        break;
      case "reportSmsMemoryStatus":
        break;
      case "reportStkServiceIsRunning":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "acknowledgeIncomingGsmSmsWithPDU":
        break;
      case "dataDownloadViaSMSPP":
        break;
      case "getVoiceRadioTechnology":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_VOICE_RADIO_TECH radioTech = " +
                response.radioTech
            );
          }
          this.handleVoiceRadioTechnology(response);
          result = this.radioTech;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_VOICE_RADIO_TECH error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getCellInfoList":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_CELL_INFO_LIST cellInfoLists = " +
                JSON.stringify(response.getCellInfoList())
            );
          }
          result.result = this.handleCellInfoList(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_VOICE_RADIO_TECH error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setInitialAttachApn":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_INITIAL_ATTACH_APN"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_INITIAL_ATTACH_APN error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setDataProfile":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_DATA_PROFILE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_DATA_PROFILE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "iccOpenChannel":
        break;
      case "iccCloseChannel":
        break;
      case "iccExchangeAPDU":
        break;
      case "setUiccSubscription":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_UICC_SUBSCRIPTION"
            );
          }
          this.handleUiccSubscription(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_UICC_SUBSCRIPTION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setDataRegistration":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_ALLOW_DATA"
            );
          }
          this.handleDataRegistration(response);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_ALLOW_DATA error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getIccAuthentication":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SIM_AUTHENTICATION sw1 =" +
                response.iccIo.sw1 +
                " , sw2 = " +
                response.iccIo.sw2 +
                " , simResponse = " +
                response.iccIo.simResponse
            );
          }
          result.sw1 = response.iccIo.sw1;
          result.sw2 = response.iccIo.sw2;
          result.responseData = response.iccIo.simResponse;
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SIM_AUTHENTICATION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setCellInfoListRate":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_CELL_INFO_LIST_RATE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_CELL_INFO_LIST_RATE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "getRadioCapability":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_GET_RADIO_CAPABILITY"
            );
          }
          this.handlRadioCapability(response.radioCapability);
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_GET_RADIO_CAPABILITY error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "stkHandleCallSetup":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "sendStkTerminalResponse":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "sendEnvelopeResponse":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND error = " +
              response.errorMsg
          );
        }
        break;
      case "stopNetworkScan":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_STOP_NETWORK_SCAN"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_STOP_NETWORK_SCAN error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "setUnsolResponseFilter":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "updateICCContact":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "readICCContacts":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "getMaxContactCount":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "setSuppServiceNotifications":
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" +
                response.rilMessageToken +
                "] < RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION"
            );
          }
        } else if (DEBUG) {
          this.debug(
            "RILJ: [" +
              response.rilMessageToken +
              "] < RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "updateSMSDeliveryStatus":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "getIccServiceState":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "iccUnlockSubsidyLock":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;
      case "iccGetSubsidyLockStatus":
        // This is not a ril reponse.
        response.errorMsg = result.errorMsg;
        result = response;
        break;

      default:
        throw new Error(
          "Unknow response message type : " + response.rilMessageType
        );
    }

    if (this.telephonyRequestQueue.isValidRequest(response.rilMessageType)) {
      this.telephonyRequestQueue.pop(response.rilMessageType);
    }

    let token = response.rilMessageToken;
    let callback = this.tokenCallbackMap[token];

    if (!callback) {
      if (DEBUG) {
        this.debug("Ignore orphan token: " + token);
      }
      return;
    }

    let keep = false;
    try {
      keep = callback(result);
    } catch (e) {
      if (DEBUG) {
        this.debug("callback throws an exception: " + e);
      }
    }

    if (!keep) {
      delete this.tokenCallbackMap[token];
    }
  },
  /* eslint-enable complexity */

  _handleLastCallFailCause(aResponse, aResult) {
    if (aResponse.errorMsg == 0) {
      if (DEBUG) {
        this.debug(
          "RILJ: [" +
            aResponse.rilMessageToken +
            "] < RIL_REQUEST_LAST_CALL_FAIL_CAUSE Cause = " +
            aResponse.causeCode
        );
      }
      aResult.failCause = this._disconnectCauseFromCode(aResponse.causeCode);
      aResult.vendorCause = aResponse.vendorCause;
    } else if (DEBUG) {
      this.debug(
        "RILJ: [" +
          aResponse.rilMessageToken +
          "] < RIL_REQUEST_LAST_CALL_FAIL_CAUSE error = " +
          aResult.errorMsg +
          " (" +
          aResponse.errorMsg +
          ")"
      );
    }
  },

  _disconnectCauseFromCode(aCauseCode) {
    // Treat it as CALL_FAIL_ERROR_UNSPECIFIED if the request failed.
    let failCause = lazy.RIL.GECKO_CALL_ERROR_UNSPECIFIED;
    failCause =
      lazy.RIL.RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[aCauseCode] || failCause;

    // Out Of Service case.
    if (
      !this.voiceRegistrationState.connected &&
      (failCause === lazy.RIL.GECKO_CALL_ERROR_NORMAL_CALL_CLEARING ||
        failCause === lazy.RIL.GECKO_CALL_ERROR_UNSPECIFIED)
    ) {
      failCause = lazy.RIL.GECKO_CALL_ERROR_SERVICE_NOT_AVAILABLE;
    }
    if (DEBUG) {
      this.debug("Last call fail cause: " + failCause);
    }

    return failCause;
  },

  // We combine all of the NETWORK_INFO_MESSAGE_TYPES into one "networkinfochange"
  // message to the RadioInterfaceLayer, so we can avoid sending multiple
  // VoiceInfoChanged events for both operator / voice_data_registration
  //
  // State management here is a little tricky. We need to know both:
  // 1. Whether or not a response was received for each of the
  //    NETWORK_INFO_MESSAGE_TYPES
  // 2. The outbound message that corresponds with that response -- but this
  //    only happens when internal state changes (i.e. it isn't guaranteed)
  //
  // To collect this state, each message response function first calls
  // _receivedNetworkInfo, to mark the response as received. When the
  // final response is received, a call to _sendPendingNetworkInfo is placed
  // on the next tick of the worker thread.
  //
  // Since the original call to _receivedNetworkInfo happens at the top
  // of the response handler, this gives the final handler a chance to
  // queue up it's "changed" message by calling _sendNetworkInfoMessage if/when
  // the internal state has actually changed.
  _sendNetworkInfoMessage(type, message) {
    if (!this._processingNetworkInfo) {
      // We only combine these messages in the case of the combined request
      // in requestNetworkInfo()
      this.handleUnsolicitedMessage(message);
      return;
    }

    if (DEBUG) {
      this.debug(
        "Queuing " + type + " network info message: " + JSON.stringify(message)
      );
    }
    this._pendingNetworkInfo[type] = message;
  },

  _receivedNetworkInfo(type) {
    if (DEBUG) {
      this.debug("Received " + type + " network info.");
    }
    if (!this._processingNetworkInfo) {
      return;
    }

    let pending = this._pendingNetworkInfo;

    // We still need to track states for events that aren't fired.
    if (!(type in pending)) {
      pending[type] = this.pendingNetworkType;
    }

    // Pending network info is ready to be sent when no more messages
    // are waiting for responses, but the combined payload hasn't been sent.
    for (let i = 0; i < lazy.RIL.NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      let msgType = lazy.RIL.NETWORK_INFO_MESSAGE_TYPES[i];
      if (!(msgType in pending)) {
        if (DEBUG) {
          this.debug(
            "Still missing some more network info, not " +
              "notifying main thread."
          );
        }
        return;
      }
    }

    // Do a pass to clean up the processed messages that didn't create
    // a response message, so we don't have unused keys in the outbound
    // networkinfochanged message.
    for (let key in pending) {
      if (pending[key] == this.pendingNetworkType) {
        delete pending[key];
      }
    }

    if (DEBUG) {
      this.debug(
        "All pending network info has been received: " + JSON.stringify(pending)
      );
    }

    // Send the message on the next tick of the worker's loop, so we give the
    // last message a chance to call _sendNetworkInfoMessage first.
    setTimeout(this._sendPendingNetworkInfo.bind(this), 0);
  },

  _sendPendingNetworkInfo() {
    this.handleUnsolicitedMessage(this._pendingNetworkInfo);

    this._processingNetworkInfo = false;
    for (let i = 0; i < lazy.RIL.NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      delete this._pendingNetworkInfo[lazy.RIL.NETWORK_INFO_MESSAGE_TYPES[i]];
    }

    if (this._needRepollNetworkInfo) {
      this._needRepollNetworkInfo = false;
      this.requestNetworkInfo();
    }
  },
  /**
   * Process LTE signal strength to the signal info object.
   *
   * @param signal
   *        The signal object reported from RIL/modem.
   *
   * @return The object of signal strength info.
   *         Or null if invalid signal input.
   *
   * TODO: Bug 982013: reconsider the format of signal strength APIs for
   *       GSM/CDMA/LTE to expose details, such as rsrp and rsnnr,
   *       individually.
   */

  /**
   * Process the network registration flags.
   *
   * @return true if the state changed, false otherwise.
   */
  _processCREG(curState, newState) {
    let changed = false;

    let regState = newState.regState;
    if (curState.regState === undefined || curState.regState !== regState) {
      changed = true;
      curState.regState = regState;

      curState.state =
        lazy.RIL.NETWORK_CREG_TO_GECKO_MOBILE_CONNECTION_STATE[regState];
      curState.connected =
        regState == Ci.nsIRilResponseResult.RADIO_REG_STATE_REG_HOME ||
        regState == Ci.nsIRilResponseResult.RADIO_REG_STATE_REG_ROAMING;
      curState.roaming =
        regState == Ci.nsIRilResponseResult.RADIO_REG_STATE_REG_ROAMING;
      curState.emergencyCallsOnly = !curState.connected;
    }

    // Handle the reasonDataDenied
    if (curState.state === lazy.RIL.GECKO_MOBILE_CONNECTION_STATE_DENIED) {
      if (
        curState.reasonDataDenied === undefined ||
        curState.reasonDataDenied !== newState.reasonDataDenied
      ) {
        changed = true;
        curState.reasonDataDenied = newState.reasonDataDenied;
      }
    } else {
      curState.reasonDataDenied = 0;
    }

    if (!curState.cell) {
      curState.cell = {};
    }
    //Cameron TODO need to separate it to GSM/WCDMA/LTE/TDSCDMA
    // Current MobileCellInfo only support GSM or CDMA.
    if (newState.cellIdentity) {
      switch (newState.cellIdentity.cellInfoType) {
        case Ci.nsIRilResponseResult.RADIO_CELL_INFO_TYPE_GSM: {
          let lac = newState.cellIdentity.cellIdentityGsm.lac;
          if (
            curState.cell.gsmLocationAreaCode === undefined ||
            curState.cell.gsmLocationAreaCode !== lac
          ) {
            curState.cell.gsmLocationAreaCode = lac;
            changed = true;
          }

          let cid = newState.cellIdentity.cellIdentityGsm.cid;
          if (
            curState.cell.gsmCellId === undefined ||
            curState.cell.gsmCellId !== cid
          ) {
            curState.cell.gsmCellId = cid;
            changed = true;
          }
          break;
        }
        case Ci.nsIRilResponseResult.RADIO_CELL_INFO_TYPE_WCDMA: {
          let lac = newState.cellIdentity.cellIdentityWcdma.lac;
          if (
            curState.cell.gsmLocationAreaCode === undefined ||
            curState.cell.gsmLocationAreaCode !== lac
          ) {
            curState.cell.gsmLocationAreaCode = lac;
            changed = true;
          }

          let cid = newState.cellIdentity.cellIdentityWcdma.cid;
          if (
            curState.cell.gsmCellId === undefined ||
            curState.cell.gsmCellId !== cid
          ) {
            curState.cell.gsmCellId = cid;
            changed = true;
          }
          break;
        }
        case Ci.nsIRilResponseResult.RADIO_CELL_INFO_TYPE_TD_SCDMA: {
          let lac = newState.cellIdentity.cellIdentityTdScdma.lac;
          if (
            curState.cell.gsmLocationAreaCode === undefined ||
            curState.cell.gsmLocationAreaCode !== lac
          ) {
            curState.cell.gsmLocationAreaCode = lac;
            changed = true;
          }

          let cid = newState.cellIdentity.cellIdentityTdScdma.cid;
          if (
            curState.cell.gsmCellId === undefined ||
            curState.cell.gsmCellId !== cid
          ) {
            curState.cell.gsmCellId = cid;
            changed = true;
          }
          break;
        }
        default:
          break;
      }
    } else if (DEBUG) {
      this.debug("cellIdentity null.");
    }

    let radioTech = newState.rat;
    if (curState.radioTech === undefined || curState.radioTech !== radioTech) {
      changed = true;
      curState.radioTech = radioTech;
      curState.type = lazy.RIL.GECKO_RADIO_TECH[radioTech] || null;
    }

    // From TS 23.003, 0000 and 0xfffe are indicated that no valid LAI exists
    // in MS. So we still need to report the '0000' as well.
    /*let lac = this.parseInt(newState[1], -1, 16);
    if (curState.cell.gsmLocationAreaCode === undefined ||
        curState.cell.gsmLocationAreaCode !== lac) {
      curState.cell.gsmLocationAreaCode = lac;
      changed = true;
    }

    let cid = this.parseInt(newState[2], -1, 16);
    if (curState.cell.gsmCellId === undefined ||
        curState.cell.gsmCellId !== cid) {
      curState.cell.gsmCellId = cid;
      changed = true;
    }

    let radioTech = (newState[3] === undefined ?
                     NETWORK_CREG_TECH_UNKNOWN :
                     this.parseInt(newState[3], NETWORK_CREG_TECH_UNKNOWN));
    if (curState.radioTech === undefined || curState.radioTech !== radioTech) {
      changed = true;
      curState.radioTech = radioTech;
      curState.type = GECKO_RADIO_TECH[radioTech] || null;
    }*/
    return changed;
  },

  handleVoiceRegistrationState(response) {
    if (DEBUG) {
      this.debug(
        "handleVoiceRegistrationState " +
          JSON.stringify(response.voiceRegStatus)
      );
    }
    let rs = this.voiceRegistrationState;
    let newState = response.voiceRegStatus;
    let stateChanged = this._processCREG(rs, newState);
    if (stateChanged && rs.connected) {
      this.sendRilRequest("getSmscAddress", null);
    }

    let cell = rs.cell;
    if (this._isCdma) {
      if (
        newState.cellIdentity &&
        newState.cellIdentity.cellInfoType ==
          Ci.nsIRilResponseResult.RADIO_CELL_INFO_TYPE_CDMA
      ) {
        // Some variables below are not used. Comment them instead of removing to
        // keep the information about state[x].
        let cdmaBaseStationId =
          newState.cellIdentity.cellIdentityCdma.baseStationId;
        let cdmaBaseStationLatitude =
          newState.cellIdentity.cellIdentityCdma.latitude;
        let cdmaBaseStationLongitude =
          newState.cellIdentity.cellIdentityCdma.longitude;
        // let cssIndicator = this.parseInt(state[7]);
        let cdmaSystemId = newState.cellIdentity.cellIdentityCdma.systemId;
        let cdmaNetworkId = newState.cellIdentity.cellIdentityCdma.networkId;
        let cdmaRoamingIndicator = newState.roamingIndicator;
        let cdmaSystemIsInPRL = !!newState.systemIsInPrl;
        let cdmaDefaultRoamingIndicator = newState.defaultRoamingIndicator;
        // let reasonForDenial = this.parseInt(state[13]);

        if (
          cell.cdmaBaseStationId !== cdmaBaseStationId ||
          cell.cdmaBaseStationLatitude !== cdmaBaseStationLatitude ||
          cell.cdmaBaseStationLongitude !== cdmaBaseStationLongitude ||
          cell.cdmaSystemId !== cdmaSystemId ||
          cell.cdmaNetworkId !== cdmaNetworkId ||
          cell.cdmaRoamingIndicator !== cdmaRoamingIndicator ||
          cell.cdmaSystemIsInPRL !== cdmaSystemIsInPRL ||
          cell.cdmaDefaultRoamingIndicator !== cdmaDefaultRoamingIndicator
        ) {
          stateChanged = true;
          cell.cdmaBaseStationId = cdmaBaseStationId;
          cell.cdmaBaseStationLatitude = cdmaBaseStationLatitude;
          cell.cdmaBaseStationLongitude = cdmaBaseStationLongitude;
          cell.cdmaSystemId = cdmaSystemId;
          cell.cdmaNetworkId = cdmaNetworkId;
          cell.cdmaRoamingIndicator = cdmaRoamingIndicator;
          cell.cdmaSystemIsInPRL = cdmaSystemIsInPRL;
          cell.cdmaDefaultRoamingIndicator = cdmaDefaultRoamingIndicator;
        }
      }
    }

    if (stateChanged) {
      rs.rilMessageType = "voiceregistrationstatechange";
      this._sendNetworkInfoMessage(
        lazy.RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE,
        rs
      );
    }
  },

  handleDataRegistrationState(response) {
    if (DEBUG) {
      this.debug(
        "handleDataRegistrationState " + JSON.stringify(response.dataRegStatus)
      );
    }
    let rs = this.dataRegistrationState;
    let state = response.dataRegStatus;
    let stateChanged = this._processCREG(rs, state);
    if (stateChanged) {
      rs.rilMessageType = "dataregistrationstatechange";
      this._sendNetworkInfoMessage(
        lazy.RIL.NETWORK_INFO_DATA_REGISTRATION_STATE,
        rs
      );
    }
  },

  handleChangedEmergencyCbMode(active) {
    this._isInEmergencyCbMode = active;

    // Clear the existed timeout event.
    this.cancelEmergencyCbModeTimeout();

    // Start a new timeout event when entering the mode.
    if (active) {
      this._exitEmergencyCbModeTimer.initWithCallback(
        this.exitEmergencyCbMode.bind(this),
        EMERGENCY_CB_MODE_TIMEOUT_MS,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    }

    lazy.gMobileConnectionService.notifyEmergencyCallbackModeChanged(
      this.clientId,
      active,
      EMERGENCY_CB_MODE_TIMEOUT_MS
    );
  },

  cancelEmergencyCbModeTimeout() {
    this._exitEmergencyCbModeTimer.cancel();
  },

  exitEmergencyCbMode() {
    this.cancelEmergencyCbModeTimeout();
    this.sendRilRequest("sendExitEmergencyCbModeRequest", null);
  },

  handleSimRefresh(aRefreshResult) {
    if (!aRefreshResult) {
      if (DEBUG) {
        this.debug("aRefreshResult null.");
      }
      return;
    }

    // Match the aid.
    let aid = aRefreshResult.aid;
    let simRecord;
    if (this.simIOcontext.SimRecordHelper.aid === aid) {
      if (DEBUG) {
        this.debug("SimRecordHelper. aid = " + aid);
      }
      simRecord = this.simIOcontext.SimRecordHelper;
    } else if (this.simIOcontext.ISimRecordHelper.aid === aid) {
      if (DEBUG) {
        this.debug("ISimRecordHelper. aid = " + aid);
      }
      simRecord = this.simIOcontext.ISimRecordHelper;
    } else {
      // TODO: handle ruim later.
      if (DEBUG) {
        this.debug("aid not match. aid = " + aid);
      }
      return;
    }

    switch (aRefreshResult.type) {
      case lazy.RIL.SIM_FILE_UPDATE:
        if (DEBUG) {
          this.debug("handleSimRefresh with SIM_FILE_UPDATED");
        }
        simRecord.handleFileUpdate(aRefreshResult.efId);
        break;
      case lazy.RIL.SIM_INIT:
      case lazy.RIL.SIM_RESET:
        if (DEBUG) {
          this.debug("handleSimRefresh with SIM_FILE_INIT or SIM_FILE_RESET");
        }
        this.sendRilRequest("getICCStatus", null);
        break;
      default:
        if (DEBUG) {
          this.debug("handleSimRefresh with unknown operation");
        }
        break;
    }
  },

  /**
   * The "numeric" portion of the operator info is a tuple
   * containing MCC (country code) and MNC (network code).
   * AFAICT, MCC should always be 3 digits, making the remaining
   * portion the MNC.
   */
  _processNetworkTuple(networkTuple, network) {
    let tupleLen = networkTuple.length;

    if (tupleLen >= 5) {
      network.mcc = networkTuple.substr(0, 3);
      network.mnc = networkTuple.substr(3);
    } else {
      network.mcc = null;
      network.mnc = null;

      throw new Error(
        "Invalid network tuple (should be 5 or 6 digits): " + networkTuple
      );
    }
  },

  handleOperator(response) {
    if (DEBUG) {
      this.debug("handleOperator " + JSON.stringify(response.operator));
    }
    let operatorData = response.operator;

    if (!this.operator) {
      this.operator = {
        rilMessageType: "operatorchange",
        longName: null,
        shortName: null,
      };
    }

    let longName = operatorData.alphaLong;
    let shortName = operatorData.alphaShort;
    let networkTuple = operatorData.operatorNumeric;
    let state = operatorData.status;
    let thisTuple = (this.operator.mcc || "") + (this.operator.mnc || "");

    if (
      this.operator.longName !== longName ||
      this.operator.shortName !== shortName ||
      thisTuple !== networkTuple
    ) {
      this.operator.mcc = null;
      this.operator.mnc = null;

      if (networkTuple) {
        try {
          this._processNetworkTuple(networkTuple, this.operator);
        } catch (e) {
          if (DEBUG) {
            this.debug("Error processing operator tuple: " + e);
          }
        }
      } else if (DEBUG) {
        // According to ril.h, the operator fields will be NULL when the operator
        // is not currently registered. We can avoid trying to parse the numeric
        // tuple in that case.
        this.debug("Operator is currently unregistered");
      }

      this.operator.longName = longName;
      this.operator.shortName = shortName;
      this.operator.state = lazy.RIL.RIL_QAN_STATE_TO_GECKO_STATE[state];

      let ICCUtilsHelper = this.simIOcontext.ICCUtilsHelper;
      if (ICCUtilsHelper.updateDisplayCondition() && this.iccInfo.iccid) {
        ICCUtilsHelper.handleICCInfoChange();
      }

      // NETWORK_INFO_OPERATOR message will be sent out by overrideNetworkName
      // itself if operator name is overridden after checking, or we have to
      // do it by ourself.
      if (!this.overrideNetworkName()) {
        this._sendNetworkInfoMessage(
          lazy.RIL.NETWORK_INFO_OPERATOR,
          this.operator
        );
      }
    }
  },

  /**
   * PLMN name display priority: EONS > ONS > NITZ (done by rild) > PLMN.
   *
   * @return true if PLMN name is overridden, false otherwise.
   */
  overrideNetworkName() {
    if (!this.operator) {
      return false;
    }

    if (this.overrideEonsNetworkName()) {
      if (DEBUG) {
        this.debug("Network name is overridden by EONS");
      }
      return true;
    } else if (this.overrideOnsNetworkName()) {
      if (DEBUG) {
        this.debug("Network name is overridden by ONS");
      }
      return true;
    }

    return false;
  },

  /**
   * Check if operator name needs to be overriden by current voiceRegistrationState
   * , EFOPL and EFPNN. See 3GPP TS 31.102 clause 4.2.58 EFPNN and 4.2.59 EFOPL
   *  for detail.
   *
   * @return true if operator name is overridden, false otherwise.
   */
  overrideEonsNetworkName() {
    // We won't get network name using PNN and OPL if voice registration isn't
    // ready.
    this.debug("overrideEonsNetworkName");
    if (
      !this.voiceRegistrationState.cell ||
      this.voiceRegistrationState.cell.gsmLocationAreaCode == -1
    ) {
      return false;
    }

    this.debug(
      "overrideEonsNetworkName gsmLocationAreaCode=" +
        this.voiceRegistrationState.cell.gsmLocationAreaCode
    );

    let ICCUtilsHelper = this.simIOcontext.ICCUtilsHelper;
    let networkName = ICCUtilsHelper.getNetworkNameFromICC(
      this.operator.mcc,
      this.operator.mnc,
      this.voiceRegistrationState.cell.gsmLocationAreaCode
    );

    if (!networkName) {
      return false;
    }

    if (DEBUG) {
      this.debug(
        "Operator names will be overriden: " +
          "longName = " +
          networkName.fullName +
          ", " +
          "shortName = " +
          networkName.shortName
      );
    }

    this.operator.longName = networkName.fullName;
    this.operator.shortName = networkName.shortName;

    this._sendNetworkInfoMessage(lazy.RIL.NETWORK_INFO_OPERATOR, this.operator);
    return true;
  },

  /**
   * Check if operator name needs to be overriden by current voiceRegistrationState
   * , CPHS Operator Name String and Operator Name Shortform. See B.4.1.2
   * Network Operator Namefor detail.
   *
   * @return true if operator name is overridden, false otherwise.
   */
  overrideOnsNetworkName() {
    if (!this.iccInfoPrivate.ons) {
      return false;
    }
    this.debug(
      "overrideOnsNetworkName this.iccInfoPrivate.ons=" +
        this.iccInfoPrivate.ons
    );

    // If ONS exists, only replace alpha long/short while under HPLMN.
    if (!this.voiceRegistrationState.roaming) {
      this.operator.longName = this.iccInfoPrivate.ons;
      this.operator.shortName = this.iccInfoPrivate.ons_short_form || "";

      this._sendNetworkInfoMessage(
        lazy.RIL.NETWORK_INFO_OPERATOR,
        this.operator
      );
      return true;
    }

    return false;
  },

  handleNeighboringCellIds(response) {
    let result = {};
    let rilNeighboringCellIds = response.getNeighboringCids();
    if (DEBUG) {
      this.debug(
        "handleNeighboringCellIds " + JSON.stringify(rilNeighboringCellIds)
      );
    }

    let radioTech = this.voiceRegistrationState.radioTech;
    if (
      radioTech == undefined ||
      radioTech == lazy.RIL.NETWORK_CREG_TECH_UNKNOWN
    ) {
      result.errorMsg = "RadioTechUnavailable";
      return result;
    }

    // Reference :
    // http://androidxref.com/7.1.1_r6/xref/frameworks/base/telephony/java/android/telephony/NeighboringCellInfo.java#113
    if (
      !this._isGsmTechGroup(radioTech) ||
      radioTech === lazy.RIL.NETWORK_CREG_TECH_GSM ||
      radioTech === lazy.RIL.NETWORK_CREG_TECH_LTE
    ) {
      result.errorMsg = "UnsupportedRadioTech";
      return result;
    }

    let neighboringCellIds = [];
    let num = rilNeighboringCellIds.length;
    for (let i = 0; i < num; i++) {
      let cellId = {};
      cellId.networkType = lazy.RIL.GECKO_RADIO_TECH[radioTech];
      cellId.signalStrength = rilNeighboringCellIds[i].rssi;

      let cid = rilNeighboringCellIds[i].cid;
      // pad cid string with leading "0"
      let length = cid.length;
      if (length > 8) {
        continue;
      }
      if (length < 8) {
        for (let j = 0; j < 8 - length; j++) {
          cid = "0" + cid;
        }
      }

      switch (radioTech) {
        case lazy.RIL.NETWORK_CREG_TECH_GPRS:
        case lazy.RIL.NETWORK_CREG_TECH_EDGE:
          cellId.gsmCellId = this.parseInt(cid.substring(4), -1, 16);
          cellId.gsmLocationAreaCode = this.parseInt(
            cid.substring(0, 4),
            -1,
            16
          );
          break;
        case lazy.RIL.NETWORK_CREG_TECH_UMTS:
        case lazy.RIL.NETWORK_CREG_TECH_HSDPA:
        case lazy.RIL.NETWORK_CREG_TECH_HSUPA:
        case lazy.RIL.NETWORK_CREG_TECH_HSPA:
        case lazy.RIL.NETWORK_CREG_TECH_HSPAP:
          cellId.wcdmaPsc = this.parseInt(cid, -1, 16);
          break;
        default:
          continue;
      }

      neighboringCellIds.push(cellId);
    }

    result.result = neighboringCellIds;
    return result;
  },

  /**
   * Helper for processing sent multipart SMS.
   */
  _processSentSmsSegment(options) {
    // Setup attributes for sending next segment
    let next = options.segmentSeq;
    options.body = options.segments[next].body;
    options.encodedBodyLength = options.segments[next].encodedBodyLength;
    options.segmentSeq = next + 1;
    let callback = this.tokenCallbackMap[options.rilMessageToken];
    delete this.tokenCallbackMap[options.rilMessageToken];
    if (DEBUG) {
      this.debug("postpone sendSMS callback to next sms segment");
    }
    this.sendRilRequest("sendSMS", options, callback);
  },

  /**
   * Helper for processing result of send SMS.
   *
   */
  handleSmsSendResult(response, message) {
    message.messageRef = response.sms.messageRef;
    message.ackPDU = response.sms.ackPDU;
    message.errorCode = response.sms.errorCode;
    if (
      message.segmentMaxSeq > 1 &&
      message.segmentSeq < message.segmentMaxSeq
    ) {
      // Not last segment
      this._processSentSmsSegment(message);
    } else if (message.requestStatusReport) {
      // Last segment sent with success.
      if (DEBUG) {
        this.debug(
          "waiting SMS-STATUS-REPORT for messageRef " + message.messageRef
        );
      }
      this._pendingSentSmsMap[response.sms.messageRef] = message;
    }
    return message;
  },

  /**
   * Parse an integer from a string, falling back to a default value
   * if the the provided value is not a string or does not contain a valid
   * number.
   *
   * @param string
   *        String to be parsed.
   * @param defaultValue [optional]
   *        Default value to be used.
   * @param radix [optional]
   *        A number that represents the numeral system to be used. Default 10.
   */
  parseInt(string, defaultValue, radix) {
    let number = parseInt(string, radix || 10);
    if (!isNaN(number)) {
      return number;
    }
    if (defaultValue === undefined) {
      defaultValue = null;
    }
    return defaultValue;
  },

  handleNetworkSelectionMode(mode) {
    if (DEBUG) {
      this.debug("handleNetworkSelectionMode " + JSON.stringify(mode));
    }

    let selectionMode = mode
      ? lazy.RIL.GECKO_NETWORK_SELECTION_MANUAL
      : lazy.RIL.GECKO_NETWORK_SELECTION_AUTOMATIC;
    if (this.networkSelectionMode === selectionMode) {
      return;
    }

    let options = {
      rilMessageType: "networkselectionmodechange",
      mode: selectionMode,
    };
    this.networkSelectionMode = selectionMode;
    this._sendNetworkInfoMessage(
      lazy.RIL.NETWORK_INFO_NETWORK_SELECTION_MODE,
      options
    );
  },

  handleSignalStrength(response) {
    if (DEBUG) {
      this.debug(
        "handleSignalStrength " + JSON.stringify(response.signalStrength)
      );
    }
    let signalStrength = response.signalStrength;

    let signal = {};

    signal.gsmSignalStrength = signalStrength.gsmSignalStrength.signalStrength;
    signal.gsmBitErrorRate = signalStrength.gsmSignalStrength.bitErrorRate;

    signal.cdmaDbm = signalStrength.cdmaSignalStrength.dbm;
    signal.cdmaEcio = signalStrength.cdmaSignalStrength.ecio;
    signal.cdmaEvdoDbm = signalStrength.evdoSignalStrength.dbm;
    signal.cdmaEvdoEcio = signalStrength.evdoSignalStrength.ecio;
    signal.cdmaEvdoSNR = signalStrength.evdoSignalStrength.signalNoiseRatio;

    signal.lteSignalStrength = signalStrength.lteSignalStrength.signalStrength;
    signal.lteRsrp = signalStrength.lteSignalStrength.rsrp;
    signal.lteRsrq = signalStrength.lteSignalStrength.rsrq;
    signal.lteRssnr = signalStrength.lteSignalStrength.rssnr;
    signal.lteCqi = signalStrength.lteSignalStrength.cqi;
    signal.lteTimingAdvance = signalStrength.lteSignalStrength.timingAdvance;

    signal.tdscdmaRscp = signalStrength.tdscdmaSignalStrength.rscp;

    if (DEBUG) {
      this.debug("signal strength: " + JSON.stringify(signal));
    }

    lazy.gMobileConnectionService.notifySignalStrengthChanged(
      this.clientId,
      signal
    );
  },

  handleUiccSubscription(response) {
    // Resend data subscription after uicc subscription.
    if (this._attachDataRegistration.attach) {
      this.setDataRegistration({ attach: true });
    }
  },

  /* eslint-disable complexity */
  handleIccStatus(response) {
    if (DEBUG) {
      this.debug("handleIccStatus " + JSON.stringify(response.cardStatus));
    }
    // If |_waitingRadioTech| is true, we should not get app information because
    // the |_isCdma| flag is not ready yet. Otherwise we may use wrong index to
    // get app information, especially for the case that icc card has both cdma
    // and gsm subscription.
    if (this._waitingRadioTech) {
      return;
    }

    let iccStatus = response.cardStatus;
    // When |iccStatus.cardState| is not CARD_STATE_PRESENT, set cardState to
    // undetected.
    if (iccStatus.cardState !== Ci.nsIRilResponseResult.CARD_STATE_PRESENT) {
      if (this.cardState !== Ci.nsIRilResponseResult.CARD_STATE_UNDETECTED) {
        this.operator = null;
        // We should send |cardstatechange| before |iccinfochange|, otherwise we
        // may lost cardstatechange event when icc card becomes undetected.
        this.cardState = Ci.nsIRilResponseResult.CARD_STATE_UNDETECTED;
        this.pin2CardState = Ci.nsIRilResponseResult.CARD_STATE_UNDETECTED;

        lazy.gIccService.notifyCardStateChanged(
          this.clientId,
          this.cardState,
          this.pin2CardState
        );
        lazy.gRadioEnabledController.receiveCardState(this.clientId);
        this.iccInfo = { iccType: null };
        lazy.gIccService.notifyIccInfoChanged(this.clientId, this.iccInfo);
      }
      return;
    }

    if (
      libcutils.property_get("ro.moz.ril.subscription_control", "false") ===
      "true"
    ) {
      // All appIndex is -1 means the subscription is not activated yet.
      // Note that we don't support "ims" for now, so we don't take it into
      // account.
      let neetToActivate =
        iccStatus.cdmaSubscriptionAppIndex === -1 &&
        iccStatus.gsmUmtsSubscriptionAppIndex === -1;
      if (
        neetToActivate &&
        // Note: setUiccSubscription works abnormally when RADIO is OFF,
        // which causes SMS function broken in Flame.
        // See bug 1008557 for detailed info.
        this.radioState === Ci.nsIRilIndicationResult.RADIOSTATE_ENABLED
      ) {
        for (let i = 0; i < iccStatus.apps.length; i++) {
          this.sendRilRequest("setUiccSubscription", {
            slotId: this.clientId,
            appIndex: i,
            subId: this.clientId,
            subStatus: true,
          });
        }
      }
    }

    let newCardState;
    let newPin2CardState;
    let index = this._isCdma
      ? iccStatus.cdmaSubscriptionAppIndex
      : iccStatus.gsmUmtsSubscriptionAppIndex;
    let apps = iccStatus.getAppStatus();
    let app = apps[index];
    if (DEBUG) {
      this.debug("handleIccStatus app = " + JSON.stringify(app));
    }

    if (app) {
      // fetchICCRecords will need to read aid, so read aid here.
      this.aid = app.aidPtr;
      this.appType = app.appType;
      this.iccInfo.iccType = lazy.RIL.GECKO_CARD_TYPE[this.appType];

      switch (app.appState) {
        case lazy.RIL.CARD_APPSTATE_ILLEGAL:
          newCardState = lazy.RIL.GECKO_CARDSTATE_ILLEGAL;
          break;
        case lazy.RIL.CARD_APPSTATE_PIN:
          newCardState = lazy.RIL.GECKO_CARDSTATE_PIN_REQUIRED;
          break;
        case lazy.RIL.CARD_APPSTATE_PUK:
          newCardState = lazy.RIL.GECKO_CARDSTATE_PUK_REQUIRED;
          break;
        case lazy.RIL.CARD_APPSTATE_READY:
          newCardState = lazy.RIL.GECKO_CARDSTATE_READY;
          break;
        case lazy.RIL.CARD_APPSTATE_SUBSCRIPTION_PERSO:
          newCardState = lazy.RIL.PERSONSUBSTATE[app.persoSubstate];
          break;
        case lazy.RIL.CARD_APPSTATE_UNKNOWN:
        case lazy.RIL.CARD_APPSTATE_DETECTED:
        // Fall through.
        default:
          newCardState = lazy.RIL.GECKO_CARDSTATE_UNKNOWN;
      }

      let pin1State = app.pin1Replaced ? iccStatus.universalPinState : app.pin1;
      if (pin1State === lazy.RIL.CARD_PINSTATE_ENABLED_PERM_BLOCKED) {
        newCardState = lazy.RIL.GECKO_CARDSTATE_PERMANENT_BLOCKED;
      }

      // Handle pin2 state.
      switch (app.pin2) {
        case lazy.RIL.CARD_PINSTATE_ENABLED_NOT_VERIFIED:
          newPin2CardState = lazy.RIL.GECKO_CARDSTATE_PIN_REQUIRED;
          break;
        case lazy.RIL.CARD_PINSTATE_ENABLED_VERIFIED:
          newPin2CardState = lazy.RIL.GECKO_CARDSTATE_READY;
          break;
        case lazy.RIL.CARD_PINSTATE_ENABLED_BLOCKED:
          newPin2CardState = lazy.RIL.GECKO_CARDSTATE_PUK_REQUIRED;
          break;
        case lazy.RIL.CARD_PINSTATE_ENABLED_PERM_BLOCKED:
          newPin2CardState = lazy.RIL.GECKO_CARDSTATE_PERMANENT_BLOCKED;
          break;
        default:
          newPin2CardState = lazy.RIL.GECKO_CARDSTATE_UNKNOWN;
      }
    } else {
      // Having incorrect app information, set card state to unknown.
      newCardState = lazy.RIL.GECKO_CARDSTATE_UNKNOWN;
      newPin2CardState = lazy.RIL.GECKO_CARDSTATE_UNKNOWN;
    }

    //Read the iccid only when card app status change from
    //uninitialized,undetected,unknown to ready or locked
    if (
      (this.cardState === lazy.RIL.GECKO_CARDSTATE_UNINITIALIZED ||
        this.cardState === lazy.RIL.GECKO_CARDSTATE_UNDETECTED ||
        this.cardState === lazy.RIL.GECKO_CARDSTATE_UNKNOWN) &&
      newCardState != lazy.RIL.GECKO_CARDSTATE_UNINITIALIZED &&
      newCardState != lazy.RIL.GECKO_CARDSTATE_UNDETECTED &&
      newCardState != lazy.RIL.GECKO_CARDSTATE_UNKNOWN
    ) {
      this.simIOcontext.ICCRecordHelper.readICCID();
    }

    if (
      this.cardState == newCardState &&
      this.pin2CardState == newPin2CardState
    ) {
      return;
    }

    // This was moved down from CARD_APPSTATE_READY
    this.requestNetworkInfo();
    if (newCardState == lazy.RIL.GECKO_CARDSTATE_READY) {
      // Cache SIM/RUIM/ISIM aid(s) for later use.
      let usimApp = apps[iccStatus.gsmUmtsSubscriptionAppIndex];
      if (usimApp) {
        this.simIOcontext.SimRecordHelper.setAid(usimApp.aidPtr);
      }

      //let ruimApp = apps[iccStatus.cdmaSubscriptionAppIndex];
      //if (ruimApp) {
      //  this.simIOcontext.RuimRecordHelper.setAid(ruimApp.aidPtr);
      //}

      let isimApp = apps[iccStatus.imsSubscriptionAppIndex];
      if (isimApp) {
        this.simIOcontext.ISimRecordHelper.setAid(isimApp.aidPtr);
      }

      // For type SIM, we need to check EF_phase first.
      // Other types of ICC we can send Terminal_Profile immediately.
      if (this.appType == lazy.RIL.CARD_APPTYPE_SIM) {
        this.simIOcontext.SimRecordHelper.readSimPhase();
      } else if (
        libcutils.property_get("ro.moz.ril.send_stk_profile_dl", "false") ===
        "true"
      ) {
        //Cameron mark first.
        //this.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }
      this.simIOcontext.ICCRecordHelper.fetchICCRecords();
    }

    this.cardState = newCardState;
    this.pin2CardState = newPin2CardState;
    lazy.gIccService.notifyCardStateChanged(
      this.clientId,
      this.cardState,
      this.pin2CardState
    );
    lazy.gRadioEnabledController.receiveCardState(this.clientId);
  },
  /* eslint-enable complexity */

  handleDeviceIdentity(response) {
    if (DEBUG) {
      this.debug(
        "handleDeviceIdentity imei=" +
          response.imei +
          ", imeisv=" +
          response.imeisv +
          " ,esn =" +
          response.esn +
          " , meid=" +
          response.meid
      );
    }
    if (response.errorMsg != 0) {
      if (DEBUG) {
        this.debug("Failed to get device identities:" + response.errorMsg);
      }
      return this.deviceIdentities;
    }

    let newIMEI, newIMEISV, newESN, newMEID;
    newIMEI = response.imei;
    newIMEISV = response.imeisv;
    newESN = response.esn;
    newMEID = response.meid;

    if (
      !this.deviceIdentities ||
      newIMEI != this.deviceIdentities.imei ||
      newIMEISV != this.deviceIdentities.imeisv ||
      newESN != this.deviceIdentities.esn ||
      newMEID != this.deviceIdentities.meid
    ) {
      this.deviceIdentities = {
        imei: newIMEI || null,
        imeisv: newIMEISV || null,
        esn: newESN || null,
        meid: newMEID || null,
      };

      lazy.gMobileConnectionService.notifyDeviceIdentitiesChanged(
        this.clientId,
        this.deviceIdentities.imei,
        this.deviceIdentities.imeisv,
        this.deviceIdentities.esn,
        this.deviceIdentities.meid
      );
      return this.deviceIdentities;
    }

    return this.deviceIdentities;
  },

  handlRadioCapability(aRadioCapability) {
    if (!aRadioCapability) {
      this.debug("handleRadioCapability without aRadioCapability");
      return;
    }
    if (DEBUG) {
      this.debug(
        "handleRadioCapability session=" +
          aRadioCapability.session +
          ", phase=" +
          aRadioCapability.phase +
          ", raf=" +
          aRadioCapability.raf +
          ", logicalModemUuid=" +
          aRadioCapability.logicalModemUuid +
          ", status=" +
          aRadioCapability.status
      );
    }

    this._radioCapability = {
      session: aRadioCapability.session,
      phase: aRadioCapability.phase,
      raf: aRadioCapability.raf,
      logicalModemUuid: aRadioCapability.logicalModemUuid,
      status: aRadioCapability.status,
    };
  },

  handleVoiceRadioTechnology(response) {
    if (DEBUG) {
      this.debug(
        "Received message from worker handleVoiceRadioTechnology:" +
          JSON.stringify(response.radioTech)
      );
    }

    let isCdma = !this._isGsmTechGroup(response.radioTech);
    this.radioTech = response.radioTech;

    if (DEBUG) {
      this.debug(
        "Radio tech is set to: " +
          lazy.RIL.GECKO_RADIO_TECH[response.radioTech] +
          ", it is a " +
          (isCdma ? "cdma" : "gsm") +
          " technology"
      );
    }

    // We should request SIM information when
    //  1. Radio state has been changed, so we are waiting for radioTech or
    //  2. isCdma is different from this._isCdma.
    if (this._waitingRadioTech || isCdma != this._isCdma) {
      this._isCdma = isCdma;
      this._waitingRadioTech = false;

      this.sendRilRequest("getDeviceIdentity", null);
      //this.getDeviceIdentity();
      this.sendRilRequest("getICCStatus", null);
      //this.getICCStatus();
    }
  },

  /**
   * Check if GSM radio access technology group.
   */
  _isGsmTechGroup(radioTech) {
    if (!radioTech) {
      return true;
    }

    switch (radioTech) {
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_GPRS:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_EDGE:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_UMTS:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_HSDPA:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_HSUPA:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_HSPA:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_LTE:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_HSPAP:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_GSM:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_TD_SCDMA:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_IWLAN:
      case Ci.nsIRadioTechnologyState.RADIO_CREG_TECH_LTE_CA:
        return true;
    }

    return false;
  },

  handleDataRegistration(response) {
    if (response.errorMsg != 0) {
      this._attachDataRegistration.result = false;
    } else {
      this._attachDataRegistration.result = true;
    }
  },

  handleBasebandVersion(response) {
    if (response.errorMsg != 0) {
      return;
    }
    this.basebandVersion = response.basebandVersion;
  },

  /**
   * Set CallForward Options to EFCfis
   * TODO: Only support SERVICE_CLASS_VOICE currently
   */
  _setCallForwardToSim(options) {
    if (DEBUG) {
      this.debug("setCallForward to sim, options:" + JSON.stringify(options));
    }

    if (options.serviceClass & (lazy.RIL.ICC_SERVICE_CLASS_VOICE == 0)) {
      if (DEBUG) {
        this.debug("setCallForwardToSim only support ICC_SERVICE_CLASS_VOICE");
      }
      return;
    }

    // Only unconditional CF needs to set flag.
    if (options.reason != lazy.RIL.CALL_FORWARD_REASON_UNCONDITIONAL) {
      return;
    }

    let iccInfo = this.iccInfoPrivate;

    if (iccInfo.cfis) {
      // Cameron mark first.
      //this.context.SimRecordHelper.updateCFIS(options);
    }
    if (iccInfo.cff) {
      // Cameron mark first.
      //this.context.SimRecordHelper.updateCphsCFF(options.action);
    }
  },

  handleCurrentCalls(response) {
    let rilCallResponse = response.getCurrentCalls();
    if (DEBUG) {
      this.debug("handleCurrentCalls " + JSON.stringify(rilCallResponse));
    }
    let calls = {};
    for (let i = 0; i < rilCallResponse.length; i++) {
      let call = {};

      call.state = rilCallResponse[i].state; // CALL_STATE_*
      call.callIndex = rilCallResponse[i].index; // GSM index (1-based)
      call.toa = rilCallResponse[i].toa;
      call.isMpty = rilCallResponse[i].isMpty;
      call.isMT = rilCallResponse[i].isMT;
      call.als = rilCallResponse[i].als;
      call.isVoice = rilCallResponse[i].isVoice;
      call.isVoicePrivacy = rilCallResponse[i].isVoicePrivacy;
      call.number = rilCallResponse[i].number;
      call.numberPresentation = rilCallResponse[i].numberPresentation; // CALL_PRESENTATION_*
      call.name = rilCallResponse[i].name;
      call.namePresentation = rilCallResponse[i].namePresentation;

      call.radioTech = Ci.nsITelephonyCallInfo.RADIO_TECH_CS;

      let rilUusInfos = rilCallResponse[i].getUusInfo();
      let uusInfos = {};
      for (let j = 0; j < rilUusInfos.length; j++) {
        let uusInfo = {};

        uusInfo.type = rilUusInfos[j].uusType;
        uusInfo.dcs = rilUusInfos[j].uusDcs;
        uusInfo.userData = rilUusInfos[j].uusData;
        uusInfos[j] = uusInfo;
      }

      call.uusinfo = uusInfos;

      //Add dummy values for upper layer
      call.rttMode = lazy.RIL.RTT_MODE_OFF;

      if (call.isVoice) {
        calls[call.callIndex] = call;
      }
    }

    lazy.gTelephonyService.notifyCurrentCalls(this.clientId, calls);
    return calls;
  },

  handleQueryCallForwardStatus(response) {
    let rilCallForwardStatus = response.getCallForwardStatus();
    if (DEBUG) {
      this.debug(
        "handleQueryCallForwardStatus " + JSON.stringify(rilCallForwardStatus)
      );
    }

    let result = {};

    let rulesLength = rilCallForwardStatus.length;
    if (!rulesLength) {
      result.errorMsg = lazy.RIL.GECKO_ERROR_GENERIC_FAILURE;
      return result;
    }

    let rules = new Array(rulesLength);
    for (let i = 0; i < rulesLength; i++) {
      let rule = {};
      rule.active = rilCallForwardStatus[i].status == 1; // CALL_FORWARD_STATUS_*
      rule.reason = rilCallForwardStatus[i].reason; // CALL_FORWARD_REASON_*
      rule.serviceClass = rilCallForwardStatus[i].serviceClass;
      rule.toa = rilCallForwardStatus[i].toa;
      rule.number = rilCallForwardStatus[i].number;
      rule.timeSeconds = rilCallForwardStatus[i].timeSeconds;
      rules[i] = rule;
    }
    result.rules = rules;
    return result;
  },

  handleSmscAddress(response) {
    let result = {};
    let tosca = lazy.RIL.TOA_UNKNOWN;
    let smsc = response.smsc;

    let segments = smsc.split(",", 2);
    console.log(
      "handleSmscAddress segments「[0] " +
        segments[0] +
        "segments[1] " +
        segments[1]
    );
    // Parse TOA only if it presents since some devices might omit the TOA
    // segment in the reported SMSC address. If TOA does not present, keep the
    // default value TOA_UNKNOWN.
    if (segments.length === 2) {
      tosca = parseInt(segments[1], 10);
    }
    smsc = segments[0].replace(/\"/g, "");

    // Convert the NPI value to the corresponding index of CALLED_PARTY_BCD_NPI
    // array. If the value does not present in the array, use
    // CALLED_PARTY_BCD_NPI_ISDN.
    let npi = lazy.RIL.CALLED_PARTY_BCD_NPI.indexOf(tosca & 0xf);
    if (npi === -1) {
      npi = lazy.RIL.CALLED_PARTY_BCD_NPI.indexOf(
        lazy.RIL.CALLED_PARTY_BCD_NPI_ISDN
      );
    }

    // Extract TON.
    let ton = (tosca & 0x70) >> 4;

    // Ensure + sign if TON is international, and vice versa.
    const TON_INTERNATIONAL = (lazy.RIL.TOA_INTERNATIONAL & 0x70) >> 4;
    if (ton === TON_INTERNATIONAL && smsc.charAt(0) !== "+") {
      smsc = "+" + smsc;
    } else if (smsc.charAt(0) === "+" && ton !== TON_INTERNATIONAL) {
      if (DEBUG) {
        this.debug(
          "SMSC address number begins with '+' while the TON is not international. Change TON to international."
        );
      }
      ton = TON_INTERNATIONAL;
    }

    result.smscAddress = smsc;
    result.typeOfNumber = ton;
    result.numberPlanIdentification = npi;
    return result;
  },

  handleAvailableNetworks(response) {
    let rilAvailableNetworks = response.getAvailableNetworks();
    let networks = [];
    for (let i = 0; i < rilAvailableNetworks.length; i++) {
      let network = {
        longName: rilAvailableNetworks[i].alphaLong,
        shortName: rilAvailableNetworks[i].alphaShort,
        mcc: null,
        mnc: null,
        state:
          lazy.RIL.RIL_QAN_STATE_TO_GECKO_STATE[rilAvailableNetworks[i].status],
      };

      let networkTuple = rilAvailableNetworks[i].operatorNumeric;
      try {
        this._processNetworkTuple(networkTuple, network);
      } catch (e) {
        if (DEBUG) {
          this.debug("Error processing operator tuple: " + e);
        }
      }

      networks.push(network);
    }
    return networks;
  },

  handleCellInfoList(response) {
    let rilCellInfoLists = response.getCellInfoList();
    let cellInfoLists = [];
    for (let i = 0; i < rilCellInfoLists.length; i++) {
      let cellInfo = {};
      switch (rilCellInfoLists[i].cellInfoType) {
        case lazy.RIL.CELL_INFO_TYPE_GSM:
          //nsICellIdentityGsm
          cellInfo.mcc = rilCellInfoLists[i].gsm.cellIdentityGsm.mcc;
          cellInfo.mnc = rilCellInfoLists[i].gsm.cellIdentityGsm.mnc;
          cellInfo.lac = rilCellInfoLists[i].gsm.cellIdentityGsm.lac;
          cellInfo.cid = rilCellInfoLists[i].gsm.cellIdentityGsm.cid;
          cellInfo.arfcn = rilCellInfoLists[i].gsm.cellIdentityGsm.arfcn;
          cellInfo.bsic = rilCellInfoLists[i].gsm.cellIdentityGsm.bsic;
          //nsIGsmSignalStrength
          cellInfo.signalStrength =
            rilCellInfoLists[i].gsm.signalStrengthGsm.signalStrength;
          cellInfo.bitErrorRate =
            rilCellInfoLists[i].gsm.signalStrengthGsm.bitErrorRate;
          cellInfo.timingAdvance =
            rilCellInfoLists[i].gsm.signalStrengthGsm.timingAdvance;
          break;

        case lazy.RIL.CELL_INFO_TYPE_WCDMA:
          //nsICellIdentityWcdma
          cellInfo.mcc = rilCellInfoLists[i].wcdma.cellIdentityWcdma.mcc;
          cellInfo.mnc = rilCellInfoLists[i].wcdma.cellIdentityWcdma.mnc;
          cellInfo.lac = rilCellInfoLists[i].wcdma.cellIdentityWcdma.lac;
          cellInfo.cid = rilCellInfoLists[i].wcdma.cellIdentityWcdma.cid;
          cellInfo.psc = rilCellInfoLists[i].wcdma.cellIdentityWcdma.psc;
          cellInfo.uarfcn = rilCellInfoLists[i].wcdma.cellIdentityWcdma.uarfcn;
          //nsIWcdmaSignalStrength
          cellInfo.signalStrength =
            rilCellInfoLists[i].wcdma.signalStrengthWcdma.signalStrength;
          cellInfo.bitErrorRate =
            rilCellInfoLists[i].wcdma.signalStrengthWcdma.bitErrorRate;
          break;

        case lazy.RIL.CELL_INFO_TYPE_LTE:
          //nsICellIdentityLte
          cellInfo.mcc = rilCellInfoLists[i].lte.cellIdentityLte.mcc;
          cellInfo.mnc = rilCellInfoLists[i].lte.cellIdentityLte.mnc;
          // Note: naming different between ril and mobileconnectionservice.
          cellInfo.cid = rilCellInfoLists[i].lte.cellIdentityLte.ci;
          cellInfo.pci = rilCellInfoLists[i].lte.cellIdentityLte.pci;
          cellInfo.tac = rilCellInfoLists[i].lte.cellIdentityLte.tac;
          cellInfo.earfcn = rilCellInfoLists[i].lte.cellIdentityLte.earfcn;
          //nsILteSignalStrength
          cellInfo.signalStrength =
            rilCellInfoLists[i].lte.signalStrengthLte.signalStrength;
          cellInfo.rsrp = rilCellInfoLists[i].lte.signalStrengthLte.rsrp;
          cellInfo.rsrq = rilCellInfoLists[i].lte.signalStrengthLte.rsrq;
          cellInfo.rssnr = rilCellInfoLists[i].lte.signalStrengthLte.rssnr;
          cellInfo.cqi = rilCellInfoLists[i].lte.signalStrengthLte.cqi;
          cellInfo.timingAdvance =
            rilCellInfoLists[i].lte.signalStrengthLte.timingAdvance;
          break;

        case lazy.RIL.CELL_INFO_TYPE_TDSCDMA:
          //nsICellIdentityTdScdma
          cellInfo.mcc = rilCellInfoLists[i].tdscdma.nsICellIdentityTdScdma.mcc;
          cellInfo.mnc = rilCellInfoLists[i].tdscdma.nsICellIdentityTdScdma.mnc;
          cellInfo.cid = rilCellInfoLists[i].tdscdma.nsICellIdentityTdScdma.ci;
          cellInfo.pci = rilCellInfoLists[i].tdscdma.nsICellIdentityTdScdma.pci;
          cellInfo.cpid =
            rilCellInfoLists[i].tdscdma.nsICellIdentityTdScdma.cpid;
          //nsITdScdmaSignalStrength
          cellInfo.rscp =
            rilCellInfoLists[i].tdscdma.signalStrengthTdScdma.rscp;
          break;

        case lazy.RIL.CELL_INFO_TYPE_CDMA:
          //nsICellIdentityCdma
          cellInfo.networkId =
            rilCellInfoLists[i].cdma.cellIdentityCdma.networkId;
          cellInfo.systemId =
            rilCellInfoLists[i].cdma.cellIdentityCdma.systemId;
          cellInfo.baseStationId =
            rilCellInfoLists[i].cdma.cellIdentityCdma.baseStationId;
          cellInfo.longitude =
            rilCellInfoLists[i].cdma.cellIdentityCdma.longitude;
          cellInfo.latitude =
            rilCellInfoLists[i].cdma.cellIdentityCdma.latitude;
          //nsICdmaSignalStrength
          // Note: naming different between ril and mobileconnectionservice.
          cellInfo.cdmaDbm = rilCellInfoLists[i].cdma.signalStrengthCdma.dbm;
          cellInfo.cdmaEcio = rilCellInfoLists[i].cdma.signalStrengthCdma.ecio;
          //nsIEvdoSignalStrength
          cellInfo.evdoDbm = rilCellInfoLists[i].cdma.signalStrengthEvdo.dbm;
          cellInfo.evdoEcio = rilCellInfoLists[i].cdma.signalStrengthEvdo.ecio;
          cellInfo.evdoSnr =
            rilCellInfoLists[i].cdma.signalStrengthEvdo.signalNoiseRatio;
          break;
      }
      cellInfoLists.push(cellInfo);
    }
    return cellInfoLists;
  },

  handleIccIoResult(response) {
    // Get the requset options.
    let token = response.rilMessageToken;
    let options = this.tokenOptionsMap[token];
    if (!options) {
      if (DEBUG) {
        this.debug(this.clientId + "Ignore orphan token: " + token);
      }
      return;
    }

    // Handle the icc io response
    let iccIoResult = response.iccIo;

    options.sw1 = iccIoResult.sw1;
    options.sw2 = iccIoResult.sw2;
    options.simResponse = iccIoResult.simResponse;
    // See 3GPP TS 11.11, clause 9.4.1 for operation success results.
    if (
      options.sw1 !== lazy.RIL.ICC_STATUS_NORMAL_ENDING &&
      options.sw1 !== lazy.RIL.ICC_STATUS_NORMAL_ENDING_WITH_EXTRA &&
      options.sw1 !== lazy.RIL.ICC_STATUS_WITH_SIM_DATA &&
      options.sw1 !== lazy.RIL.ICC_STATUS_WITH_RESPONSE_DATA
    ) {
      if (DEBUG) {
        this.debug(
          "ICC I/O Error EF id = 0x" +
            options.fileId.toString(16) +
            ", command = 0x" +
            options.command.toString(16) +
            ", sw1 = 0x" +
            options.sw1.toString(16) +
            ", sw2 = 0x" +
            options.sw2.toString(16)
        );
      }
      if (options.onerror) {
        // We can get fail cause from sw1/sw2 (See TS 11.11 clause 9.4.1 and
        // ISO 7816-4 clause 6). But currently no one needs this information,
        // so simply reports "GenericFailure" for now.
        options.onerror(lazy.RIL.GECKO_ERROR_GENERIC_FAILURE);
      }
      delete this.tokenOptionsMap[token];
      return;
    }
    this.simIOcontext.ICCIOHelper.processICCIO(options);

    // All finish delete option.
    delete this.tokenOptionsMap[token];
  },

  handleIccIoErrorResult(response) {
    // Get the requset options.
    let token = response.rilMessageToken;
    let options = this.tokenOptionsMap[token];
    if (!options) {
      if (DEBUG) {
        this.debug(this.clientId + "Ignore orphan token: " + token);
      }
      return;
    }

    if (options.onerror) {
      options.onerror(response.errorMsg);
    }

    // All finish delete option.
    delete this.tokenOptionsMap[token];
  },

  /**
   * Send arbitrary message to worker.
   *
   * @param rilMessageType
   *        A text message type.
   * @param message [optional]
   *        An optional message object to send.
   * @param callback [optional]
   *        An optional callback function which is called when worker replies
   *        with an message containing a "rilMessageToken" attribute of the
   *        same value we passed.  This callback function accepts only one
   *        parameter -- the reply from worker.  It also returns a boolean
   *        value true to keep current token-callback mapping and wait for
   *        another worker reply, or false to remove the mapping.
   */
  /* eslint-disable complexity */
  sendRilRequest(rilMessageType, message, callback) {
    message = message || {};

    message.rilMessageToken = this.token;
    // TODO what if meet the maximum value?
    this.token++;

    if (callback) {
      // Only create the map if callback is provided.  For sending a request
      // and intentionally leaving the callback undefined, that reply will
      // be dropped in |this.onmessage| because of that orphan token.
      //
      // For sending a request that never replied at all, we're fine with this
      // because no callback shall be passed and we leave nothing to be cleaned
      // up later.
      this.tokenCallbackMap[message.rilMessageToken] = callback;
    }
    message.rilMessageType = rilMessageType;

    // Handle the solic request by function then call the ril_worker.
    switch (message.rilMessageType) {
      case "getICCStatus":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_SIM_STATUS"
          );
        }
        this.rilworker.getIccCardStatus(message.rilMessageToken);
        break;
      case "iccUnlockCardLock":
      case "iccUnlockSubsidyLock":
        this.processIccUnlockCardLock(message);
        break;
      case "iccSetCardLockEnabled":
        this.processIccSetCardLockEnabled(message);
        break;
      case "iccGetCardLockEnabled":
        this.processIccGetCardLockEnabled(message);
        break;
      case "iccChangeCardLockPassword":
        this.processIccChangeCardLockPassword(message);
        break;
      case "iccGetCardLockRetryCount":
        this.processIccGetCardLockRetryCount(message);
        break;
      // For no sim case. OemHook api.
      case "iccGetSubsidyLockStatus":
        message.lockType = lazy.RIL.GECKO_CARDLOCK_ALL;
        this.processGetPersonalizationStatus(message);
        break;
      case "enterDepersonalization":
        break;
      case "getCurrentCalls":
        this.processGetCurrentCalls(message);
        break;
      case "dial":
        this.processRequestDial(message);
        break;
      case "getIMSI":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_IMSI aid = " +
              message.aid
          );
        }
        this.rilworker.getIMSI(
          message.rilMessageToken,
          message.aid || this.aid
        );
        break;
      case "hangUpCall":
        this.processHangUpCall(message);
        break;
      case "hangUpBackground":
        this.processHangUpBackgroundCall(message);
        break;
      case "hangUpForeground":
        this.processHangUpForegroundCall(message);
        break;
      case "switchActiveCall":
        this.processSwitchActiveCall(message);
        break;
      case "conferenceCall":
        this.processConferenceCall(message);
        break;
      case "udub":
        this.processRejectCall(message);
        break;
      case "explicitCallTransfer":
        this.processExplicitCallTransfer(message);
        break;
      case "getFailCause":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_LAST_CALL_FAIL_CAUSE"
          );
        }
        this.rilworker.getLastCallFailCause(message.rilMessageToken);
        break;
      case "getSignalStrength":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SIGNAL_STRENGTH"
          );
        }
        this.rilworker.getSignalStrength(message.rilMessageToken);
        break;
      case "getVoiceRegistrationState":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_VOICE_REGISTRATION_STATE"
          );
        }
        this.rilworker.getVoiceRegistrationState(message.rilMessageToken);
        break;
      case "getDataRegistrationState":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DATA_REGISTRATION_STATE"
          );
        }
        this.rilworker.getDataRegistrationState(message.rilMessageToken);
        break;
      case "getOperator":
        if (DEBUG) {
          this.debug(
            "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_OPERATOR"
          );
        }
        this.rilworker.getOperator(message.rilMessageToken);
        break;
      case "setRadioEnabled":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_RADIO_POWER on = " +
              message.enabled
          );
        }
        this.rilworker.setRadioPower(message.rilMessageToken, message.enabled);
        break;
      case "sendSMS":
        if (this._isCdma) {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + message.rilMessageToken + "] > REQUEST_SEND_CDMA_SMS"
            );
          }
          //FIXME
          //let CdmaPDUHelper = this.simIOcontext.CdmaPDUHelper;
          //CdmaPDUHelper.writeMessage(message);
          //this.rilworker.sendCdmaSMS(message.rilMessageToken, );
        } else {
          if (DEBUG) {
            this.debug(
              "RILJ: [" + message.rilMessageToken + "] > REQUEST_SEND_SMS"
            );
          }
          message.langIndex =
            message.langIndex || lazy.RIL.PDU_NL_IDENTIFIER_DEFAULT;
          message.langShiftIndex =
            message.langShiftIndex || lazy.RIL.PDU_NL_IDENTIFIER_DEFAULT;

          if (!message.segmentSeq) {
            // Fist segment to send
            message.segmentSeq = 1;
            message.body = message.segments[0].body;
            message.encodedBodyLength = message.segments[0].encodedBodyLength;
          }
          this._pendingSmsRequest[message.rilMessageToken] = message;
          let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
          GsmPDUHelper.initWith();
          GsmPDUHelper.writeMessage(message);
          this.rilworker.sendSMS(
            message.rilMessageToken,
            message.SMSC,
            GsmPDUHelper.pdu
          );
        }
        break;
      case "setupDataCall":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SETUP_DATA_CALL radioTechnology = " +
              message.radioTechnology +
              ", DataProfile = " +
              JSON.stringify(message.profile) +
              ", isRoaming = " +
              message.isRoaming +
              ", allowRoaming = " +
              message.allowRoaming
          );
        }
        this.rilworker.setupDataCall(
          message.rilMessageToken,
          message.radioTechnology,
          new DataProfile(message.profile),
          message.isRoaming,
          message.allowRoaming
        );
        break;
      case "iccIO":
        let data = null;
        if (
          message.command === lazy.RIL.ICC_COMMAND_UPDATE_RECORD &&
          message.dataWriter
        ) {
          data = message.dataWriter;
        }

        let pin2 = null;
        if (
          message.command === lazy.RIL.ICC_COMMAND_UPDATE_RECORD &&
          message.pin2
        ) {
          pin2 = message.pin2;
        }
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SIM_IO command = " +
              message.command +
              " , fileId = 0x" +
              message.fileId.toString(16) +
              " , path = " +
              message.pathId +
              " , p1 = " +
              message.p1 +
              " , p2 = " +
              message.p2 +
              " , p3 = " +
              message.p3 +
              " , data = " +
              data +
              " , pin2 = " +
              pin2 +
              " , aid = " +
              message.aid
          );
        }
        // Store the command options(message).
        this.tokenOptionsMap[message.rilMessageToken] = message;
        this.rilworker.iccIOForApp(
          message.rilMessageToken,
          message.command,
          message.fileId,
          message.pathId,
          message.p1,
          message.p2,
          message.p3,
          data,
          pin2,
          message.aid || this.aid
        );
        break;
      case "sendUSSD":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SEND_USSD ussd = " +
              message.ussd
          );
        }
        this.rilworker.sendUssd(message.rilMessageToken, message.ussd);
        break;
      case "cancelUSSD":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_CANCEL_USSD ussd = " +
              message.ussd
          );
        }
        this.rilworker.cancelPendingUssd(message.rilMessageToken);
        break;
      case "setCallBarring":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_CALLBARRING"
          );
        }
        this.processSetCallBarring(message);
        break;
      case "queryCallBarringStatus":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_CALLBARRING"
          );
        }
        this.processQueryCallBarringStatus(message);
        break;
      case "getCLIR":
        if (DEBUG) {
          this.debug(
            "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_GET_CLIR"
          );
        }
        this.rilworker.getClir(message.rilMessageToken);
        break;
      case "setCLIR":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_CLIR clirMode = " +
              message.clirMode
          );
        }
        this.rilworker.setClir(message.rilMessageToken, message.clirMode);
        break;
      case "queryCallForwardStatus":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_CALL_FORWARD_STATUS reason = " +
              message.reason +
              " , serviceClass = " +
              message.serviceClass +
              " , number = " +
              message.number
          );
        }
        let toaNumber = this._toaFromString(message.number);
        this.rilworker.getCallForwardStatus(
          message.rilMessageToken,
          message.reason,
          message.serviceClass,
          message.number || "",
          toaNumber || ""
        );
        break;
      case "setCallForward":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_CALL_FORWARD action = " +
              message.action +
              " , reason = " +
              message.reason +
              " , serviceClass = " +
              message.serviceClass +
              " , number = " +
              message.number +
              " , timeSeconds = " +
              message.timeSeconds
          );
        }
        let number = this._toaFromString(message.number);
        this._callForwardOptions.action = message.action;
        this._callForwardOptions.reason = message.reason;
        this._callForwardOptions.serviceClass = message.serviceClass;
        this._callForwardOptions.number = message.number || "";
        this._callForwardOptions.toaNumner = number || "";
        this._callForwardOptions.timeSeconds = message.timeSeconds;
        this.rilworker.setCallForwardStatus(
          message.rilMessageToken,
          message.action,
          message.reason,
          message.serviceClass,
          message.number || "",
          number || "",
          message.timeSeconds
        );
        break;
      case "queryCallWaiting":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_CALL_WAITING serviceClass = " +
              message.serviceClass
          );
        }
        this.rilworker.getCallWaiting(
          message.rilMessageToken,
          message.serviceClass
        );
        break;
      case "setCallWaiting":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_CALL_WAITING enable = " +
              message.enabled +
              " , serviceClass = " +
              message.serviceClass
          );
        }
        this.rilworker.setCallWaiting(
          message.rilMessageToken,
          message.enabled,
          message.serviceClass
        );
        break;
      case "ackSMS":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SMS_ACKNOWLEDGE result = " +
              message.result
          );
        }
        this.rilworker.acknowledgeLastIncomingGsmSms(
          message.rilMessageToken,
          message.result == lazy.RIL.PDU_FCS_OK,
          message.result
        );
        break;
      case "answerCall":
        this.processAnswerCall(message);
        break;
      case "deactivateDataCall":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DEACTIVATE_DATA_CALL"
          );
        }
        this.rilworker.deactivateDataCall(
          message.rilMessageToken,
          message.cid,
          message.reason
        );
        break;
      case "changeCallBarringPassword":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_CHANGE_BARRING_PASSWORD"
          );
        }
        this.rilworker.setBarringPassword(
          message.rilMessageToken,
          lazy.RIL.ICC_CB_FACILITY_BA_ALL,
          message.pin || "",
          message.newPin || ""
        );
        break;
      case "getNetworkSelectionMode":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE"
          );
        }
        this.rilworker.getNetworkSelectionMode(message.rilMessageToken);
        break;
      case "selectNetworkAuto":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC"
          );
        }
        this.rilworker.setNetworkSelectionModeAutomatic(
          message.rilMessageToken
        );
        break;
      case "selectNetwork":
        let numeric =
          message.mcc && message.mnc ? message.mcc + message.mnc : null;
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL numeric = " +
              numeric
          );
        }
        this.rilworker.setNetworkSelectionModeManual(
          message.rilMessageToken,
          numeric
        );
        break;
      case "getAvailableNetworks":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_AVAILABLE_NETWORKS"
          );
        }
        this.rilworker.getAvailableNetworks(message.rilMessageToken);
        break;
      case "sendTone":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DTMF dtmfChar = " +
              message.dtmfChar
          );
        }
        this.rilworker.sendDtmf(message.rilMessageToken, message.dtmfChar + "");
        break;
      case "startTone":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DTMF_START dtmfChar = " +
              message.dtmfChar
          );
        }
        this.rilworker.startDtmf(
          message.rilMessageToken,
          message.dtmfChar + ""
        );
        break;
      case "stopTone":
        if (DEBUG) {
          this.debug(
            "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_DTMF_STOP"
          );
        }
        this.rilworker.stopDtmf(message.rilMessageToken);
        break;
      case "getBasebandVersion":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_BASEBAND_VERSION"
          );
        }
        this.rilworker.getBasebandVersion(message.rilMessageToken);
        break;
      case "separateCall":
        this.processSeparateCall(message);
        break;
      case "setMute":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_MUTE enableMute = " +
              message.muted
          );
        }
        this.rilworker.setMute(message.rilMessageToken, message.muted);
        break;
      case "getMute":
        if (DEBUG) {
          this.debug(
            "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_GET_MUTE"
          );
        }
        this.rilworker.getMute(message.rilMessageToken);
        break;
      case "queryCLIP":
        if (DEBUG) {
          this.debug(
            "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_QUERY_CLIP "
          );
        }
        this.rilworker.getClip(message.rilMessageToken);
        break;
      case "getDataCallList":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DATA_CALL_LIST"
          );
        }
        this.rilworker.getDataCallList(message.rilMessageToken);
        break;
      case "writeSmsToSIM":
        break;
      case "setPreferredNetworkType":
        let networkType = message.type;
        if (
          networkType < 0 ||
          networkType >= lazy.RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO.length
        ) {
          message.errorMsg = lazy.RIL.GECKO_ERROR_INVALID_ARGUMENTS;
          this.handleRilResponse(message);
          return;
        }
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE networkType = " +
              networkType
          );
        }
        this.rilworker.setPreferredNetworkType(
          message.rilMessageToken,
          networkType
        );
        break;
      case "getPreferredNetworkType":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE"
          );
        }
        this.rilworker.getPreferredNetworkType(message.rilMessageToken);
        break;
      case "getNeighboringCellIds":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_NEIGHBORING_CELL_IDS"
          );
        }
        this.rilworker.getNeighboringCids(message.rilMessageToken);
        break;
      case "setRoamingPreference":
        break;
      case "queryRoamingPreference":
        break;
      case "setTtyMode":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_TTY_MODE ttyMode = " +
              message.ttyMode
          );
        }
        this.rilworker.setTTYMode(message.rilMessageToken, message.ttyMode);
        break;
      case "queryTtyMode":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_QUERY_TTY_MODE"
          );
        }
        this.rilworker.queryTTYMode(message.rilMessageToken);
        break;
      case "setVoicePrivacyMode":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE enabled = " +
              message.enabled
          );
        }
        this.rilworker.setPreferredVoicePrivacy(
          message.rilMessageToken,
          message.enabled
        );
        break;
      case "queryVoicePrivacyMode":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE"
          );
        }
        this.rilworker.getPreferredVoicePrivacy(message.rilMessageToken);
        break;
      case "cdmaFlash":
        break;
      case "acknowledgeCdmaSms":
        break;
      case "setGsmBroadcastConfig":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GSM_SET_BROADCAST_CONFIG config = " +
              message.config
          );
        }
        this.rilworker.setGsmBroadcastConfig(
          message.rilMessageToken,
          message.config
        );
        break;
      case "setGsmBroadcastActivation":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GSM_BROADCAST_ACTIVATION activate = " +
              message.activate
          );
        }
        this.rilworker.setGsmBroadcastActivation(
          message.rilMessageToken,
          message.activate
        );
        break;
      case "setCdmaSmsBroadcastConfig":
        break;
      case "getCdmaSubscription":
        break;
      case "getDeviceIdentity":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_DEVICE_IDENTITY"
          );
        }
        this.rilworker.getDeviceIdentity(message.rilMessageToken);
        break;
      case "sendExitEmergencyCbModeRequest":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE"
          );
        }
        this.rilworker.exitEmergencyCallbackMode(message.rilMessageToken);
        break;
      case "getSmscAddress":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_SMSC_ADDRESS"
          );
        }
        this.rilworker.getSmscAddress(message.rilMessageToken);
        break;
      case "setSmscAddress":
        this.processSetSmscAddress(message);
        break;
      case "reportSmsMemoryStatus":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_REPORT_SMS_MEMORY_STATUS"
          );
        }
        this.rilworker.reportSmsMemoryStatus(
          message.rilMessageToken,
          message.isAvailable
        );
        break;
      case "reportStkServiceIsRunning":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING"
          );
        }
        this.rilworker.reportStkServiceIsRunning(message.rilMessageToken);
        break;
      case "acknowledgeIncomingGsmSmsWithPDU":
        break;
      case "dataDownloadViaSMSPP":
        break;
      case "getVoiceRadioTechnology":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_VOICE_RADIO_TECH"
          );
        }
        this.rilworker.getVoiceRadioTechnology(message.rilMessageToken);
        break;
      case "getCellInfoList":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_CELL_INFO_LIST"
          );
        }
        this.rilworker.getCellInfoList(message.rilMessageToken);
        break;
      case "setInitialAttachApn":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_INITIAL_ATTACH_APN profile = " +
              JSON.stringify(message.profile)
          );
        }
        this.rilworker.setInitialAttachApn(
          message.rilMessageToken,
          new DataProfile(message.profile),
          message.isRoaming
        );
        break;
      case "setDataProfile":
        this.processSetDataProfile(message);
        break;
      case "iccOpenChannel":
        break;
      case "iccCloseChannel":
        break;
      case "iccExchangeAPDU":
        break;
      case "setUiccSubscription":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_UICC_SUBSCRIPTION slotId = " +
              message.slotId +
              " appIndex = " +
              message.appIndex +
              " subId = " +
              message.subId +
              " subStatus = " +
              message.subStatus
              ? 1
              : 0
          );
        }
        this.rilworker.setUiccSubscription(
          message.rilMessageToken,
          message.serial,
          message.slotId,
          message.appIndex,
          message.subId,
          message.subStatus ? 1 : 0
        );
        break;
      case "setDataRegistration":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_ALLOW_DATA allowed = " +
              message.attach
          );
        }
        this.rilworker.setDataAllowed(message.rilMessageToken, message.attach);
        break;
      case "getIccAuthentication":
        this.processGetIccAuthentication(message);
        break;
      case "setCellInfoListRate":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_CELL_INFO_LIST_RATE rateInMillis = " +
              message.rateInMillis
          );
        }
        if (message.rateInMillis) {
          this.rilworker.setCellInfoListRate(
            message.rilMessageToken,
            message.rateInMillis
          );
        } else {
          this.rilworker.setCellInfoListRate(message.rilMessageToken);
        }
        break;
      case "setSuppServiceNotifications":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION enable = " +
              message.enable
          );
        }
        this.rilworker.setSuppServiceNotifications(
          message.rilMessageToken,
          message.enable
        );
        break;
      case "stkHandleCallSetup":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM accept = " +
              message.hasConfirmed
          );
        }
        this.rilworker.handleStkCallSetupRequestFromSim(
          message.rilMessageToken,
          message.hasConfirmed
        );
        break;
      case "sendStkTerminalResponse":
        this.processSendStkTerminalResponse(message);
        break;
      case "sendStkMenuSelection":
        this.processSendStkMenuSelection(message);
        break;
      case "sendStkTimerExpiration":
        this.processSendStkTimerExpiration(message);
        break;
      case "sendStkEventDownload":
        this.processSendStkEventDownload(message);
        break;
      case "setCellBroadcastDisabled":
        // This is not a ril request.
        this.setCellBroadcastDisabled(message);
        break;
      case "setCellBroadcastSearchList":
        // This is not a ril request.
        this.setCellBroadcastSearchList(message);
        break;
      case "getRadioCapability":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_GET_RADIO_CAPABILITY"
          );
        }
        this.rilworker.getRadioCapability(message.rilMessageToken);
        break;
      case "updateICCContact":
        // This is not a ril request.
        this.updateICCContact(message);
        break;
      case "readICCContacts":
        // This is not a ril request.
        this.readICCContacts(message);
        break;
      //TODO: Refactor and simplify this checking mechanism.
      case "getMaxContactCount":
        // This is not a ril request.
        this.getMaxContactCount(message);
        break;
      case "getIccServiceState":
        // This is not a ril request.
        this.getIccServiceState(message);
        break;
      case "stopNetworkScan":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_STOP_NETWORK_SCAN"
          );
        }
        this.rilworker.stopNetworkScan(message.rilMessageToken);
        break;
      case "setUnsolResponseFilter":
        if (DEBUG) {
          this.debug(
            "RILJ: [" +
              message.rilMessageToken +
              "] > RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER filter: " +
              message.filter
          );
        }
        this.rilworker.setUnsolResponseFilter(
          message.rilMessageToken,
          message.filter
        );
        break;
      default:
        this.debug(message.rilMessageType + " not supported.");
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
        break;
    }
  },
  /* eslint-enable complexity */

  /**
   * Helper for returning the TOA for the given dial string.
   */
  _toaFromString(number) {
    let toa = lazy.RIL.TOA_UNKNOWN;
    if (number && number.length > 0 && number[0] == "+") {
      toa = lazy.RIL.TOA_INTERNATIONAL;
    }
    return toa;
  },

  processRequestDial(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_DIAL address = " +
          message.number +
          " , clirMode = " +
          message.clirMode +
          " , uusInfo = " +
          message.uusInfo
      );
    }

    if (!message.isEmergency && this._isInEmergencyCbMode) {
      // Exit emergency callback mode when user dial a non-emergency call.
      this.exitEmergencyCbMode();
    }

    this.telephonyRequestQueue.push("dial", () => {
      this.rilworker.requestDial(
        message.rilMessageToken,
        message.number || "",
        message.clirMode,
        message.uusInfo ? message.uusInfo.uusType || 0 : 0,
        message.uusInfo ? message.uusInfo.uusDcs || 0 : 0,
        message.uusInfo ? message.uusInfo.uusData || "" : ""
      );
    });
  },

  processHangUpCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_HANGUP callIndex = " +
          message.callIndex
      );
    }
    this.telephonyRequestQueue.push("hangUpCall", () => {
      this.rilworker.hangupConnection(
        message.rilMessageToken,
        message.callIndex
      );
    });
  },

  processAnswerCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_ANSWER"
      );
    }
    this.telephonyRequestQueue.push("answerCall", () => {
      this.rilworker.acceptCall(message.rilMessageToken, message.callIndex);
    });
  },

  processHangUpBackgroundCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND"
      );
    }
    this.telephonyRequestQueue.push("hangUpBackground", () => {
      this.rilworker.hangupWaitingOrBackground(message.rilMessageToken);
    });
  },

  processHangUpForegroundCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND"
      );
    }
    this.telephonyRequestQueue.push("hangUpForeground", () => {
      this.rilworker.hangupForegroundResumeBackground(message.rilMessageToken);
    });
  },

  processSwitchActiveCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE"
      );
    }
    this.telephonyRequestQueue.push("switchActiveCall", () => {
      this.rilworker.switchWaitingOrHoldingAndActive(message.rilMessageToken);
    });
  },

  processConferenceCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_CONFERENCE"
      );
    }
    this.telephonyRequestQueue.push("conferenceCall", () => {
      this.rilworker.conference(message.rilMessageToken);
    });
  },

  processRejectCall(message) {
    if (DEBUG) {
      this.debug("RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_UDUB");
    }
    this.telephonyRequestQueue.push("udub", () => {
      this.rilworker.rejectCall(message.rilMessageToken);
    });
  },

  processExplicitCallTransfer(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_EXPLICIT_CALL_TRANSFER"
      );
    }
    this.telephonyRequestQueue.push("explicitCallTransfer", () => {
      this.rilworker.explicitCallTransfer(message.rilMessageToken);
    });
  },

  processGetIccAuthentication(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_SIM_AUTHENTICATION"
      );
    }
    // We should use record helper to process according to options.appType.
    message.aid = this._getAidByAppType(message.appType);
    this.rilworker.requestIccSimAuthentication(
      message.rilMessageToken,
      message.authType,
      message.data,
      message.aid
    );
  },

  processSeparateCall(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_SEPARATE_CONNECTION"
      );
    }
    this.telephonyRequestQueue.push("separateCall", () => {
      this.rilworker.separateConnection(
        message.rilMessageToken,
        message.callIndex
      );
    });
  },

  processGetCurrentCalls(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_GET_CURRENT_CALLS"
      );
    }
    this.telephonyRequestQueue.push("getCurrentCalls", () => {
      this.rilworker.getCurrentCalls(message.rilMessageToken);
    });
  },

  _getAidByAppType(appType) {
    switch (appType) {
      case lazy.RIL.CARD_APPTYPE_SIM:
      case lazy.RIL.CARD_APPTYPE_USIM:
        return this.simIOcontext.SimRecordHelper.aid;
      case lazy.RIL.CARD_APPTYPE_RUIM:
      case lazy.RIL.CARD_APPTYPE_CSIM:
        return this.simIOcontext.RuimRecordHelper.aid;
      case lazy.RIL.CARD_APPTYPE_ISIM:
        return this.simIOcontext.ISimRecordHelper.aid;
    }

    return this.aid;
  },

  /**
   * Queries current call barring rules.
   *
   * @param program
   *        One of CALL_BARRING_PROGRAM_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   */
  processQueryCallBarringStatus(message) {
    message.facility =
      lazy.RIL.CALL_BARRING_PROGRAM_TO_FACILITY[message.program];
    message.password = ""; // For query no need to provide it.

    // For some operators, querying specific serviceClass doesn't work. We use
    // serviceClass 0 instead, and then process the response to extract the
    // answer for queryServiceClass.
    message.queryServiceClass = message.serviceClass;
    message.serviceClass = 0;

    this.queryICCFacilityLock(message);
  },

  /**
   * Configures call barring rule.
   *
   * @param program
   *        One of CALL_BARRING_PROGRAM_* constants.
   * @param enabled
   *        Enable or disable the call barring.
   * @param password
   *        Barring password.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   */
  processSetCallBarring(message) {
    message.facility =
      lazy.RIL.CALL_BARRING_PROGRAM_TO_FACILITY[message.program];
    this.setICCFacilityLock(message);
  },

  /**
   * Helper function for unlocking ICC locks.
   */
  processIccUnlockCardLock(message) {
    switch (message.lockType) {
      case lazy.RIL.GECKO_CARDLOCK_PIN:
        this.enterICCPIN(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_PIN2:
        this.enterICCPIN2(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_PUK:
        this.enterICCPUK(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_PUK2:
        this.enterICCPUK2(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_NCK:
      case lazy.RIL.GECKO_CARDLOCK_NSCK:
      case lazy.RIL.GECKO_CARDLOCK_CCK:
      case lazy.RIL.GECKO_CARDLOCK_SPCK:
        this.enterDepersonalizationReq(message);
        break;
      default:
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
    }
  },

  enterICCPIN(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_ENTER_SIM_PIN pin = " +
          message.password +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.supplyIccPinForApp(
      message.rilMessageToken,
      message.password || "",
      this.aid
    );
  },

  enterICCPIN2(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_ENTER_SIM_PIN2 pin = " +
          message.password +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.supplyIccPin2ForApp(
      message.rilMessageToken,
      message.password || "",
      this.aid
    );
  },

  enterICCPUK(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_ENTER_SIM_PUK puk = " +
          message.password +
          " , newPin = " +
          message.newPin +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.supplyIccPukForApp(
      message.rilMessageToken,
      message.password || "",
      message.newPin || "",
      this.aid
    );
  },

  enterICCPUK2(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_ENTER_SIM_PUK2 pin = " +
          message.password +
          " , newPin = " +
          message.newPin +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.supplyIccPuk2ForApp(
      message.rilMessageToken,
      message.password || "",
      message.newPin || "",
      this.aid
    );
  },

  setICCFacilityLock(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_SET_FACILITY_LOCK facility = " +
          message.facility +
          " , enabled = " +
          message.enabled +
          " , password = " +
          message.password +
          " , serviceClass = " +
          message.serviceClass +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.setFacilityLockForApp(
      message.rilMessageToken,
      message.facility || "",
      message.enabled,
      message.password || "",
      message.serviceClass,
      this.aid
    );
  },

  // OemHook api
  enterDepersonalizationReq(message) {
    this.getOemHook();
    if (this.oemHook) {
      if (DEBUG) {
        this.debug(
          "OEMHOOK: [" +
            message.rilMessageToken +
            "] > OEMHOOK_ENTER_DEPERSONALIZATION_REQ  lockType = " +
            message.lockType +
            " , password = " +
            message.password
        );
      }
      this.oemHook.enterDepersonalizationReq(
        message.rilMessageToken,
        message.lockType,
        message.password
      );
    } else {
      this.debug("can not get OemHook");
      message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
      this.handleRilResponse(message);
    }
  },

  /**
   * Helper function for setting the state of ICC locks.
   */
  processIccSetCardLockEnabled(message) {
    switch (message.lockType) {
      case lazy.RIL.GECKO_CARDLOCK_PIN: // Fall through.
      case lazy.RIL.GECKO_CARDLOCK_FDN:
        message.facility =
          lazy.RIL.GECKO_CARDLOCK_TO_FACILITY[message.lockType];
        break;
      default:
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
        return;
    }

    message.serviceClass =
      lazy.RIL.ICC_SERVICE_CLASS_VOICE |
      lazy.RIL.ICC_SERVICE_CLASS_DATA |
      lazy.RIL.ICC_SERVICE_CLASS_FAX;
    this.setICCFacilityLock(message);
  },

  /**
   * Helper function for fetching the state of ICC locks.
   */
  processIccGetCardLockEnabled(message) {
    switch (message.lockType) {
      case lazy.RIL.GECKO_CARDLOCK_PIN:
      case lazy.RIL.GECKO_CARDLOCK_FDN:
        message.facility =
          lazy.RIL.GECKO_CARDLOCK_TO_FACILITY[message.lockType];
        message.password = ""; // For query no need to provide pin.
        message.serviceClass =
          lazy.RIL.ICC_SERVICE_CLASS_VOICE |
          lazy.RIL.ICC_SERVICE_CLASS_DATA |
          lazy.RIL.ICC_SERVICE_CLASS_FAX;
        this.queryICCFacilityLock(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_NCK:
      case lazy.RIL.GECKO_CARDLOCK_NSCK:
      case lazy.RIL.GECKO_CARDLOCK_CCK:
      case lazy.RIL.GECKO_CARDLOCK_SPCK:
        this.processGetPersonalizationStatus(message);
        break;
      default:
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
    }
  },

  queryICCFacilityLock(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_QUERY_FACILITY_LOCK facility = " +
          message.facility +
          " , password = " +
          message.password +
          " , serviceClass = " +
          message.serviceClass +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.getFacilityLockForApp(
      message.rilMessageToken,
      message.facility || "",
      message.password || "",
      message.serviceClass,
      this.aid
    );
  },

  // OemHook api
  processGetPersonalizationStatus(message) {
    this.getOemHook();
    if (this.oemHook) {
      if (DEBUG) {
        this.debug(
          "OEMHOOK: [" +
            message.rilMessageToken +
            "] > OEMHOOK_PERSONALIZATION_STATUS_REQ  lockType = " +
            message.lockType
        );
      }
      this.oemHook.personalizationStatusReq(
        message.rilMessageToken,
        message.lockType
      );
    } else {
      this.debug("can not get OemHook");
      message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
      this.handleRilResponse(message);
    }
  },

  /**
   * Helper function for changing ICC locks.
   */
  processIccChangeCardLockPassword(message) {
    switch (message.lockType) {
      case lazy.RIL.GECKO_CARDLOCK_PIN:
        this.changeICCPIN(message);
        break;
      case lazy.RIL.GECKO_CARDLOCK_PIN2:
        this.changeICCPIN2(message);
        break;
      default:
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
    }
  },

  changeICCPIN(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_CHANGE_SIM_PIN old = " +
          message.password +
          " , new = " +
          message.newPassword +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.changeIccPinForApp(
      message.rilMessageToken,
      message.password || "",
      message.newPassword || "",
      this.aid
    );
  },

  changeICCPIN2(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_CHANGE_SIM_PIN2 old = " +
          message.password +
          " , new = " +
          message.newPassword +
          " , aid = " +
          this.aid
      );
    }
    this.rilworker.changeIccPin2ForApp(
      message.rilMessageToken,
      message.password || "",
      message.newPassword || "",
      this.aid
    );
  },

  /**
   * Helper function for fetching the number of unlock retries of ICC locks.
   *
   * We only query the retry count when we're on the emulator. The phones do
   * not support the request id and their rild doesn't return an error.
   */
  processIccGetCardLockRetryCount(message) {
    this.debug(
      "Current ril do not support processIccGetCardLockRetryCount api."
    );
    message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
    this.handleRilResponse(message);

    /*if (!lazy.RIL.RILQUIRKS_HAVE_QUERY_ICC_LOCK_RETRY_COUNT) {
      // Only the emulator supports this request.
      message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
      this.handleRilResponse(message);
      return;
    }

    switch (message.lockType) {
      case lazy.RIL.GECKO_CARDLOCK_PIN:
      case lazy.RIL.GECKO_CARDLOCK_PIN2:
      case lazy.RIL.GECKO_CARDLOCK_PUK:
      case lazy.RIL.GECKO_CARDLOCK_PUK2:
      case lazy.RIL.GECKO_CARDLOCK_NCK:
      case lazy.RIL.GECKO_CARDLOCK_NSCK:
      case lazy.RIL.GECKO_CARDLOCK_CCK: // Fall through.
      case lazy.RIL.GECKO_CARDLOCK_SPCK:
      // TODO: Bug 1116072: identify the mapping between RIL_PERSOSUBSTATE_SIM_SIM
      //       @ ril.h and TS 27.007, clause 8.65 for GECKO_CARDLOCK_SPCK.
        message.selCode = lazy.RIL.GECKO_CARDLOCK_TO_SEL_CODE[message.lockType];
        break;
      default:
        message.errorMsg = lazy.RIL.GECKO_ERROR_REQUEST_NOT_SUPPORTED;
        this.handleRilResponse(message);
        return;
    }*/
    // New ril do not support this REQUEST_GET_UNLOCK_RETRY_COUNT command.
    //this.queryICCLockRetryCount(message);
  },

  processSetSmscAddress(message) {
    if (DEBUG) {
      this.debug(
        "RILJ: [" + message.rilMessageToken + "] > RIL_REQUEST_SET_SMSC_ADDRESS"
      );
    }
    let ton = message.typeOfNumber;
    let npi = lazy.RIL.CALLED_PARTY_BCD_NPI[message.numberPlanIdentification];

    // If any of the mandatory arguments is not available, return an error
    // immediately.
    if (ton === undefined || npi === undefined || !message.smscAddress) {
      message.errorMsg = lazy.RIL.GECKO_ERROR_INVALID_ARGUMENTS;
      //this.sendChromeMessage(options);
      return;
    }

    // Remove all illegal characters in the number string for user-input fault
    // tolerance.
    let tosca = (0x1 << 7) + (ton << 4) + npi;
    let numStart = message.smscAddress[0] === "+" ? 1 : 0;
    let number =
      message.smscAddress.substring(0, numStart) +
      message.smscAddress.substring(numStart).replace(/[^0-9*#abc]/gi, "");

    // If the filtered number is an empty string, return an error immediately.
    if (number.length === 0) {
      message.errorMsg = lazy.RIL.GECKO_ERROR_INVALID_ARGUMENTS;
      //this.sendChromeMessage(message);
      return;
    }

    let sca;
    sca = '"' + number + '",' + tosca;
    this.rilworker.setSmscAddress(message.rilMessageToken, sca);
  },

  /**
   * Send STK terminal response.
   *
   * @param command
   * @param deviceIdentities
   * @param resultCode
   * @param [optional] additionalInformation
   * @param [optional] itemIdentifier
   * @param [optional] input
   * @param [optional] isYesNo
   * @param [optional] hasConfirmed
   * @param [optional] localInfo
   * @param [optional] timer
   */
  processSendStkTerminalResponse(message) {
    if (message.hasConfirmed !== undefined) {
      this.sendRilRequest("stkHandleCallSetup", message);
      return;
    }

    if (message.command.typeOfCommand === lazy.RIL.STK_CMD_OPEN_CHANNEL) {
      message.hasConfirmed = message.isYesNo;
      this.sendRilRequest("stkHandleCallSetup", message);
      return;
    }

    let ComprehensionTlvHelper = this.simIOcontext.ComprehensionTlvHelper;
    let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
    GsmPDUHelper.initWith();

    let command = message.command;
    // Command Details
    GsmPDUHelper.writeHexOctet(
      lazy.RIL.COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
        lazy.RIL.COMPREHENSIONTLV_FLAG_CR
    );
    GsmPDUHelper.writeHexOctet(3);
    if (command) {
      GsmPDUHelper.writeHexOctet(command.commandNumber);
      GsmPDUHelper.writeHexOctet(command.typeOfCommand);
      GsmPDUHelper.writeHexOctet(command.commandQualifier);
    } else {
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
    }

    // Device Identifier
    // According to TS102.223/TS31.111 section 6.8 Structure of
    // TERMINAL RESPONSE, "For all SIMPLE-TLV objects with Min=N,
    // the ME should set the CR(comprehension required) flag to
    // comprehension not required.(CR=0)"
    // Since DEVICE_IDENTITIES and DURATION TLVs have Min=N,
    // the CR flag is not set.
    GsmPDUHelper.writeHexOctet(lazy.RIL.COMPREHENSIONTLV_TAG_DEVICE_ID);
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(lazy.RIL.STK_DEVICE_ID_ME);
    GsmPDUHelper.writeHexOctet(lazy.RIL.STK_DEVICE_ID_SIM);

    // Result
    GsmPDUHelper.writeHexOctet(
      lazy.RIL.COMPREHENSIONTLV_TAG_RESULT | lazy.RIL.COMPREHENSIONTLV_FLAG_CR
    );
    if ("additionalInformation" in message) {
      // In |12.12 Result| TS 11.14, the length of additional information is
      // varied and all possible values are addressed in 12.12.1-11 of TS 11.14
      // and 8.12.1-13 in TS 31.111.
      // However,
      // 1. Only SEND SS requires info with more than 1 octet.
      // 2. In rild design, SEND SS is expected to be handled by modem and
      //    UNSOLICITED_STK_EVENT_NOTIFY will be sent to application layer to
      //    indicate appropriate messages to users. TR is not required in this
      //    case.
      // Hence, we simplify the structure of |additionalInformation| to a
      // numeric value instead of a octet array.
      GsmPDUHelper.writeHexOctet(2);
      GsmPDUHelper.writeHexOctet(message.resultCode);
      GsmPDUHelper.writeHexOctet(message.additionalInformation);
    } else {
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(message.resultCode);
    }

    // Item Identifier
    if (message.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_ITEM_ID |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(message.itemIdentifier);
    }

    // No need to process Text data if user requests help information.
    if (
      message.resultCode != lazy.RIL.STK_RESULT_HELP_INFO_REQUIRED &&
      command &&
      command.options
    ) {
      let coding = command.options.isUCS2
        ? lazy.RIL.STK_TEXT_CODING_UCS2
        : command.options.isPacked;

      coding = coding
        ? lazy.RIL.STK_TEXT_CODING_GSM_7BIT_PACKED
        : lazy.RIL.STK_TEXT_CODING_GSM_8BIT;
      if (message.isYesNo !== undefined) {
        // Tag: GET_INKEY
        // When the ME issues a successful TERMINAL RESPONSE for a GET INKEY
        // ("Yes/No") command with command qualifier set to "Yes/No", it shall
        // supply the value '01' when the answer is "positive" and the value
        // '00' when the answer is "negative" in the Text string data object.
        GsmPDUHelper.writeHexOctet(
          lazy.RIL.COMPREHENSIONTLV_TAG_TEXT_STRING |
            lazy.RIL.COMPREHENSIONTLV_FLAG_CR
        );
        // Length: 2
        GsmPDUHelper.writeHexOctet(2);
        // Value: Coding, Yes/No.
        GsmPDUHelper.writeHexOctet(coding);
        GsmPDUHelper.writeHexOctet(message.isYesNo ? 0x01 : 0x00);
      } else if (message.input !== undefined) {
        ComprehensionTlvHelper.writeTextStringTlv(message.input, coding);
      }
    }

    // Duration
    if (message.resultCode === lazy.RIL.STK_RESULT_NO_RESPONSE_FROM_USER) {
      // In TS102 223, 6.4.2 GET INKEY, "if the UICC requests a variable timeout,
      // the terminal shall wait until either the user enters a single character
      // or the timeout expires. The timer starts when the text is displayed on
      // the screen and stops when the TERMINAL RESPONSE is sent. The terminal
      // shall pass the total display text duration (command execution duration)
      // to the UICC using the TERMINAL RESPONSE. The time unit of the response
      // is identical to the time unit of the requested variable timeout."
      let duration = command && command.options && command.options.duration;
      if (duration) {
        GsmPDUHelper.writeHexOctet(lazy.RIL.COMPREHENSIONTLV_TAG_DURATION);
        GsmPDUHelper.writeHexOctet(2);
        GsmPDUHelper.writeHexOctet(duration.timeUnit);
        GsmPDUHelper.writeHexOctet(duration.timeInterval);
      }
    }

    // Local Information
    if (message.localInfo) {
      let localInfo = message.localInfo;

      // Location Infomation
      if (localInfo.locationInfo) {
        ComprehensionTlvHelper.writeLocationInfoTlv(localInfo.locationInfo);
      }

      // IMEI
      if (localInfo.imei != null) {
        let imei = localInfo.imei;
        if (imei.length == 15) {
          imei = imei + "0";
        }

        GsmPDUHelper.writeHexOctet(lazy.RIL.COMPREHENSIONTLV_TAG_IMEI);
        GsmPDUHelper.writeHexOctet(8);
        for (let i = 0; i < imei.length / 2; i++) {
          GsmPDUHelper.writeHexOctet(parseInt(imei.substr(i * 2, 2), 16));
        }
      }

      // Date and Time Zone
      if (localInfo.date != null) {
        ComprehensionTlvHelper.writeDateTimeZoneTlv(localInfo.date);
      }

      // Language
      if (localInfo.language) {
        ComprehensionTlvHelper.writeLanguageTlv(localInfo.language);
      }
    }

    // Timer
    if (message.timer) {
      let timer = message.timer;

      if (timer.timerId) {
        GsmPDUHelper.writeHexOctet(
          lazy.RIL.COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER
        );
        GsmPDUHelper.writeHexOctet(1);
        GsmPDUHelper.writeHexOctet(timer.timerId);
      }

      if (timer.timerValue) {
        ComprehensionTlvHelper.writeTimerValueTlv(timer.timerValue, false);
      }
    }

    //Send command.
    let contents = GsmPDUHelper.pdu;
    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE contents = " +
          contents
      );
    }
    this.rilworker.sendTerminalResponseToSim(message.rilMessageToken, contents);
  },

  /**
   * Send STK Envelope(Menu Selection) command.
   *
   * @param message
   *          itemIdentifier
   *          helpRequested
   */
  processSendStkMenuSelection(message) {
    message.tag = lazy.RIL.BER_MENU_SELECTION_TAG;
    message.deviceId = {
      sourceId: lazy.RIL.STK_DEVICE_ID_KEYPAD,
      destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
    };
    this.sendICCEnvelopeCommand(message);
  },

  /**
   * Send STK Envelope(Timer Expiration) command.
   *
   * @param message
   *          timer
   */
  processSendStkTimerExpiration(message) {
    message.tag = lazy.RIL.BER_TIMER_EXPIRATION_TAG;
    message.deviceId = {
      sourceId: lazy.RIL.STK_DEVICE_ID_ME,
      destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
    };
    message.timerId = message.timer.timerId;
    message.timerValue = message.timer.timerValue;
    this.sendICCEnvelopeCommand(message);
  },

  /**
   * Send STK Envelope(Event Download) command.
   * @param message
   *          event
   */
  processSendStkEventDownload(message) {
    message.tag = lazy.RIL.BER_EVENT_DOWNLOAD_TAG;
    message.eventList = message.event.eventType;
    switch (message.eventList) {
      case lazy.RIL.STK_EVENT_TYPE_LOCATION_STATUS:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_ME,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        message.locationStatus = message.event.locationStatus;
        // Location info should only be provided when locationStatus is normal.
        if (message.locationStatus == lazy.RIL.STK_SERVICE_STATE_NORMAL) {
          message.locationInfo = message.event.locationInfo;
        }
        break;
      case lazy.RIL.STK_EVENT_TYPE_MT_CALL:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_NETWORK,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        message.transactionId = 0;
        message.address = message.event.number;
        break;
      case lazy.RIL.STK_EVENT_TYPE_CALL_DISCONNECTED:
        message.cause = message.event.error;
      // Fall through.
      case lazy.RIL.STK_EVENT_TYPE_CALL_CONNECTED:
        message.deviceId = {
          sourceId: message.event.isIssuedByRemote
            ? lazy.RIL.STK_DEVICE_ID_NETWORK
            : lazy.RIL.STK_DEVICE_ID_ME,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        message.transactionId = 0;
        break;
      case lazy.RIL.STK_EVENT_TYPE_USER_ACTIVITY:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_ME,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        break;
      case lazy.RIL.STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_DISPLAY,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        break;
      case lazy.RIL.STK_EVENT_TYPE_LANGUAGE_SELECTION:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_ME,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        message.language = message.event.language;
        break;
      case lazy.RIL.STK_EVENT_TYPE_BROWSER_TERMINATION:
        message.deviceId = {
          sourceId: lazy.RIL.STK_DEVICE_ID_ME,
          destinationId: lazy.RIL.STK_DEVICE_ID_SIM,
        };
        message.terminationCause = message.event.terminationCause;
        break;
    }
    this.sendICCEnvelopeCommand(message);
  },

  /**
   * Send REQUEST_STK_SEND_ENVELOPE_COMMAND to ICC.
   *
   * @param options
   *          tag
   *          deviceId
   *          [optioanl] itemIdentifier
   *          [optional] helpRequested
   *          [optional] eventList
   *          [optional] locationStatus
   *          [optional] locationInfo
   *          [optional] address
   *          [optional] transactionId
   *          [optional] cause
   *          [optional] timerId
   *          [optional] timerValue
   *          [optional] terminationCause
   */
  sendICCEnvelopeCommand(options) {
    if (DEBUG) {
      this.debug("Stk Envelope " + JSON.stringify(options));
    }

    let ComprehensionTlvHelper = this.simIOcontext.ComprehensionTlvHelper;
    let GsmPDUHelper = this.simIOcontext.GsmPDUHelper;
    GsmPDUHelper.initWith();

    // Write a BER-TLV
    GsmPDUHelper.writeHexOctet(options.tag);
    GsmPDUHelper.startCalOutgoingSize();

    // Event List
    if (options.eventList != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_EVENT_LIST |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.eventList);
    }

    // Device Identifies
    GsmPDUHelper.writeHexOctet(
      lazy.RIL.COMPREHENSIONTLV_TAG_DEVICE_ID |
        lazy.RIL.COMPREHENSIONTLV_FLAG_CR
    );
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(options.deviceId.sourceId);
    GsmPDUHelper.writeHexOctet(options.deviceId.destinationId);

    // Item Identifier
    if (options.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_ITEM_ID |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.itemIdentifier);
    }

    // Help Request
    if (options.helpRequested) {
      GsmPDUHelper.writeHexOctet(lazy.RIL.COMPREHENSIONTLV_TAG_HELP_REQUEST);
      GsmPDUHelper.writeHexOctet(0);
      // Help Request doesn't have value
    }

    // Location Status
    if (options.locationStatus != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_LOCATION_STATUS |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.locationStatus);
    }

    // Location Info
    if (options.locationInfo) {
      ComprehensionTlvHelper.writeLocationInfoTlv(options.locationInfo);
    }

    // Transaction Id
    if (options.transactionId != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_TRANSACTION_ID |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.transactionId);
    }

    // Address
    if (options.address) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_ADDRESS |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      let addressLength =
        options.address[0] == "+"
          ? options.address.length - 1
          : options.address.length;
      ComprehensionTlvHelper.writeLength(
        Math.ceil(addressLength / 2) + 1 // address BCD + TON
      );
      this.simIOcontext.ICCPDUHelper.writeDiallingNumber(options.address);
    }

    // Cause of disconnection.
    if (options.cause != null) {
      ComprehensionTlvHelper.writeCauseTlv(options.cause);
    }

    // Timer Identifier
    if (options.timerId != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.timerId);
    }

    // Timer Value
    if (options.timerValue != null) {
      ComprehensionTlvHelper.writeTimerValueTlv(options.timerValue, true);
    }

    // Language
    if (options.language) {
      ComprehensionTlvHelper.writeLanguageTlv(options.language);
    }

    // Browser Termination
    if (options.terminationCause != null) {
      GsmPDUHelper.writeHexOctet(
        lazy.RIL.COMPREHENSIONTLV_TAG_BROWSER_TERMINATION_CAUSE |
          lazy.RIL.COMPREHENSIONTLV_FLAG_CR
      );
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.terminationCause);
    }

    //set ber-tlv length
    GsmPDUHelper.stopCalOutgoingSize();

    //Send command.
    let contents = GsmPDUHelper.pdu;

    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          options.rilMessageToken +
          "] > RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND contents = " +
          contents
      );
    }
    this.rilworker.sendEnvelope(options.rilMessageToken, contents);
  },

  processSetDataProfile(message) {
    // profileList must be a nsIDataProfile list.
    if (!(message.profileList instanceof Array)) {
      message.errorMsg = lazy.RIL.GECKO_ERROR_INVALID_ARGUMENTS;
      this.handleRilResponse(message);
      return;
    }

    let profileList = [];
    message.profileList.forEach(function(profile) {
      if (!(profile instanceof Ci.nsIDataProfile)) {
        profileList.push(new DataProfile(profile));
      } else {
        profileList.push(profile);
      }
    });

    let roaming;
    if (!message.isRoaming) {
      roaming = this.dataRegistrationState.roaming;
    }

    if (DEBUG) {
      this.debug(
        "RILJ: [" +
          message.rilMessageToken +
          "] > RIL_REQUEST_SET_DATA_PROFILE profile = " +
          JSON.stringify(profileList)
      );
    }
    this.rilworker.setDataProfile(
      message.rilMessageToken,
      profileList,
      roaming
    );
  },

  sendWorkerMessage(rilMessageType, message, callback) {
    if (DEBUG) {
      this.debug(
        "sendWorkerMessage: rilMessageType: " +
          rilMessageType +
          ": " +
          JSON.stringify(message)
      );
    }
    // Special handler for setRadioEnabled.
    if (rilMessageType === "setRadioEnabled") {
      // Forward it to gRadioEnabledController.
      lazy.gRadioEnabledController.setRadioEnabled(
        this.clientId,
        message,
        callback.handleResponse
      );
      return;
    }

    if (callback) {
      this.sendRilRequest(rilMessageType, message, function(response) {
        return callback.handleResponse(response);
      });
    } else {
      this.sendRilRequest(rilMessageType, message);
    }
  },

  getOemHook() {
    if (this.oemHook) {
      return;
    }

    if (Ci.nsIOemHook) {
      this.debug("Init OemHook module.");
      this.oemHook = lazy.gOemHookService.getOemHook(this.clientId);
      if (this.oemHook) {
        this.oemHook.init(this);
      }
    }
  },

  // nsIOemHookCallback
  handleOemHookResponse(response) {
    if (DEBUG) {
      this.debug(
        "Received message from oemHook handleOemHookResponse:" +
          JSON.stringify(response.messageType)
      );
    }

    if (!response) {
      return;
    }

    // For backward avaliable. Intention for not change the origianl parameter naming for the upperlayer.
    let result = {};
    result.rilMessageType = response.messageType;
    result.rilMessageToken = response.messageToken;

    if (typeof response.errorMsg == "string") {
      result.errorMsg = response.errorMsg;
    } else {
      result.errorMsg = lazy.RIL.RIL_ERROR_TO_GECKO_ERROR[response.errorMsg];
    }

    switch (response.messageType) {
      case "personalizationStatusReq":
        // This is a oemhook resposne
        if (response.errorMsg == 0) {
          let persoStatus = response.persoResp.persoStatus;
          // For iccGetSubsidyLockStatus response
          if (persoStatus.length > 1) {
            if (DEBUG) {
              this.debug(
                "OEMHOOK: [" +
                  response.messageToken +
                  "] < OEMHOOK_PERSONALIZATION_STATUS_REQ  persoStatus = " +
                  JSON.stringify(persoStatus)
              );
            }
            result.persoStatus = persoStatus;
          }
          // For getCardLockEnabled response
          else if (persoStatus.length == 1) {
            if (DEBUG) {
              this.debug(
                "OEMHOOK: [" +
                  response.messageToken +
                  "] < OEMHOOK_PERSONALIZATION_STATUS_REQ  lock = " +
                  persoStatus[0].hasPersoStatus +
                  " , remainingRetries = " +
                  persoStatus[0].verifyAttempts
              );
            }
            result.enabled = persoStatus[0].hasPersoStatus;
          } else {
            if (DEBUG) {
              this.debug(
                "OEMHOOK: [" +
                  response.messageToken +
                  "] < OEMHOOK_PERSONALIZATION_STATUS_REQ  error = "
              );
            }
            result.errorMsg = lazy.RIL.GECKO_ERROR_GENERIC_FAILURE;
          }
        } else if (DEBUG) {
          this.debug(
            "OEMHOOK: [" +
              response.messageToken +
              "] < OEMHOOK_PERSONALIZATION_STATUS_REQ error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")"
          );
        }
        break;
      case "enterDepersonalizationReq":
        // This is a oemhook resposne
        if (response.errorMsg == 0) {
          if (DEBUG) {
            this.debug(
              "OEMHOOK: [" +
                response.messageToken +
                "] < OEMHOOK_ENTER_DEPERSONALIZATION_REQ"
            );
          }
        } else if (DEBUG) {
          let remainingRetries = response.depersoResp
            ? response.depersoResp.depersoRetries
            : -1;
          this.debug(
            "OEMHOOK: [" +
              response.messageToken +
              "] < OEMHOOK_ENTER_DEPERSONALIZATION_REQ error = " +
              result.errorMsg +
              " (" +
              response.errorMsg +
              ")" +
              "remainingRetries = " +
              remainingRetries
          );
        }
        result.remainingRetries = response.depersoResp
          ? response.depersoResp.depersoRetries
          : -1;
        break;
      default:
        throw new Error(
          "Unknow response message type : " + response.rilMessageType
        );
    }

    let token = response.messageToken;
    let callback = this.tokenCallbackMap[token];

    if (!callback) {
      if (DEBUG) {
        this.debug("Ignore orphan token: " + token);
      }
      return;
    }

    let keep = false;
    try {
      keep = callback(result);
    } catch (e) {
      if (DEBUG) {
        this.debug("callback throws an exception: " + e);
      }
    }

    if (!keep) {
      delete this.tokenCallbackMap[token];
    }
  },
};

var EXPORTED_SYMBOLS = ["RadioInterfaceLayer"];
