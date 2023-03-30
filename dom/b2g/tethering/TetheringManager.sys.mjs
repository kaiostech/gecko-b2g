/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);

const DEBUG = true;

var debug = DEBUG
  ? s => {
      console.log("-*- TetheringManager: ", s);
    }
  : s => {};

const TETHERING_TYPE_WIFI = "wifi";
const TETHERING_TYPE_BLUETOOTH = "bt";
const TETHERING_TYPE_USB = "usb";

export function TetheringConfigInfo() {}

TetheringConfigInfo.prototype = {
  init(aWindow) {
    this._window = aWindow;
  },

  __init(obj) {
    for (let key in obj) {
      this[key] = obj[key];
    }
  },

  classID: Components.ID("{1923236b-a9bc-4f54-a394-37487dac3a7b}"),
  contractID: "@mozilla.org/tetheringconfiginfo;1",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};

export function TetheringManager() {
  this.defineEventHandlerGetterSetter("ontetheringstatuschange");
}

TetheringManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  classDescription: "TetheringManager",
  classID: Components.ID("{bd8a831c-d8ec-4f00-8803-606e50781097}"),
  contractID: "@mozilla.org/dom/tetheringmanager;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),

  _window: null,

  init(aWindow) {
    const messages = [
      "WifiManager:setWifiTethering:Return:OK",
      "WifiManager:setWifiTethering:Return:NO",
      "WifiManager:tetheringconfigchange",
      "TetheringService:setUsbTethering:Return:OK",
      "TetheringService:setUsbTethering:Return:NO",
      "TetheringService:tetheringstatuschange",
      "TetheringService:tetheringconfigchange",
    ];

    this._window = aWindow;
    this.initDOMRequestHelper(aWindow, messages);
    var state = Services.cpmm.sendSyncMessage("TetheringService:getStatus")[0];
    if (state) {
      this._wifiTetheringState = state.wifiTetheringState;
      this._usbTetheringState = state.usbTetheringState;
      this._usbTetheringConfig = this._convertTetheringConfigInfo(
        TETHERING_TYPE_USB,
        state.usbTetheringConfig
      );
    } else {
      this._wifiTetheringState =
        Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
      this._usbTetheringState = Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
      this._usbTetheringConfig = this._convertTetheringConfigInfo(
        TETHERING_TYPE_USB,
        null
      );
    }

    // Acquire wifi tethering configuration.
    var wifiConfig = Services.cpmm.sendSyncMessage("WifiManager:getState")[0];
    if (wifiConfig) {
      this._wifiTetheringConfig = this._convertTetheringConfigInfo(
        TETHERING_TYPE_WIFI,
        wifiConfig.wifiTetheringConfig
      );
    } else {
      this._wifiTetheringConfig = this._convertTetheringConfigInfo(
        TETHERING_TYPE_WIFI,
        null
      );
    }
  },

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
      case "WifiManager:setWifiTethering:Return:OK":
      case "TetheringService:setUsbTethering:Return:OK":
        this.handlePromiseMessage(true, aMessage);
        break;
      case "WifiManager:setWifiTethering:Return:NO":
      case "TetheringService:setUsbTethering:Return:NO":
        this.handlePromiseMessage(false, aMessage);
        break;
      case "TetheringService:tetheringstatuschange":
        this._wifiTetheringState = msg.wifiTetheringState;
        this._usbTetheringState = msg.usbTetheringState;
        this._fireTetheringStatusChangeEvent(msg);
        break;
      case "TetheringService:tetheringconfigchange":
        this._usbTetheringConfig = this._convertTetheringConfigInfo(
          TETHERING_TYPE_USB,
          msg.usbTetheringConfig
        );
        this._fireTetheringConfigChangeEvent(
          this._wifiTetheringConfig,
          this._usbTetheringConfig
        );
        break;
      case "WifiManager:tetheringconfigchange":
        this._wifiTetheringConfig = this._convertTetheringConfigInfo(
          TETHERING_TYPE_WIFI,
          msg.wifiTetheringConfig
        );
        this._fireTetheringConfigChangeEvent(
          this._wifiTetheringConfig,
          this._usbTetheringConfig
        );
        break;
    }
  },

  setTetheringEnabled: function setTetheringEnabled(aEnabled, aType, aConfig) {
    let self = this;
    switch (aType) {
      case TETHERING_TYPE_WIFI:
        return this.createPromiseWithId(function(aResolverId) {
          let data = {
            resolverId: aResolverId,
            enabled: aEnabled,
            config: aConfig,
          };
          Services.cpmm.sendAsyncMessage("WifiManager:setWifiTethering", {
            data,
          });
        });
      case TETHERING_TYPE_USB:
        return this.createPromiseWithId(function(aResolverId) {
          let data = {
            resolverId: aResolverId,
            enabled: aEnabled,
            config: aConfig,
          };
          Services.cpmm.sendAsyncMessage("TetheringService:setUsbTethering", {
            data,
          });
        });
      case TETHERING_TYPE_BLUETOOTH:
      default:
        debug("tethering type(" + aType + ") doesn't support");
        return this.createPromiseWithId(function(aResolverId) {
          self.takePromiseResolver(aResolverId).reject();
        });
    }
  },

  handlePromiseMessage: function handlePromise(aResolve, aMessage) {
    let data = aMessage.data.data;
    let resolver = this.takePromiseResolver(data.resolverId);
    if (!resolver) {
      return;
    }
    if (aResolve) {
      resolver.resolve(data);
    } else {
      resolver.reject(data.reason);
    }
  },

  get wifiTetheringState() {
    return this._wifiTetheringState;
  },

  get usbTetheringState() {
    return this._usbTetheringState;
  },

  get usbTetheringConfig() {
    return this._usbTetheringConfig;
  },

  get wifiTetheringConfig() {
    return this._wifiTetheringConfig;
  },

  _convertTetheringConfigInfo(aType, aConfig) {
    if (!aConfig) {
      return null;
    }

    // Upper layer no need to distinguish between usb/wifi.
    aConfig.startIp =
      aType == TETHERING_TYPE_WIFI ? aConfig.wifiStartIp : aConfig.usbStartIp;
    aConfig.endIp =
      aType == TETHERING_TYPE_WIFI ? aConfig.wifiEndIp : aConfig.usbEndIp;

    let config = new this._window.TetheringConfigInfo(aConfig);
    return config;
  },

  _fireTetheringStatusChangeEvent: function onTetheringStatusChangeEvent(info) {
    var event = new this._window.TetheringStatusChangeEvent(
      "tetheringstatuschange",
      {
        usbTetheringState: info.usbTetheringState,
        wifiTetheringState: info.wifiTetheringState,
      }
    );
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  _fireTetheringConfigChangeEvent: function onTetheringConfigChangeEvent(
    wifiConfig,
    usbConfig
  ) {
    var event = new this._window.TetheringConfigChangeEvent(
      "tetheringconfigchange",
      { wifiTetheringConfig: wifiConfig, usbTetheringConfig: usbConfig }
    );
    this.__DOM_IMPL__.dispatchEvent(event);
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
