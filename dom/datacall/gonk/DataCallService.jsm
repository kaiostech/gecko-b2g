/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const DATACALLSERVICE_CONTRACTID = "@mozilla.org/datacallservice;1";
const DATACALLSERVICE_CID = Components.ID(
  "{e29c041d-290d-4e6c-8bca-452f6557de68}"
);

const DATACALL_IPC_MSG_ENTRIES = [
  "DataCall:RequestDataCall",
  "DataCall:ReleaseDataCall",
  "DataCall:GetDataCallState",
  "DataCall:AddHostRoute",
  "DataCall:RemoveHostRoute",
  "DataCall:Register",
];

const DATACALL_CONNECT_TIMEOUT = 30000;
const DATACALL_TYPES = [
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_HIPRI,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_XCAP,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_CBS,
  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_ECC,
];

const NETWORK_STATE_CONNECTED = Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED;

const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const TOPIC_INNER_WINDOW_DESTROYED = "inner-window-destroyed";
const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";
const MESSAGE_CHILD_PROCESS_SHUTDOWN = "child-process-shutdown";
const SETTINGS_DATA_DEFAULT_SERVICE_ID = "ril.data.defaultServiceId";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRil",
  "@mozilla.org/ril;1",
  "nsIRadioInterfaceLayer"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkManager",
  "@mozilla.org/network/manager;1",
  "nsINetworkManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);

