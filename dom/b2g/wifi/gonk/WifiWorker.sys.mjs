/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { libcutils, netHelpers } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);
import { WifiInfo } from "resource://gre/modules/WifiInfo.sys.mjs";
import { WifiNetUtil } from "resource://gre/modules/WifiNetUtil.sys.mjs";
import { WifiCommand } from "resource://gre/modules/WifiCommand.sys.mjs";
import { OpenNetworkNotifier } from "resource://gre/modules/OpenNetworkNotifier.sys.mjs";
import { WifiNetworkSelector } from "resource://gre/modules/WifiNetworkSelector.sys.mjs";
import { WifiConfigManager } from "resource://gre/modules/WifiConfigManager.sys.mjs";
import {
  WifiScanSettings,
  WifiPnoSettings,
} from "resource://gre/modules/WifiScanSettings.sys.mjs";
import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

import { WifiConstants } from "resource://gre/modules/WifiConstants.sys.mjs";
import {
  ScanResult,
  WifiNetwork,
  WifiConfigUtils,
} from "resource://gre/modules/WifiConfiguration.sys.mjs";

const { TetheringConfigStore } = ChromeUtils.import(
  "resource://gre/modules/TetheringConfigStore.jsm"
);
import { BinderServices } from "resource://gre/modules/BinderServices.sys.mjs";

import { PasspointManager } from "resource://gre/modules/PasspointManager.sys.mjs";

var gDebug = false;

/* eslint-disable no-unused-vars */
const FEATURE_APF = Ci.nsIWifiResult.FEATURE_APF;
const FEATURE_BACKGROUND_SCAN = Ci.nsIWifiResult.FEATURE_BACKGROUND_SCAN;
const FEATURE_LINK_LAYER_STATS = Ci.nsIWifiResult.FEATURE_LINK_LAYER_STATS;
const FEATURE_RSSI_MONITOR = Ci.nsIWifiResult.FEATURE_RSSI_MONITOR;
const FEATURE_CONTROL_ROAMING = Ci.nsIWifiResult.FEATURE_CONTROL_ROAMING;
const FEATURE_PROBE_IE_ALLOWLIST = Ci.nsIWifiResult.FEATURE_PROBE_IE_ALLOWLIST;
const FEATURE_SCAN_RAND = Ci.nsIWifiResult.FEATURE_SCAN_RAND;
const FEATURE_STA_5G = Ci.nsIWifiResult.FEATURE_STA_5G;
const FEATURE_HOTSPOT = Ci.nsIWifiResult.FEATURE_HOTSPOT;
const FEATURE_PNO = Ci.nsIWifiResult.FEATURE_PNO;
const FEATURE_TDLS = Ci.nsIWifiResult.FEATURE_TDLS;
const FEATURE_TDLS_OFFCHANNEL = Ci.nsIWifiResult.FEATURE_TDLS_OFFCHANNEL;
const FEATURE_ND_OFFLOAD = Ci.nsIWifiResult.FEATURE_ND_OFFLOAD;
const FEATURE_KEEP_ALIVE = Ci.nsIWifiResult.FEATURE_KEEP_ALIVE;
const FEATURE_DEBUG_PACKET_FATE = Ci.nsIWifiResult.FEATURE_DEBUG_PACKET_FATE;
const FEATURE_DEBUG_MEMORY_FIRMWARE_DUMP =
  Ci.nsIWifiResult.FEATURE_DEBUG_MEMORY_FIRMWARE_DUMP;
const FEATURE_DEBUG_MEMORY_DRIVER_DUMP =
  Ci.nsIWifiResult.FEATURE_DEBUG_MEMORY_DRIVER_DUMP;
const FEATURE_DEBUG_RING_BUFFER_CONNECT_EVENT =
  Ci.nsIWifiResult.FEATURE_DEBUG_RING_BUFFER_CONNECT_EVENT;
const FEATURE_DEBUG_RING_BUFFER_POWER_EVENT =
  Ci.nsIWifiResult.FEATURE_DEBUG_RING_BUFFER_POWER_EVENT;
const FEATURE_DEBUG_RING_BUFFER_WAKELOCK_EVENT =
  Ci.nsIWifiResult.FEATURE_DEBUG_RING_BUFFER_WAKELOCK_EVENT;
const FEATURE_DEBUG_RING_BUFFER_VENDOR_DATA =
  Ci.nsIWifiResult.FEATURE_DEBUG_RING_BUFFER_VENDOR_DATA;
const FEATURE_DEBUG_HOST_WAKE_REASON_STATS =
  Ci.nsIWifiResult.FEATURE_DEBUG_HOST_WAKE_REASON_STATS;
const FEATURE_DEBUG_ERROR_ALERTS = Ci.nsIWifiResult.FEATURE_DEBUG_ERROR_ALERTS;

const SUCCESS = Ci.nsIWifiResult.SUCCESS;
const ERROR_COMMAND_FAILED = Ci.nsIWifiResult.ERROR_COMMAND_FAILED;
const ERROR_INVALID_INTERFACE = Ci.nsIWifiResult.ERROR_INVALID_INTERFACE;
const ERROR_INVALID_ARGS = Ci.nsIWifiResult.ERROR_INVALID_ARGS;
const ERROR_NOT_SUPPORTED = Ci.nsIWifiResult.ERROR_NOT_SUPPORTED;
const ERROR_TIMEOUT = Ci.nsIWifiResult.ERROR_TIMEOUT;
const ERROR_UNKNOWN = Ci.nsIWifiResult.ERROR_UNKNOWN;

const AP_BAND_24GHZ = Ci.nsISoftapConfiguration.AP_BAND_24GHZ;
const AP_BAND_5GHZ = Ci.nsISoftapConfiguration.AP_BAND_5GHZ;
const AP_BAND_ANY = Ci.nsISoftapConfiguration.AP_BAND_ANY;
const AP_SECURITY_NONE = Ci.nsISoftapConfiguration.SECURITY_NONE;
const AP_SECURITY_WPA = Ci.nsISoftapConfiguration.SECURITY_WPA;
const AP_SECURITY_WPA2 = Ci.nsISoftapConfiguration.SECURITY_WPA2;

const LEVEL_EXCESSIVE = Ci.nsISupplicantDebugLevel.LOG_EXCESSIVE;
const LEVEL_MSGDUMP = Ci.nsISupplicantDebugLevel.LOG_MSGDUMP;
const LEVEL_DEBUG = Ci.nsISupplicantDebugLevel.LOG_DEBUG;
const LEVEL_INFO = Ci.nsISupplicantDebugLevel.LOG_INFO;
const LEVEL_WARNING = Ci.nsISupplicantDebugLevel.LOG_WARNING;
const LEVEL_ERROR = Ci.nsISupplicantDebugLevel.LOG_ERROR;

const USE_SINGLE_SCAN = Ci.nsIScanSettings.USE_SINGLE_SCAN;
const USE_PNO_SCAN = Ci.nsIScanSettings.USE_PNO_SCAN;

const AUTH_FAILURE_UNKNOWN = Ci.nsIWifiEvent.AUTH_FAILURE_UNKNOWN;
const AUTH_FAILURE_NONE = Ci.nsIWifiEvent.AUTH_FAILURE_NONE;
const AUTH_FAILURE_TIMEOUT = Ci.nsIWifiEvent.AUTH_FAILURE_TIMEOUT;
const AUTH_FAILURE_WRONG_KEY = Ci.nsIWifiEvent.AUTH_FAILURE_WRONG_KEY;
const AUTH_FAILURE_EAP_FAILURE = Ci.nsIWifiEvent.AUTH_FAILURE_EAP_FAILURE;

const ERROR_EAP_SIM_NOT_SUBSCRIBED =
  Ci.nsIWifiEvent.ERROR_EAP_SIM_NOT_SUBSCRIBED;
const ERROR_EAP_SIM_VENDOR_SPECIFIC_EXPIRED_CERT =
  Ci.nsIWifiEvent.ERROR_EAP_SIM_VENDOR_SPECIFIC_EXPIRED_CERT;

const WIFIWORKER_CONTRACTID = "@mozilla.org/wifi/worker;1";
const WIFIWORKER_CID = Components.ID("{a14e8977-d259-433a-a88d-58dd44657e5b}");

const kFinalUiStartUpTopic = "final-ui-startup";
const kXpcomShutdownChangedTopic = "xpcom-shutdown";
const kScreenStateChangedTopic = "screen-state-changed";
const kInterfaceAddressUpdatedTopic = "interface-address-updated";
const kInterfaceAddressRemovedTopic = "interface-address-removed";
const kInterfaceDnsInfoTopic = "interface-dns-info";
const kRouteChangedTopic = "route-change";
const kPrefDefaultServiceId = "dom.telephony.defaultServiceId";
const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const kCaptivePortalResult = "captive-portal-result";
const kOpenCaptivePortalLoginEvent = "captive-portal-login";
const kCaptivePortalLoginSuccessEvent = "captive-portal-login-success";

const MAX_SUPPLICANT_LOOP_ITERATIONS = 4;

// Settings DB for wifi debugging
const SETTINGS_WIFI_DEBUG_ENABLED = "wifi.debugging.enabled";

// Settings DB for airplane mode.
const SETTINGS_AIRPLANE_MODE = "airplaneMode.enabled";
const SETTINGS_AIRPLANE_MODE_STATUS = "airplaneMode.status";

// Settings DB for the list of imported wifi certificate nickname
const SETTINGS_WIFI_CERT_NICKNAME = "wifi.certificate.nickname";

// Settings DB for passpoint
const SETTINGS_PASSPOINT_ENABLED = "wifi.passpoint.enabled";

// Preference for wifi persist state
const PREFERENCE_WIFI_ENABLED = "persist.wifi.enabled";
// Preference for open network notification persist state
const PREFERENCE_WIFI_NOTIFICATION = "persist.wifi.notification";

// Default value for WIFI tethering.
const DEFAULT_HOTSPOT_IP = "192.168.1.1";
const DEFAULT_HOTSPOT_PREFIX = "24";
const DEFAULT_HOTSPOT_CHANNEL = 1;
const DEFAULT_HOTSPOT_BAND = AP_BAND_24GHZ;
const DEFAULT_HOTSPOT_ENABLE_11N = true;
const DEFAULT_HOTSPOT_ENABLE_11AC = false;
const DEFAULT_HOTSPOT_ENABLE_ACS = false;
const DEFAULT_HOTSPOT_DHCPSERVER_STARTIP = "192.168.1.10";
const DEFAULT_HOTSPOT_DHCPSERVER_ENDIP = "192.168.1.30";
const DEFAULT_HOTSPOT_SSID = "FirefoxHotspot";
const DEFAULT_HOTSPOT_SECURITY_TYPE = "open";
const DEFAULT_HOTSPOT_SECURITY_PASSWORD = "1234567890";
const DEFAULT_HOTSPOT_HIDDEN = false;

// Default value for USB tethering.
const DEFAULT_USB_DHCPSERVER_STARTIP = "192.168.0.10";
const DEFAULT_USB_DHCPSERVER_ENDIP = "192.168.0.30";

const WIFI_FIRMWARE_AP = "AP";
const WIFI_FIRMWARE_STATION = "STA";
const WIFI_SECURITY_TYPE_NONE = "open";
const WIFI_SECURITY_TYPE_WPA_PSK = "wpa-psk";
const WIFI_SECURITY_TYPE_WPA2_PSK = "wpa2-psk";

const NETWORK_INTERFACE_UP = "up";
const NETWORK_INTERFACE_DOWN = "down";

const DEFAULT_WLAN_INTERFACE = "wlan0";

const DHCP_PROP = "init.svc.dhcpcd";

const POWER_MODE_DHCP = 1;
const POWER_MODE_SCREEN_STATE = 1 << 1;
const POWER_MODE_SETTING_CHANGED = 1 << 2;
/* eslint-enable no-unused-vars */

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "ppmm", () => {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
});

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
  "gTetheringService",
  "@mozilla.org/tethering/service;1",
  "nsITetheringService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gImsRegService",
  "@mozilla.org/mobileconnection/imsregservice;1",
  "nsIImsRegService"
);
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const hasRilSupport =
  "MOZ_B2G_RIL" in AppConstants ? AppConstants.MOZ_B2G_RIL : false;

if (hasRilSupport) {
  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "gTelephonyService",
    "@mozilla.org/telephony/telephonyservice;1",
    "nsITelephonyService"
  );
}

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gPhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm",
  "PhoneNumberUtils"
);

var wifiInfo = new WifiInfo();
var lastNetwork = null;
var autoRoaming = false;

function debug(s) {
  if (gDebug) {
    dump("-*- WifiWorker component: " + s + "\n");
  }
}

