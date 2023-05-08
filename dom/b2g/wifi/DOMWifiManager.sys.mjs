/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);

const DOMWIFIMANAGER_CONTRACTID = "@mozilla.org/wifimanager;1";
const DOMWIFIMANAGER_CID = Components.ID(
  "{c9b5f09e-25d2-40ca-aef4-c4d13d93c706}"
);

const lazy = {};
XPCOMUtils.defineLazyGetter(lazy, "cpmm", () => {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
});

export function WifiNetwork() {}

WifiNetwork.prototype = {
  init(aWindow) {
    this._window = aWindow;
  },

  __init(obj) {
    for (let key in obj) {
      this[key] = obj[key];
    }
  },

  classID: Components.ID("{c01fd751-43c0-460a-8b64-abf652ec7220}"),
  contractID: "@mozilla.org/wifinetwork;1",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};

export function WifiConnection(obj) {
  this.status = obj.status;
  this.network = obj.network;
}

WifiConnection.prototype = {
  classID: Components.ID("{23579da4-201b-4319-bd42-9b7f337343ac}"),
  contractID: "@mozilla.org/wificonnection;1",
};

export function WifiConnectionInfo(obj) {
  this.signalStrength = obj.signalStrength;
  this.relSignalStrength = obj.relSignalStrength;
  this.linkSpeed = obj.linkSpeed;
  this.ipAddress = obj.ipAddress;
}

WifiConnectionInfo.prototype = {
  classID: Components.ID("{83670352-6ed4-4c35-8de9-402296a1959c}"),
  contractID: "@mozilla.org/wificonnectioninfo;1",
};

export function WifiCapabilities(obj) {
  this.security = obj.security;
  this.eapMethod = obj.eapMethod;
  this.eapPhase2 = obj.eapPhase2;
  this.certificate = obj.certificate;
}

WifiCapabilities.prototype = {
  classID: Components.ID("{08c88ece-8092-481b-863b-5515a52e411a}"),
  contractID: "@mozilla.org/wificapabilities;1",

  getSecurity() {
    return this.security || [];
  },

  getEapMethod() {
    return this.eapMethod || [];
  },

  getEapPhase2() {
    return this.eapPhase2 || [];
  },

  getCertificate() {
    return this.certificate || [];
  },
};

export function DOMWifiManager() {
  this.defineEventHandlerGetterSetter("onstatuschange");
  this.defineEventHandlerGetterSetter("onconnectioninfoupdate");
  this.defineEventHandlerGetterSetter("onenabled");
  this.defineEventHandlerGetterSetter("ondisabled");
  this.defineEventHandlerGetterSetter("onstationinfoupdate");
  this.defineEventHandlerGetterSetter("onopennetwork");
  this.defineEventHandlerGetterSetter("onscanresult");
  this.defineEventHandlerGetterSetter("onwifihasinternet");
  this.defineEventHandlerGetterSetter("oncaptiveportallogin");
}

DOMWifiManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  classDescription: "DOMWifiManager",
  classID: DOMWIFIMANAGER_CID,
  contractID: DOMWIFIMANAGER_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),

  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    // Maintain this state for synchronous APIs.
    this._currentNetwork = null;
    this._connectionStatus = "disconnected";
    this._enabled = false;
    this._notification = false;
    this._hasInternet = false;
    this._loginSuccess = false;
    this._scanResult = null;
    this._lastConnectionInfo = null;
    this._capabilities = null;
    this._stationNumber = 0;

    const messages = [
      "WifiManager:getNetworks:Return:OK",
      "WifiManager:getNetworks:Return:NO",
      "WifiManager:getKnownNetworks:Return:OK",
      "WifiManager:getKnownNetworks:Return:NO",
      "WifiManager:associate:Return:OK",
      "WifiManager:associate:Return:NO",
      "WifiManager:forget:Return:OK",
      "WifiManager:forget:Return:NO",
      "WifiManager:wps:Return:OK",
      "WifiManager:wps:Return:NO",
      "WifiManager:setPowerSavingMode:Return:OK",
      "WifiManager:setPowerSavingMode:Return:NO",
      "WifiManager:setHttpProxy:Return:OK",
      "WifiManager:setHttpProxy:Return:NO",
      "WifiManager:setStaticIpMode:Return:OK",
      "WifiManager:setStaticIpMode:Return:NO",
      "WifiManager:importCert:Return:OK",
      "WifiManager:importCert:Return:NO",
      "WifiManager:getImportedCerts:Return:OK",
      "WifiManager:getImportedCerts:Return:NO",
      "WifiManager:deleteCert:Return:OK",
      "WifiManager:deleteCert:Return:NO",
      "WifiManager:setWifiEnabled:Return:OK",
      "WifiManager:setWifiEnabled:Return:NO",
      "WifiManager:setPasspointConfig:Return:OK",
      "WifiManager:setPasspointConfig:Return:NO",
      "WifiManager:getPasspointConfigs:Return:OK",
      "WifiManager:getPasspointConfigs:Return:NO",
      "WifiManager:removePasspointConfig:Return:OK",
      "WifiManager:removePasspointConfig:Return:NO",
      "WifiManager:getSoftapStations:Return:OK",
      "WifiManager:getSoftapStations:Return:NO",
      "WifiManager:wifiDown",
      "WifiManager:wifiUp",
      "WifiManager:onconnecting",
      "WifiManager:onassociate",
      "WifiManager:onconnect",
      "WifiManager:ondisconnect",
      "WifiManager:onwpstimeout",
      "WifiManager:onwpsfail",
      "WifiManager:onwpsoverlap",
      "WifiManager:connectioninfoupdate",
      "WifiManager:onauthenticating",
      "WifiManager:ondhcpfailed",
      "WifiManager:onauthenticationfailed",
      "WifiManager:onassociationreject",
      "WifiManager:stationinfoupdate",
      "WifiManager:opennetwork",
      "WifiManager:scanresult",
      "WifiManager:wifihasinternet",
      "WifiManager:captiveportallogin",
    ];
    this.initDOMRequestHelper(aWindow, messages);

    var state = lazy.cpmm.sendSyncMessage("WifiManager:getState")[0];
    if (state) {
      this._currentNetwork = this._convertWifiNetwork(state.network);
      if (this._currentNetwork) {
        this._hasInternet = this._currentNetwork.hasInternet || false;
      }
      this._lastConnectionInfo = this._convertConnectionInfo(
        state.connectionInfo
      );
      this._enabled = state.enabled;
      this._connectionStatus = state.status;
      this._macAddress = state.macAddress;
      this._capabilities = this._convertWifiCapabilities(state.capabilities);
    } else {
      this._currentNetwork = null;
      this._lastConnectionInfo = null;
      this._enabled = false;
      this._connectionStatus = "disconnected";
      this._macAddress = "";
    }
  },

  _convertWifiNetworkToJSON(aNetwork) {
    let json = {};

    for (let key in aNetwork) {
      // In WifiWorker.js there are lots of check using "key in network".
      // So if the value of any property of WifiNetwork is undefined, do not clone it.
      if (aNetwork[key] != undefined) {
        json[key] = aNetwork[key];
      }
    }
    return json;
  },

  _convertWifiNetwork(aNetwork) {
    let network = aNetwork ? new this._window.WifiNetwork(aNetwork) : null;
    return network;
  },

  _convertWifiNetworks(aNetworks) {
    let networks = new this._window.Array();
    for (let i in aNetworks) {
      networks.push(this._convertWifiNetwork(aNetworks[i]));
    }
    return networks;
  },

  _convertConnection(aConn) {
    let conn = aConn ? new WifiConnection(aConn) : null;
    return conn;
  },

  _convertConnectionInfo(aInfo) {
    let info = aInfo ? new WifiConnectionInfo(aInfo) : null;
    return info;
  },

  _convertWifiCapabilities(aCapabilities) {
    let capabilities = aCapabilities
      ? new WifiCapabilities(aCapabilities)
      : null;
    return capabilities;
  },

  _sendMessageForRequest(name, data, request) {
    let id = this.getRequestId(request);
    lazy.cpmm.sendAsyncMessage(name, { data, rid: id, mid: this._id });
  },

  /* eslint-disable complexity */
  receiveMessage(aMessage) {
    let msg = aMessage.json;
    if (msg.mid && msg.mid != this._id) {
      return;
    }

    let request;
    if (msg.rid) {
      request = this.takeRequest(msg.rid);
      if (!request) {
        return;
      }
    }

    switch (aMessage.name) {
      case "WifiManager:setWifiEnabled:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setWifiEnabled:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to enable/disable Wifi");
        break;

      case "WifiManager:getNetworks:Return:OK":
        Services.DOMRequest.fireSuccess(
          request,
          this._convertWifiNetworks(msg.data)
        );
        break;

      case "WifiManager:getNetworks:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to scan for networks");
        break;

      case "WifiManager:getKnownNetworks:Return:OK":
        Services.DOMRequest.fireSuccess(
          request,
          this._convertWifiNetworks(msg.data)
        );
        break;

      case "WifiManager:getKnownNetworks:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to get known networks");
        break;

      case "WifiManager:associate:Return:OK":
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:associate:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:forget:Return:OK":
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:forget:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:wps:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:wps:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setPowerSavingMode:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setPowerSavingMode:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setHttpProxy:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setHttpProxy:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setStaticIpMode:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setStaticIpMode:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:importCert:Return:OK":
        Services.DOMRequest.fireSuccess(
          request,
          Cu.cloneInto(msg.data, this._window)
        );
        break;

      case "WifiManager:importCert:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:getImportedCerts:Return:OK":
        Services.DOMRequest.fireSuccess(
          request,
          Cu.cloneInto(msg.data, this._window)
        );
        break;

      case "WifiManager:getImportedCerts:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:deleteCert:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:deleteCert:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setPasspointConfig:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setPasspointConfig:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:getPasspointConfigs:Return:OK":
        Services.DOMRequest.fireSuccess(
          request,
          Cu.cloneInto(msg.data, this._window)
        );
        break;

      case "WifiManager:getPasspointConfigs:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:removePasspointConfig:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:removePasspointConfig:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:getSoftapStations:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:getSoftapStations:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:wifiDown":
        this._enabled = false;
        this._currentNetwork = null;
        this._fireEnabledOrDisabled(false);
        break;

      case "WifiManager:wifiUp":
        this._enabled = true;
        this._macAddress = msg.macAddress;
        this._fireEnabledOrDisabled(true);
        break;

      case "WifiManager:onconnecting":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "connecting";
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onassociate":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "associated";
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onconnect":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "connected";
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:ondisconnect":
        this._currentNetwork = null;
        this._connectionStatus = "disconnected";
        this._lastConnectionInfo = null;
        this._hasInternet = false;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onwpstimeout":
        this._currentNetwork = null;
        this._connectionStatus = "wps-timedout";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onwpsfail":
        this._currentNetwork = null;
        this._connectionStatus = "wps-failed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onwpsoverlap":
        this._currentNetwork = null;
        this._connectionStatus = "wps-overlapped";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:connectioninfoupdate":
        this._lastConnectionInfo = this._convertConnectionInfo(msg);
        this._fireConnectionInfoUpdate(msg);
        break;

      case "WifiManager:ondhcpfailed":
        this._currentNetwork = null;
        this._connectionStatus = "dhcpfailed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onauthenticationfailed":
        this._currentNetwork = null;
        this._connectionStatus = "authenticationfailed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onassociationreject":
        this._currentNetwork = null;
        this._connectionStatus = "associationreject";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:onauthenticating":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "authenticating";
        this._fireStatusChangeEvent(msg.network);
        break;

      case "WifiManager:stationinfoupdate":
        this._stationNumber = msg.station;
        this._fireStationInfoUpdate(msg);
        break;

      case "WifiManager:opennetwork":
        this._notification = msg.availability;
        this._fireOpenNetwork(msg);
        break;

      case "WifiManager:scanresult":
        this._scanResult = this._convertWifiNetworks(
          JSON.parse(msg.scanResult)
        );
        this._fireScanResult(this._scanResult);
        break;

      case "WifiManager:wifihasinternet":
        this._hasInternet = msg.hasInternet;
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._fireWifiHasInternet(msg.network);
        break;

      case "WifiManager:captiveportallogin":
        this._loginSuccess = msg.loginSuccess;
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._fireCaptivePortalLogin(msg.network);
        break;
    }
  },
  /* eslint-enable complexity */

  _fireStatusChangeEvent: function StatusChangeEvent(aNetwork) {
    var event = new this._window.WifiStatusChangeEvent("statuschange", {
      network: this._convertWifiNetwork(aNetwork),
      status: this._connectionStatus,
    });
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  _fireConnectionInfoUpdate: function onConnectionInfoUpdate(info) {
    var evt = new this._window.WifiConnectionInfoEvent("connectioninfoupdate", {
      network: this._currentNetwork,
      signalStrength: info.signalStrength,
      relSignalStrength: info.relSignalStrength,
      linkSpeed: info.linkSpeed,
      ipAddress: info.ipAddress,
    });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireEnabledOrDisabled: function enabledDisabled(enabled) {
    var evt = new this._window.Event(enabled ? "enabled" : "disabled");
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireStationInfoUpdate: function onStationInfoUpdate(info) {
    var evt = new this._window.WifiStationInfoEvent("stationinfoupdate", {
      station: this._stationNumber,
    });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireOpenNetwork: function onOpenNetwork(notify) {
    var evt = new this._window.WifiOpenNetworkEvent("opennetwork", {
      availability: this._notification,
    });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireScanResult: function onScanResult(scanResult) {
    var evt = new this._window.WifiScanResultEvent("scanresult", {
      scanResult,
    });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireWifiHasInternet: function onWifiHasInternet(aNetwork) {
    var event = new this._window.WifiHasInternetEvent("wifihasinternet", {
      hasInternet: this._hasInternet,
      network: this._convertWifiNetwork(aNetwork),
    });
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  _fireCaptivePortalLogin: function onCaptivePortalLogin(aNetwork) {
    var event = new this._window.CaptivePortalLoginEvent("captiveportallogin", {
      loginSuccess: this._loginSuccess,
      network: this._convertWifiNetwork(aNetwork),
    });
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  setWifiEnabled: function setWifiEnabled(enabled) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setWifiEnabled", enabled, request);
    return request;
  },

  getNetworks: function getNetworks() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getNetworks", null, request);
    return request;
  },

  getKnownNetworks: function getKnownNetworks() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getKnownNetworks", null, request);
    return request;
  },

  associate: function associate(network) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:associate",
      this._convertWifiNetworkToJSON(network),
      request
    );
    return request;
  },

  forget: function forget(network) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:forget",
      this._convertWifiNetworkToJSON(network),
      request
    );
    return request;
  },

  wps: function wps(detail) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:wps", detail, request);
    return request;
  },

  setPowerSavingMode: function setPowerSavingMode(enabled) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:setPowerSavingMode",
      enabled,
      request
    );
    return request;
  },

  setHttpProxy: function setHttpProxy(network, info) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:setHttpProxy",
      { network: this._convertWifiNetworkToJSON(network), info },
      request
    );
    return request;
  },

  setStaticIpMode: function setStaticIpMode(network, info) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:setStaticIpMode",
      { network: this._convertWifiNetworkToJSON(network), info },
      request
    );
    return request;
  },

  importCert: function nsIDOMWifiManager_importCert(
    certBlob,
    certPassword,
    certNickname
  ) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:importCert",
      {
        certBlob,
        certPassword,
        certNickname,
      },
      request
    );
    return request;
  },

  getImportedCerts: function nsIDOMWifiManager_getImportedCerts() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getImportedCerts", null, request);
    return request;
  },

  deleteCert: function nsIDOMWifiManager_deleteCert(certNickname) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:deleteCert",
      {
        certNickname,
      },
      request
    );
    return request;
  },

  setPasspointConfig: function setPasspointConfig(config) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:setPasspointConfig",
      config,
      request
    );
    return request;
  },

  getPasspointConfigs: function getPasspointConfigs() {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:getPasspointConfigs",
      null,
      request
    );
    return request;
  },

  removePasspointConfig: function removePasspointConfig(fqdn) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:removePasspointConfig",
      fqdn,
      request
    );
    return request;
  },

  getSoftapStations: function getSoftapStations() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getSoftapStations", null, request);
    return request;
  },

  set openNetworkNotificationEnabled(enabled) {
    var request = this.createRequest();
    this._sendMessageForRequest(
      "WifiManager:setOpenNetworkNotification",
      enabled,
      request
    );
  },

  get openNetworkNotificationEnabled() {
    return Services.prefs.getBoolPref("persist.wifi.notification", false);
  },

  get enabled() {
    return this._enabled;
  },

  get macAddress() {
    return this._macAddress;
  },

  get openNetworkNotify() {
    return this._notification;
  },

  get scanResult() {
    return this._scanResult;
  },

  get hasInternet() {
    return this._hasInternet;
  },

  get connection() {
    let _connection = this._convertConnection({
      status: this._connectionStatus,
      network: this._currentNetwork,
    });
    return _connection;
  },

  get connectionInformation() {
    return this._lastConnectionInfo;
  },

  get capabilities() {
    return this._capabilities;
  },

  defineEventHandlerGetterSetter(name) {
    Object.defineProperty(this, name, {
      get() {
        return this.__DOM_IMPL__.getEventHandler(name);
      },
      set(handler) {
        this.__DOM_IMPL__.setEventHandler(name, handler);
      },
    });
  },
};
