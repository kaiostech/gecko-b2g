/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TetheringService"];

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { libcutils } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);
const { BinderServices } = ChromeUtils.importESModule(
  "resource://gre/modules/BinderServices.sys.mjs"
);
const { TetheringConfigStore } = ChromeUtils.import(
  "resource://gre/modules/TetheringConfigStore.jsm"
);

const TETHERINGSERVICE_CONTRACTID = "@mozilla.org/tethering/service;1";
const TETHERINGSERVICE_CID = Components.ID(
  "{527a4121-ee5a-4651-be9c-f46f59cf7c01}"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkManager",
  "@mozilla.org/network/manager;1",
  "nsINetworkManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkService",
  "@mozilla.org/network/service;1",
  "nsINetworkService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRndisController",
  "@mozilla.org/network/rndiscontroller;1",
  "nsIRndisController"
);

XPCOMUtils.defineLazyGetter(lazy, "ppmm", () => {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);

XPCOMUtils.defineLazyGetter(lazy, "gRil", function() {
  try {
    return Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}

  return null;
});

const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const PREF_MANAGE_OFFLINE_STATUS = "network.gonk.manage-offline-status";
const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";

const POSSIBLE_USB_INTERFACE_NAME = "rndis0,usb0";
const DEFAULT_USB_INTERFACE_NAME = "rndis0";
const DEFAULT_3G_INTERFACE_NAME = "rmnet0";
const DEFAULT_WIFI_INTERFACE_NAME = "wlan0";

// The kernel's proc entry for network lists.
const KERNEL_NETWORK_ENTRY = "/sys/class/net";

const TETHERING_TYPE_WIFI = "WiFi";
const TETHERING_TYPE_USB = "USB";

const NETWORK_INTERFACE_UP = "up";
const NETWORK_INTERFACE_DOWN = "down";

const TETHERING_STATE_ONGOING = "ongoing";
const TETHERING_STATE_IDLE = "idle";
const TETHERING_STATE_ACTIVE = "active";

// Default value for USB tethering.
const DEFAULT_USB_IP = "192.168.0.1";
const DEFAULT_USB_PREFIX = "24";
const DEFAULT_USB_DHCPSERVER_STARTIP = "192.168.0.10";
const DEFAULT_USB_DHCPSERVER_ENDIP = "192.168.0.30";

// Backup value for USB tethering.
const BACKUP_USB_IP = "172.16.0.1";
const BACKUP_USB_PREFIX = "24";
const BACKUP_USB_DHCPSERVER_STARTIP = "172.16.0.10";
const BACKUP_USB_DHCPSERVER_ENDIP = "172.16.0.30";

const DEFAULT_WIFI_DHCPSERVER_STARTIP = "192.168.1.10";
const DEFAULT_WIFI_DHCPSERVER_ENDIP = "192.168.1.30";

const SETTINGS_DATA_DEFAULT_SERVICE_ID = "ril.data.defaultServiceId";
const DUN_REQUIRED_PROPERTY = "ro.tethering.dun_required";
const MOBILE_DUN_CONNECT_TIMEOUT = 15000;
const MOBILE_DUN_RETRY_INTERVAL = 5000;
const MOBILE_DUN_MAX_RETRIES = 2;

var DEBUG = false;
function updateDebug() {
  try {
    DEBUG = DEBUG || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}
}

function debug(s) {
  if (DEBUG) {
    console.log("-*- TetheringService: ", s, "\n");
  }
}

updateDebug();

function TetheringService() {
  const messages = [
    "TetheringService:setUsbTethering",
    "TetheringService:getStatus",
    "child-process-shutdown",
  ];

  messages.forEach(
    function(msgName) {
      lazy.ppmm.addMessageListener(msgName, this);
    }.bind(this)
  );

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this);
  Services.prefs.addObserver(PREF_MANAGE_OFFLINE_STATUS, this);

  this._manageOfflineStatus = Services.prefs.getBoolPref(
    PREF_MANAGE_OFFLINE_STATUS
  );

  this._dataDefaultServiceId = 0;

  // Possible usb tethering interfaces for different gonk platform.
  this.possibleInterface = POSSIBLE_USB_INTERFACE_NAME.split(",");

  // Default values for internal and external interfaces.
  this._externalInterface = {};
  this._internalInterface = {};
  this._externalInterface[TETHERING_TYPE_USB] = DEFAULT_3G_INTERFACE_NAME;
  this._externalInterface[TETHERING_TYPE_WIFI] = DEFAULT_3G_INTERFACE_NAME;
  this._internalInterface[TETHERING_TYPE_USB] = DEFAULT_USB_INTERFACE_NAME;
  this._internalInterface[TETHERING_TYPE_WIFI] = DEFAULT_WIFI_INTERFACE_NAME;

  lazy.gNetworkService.createNetwork(
    DEFAULT_3G_INTERFACE_NAME,
    Ci.nsINetworkInfo.NETWORK_TYPE_UNKNOWN,
    function() {
      debug("Create a default Network interface: " + DEFAULT_3G_INTERFACE_NAME);
    }
  );

  // Read usb tethering configuration.
  this._usbTetheringConfig = TetheringConfigStore.read(
    TetheringConfigStore.TETHERING_TYPE_USB
  );
  if (!this._usbTetheringConfig) {
    this._usbTetheringConfig = this.fillUSBTetheringConfiguration({});
    TetheringConfigStore.write(
      TetheringConfigStore.TETHERING_TYPE_USB,
      this._usbTetheringConfig,
      null
    );
  }
  this._fireEvent("tetheringconfigchange", {
    usbTetheringConfig: this._usbTetheringConfig,
  });

  this.getSettingValue(SETTINGS_DATA_DEFAULT_SERVICE_ID);
  this.addSettingObserver(SETTINGS_DATA_DEFAULT_SERVICE_ID);

  this.wantConnectionEvent = null;

  this.dunRequired = libcutils.property_get(DUN_REQUIRED_PROPERTY) === "1";
  this.dunConnectTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this.dunRetryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  this._pendingTetheringRequests = [];
}
TetheringService.prototype = {
  classID: TETHERINGSERVICE_CID,
  contractID: TETHERINGSERVICE_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsITetheringService,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
    Ci.nsISettingsObserver,
  ]),

  // Flag to record the default client id for data call.
  _dataDefaultServiceId: null,

  // Usb tethering state.
  _usbTetheringAction: TETHERING_STATE_IDLE,

  // Wifi tethering state.
  _wifiTetheringAction: TETHERING_STATE_IDLE,

  // Previous usb tethering enabled state.
  _oldUsbTetheringEnabledState: null,

  // External and internal interface name.
  _externalInterface: null,

  _internalInterface: null,

  // Dun required.
  dunRequired: false,

  // Dun connection timer.
  dunConnectTimer: null,

  // Dun connection retry times.
  dunRetryTimes: 0,

  // Dun retry timer.
  dunRetryTimer: null,

  // Pending tethering request to handle after dun is connected.
  _pendingTetheringRequests: null,

  // Flag to notify restart USB tethering
  _usbTetheringRequestRestart: false,

  // The state of USB tethering.
  usbState: Ci.nsITetheringService.TETHERING_STATE_INACTIVE,

  // The state of WIFI tethering.
  wifiState: Ci.nsITetheringService.TETHERING_STATE_INACTIVE,

  // Flag to check if we can modify the Services.io.offline.
  _manageOfflineStatus: true,

  // Queue system
  _stateRequests: [],

  _requestProcessing: false,

  _currentRequest: null,

  queueRequest(aMsg) {
    // Clear previous duplicate request.
    // We only care about lastest request for tethering.
    let index = this._stateRequests.indexOf(aMsg);
    if (index !== -1) {
      this._stateRequests.splice(index, 1);
    }

    this._stateRequests.push({
      msg: aMsg,
    });
    this.nextRequest();
  },

  requestDone: function requestDone() {
    this._currentRequest = null;
    this._requestProcessing = false;
    this.nextRequest();
  },

  nextRequest: function nextRequest() {
    // No request to process
    if (this._stateRequests.length === 0) {
      return;
    }

    // Handling request, wait for it.
    if (this._requestProcessing) {
      return;
    }
    // Hold processing lock
    this._requestProcessing = true;

    // Find next valid request
    this._currentRequest = this._stateRequests.shift();

    this.handleRequest(this._currentRequest);
  },

  handleRequest(request) {
    let msg = request.msg;
    debug("handleRequest msg.name=" + msg.name);
    switch (msg.name) {
      case "handleUsbRequest":
        this.onHandleUsbRequest(msg.enable, msg.msgCallback);
        break;
      case "handleWifiRequest":
        this.onHandleWifiRequest(msg.enable, msg.config, msg.callback);
        break;
    }
  },

  // nsIObserver
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        Services.prefs.removeObserver(PREF_MANAGE_OFFLINE_STATUS, this);
        this.removeSettingObservers(SETTINGS_DATA_DEFAULT_SERVICE_ID);

        this.dunConnectTimer.cancel();
        this.dunRetryTimer.cancel();
        break;
      case PREF_MANAGE_OFFLINE_STATUS:
        this._manageOfflineStatus = Services.prefs.getBoolPref(
          PREF_MANAGE_OFFLINE_STATUS
        );
        break;
    }
  },

  // nsISettingsObserver
  observeSetting(aSettingInfo) {
    if (aSettingInfo) {
      let name = aSettingInfo.name;
      let result = aSettingInfo.value;
      this.handleSettingChanged(name, result);
    }
  },

  // Helper functions.
  getSettingValue(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      debug("get " + aKey + " setting.");
      let self = this;
      lazy.gSettingsManager.get(aKey, {
        resolve: info => {
          self.observeSetting(info);
        },
        reject: () => {
          debug("get " + aKey + " failed.");
        },
      });
    }
  },

  //When the setting value change would be notify by the observe function.
  addSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      debug("add " + aKey + " setting observer.");
      lazy.gSettingsManager.addObserver(aKey, this, {
        resolve: () => {
          debug("observed " + aKey + " successed.");
        },
        reject: () => {
          debug("observed " + aKey + " failed.");
        },
      });
    }
  },

  removeSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      debug("remove " + aKey + " setting observer.");
      lazy.gSettingsManager.removeObserver(aKey, this, {
        resolve: () => {
          debug("remove observer " + aKey + " successed.");
        },
        reject: () => {
          debug("remove observer " + aKey + " failed.");
        },
      });
    }
  },

  handleSettingChanged(aName, aResult) {
    switch (aName) {
      case SETTINGS_DATA_DEFAULT_SERVICE_ID:
        this._dataDefaultServiceId = aResult || 0;
        debug("'_dataDefaultServiceId' is now " + this._dataDefaultServiceId);
        break;
    }
  },

  handleUsbRequest(aEnable, aMsgCallback) {
    debug("handleUsbRequest " + aEnable);
    this.queueRequest({
      name: "handleUsbRequest",
      enable: aEnable,
      msgCallback: aMsgCallback,
    });
  },

  handleWifiRequest(aEnable, aConfig, aCallback) {
    debug("handleWifiRequest " + aEnable);
    this.queueRequest({
      name: "handleWifiRequest",
      enable: aEnable,
      config: aConfig,
      callback: aCallback,
    });
  },

  getNetworkInfo(aType, aServiceId) {
    for (let networkId in lazy.gNetworkManager.allNetworkInfo) {
      let networkInfo = lazy.gNetworkManager.allNetworkInfo[networkId];
      if (networkInfo.type == aType) {
        try {
          if (networkInfo instanceof Ci.nsIRilNetworkInfo) {
            let rilNetwork = networkInfo.QueryInterface(Ci.nsIRilNetworkInfo);
            if (rilNetwork.serviceId != aServiceId) {
              continue;
            }
          }
        } catch (e) {}
        return networkInfo;
      }
    }
    return null;
  },

  getActiveNetworkInfo() {
    if (this.dunRequired) {
      let allNetworkInfo = lazy.gNetworkManager.allNetworkInfo;
      for (let networkId in allNetworkInfo) {
        let networkInfo = allNetworkInfo[networkId];
        if (
          (networkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN ||
            networkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI) &&
          networkInfo.state === Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          return networkInfo;
        }
      }
    } else {
      return lazy.gNetworkManager.activeNetworkInfo;
    }
    return null;
  },

  /**
   * Callback when dun connection fails to connect within timeout.
   */
  onDunConnectTimerTimeout() {
    while (this._pendingTetheringRequests.length) {
      debug("onDunConnectTimerTimeout: callback without network info.");
      let callback = this._pendingTetheringRequests.shift();
      if (typeof callback === "function") {
        callback();
      }
    }
  },

  setupDunConnection() {
    this.dunRetryTimer.cancel();
    let connection = lazy.gMobileConnectionService.getItemByServiceId(
      this._dataDefaultServiceId
    );
    let data = connection && connection.data;
    if (data && data.state === "registered") {
      let ril = lazy.gRil.getRadioInterface(this._dataDefaultServiceId);

      try {
        this.dunConnectTimer.cancel();
        ril.setupDataCallByType(Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN);
        this.dunRetryTimes = 0;
        this.dunConnectTimer.initWithCallback(
          this.onDunConnectTimerTimeout.bind(this),
          MOBILE_DUN_CONNECT_TIMEOUT,
          Ci.nsITimer.TYPE_ONE_SHOT
        );
        return;
      } catch (e) {
        debug("setupDunConnection: error: " + e);
      }
    }

    if (this.dunRetryTimes++ >= MOBILE_DUN_MAX_RETRIES) {
      debug("setupDunConnection: max retries reached.");
      this.dunRetryTimes = 0;
      // same as dun connect timeout.
      this.onDunConnectTimerTimeout();
      return;
    }

    debug(
      "Data not ready, retry dun after " + MOBILE_DUN_RETRY_INTERVAL + " ms."
    );
    this.dunRetryTimer.initWithCallback(
      this.setupDunConnection.bind(this),
      MOBILE_DUN_RETRY_INTERVAL,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _dunActiveUsers: 0,
  handleDunConnection(aEnable, aCallback) {
    debug("handleDunConnection: " + aEnable);
    let dun = this.getNetworkInfo(
      Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN,
      this._dataDefaultServiceId
    );

    if (!aEnable) {
      this._dunActiveUsers--;
      if (this._dunActiveUsers > 0) {
        debug("Dun still needed by others, do not disconnect.");
        return;
      }

      this.dunRetryTimes = 0;
      this.dunRetryTimer.cancel();
      this.dunConnectTimer.cancel();
      this._pendingTetheringRequests = [];

      if (dun && dun.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        try {
          lazy.gRil
            .getRadioInterface(this._dataDefaultServiceId)
            .deactivateDataCallByType(
              Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN
            );
        } catch (e) {
          debug("deactivateDunConnection: error: " + e);
        }
      }
      return;
    }

    this._dunActiveUsers++;
    // If Wifi active, enable tethering directly.
    if (
      lazy.gNetworkManager.activeNetworkInfo &&
      lazy.gNetworkManager.activeNetworkInfo.type ===
        Ci.nsINetworkInfo.NETWORK_TYPE_WIFI
    ) {
      debug(
        "Wifi active, enable tethering directly " +
          lazy.gNetworkManager.activeNetworkInfo.name
      );
      aCallback(lazy.gNetworkManager.activeNetworkInfo);
      return;
    }
    // else, trigger dun connection first.
    else if (!dun || dun.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
      debug("DUN data call inactive, setup dun data call!");
      this._pendingTetheringRequests.push(aCallback);
      this.dunRetryTimes = 0;
      this.setupDunConnection();

      return;
    }

    aCallback(dun);
  },

  fillUSBTetheringConfiguration(aConfig) {
    let config = {};
    let check = function(field, _default) {
      config[field] = field in aConfig ? aConfig[field] : _default;
    };

    // Replace IPs cause webidl provide simple naming without usb prefix.
    if (aConfig.startIp) {
      aConfig.usbStartIp = aConfig.startIp;
    }
    if (aConfig.endIp) {
      aConfig.usbEndIp = aConfig.endIp;
    }

    check("ip", DEFAULT_USB_IP);
    check("prefix", DEFAULT_USB_PREFIX);
    check("wifiStartIp", DEFAULT_WIFI_DHCPSERVER_STARTIP);
    check("wifiEndIp", DEFAULT_WIFI_DHCPSERVER_ENDIP);
    check("usbStartIp", DEFAULT_USB_DHCPSERVER_STARTIP);
    check("usbEndIp", DEFAULT_USB_DHCPSERVER_ENDIP);

    return config;
  },

  getUSBTetheringConfiguration(aEnable, aTetheringInterface) {
    let config = {};
    let params = this._usbTetheringConfig;

    config = this.fillUSBTetheringConfiguration(params);
    // Assigned external interface if try to bring up Usb Tethering
    if (aEnable) {
      this.setExternalInterface(TETHERING_TYPE_USB);
    }

    config.ifname = aTetheringInterface;
    config.internalIfname = aTetheringInterface;
    config.externalIfname = this._externalInterface[TETHERING_TYPE_USB];
    config.enable = aEnable;
    config.link = aEnable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN;

    let activeNetworkInfo = this.getActiveNetworkInfo();
    config.dnses = activeNetworkInfo
      ? activeNetworkInfo.getDnses()
      : new Array(0);
    config.ipv6Ip = this.getIpv6TetheringAddress(activeNetworkInfo);

    // Using the default values here until application supports these settings.
    if (
      config.ip == "" ||
      config.prefix == "" ||
      config.wifiStartIp == "" ||
      config.wifiEndIp == "" ||
      config.usbStartIp == "" ||
      config.usbEndIp == ""
    ) {
      debug("Invalid subnet information.");
      return null;
    }

    return config;
  },

  notifyError(aCallback, aErrorMsg) {
    debug("setWifiTethering: " + (aErrorMsg ? aErrorMsg : "success"));

    if (aCallback) {
      // Callback asynchronously to avoid netsted toggling.
      Services.tm.currentThread.dispatch(() => {
        aCallback.wifiTetheringEnabledChange(aErrorMsg);
      }, Ci.nsIThread.DISPATCH_NORMAL);
    }
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering(aEnable, aInterfaceName, aConfig, aCallback) {
    debug("setWifiTethering: " + aEnable);
    if (!aInterfaceName) {
      this.notifyError(aCallback, "invalid network interface name");
      return;
    }

    if (!aConfig) {
      this.notifyError(aCallback, "invalid configuration");
      return;
    }

    // Re-check again, test cases set this property later.
    this.dunRequired = libcutils.property_get(DUN_REQUIRED_PROPERTY) === "1";

    this._internalInterface[TETHERING_TYPE_WIFI] = aInterfaceName;

    this.handleWifiRequest(aEnable, aConfig, aCallback);
  },

  onHandleWifiRequest(aEnable, aConfig, aCallback) {
    // Fill in config's required fields.
    aConfig.ifname = this._internalInterface[TETHERING_TYPE_WIFI];
    aConfig.internalIfname = this._internalInterface[TETHERING_TYPE_WIFI];
    let activeNetworkInfo = this.getActiveNetworkInfo();
    aConfig.dnses = activeNetworkInfo
      ? activeNetworkInfo.getDnses()
      : new Array(0);
    aConfig.ipv6Ip = this.getIpv6TetheringAddress(activeNetworkInfo);

    // WifiWorker will do the enabled/disabled check.

    if (!aEnable) {
      aConfig.externalIfname = this._externalInterface[TETHERING_TYPE_WIFI];
      lazy.gNetworkService.setWifiTethering(
        aEnable,
        aConfig,
        this.wifiTetheringResult.bind(this, aEnable, aConfig, aCallback)
      );
      return;
    }

    this._wifiTetheringAction = TETHERING_STATE_ONGOING;
    if (this.dunRequired) {
      this.handleDunConnection(true, aNetworkInfo => {
        if (!aNetworkInfo) {
          debug(
            "Dun connection failed, remain enable hotspot with default interface."
          );
        }
        // Re-assigned exernal interface info for dun connection
        this.setExternalInterface(TETHERING_TYPE_WIFI);
        aConfig.externalIfname = this._externalInterface[TETHERING_TYPE_WIFI];
        aConfig.dnses = aNetworkInfo ? aNetworkInfo.getDnses() : new Array(0);
        aConfig.ipv6Ip = this.getIpv6TetheringAddress(aNetworkInfo);

        lazy.gNetworkService.setWifiTethering(
          aEnable,
          aConfig,
          this.wifiTetheringResult.bind(this, aEnable, aConfig, aCallback)
        );
      });
      return;
    }

    // Assigned external interface if try to bring up Wifi Tethering
    this.setExternalInterface(TETHERING_TYPE_WIFI);
    aConfig.externalIfname = this._externalInterface[TETHERING_TYPE_WIFI];

    lazy.gNetworkService.setWifiTethering(
      aEnable,
      aConfig,
      this.wifiTetheringResult.bind(this, aEnable, aConfig, aCallback)
    );
  },

  wifiTetheringResult(aEnable, aConfig, aCallback, aError) {
    debug(
      "wifiTetheringResult callback. enable: " + aEnable + ", error: " + aError
    );
    // Change the tethering state to WIFI if there is no error.
    if (aEnable && !aError) {
      this._wifiTetheringAction = TETHERING_STATE_ACTIVE;
      this.wifiState = Ci.nsITetheringService.TETHERING_STATE_ACTIVE;
    } else {
      this.wifiState = Ci.nsITetheringService.TETHERING_STATE_INACTIVE;

      // Disconnect dun on error or when wifi tethering is disabled.
      if (this.dunRequired) {
        this.handleDunConnection(false);
      }
      this._wifiTetheringAction = TETHERING_STATE_IDLE;
    }

    this.checkPendingEvent();
    this._fireEvent("tetheringstatuschange", {
      wifiTetheringState: this.wifiState,
      usbTetheringState: this.usbState,
    });
    BinderServices.connectivity.onTetheringChanged(
      this.wifiState,
      this.usbState
    );
    if (this._manageOfflineStatus) {
      Services.io.offline =
        !this.isAnyConnected() &&
        this.wifiState === Ci.nsITetheringService.TETHERING_STATE_INACTIVE &&
        this.usbState === Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
    }

    debug("gNetworkService.setWifiTethering finished");
    this.notifyError(aCallback, aError);
    this.requestDone();
  },

  onHandleUsbRequest(aEnable, aMsgCallback) {
    debug("onHandleUsbRequest: " + aEnable);
    let self = this;
    if (
      aEnable &&
      (this._usbTetheringAction === TETHERING_STATE_ONGOING ||
        this._usbTetheringAction === TETHERING_STATE_ACTIVE)
    ) {
      debug("Usb tethering already connecting/connected.");
      if (aMsgCallback) {
        aMsgCallback.usbMessageCallback(true);
      }
      this.requestDone();
      return;
    }

    if (!aEnable && this._usbTetheringAction === TETHERING_STATE_IDLE) {
      debug("Usb tethering already disconnected.");
      if (aMsgCallback) {
        aMsgCallback.usbMessageCallback(true);
      }
      this.requestDone();
      return;
    }

    if (!aEnable) {
      lazy.gRndisController.setupRndis(false, {
        onResult(success) {
          self.setupRndisResult(success, false, aMsgCallback);
        },
      });
      return;
    }

    this._usbTetheringAction = TETHERING_STATE_ONGOING;

    // Refine USB tethering IP if subnet conflict with external interface
    if (aEnable && this.isTetherSubnetConflict()) {
      this.refineTetherSubnet(false);
    }

    if (this.dunRequired) {
      this.handleDunConnection(true, aNetworkInfo => {
        // If dun and wifi not active, just re-enable rndis with default interface
        if (!aNetworkInfo) {
          debug("Dun connection failed");
          self.usbTetheringResult(
            aEnable,
            "Dun connection failed",
            aMsgCallback
          );
          return;
        }
        lazy.gRndisController.setupRndis(true, {
          onResult(success) {
            self.setupRndisResult(success, true, aMsgCallback);
          },
        });
      });
      return;
    }

    lazy.gRndisController.setupRndis(true, {
      onResult(success) {
        self.setupRndisResult(success, true, aMsgCallback);
      },
    });
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering(aEnable, aTetheringInterface, aMsgCallback, aCallback) {
    let self = this;
    let config = this.getUSBTetheringConfiguration(
      aEnable,
      aTetheringInterface
    );

    if (config === null) {
      lazy.gRndisController.setupRndis(false, {
        onResult(success) {
          self.usbTetheringResult(aEnable, "Invalid config", aMsgCallback);
        },
      });
      return;
    }

    lazy.gNetworkService.setUSBTethering(
      aEnable,
      config,
      aMsgCallback,
      aCallback
    );
  },

  getUsbInterface() {
    // Find the rndis interface.
    for (let i = 0; i < this.possibleInterface.length; i++) {
      try {
        let file = new FileUtils.File(
          KERNEL_NETWORK_ENTRY + "/" + this.possibleInterface[i]
        );
        if (file.exists()) {
          return this.possibleInterface[i];
        }
      } catch (e) {
        debug("Not " + this.possibleInterface[i] + " interface.");
      }
    }
    debug("Can't find rndis interface in possible lists.");
    return DEFAULT_USB_INTERFACE_NAME;
  },

  setupRndisResult(aSuccess, aEnable, aMsgCallback) {
    let self = this;
    if (aSuccess) {
      // If enable is false, don't find usb interface cause it is already down,
      // just use the internal interface in settings.
      if (aEnable) {
        this._internalInterface[TETHERING_TYPE_USB] = this.getUsbInterface();
      }
      this.setUSBTethering(
        aEnable,
        this._internalInterface[TETHERING_TYPE_USB],
        aMsgCallback,
        this.usbTetheringResult.bind(this, aEnable)
      );
    } else {
      lazy.gRndisController.setupRndis(false, {
        onResult(success) {
          self.usbTetheringResult(
            aEnable,
            "setupRndisResult failure",
            aMsgCallback
          );
        },
      });
      throw new Error("failed to set USB Function to adb");
    }
  },

  usbTetheringResult(aEnable, aError, aMsgCallback) {
    let self = this;
    debug(
      "usbTetheringResult callback. enable: " + aEnable + ", error: " + aError
    );

    // Disable tethering settings when fail to enable it.
    if (aError) {
      if (this.dunRequired) {
        this.handleDunConnection(false);
        if (aError == "Dun connection failed") {
          lazy.gRndisController.setupRndis(true, {
            onResult(success) {
              self.setupRndisResult(success, true, aMsgCallback);
            },
          });
          return;
        }
      }
      // Reset rndis when tethering cmd failed.
      lazy.gRndisController.setupRndis(false, {
        onResult(success) {},
      });
      this._usbTetheringRequestRestart = false;
      this._usbTetheringAction = TETHERING_STATE_IDLE;
      this.usbState = Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
    } else {
      if (aEnable) {
        this._usbTetheringAction = TETHERING_STATE_ACTIVE;
        this.usbState = Ci.nsITetheringService.TETHERING_STATE_ACTIVE;
      } else {
        this._usbTetheringAction = TETHERING_STATE_IDLE;
        this.usbState = Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
        if (this.dunRequired) {
          this.handleDunConnection(false);
        }
        // Restart USB tethering if needed
        if (this._usbTetheringRequestRestart) {
          debug("Restart USB tethering by request");
          this._usbTetheringRequestRestart = false;
          this.handleUsbRequest(true, null);
        }
      }

      this.checkPendingEvent();
      this._fireEvent("tetheringstatuschange", {
        usbTetheringState: this.usbState,
        wifiTetheringState: this.wifiState,
      });
      BinderServices.connectivity.onTetheringChanged(
        this.wifiState,
        this.usbState
      );

      if (this._manageOfflineStatus) {
        Services.io.offline =
          !this.isAnyConnected() &&
          this.usbState === Ci.nsITetheringService.TETHERING_STATE_INACTIVE &&
          this.wifiState === Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
      }
    }

    if (aMsgCallback) {
      aMsgCallback.usbMessageCallback(!aError);
    }
    this.requestDone();
  },

  checkPendingEvent() {
    if (this.wantConnectionEvent) {
      if ((this._usbTetheringAction = TETHERING_STATE_ACTIVE)) {
        this.wantConnectionEvent(this);
      }
      this.wantConnectionEvent = null;
    }
  },

  onExternalConnectionChangedReport(aType, aSuccess, aExternalIfname) {
    debug(
      "onExternalConnectionChangedReport result: success= " +
        aSuccess +
        " ,type= " +
        aType
    );

    if (aSuccess) {
      // Update the external interface.
      this._externalInterface[aType] = aExternalIfname;
      debug("Change the interface name to " + aExternalIfname);
    }
  },

  // Re-design the external connection state change flow for tethering function.
  onExternalConnectionChanged(aNetworkInfo) {
    let self = this;
    // Check if the aNetworkInfo change is the external connection.
    if (
      aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_WIFI ||
      (this.dunRequired &&
        aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN) ||
      (!this.dunRequired &&
        aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE)
    ) {
      debug(
        "onExternalConnectionChanged. aNetworkInfo.type = " +
          aNetworkInfo.type +
          " , aNetworkInfo.state = " +
          aNetworkInfo.state +
          " , dun_required = " +
          this.dunRequired
      );

      let wifiTetheringEnabled =
        this._wifiTetheringAction != TETHERING_STATE_IDLE;
      let usbTetheringEnabled =
        this._usbTetheringAction != TETHERING_STATE_IDLE;
      if (!usbTetheringEnabled && !wifiTetheringEnabled) {
        debug("onExternalConnectionChanged. Tethering is not enabled, return.");
        return;
      }
      debug(
        "onExternalConnectionChanged. wifi : " +
          wifiTetheringEnabled +
          " usb : " +
          usbTetheringEnabled
      );

      // Re-config the NETD.
      let tetheringType = [TETHERING_TYPE_WIFI, TETHERING_TYPE_USB];
      let previous = {};
      let current = {};

      for (let i = 0; i < tetheringType.length; i++) {
        previous[tetheringType[i]] = {
          internalIfname: this._internalInterface[tetheringType[i]],
          externalIfname: this._externalInterface[tetheringType[i]],
        };

        current[tetheringType[i]] = {
          internalIfname: this._internalInterface[tetheringType[i]],
          externalIfname: aNetworkInfo.name,
          dnses: aNetworkInfo.getDnses(),
          ipv6Ip: this.getIpv6TetheringAddress(aNetworkInfo),
        };
      }

      if (aNetworkInfo.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        // Handle the wifi as external interface case, when embedded is dun.
        if (
          this.dunRequired &&
          aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_WIFI
        ) {
          let dun = this.getNetworkInfo(
            Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN,
            this._dataDefaultServiceId
          );
          if (dun && dun.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
            debug("Wifi connected, switch dun to Wifi as external interface.");
            try {
              lazy.gRil
                .getRadioInterface(this._dataDefaultServiceId)
                .deactivateDataCallByType(
                  Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN
                );
            } catch (e) {
              debug("deactivateDunConnection: error: " + e);
            }
          }
        }
        // Handle the dun as external interface case, when embedded is dun.
        else if (
          this.dunRequired &&
          aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN
        ) {
          this.dunConnectTimer.cancel();
          // Handle the request change case.
          if (this._pendingTetheringRequests.length) {
            debug("DUN data call connected, process callbacks.");
            while (this._pendingTetheringRequests.length) {
              let callback = this._pendingTetheringRequests.shift();
              if (typeof callback === "function") {
                callback(aNetworkInfo);
              }
            }
            return;
          }
          // Handle the aNetworkInfo side change case. Run the updateUpStream flow.
          debug("DUN data call connected, process updateUpStream.");
        }

        let callback = function() {
          // Update external aNetworkInfo interface.
          debug("Update upstream interface to " + aNetworkInfo.name);
          if (wifiTetheringEnabled) {
            lazy.gNetworkService.updateUpStream(
              TETHERING_TYPE_WIFI,
              previous[TETHERING_TYPE_WIFI],
              current[TETHERING_TYPE_WIFI],
              this.onExternalConnectionChangedReport.bind(this)
            );
          }
          if (usbTetheringEnabled) {
            // Restart and refine USB tethering if subnet conflict with external interface
            if (this.isTetherSubnetConflict()) {
              this.refineTetherSubnet(true);
              return;
            }
            lazy.gNetworkService.updateUpStream(
              TETHERING_TYPE_USB,
              previous[TETHERING_TYPE_USB],
              current[TETHERING_TYPE_USB],
              this.onExternalConnectionChangedReport.bind(this)
            );
          }
        }.bind(this);

        if (
          this._usbTetheringAction === TETHERING_STATE_ONGOING ||
          this._wifiTetheringAction === TETHERING_STATE_ONGOING
        ) {
          debug("Postpone the event and handle it when state is idle.");
          this.wantConnectionEvent = callback;
          return;
        }
        this.wantConnectionEvent = null;

        callback.call(this);
      } else if (
        aNetworkInfo.state == Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED
      ) {
        // Remove current forwarding forwarding rules
        debug("removing forwarding first before interface removed.");
        if (wifiTetheringEnabled) {
          lazy.gNetworkService.removeUpStream(
            current[TETHERING_TYPE_WIFI],
            aSuccess => {
              if (aSuccess) {
                debug(
                  "remove " +
                    TETHERING_TYPE_WIFI +
                    " forwarding rules successuly!"
                );
                self.setExternalInterface(TETHERING_TYPE_WIFI);
              }
            }
          );
        }
        if (usbTetheringEnabled) {
          lazy.gNetworkService.removeUpStream(
            current[TETHERING_TYPE_USB],
            aSuccess => {
              if (aSuccess) {
                debug(
                  "remove " +
                    TETHERING_TYPE_USB +
                    " forwarding rules successuly!"
                );
                self.setExternalInterface(TETHERING_TYPE_USB);
              }
            }
          );
        }
        // If dun required, retrigger dun connection.
        if (
          aNetworkInfo.type === Ci.nsINetworkInfo.NETWORK_TYPE_WIFI &&
          this.dunRequired
        ) {
          let dun = this.getNetworkInfo(
            Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN,
            this._dataDefaultServiceId
          );
          if (!dun || dun.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
            debug("Wifi disconnect, retrigger dun connection.");
            this.dunRetryTimes = 0;
            this.setupDunConnection();
          }
        }
      }
    } else if (this.dunRequired) {
      debug("onExternalConnectionChanged. Not dun type state change, return.");
    } else {
      debug(
        "onExternalConnectionChanged. Not default type state change, return."
      );
    }
  },

  isAnyConnected() {
    let allNetworkInfo = lazy.gNetworkManager.allNetworkInfo;
    for (let networkId in allNetworkInfo) {
      if (
        allNetworkInfo.hasOwnProperty(networkId) &&
        allNetworkInfo[networkId].state ===
          Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
      ) {
        return true;
      }
    }
    return false;
  },

  // Provide suitable external interface
  setExternalInterface(aType) {
    // Dun case, find Wifi or Dun as external interface.
    if (this.dunRequired) {
      let allNetworkInfo = lazy.gNetworkManager.allNetworkInfo;
      this._externalInterface[aType] = null;
      for (let networkId in allNetworkInfo) {
        if (
          allNetworkInfo[networkId].state ===
            Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED &&
          (allNetworkInfo[networkId].type ==
            Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN ||
            allNetworkInfo[networkId].type ==
              Ci.nsINetworkInfo.NETWORK_TYPE_WIFI)
        ) {
          this._externalInterface[aType] = allNetworkInfo[networkId].name;
        }
      }
      if (!this._externalInterface[aType]) {
        this._externalInterface[aType] = DEFAULT_3G_INTERFACE_NAME;
      }
      // Non Dun case, find available external interface.
    } else if (lazy.gNetworkManager.activeNetworkInfo) {
      this._externalInterface[aType] =
        lazy.gNetworkManager.activeNetworkInfo.name;
    } else {
      this._externalInterface[aType] = DEFAULT_3G_INTERFACE_NAME;
    }
  },

  // Check external/internal interface subnet conlict or not
  isTetherSubnetConflict() {
    // Check USB tethering only since no Wi-Fi concurrent mode
    if (this._usbTetheringAction == TETHERING_STATE_IDLE) {
      return false;
    }

    let activeNetworkInfo = this.getActiveNetworkInfo();
    if (
      activeNetworkInfo &&
      activeNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI
    ) {
      // Get external interface ipaddr & prefix
      let ips = {};
      let prefixLengths = {};
      let localConfig = this._usbTetheringConfig;
      activeNetworkInfo.getAddresses(ips, prefixLengths);

      if (!localConfig.ip || !ips.value || !prefixLengths.value) {
        debug("fail to compare subnet due to empty argument");
        return false;
      }

      for (let i in ips.value) {
        // We only care about IPv4 conflict
        if (!ips.value[i].includes(".")) {
          continue;
        }
        // Filter wan/lan interface network segment
        let subnet = this.cidrToSubnet(prefixLengths.value[i]);
        let lanMask = [],
          wanMask = [];
        let lanIpaddrStr = localConfig.ip.toString().split(".");
        let wanIpaddrStr = ips.value[i].toString().split(".");
        let subnetStr = subnet.toString().split(".");

        for (let j = 0; j < lanIpaddrStr.length; j++) {
          lanMask.push(parseInt(lanIpaddrStr[j]) & parseInt(subnetStr[j]));
          wanMask.push(parseInt(wanIpaddrStr[j]) & parseInt(subnetStr[j]));
        }

        if (lanMask.join(".") == wanMask.join(".")) {
          debug("Internal/External interface subnet Conflict : " + lanMask);
          return true;
        }
      }
    }
    return false;
  },

  // Transfer CIDR prefix to subnet
  cidrToSubnet(cidr) {
    let mask = [];
    for (let i = 0; i < 4; i++) {
      let n = Math.min(cidr, 8);
      mask.push(256 - Math.pow(2, 8 - n));
      cidr -= n;
    }
    return mask.join(".");
  },

  // Replace non-conflict subnet for tether ipaddr
  refineTetherSubnet(restartTether) {
    if (this._usbTetheringConfig.ip == DEFAULT_USB_IP) {
      debug("setup backup tethering settings");
      this._usbTetheringConfig.ip = BACKUP_USB_IP;
      this._usbTetheringConfig.prefix = BACKUP_USB_PREFIX;
      this._usbTetheringConfig.usbStartIp = BACKUP_USB_DHCPSERVER_STARTIP;
      this._usbTetheringConfig.usbEndIp = BACKUP_USB_DHCPSERVER_ENDIP;
    } else {
      this._usbTetheringConfig.ip = DEFAULT_USB_IP;
      this._usbTetheringConfig.usbStartIp = DEFAULT_USB_DHCPSERVER_STARTIP;
      this._usbTetheringConfig.usbEndIp = DEFAULT_USB_DHCPSERVER_ENDIP;
    }

    TetheringConfigStore.write(
      TetheringConfigStore.TETHERING_TYPE_USB,
      this._usbTetheringConfig,
      null
    );
    this._fireEvent("tetheringconfigchange", {
      usbTetheringConfig: this._usbTetheringConfig,
    });

    if (restartTether) {
      debug(
        "restart USB tethering due to subnet conflict with external interface"
      );
      this._usbTetheringRequestRestart = restartTether;
      this.handleUsbRequest(false, null);
    }
  },

  getIpv6TetheringAddress(aNetworkInfo) {
    if (!aNetworkInfo) {
      return null;
    }

    let ipv6s = {};
    let prefixLengths = {};
    let length = aNetworkInfo.getAddresses(ipv6s, prefixLengths);

    for (let i = 0; i < length; i++) {
      // We only take care about Ipv6 address
      if (!ipv6s.value[i].includes(":")) {
        continue;
      }

      // Ignore link local address
      if (ipv6s.value[i].substring(0, 4) == "fe80") {
        continue;
      }

      return ipv6s.value[i];
    }

    return null;
  },

  _usbTetheringConfig: {},

  get usbTetheringConfig() {
    return this._usbTetheringConfig;
  },

  _setUsbTethering(msg) {
    const message = "TetheringService:setUsbTethering:Return";
    let self = this;
    let enabled = msg.data.enabled;

    // Save the new usb tethering configuration when idle.
    if (
      this._usbTetheringAction == TETHERING_STATE_IDLE &&
      Object.entries(msg.data.config).length
    ) {
      let newConfig = this.fillUSBTetheringConfiguration(msg.data.config);
      if (
        JSON.stringify(this._usbTetheringConfig) != JSON.stringify(newConfig)
      ) {
        this._usbTetheringConfig = newConfig;
        TetheringConfigStore.write(
          TetheringConfigStore.TETHERING_TYPE_USB,
          this._usbTetheringConfig,
          null
        );
        this._fireEvent("tetheringconfigchange", {
          usbTetheringConfig: this._usbTetheringConfig,
        });
      }
    }

    if (
      (enabled && this._usbTetheringAction == TETHERING_STATE_ACTIVE) ||
      (!enabled && this._usbTetheringAction == TETHERING_STATE_IDLE)
    ) {
      msg.data.reason =
        "USB tethering is already " + (enabled ? "enabled" : "disable");
      this._sendMessage(message, true, msg.data, msg);
      return;
    }

    if (this._usbTetheringAction == TETHERING_STATE_ONGOING) {
      msg.data.reason = "previous USB tethering request is ongoing";
      this._sendMessage(message, false, msg.data, msg);
      return;
    }

    this.handleUsbRequest(enabled, {
      usbMessageCallback(success) {
        if (!success) {
          msg.data.reason = enabled
            ? "Enable USB tethering faild"
            : "Disable USB tethering faild";
        }
        self._sendMessage(message, success, msg.data, msg);
      },
    });
  },

  // TetheringManager
  _domManagers: [],

  _fireEvent(message, data) {
    this._domManagers.forEach(function(manager) {
      manager.sendAsyncMessage("TetheringService:" + message, data);
    });
  },

  _sendMessage(message, success, data, msg) {
    try {
      msg.manager.sendAsyncMessage(message + (success ? ":OK" : ":NO"), {
        data,
        rid: msg.rid,
        mid: msg.mid,
      });
    } catch (e) {
      debug("sendAsyncMessage error : " + e);
    }
    this._splicePendingRequest(msg);
  },

  _domRequest: [],

  _splicePendingRequest(msg) {
    for (let i = 0; i < this._domRequest.length; i++) {
      if (this._domRequest[i].msg === msg) {
        this._domRequest.splice(i, 1);
        return;
      }
    }
  },

  receiveMessage(aMessage) {
    let message = aMessage.data || {};
    message.manager = aMessage.target;

    if (aMessage.name === "child-process-shutdown") {
      let i;
      if ((i = this._domManagers.indexOf(message.manager)) != -1) {
        this._domManagers.splice(i, 1);
      }
      for (i = this._domRequest.length - 1; i >= 0; i--) {
        if (this._domRequest[i].msg.manager === message.manager) {
          this._domRequest.splice(i, 1);
        }
      }
      return {};
    }
    // We are interested in DOMRequests only.
    if (aMessage.name != "TetheringService:getStatus") {
      this._domRequest.push({ name: aMessage.name, msg: message });
    }

    switch (aMessage.name) {
      case "TetheringService:setUsbTethering":
        this._setUsbTethering(message);
        break;
      case "TetheringService:getStatus": {
        if (!this._domManagers.includes(message.manager)) {
          this._domManagers.push(message.manager);
        }

        return {
          wifiTetheringState: this.wifiState,
          usbTetheringState: this.usbState,
          usbTetheringConfig: this._usbTetheringConfig,
        };
      }
    }
    return {};
  },
};