// A note about errors and error handling in this file:
// The libraries that we use in this file are intended for C code. For
// C code, it is natural to return -1 for errors and 0 for success.
// Therefore, the code that interacts directly with the worker uses this
// convention (note: command functions do get boolean results since the
// command always succeeds and we do a string/boolean check for the
// expected results).
var WifiManager = (function() {
  var manager = {};

  function getStartupPrefs() {
    return {
      sdkVersion: parseInt(libcutils.property_get("ro.build.version.sdk"), 10),
      schedScanRecovery:
        libcutils.property_get("ro.moz.wifi.sched_scan_recover") !== "false",
      eapSimSupported:
        libcutils.property_get("ro.moz.wifi.eapsim_supported", "true") ===
        "true",
      ibssSupported:
        libcutils.property_get("ro.moz.wifi.ibss_supported", "true") === "true",
      ifname: libcutils.property_get("wifi.interface"),
      numRil: hasRilSupport
        ? Services.prefs.getIntPref("ril.numRadioInterfaces")
        : 0,
      wapiKeyType: libcutils.property_get(
        "ro.wifi.wapi.wapi_key_type",
        "wapi_key_type"
      ),
      wapiPsk: libcutils.property_get("ro.wifi.wapi.wapi_psk", "wapi_psk"),
      asCertFile: libcutils.property_get(
        "ro.wifi.wapi.as_cert_file",
        "as_cert_file"
      ),
      userCertFile: libcutils.property_get(
        "ro.wifi.wapi.user_cert_file",
        "user_cert_file"
      ),
    };
  }

  let {
    sdkVersion,
    schedScanRecovery,
    eapSimSupported,
    ibssSupported,
    ifname,
    numRil,
    wapiKeyType,
    wapiPsk,
    asCertFile,
    userCertFile,
  } = getStartupPrefs();

  manager.wapiKeyType = wapiKeyType;
  manager.wapiPsk = wapiPsk;
  manager.asCertFile = asCertFile;
  manager.userCertFile = userCertFile;

  let capabilities = {
    security: [
      "OPEN",
      "WEP",
      "WPA-PSK",
      "WPA-EAP",
      "SAE",
      "WAPI-PSK",
      "WAPI-CERT",
    ],
    eapMethod: ["PEAP", "TTLS", "TLS"],
    eapPhase2: ["PAP", "MSCHAP", "MSCHAPV2", "GTC"],
    certificate: ["SERVER", "USER"],
    mode: [WifiConstants.MODE_ESS],
  };
  if (eapSimSupported) {
    capabilities.eapMethod.unshift("SIM");
    capabilities.eapMethod.unshift("AKA");
    capabilities.eapMethod.unshift("AKA'");
  }
  if (ibssSupported) {
    capabilities.mode.push(WifiConstants.MODE_IBSS);
  }
  if (PasspointManager) {
    PasspointManager.setWifiManager(manager);
    PasspointManager.loadFromStore();
  }

  // Features supported by vendor module.
  let supportedFeatures = 0;

  let wifiListener = {
    onEventCallback(event, iface) {
      handleEvent(event, iface);
    },

    onCommandResult(result, iface) {
      onmessageresult(result, iface);
    },
  };

  manager.ifname = ifname;
  // Emulator build runs to here.
  // The debug() should only be used after WifiManager.
  if (!ifname) {
    manager.ifname = DEFAULT_WLAN_INTERFACE;
  }
  manager.schedScanRecovery = schedScanRecovery;

  // Regular Wifi stuff.
  var netUtil = WifiNetUtil(controlMessage);
  var wifiCommand = WifiCommand(controlMessage, manager.ifname, sdkVersion);

  let wifiService = Cc["@mozilla.org/wifi/service;1"];
  if (wifiService) {
    wifiService = wifiService.getService(Ci.nsIWifiProxyService);
    let interfaces = [manager.ifname];
    wifiService.start(wifiListener, interfaces, interfaces.length);
  } else {
    debug("No wifi service component available!");
  }

  // Callbacks to invoke when a reply arrives from the wifi service.
  var controlCallbacks = Object.create(null);
  var idgen = 0;

  function controlMessage(obj, callback) {
    var id = idgen++;
    obj.id = id;
    if (callback) {
      controlCallbacks[id] = callback;
    }
    wifiService.sendCommand(obj, obj.iface);
  }

  let onmessageresult = function(data, iface) {
    var id = data.id;
    var callback = controlCallbacks[id];
    if (callback) {
      callback(data);
      delete controlCallbacks[id];
    }
  };

  // initialize wifi hal
  halInitialize(function() {});

  // Commands to the control worker.
  function halInitialize(callback) {
    wifiCommand.initialize(function(result) {
      callback(result.status == SUCCESS);
    });
  }

  var screenOn = true;
  function handleScreenStateChanged(enabled) {
    screenOn = enabled;
    if (!manager.isWifiEnabled(manager.state)) {
      return;
    }

    setSuspendOptimizationsMode(
      POWER_MODE_SCREEN_STATE,
      !screenOn,
      function() {}
    );

    if (screenOn) {
      fullBandConnectedTimeIntervalMilli =
        WifiConstants.WIFI_ASSOCIATED_SCAN_INTERVAL;
      enableBackgroundScan(false);
      manager.startDelayScan();
      notify("startconnectioninfotimer");
      // If screen on from a long time off, it should be better to reset traffic stats.
      // Otherwise, screen on from short time off, it is harmless.
      manager.lastTrafficStats = null;
    } else {
      if (manager.isConnectState(manager.state)) {
        enableBackgroundScan(false);
      } else {
        enableBackgroundScan(true);
      }
      manager.stopDelayScan();
      notify("stopconnectioninfotimer");
    }
  }

  manager.isScreenOn = function() {
    return screenOn;
  };

  // PNO (Preferred Network Offload):
  // device will search known networks if device disconnected and screen off.
  // It will only work in wlan FW without waking system up.
  // Once matched network found, it will wakeup system to start reconnect.
  var pnoEnabled = false;
  var schedulePnoFailed = false;
  function enableBackgroundScan(enable, callback) {
    pnoEnabled = enable;
    if (enable) {
      let pnoSettings = WifiPnoSettings.pnoSettings;
      pnoSettings.interval = WifiPnoSettings.interval;
      pnoSettings.min2gRssi = WifiPnoSettings.min2gRssi;
      pnoSettings.min5gRssi = WifiPnoSettings.min5gRssi;

      let network = {};
      let configuredNetworks = WifiConfigManager.configuredNetworks;
      for (let networkKey in configuredNetworks) {
        // TODO: memory size in firmware is limited,
        //       so we should optimize the pno list.
        let config = configuredNetworks[networkKey];
        network.ssid = config.ssid;
        network.isHidden = config.scanSsid;
        network.frequencies = manager.configurationChannels.get(config.netId);
        WifiPnoSettings.pnoNetworks.push(network);
      }
      pnoSettings.pnoNetworks = WifiPnoSettings.pnoNetworks;

      if (!pnoSettings.pnoNetworks.length) {
        debug("Empty PNO network list");
        if (callback) {
          callback(false);
        }
        return;
      }

      wifiCommand.startPnoScan(pnoSettings, function(result) {
        if (callback) {
          callback(result.status == SUCCESS);
        }
      });
    } else {
      wifiCommand.stopPnoScan(function(result) {
        if (callback) {
          callback(result.status == SUCCESS);
        }
      });
    }
  }

  function updateChannels(callback) {
    WifiScanSettings.bandMask = WifiScanSettings.BAND_2_4_GHZ;
    if (manager.isFeatureSupported(FEATURE_STA_5G)) {
      WifiScanSettings.bandMask |=
        WifiScanSettings.BAND_5_GHZ | WifiScanSettings.BAND_5_GHZ_DFS;
    }

    wifiCommand.getChannelsForBand(WifiScanSettings.bandMask, function(result) {
      let channels = result.getChannels();
      if (channels.length) {
        WifiScanSettings.channels = channels;
      }
      callback(result.status == SUCCESS);
    });
  }

  manager.lastTrafficStats = null;
  function trafficHeavy(callback) {
    wifiCommand.getLinkLayerStats(function(result) {
      if (result.status != SUCCESS) {
        debug("getLinkLayerStats failed");
        callback(false);
        return;
      }

      let linkLayerStats = result.linkLayerStats;
      let trafficOverThreshold = false;
      let txSuccessRate = 0;
      let rxSuccessRate = 0;
      let lastTrafficStats = manager.lastTrafficStats;

      if (lastTrafficStats != null) {
        let lastLinkLayerStats = lastTrafficStats.lastLinkLayerStats;
        txSuccessRate = lastTrafficStats.txSuccessRate;
        rxSuccessRate = lastTrafficStats.rxSuccessRate;

        let be = linkLayerStats.wmeBePktStats;
        let bk = linkLayerStats.wmeBkPktStats;
        let vi = linkLayerStats.wmeViPktStats;
        let vo = linkLayerStats.wmeVoPktStats;
        let lbe = lastLinkLayerStats.wmeBePktStats;
        let lbk = lastLinkLayerStats.wmeBkPktStats;
        let lvi = lastLinkLayerStats.wmeViPktStats;
        let lvo = lastLinkLayerStats.wmeVoPktStats;

        let txSuccessDelta =
          be.txMpdu +
          bk.txMpdu +
          vi.txMpdu +
          vo.txMpdu -
          (lbe.txMpdu + lbk.txMpdu + lvi.txMpdu + lvo.txMpdu);
        let rxSuccessDelta =
          be.rxMpdu +
          bk.rxMpdu +
          vi.rxMpdu +
          vo.rxMpdu -
          (lbe.rxMpdu + lbk.rxMpdu + lvi.rxMpdu + lvo.rxMpdu);
        let timeMsDelta =
          linkLayerStats.timeStampMs - lastLinkLayerStats.timeStampMs;
        let lastSampleWeight = Math.exp(
          (-1.0 * timeMsDelta) / WifiConstants.FILTER_TIME_CONSTANT
        );
        let currentSampleWeight = 1.0 - lastSampleWeight;

        debug(
          "Traffic state:" +
            " txSuccessDelta=" +
            txSuccessDelta +
            ", rxSuccessDelta=" +
            rxSuccessDelta +
            ", timeMsDelta=" +
            timeMsDelta +
            ", lastSampleWeight=" +
            lastSampleWeight.toFixed(5)
        );

        txSuccessRate =
          txSuccessRate * lastSampleWeight +
          ((txSuccessDelta * 1000.0) / timeMsDelta) * currentSampleWeight;
        rxSuccessRate =
          rxSuccessRate * lastSampleWeight +
          ((rxSuccessDelta * 1000.0) / timeMsDelta) * currentSampleWeight;
        trafficOverThreshold =
          txSuccessRate > WifiConstants.FULL_SCAN_MAX_TX_RATE ||
          rxSuccessRate > WifiConstants.FULL_SCAN_MAX_RX_RATE;

        debug(
          "Traffic state:" +
            " txSuccessRate=" +
            txSuccessRate.toFixed(2) +
            ", rxSuccessRate=" +
            rxSuccessRate.toFixed(2) +
            ", trafficOverThreshold=" +
            trafficOverThreshold
        );
      } else {
        lastTrafficStats = Object.create(null);
      }

      lastTrafficStats.txSuccessRate = txSuccessRate;
      lastTrafficStats.rxSuccessRate = rxSuccessRate;
      lastTrafficStats.lastLinkLayerStats = linkLayerStats;
      manager.lastTrafficStats = lastTrafficStats;

      callback(trafficOverThreshold);
    });
  }

  function handleScanRequest(fullScan, callback) {
    if (!manager.enabled) {
      debug("WiFi is off, skip scan request");
      callback(false);
      return;
    }

    updateChannels(function(ok) {
      if (!ok) {
        debug("Failed to get supported channels");
      }

      if (OpenNetworkNotifier && OpenNetworkNotifier.isEnabled()) {
        fullScan = true;
      }

      // Concat all channels into an array.
      let configChannels = [];
      function concatChannels(value, key, map) {
        configChannels = configChannels.concat(value);
      }
      manager.configurationChannels.forEach(concatChannels);
      configChannels = configChannels.filter(
        (value, index) => configChannels.indexOf(value) === index
      );

      let scanSettings = WifiScanSettings.singleScanSettings;
      scanSettings.scanType = WifiScanSettings.SCAN_TYPE_HIGH_ACCURACY;
      scanSettings.channels =
        fullScan || configChannels.length === 0
          ? WifiScanSettings.channels
          : configChannels;
      scanSettings.hiddenNetworks = WifiConfigManager.getHiddenNetworks();

      if (!scanSettings.channels.length) {
        callback(false);
      }

      wifiCommand.startScan(scanSettings, function(result) {
        if (callback) {
          callback(result.status == SUCCESS);
        }
      });
    });
  }

  var delayScanInterval = WifiConstants.WIFI_SCHEDULED_SCAN_INTERVAL;
  var delayScanId = null;
  var fullBandConnectedTimeIntervalMilli =
    WifiConstants.WIFI_ASSOCIATED_SCAN_INTERVAL;
  var maxFullBandConnectedTimeIntervalMilli = 5 * 60 * 1000;
  var lastFullBandConnectedTimeMilli = -1;
  manager.configurationChannels = new Map();
  manager.startDelayScan = function() {
    clearTimeout(delayScanId);
    debug(
      "startDelayScan: manager.state=" + manager.state + " screenOn=" + screenOn
    );

    if (schedulePnoFailed) {
      schedulePnoFailed = false;
      enableBackgroundScan(pnoEnabled);
    }

    if (screenOn) {
      if (manager.state == "COMPLETED") {
        delayScanInterval = WifiConstants.WIFI_ASSOCIATED_SCAN_INTERVAL;

        let tryFullBandScan = false;
        var now_ms = Date.now();
        debug(
          "start delay scan with age=" +
            (now_ms - lastFullBandConnectedTimeMilli) +
            " full scan interval=" +
            fullBandConnectedTimeIntervalMilli +
            " maxinterval=" +
            maxFullBandConnectedTimeIntervalMilli
        );
        if (
          now_ms - lastFullBandConnectedTimeMilli >
          fullBandConnectedTimeIntervalMilli
        ) {
          tryFullBandScan = true;
        }
        // TODO: Don't scan if lots of packets are being sent.

        // If the WiFi traffic is heavy, only partial scan is proposed.
        trafficHeavy(function(isTrafficHeavy) {
          if (isTrafficHeavy) {
            tryFullBandScan = false;
          }

          if (!tryFullBandScan && manager.configurationChannels.size > 0) {
            handleScanRequest(false, function() {});
          } else {
            lastFullBandConnectedTimeMilli = now_ms;
            if (
              fullBandConnectedTimeIntervalMilli <
              maxFullBandConnectedTimeIntervalMilli
            ) {
              // Increase the interval
              fullBandConnectedTimeIntervalMilli =
                (fullBandConnectedTimeIntervalMilli * 12) / 8;
            }

            handleScanRequest(true, function() {});
            delayScanInterval = fullBandConnectedTimeIntervalMilli;
          }
        });
      } else if (!manager.isConnectState(manager.state)) {
        delayScanInterval = WifiConstants.WIFI_SCHEDULED_SCAN_INTERVAL;
        handleScanRequest(true, function() {});
      }
    }

    delayScanId = setTimeout(manager.startDelayScan, delayScanInterval);
  };

  manager.stopDelayScan = function() {
    clearTimeout(delayScanId);
  };

  function syncSupplicantDebug(enable, callback) {
    let level = {
      logLevel: enable ? LEVEL_DEBUG : LEVEL_INFO,
      showTimeStamp: false,
      showKeys: false,
    };

    wifiCommand.setDebugLevel(level, function(result) {
      callback(result.status == SUCCESS);
    });
  }

  function syncDebug() {
    let enable = gDebug;
    if (WifiNetworkSelector) {
      WifiNetworkSelector.setDebug(enable);
    }
    if (WifiConfigManager) {
      WifiConfigManager.setDebug(enable);
    }
    if (OpenNetworkNotifier) {
      OpenNetworkNotifier.setDebug(enable);
    }
    if (WifiConfigUtils) {
      WifiConfigUtils.setDebug(enable);
    }
    if (PasspointManager) {
      PasspointManager.setDebug(enable);
    }
    if (manager.supplicantStarted) {
      syncSupplicantDebug(enable, function() {});
    }
  }

  var httpProxyConfig = Object.create(null);

  /**
   * Given a network, configure http proxy when using wifi.
   * @param network A network object to update http proxy
   * @param info Info should have following field:
   *        - httpProxyHost ip address of http proxy.
   *        - httpProxyPort port of http proxy, set 0 to use default port 8080.
   * @param callback callback function.
   */
  function configureHttpProxy(network, info, callback) {
    if (!network) {
      return;
    }

    let networkKey = WifiConfigUtils.getNetworkKey(network);

    if (!info || info.httpProxyHost === "") {
      delete httpProxyConfig[networkKey];
    } else {
      httpProxyConfig[networkKey] = network;
      httpProxyConfig[networkKey].httpProxyHost = info.httpProxyHost;
      httpProxyConfig[networkKey].httpProxyPort = info.httpProxyPort;
    }

    callback(true);
  }

  function getHttpProxyNetwork(network) {
    if (!network) {
      return null;
    }

    let networkKey = WifiConfigUtils.getNetworkKey(network);
    return httpProxyConfig[networkKey];
  }

  function setHttpProxy(network) {
    if (!network) {
      return;
    }

    // If we got here, arg network must be the current network, so we just update
    // WifiNetworkInterface correspondingly and notify NetworkManager.
    WifiNetworkInterface.httpProxyHost = network.httpProxyHost;
    WifiNetworkInterface.httpProxyPort = network.httpProxyPort;

    if (
      WifiNetworkInterface.info.state ==
      Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
    ) {
      lazy.gNetworkManager.updateNetworkInterface(WifiNetworkInterface);
    }
  }

  var staticIpConfig = Object.create(null);
  function setStaticIpMode(network, info, callback) {
    let setNetworkKey = WifiConfigUtils.getNetworkKey(network);
    let curNetworkKey = null;
    let current = Object.create(null);
    current.netId = wifiInfo.networkId;

    WifiConfigManager.fetchNetworkConfiguration(current, function() {
      curNetworkKey = WifiConfigUtils.getNetworkKey(current);

      // Add additional information to static ip configuration
      // It is used to compatiable with information dhcp callback.
      info.ipaddr = netHelpers.stringToIP(info.ipaddr_str);
      info.gateway = netHelpers.stringToIP(info.gateway_str);
      info.mask_str = netHelpers.ipToString(
        netHelpers.makeMask(info.maskLength)
      );

      // Optional
      info.dns1 = netHelpers.stringToIP(info.dns1_str);
      info.dns2 = netHelpers.stringToIP(info.dns2_str);
      info.proxy = netHelpers.stringToIP(info.proxy_str);

      staticIpConfig[setNetworkKey] = info;

      // If the ssid of current connection is the same as configured ssid
      // It means we need update current connection to use static IP address.
      if (setNetworkKey == curNetworkKey) {
        // Use configureInterface directly doesn't work, the network interface
        // and routing table is changed but still cannot connect to network
        // so the workaround here is disable interface the enable again to
        // trigger network reconnect with static ip.
        lazy.gNetworkService.setInterfaceConfig(
          { ifname: manager.ifname, link: "down" },
          function(ok) {
            lazy.gNetworkService.setInterfaceConfig(
              { ifname: manager.ifname, link: "up" },
              function(ok) {
                callback(ok);
              }
            );
          }
        );
        return;
      }

      callback(true);
    });
  }

  var dhcpInfo = null;

  function runStaticIp(ifname, key) {
    debug("Run static ip");

    // TODO: NetworkService no longer support libnetutils.
    /*
    // Read static ip information from settings.
    let staticIpInfo;

    if (!(key in staticIpConfig)) {
      return;
    }

    staticIpInfo = staticIpConfig[key];

    // Stop dhcpd when use static IP
    if (dhcpInfo != null) {
      netUtil.stopDhcp(manager.ifname, function() {});
    }

    // Set ip, mask length, gateway, dns to network interface
    lazy.gNetworkService.configureInterface(
      {
        ifname,
        ipaddr: staticIpInfo.ipaddr,
        mask: staticIpInfo.maskLength,
        gateway: staticIpInfo.gateway,
        dns1: staticIpInfo.dns1,
        dns2: staticIpInfo.dns2,
      },
      function(data) {
        netUtil.runIpConfig(ifname, staticIpInfo, function(data) {
          dhcpInfo = data.info;
          wifiInfo.setInetAddress(dhcpInfo.ipaddr_str);
          notify("networkconnected", data);
        });
      }
    );
    */
  }

  var suppressEvents = false;
  function notify(eventName, eventObject) {
    if (suppressEvents) {
      return;
    }
    var handler = manager["on" + eventName];
    if (handler) {
      if (!eventObject) {
        eventObject = {};
      }
      handler.call(eventObject);
    }
  }

  function notifyStateChange(fields) {
    // If screen is off, enable background scan when wifi is disconnected,
    // or disable background scan when wifi is connecting.
    if (!screenOn) {
      if (
        manager.isConnectState(manager.state) &&
        fields.state === "DISCONNECTED"
      ) {
        enableBackgroundScan(true);
      } else if (manager.isConnectState(fields.state)) {
        enableBackgroundScan(false);
      }
    }

    fields.prevState = manager.state;
    // Detect wpa_supplicant's loop iterations.
    manager.supplicantLoopDetection(fields.prevState, fields.state);
    notify("statechange", fields);

    // Don't update state when and after disabling.
    if (manager.state === "DISABLING" || manager.state === "UNINITIALIZED") {
      return;
    }
    manager.state = fields.state;
  }

  let dhcpRequestGen = 0;
  function onconnected() {
    let current = Object.create(null);
    current.netId = wifiInfo.networkId;
    // Clear the bssid in the current config's network block
    WifiConfigManager.clearCurrentConfigBssid(current.netId, function() {});

    // For now we do our own DHCP. In the future, this should be handed
    // off to the Network Manager or IpClient relate.
    // Start IPv6 provision.
    lazy.gNetworkService.setIpv6Status(manager.ifname, true, function() {});

    // Start IPv4 discovery.
    WifiConfigManager.fetchNetworkConfiguration(current, function() {
      let key = WifiConfigUtils.getNetworkKey(current);
      if (
        staticIpConfig &&
        key in staticIpConfig &&
        staticIpConfig[key].enabled
      ) {
        debug("Run static ip");
        runStaticIp(manager.ifname, key);
        return;
      }

      preDhcpSetup();
      netUtil.runDhcp(manager.ifname, dhcpRequestGen++, function(data, gen) {
        dhcpInfo = data.info;
        debug("dhcpRequestGen: " + dhcpRequestGen + ", gen: " + gen);
        // Only handle dhcp response in COMPLETED state.
        if (manager.state !== "COMPLETED") {
          postDhcpSetup(function(ok) {});
          return;
        }

        if (!dhcpInfo) {
          wifiInfo.setInetAddress(null);
          if (gen + 1 < dhcpRequestGen) {
            debug("Do not bother younger DHCP request.");
            return;
          }
          WifiConfigManager.updateNetworkSelectionStatus(
            lastNetwork.netId,
            WifiConstants.DISABLED_DHCP_FAILURE,
            function(doDisable) {
              if (doDisable) {
                notify("networkdisable", {
                  reason: WifiConstants.DISABLED_DHCP_FAILURE,
                });
              } else {
                WifiManager.disconnect(function() {
                  WifiManager.reassociate(function() {});
                });
              }
            }
          );
        } else {
          wifiInfo.setInetAddress(dhcpInfo.ipaddr_str);
          notify("networkconnected", data);
        }
      });
    });
  }

  function preDhcpSetup() {
    manager.inObtainingIpState = true;
    // Hold wakelock during doing DHCP
    acquireWifiWakeLock();

    // Disable power saving mode when doing dhcp
    setSuspendOptimizationsMode(POWER_MODE_DHCP, false, function() {});
    manager.setPowerSave(false, function() {});
  }

  function postDhcpSetup(callback) {
    if (!manager.inObtainingIpState) {
      callback(true);
      return;
    }
    setSuspendOptimizationsMode(POWER_MODE_DHCP, true, function(ok) {
      manager.setPowerSave(true, function(result) {
        // Release wakelock during doing DHCP
        releaseWifiWakeLock();

        manager.inObtainingIpState = false;
        callback(result.status == SUCCESS);
      });
    });
  }

  var wifiWakeLock = null;
  var wifiWakeLockTimer = null;
  function acquireWifiWakeLock() {
    if (!wifiWakeLock) {
      debug("Acquiring Wifi Wakelock");
      wifiWakeLock = lazy.gPowerManagerService.newWakeLock("cpu");
    }
    if (!wifiWakeLockTimer) {
      debug("Creating Wifi WakeLock Timer");
      wifiWakeLockTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }
    function onTimeout() {
      releaseWifiWakeLock();
    }
    debug("Setting Wifi WakeLock Timer");
    wifiWakeLockTimer.initWithCallback(onTimeout, 60000, Ci.nsITimer.ONE_SHOT);
  }

  function releaseWifiWakeLock() {
    debug("Releasing Wifi WakeLock");
    if (wifiWakeLockTimer) {
      wifiWakeLockTimer.cancel();
    }
    if (wifiWakeLock) {
      wifiWakeLock.unlock();
      wifiWakeLock = null;
    }
  }

  var supplicantStatesMap = [
    "DISCONNECTED",
    "INTERFACE_DISABLED",
    "INACTIVE",
    "SCANNING",
    "AUTHENTICATING",
    "ASSOCIATING",
    "ASSOCIATED",
    "FOUR_WAY_HANDSHAKE",
    "GROUP_HANDSHAKE",
    "COMPLETED",
    "UNINITIALIZED",
  ];

  var eventTable = new Map([
    ["SUPPLICANT_STATE_CHANGED", supplicantStateChanged],
    ["SUPPLICANT_TERMINATING", supplicantTerminating],
    ["SUPPLICANT_NETWORK_CONNECTED", supplicantNetworkConnected],
    ["SUPPLICANT_NETWORK_DISCONNECTED", supplicantNetworkDisconnected],
    ["SUPPLICANT_TARGET_BSSID", supplicantTargetBssid],
    ["SUPPLICANT_ASSOCIATED_BSSID", supplicantAssocistedBssid],
    ["SUPPLICANT_AUTH_FAILURE", supplicantAuthenticationFailure],
    ["SUPPLICANT_ASSOC_REJECT", supplicantAssociationReject],
    ["EAP_SIM_GSM_AUTH_REQUEST", eapSimGsmAuthRequest],
    ["EAP_SIM_UMTS_AUTH_REQUEST", eapSimUmtsAuthRequest],
    ["EAP_SIM_IDENTITY_REQUEST", eapSimIdentityRequest],
    ["SCAN_RESULT_READY", scanResultReady],
    ["SCAN_RESULT_FAILED", scanResultFailed],
    ["PNO_SCAN_FOUND", pnoScanFound],
    ["PNO_SCAN_FAILED", pnoScanFailed],
    ["HOTSPOT_CLIENT_CHANGED", hotspotClientChanged],
    ["ANQP_QUERY_DONE", anqpResponse],
    ["HS20_ICON_QUERY_DONE", iconResponse],
    ["WNM_FRAME_RECEIVED", wnmFrameReceived],
    ["WPS_CONNECTION_SUCCESS", wps_connection_success],
    ["WPS_CONNECTION_FAIL", wps_connection_fail],
    ["WPS_CONNECTION_TIMEOUT", wps_connection_timeout],
    ["WPS_CONNECTION_PBC_OVERLAP", wps_pbc_overlap],
  ]);

  // Handle events sent to us by the event worker.
  function handleEvent(event, iface) {
    debug("Event coming: [" + iface + "] " + event.name);

    if (eventTable.has(event.name)) {
      eventTable.get(event.name)(event);
    }
    return true;
  }

  function supplicantStateChanged(event) {
    // Parse the event data.
    var fields = {};
    fields.state = supplicantStatesMap[event.stateChanged.state];
    fields.id = event.stateChanged.id;
    fields.ssid = event.stateChanged.ssid;
    fields.bssid = event.stateChanged.bssid;
    wifiInfo.setSupplicantState(fields.state);

    if (manager.isConnectState(fields.state)) {
      if (wifiInfo.ssid != fields.ssid && fields.ssid.length) {
        wifiInfo.setSSID(fields.ssid);
      }

      if (
        wifiInfo.bssid != fields.bssid &&
        fields.bssid !== "00:00:00:00:00:00"
      ) {
        wifiInfo.setBSSID(fields.bssid);
      }

      if (manager.targetNetworkId == WifiConstants.INVALID_NETWORK_ID) {
        manager.targetNetworkId = fields.id;
      }

      let network = WifiConfigManager.getNetworkConfiguration(
        manager.targetNetworkId
      );
      if (network && network.netId !== WifiConstants.INVALID_NETWORK_ID) {
        wifiInfo.setNetworkId(network.netId);
        wifiInfo.setSecurity(WifiConfigUtils.matchKeyMgmtToSecurity(network));
      }
    } else if (manager.isConnectState(manager.state)) {
      manager.lastDriverRoamAttempt = 0;
      autoRoaming = false;
      wifiInfo.reset();
    }

    if (manager.wpsStarted && manager.state !== "COMPLETED") {
      return;
    }

    notifyStateChange(fields);
    if (fields.state === "COMPLETED") {
      manager.lastDriverRoamAttempt = 0;
      manager.targetNetworkId = WifiConstants.INVALID_NETWORK_ID;
      onconnected();
    }
  }

  function supplicantTerminating() {
    wifiInfo.reset();
    notifyStateChange({ state: "DISCONNECTED", BSSID: null, id: -1 });

    // If the supplicant is terminated as commanded, the supplicant lost
    // notification will be sent after driver unloaded. In such case, the
    // manager state will be "DISABLING" or "UNINITIALIZED".
    // So if supplicant terminated with incorrect manager state, implying
    // unexpected condition, we should notify supplicant lost here.
    if (manager.state !== "DISABLING" && manager.state !== "UNINITIALIZED") {
      notify("supplicantlost", { success: true });
    }
  }

  function supplicantNetworkConnected(event) {
    var bssid = event.bssid;
    wifiInfo.setBSSID(bssid);

    if (WifiNetworkSelector.trackBssid(bssid, true, 0)) {
      manager.setFirmwareRoamingConfiguration();
    }
    if (manager.wpsStarted) {
      // The connected event is from WPS, but the network may not be saved in
      // config store yet. So first, we get the network configuration from
      // supplicant, then update as latest and save to config store. Finally
      // trigger disconnect to let network selection pick it automatically.
      manager.updateWpsConfiguration(config => {
        manager.wpsStarted = false;
        if (config == null) {
          debug("Failed to save WPS configuration");
          return;
        }
        let networkId = WifiConfigManager.getNetworkId(config);
        WifiConfigManager.updateLastSelectedNetwork(networkId, ok => {
          manager.disconnect(function() {
            config.netId = networkId;
            manager.startToConnect(config, function() {});
          });
        });
      });
    }
  }

  function supplicantNetworkDisconnected(event) {
    var reason = event.reason;
    fullBandConnectedTimeIntervalMilli =
      WifiConstants.WIFI_ASSOCIATED_SCAN_INTERVAL;
    if (manager.lastDriverRoamAttempt != 0) {
      manager.lastDriverRoamAttempt = 0;
    }

    debug("DISCONNECTED wifiInfo: " + JSON.stringify(wifiInfo));

    if (manager.isWifiEnabled(manager.state)) {
      // Clear the bssid in the current config's network block
      let id = lastNetwork
        ? lastNetwork.netId
        : WifiConstants.INVALID_NETWORK_ID;
      WifiConfigManager.clearCurrentConfigBssid(id, function() {});

      let networkKey = escape(wifiInfo.wifiSsid) + wifiInfo.security;
      var configuredNetworks = WifiConfigManager.configuredNetworks;
      if (typeof configuredNetworks[networkKey] !== "undefined") {
        let networkEnabled = !configuredNetworks[networkKey]
          .networkSelectionStatus;
        handleScanRequest(true, function() {});
        debug(
          "Receive DISCONNECTED:" +
            " BSSID=" +
            wifiInfo.bssid +
            " RSSI=" +
            wifiInfo.rssi +
            " freq=" +
            wifiInfo.frequency +
            " reason=" +
            reason +
            " Network Enabled Status=" +
            networkEnabled
        );
      } else {
        debug(networkKey + " is not defined in conifgured networks");
      }
    }

    autoRoaming = false;
    wifiInfo.reset();
    // Restore power save and suspend optimizations when dhcp failed.
    postDhcpSetup(function(ok) {});
  }

  function supplicantTargetBssid(event) {
    debug("Current target bssid: " + event.bssid);
  }

  function supplicantAssocistedBssid() {
    if (manager.state === "COMPLETED") {
      manager.lastDriverRoamAttempt = Date.now();
    }
  }

  function supplicantAuthenticationFailure(event) {
    let reasonCode = event.reason;
    let disableReason = WifiConstants.DISABLED_AUTHENTICATION_FAILURE;
    let network = WifiConfigManager.getNetworkConfiguration(
      manager.targetNetworkId
    );
    if (reasonCode == AUTH_FAILURE_WRONG_KEY) {
      if (network && network.hasEverConnected) {
        debug("Network " + manager.targetNetworkId + " is ever connected");
      } else {
        disableReason = WifiConstants.DISABLED_BY_WRONG_PASSWORD;
      }
    } else if (reasonCode == AUTH_FAILURE_EAP_FAILURE) {
      let errorCode = event.errorCode;
      if (errorCode == ERROR_EAP_SIM_NOT_SUBSCRIBED) {
        disableReason = WifiConstants.DISABLED_AUTHENTICATION_NO_SUBSCRIBED;
      }
    }
    WifiConfigManager.updateNetworkSelectionStatus(
      manager.targetNetworkId,
      disableReason,
      function(doDisable) {
        if (doDisable) {
          notify("networkdisable", {
            reason: WifiConstants.DISABLED_AUTHENTICATION_FAILURE,
          });
        }
      }
    );
  }

  function supplicantAssociationReject(event) {
    if (manager.wpsStarted) {
      return;
    }
    var bssid = event.bssid;
    if (bssid && bssid !== "00:00:00:00:00:00") {
      // If we have a BSSID, tell configStore to add it into deny list.
      if (WifiNetworkSelector.trackBssid(bssid, false, event.statusCode)) {
        manager.setFirmwareRoamingConfiguration();
      }

      WifiConfigManager.updateNetworkSelectionStatus(
        manager.targetNetworkId,
        WifiConstants.DISABLED_ASSOCIATION_REJECTION,
        function(doDisable) {
          if (doDisable) {
            notify("networkdisable", {
              reason: WifiConstants.DISABLED_ASSOCIATION_REJECTION,
            });
          }
        }
      );
    }
  }

  function eapSimGsmAuthRequest(event) {
    let rands = event.getGsmRands();
    let network = WifiConfigManager.getNetworkConfiguration(
      manager.targetNetworkId
    );
    let simIndex = network.simIndex || 1;
    simGsmAuthRequest(simIndex, rands);
  }

  function eapSimUmtsAuthRequest(event) {
    let network = WifiConfigManager.getNetworkConfiguration(
      manager.targetNetworkId
    );
    let simIndex = network.simIndex || 1;
    simUmtsAuthRequest(simIndex, event.rand, event.autn);
  }

  function eapSimIdentityRequest() {
    let network = WifiConfigManager.getNetworkConfiguration(
      manager.targetNetworkId
    );
    let simIndex = network.simIndex || 1;
    simIdentityRequest(
      manager.targetNetworkId,
      simIndex,
      getIdentityPrefix(network.eap)
    );
  }

  function scanResultReady() {
    debug("Notifying of scan results available");

    if (!screenOn && manager.state === "SCANNING") {
      enableBackgroundScan(true);
    }
    notify("scanresultsavailable", { type: USE_SINGLE_SCAN });
  }

  function scanResultFailed() {
    debug("Receive single scan failure");
    manager.cachedScanResults = [];
  }

  function pnoScanFound() {
    notify("scanresultsavailable", { type: USE_PNO_SCAN });
  }

  function pnoScanFailed() {
    debug("Receive PNO scan failure");
    schedulePnoFailed = true;
  }

  function hotspotClientChanged(event) {
    notify("stationinfoupdate", { station: event.numStations });
  }

  function anqpResponse(event) {
    PasspointManager.onANQPResponse(event.anqpNetworkKey, event.anqpResponse);
  }

  function iconResponse() {
    // TODO: handle icon response
  }

  function wnmFrameReceived() {
    // TODO: handle management frame received
  }

  function wps_connection_success() {
    debug("WPS success, wait for full connection");
  }

  function wps_connection_fail(event) {
    manager.wpsStarted = false;
    debug(
      "WPS failed with error code " +
        event.wpsConfigError +
        ":" +
        event.wpsErrorIndication
    );
    notifyStateChange({ state: "WPS_FAIL", BSSID: null, id: -1 });
  }

  function wps_connection_timeout() {
    manager.wpsStarted = false;
    debug("WPS timeout");
    notifyStateChange({ state: "WPS_TIMEOUT", BSSID: null, id: -1 });
  }

  function wps_pbc_overlap() {
    manager.wpsStarted = false;
    debug("WPS pbc overlap");
    notifyStateChange({ state: "WPS_OVERLAP_DETECTED", BSSID: null, id: -1 });
  }

  var requestOptimizationMode = 0;
  function setSuspendOptimizationsMode(reason, enable, callback) {
    debug(
      "setSuspendOptimizationsMode reason = " +
        reason +
        ", enable = " +
        enable +
        ", requestOptimizationMode = " +
        requestOptimizationMode
    );
    if (enable) {
      requestOptimizationMode &= ~reason;
      if (!requestOptimizationMode) {
        wifiCommand.setSuspendMode(enable, function(result) {
          callback(result.status == SUCCESS);
        });
      } else {
        callback(true);
      }
    } else {
      requestOptimizationMode |= reason;
      wifiCommand.setSuspendMode(enable, function(result) {
        callback(result.status == SUCCESS);
      });
    }
  }

  function prepareForStartup(callback) {
    let status = libcutils.property_get(DHCP_PROP + "_" + manager.ifname);
    if (status !== "running") {
      callback();
      return;
    }
    manager.connectionDropped(function() {
      callback();
    });
  }

  // Initial state.
  manager.state = "UNINITIALIZED";
  manager.tetheringState = "UNINITIALIZED";
  manager.supplicantStarted = false;
  manager.lastKnownCountryCode = null;
  manager.telephonyServiceId = 0;
  manager.numRil = numRil;
  initForEnabled();

  // Call to init everytime when we need to setWifiEnabled
  function initForEnabled() {
    manager.wpsStarted = false;
    manager.inObtainingIpState = false;
    manager.lastNetwork = null;
    manager.lastDriverRoamAttempt = 0;
    manager.loopDetectionCount = 0;
    manager.cachedScanResults = [];
    manager.cachedScanTime = 0;
    manager.targetNetworkId = WifiConstants.INVALID_NETWORK_ID;
    manager.lastTrafficStats = null;

    autoRoaming = false;
    pnoEnabled = false;
    schedulePnoFailed = false;
    dhcpInfo = null;
    delayScanInterval = WifiConstants.WIFI_SCHEDULED_SCAN_INTERVAL;
    fullBandConnectedTimeIntervalMilli =
      WifiConstants.WIFI_ASSOCIATED_SCAN_INTERVAL;
  }

  manager.__defineGetter__("enabled", function() {
    switch (manager.state) {
      case "UNINITIALIZED":
      case "INITIALIZING":
      case "DISABLING":
        return false;
      default:
        return true;
    }
  });

  // EAP-SIM: convert string from hex to base64.
  function gsmHexToBase64(hex) {
    let octects = String.fromCharCode(hex.length / 2);
    for (let i = 0; i < hex.length; i += 2) {
      octects += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
    }
    return btoa(octects);
  }

  // EAP-AKA/AKA': convert string from hex to base64.
  function umtsHexToBase64(rand, authn) {
    let octects = String.fromCharCode(rand.length / 2);
    for (let i = 0; i < rand.length; i += 2) {
      octects += String.fromCharCode(parseInt(rand.substr(i, 2), 16));
    }
    octects += String.fromCharCode(authn.length / 2);
    for (let i = 0; i < authn.length; i += 2) {
      octects += String.fromCharCode(parseInt(authn.substr(i, 2), 16));
    }
    return btoa(octects);
  }

  // EAP-SIM/AKA/AKA': convert string from base64 to byte.
  function base64Tobytes(str) {
    let octects = atob(
      str
        .replace(/-/g, "+")
        .replace(/_/g, "/")
        .replace(/\r?\n|\r/g, "")
    );
    return octects;
  }

  // EAP-SIM/AKA/AKA': convert string from byte to hex.
  function bytesToHex(str, from, len) {
    let hexs = "";
    for (let i = 0; i < len; i++) {
      hexs += ("0" + str.charCodeAt(from + i).toString(16)).substr(-2);
    }
    return hexs;
  }

  // EAP-SIM/AKA/AKA': get identity prefix value.
  function getIdentityPrefix(eapMethod) {
    // Prefix value:
    // "\0" for Encrypted Identity
    // "0" for EAP-AKA Identity
    // "1" for EAP-SIM Identity
    // "6" for EAP-AKA' Identity
    if (eapMethod == "SIM") {
      return "1";
    } else if (eapMethod == "AKA") {
      return "0";
    } else if (eapMethod == "AKA'") {
      return "6";
    }
    return "\0";
  }

  function simIdentityRequest(networkId, simIndex, prefix) {
    // For SIM & AKA/AKA' EAP method Only, get identity from ICC
    let icc = lazy.gIccService.getIccByServiceId(simIndex - 1);
    if (!icc || !icc.iccInfo || !icc.iccInfo.mcc || !icc.iccInfo.mnc) {
      debug("SIM is not ready or iccInfo is invalid");
      WifiConfigManager.updateNetworkSelectionStatus(
        networkId,
        WifiConstants.DISABLED_AUTHENTICATION_NO_CREDENTIALS,
        function(doDisable) {
          if (doDisable) {
            manager.disableNetwork(function() {
              notify("networkdisable", {
                reason: WifiConstants.DISABLED_AUTHENTICATION_FAILURE,
              });
            });
          }
        }
      );
      return;
    }

    // imsi, mcc, mnc
    let imsi = icc.imsi;
    let mcc = icc.iccInfo.mcc;
    let mnc =
      icc.iccInfo.mnc.length === 2 ? "0" + icc.iccInfo.mnc : icc.iccInfo.mnc;
    let identity = {
      identity:
        prefix + imsi + "@wlan.mnc" + mnc + ".mcc" + mcc + ".3gppnetwork.org",
    };

    debug("identity = " + uneval(identity));
    wifiCommand.simIdentityResponse(identity, function() {});
  }

  function simGsmAuthRequest(simIndex, data) {
    let authResponse = [];
    let count = 0;

    let icc = lazy.gIccService.getIccByServiceId(simIndex - 1);
    for (let value in data) {
      let challenge = data[value];
      if (!challenge.length) {
        continue;
      }
      let base64Challenge = gsmHexToBase64(challenge);
      debug("base64Challenge = " + base64Challenge);

      // Try USIM first for authentication.
      icc.getIccAuthentication(
        Ci.nsIIcc.APPTYPE_USIM,
        Ci.nsIIcc.AUTHTYPE_EAP_SIM,
        base64Challenge,
        {
          notifyAuthResponse(iccResponse) {
            debug("Receive USIM iccResponse: " + iccResponse);
            iccResponseReady(iccResponse);
          },
          notifyError(aErrorMsg) {
            debug("Receive USIM iccResponse error: " + aErrorMsg);
            // In case of failure, retry as a simple SIM.
            icc.getIccAuthentication(
              Ci.nsIIcc.APPTYPE_SIM,
              Ci.nsIIcc.AUTHTYPE_EAP_SIM,
              base64Challenge,
              {
                notifyAuthResponse(iccResponse) {
                  debug("Receive SIM iccResponse: " + iccResponse);
                  iccResponseReady(iccResponse);
                },
                notifyError(aErrorMsg) {
                  debug("Receive SIM iccResponse error: " + aErrorMsg);
                  wifiCommand.simGsmAuthFailure(function() {});
                },
              }
            );
          },
        }
      );
    }

    function iccResponseReady(iccResponse) {
      if (!iccResponse || iccResponse.length <= 4) {
        debug("bad response - " + iccResponse);
        wifiCommand.simGsmAuthFailure(function() {});
        return;
      }
      let result = base64Tobytes(iccResponse);
      let sres_len = result.charCodeAt(0);
      let sres = bytesToHex(result, 1, sres_len);
      let kc_offset = 1 + sres_len;
      let kc_len = result.charCodeAt(kc_offset);
      if (
        sres_len >= result.length ||
        kc_offset >= result.length ||
        kc_offset + kc_len > result.length
      ) {
        debug("malfomed response - " + iccResponse);
        wifiCommand.simGsmAuthFailure(function() {});
        return;
      }
      let kc = bytesToHex(result, 1 + kc_offset, kc_len);
      debug("kc:" + kc + ", sres:" + sres);
      authResponse[count] = { kc, sres };
      count++;
      if (count == 3) {
        debug("Supplicant Response -" + uneval(authResponse));
        wifiCommand.simGsmAuthResponse(authResponse, function() {});
      }
    }
  }

  function simUmtsAuthRequest(simIndex, rand, autn) {
    let icc = lazy.gIccService.getIccByServiceId(simIndex - 1);
    if (rand == null || autn == null) {
      debug("null rand or autn");
      return;
    }

    let base64Challenge = umtsHexToBase64(rand, autn);
    debug("base64Challenge = " + base64Challenge);

    icc.getIccAuthentication(
      Ci.nsIIcc.APPTYPE_USIM,
      Ci.nsIIcc.AUTHTYPE_EAP_AKA,
      base64Challenge,
      {
        notifyAuthResponse(iccResponse) {
          debug("Receive iccResponse: " + iccResponse);
          iccResponseReady(iccResponse);
        },
        notifyError(aErrorMsg) {
          debug("Receive iccResponse error: " + aErrorMsg);
        },
      }
    );

    function iccResponseReady(iccResponse) {
      if (!iccResponse || iccResponse.length <= 4) {
        debug("bad response - " + iccResponse);
        wifiCommand.simUmtsAuthFailure(function() {});
        return;
      }
      let result = base64Tobytes(iccResponse);
      let tag = result.charCodeAt(0);
      if (tag == 0xdb) {
        debug("successful 3G authentication ");
        let res_len = result.charCodeAt(1);
        let res = bytesToHex(result, 2, res_len);
        let ck_len = result.charCodeAt(res_len + 2);
        let ck = bytesToHex(result, res_len + 3, ck_len);
        let ik_len = result.charCodeAt(res_len + ck_len + 3);
        let ik = bytesToHex(result, res_len + ck_len + 4, ik_len);
        debug("ik:" + ik + " ck:" + ck + " res:" + res);
        let authResponse = { res, ik, ck };
        wifiCommand.simUmtsAuthResponse(authResponse, function() {});
      } else if (tag == 0xdc) {
        debug("synchronisation failure");
        let auts_len = result.charCodeAt(1);
        let auts = bytesToHex(result, 2, auts_len);
        debug("auts:" + auts);
        let authResponse = { auts };
        wifiCommand.simUmtsAutsResponse(authResponse, function() {});
      } else {
        debug("bad authResponse - unknown tag = " + tag);
        wifiCommand.simUmtsAuthFailure(function() {});
      }
    }
  }

  manager.startWifi = wifiCommand.startWifi;
  manager.stopWifi = wifiCommand.stopWifi;
  manager.getMacAddress = wifiCommand.getMacAddress;
  manager.getScanResults = wifiCommand.getScanResults;
  manager.handleScanRequest = handleScanRequest;
  manager.setWpsDetail = wifiCommand.setWpsDetail;
  manager.wpsPbc = wifiCommand.wpsPbc;
  manager.wpsPin = wifiCommand.wpsPin;
  manager.wpsDisplay = wifiCommand.wpsDisplay;
  manager.wpsCancel = wifiCommand.wpsCancel;
  manager.setPowerSave = wifiCommand.setPowerSave;
  manager.setExternalSim = wifiCommand.setExternalSim;
  manager.getHttpProxyNetwork = getHttpProxyNetwork;
  manager.setHttpProxy = setHttpProxy;
  manager.configureHttpProxy = configureHttpProxy;
  manager.setSuspendOptimizationsMode = setSuspendOptimizationsMode;
  manager.setStaticIpMode = setStaticIpMode;
  manager.getRssiApprox = wifiCommand.getRssiApprox;
  manager.getLinkSpeed = wifiCommand.getLinkSpeed;
  manager.getDhcpInfo = function() {
    return dhcpInfo;
  };
  manager.getConnectionInfo = wifiCommand.getConnectionInfo;
  manager.getLinkLayerStats = wifiCommand.getLinkLayerStats;
  manager.setScanInterval = wifiCommand.setScanInterval;
  manager.enableAutoReconnect = wifiCommand.enableAutoReconnect;
  manager.autoScanMode = wifiCommand.autoScanMode;
  manager.handleScreenStateChanged = handleScreenStateChanged;
  manager.setCountryCode = wifiCommand.setCountryCode;
  manager.postDhcpSetup = postDhcpSetup;
  manager.setNetworkVariable = wifiCommand.setNetworkVariable;
  manager.setDebugLevel = wifiCommand.setDebugLevel;
  manager.setAutoReconnect = wifiCommand.setAutoReconnect;
  manager.disconnect = wifiCommand.disconnect;
  manager.reconnect = wifiCommand.reconnect;
  manager.reassociate = wifiCommand.reassociate;
  manager.disableNetwork = wifiCommand.disableNetwork;
  manager.getSupplicantNetwork = wifiCommand.getSupplicantNetwork;
  manager.requestAnqp = wifiCommand.requestAnqp;
  manager.getSoftapStations = wifiCommand.getSoftapStations;
  manager.syncDebug = syncDebug;

  // Public interface of the wifi service.
  manager.setWifiEnabled = function(enabled, callback) {
    if (enabled === manager.isWifiEnabled(manager.state)) {
      callback(true);
      return;
    }

    BinderServices.wifi.onWifiStateChanged(
      enabled
        ? WifiConstants.WIFI_STATE_ENABLING
        : WifiConstants.WIFI_STATE_DISABLING
    );

    if (enabled) {
      manager.state = "INITIALIZING";
      initForEnabled();
      prepareForStartup(function() {
        wifiCommand.startWifi(function(result) {
          if (result.status != SUCCESS) {
            callback(false);
            manager.state = "UNINITIALIZED";
            return;
          }
          // wlan module is loaded, update interface name here
          if (result.staInterface.length) {
            manager.ifname = result.staInterface;
          }

          // get supported features on current driver module
          wifiCommand.getSupportedFeatures(function(result) {
            supportedFeatures = result.supportedFeatures;
            debug("supported features: " + supportedFeatures);
          });

          // Register as network interface.
          WifiNetworkInterface.info.name = manager.ifname;
          if (!WifiNetworkInterface.registered) {
            lazy.gNetworkManager.registerNetworkInterface(WifiNetworkInterface);
            WifiNetworkInterface.registered = true;
          }
          WifiNetworkInterface.info.state =
            Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;
          WifiNetworkInterface.info.ips = [];
          WifiNetworkInterface.info.prefixLengths = [];
          WifiNetworkInterface.info.gateways = [];
          WifiNetworkInterface.info.dnses = [];
          lazy.gNetworkManager.updateNetworkInterface(WifiNetworkInterface);

          manager.supplicantStarted = true;

          lazy.gNetworkService.setIpv6Status(
            manager.ifname,
            false,
            function() {}
          );
          manager.supplicantConnected();
          callback(true);
          BinderServices.wifi.onWifiStateChanged(
            WifiConstants.WIFI_STATE_ENABLED
          );
        });
      });
    } else {
      let imsCapability;
      try {
        imsCapability = lazy.gImsRegService.getHandlerByServiceId(
          manager.telephonyServiceId
        ).capability;
      } catch (e) {
        manager.setWifiDisable(callback);
        return;
      }
      if (
        imsCapability == Ci.nsIImsRegHandler.IMS_CAPABILITY_VOICE_OVER_WIFI ||
        imsCapability == Ci.nsIImsRegHandler.IMS_CAPABILITY_VIDEO_OVER_WIFI
      ) {
        notify("registerimslistener", { register: true, callback });
      } else {
        manager.setWifiDisable(callback);
      }
    }
  };

  manager.setWifiDisable = function(callback) {
    notify("registerimslistener", { register: false });

    // To faster disable wifi, we should stop dhcp as soon as possible
    // if it is obtaining ip.
    if (manager.inObtainingIpState) {
      netUtil.stopDhcp(manager.ifname, function() {
        releaseWifiWakeLock();
      });
    }

    manager.state = "DISABLING";
    clearTimeout(delayScanId);
    delayScanId = null;
    wifiInfo.reset();
    // Note these following calls ignore errors. If we fail to kill the
    // supplicant gracefully, then we need to continue telling it to die
    // until it does.
    manager.state = "UNINITIALIZED";
    manager.disconnect(function() {
      lazy.gNetworkService.setInterfaceConfig(
        { ifname: manager.ifname, link: "down" },
        function(ok) {
          wifiCommand.stopWifi(function(result) {
            notify("supplicantlost", { success: true });
            callback(result.status == SUCCESS);
            BinderServices.wifi.onWifiStateChanged(
              WifiConstants.WIFI_STATE_DISABLED
            );
          });
        }
      );
    });

    // We are going to terminate the connection between wpa_supplicant.
    // Stop the polling timer immediately to prevent connection info update
    // command blocking in control thread until socket timeout.
    notify("stopconnectioninfotimer");

    postDhcpSetup(function() {
      manager.connectionDropped(function() {});
    });
  };

  // Get wifi interface and load wifi driver when enable Ap mode.
  manager.setWifiApEnabled = function(enabled, configuration, callback) {
    if (enabled === manager.isWifiTetheringEnabled(manager.tetheringState)) {
      callback(true);
      return;
    }

    if (enabled) {
      debug("softap configuration -> " + uneval(configuration));
      manager.tetheringState = "INITIALIZING";
      wifiCommand.startSoftap(configuration, function(result) {
        if (result.status != SUCCESS) {
          callback(false);
          manager.tetheringState = "UNINITIALIZED";
          return;
        }
        var ifaceName = result.apInterface;
        lazy.gNetworkService.setInterfaceConfig(
          { ifname: ifaceName, link: "up" },
          function(ok) {}
        );
        WifiNetworkInterface.info.name = ifaceName;

        lazy.gTetheringService.setWifiTethering(
          enabled,
          WifiNetworkInterface.info.name,
          configuration,
          function(errorMsg) {
            manager.tetheringState = errorMsg ? "UNINITIALIZED" : "COMPLETED";
            // Pop out current request.
            callback(!errorMsg);
            // Should we fire a dom event if we fail to set wifi tethering  ?
            debug(
              "Enable Wifi tethering result: " +
                (errorMsg ? errorMsg : "successfully")
            );
            // Unload wifi driver status.
            if (errorMsg) {
              wifiCommand.stopSoftap(function() {});
            }
          }
        );
      });
    } else {
      lazy.gTetheringService.setWifiTethering(
        enabled,
        WifiNetworkInterface.info.name,
        configuration,
        function(errorMsg) {
          // Should we fire a dom event if we fail to set wifi tethering  ?
          debug(
            "Disable Wifi tethering result: " +
              (errorMsg ? errorMsg : "successfully")
          );
          // Unload wifi driver even if we fail to control wifi tethering.
          wifiCommand.stopSoftap(function(result) {
            if (result.status != SUCCESS) {
              debug("Fail to stop softap");
            }
            manager.tetheringState = "UNINITIALIZED";
            callback(result.status == SUCCESS);
          });
        }
      );
    }
  };

  manager.connectionDropped = function(callback) {
    // If we got disconnected, kill the DHCP client and disable IPv6
    // in preparation for reconnection.
    lazy.gNetworkService.setIpv6Status(
      WifiManager.ifname,
      false,
      function() {}
    );
    lazy.gNetworkService.resetRoutingTable(manager.ifname, function() {
      netUtil.stopDhcp(manager.ifname, function() {
        callback();
      });
    });
  };

  manager.supplicantConnected = function(callback) {
    debug("detected SDK version " + sdkVersion);

    // Give it a state other than UNINITIALIZED, INITIALIZING or DISABLING
    // defined in getter function of WifiManager.enabled. It implies that
    // we are ready to accept dom request from applications.
    WifiManager.state = "DISCONNECTED";

    // Sync debug level to supplicant.
    manager.syncDebug();

    notify("supplicantconnection");

    // WPA supplicant already connected.
    manager.enableAutoReconnect(true, function() {});
    manager.setPowerSave(true, function() {});
    manager.setWpsDetail(function() {});
    setSuspendOptimizationsMode(
      POWER_MODE_SCREEN_STATE,
      !screenOn,
      function() {}
    );
  };

  manager.setAutoRoam = function(config, callback) {
    if (config == null || config.netId === WifiConstants.INVALID_NETWORK_ID) {
      debug("Roam to invalid network");
      callback(false);
      return;
    }
    let network = WifiConfigManager.getNetworkConfiguration(config.netId);
    network.bssid = config.bssid;
    manager.startToRoam(network, callback);
  };

  manager.setAutoConnect = function(config, callback) {
    if (config == null || config.netId === WifiConstants.INVALID_NETWORK_ID) {
      debug("Connect to invalid network");
      callback(false);
      return;
    }
    let network = WifiConfigManager.getNetworkConfiguration(config.netId);
    network.bssid = config.bssid;
    manager.startToConnect(network, callback);
  };

  manager.setFirmwareRoamingConfiguration = function() {
    let denyList = Array.from(WifiNetworkSelector.bssidDenylist.keys());

    let roamingConfig = {
      bssidDenylist: denyList,
      ssidAllowlist: [],
    };
    wifiCommand.configureFirmwareRoaming(roamingConfig, function() {});
  };

  manager.saveNetwork = function(config, callback) {
    WifiConfigManager.addOrUpdateNetwork(config, callback);
  };

  manager.removeNetwork = function(netId, callback) {
    manager.configurationChannels.delete(netId);
    WifiConfigManager.removeNetwork(netId, callback);
  };

  manager.removeNetworks = function(callback) {
    wifiCommand.removeNetworks(result => {
      callback(result.status == SUCCESS);
    });
  };

  manager.startToConnect = function(config, callback) {
    manager.targetNetworkId = config.netId;
    autoRoaming = false;
    wifiCommand.startToConnect(config, result => {
      callback(result.status == SUCCESS);
    });
  };

  manager.startToRoam = function(config, callback) {
    manager.targetNetworkId = config.netId;
    autoRoaming = true;
    wifiCommand.startToRoam(config, result => {
      callback(result.status == SUCCESS);
    });
  };

  manager.enableNetwork = function(netId, callback) {
    wifiCommand.enableNetwork(netId, callback);
  };

  manager.ensureSupplicantDetached = aCallback => {
    if (!manager.enabled) {
      aCallback();
      return;
    }
    wifiCommand.closeSupplicantConnection(aCallback);
  };

  manager.updateWpsConfiguration = function(callback) {
    // Get configuration from supplicant.
    manager.getSupplicantNetwork(result => {
      if (result.status != SUCCESS) {
        debug("Failed to get supplicant configurations");
        callback(null);
        return;
      }

      let config = Object.create(null);
      for (let field in result.wifiConfig) {
        let value = result.wifiConfig[field];
        if (typeof value == "string" && !value.length) {
          continue;
        }
        config[field] = value;
      }
      config.bssid = WifiConstants.SUPPLICANT_BSSID_ANY;
      // Ignore the network ID from supplicant.
      delete config.netId;
      // For OPEN network, 'psk' is invalid
      if (config.keyMgmt === "NONE") {
        delete config.psk;
      }

      // Save WPS network configurations into config store.
      manager.saveNetwork(config, ok => {
        callback(ok ? config : null);
      });
    });
  };

  manager.isHandShakeState = function(state) {
    switch (state) {
      case "AUTHENTICATING":
      case "ASSOCIATING":
      case "ASSOCIATED":
      case "FOUR_WAY_HANDSHAKE":
      case "GROUP_HANDSHAKE":
        return true;
      case "COMPLETED":
      case "DISCONNECTED":
      case "INTERFACE_DISABLED":
      case "INACTIVE":
      case "SCANNING":
      case "UNINITIALIZED":
      case "INVALID":
      case "CONNECTED":
      default:
        return false;
    }
  };

  manager.isConnectState = function(state) {
    switch (state) {
      case "AUTHENTICATING":
      case "ASSOCIATING":
      case "ASSOCIATED":
      case "FOUR_WAY_HANDSHAKE":
      case "GROUP_HANDSHAKE":
      case "CONNECTED":
      case "COMPLETED":
        return true;
      case "DISCONNECTED":
      case "INTERFACE_DISABLED":
      case "INACTIVE":
      case "SCANNING":
      case "UNINITIALIZED":
      case "INVALID":
      default:
        return false;
    }
  };

  manager.isWifiEnabled = function(state) {
    switch (state) {
      case "UNINITIALIZED":
      case "DISABLING":
        return false;
      default:
        return true;
    }
  };

  manager.isWifiTetheringEnabled = function(state) {
    switch (state) {
      case "UNINITIALIZED":
        return false;
      default:
        return true;
    }
  };

  manager.stateOrdinal = function(state) {
    return supplicantStatesMap.indexOf(state);
  };

  manager.supplicantLoopDetection = function(prevState, state) {
    var isPrevStateInHandShake = manager.isHandShakeState(prevState);
    var isStateInHandShake = manager.isHandShakeState(state);

    if (isStateInHandShake) {
      if (isPrevStateInHandShake) {
        // Increase the count only if we are in the loop.
        if (manager.stateOrdinal(state) > manager.stateOrdinal(prevState)) {
          manager.loopDetectionCount++;
        }
        if (manager.loopDetectionCount > MAX_SUPPLICANT_LOOP_ITERATIONS) {
          WifiConfigManager.updateNetworkSelectionStatus(
            lastNetwork.netId,
            WifiConstants.DISABLED_AUTHENTICATION_FAILURE,
            function(doDisable) {
              manager.loopDetectionCount = 0;
              if (doDisable) {
                notify("networkdisable", {
                  reason: WifiConstants.DISABLED_AUTHENTICATION_FAILURE,
                });
              }
            }
          );
        }
      } else {
        // From others state to HandShake state. Reset the count.
        manager.loopDetectionCount = 0;
      }
    }
  };

  // If got multiple networks with same bssid in the scan results,
  // keep the most current one which has biggest tsf.
  manager.filterExpiredBssid = function(scanResults) {
    let keepResults = [];

    scanResults.forEach((currentResult, index) => {
      let existedNewer = scanResults.some(
        filterResult =>
          filterResult.bssid === currentResult.bssid &&
          filterResult.tsf > currentResult.tsf
      );

      if (existedNewer) {
        debug(
          "filter out" +
            " " +
            currentResult.ssid +
            " " +
            currentResult.bssid +
            " " +
            currentResult.frequency +
            " " +
            currentResult.tsf +
            " " +
            currentResult.capability +
            " " +
            currentResult.signal
        );
      } else {
        // keep the newer one
        keepResults.push(currentResult);
      }
    });

    return keepResults;
  };

  manager.getCapabilities = function() {
    return capabilities;
  };

  manager.isFeatureSupported = function(feature) {
    return supportedFeatures & feature;
  };

  // Cert Services
  let wifiCertService = Cc["@mozilla.org/wifi/certservice;1"];
  if (wifiCertService) {
    wifiCertService = wifiCertService.getService(Ci.nsIWifiCertService);
    wifiCertService.start(wifiListener);
  } else {
    console.log("No wifi CA service component available");
  }

  manager.importCert = function(caInfo, callback) {
    var id = idgen++;
    if (callback) {
      controlCallbacks[id] = callback;
    }

    wifiCertService.importCert(
      id,
      caInfo.certBlob,
      caInfo.certPassword,
      caInfo.certNickname
    );
  };

  manager.deleteCert = function(caInfo, callback) {
    var id = idgen++;
    if (callback) {
      controlCallbacks[id] = callback;
    }

    wifiCertService.deleteCert(id, caInfo.certNickname);
  };

  manager.filterCert = function(certList) {
    return wifiCertService.filterCert(certList);
  };

  manager.sdkVersion = function() {
    return sdkVersion;
  };

  return manager;
})();