XPCOMUtils.defineLazyGetter(lazy, "RIL", function() {
  return ChromeUtils.import("resource://gre/modules/ril_consts.js");
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

// set to true in ril_consts.js to see debug messages
let DEBUG = RIL_DEBUG.DEBUG_RIL;

function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref = Services.prefs.getBoolPref(
    RIL_DEBUG.PREF_RIL_DEBUG_ENABLED,
    false
  );
  DEBUG = debugPref || RIL_DEBUG.DEBUG_RIL;
}
updateDebugFlag();

function DataCallService() {
  if (DEBUG) {
    this.debug("DataCallService starting.");
  }

  this.dataCallsContext = {};
  for (let type of DATACALL_TYPES) {
    this.dataCallsContext[type] = {};
    this.dataCallsContext[type].connectTimer = null;
    this.dataCallsContext[type].requestTargets = [];
  }

  this._dataDefaultServiceId = 0;
  this._listeners = {};

  // Read the default service id for data call.
  this.getSettingValue(SETTINGS_DATA_DEFAULT_SERVICE_ID);
  this.addSettingObserver(SETTINGS_DATA_DEFAULT_SERVICE_ID);

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
  Services.obs.addObserver(this, TOPIC_INNER_WINDOW_DESTROYED);
  Services.obs.addObserver(this, TOPIC_CONNECTION_STATE_CHANGED);
  this._registerMessageListeners();
}
DataCallService.prototype = {
  classDescription: "DataCallService",
  classID: DATACALLSERVICE_CID,
  contractID: DATACALLSERVICE_CONTRACTID,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIMessageListener,
    Ci.nsIObserver,
    Ci.nsISettingsObserver,
  ]),

  dataCallsContext: null,

  _dataDefaultServiceId: null,

  _listeners: null,

  debug(aMsg) {
    dump("-*- DataCallService: " + aMsg + "\n");
  },

  _registerMessageListeners() {
    Services.ppmm.addMessageListener(MESSAGE_CHILD_PROCESS_SHUTDOWN, this);
    for (let msgName of DATACALL_IPC_MSG_ENTRIES) {
      Services.ppmm.addMessageListener(msgName, this);
    }
  },

  _unregisterMessageListeners() {
    Services.ppmm.removeMessageListener(MESSAGE_CHILD_PROCESS_SHUTDOWN, this);
    for (let msgName of DATACALL_IPC_MSG_ENTRIES) {
      Services.ppmm.removeMessageListener(msgName, this);
    }
  },

  _createDataCall(aNetwork) {
    let ips = {};
    let prefixLengths = {};
    let length = aNetwork.getAddresses(ips, prefixLengths);

    let addresses = [];
    for (let i = 0; i < length; i++) {
      let addr = ips.value[i] + "/" + prefixLengths.value[i];
      addresses.push(addr);
    }

    let dataCall = {
      state: aNetwork.state,
      serviceId: aNetwork.serviceId,
      type: aNetwork.type,
      name: aNetwork.name,
      addresses,
      gateways: aNetwork.getGateways(),
      dnses: aNetwork.getDnses(),
      netId: aNetwork.netId,
    };

    return dataCall;
  },

  getDataCallContext(aType) {
    return this.dataCallsContext[aType];
  },

  _processContextRequests(aDataCallContext, aResult) {
    if (!aDataCallContext.requestTargets.length) {
      return;
    }

    aDataCallContext.requestTargets.forEach(aRequest => {
      if (aResult) {
        let target = aRequest.target;
        let resolverId = aRequest.resolverId;

        if (aResult.errorMsg) {
          target.sendAsyncMessage("DataCall:RequestDataCall:Rejected", {
            resolverId,
            reason: aResult.errorMsg,
          });
        } else {
          target.sendAsyncMessage("DataCall:RequestDataCall:Resolved", {
            resolverId,
            result: aResult,
          });
        }
      }
    });
    aDataCallContext.requestTargets = [];
  },

  _cleanupRequestsByTarget(aTarget) {
    for (let type of DATACALL_TYPES) {
      let context = this.dataCallsContext[type];
      if (!context) {
        continue;
      }

      let requests = context.requestTargets;

      for (let i = requests.length - 1; i >= 0; i--) {
        if (requests[i].target == aTarget) {
          this._deactivateDataCall(requests[i].serviceId, requests[i].type);
          requests.splice(i, 1);
        }
      }

      if (requests.length == 0 && context.connectTimer) {
        context.connectTimer.cancel();
        context.connectTimer = null;
      }
    }
  },

  _cleanupRequestsByWinId(aWindowId) {
    for (let type of DATACALL_TYPES) {
      let context = this.dataCallsContext[type];
      if (!context) {
        continue;
      }
      let requests = context.requestTargets;

      for (let i = requests.length - 1; i >= 0; i--) {
        if (requests[i].windowId == aWindowId) {
          this._deactivateDataCall(requests[i].serviceId, requests[i].type);
          requests.splice(i, 1);
        }
      }

      if (requests.length == 0 && context.connectTimer) {
        context.connectTimer.cancel();
        context.connectTimer = null;
      }
    }
  },

  onConnectionStateChanged(aNetwork) {
    let context = this.getDataCallContext(aNetwork.type);
    if (!context) {
      return;
    }

    if (
      aNetwork.state == NETWORK_STATE_CONNECTED &&
      context.requestTargets.length
    ) {
      context.connectTimer.cancel();
      context.connectTimer = null;

      let dataCall = this._createDataCall(aNetwork);
      this._processContextRequests(context, dataCall);
    }
  },

  sendStateChangeEvent(aNetwork) {
    let topic = this._getTopicName(aNetwork.serviceId, aNetwork.type);

    let details = {};
    if (aNetwork.state == NETWORK_STATE_CONNECTED) {
      let dataCall = this._createDataCall(aNetwork);
      details.name = dataCall.name;
      details.addresses = dataCall.addresses;
      details.gateways = dataCall.gateways;
      details.dnses = dataCall.dnses;
    }

    this._notifyMessageTarget(topic, "DataCall:OnStateChanged", {
      state: aNetwork.state,
      reason: aNetwork.reason,
      details,
    });
  },

  _getNetworkInfo(aServiceId, aType) {
    for (let networkId in lazy.gNetworkManager.allNetworkInfo) {
      let networkInfo = lazy.gNetworkManager.allNetworkInfo[networkId];
      if (networkInfo.type == aType) {
        try {
          if (networkInfo instanceof Ci.nsIRilNetworkInfo) {
            let rilNetworkInfo = networkInfo.QueryInterface(
              Ci.nsIRilNetworkInfo
            );
            if (rilNetworkInfo.serviceId != aServiceId) {
              continue;
            }
          }
        } catch (e) {}
        return networkInfo;
      }
    }
    return null;
  },

  _sendResponse(aMessage, aResult) {
    if (aResult && aResult.errorMsg) {
      aMessage.target.sendAsyncMessage(aMessage.name + ":Rejected", {
        dataCallId: aMessage.data.dataCallId,
        resolverId: aMessage.data.resolverId,
        reason: aResult.errorMsg,
      });
      return;
    }

    aMessage.target.sendAsyncMessage(aMessage.name + ":Resolved", {
      dataCallId: aMessage.data.dataCallId,
      resolverId: aMessage.data.resolverId,
      result: aResult,
    });
  },

  _setupDataCall(aData, aTargetCallback, aTarget) {
    if (DEBUG) {
      this.debug("Setup data call for " + aData.serviceId + "-" + aData.type);
    }

    let serviceId =
      aData.serviceId != undefined
        ? aData.serviceId
        : this._dataDefaultServiceId;
    let type = aData.type;

    let context = this.getDataCallContext(type);
    if (!context) {
      aTargetCallback({ errorMsg: "Mobile network type not supported." });
      return;
    }

    let ril = lazy.gRil.getRadioInterface(serviceId);
    if (!ril) {
      aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      return;
    }

    let connection = lazy.gMobileConnectionService.getItemByServiceId(
      this._dataDefaultServiceId
    );

    let dataInfo = connection && connection.data;
    let radioTechType = dataInfo && dataInfo.type;
    let radioTechnology = lazy.RIL.GECKO_RADIO_TECH.indexOf(radioTechType);

    // This check avoids data call connection if the radio is not ready
    // yet after toggling off airplane mode.
    let radioState = connection && connection.radioState;
    if (radioState != Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED) {
      if (radioTechnology != lazy.RIL.NETWORK_CREG_TECH_IWLAN) {
        aTargetCallback({ errorMsg: "Radio state is off." });
        return;
      }
      if (DEBUG) {
        this.debug("IWLAN network consider as radio power on.");
      }
    }

    if (dataInfo && dataInfo.state != "registered") {
      aTargetCallback({ errorMsg: "Data registration not registered." });
      return;
    }

    try {
      // Call this no matter what for ref counting in RadioInterfaceLayer.
      ril.setupDataCallByType(type);
    } catch (e) {
      // Throws when setup request fail.
      aTargetCallback({ errorMsg: "Error when setup data call: " + e });
      return;
    }

    if (ril.getDataCallStateByType(type) == NETWORK_STATE_CONNECTED) {
      let networkInfo = this._getNetworkInfo(serviceId, type);
      // For make sure the networkmnager got the connected event.
      if (networkInfo.state == NETWORK_STATE_CONNECTED) {
        let dataCall = this._createDataCall(networkInfo);
        aTargetCallback(dataCall);
        return;
      }
    }

    context.requestTargets.push({
      target: aTarget,
      windowId: aData.windowId,
      resolverId: aData.resolverId,
      serviceId,
      type,
    });

    // Start connect timer if not started yet.
    if (context.connectTimer) {
      return;
    }
    context.connectTimer = Cc["@mozilla.org/timer;1"].createInstance(
      Ci.nsITimer
    );
    context.connectTimer.initWithCallback(
      () => {
        context.connectTimer = null;
        if (DEBUG) {
          this.debug("Connection time out for type: " + type);
        }
        this._processContextRequests(context, {
          errorMsg: "Connection timeout.",
        });
      },
      DATACALL_CONNECT_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _getDataCallState(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug(
        "Get data call state for: " + aData.serviceId + "-" + aData.type
      );
    }

    let serviceId =
      aData.serviceId != undefined
        ? aData.serviceId
        : this._dataDefaultServiceId;
    let ril = lazy.gRil.getRadioInterface(serviceId);
    if (!ril) {
      aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      return;
    }

    let state;
    try {
      state = ril.getDataCallStateByType(aData.type);
    } catch (e) {
      // Throws when type is not a mobile network type.
      aTargetCallback({ errorMsg: "Error getting data call state: " + e });
      return;
    }

    if (DEBUG) {
      this.debug(
        "_getDataCallState:  type=" + aData.type + " , state=" + state
      );
    }

    aTargetCallback(state);
  },

  _releaseDataCall(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Release data call: " + +aData.serviceId + "-" + aData.type);
    }

    let type = aData.type;
    let serviceId = aData.serviceId;

    this._deactivateDataCall(serviceId, type, aTargetCallback);
  },

  _addHostRoute(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Add host route for " + aData.serviceId + "-" + aData.type);
    }

    let networkInfo = this._getNetworkInfo(aData.serviceId, aData.type);
    lazy.gNetworkManager.addHostRoute(networkInfo, aData.host).then(
      () => {
        aTargetCallback();
      },
      aReason => {
        aTargetCallback({ errorMsg: aReason });
      }
    );
  },

  _removeHostRoute(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Remove host route for " + aData.serviceId + "-" + aData.type);
    }

    let networkInfo = this._getNetworkInfo(aData.serviceId, aData.type);
    lazy.gNetworkManager.removeHostRoute(networkInfo, aData.host).then(
      () => {
        aTargetCallback();
      },
      aReason => {
        aTargetCallback({ errorMsg: aReason });
      }
    );
  },

  _deactivateDataCall(aServiceId, aType, aTargetCallback) {
    let ril = lazy.gRil.getRadioInterface(aServiceId);
    if (!ril) {
      if (aTargetCallback) {
        aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      }
      return;
    }

    try {
      // Call this no matter what for ref counting in RadioInterfaceLayer.
      ril.deactivateDataCallByType(aType);
    } catch (e) {
      // Throws when type is not a mobile network type.
      if (aTargetCallback) {
        aTargetCallback({ errorMsg: "Error when deactivate data call: " + e });
      }
      return;
    }

    if (aTargetCallback) {
      aTargetCallback();
    }
  },

  _registerMessageTarget(aTopic, aTarget, aData) {
    let listeners = this._listeners[aTopic];
    if (!listeners) {
      listeners = this._listeners[aTopic] = [];
    }

    listeners.push({
      target: aTarget,
      windowId: aData.windowId,
      dataCallId: aData.dataCallId,
    });
    if (DEBUG) {
      this.debug("Registerd " + aTopic + " dataCallId: " + aData.dataCallId);
    }
  },

  _unregisterMessageTarget(aTopic, aTarget, aDataCallId, aCleanup) {
    if (!aTopic) {
      for (let topic in this._listeners) {
        this._unregisterMessageTarget(topic, aTarget, aDataCallId, aCleanup);
      }
      return;
    }

    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    let index = listeners.length;
    while (index--) {
      let listener = listeners[index];
      // In addition to target, dataCallId must match, if available.
      if (
        listener.target === aTarget &&
        (!aDataCallId || listener.dataCallId === aDataCallId)
      ) {
        if (DEBUG) {
          this.debug(
            "Unregistering " + aTopic + " dataCallId: " + listener.dataCallId
          );
        }

        listeners.splice(index, 1);
        // Cleanup data call if it was not released propoerly.
        if (aCleanup) {
          let [serviceId, type] = aTopic.split("-");
          this._deactivateDataCall(serviceId, type);
        }
      }
    }
  },

  _unregisterMessageTargetByWinId(aTopic, aWindowId, aCleanup) {
    if (!aTopic) {
      for (let topic in this._listeners) {
        this._unregisterMessageTargetByWinId(topic, aWindowId, aCleanup);
      }
      return;
    }

    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    let index = listeners.length;
    while (index--) {
      if (listeners[index].windowId === aWindowId) {
        if (DEBUG) {
          this.debug("Unregistering " + aTopic + " windowId: " + aWindowId);
        }

        listeners.splice(index, 1);
        // Cleanup data call if it was not released propoerly.
        if (aCleanup) {
          let [serviceId, type] = aTopic.split("-");
          this._deactivateDataCall(serviceId, type);
        }
      }
    }
  },

  _notifyMessageTarget(aTopic, aMessage, aOptions) {
    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    // Group listeners that have the same target to avoid flooding the IPC
    // channel.
    let groupedListeners = new Map();
    listeners.forEach(aListener => {
      let target = aListener.target;
      if (!groupedListeners.has(target)) {
        groupedListeners.set(target, [aListener.dataCallId]);
      } else {
        let ids = groupedListeners.get(target);
        ids.push(aListener.dataCallId);
        groupedListeners.set(target, ids);
      }
    });

    groupedListeners.forEach((ids, target) => {
      aOptions.dataCallIds = ids;
      target.sendAsyncMessage(aMessage, aOptions);
    });
  },

  _getTopicName(aServiceId, aType) {
    return aServiceId + "-" + aType;
  },

  /**
   * nsIObserver interface methods.
   */
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_CONNECTION_STATE_CHANGED:
        if (!(aSubject instanceof Ci.nsIRilNetworkInfo)) {
          return;
        }
        let networkInfo = aSubject.QueryInterface(Ci.nsIRilNetworkInfo);
        if (DEBUG) {
          this.debug(
            "Network " +
              networkInfo.type +
              "/" +
              networkInfo.name +
              " changed state to " +
              networkInfo.state
          );
        }

        this.onConnectionStateChanged(networkInfo);
        this.sendStateChangeEvent(networkInfo);
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        for (let type of DATACALL_TYPES) {
          let context = this.dataCallsContext[type];
          if (!context) {
            continue;
          }
          if (context.connectTimer) {
            context.connectTimer.cancel();
            context.connectTimer = null;
          }
          this._processContextRequests(context, { errorMsg: "xpcom-shutdown" });
          delete this.dataCallsContext[type];
        }
        this.dataCallsContext = null;

        this._unregisterMessageListeners();
        this.removeSettingObserver(SETTINGS_DATA_DEFAULT_SERVICE_ID);
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_INNER_WINDOW_DESTROYED);
        Services.obs.removeObserver(this, TOPIC_CONNECTION_STATE_CHANGED);
        break;
      case TOPIC_INNER_WINDOW_DESTROYED:
        let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
        this._cleanupRequestsByTarget(wId);
        this._unregisterMessageTargetByWinId(null, wId, true);
        break;
    }
  },

  /**
   * nsIMessageListener interface methods.
   */
  /* eslint-disable consistent-return */
  receiveMessage(aMessage) {
    if (DEBUG) {
      this.debug(
        "Received '" + aMessage.name + "' message from content process."
      );
    }

    if (aMessage.name == MESSAGE_CHILD_PROCESS_SHUTDOWN) {
      this._cleanupRequestsByTarget(aMessage.target);
      this._unregisterMessageTarget(null, aMessage.target, null, true);
      return;
    }

    if (!DATACALL_IPC_MSG_ENTRIES.includes(aMessage.name)) {
      if (DEBUG) {
        this.debug("Ignoring unknown message type: " + aMessage.name);
      }
      return;
    }

    let callback = aResult => this._sendResponse(aMessage, aResult);
    let data = aMessage.data;

    switch (aMessage.name) {
      case "DataCall:RequestDataCall":
        this._setupDataCall(data, callback, aMessage.target);
        break;
      case "DataCall:GetDataCallState":
        this._getDataCallState(data, callback);
        break;
      case "DataCall:ReleaseDataCall": {
        let topic = this._getTopicName(data.serviceId, data.type);
        this._unregisterMessageTarget(topic, aMessage.target, data.dataCallId);
        this._releaseDataCall(data, callback);
        break;
      }
      case "DataCall:AddHostRoute":
        this._addHostRoute(data, callback);
        break;
      case "DataCall:RemoveHostRoute":
        this._removeHostRoute(data, callback);
        break;
      case "DataCall:Register": {
        let topic = this._getTopicName(data.serviceId, data.type);
        this._registerMessageTarget(topic, aMessage.target, data);
        break;
      }
    }

    return null;
  },
  /* eslint-enable consistent-return */

  /**
   * nsISettingsObserver
   */
  observeSetting(aSettingInfo) {
    if (aSettingInfo) {
      let name = aSettingInfo.name;
      let result = aSettingInfo.value;
      this.handleSettingChanged(name, result);
    }
  },

  //TODO when dds change should reset the connection and re establish on the DDS slot.
  handleSettingChanged(aName, aResult) {
    switch (aName) {
      case SETTINGS_DATA_DEFAULT_SERVICE_ID:
        this._dataDefaultServiceId = aResult || 0;
        if (DEBUG) {
          this.debug(
            "'_dataDefaultServiceId' is now " + this._dataDefaultServiceId
          );
        }
        break;
    }
  },

  // Helper functions.
  getSettingValue(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      this.debug("get " + aKey + " setting.");
      let self = this;
      lazy.gSettingsManager.get(aKey, {
        resolve: info => {
          self.observeSetting(info);
        },
        reject: () => {
          self.debug("get " + aKey + " failed.");
        },
      });
    }
  },
  setSettingValue(aKey, aValue) {
    if (!aKey || !aValue) {
      return;
    }

    if (lazy.gSettingsManager) {
      this.debug(
        "set " + aKey + " setting with value = " + JSON.stringify(aValue)
      );
      let self = this;
      lazy.gSettingsManager.set(
        [{ name: aKey, value: JSON.stringify(aValue) }],
        {
          resolve: () => {
            self.debug(" Set " + aKey + " succedded. ");
          },
          reject: () => {
            self.debug("Set " + aKey + " failed.");
          },
        }
      );
    }
  },

  //When setting value change would be notify by the observe function.
  addSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      this.debug("add " + aKey + " setting observer.");
      let self = this;
      lazy.gSettingsManager.addObserver(aKey, this, {
        resolve: () => {
          self.debug("observed " + aKey + " successed.");
        },
        reject: () => {
          self.debug("observed " + aKey + " failed.");
        },
      });
    }
  },

  removeSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      this.debug("remove " + aKey + " setting observer.");
      let self = this;
      lazy.gSettingsManager.removeObserver(aKey, this, {
        resolve: () => {
          self.debug("remove observer " + aKey + " successed.");
        },
        reject: () => {
          self.debug("remove observer " + aKey + " failed.");
        },
      });
    }
  },
};

var EXPORTED_SYMBOLS = ["DataCallService"];