function shouldBroadcastRSSIForIMS(newRssi, lastRssi) {
  let needBroadcast = false;
  if (newRssi == lastRssi || lastRssi == WifiConstants.INVALID_RSSI) {
    return needBroadcast;
  }

  debug(
    "shouldBroadcastRSSIForIMS, newRssi =" + newRssi + " , oldRssi= " + lastRssi
  );
  if (newRssi > -75 && lastRssi <= -75) {
    needBroadcast = true;
  } else if (newRssi <= -85 && lastRssi > -85) {
    needBroadcast = true;
  }
  return needBroadcast;
}

function quote(s) {
  return '"' + s + '"';
}

function dequote(s) {
  if (s[0] != '"' || s[s.length - 1] != '"') {
    throw Components.Exception(
      "Invalid argument, not a quoted string: ",
      Cr.NS_ERROR_INVALID_ARG
    );
  }
  return s.substr(1, s.length - 2);
}

function isWepHexKey(s) {
  if (s.length != 10 && s.length != 26 && s.length != 58) {
    return false;
  }
  return !/[^a-fA-F0-9]/.test(s);
}

const wepKeyList = ["wepKey0", "wepKey1", "wepKey2", "wepKey3"];
function isInWepKeyList(obj) {
  for (let item in wepKeyList) {
    if (obj[item]) {
      return true;
    }
  }
  return false;
}

var WifiNetworkInterface = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInterface]),

  registered: false,

  // nsINetworkInterface
  NETWORK_STATE_UNKNOWN: Ci.nsINetworkInfo.NETWORK_STATE_UNKNOWN,
  NETWORK_STATE_CONNECTING: Ci.nsINetworkInfo.CONNECTING,
  NETWORK_STATE_CONNECTED: Ci.nsINetworkInfo.CONNECTED,
  NETWORK_STATE_DISCONNECTING: Ci.nsINetworkInfo.DISCONNECTING,
  NETWORK_STATE_DISCONNECTED: Ci.nsINetworkInfo.DISCONNECTED,

  NETWORK_TYPE_WIFI: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,
  NETWORK_TYPE_MOBILE: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS,
  NETWORK_TYPE_MOBILE_SUPL: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL,

  updateConfig(action, config) {
    if (autoRoaming) {
      return;
    }

    debug(
      "Interface " +
        this.info.name +
        " updateConfig: " +
        action +
        " " +
        JSON.stringify(config)
    );

    this.info.state =
      config.state != undefined ? config.state : this.info.state;
    let configHasChanged = false;
    if (action == "updated") {
      if (config.ip != undefined && !this.info.ips.includes(config.ip)) {
        configHasChanged = true;
        this.info.ips.push(config.ip);
        this.info.prefixLengths.push(config.prefixLengths);
      }
      if (
        config.gateway != undefined &&
        !this.info.gateways.includes(config.gateway)
      ) {
        configHasChanged = true;
        this.info.gateways.push(config.gateway);
      }
      if (config.dnses != undefined) {
        for (let i in config.dnses) {
          if (
            typeof config.dnses[i] == "string" &&
            config.dnses[i].length &&
            !this.info.dnses.includes(config.dnses[i])
          ) {
            configHasChanged = true;
            this.info.dnses.push(config.dnses[i]);
          }
        }
      }
    } else {
      if (config.ip != undefined) {
        let found = this.info.ips.indexOf(config.ip);
        if (found !== -1) {
          configHasChanged = true;
          this.info.ips.splice(found, 1);
          if (this.info.prefixlengths != undefined) {
            this.info.prefixlengths.splice(found, 1);
          }
        }
      }
      if (config.gateway != undefined) {
        let found = this.info.gateways.indexOf(config.gateway);
        if (found !== -1) {
          configHasChanged = true;
          this.info.gateways.splice(found, 1);
        }
      }
    }
    if (
      configHasChanged &&
      this.info.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
    ) {
      debug("WifiNetworkInterface changed, update network interface");
      lazy.gNetworkManager.updateNetworkInterface(WifiNetworkInterface);
    }
  },

  info: {
    QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInfo]),

    state: Ci.nsINetworkInfo.NETWORK_STATE_UNKNOWN,

    type: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,

    name: null,

    ips: [],

    prefixLengths: [],

    dnses: [],

    gateways: [],

    meter: false,

    getAddresses(ips, prefixLengths) {
      ips.value = this.ips.slice();
      prefixLengths.value = this.prefixLengths.slice();

      return this.ips.length;
    },

    getGateways(count) {
      if (count) {
        count.value = this.gateways.length;
      }
      return this.gateways.slice();
    },

    getDnses(count) {
      if (count) {
        count.value = this.dnses.length;
      }
      return this.dnses.slice();
    },
  },

  httpProxyHost: null,

  httpProxyPort: null,
};

function WifiScanResult() {}

// TODO Make the difference between a DOM-based network object and our
// networks objects much clearer.
var netToDOM;
var netFromDOM;

export function WifiWorker() {
  var self = this;

  const messages = [
    "WifiManager:getNetworks",
    "WifiManager:getKnownNetworks",
    "WifiManager:associate",
    "WifiManager:forget",
    "WifiManager:wps",
    "WifiManager:getState",
    "WifiManager:setPowerSavingMode",
    "WifiManager:setHttpProxy",
    "WifiManager:setStaticIpMode",
    "WifiManager:importCert",
    "WifiManager:getImportedCerts",
    "WifiManager:deleteCert",
    "WifiManager:setPasspointConfig",
    "WifiManager:getPasspointConfigs",
    "WifiManager:removePasspointConfig",
    "WifiManager:setWifiEnabled",
    "WifiManager:setWifiTethering",
    "WifiManager:getSoftapStations",
    "WifiManager:setOpenNetworkNotification",
    "child-process-shutdown",
  ];

  messages.forEach(
    function(msgName) {
      lazy.ppmm.addMessageListener(msgName, this);
    }.bind(this)
  );

  Services.obs.addObserver(this, kFinalUiStartUpTopic);
  Services.obs.addObserver(this, kXpcomShutdownChangedTopic);
  Services.obs.addObserver(this, kScreenStateChangedTopic);
  Services.obs.addObserver(this, kInterfaceAddressUpdatedTopic);
  Services.obs.addObserver(this, kInterfaceAddressRemovedTopic);
  Services.obs.addObserver(this, kInterfaceDnsInfoTopic);
  Services.obs.addObserver(this, kRouteChangedTopic);
  Services.obs.addObserver(this, kCaptivePortalResult);
  Services.obs.addObserver(this, kOpenCaptivePortalLoginEvent);
  Services.obs.addObserver(this, kCaptivePortalLoginSuccessEvent);
  Services.prefs.addObserver(kPrefDefaultServiceId, this);

  this._wantScanResults = [];

  this._addingNetworks = Object.create(null);

  this._ipAddress = "";

  this._lastConnectionInfo = null;
  this._connectionInfoTimer = null;
  this._listeners = [];
  this._wifiDisableDelayId = null;

  if (hasRilSupport) {
    lazy.gTelephonyService.registerListener(this);

    WifiManager.telephonyServiceId = this._getDefaultServiceId();
  }

  // Users of instances of nsITimer should keep a reference to the timer until
  // it is no longer needed in order to assure the timer is fired.
  this._callbackTimer = null;

  // A list of requests to turn wifi on or off.
  this._stateRequests = [];

  // A list of imported wifi certificate nicknames.
  this._certNicknames = [];

  // Given a connection status network, takes a network and
  // prepares it for the DOM.
  netToDOM = function(net) {
    if (!net) {
      return null;
    }
    var ssid = net.ssid ? dequote(net.ssid) : null;
    var mode = net.mode;
    var frequency = net.frequency;
    var security = WifiConfigUtils.matchKeyMgmtToSecurity(net);
    var password;
    if (
      ("psk" in net && net.psk) ||
      ("password" in net && net.password) ||
      (WifiManager.wapiPsk in net && net[WifiManager.wapiPsk]) ||
      isInWepKeyList(net)
    ) {
      password = "*";
    }

    var pub = new WifiNetwork(ssid, mode, frequency, security, password);
    if (net.identity) {
      pub.identity = dequote(net.identity);
    }
    if ("netId" in net) {
      pub.known = true;
      if (net.netId == wifiInfo.networkId && self._ipAddress) {
        pub.connected = true;
        if (lastNetwork.everValidated) {
          pub.hasInternet = true;
          pub.captivePortalDetected = false;
        } else if (lastNetwork.everCaptivePortalDetected) {
          pub.hasInternet = false;
          pub.captivePortalDetected = true;
        } else {
          pub.hasInternet = false;
          pub.captivePortalDetected = false;
        }
      }
    }
    pub.hidden = net.scanSsid;

    if (
      "caCert" in net &&
      net.caCert &&
      net.caCert.indexOf("keystore://WIFI_SERVERCERT_" === 0)
    ) {
      pub.serverCertificate = net.caCert.substr(27);
    }
    if (net.subjectMatch) {
      pub.subjectMatch = net.subjectMatch;
    }
    if (
      "clientCert" in net &&
      net.clientCert &&
      net.clientCert.indexOf("keystore://WIFI_USERCERT_" === 0)
    ) {
      pub.userCertificate = net.clientCert.substr(25);
    }
    if (WifiManager.wapiKeyType in net && net[WifiManager.wapiKeyType]) {
      if (net[WifiManager.wapiKeyType] == 1) {
        pub.pskType = "HEX";
      }
    }
    if (WifiManager.asCertFile in net && net[WifiManager.asCertFile]) {
      pub.wapiAsCertificate = net[WifiManager.asCertFile];
    }
    if (WifiManager.userCertFile in net && net[WifiManager.userCertFile]) {
      pub.wapiUserCertificate = net[WifiManager.userCertFile];
    }
    return pub;
  };

  netFromDOM = function(net, configured) {
    // Takes a network from the DOM and makes it suitable for insertion into
    // configuredNetworks (that is calling addNetwork will
    // do the right thing).
    // NB: Modifies net in place: safe since we don't share objects between
    // the dom and the chrome code.

    let keyMgmt = WifiConfigUtils.matchSecurityToKeyMgmt(net);
    // Things that are useful for the UI but not to us.
    delete net.bssid;
    delete net.signalStrength;
    delete net.relSignalStrength;
    delete net.security;
    delete net.wpsSupported;
    delete net.dontConnect;

    if (!configured) {
      configured = {};
    }

    net.ssid = quote(net.ssid);

    let wep = false;
    if (keyMgmt === "WEP") {
      wep = true;
      keyMgmt = "NONE";
    } else if (keyMgmt === "SAE") {
      net.pmf = true;
    }
    configured.keyMgmt = net.keyMgmt = keyMgmt;

    if (net.hidden) {
      configured.scanSsid = net.scanSsid = true;
      delete net.hidden;
    }

    function checkAssign(name, checkStar) {
      if (name in net) {
        let value = net[name];
        if (!value || (checkStar && value === "*")) {
          if (name in configured && configured[name] !== "*") {
            net[name] = configured[name];
          } else {
            delete net[name];
          }
        } else {
          configured[name] = net[name] = quote(value);
        }
      }
    }

    checkAssign("psk", true);
    checkAssign("identity", false);
    checkAssign("password", true);
    if (wep) {
      configured.authAlg = net.authAlg = "OPEN SHARED";
      if (net.wep && net.wep != "*") {
        net.keyIndex = net.keyIndex || 0;
        configured["wepKey" + net.keyIndex] = net[
          "wepKey" + net.keyIndex
        ] = isWepHexKey(net.wep) ? net.wep : quote(net.wep);
        configured.wepTxKeyIndex = net.wepTxKeyIndex = net.keyIndex;
      }
    }

    if (net.keyMgmt == "WAPI-PSK") {
      net.proto = configured.proto = "WAPI";
      if (net.pskType == "HEX") {
        net[WifiManager.wapiKeyType] = 1;
      }
      net[WifiManager.wapiPsk] = configured[WifiManager.wapiPsk] = quote(
        net.wapiPsk
      );

      if (WifiManager.wapiPsk != "wapi_psk") {
        delete net.wapiPsk;
      }
    }
    if (net.keyMgmt == "WAPI-CERT") {
      net.proto = configured.proto = "WAPI";
      if (hasValidProperty("wapiAsCertificate")) {
        net[WifiManager.asCertFile] = quote(net.wapiAsCertificate);
        delete net.wapiAsCertificate;
      }
      if (hasValidProperty("wapiUserCertificate")) {
        net[WifiManager.userCertFile] = quote(net.wapiUserCertificate);
        delete net.wapiUserCertificate;
      }
    }

    function hasValidProperty(name) {
      return name in net && net[name] != null;
    }

    if (hasValidProperty("eap") || net.keyMgmt.includes("WPA-EAP")) {
      if (hasValidProperty("pin")) {
        net.pin = quote(net.pin);
      }

      if (net.eap === "SIM" || net.eap === "AKA" || net.eap === "AKA'") {
        configured.simIndex = net.simIndex = net.simIndex || 1;
      }

      if (hasValidProperty("phase1")) {
        net.phase1 = quote(net.phase1);
      }

      if (hasValidProperty("phase2")) {
        net.phase2 = quote(net.phase2);
      }

      if (hasValidProperty("serverCertificate")) {
        net.caCert = quote(
          "keystore://WIFI_SERVERCERT_" + net.serverCertificate
        );
      }

      if (hasValidProperty("subjectMatch")) {
        net.subjectMatch = quote(net.subjectMatch);
      }

      if (hasValidProperty("userCertificate")) {
        let userCertName = "WIFI_USERCERT_" + net.userCertificate;
        net.clientCert = quote("keystore://" + userCertName);

        let wifiCertService = Cc["@mozilla.org/wifi/certservice;1"].getService(
          Ci.nsIWifiCertService
        );
        if (wifiCertService.hasPrivateKey(userCertName)) {
          net.engine = true;
          net.engineId = quote("keystore");
          net.privateKeyId = quote("WIFI_USERKEY_" + net.userCertificate);
        }
      }
    }
    return net;
  };

  WifiManager.onsupplicantconnection = function() {
    debug("Connected to supplicant");
    // Register listener for mobileConnectionService
    if (hasRilSupport && !self.mobileConnectionRegistered) {
      lazy.gMobileConnectionService
        .getItemByServiceId(WifiManager.telephonyServiceId)
        .registerListener(self);
      self.mobileConnectionRegistered = true;
    }
    // Set current country code
    let countryCode = self.pickWifiCountryCode();
    if (countryCode !== null) {
      self.lastKnownCountryCode = countryCode;
      WifiManager.setCountryCode(countryCode, function() {});
    }
    // wifi enabled and reset open network notification.
    OpenNetworkNotifier.clearPendingNotification();

    // Start to trigger period scan.
    WifiManager.startDelayScan();

    // Notify everybody, even if they didn't ask us to come up.
    WifiManager.getMacAddress(function(result) {
      self._macAddress = result.macAddress;
      debug("Got mac: " + self._macAddress);
      self._fireEvent("wifiUp", { macAddress: self._macAddress });
    });

    // Use external processing for SIM/USIM operations.
    WifiManager.setExternalSim(true, function() {});
  };

  WifiManager.onsupplicantlost = function() {
    WifiManager.supplicantStarted = false;
    WifiManager.state = "UNINITIALIZED";
    debug("Supplicant died!");

    // wifi disabled and reset open network notification.
    OpenNetworkNotifier.clearPendingNotification();
    WifiConfigManager.clearLastSelectedNetwork();

    // Notify everybody, even if they didn't ask us to come up.
    self._fireEvent("wifiDown", {});
  };

  WifiManager.onnetworkdisable = function() {
    debug(
      "Notify event: " +
        WifiConfigManager.QUALITY_NETWORK_SELECTION_DISABLE_REASON[this.reason]
    );
    self.handleNetworkConnectionFailure(lastNetwork);
    switch (this.reason) {
      case WifiConstants.DISABLED_DHCP_FAILURE:
        let security = WifiConfigUtils.matchKeyMgmtToSecurity(lastNetwork);
        if (security == "WEP") {
          self._fireEvent("onauthenticationfailed", {
            network: netToDOM(lastNetwork),
          });
        } else {
          self._fireEvent("ondhcpfailed", { network: netToDOM(lastNetwork) });
        }
        break;
      case WifiConstants.DISABLED_AUTHENTICATION_FAILURE:
        self._fireEvent("onauthenticationfailed", {
          network: netToDOM(lastNetwork),
        });
        break;
      case WifiConstants.DISABLED_ASSOCIATION_REJECTION:
        self._fireEvent("onassociationreject", {
          network: netToDOM(lastNetwork),
        });
        break;
    }
  };

  WifiManager.onstatechange = function() {
    debug("State change: " + this.prevState + " -> " + this.state);

    if (
      self._connectionInfoTimer &&
      this.state !== "CONNECTED" &&
      this.state !== "COMPLETED"
    ) {
      self._stopConnectionInfoTimer();
    }

    switch (this.state) {
      case "ASSOCIATING":
        // id has not yet been filled in, so we can only report the ssid and
        // bssid. mode and frequency are simply made up.
        let networkEverValidated = false;
        let networkEverCaptivePortalDetected = false;
        if (lastNetwork) {
          networkEverValidated = lastNetwork.everValidated;
          networkEverCaptivePortalDetected =
            lastNetwork.everCaptivePortalDetected;
        }

        lastNetwork = {
          bssid: wifiInfo.bssid,
          ssid: quote(wifiInfo.wifiSsid),
          mode: WifiConstants.MODE_ESS,
          frequency: 0,
          everValidated: networkEverValidated,
          everCaptivePortalDetected: networkEverCaptivePortalDetected,
        };
        if (wifiInfo.networkId !== WifiConstants.INVALID_NETWORK_ID) {
          lastNetwork.netId = wifiInfo.networkId;
        }
        self.handleConnectionStateChanged("onconnecting", lastNetwork);
        break;
      case "ASSOCIATED":
        if (!lastNetwork) {
          lastNetwork = {};
        }
        lastNetwork.bssid = wifiInfo.bssid;
        lastNetwork.ssid = quote(wifiInfo.wifiSsid);
        lastNetwork.netId = wifiInfo.networkId;
        WifiConfigManager.fetchNetworkConfiguration(lastNetwork, () => {
          self.handleConnectionStateChanged("onconnecting", lastNetwork);
        });
        break;
      case "COMPLETED":
        var _oncompleted = function() {
          self._startConnectionInfoTimer();
          self.handleConnectionStateChanged("onassociate", lastNetwork);
        };

        if (!lastNetwork) {
          lastNetwork = {};
        }
        lastNetwork.bssid = wifiInfo.bssid;
        lastNetwork.ssid = quote(wifiInfo.wifiSsid);
        lastNetwork.netId = wifiInfo.networkId;
        WifiConfigManager.setEverConnected(lastNetwork, true);
        WifiConfigManager.fetchNetworkConfiguration(lastNetwork, _oncompleted);
        break;
      case "CONNECTED":
        // BSSID is read after connected, update it.
        lastNetwork.bssid = wifiInfo.bssid;
        break;
      case "DISCONNECTED":
        // wpa_supplicant may give us a "DISCONNECTED" event even if
        // we are already in "DISCONNECTED" state.
        if (
          WifiNetworkInterface.info.state ===
            Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED &&
          (this.prevState === "INITIALIZING" ||
            this.prevState === "DISCONNECTED" ||
            this.prevState === "INTERFACE_DISABLED" ||
            this.prevState === "INACTIVE" ||
            this.prevState === "UNINITIALIZED" ||
            this.prevState === "SCANNING")
        ) {
          return;
        }

        if (!lastNetwork) {
          lastNetwork = {};
        }
        self._fireEvent("ondisconnect", { network: netToDOM(lastNetwork) });

        lastNetwork.everValidated = false;
        lastNetwork.everCaptivePortalDetected = false;
        if (self.handlePromptUnvalidatedId !== null) {
          clearTimeout(self.handlePromptUnvalidatedId);
          self.handlePromptUnvalidatedId = null;
        }
        self._ipAddress = "";

        WifiManager.connectionDropped(function() {});
        // When disconnected, traffic stats should be reset.
        WifiManager.lastTrafficStats = null;

        // wifi disconnected and reset open network notification.
        OpenNetworkNotifier.clearPendingNotification();

        WifiNetworkInterface.info.state =
          Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;
        WifiNetworkInterface.info.ips = [];
        WifiNetworkInterface.info.prefixLengths = [];
        WifiNetworkInterface.info.gateways = [];
        WifiNetworkInterface.info.dnses = [];
        lazy.gNetworkManager.updateNetworkInterface(WifiNetworkInterface);
        break;
      case "WPS_TIMEOUT":
        self._fireEvent("onwpstimeout", {});
        break;
      case "WPS_FAIL":
        self._fireEvent("onwpsfail", {});
        break;
      case "WPS_OVERLAP_DETECTED":
        self._fireEvent("onwpsoverlap", {});
        break;
      case "AUTHENTICATING":
        if (!lastNetwork) {
          lastNetwork = {};
        }
        lastNetwork.bssid = wifiInfo.bssid;
        lastNetwork.ssid = quote(wifiInfo.wifiSsid);
        lastNetwork.netId = wifiInfo.networkId;
        self._fireEvent("onauthenticating", { network: netToDOM(lastNetwork) });
        break;
      case "SCANNING":
        break;
    }
  };

  WifiManager.onnetworkconnected = function() {
    if (!this.info || !this.info.ipaddr_str) {
      debug("Network information is invalid.");
      return;
    }

    for (let i in WifiNetworkInterface.info.ips) {
      if (!WifiNetworkInterface.info.ips[i].includes(".")) {
        continue;
      }

      if (!WifiNetworkInterface.info.ips[i].includes(this.info.ipaddr_str)) {
        debug("router subnet change, disconnect it.");
        WifiManager.disconnect(function() {
          WifiManager.reassociate(function() {});
        });
        return;
      }
    }

    let maskLength = netHelpers.getMaskLength(
      netHelpers.stringToIP(this.info.mask_str)
    );
    if (!maskLength) {
      maskLength = 32; // max prefix for IPv4.
    }

    let netConnect = WifiManager.getHttpProxyNetwork(lastNetwork);
    if (netConnect) {
      WifiNetworkInterface.httpProxyHost = netConnect.httpProxyHost;
      WifiNetworkInterface.httpProxyPort = netConnect.httpProxyPort;
    }

    self.handlePromptUnvalidatedId = setTimeout(
      self.handlePromptUnvalidated.bind(self),
      WifiConstants.PROMPT_UNVALIDATED_DELAY_MS
    );

    // wifi connected and reset open network notification.
    OpenNetworkNotifier.clearPendingNotification();
    // reset disable counter tracking
    WifiManager.loopDetectionCount = 0;
    WifiConfigManager.clearDisableReasonCounter(
      escape(wifiInfo.wifiSsid) + wifiInfo.security
    );

    WifiNetworkInterface.updateConfig("updated", {
      state: Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED,
      ip: this.info.ipaddr_str,
      gateway: this.info.gateway_str,
      prefixLengths: maskLength,
      dnses: [this.info.dns1_str, this.info.dns2_str],
    });

    self._ipAddress = this.info.ipaddr_str;

    if (autoRoaming) {
      // TODO: We're doing this because we only support dual stack currently.
      // We'll need arp/ns detection mechanism to achieve ipv4/ipv6 independent.

      // Reset IPv6 interface to trigger neighbor solicit and
      // router solicit immediately.
      lazy.gNetworkService.setIpv6Status(
        WifiManager.ifname,
        false,
        function() {}
      );
      lazy.gNetworkService.setIpv6Status(
        WifiManager.ifname,
        true,
        function() {}
      );
    }

    // We start the connection information timer when we associate, but
    // don't have our IP address until here. Make sure that we fire a new
    // connectionInformation event with the IP address the next time the
    // timer fires.
    self._lastConnectionInfo = null;
    self.handleConnectionStateChanged("onconnect", lastNetwork);
    WifiManager.postDhcpSetup(function() {});

    // Reset roaming state.
    autoRoaming = false;
  };

  WifiManager.onscanresultsavailable = function() {
    WifiManager.getScanResults(this.type, function(data) {
      if (data.status != SUCCESS) {
        if (self._wantScanResults.length !== 0) {
          self._wantScanResults.forEach(function(callback) {
            callback(null);
          });
          self._wantScanResults = [];
        }
        return;
      }

      let scanResults = data.getScanResults();
      WifiManager.cachedScanResults = [];

      scanResults = WifiManager.filterExpiredBssid(scanResults);
      let capabilities = WifiManager.getCapabilities();

      // Now that we have scan results, there's no more need to continue
      // scanning. Ignore any errors from this command.
      self.networksArray = [];
      let numOpenNetworks = 0;
      for (let i = 0; i < scanResults.length; i++) {
        let result = scanResults[i];
        let ie = result.getInfoElement();
        debug(
          " * " +
            result.ssid +
            " " +
            result.bssid +
            " " +
            result.frequency +
            " " +
            result.tsf +
            " " +
            result.capability +
            " " +
            result.signal +
            " " +
            result.associated
        );

        // Discard network with invalid SSID
        if (result.ssid == null || !result.ssid.length) {
          continue;
        }

        let signalLevel = result.signal / 100;
        let infoElement = WifiConfigUtils.parseInformationElements(ie);
        let flags = WifiConfigUtils.parseCapabilities(
          infoElement,
          result.capability
        );
        let passpoint = WifiConfigUtils.parsePasspointElements(infoElement);

        if (passpoint.isInterworking) {
          debug(
            "[" + result.ssid + "] passpoint info: " + JSON.stringify(passpoint)
          );
        }

        /* Skip networks with unknown or unsupported modes. */
        if (!capabilities.mode.includes(WifiConfigUtils.getMode(flags))) {
          continue;
        }

        // If this is the first time that we've seen this SSID in the scan
        // results, add it to the list along with any other information.
        // Also, we use the highest signal strength that we see.
        let network = new ScanResult(
          result.ssid,
          result.bssid,
          result.frequency,
          flags,
          signalLevel,
          result.associated
        );

        let networkKey = WifiConfigUtils.getNetworkKey(network);
        network.networkKey = networkKey;
        network.passpoint = passpoint;

        // Check whether open network is found.
        if (network.security.includes("OPEN")) {
          numOpenNetworks++;
        }

        var configuredNetworks = WifiConfigManager.configuredNetworks;
        if (networkKey in configuredNetworks) {
          let known = configuredNetworks[networkKey];
          network.known = true;
          network.netId = known.netId;

          if ("hasInternet" in known) {
            network.hasInternet = known.hasInternet;
          }

          if ("captivePortalDetected" in known) {
            network.captivePortalDetected = known.captivePortalDetected;
          }

          if (
            network.netId == wifiInfo.networkId &&
            self._ipAddress &&
            result.associated
          ) {
            network.connected = true;
            if (lastNetwork.everValidated) {
              network.hasInternet = true;
              network.captivePortalDetected = false;
            } else if (lastNetwork.everCaptivePortalDetected) {
              network.hasInternet = false;
              network.captivePortalDetected = true;
            } else {
              network.hasInternet = false;
              network.captivePortalDetected = false;
            }
          }

          if ("identity" in known && known.identity) {
            network.identity = dequote(known.identity);
          }

          // Note: we don't hand out passwords here! The * marks that there
          // is a password that we're hiding.
          if (
            ("psk" in known && known.psk) ||
            ("password" in known && known.password) ||
            isInWepKeyList(known)
          ) {
            network.password = "*";
          }

          if (!WifiManager.configurationChannels.has(network.netId)) {
            WifiManager.configurationChannels.set(network.netId, []);
          }

          let channels = WifiManager.configurationChannels.get(network.netId);
          if (!channels.includes(result.frequency)) {
            channels.push(result.frequency);
          }
        }

        let signal = WifiConfigUtils.calculateSignal(Number(signalLevel));
        if (signal > network.relSignalStrength) {
          network.relSignalStrength = signal;
        }
        self.networksArray.push(network);
        WifiManager.cachedScanResults.push(network);
        WifiManager.cachedScanTime = Date.now();
      }

      // Notify to user whenever there is any open network.
      if (
        WifiNetworkInterface.info.state ==
        Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED
      ) {
        // Notify open network only when SCREEN ON
        // TODO: Will not count the number of times found open network while screen off.
        //      Normally, it only do PNO scan and can not find open network while screen off.
        //      But not sure problems else will be occured by this and maybe improve latter.
        if (numOpenNetworks > 0 && WifiManager.isScreenOn()) {
          OpenNetworkNotifier.handleOpenNetworkFound();
        }
      }

      if (!WifiManager.wpsStarted && self.networksArray.length) {
        self.handleScanResults(self.networksArray);
      }
      if (self._wantScanResults.length !== 0) {
        self._wantScanResults.forEach(callback => {
          callback(self.networksArray);
        });
        self._wantScanResults = [];
      }
      self._fireEvent("scanresult", {
        scanResult: JSON.stringify(self.networksArray),
      });
    });
  };

  WifiManager.onregisterimslistener = function() {
    let imsService;
    try {
      imsService = lazy.gImsRegService.getHandlerByServiceId(
        WifiManager.telephonyServiceId
      );
    } catch (e) {
      return;
    }
    if (this.register) {
      imsService.registerListener(self);
      let imsDelayTimeout = Services.prefs.getIntPref(
        "vowifi.delay.timer",
        7000
      );
      debug("delay " + imsDelayTimeout / 1000 + " secs for disabling wifi");
      self._wifiDisableDelayId = setTimeout(
        WifiManager.setWifiDisable,
        imsDelayTimeout,
        this.callback
      );
    } else {
      if (self._wifiDisableDelayId === null) {
        return;
      }
      clearTimeout(self._wifiDisableDelayId);
      self._wifiDisableDelayId = null;
      imsService.unregisterListener(self);
    }
  };

  WifiManager.onstationinfoupdate = function() {
    self._fireEvent("stationinfoupdate", { station: this.station });
  };

  OpenNetworkNotifier.onopennetworknotification = function() {
    self._fireEvent("opennetwork", { availability: this.enabled });
  };

  WifiManager.onstartconnectioninfotimer = function() {
    if (WifiManager.state != "COMPLETED") {
      return;
    }
    self._startConnectionInfoTimer();
  };

  WifiManager.onstopconnectioninfotimer = function() {
    self._stopConnectionInfoTimer();
  };

  // Update debug status
  WifiManager.syncDebug();

  // Get settings value and initialize.
  this.getSettings(SETTINGS_WIFI_DEBUG_ENABLED);
  this.getSettings(SETTINGS_AIRPLANE_MODE);
  this.getSettings(SETTINGS_WIFI_CERT_NICKNAME);
  this.getSettings(SETTINGS_PASSPOINT_ENABLED);

  // Add settings observers.
  this.addSettingsObserver(SETTINGS_WIFI_DEBUG_ENABLED);
  this.addSettingsObserver(SETTINGS_AIRPLANE_MODE);
  this.addSettingsObserver(SETTINGS_AIRPLANE_MODE_STATUS);
  this.addSettingsObserver(SETTINGS_PASSPOINT_ENABLED);

  // Initialize configured network from config file.
  WifiConfigManager.loadFromStore();

  // Read wifi tethering configuration.
  this._wifiTetheringConfig = TetheringConfigStore.read(
    TetheringConfigStore.TETHERING_TYPE_WIFI
  );
  if (!this._wifiTetheringConfig) {
    this._wifiTetheringConfig = this.fillWifiTetheringConfiguration({});
    TetheringConfigStore.write(
      TetheringConfigStore.TETHERING_TYPE_WIFI,
      this._wifiTetheringConfig,
      null
    );
  }
  this._fireEvent("tetheringconfigchange", {
    wifiTetheringConfig: this._wifiTetheringConfig,
  });
}

function translateState(state) {
  switch (state) {
    case "INTERFACE_DISABLED":
    case "INACTIVE":
    case "SCANNING":
    case "DISCONNECTED":
    default:
      return "disconnected";

    case "AUTHENTICATING":
    case "ASSOCIATING":
    case "ASSOCIATED":
    case "FOUR_WAY_HANDSHAKE":
    case "GROUP_HANDSHAKE":
      return "connecting";

    case "COMPLETED":
      return WifiManager.getDhcpInfo() ? "connected" : "associated";
  }
}

WifiWorker.prototype = {
  classID: WIFIWORKER_CID,
  contractID: WIFIWORKER_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIWorkerHolder,
    Ci.nsIWifi,
    Ci.nsIObserver,
    Ci.nsISettingsObserver,
    Ci.nsIMobileConnectionListener,
    Ci.nsIImsRegListener,
    Ci.nsIIccListener,
    Ci.nsITelephonyListener,
  ]),

  disconnectedByWifi: false,

  disconnectedByWifiTethering: false,

  lastKnownCountryCode: null,
  mobileConnectionRegistered: false,

  _airplaneMode: false,
  _airplaneMode_status: null,

  _listeners: null,

  _driverOnlyMode: false,

  wifiNetworkInfo: wifiInfo,

  // nsIImsRegListener
  notifyEnabledStateChanged(aEnabled) {},

  notifyPreferredProfileChanged(aProfile) {},

  notifyCapabilityChanged(aCapability, aUnregisteredReason) {
    let self = this;
    debug("notifyCapabilityChanged: aCapability = " + aCapability);
    if (
      aCapability != Ci.nsIImsRegHandler.IMS_CAPABILITY_VOICE_OVER_WIFI &&
      aCapability != Ci.nsIImsRegHandler.IMS_CAPABILITY_VIDEO_OVER_WIFI
    ) {
      WifiManager.setWifiDisable(function() {
        self.requestDone();
      });
    }
  },

  // nsIMobileConnectionListener
  notifyVoiceChanged() {},

  notifyDataChanged() {},

  notifyDataError(aMessage) {},

  notifyCFStateChanged(
    aAction,
    aReason,
    aNumber,
    aTimeSeconds,
    aServiceClass
  ) {},

  notifyEmergencyCbModeChanged(aActive, aTimeoutMs) {},

  notifyOtaStatusChanged(aStatus) {},

  notifyRadioStateChanged() {},

  notifyClirModeChanged(aMode) {},

  notifyLastKnownNetworkChanged() {
    let countryCode = this.getCountryName();
    if (countryCode !== null && countryCode !== this.lastKnownCountryCode) {
      debug("Set country code = " + countryCode);
      this.lastKnownCountryCode = countryCode;
      if (WifiManager.enabled) {
        WifiManager.setCountryCode(countryCode, function() {});
      }
    }
  },

  notifyLastKnownHomeNetworkChanged() {},

  notifyNetworkSelectionModeChanged() {},

  notifyDeviceIdentitiesChanged() {},

  notifySignalStrengthChanged() {},

  notifyModemRestart(aReason) {},

  // nsIIccListener
  notifyIccInfoChanged() {},

  notifyStkCommand(aStkProactiveCmd) {},

  notifyStkSessionEnd() {},

  notifyCardStateChanged() {},

  notifyIsimInfoChanged() {},

  // nsITelephonyListener
  callStateChanged(aLength, allInfo) {
    if (!Services.prefs.getBoolPref("dom.emergency.wifi-control", true)) {
      return;
    }

    // According to DBH requirement, wifi need to be enabled during emergency
    // call session for scanning. To simplify, here we try to control low level
    // wifi modules but not change wifi state on upper layer.
    let callState = Ci.nsITelephonyService.CALL_STATE_UNKNOWN;
    for (let info of allInfo) {
      if (info && info.isEmergency) {
        callState = info.callState;
        break;
      }
    }

    if (callState == Ci.nsITelephonyService.CALL_STATE_UNKNOWN) {
      // Unknown call state, do nothing.
    } else if (callState == Ci.nsITelephonyService.CALL_STATE_DISCONNECTED) {
      if (this._driverOnlyMode) {
        debug("Exit emergency call session, disable wifi");
        WifiManager.stopWifi(result => {
          if (result.status == SUCCESS) {
            this._driverOnlyMode = false;
          }
        });
      }
    } else if (!WifiManager.enabled && !this._driverOnlyMode) {
      debug(
        "Enter emergency call session by state " + callState + ", enable wifi"
      );
      WifiManager.startWifi(result => {
        if (result.status == SUCCESS) {
          this._driverOnlyMode = true;
        }
      });
    }
  },

  getCountryName() {
    for (let simId = 0; simId < WifiManager.numRil; simId++) {
      let countryName = lazy.gPhoneNumberUtils.getCountryName(simId);
      if (typeof countryName == "string") {
        return countryName.toUpperCase();
      }
    }
    return null;
  },

  pickWifiCountryCode() {
    if (this.lastKnownCountryCode) {
      return this.lastKnownCountryCode;
    }
    return this.getCountryName();
  },

  isAirplaneMode() {
    let airplaneMode = false;

    if (
      this._airplaneMode &&
      (this._airplaneMode_status === "enabling" ||
        this._airplaneMode_status === "enabled")
    ) {
      airplaneMode = true;
    }
    return airplaneMode;
  },

  handleScanResults(scanResults) {
    WifiNetworkSelector.updateBssidDenylist(function(updated) {
      if (updated) {
        WifiManager.setFirmwareRoamingConfiguration();
      }

      WifiNetworkSelector.selectNetwork(
        scanResults,
        translateState(WifiManager.state),
        wifiInfo,
        function(config) {
          if (config == null) {
            return;
          }
          debug("candidate configuration = " + uneval(config));
          var currentAssociationId =
            wifiInfo.wifiSsid + ":" + wifiInfo.networkId;
          var targetAssociationId = config.ssid + ":" + config.netId;

          if (
            config.bssid == wifiInfo.bssid &&
            WifiManager.isConnectState(WifiManager.state)
          ) {
            debug(currentAssociationId + " is already the best choice!");
          } else if (
            config.netId == wifiInfo.networkId ||
            config.networkKey == escape(wifiInfo.wifiSsid) + wifiInfo.security
          ) {
            // Framework initiates roaming only if firmware doesn't support.
            if (!WifiManager.isFeatureSupported(FEATURE_CONTROL_ROAMING)) {
              debug(
                "Roaming from " +
                  currentAssociationId +
                  " to " +
                  targetAssociationId
              );
              WifiManager.lastDriverRoamAttempt = 0;
              WifiManager.setAutoRoam(config, function() {});
            }
          } else {
            debug(
              "Reconnect from " +
                currentAssociationId +
                " to " +
                targetAssociationId
            );
            WifiManager.setAutoConnect(config, function() {});
          }
        }
      );
    });
  },

  handleNetworkConnectionFailure(network, callback) {
    let netId = network.netId;
    if (netId !== WifiConstants.INVALID_NETWORK_ID) {
      WifiManager.disableNetwork(function() {
        debug("disable network - id: " + netId);
      });
    }
    if (callback) {
      callback(true);
    }
  },

  handleConnectionStateChanged(event, network) {
    if (autoRoaming) {
      // Currently we are in roaming stage, which means that we've already
      // connected and have IP address. Therefore, the supplicant state should
      // not be reflected into upper layer.
      // Network state should be updated after roaming completed.
      return;
    }
    this._fireEvent(event, { network: netToDOM(network) });
  },

  handlePromptUnvalidatedId: null,
  handlePromptUnvalidated() {
    this.handlePromptUnvalidatedId = null;
    if (lastNetwork == null) {
      return;
    }
    debug("handlePromptUnvalidated: " + uneval(lastNetwork));
    if (lastNetwork.everValidated || lastNetwork.everCaptivePortalDetected) {
      return;
    }
    this._fireEvent("wifihasinternet", {
      hasInternet: false,
      network: netToDOM(lastNetwork),
    });
  },

  // Internal methods.
  waitForScan(callback) {
    this._wantScanResults.push(callback);
  },

  _startConnectionInfoTimer() {
    if (this._connectionInfoTimer) {
      return;
    }

    var self = this;
    function getConnectionInformation() {
      WifiManager.getConnectionInfo(function(result) {
        if (result.status != SUCCESS) {
          debug("Failed to get connection information");
          return;
        }

        let connInfo = result.signalPoll();
        // See comments in WifiConfigUtils.calculateSignal for information about this.
        if (!connInfo.length) {
          self._lastConnectionInfo = null;
          return;
        }
        let [rssi, linkSpeed, frequency] = [
          connInfo[0],
          connInfo[1],
          connInfo[2],
        ];

        if (rssi > 0) {
          rssi -= 256;
        }
        if (rssi <= WifiConfigUtils.MIN_RSSI) {
          rssi = WifiConfigUtils.MIN_RSSI;
        } else if (rssi >= WifiConfigUtils.MAX_RSSI) {
          rssi = WifiConfigUtils.MAX_RSSI;
        }

        if (shouldBroadcastRSSIForIMS(rssi, wifiInfo.rssi)) {
          self.deliverListenerEvent("notifyRssiChanged", [rssi]);
        }
        wifiInfo.setRssi(rssi);

        if (linkSpeed > 0) {
          wifiInfo.setLinkSpeed(linkSpeed);
        }
        if (frequency > 0) {
          wifiInfo.setFrequency(frequency);
        }
        let info = {
          signalStrength: rssi,
          relSignalStrength: WifiConfigUtils.calculateSignal(rssi),
          linkSpeed,
          ipAddress: self._ipAddress,
        };
        let last = self._lastConnectionInfo;
        // Only fire the event if the link speed changed or the signal
        // strength changed by more than 10%.
        function tensPlace(percent) {
          return (percent / 10) | 0;
        }
        if (
          last &&
          last.linkSpeed === info.linkSpeed &&
          last.ipAddress === info.ipAddress &&
          tensPlace(last.relSignalStrength) ===
            tensPlace(info.relSignalStrength)
        ) {
          return;
        }
        self._lastConnectionInfo = info;
        debug("Firing connectioninfoupdate: " + uneval(info));
        self._fireEvent("connectioninfoupdate", info);
      });
    }

    // Prime our _lastConnectionInfo immediately and fire the event at the
    // same time.
    getConnectionInformation();

    // Now, set up the timer for regular updates.
    this._connectionInfoTimer = Cc["@mozilla.org/timer;1"].createInstance(
      Ci.nsITimer
    );
    this._connectionInfoTimer.init(
      getConnectionInformation,
      3000,
      Ci.nsITimer.TYPE_REPEATING_SLACK
    );
  },

  _stopConnectionInfoTimer() {
    if (!this._connectionInfoTimer) {
      return;
    }

    this._connectionInfoTimer.cancel();
    this._connectionInfoTimer = null;
    this._lastConnectionInfo = null;
  },

  // nsIWifi

  _domManagers: [],
  _fireEvent(message, data) {
    this._domManagers.forEach(function(manager) {
      // Note: We should never have a dead message manager here because we
      // observe our child message managers shutting down, below.
      manager.sendAsyncMessage("WifiManager:" + message, data);
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

  _clearPendingRequest() {
    if (this._domRequest.length === 0) {
      return;
    }
    this._domRequest.forEach(
      function(req) {
        this._sendMessage(
          req.name + ":Return",
          false,
          "Wifi is disabled",
          req.msg
        );
      }.bind(this)
    );
  },

  receiveMessage: function MessageManager_receiveMessage(aMessage) {
    let msg = aMessage.data || {};
    msg.manager = aMessage.target;

    // Note: By the time we receive child-process-shutdown, the child process
    // has already forgotten its permissions so we do this before the
    // permissions check.
    if (aMessage.name === "child-process-shutdown") {
      let i;
      if ((i = this._domManagers.indexOf(msg.manager)) != -1) {
        this._domManagers.splice(i, 1);
      }
      for (i = this._domRequest.length - 1; i >= 0; i--) {
        if (this._domRequest[i].msg.manager === msg.manager) {
          this._domRequest.splice(i, 1);
        }
      }
      return true;
    }

    // We are interested in DOMRequests only.
    if (aMessage.name != "WifiManager:getState") {
      this._domRequest.push({ name: aMessage.name, msg });
    }

    switch (aMessage.name) {
      case "WifiManager:setWifiEnabled":
        this.setWifiEnabled(msg);
        break;
      case "WifiManager:getNetworks":
        this.getNetworks(msg);
        break;
      case "WifiManager:getKnownNetworks":
        this.getKnownNetworks(msg);
        break;
      case "WifiManager:associate":
        this.associate(msg);
        break;
      case "WifiManager:forget":
        this.forget(msg);
        break;
      case "WifiManager:wps":
        this.wps(msg);
        break;
      case "WifiManager:setPowerSavingMode":
        this.setPowerSavingMode(msg);
        break;
      case "WifiManager:setHttpProxy":
        this.setHttpProxy(msg);
        break;
      case "WifiManager:setStaticIpMode":
        this.setStaticIpMode(msg);
        break;
      case "WifiManager:importCert":
        this.importCert(msg);
        break;
      case "WifiManager:getImportedCerts":
        this.getImportedCerts(msg);
        break;
      case "WifiManager:deleteCert":
        this.deleteCert(msg);
        break;
      case "WifiManager:setWifiTethering":
        this.setWifiTethering(msg);
        break;
      case "WifiManager:getSoftapStations":
        this.getSoftapStations(msg);
        break;
      case "WifiManager:setOpenNetworkNotification":
        this.setOpenNetworkNotificationEnabled(msg);
        break;
      case "WifiManager:setPasspointConfig":
        this.setPasspointConfig(msg);
        break;
      case "WifiManager:getPasspointConfigs":
        this.getPasspointConfigs(msg);
        break;
      case "WifiManager:removePasspointConfig":
        this.removePasspointConfig(msg);
        break;
      case "WifiManager:getState": {
        if (!this._domManagers.includes(msg.manager)) {
          this._domManagers.push(msg.manager);
        }

        let net = lastNetwork ? netToDOM(lastNetwork) : null;
        return {
          network: net,
          connectionInfo: this._lastConnectionInfo,
          enabled: WifiManager.enabled,
          status: translateState(WifiManager.state),
          macAddress: this._macAddress,
          capabilities: WifiManager.getCapabilities(),
          wifiTetheringConfig: this._wifiTetheringConfig,
        };
      }
    }
  },

  getNetworks(msg) {
    const message = "WifiManager:getNetworks:Return";
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    let sent = false;
    let callback = function(networks) {
      if (sent) {
        return;
      }
      sent = true;
      this._sendMessage(message, networks !== null, networks, msg);
    }.bind(this);
    this.waitForScan(callback);

    WifiManager.handleScanRequest(
      true,
      function(ok) {
        // If the scan command succeeded, we're done.
        if (ok) {
          return;
        }

        // Avoid sending multiple responses.
        if (sent) {
          return;
        }

        sent = true;
        // if last results are available, return the cached scan results.
        if (
          WifiManager.cachedScanTime > 0 &&
          Date.now() - WifiManager.cachedScanTime <
            WifiConstants.WIFI_MAX_SCAN_CACHED_TIME
        ) {
          this._sendMessage(
            message,
            !!WifiManager.cachedScanResults.length,
            WifiManager.cachedScanResults,
            msg
          );
          return;
        }

        WifiManager.cachedScanTime = 0;
        WifiManager.cachedScanResults = [];
        // Otherwise, let the client know that it failed, it's responsible for
        // trying again in a few seconds.
        this._sendMessage(message, false, "ScanFailed", msg);
      }.bind(this)
    );
  },

  getWifiScanResults(callback) {
    var count = 0;
    var timer = null;
    var self = this;

    if (!WifiManager.enabled) {
      callback.onfailure();
      return;
    }

    self.waitForScan(waitForScanCallback);
    doScan();
    function doScan() {
      WifiManager.handleScanRequest(true, function(ok) {
        if (!ok) {
          if (!timer) {
            count = 0;
            timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          }

          if (count++ >= 3) {
            timer = null;
            self._wantScanResults.splice(
              self._wantScanResults.indexOf(waitForScanCallback),
              1
            );
            callback.onfailure();
            return;
          }

          // Else it's still running, continue waiting.
          timer.initWithCallback(doScan, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
        }
      });
    }

    function waitForScanCallback(networks) {
      if (networks === null) {
        callback.onfailure();
        return;
      }

      var wifiScanResults = new Array([]);
      for (let net in networks) {
        let value = networks[net];
        wifiScanResults.push(transformResult(value));
      }
      callback.onready(wifiScanResults.length, wifiScanResults);
    }

    function transformResult(element) {
      var result = new WifiScanResult();
      result.connected = false;
      for (let id in element) {
        if (id === "__exposedProps__") {
          continue;
        }
        if (id === "security") {
          result[id] = 0;
          var security = element[id];
          if (
            security === "WPA-PSK" ||
            security === "WPA2-PSK" ||
            security === "WPA/WPA2-PSK"
          ) {
            result[id] = Ci.nsIWifiScanResult.WPA_PSK;
          } else if (security === "WPA-EAP") {
            result[id] = Ci.nsIWifiScanResult.WPA_EAP;
          } else if (security === "WEP") {
            result[id] = Ci.nsIWifiScanResult.WEP;
          } else if (security === "WAPI-PSK") {
            result[id] = Ci.nsIWifiScanResult.WAPI_PSK;
          } else if (security === "WAPI-CERT") {
            result[id] = Ci.nsIWifiScanResult.WAPI_CERT;
          } else if (security === "SAE") {
            result[id] = Ci.nsIWifiScanResult.SAE;
          } else {
            result[id] = 0;
          }
        } else {
          result[id] = element[id];
        }
      }
      return result;
    }
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
        debug("listener for " + aName + " threw an exception: " + e);
      }
    }
  },

  _macAddress: null,
  get macAddress() {
    return this._macAddress;
  },

  getKnownNetworks(msg) {
    const message = "WifiManager:getKnownNetworks:Return";
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }
    var networks = [];
    var configuredNetworks = WifiConfigManager.configuredNetworks;
    for (let networkKey in configuredNetworks) {
      networks.push(netToDOM(configuredNetworks[networkKey]));
    }
    this._sendMessage(message, true, networks, msg);
  },

  handleWifiEnabled(enabled, callback) {
    // FIXME: this would also return current request.
    // Reply error to pending requests.
    // if (!enabled) {
    //   this._clearPendingRequest();
    // }
    WifiManager.setWifiEnabled(enabled, callback);
  },

  handleWifiEnabledCallback(message, msg, result) {
    if (!result) {
      this.requestDone();
      this._sendMessage(message, false, "Set wifi enabled failed", msg);
      return;
    }

    this._sendMessage(message, true, true, msg);
    if (!WifiManager.supplicantStarted) {
      this._clearPendingRequest();
    }
    this.requestDone();
  },

  setWifiEnabled(msg) {
    const message = "WifiManager:setWifiEnabled:Return";
    let enabled = msg.data;

    // Store persist wifi state.
    Services.prefs.setBoolPref(PREFERENCE_WIFI_ENABLED, enabled);

    // Make sure Wifi hotspot is idle before switching to Wifi mode.
    if (
      enabled &&
      WifiManager.isWifiTetheringEnabled(WifiManager.tetheringState)
    ) {
      this.queueRequest(
        {
          command: "setWifiApEnabled",
          value: false,
        },
        function(data) {
          this.disconnectedByWifi = true;
          this.handleHotspotEnabled(
            false,
            this.handleHotspotEnabledCallback.bind(this, message, msg)
          );
        }.bind(this)
      );
    }

    this.queueRequest(
      {
        command: "setWifiEnabled",
        value: enabled,
      },
      function(data) {
        this.handleWifiEnabled(
          enabled,
          this.handleWifiEnabledCallback.bind(this, message, msg)
        );
      }.bind(this)
    );

    if (!enabled) {
      let isWifiAffectTethering = Services.prefs.getBoolPref(
        "wifi.affect.tethering"
      );
      if (
        isWifiAffectTethering &&
        this.disconnectedByWifi &&
        this.isAirplaneMode() === false
      ) {
        this.queueRequest(
          {
            command: "setWifiApEnabled",
            value: true,
          },
          function(data) {
            this.handleHotspotEnabled(
              true,
              this.handleHotspotEnabledCallback.bind(this, message, msg)
            );
          }.bind(this)
        );
      }
      this.disconnectedByWifi = false;
    }
  },

  // requestDone() must be called to before callback complete(or error)
  // so next queue in the request quene can be executed.
  queueRequest(data, callback) {
    if (!callback) {
      throw Components.Exception(
        "Try to enqueue a request without callback",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let optimizeCommandList = ["setWifiEnabled", "setWifiApEnabled"];
    if (optimizeCommandList.includes(data.command)) {
      this._stateRequests = this._stateRequests.filter(function(element) {
        return element.data.command !== data.command;
      });
    }

    this._stateRequests.push({
      data,
      callback,
    });

    this.nextRequest();
  },

  fillWifiTetheringConfiguration(aConfig) {
    let config = {};
    let check = function(field, _default) {
      config[field] = field in aConfig ? aConfig[field] : _default;
    };

    // Replace IPs since webidl provide simple naming without wifi prefix.
    if (aConfig.startIp) {
      aConfig.wifiStartIp = aConfig.startIp;
    }
    if (aConfig.endIp) {
      aConfig.wifiEndIp = aConfig.endIp;
    }

    // TODO: need more conditions if we support 5GHz hotspot.
    // Random choose 1, 6, 11 if there's no specify channel.
    let defaultChannel = 11 - Math.floor(Math.random() * 3) * 5;

    check("ssid", DEFAULT_HOTSPOT_SSID);
    check("security", DEFAULT_HOTSPOT_SECURITY_TYPE);
    check("key", DEFAULT_HOTSPOT_SECURITY_PASSWORD);
    check("hidden", DEFAULT_HOTSPOT_HIDDEN);
    check("band", DEFAULT_HOTSPOT_BAND);
    check("channel", DEFAULT_HOTSPOT_CHANNEL);
    check("enable11N", DEFAULT_HOTSPOT_ENABLE_11N);
    check("enable11AC", DEFAULT_HOTSPOT_ENABLE_11AC);
    check("enableACS", DEFAULT_HOTSPOT_ENABLE_ACS);
    check("ip", DEFAULT_HOTSPOT_IP);
    check("prefix", DEFAULT_HOTSPOT_PREFIX);
    check("wifiStartIp", DEFAULT_HOTSPOT_DHCPSERVER_STARTIP);
    check("wifiEndIp", DEFAULT_HOTSPOT_DHCPSERVER_ENDIP);
    check("usbStartIp", DEFAULT_USB_DHCPSERVER_STARTIP);
    check("usbEndIp", DEFAULT_USB_DHCPSERVER_ENDIP);
    check("channel", defaultChannel);

    return config;
  },

  getWifiTetheringConfiguration(enable) {
    let config = {};
    let params = this._wifiTetheringConfig;

    config = this.fillWifiTetheringConfiguration(params);
    config.enable = enable;
    config.mode = enable ? WIFI_FIRMWARE_AP : WIFI_FIRMWARE_STATION;
    config.link = enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN;

    // Check the format to prevent netd from crash.
    if (enable) {
      if (!config.ssid || config.ssid == "") {
        debug("Invalid SSID value.");
        return null;
      }

      if (config.security == WIFI_SECURITY_TYPE_NONE) {
        config.keyMgmt = AP_SECURITY_NONE;
      } else if (config.security == WIFI_SECURITY_TYPE_WPA_PSK) {
        config.keyMgmt = AP_SECURITY_WPA;
      } else if (config.security == WIFI_SECURITY_TYPE_WPA2_PSK) {
        config.keyMgmt = AP_SECURITY_WPA2;
      } else {
        debug("Invalid softap security type");
        return null;
      }

      if (config.security != WIFI_SECURITY_TYPE_NONE && !config.key) {
        debug("Invalid security password.");
        return null;
      }

      config.countryCode = this.pickWifiCountryCode();
      if (config.countryCode === null && config.band == AP_BAND_5GHZ) {
        debug("Failed to set 5G hotspot without country code.");
        return null;
      }

      if (config.enableACS) {
        config.acsExcludeDfs = true;
      }
    }

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

    if (config.channel > 11 || config.channel < 1) {
      debug("Invalid channel value.");
      return null;
    }

    return config;
  },

  handleHotspotEnabled(enabled, callback) {
    let configuration = this.getWifiTetheringConfiguration(enabled);

    if (!configuration) {
      debug("Invalid Wifi Tethering configuration.");
      this.requestDone();
      callback(false);
      return;
    }
    WifiManager.setWifiApEnabled(enabled, configuration, callback);
  },

  handleHotspotEnabledCallback(message, msg, result) {
    if (!result) {
      msg.data.reason = msg.data.enabled
        ? "Enable WIFI tethering failed"
        : "Disable WIFI tethering failed";
      this._sendMessage(message, false, msg.data, msg);
    } else {
      this._sendMessage(message, true, msg.data, msg);
    }
    this.requestDone();
  },

  _wifiTetheringConfig: {},

  setWifiTethering(msg) {
    const message = "WifiManager:setWifiTethering:Return";
    let enabled = msg.data.enabled;

    if (
      !WifiManager.isWifiTetheringEnabled(WifiManager.tetheringState) &&
      Object.entries(msg.data.config).length
    ) {
      let newConfig = this.fillWifiTetheringConfiguration(msg.data.config);
      if (
        JSON.stringify(this._wifiTetheringConfig) != JSON.stringify(newConfig)
      ) {
        this._wifiTetheringConfig = newConfig;
        TetheringConfigStore.write(
          TetheringConfigStore.TETHERING_TYPE_WIFI,
          this._wifiTetheringConfig,
          null
        );
        this._fireEvent("tetheringconfigchange", {
          wifiTetheringConfig: this._wifiTetheringConfig,
        });
      }
    }

    if (enabled && WifiManager.enabled) {
      this.queueRequest(
        {
          command: "setWifiEnabled",
          value: false,
        },
        function(data) {
          this.disconnectedByWifiTethering = true;
          this.handleWifiEnabled(
            false,
            this.handleWifiEnabledCallback.bind(this, message, msg)
          );
        }.bind(this)
      );
    }

    this.queueRequest(
      {
        command: "setWifiApEnabled",
        value: enabled,
      },
      function(data) {
        this.handleHotspotEnabled(
          enabled,
          this.handleHotspotEnabledCallback.bind(this, message, msg)
        );
      }.bind(this)
    );

    if (!enabled) {
      let isTetheringAffectWifi = Services.prefs.getBoolPref(
        "tethering.affect.wifi"
      );
      if (
        isTetheringAffectWifi &&
        this.disconnectedByWifiTethering &&
        this.isAirplaneMode() === false
      ) {
        this.queueRequest(
          {
            command: "setWifiEnabled",
            value: true,
          },
          function(data) {
            this.handleWifiEnabled(
              true,
              this.handleWifiEnabledCallback.bind(this, message, msg)
            );
          }.bind(this)
        );
      }
      this.disconnectedByWifiTethering = false;
    }
  },

  getSoftapStations(msg) {
    const message = "WifiManager:getSoftapStations:Return";

    if (!WifiManager.isWifiTetheringEnabled(WifiManager.tetheringState)) {
      this._sendMessage(message, false, "WifiTethering is disabled", msg);
      return;
    }

    WifiManager.getSoftapStations(
      function(result) {
        if (result.status != SUCCESS) {
          this._sendMessage(message, false, null, msg);
          return;
        }

        this._sendMessage(message, true, result.numStations, msg);
      }.bind(this)
    );
  },

  associate(msg) {
    const message = "WifiManager:associate:Return";
    let network = msg.data;

    let privnet = network;
    let dontConnect = privnet.dontConnect;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }
    let self = this;
    function networkReady() {
      let makeConnection = function(network) {
        WifiManager.loopDetectionCount = 0;
        WifiConfigManager.updateLastSelectedNetwork(network.netId, function() {
          WifiManager.startToConnect(network, function(ok) {
            self._sendMessage(message, ok, ok, msg);
          });
        });
      };

      function prepareForConnection() {
        let callback = networks => {
          for (let net of networks) {
            if (networkKey == net.networkKey) {
              makeConnection(privnet);
              WifiNetworkSelector.skipNetworkSelection = false;
              return;
            }
          }
          WifiNetworkSelector.skipNetworkSelection = false;
          self._sendMessage(message, false, "network not found", msg);
        };

        self.waitForScan(callback);
        WifiNetworkSelector.skipNetworkSelection = true;
        WifiManager.handleScanRequest(true, function() {});
      }

      if (dontConnect) {
        self._sendMessage(message, true, "Wifi has been recorded", msg);
      } else {
        prepareForConnection();
      }
    }

    let networkKey = WifiConfigUtils.getNetworkKey(privnet);
    let configured;

    if (networkKey in this._addingNetworks) {
      this._sendMessage(message, false, "Racing associates");
      return;
    }

    var configuredNetworks = WifiConfigManager.configuredNetworks;
    if (networkKey in configuredNetworks) {
      configured = configuredNetworks[networkKey];
    }

    netFromDOM(privnet, configured);

    privnet.hasEverConnected = false;
    debug("Device associate with ap info: " + uneval(privnet));

    if (configured) {
      // network already saved, should keep previous network id
      privnet.netId = configured.netId;
    }

    this._addingNetworks[networkKey] = privnet;
    WifiManager.saveNetwork(
      privnet,
      function(ok) {
        delete this._addingNetworks[networkKey];

        if (!ok) {
          this._sendMessage(message, false, "Network is misconfigured", msg);
          return;
        }

        // Sync configuration before connect, otherwise, we may get not enough parameters
        // from DOM for the configured network.
        let config = WifiConfigManager.getNetworkConfiguration(privnet.netId);
        if (!config) {
          debug("getNetworkConfiguration failed");
          this._sendMessage(
            message,
            false,
            "Get configured network failed",
            msg
          );
          return;
        }

        privnet = config;

        // Supplicant will connect to access point directly without disconnect
        // if we are currently associated, hence trigger a disconnect
        if (
          WifiManager.state == "COMPLETED" &&
          lastNetwork &&
          lastNetwork.netId !== WifiConstants.INVALID_NETWORK_ID &&
          lastNetwork.netId !== privnet.netId
        ) {
          WifiManager.disconnect(function() {
            networkReady();
          });
        } else {
          networkReady();
        }
      }.bind(this)
    );
  },

  forget(msg) {
    const message = "WifiManager:forget:Return";
    let self = this;
    let network = msg.data;
    debug("Device forget with ap info: " + uneval(network));
    if (!WifiManager.enabled) {
      self._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    let networkKey = WifiConfigUtils.getNetworkKey(network);
    var configuredNetworks = WifiConfigManager.configuredNetworks;
    if (!(networkKey in configuredNetworks)) {
      self._sendMessage(
        message,
        false,
        "Trying to forget an unknown network",
        msg
      );
      return;
    }

    let configured = configuredNetworks[networkKey];
    WifiManager.removeNetwork(configured.netId, function(ok) {
      if (!ok) {
        self._sendMessage(message, false, "Unable to remove the network", msg);
        return;
      }
      if (configured.netId == wifiInfo.networkId) {
        WifiManager.removeNetworks(function() {});
        WifiManager.disconnect(function() {});
      }
      self._sendMessage(message, true, true, msg);
    });
  },

  wps(msg) {
    const message = "WifiManager:wps:Return";
    let self = this;
    let detail = msg.data;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.removeNetworks(function(ok) {
      if (!ok) {
        self._sendMessage(message, false, "Failed to remove networks", msg);
        return;
      }

      if (detail.method === "pbc") {
        WifiManager.wpsPbc(detail.bssid, function(result) {
          if (result.status == SUCCESS) {
            WifiManager.wpsStarted = true;
            self._sendMessage(message, true, true, msg);
          } else {
            WifiManager.wpsStarted = false;
            self._sendMessage(message, false, "WPS PBC failed", msg);
          }
        });
      } else if (detail.method === "pin") {
        WifiManager.wpsPin(detail.bssid, detail.pin, function(result) {
          if (result.status == SUCCESS) {
            WifiManager.wpsStarted = true;
            self._sendMessage(message, true, true, msg);
          } else {
            WifiManager.wpsStarted = false;
            self._sendMessage(message, false, "WPS PIN failed", msg);
          }
        });
      } else if (detail.method === "display") {
        WifiManager.wpsDisplay(detail.bssid, function(result) {
          if (result.status == SUCCESS && result.generatedPin) {
            WifiManager.wpsStarted = true;
            self._sendMessage(message, true, result.generatedPin, msg);
          } else {
            WifiManager.wpsStarted = false;
            self._sendMessage(message, false, "WPS Display failed", msg);
          }
        });
      } else if (detail.method === "cancel") {
        WifiManager.wpsCancel(function(result) {
          if (result.status == SUCCESS) {
            WifiManager.wpsStarted = false;
            self._sendMessage(message, true, true, msg);
          } else {
            self._sendMessage(message, false, "WPS Cancel failed", msg);
          }
        });
      } else {
        self._sendMessage(
          message,
          false,
          "Invalid WPS method = " + detail.method,
          msg
        );
      }
    });
  },

  setPowerSavingMode(msg) {
    const message = "WifiManager:setPowerSavingMode:Return";
    let self = this;
    let enabled = msg.data;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    // Some wifi drivers may not implement this command. Set power mode
    // even if suspend optimization command failed.
    WifiManager.setSuspendOptimizationsMode(
      POWER_MODE_SETTING_CHANGED,
      enabled,
      function(ok) {
        if (ok) {
          self._sendMessage(message, true, true, msg);
        } else {
          self._sendMessage(
            message,
            false,
            "Set power saving mode failed",
            msg
          );
        }
      }
    );
  },

  setHttpProxy(msg) {
    const message = "WifiManager:setHttpProxy:Return";
    let self = this;
    let network = msg.data.network;
    let info = msg.data.info;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.configureHttpProxy(network, info, function(ok) {
      if (ok) {
        // If configured network is current connected network
        // need update http proxy immediately.
        let setNetworkKey = WifiConfigUtils.getNetworkKey(network);
        let lastNetworkKey = lastNetwork
          ? WifiConfigUtils.getNetworkKey(lastNetwork)
          : null;
        if (setNetworkKey === lastNetworkKey) {
          WifiManager.setHttpProxy(network);
        }

        self._sendMessage(message, true, true, msg);
      } else {
        self._sendMessage(message, false, "Set http proxy failed", msg);
      }
    });
  },

  setStaticIpMode(msg) {
    const message = "WifiManager:setStaticIpMode:Return";
    let self = this;
    let network = msg.data.network;
    let info = msg.data.info;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    // To compatiable with DHCP returned info structure, do translation here
    info.ipaddr_str = info.ipaddr;
    info.proxy_str = info.proxy;
    info.gateway_str = info.gateway;
    info.dns1_str = info.dns1;
    info.dns2_str = info.dns2;

    WifiManager.setStaticIpMode(network, info, function(ok) {
      if (ok) {
        self._sendMessage(message, true, true, msg);
      } else {
        self._sendMessage(message, false, "Set static ip mode failed", msg);
      }
    });
  },

  importCert(msg) {
    const message = "WifiManager:importCert:Return";
    let self = this;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.importCert(msg.data, function(result) {
      if (result.status == SUCCESS) {
        if (!self._certNicknames.includes(result.nickname)) {
          self._certNicknames.push(result.nickname);
          self.setSettings(SETTINGS_WIFI_CERT_NICKNAME, self._certNicknames);
        }

        let usageString = ["ServerCert", "UserCert"];
        let usageArray = [];
        for (let i = 0; i < usageString.length; i++) {
          if (result.usageFlag & (0x01 << i)) {
            usageArray.push(usageString[i]);
          }
        }

        self._sendMessage(
          message,
          true,
          {
            nickname: result.nickname,
            usage: usageArray,
          },
          msg
        );
      } else if (result.duplicated) {
        self._sendMessage(message, false, "Import duplicate certificate", msg);
      } else {
        self._sendMessage(message, false, "Import damaged certificate", msg);
      }
    });
  },

  getImportedCerts(msg) {
    const message = "WifiManager:getImportedCerts:Return";
    let self = this;

    let importedCerts = {
      ServerCert: [],
      UserCert: [],
    };

    let UsageMapping = {
      SERVERCERT: "ServerCert",
      USERCERT: "UserCert",
    };

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    if (!this._certNicknames.length) {
      this._sendMessage(message, true, importedCerts, msg);
      return;
    }

    let nicknameList = [];
    let filteredCerts = WifiManager.filterCert(this._certNicknames);
    for (let nickname of filteredCerts) {
      let certNickname = /WIFI\_([A-Z]*)\_(.*)/.exec(nickname);
      if (!certNickname) {
        continue;
      }
      nicknameList.push(certNickname[2]);
      importedCerts[UsageMapping[certNickname[1]]].push(certNickname[2]);
    }

    self._certNicknames = nicknameList.slice();
    self.setSettings(SETTINGS_WIFI_CERT_NICKNAME, self._certNicknames);

    self._sendMessage(message, true, importedCerts, msg);
  },

  deleteCert(msg) {
    const message = "WifiManager:deleteCert:Return";
    let self = this;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.deleteCert(msg.data, function(result) {
      if (result.status == SUCCESS) {
        let found = self._certNicknames.indexOf(result.nickname);
        if (found !== -1) {
          self._certNicknames.splice(found, 1);
          self.setSettings(SETTINGS_WIFI_CERT_NICKNAME, self._certNicknames);
        }
      }

      self._sendMessage(
        message,
        result.status == SUCCESS,
        result.status == SUCCESS ? "Success" : "Delete Cert failed",
        msg
      );
    });
  },

  setPasspointConfig(msg) {
    const message = "WifiManager:setPasspointConfig:Return";
    let self = this;

    if (!PasspointManager) {
      this._sendMessage(message, false, "PasspointManager is null", msg);
      return;
    }

    if (
      !PasspointManager.isPasspointSupported() ||
      !PasspointManager.passpointEnabled
    ) {
      this._sendMessage(
        message,
        false,
        "Passpoint not supported or disabled",
        msg
      );
      return;
    }

    let config = msg.data;
    if (!config || !config.homeSp || !config.credential) {
      this._sendMessage(message, false, "Invalid passpoint configuration", msg);
      return;
    }

    PasspointManager.setConfiguration(msg.data, function(success) {
      self._sendMessage(
        message,
        success,
        success
          ? msg.data.homeSp.fqdn
          : "Failed to set passpoint configuration",
        msg
      );
    });
  },

  getPasspointConfigs(msg) {
    const message = "WifiManager:getPasspointConfigs:Return";

    if (!PasspointManager) {
      this._sendMessage(message, false, "PasspointManager is null", msg);
      return;
    }

    if (
      !PasspointManager.isPasspointSupported() ||
      !PasspointManager.passpointEnabled
    ) {
      this._sendMessage(
        message,
        false,
        "Passpoint not supported or disabled",
        msg
      );
      return;
    }

    let passpointConfigs = PasspointManager.getConfigurations();
    this._sendMessage(message, true, passpointConfigs, msg);
  },

  removePasspointConfig(msg) {
    const message = "WifiManager:removePasspointConfig:Return";
    let self = this;

    if (!PasspointManager) {
      this._sendMessage(message, false, "PasspointManager is null", msg);
      return;
    }

    if (
      !PasspointManager.isPasspointSupported() ||
      !PasspointManager.passpointEnabled
    ) {
      this._sendMessage(
        message,
        false,
        "Passpoint not supported or disabled",
        msg
      );
      return;
    }

    PasspointManager.removeConfiguration(msg.data, function(success) {
      self._sendMessage(message, success, success ? "Success" : "Failure", msg);
    });
  },

  setOpenNetworkNotificationEnabled(msg) {
    let enabled = msg.data;
    Services.prefs.setBoolPref(PREFERENCE_WIFI_NOTIFICATION, enabled);
    OpenNetworkNotifier.setOpenNetworkNotifyEnabled(enabled);
  },

  updateWifiState() {
    let enabled = Services.prefs.getBoolPref(PREFERENCE_WIFI_ENABLED, false);
    debug((enabled ? "Enable" : "Disable") + " wifi from preference");

    if (!enabled && !WifiManager.enabled) {
      BinderServices.wifi.onWifiStateChanged(WifiConstants.WIFI_STATE_DISABLED);
    }

    this.handleWifiEnabled(enabled, function(ok) {});
  },

  updateOpenNetworkNotification() {
    let enabled = Services.prefs.getBoolPref(
      PREFERENCE_WIFI_NOTIFICATION,
      false
    );
    debug(
      (enabled ? "Enable" : "Disable") +
        " open network notification from preference"
    );
    OpenNetworkNotifier.setOpenNetworkNotifyEnabled(enabled);
  },

  // This is a bit ugly, but works. In particular, this depends on the fact
  // that RadioManager never actually tries to get the worker from us.
  get worker() {
    throw Components.Exception("Not implemented", Cr.NS_ERROR_INVALID_ARG);
  },

  shutdown() {
    debug("shutting down ...");
    if (hasRilSupport) {
      lazy.gTelephonyService.unregisterListener(this);
    }
    this.handleWifiEnabled(false, function() {});
    this.removeSettingsObserver(SETTINGS_WIFI_DEBUG_ENABLED);
    this.removeSettingsObserver(SETTINGS_AIRPLANE_MODE);
    this.removeSettingsObserver(SETTINGS_AIRPLANE_MODE_STATUS);
    this.removeSettingsObserver(SETTINGS_PASSPOINT_ENABLED);

    Services.obs.removeObserver(this, kFinalUiStartUpTopic);
    Services.obs.removeObserver(this, kXpcomShutdownChangedTopic);
    Services.obs.removeObserver(this, kScreenStateChangedTopic);
    Services.obs.removeObserver(this, kInterfaceAddressUpdatedTopic);
    Services.obs.removeObserver(this, kInterfaceAddressRemovedTopic);
    Services.obs.removeObserver(this, kInterfaceDnsInfoTopic);
    Services.obs.removeObserver(this, kRouteChangedTopic);
    Services.obs.removeObserver(this, kCaptivePortalResult);
    Services.obs.removeObserver(this, kOpenCaptivePortalLoginEvent);
    Services.obs.removeObserver(this, kCaptivePortalLoginSuccessEvent);
    Services.prefs.removeObserver(kPrefDefaultServiceId, this);
  },

  // TODO: Remove command queue in Bug 1050147.
  requestProcessing: false, // Hold while dequeue and execution a request.
  // Released upon the request is fully executed,
  // i.e, mostly after callback is done.
  requestDone() {
    this.requestProcessing = false;
    this.nextRequest();
  },

  nextRequest() {
    // No request to process
    if (this._stateRequests.length === 0) {
      return;
    }

    // Handling request, wait for it.
    if (this.requestProcessing) {
      return;
    }

    // Hold processing lock
    this.requestProcessing = true;

    // Find next valid request
    let request = this._stateRequests.shift();

    request.callback(request.data);
  },

  _getDefaultServiceId() {
    let id = Services.prefs.getIntPref(kPrefDefaultServiceId, 0);
    let numRil = Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);

    if (id >= numRil || id < 0) {
      id = 0;
    }
    return id;
  },

  /* eslint-disable complexity */
  // nsIObserver implementation
  observe(subject, topic, data) {
    switch (topic) {
      case kFinalUiStartUpTopic:
        this.updateWifiState();
        this.updateOpenNetworkNotification();
        break;
      case kXpcomShutdownChangedTopic:
        this.shutdown();

        // Ensure the supplicant is detached from B2G to avoid XPCOM shutdown
        // blocks forever.
        WifiManager.ensureSupplicantDetached(() => {
          let wifiService = Cc["@mozilla.org/wifi/service;1"].getService(
            Ci.nsIWifiProxyService
          );
          wifiService.shutdown();
          let wifiCertService = Cc[
            "@mozilla.org/wifi/certservice;1"
          ].getService(Ci.nsIWifiCertService);
          wifiCertService.shutdown();
        });

        if (this.mobileConnectionRegistered) {
          lazy.gMobileConnectionService
            .getItemByServiceId(WifiManager.telephonyServiceId)
            .unregisterListener(this);
          this.mobileConnectionRegistered = false;
        }

        for (let simId = 0; simId < WifiManager.numRil; simId++) {
          lazy.gIccService.getIccByServiceId(simId).unregisterListener(this);
        }
        break;

      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (data === kPrefDefaultServiceId && hasRilSupport) {
          let defaultServiceId = this._getDefaultServiceId();
          if (defaultServiceId == WifiManager.telephonyServiceId) {
            return;
          }
          if (this.mobileConnectionRegistered) {
            lazy.gMobileConnectionService
              .getItemByServiceId(WifiManager.telephonyServiceId)
              .unregisterListener(this);
            this.mobileConnectionRegistered = false;
          }
          WifiManager.telephonyServiceId = defaultServiceId;
          lazy.gMobileConnectionService
            .getItemByServiceId(WifiManager.telephonyServiceId)
            .registerListener(this);
          this.mobileConnectionRegistered = true;
        }
        break;

      case kScreenStateChangedTopic:
        let enabled = data === "on";
        debug("Receive ScreenStateChanged=" + enabled);
        WifiManager.handleScreenStateChanged(enabled);
        break;

      case kInterfaceAddressUpdatedTopic:
      case kInterfaceAddressRemovedTopic:
        {
          // Format: "<addr> <iface> <flags> <scope>"
          let token = data.split(" ");
          if (token.length < 4) {
            return;
          }
          let iface = token[1];
          if (!iface.includes("wlan")) {
            return;
          }
          let action =
            topic == kInterfaceAddressUpdatedTopic ? "updated" : "removed";
          let addrTokens = token[0].split("/");
          let addr = addrTokens[0];
          let prefix = addrTokens[1];
          // We only handle IPv6 route advertisement address here.
          if (!addr.includes(":")) {
            return;
          }

          WifiNetworkInterface.updateConfig(action, {
            ip: addr,
            prefixLengths: prefix,
          });
        }
        break;

      case kInterfaceDnsInfoTopic:
        {
          // Format: "<interface> <lifetime> <servers>"
          let token = data.split(" ");
          if (token.length < 3) {
            return;
          }
          let iface = token[0];
          if (!iface.includes("wlan")) {
            return;
          }
          let dnses = token[2].split(",");
          WifiNetworkInterface.updateConfig("updated", { dnses });
        }
        break;

      case kRouteChangedTopic:
        {
          // Format: "<updated|removed> <route> <gateway> <iface>"
          let token = data.split(" ");
          if (token.length < 4) {
            return;
          }
          let action = token[0];
          let gateway = token[2];
          let iface = token[3];
          if (gateway === "" || !iface.includes("wlan")) {
            return;
          }

          WifiNetworkInterface.updateConfig(action, { gateway });
        }
        break;

      case kCaptivePortalResult:
        if (!(subject instanceof Ci.nsIPropertyBag2)) {
          return;
        }
        if (
          WifiNetworkInterface.info.state !==
          Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          debug(
            "ignore captive portal result event when network not connected"
          );
          return;
        }
        let props = subject.QueryInterface(Ci.nsIPropertyBag2);
        let landing = props.get("landing");
        let networkType = props.get("networkType");

        if (networkType != Ci.nsINetworkInfo.NETWORK_TYPE_WIFI) {
          debug("ignore none wifi type captive portal result");
          return;
        }

        if (lastNetwork == null) {
          debug("lastNetwork is null");
          return;
        }
        debug(
          "Receive lastNetwork : " +
            uneval(lastNetwork) +
            " landing = " +
            landing
        );
        lastNetwork.everValidated |= landing;
        WifiConfigManager.updateNetworkInternetAccess(
          lastNetwork.netId,
          lastNetwork.everValidated,
          lastNetwork.everCaptivePortalDetected,
          function() {}
        );
        this._fireEvent("wifihasinternet", {
          hasInternet: !!lastNetwork.everValidated,
          network: netToDOM(lastNetwork),
        });
        break;

      case kOpenCaptivePortalLoginEvent:
        if (
          WifiNetworkInterface.info.state !==
          Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          debug("ignore captive portal login event when network not connected");
          return;
        }
        if (lastNetwork == null) {
          debug("lastNetwork is null");
          return;
        }
        debug(
          "Receive captive-portal-login, set lastNetwork : " +
            uneval(lastNetwork)
        );
        lastNetwork.everCaptivePortalDetected = true;
        WifiConfigManager.updateNetworkInternetAccess(
          lastNetwork.netId,
          lastNetwork.everValidated,
          lastNetwork.everCaptivePortalDetected,
          function() {}
        );
        this._fireEvent("captiveportallogin", {
          loginSuccess: false,
          network: netToDOM(lastNetwork),
        });
        break;
      case kCaptivePortalLoginSuccessEvent:
        if (
          WifiNetworkInterface.info.state !==
          Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
        ) {
          debug("ignore captive portal login event when network not connected");
          return;
        }
        if (lastNetwork == null) {
          debug("lastNetwork is null");
          return;
        }
        debug("Receive captive-portal-login-success");
        this._fireEvent("captiveportallogin", {
          loginSuccess: true,
          network: netToDOM(lastNetwork),
        });
        break;
    }
  },
  /* eslint-enable complexity */

  // nsISettingsObserver
  observeSetting(aInfo) {
    if (aInfo) {
      let obj = JSON.parse(aInfo.value);
      this.handleSettingsChanged(aInfo.name, obj);
    }
  },

  // Helper functions.
  getSettings(aKey) {
    if (!aKey || !lazy.gSettingsManager) {
      return;
    }

    lazy.gSettingsManager.get(aKey, {
      resolve: info => {
        this.observeSetting(info);
      },
      reject: () => {
        debug("Get " + aKey + " failed");
      },
    });
  },

  setSettings(aKey, aValue) {
    if (!aKey || !lazy.gSettingsManager) {
      return;
    }
    if (aValue === null) {
      return;
    }

    debug("Set " + aKey + " settings with value: " + JSON.stringify(aValue));
    lazy.gSettingsManager.set([{ name: aKey, value: JSON.stringify(aValue) }], {
      resolve: () => {
        debug("Set " + aKey + " success");
      },
      reject: () => {
        debug("Set " + aKey + " failed");
      },
    });
  },

  addSettingsObserver(aKey) {
    if (!aKey || !lazy.gSettingsManager) {
      return;
    }

    lazy.gSettingsManager.addObserver(aKey, this, {
      resolve: () => {
        debug("Observe " + aKey + " success");
      },
      reject: () => {
        debug("Observe " + aKey + " failed");
      },
    });
  },

  removeSettingsObserver(aKey) {
    if (!aKey || !lazy.gSettingsManager) {
      return;
    }

    lazy.gSettingsManager.removeObserver(aKey, this, {
      resolve: () => {
        debug("Remove observer " + aKey + " success");
      },
      reject: () => {
        debug("Remove observer " + aKey + " failed");
      },
    });
  },

  handleSettingsChanged(aName, aValue) {
    switch (aName) {
      case SETTINGS_WIFI_DEBUG_ENABLED:
        debug("'" + SETTINGS_WIFI_DEBUG_ENABLED + "' is now " + aValue);
        gDebug = aValue;
        WifiManager.syncDebug();
        break;
      case SETTINGS_AIRPLANE_MODE:
        debug("'" + SETTINGS_AIRPLANE_MODE + "' is now " + aValue);
        this._airplaneMode = aValue === null ? false : aValue;
        break;
      case SETTINGS_AIRPLANE_MODE_STATUS:
        debug("'" + SETTINGS_AIRPLANE_MODE_STATUS + "' is now " + aValue);
        this._airplaneMode_status = aValue === null ? "" : aValue;
        break;
      case SETTINGS_PASSPOINT_ENABLED:
        debug("'" + SETTINGS_PASSPOINT_ENABLED + "' is now " + aValue);
        PasspointManager.passpointEnabled = aValue;
        break;
      case SETTINGS_WIFI_CERT_NICKNAME:
        this._certNicknames = aValue;
        break;
    }
  },
};
