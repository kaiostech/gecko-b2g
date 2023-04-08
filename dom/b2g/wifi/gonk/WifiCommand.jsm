/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WifiCommand"];

const WIFI_CMD_INITIALIZE = Ci.nsIWifiCommand.INITIALIZE;
const WIFI_CMD_GET_SUPPORTED_FEATURES =
  Ci.nsIWifiCommand.GET_SUPPORTED_FEATURES;
const WIFI_CMD_GET_MODULE_VERSION = Ci.nsIWifiCommand.GET_MODULE_VERSION;
const WIFI_CMD_GET_DEBUG_LEVEL = Ci.nsIWifiCommand.GET_DEBUG_LEVEL;
const WIFI_CMD_SET_DEBUG_LEVEL = Ci.nsIWifiCommand.SET_DEBUG_LEVEL;
const WIFI_CMD_SET_LOW_LATENCY_MODE = Ci.nsIWifiCommand.SET_LOW_LATENCY_MODE;
const WIFI_CMD_SET_CONCURRENCY_PRIORITY =
  Ci.nsIWifiCommand.SET_CONCURRENCY_PRIORITY;
const WIFI_CMD_START_WIFI = Ci.nsIWifiCommand.START_WIFI;
const WIFI_CMD_STOP_WIFI = Ci.nsIWifiCommand.STOP_WIFI;
const WIFI_CMD_GET_MAC_ADDRESS = Ci.nsIWifiCommand.GET_MAC_ADDRESS;
const WIFI_CMD_GET_STA_IFACE = Ci.nsIWifiCommand.GET_STA_IFACE;
const WIFI_CMD_SET_POWER_SAVE = Ci.nsIWifiCommand.SET_POWER_SAVE;
const WIFI_CMD_SET_SUSPEND_MODE = Ci.nsIWifiCommand.SET_SUSPEND_MODE;
const WIFI_CMD_SET_EXTERNAL_SIM = Ci.nsIWifiCommand.SET_EXTERNAL_SIM;
const WIFI_CMD_SET_AUTO_RECONNECT = Ci.nsIWifiCommand.SET_AUTO_RECONNECT;
const WIFI_CMD_SET_COUNTRY_CODE = Ci.nsIWifiCommand.SET_COUNTRY_CODE;
const WIFI_CMD_SET_BT_COEXIST_MODE = Ci.nsIWifiCommand.SET_BT_COEXIST_MODE;
const WIFI_CMD_SET_BT_COEXIST_SCAN_MODE =
  Ci.nsIWifiCommand.SET_BT_COEXIST_SCAN_MODE;
const WIFI_CMD_GET_LINK_LAYER_STATS = Ci.nsIWifiCommand.GET_LINK_LAYER_STATS;
const WIFI_CMD_SIGNAL_POLL = Ci.nsIWifiCommand.SIGNAL_POLL;
const WIFI_CMD_SET_FIRMWARE_ROAMING = Ci.nsIWifiCommand.SET_FIRMWARE_ROAMING;
const WIFI_CMD_CONFIG_FIRMWARE_ROAMING =
  Ci.nsIWifiCommand.CONFIG_FIRMWARE_ROAMING;
const WIFI_CMD_START_SINGLE_SCAN = Ci.nsIWifiCommand.START_SINGLE_SCAN;
const WIFI_CMD_STOP_SINGLE_SCAN = Ci.nsIWifiCommand.STOP_SINGLE_SCAN;
const WIFI_CMD_START_PNO_SCAN = Ci.nsIWifiCommand.START_PNO_SCAN;
const WIFI_CMD_STOP_PNO_SCAN = Ci.nsIWifiCommand.STOP_PNO_SCAN;
const WIFI_CMD_GET_SCAN_RESULTS = Ci.nsIWifiCommand.GET_SCAN_RESULTS;
const WIFI_CMD_GET_CHANNELS_FOR_BAND = Ci.nsIWifiCommand.GET_CHANNELS_FOR_BAND;
const WIFI_CMD_CONNECT = Ci.nsIWifiCommand.CONNECT;
const WIFI_CMD_RECONNECT = Ci.nsIWifiCommand.RECONNECT;
const WIFI_CMD_REASSOCIATE = Ci.nsIWifiCommand.REASSOCIATE;
const WIFI_CMD_DISCONNECT = Ci.nsIWifiCommand.DISCONNECT;
const WIFI_CMD_DISABLE_NETWORK = Ci.nsIWifiCommand.DISABLE_NETWORK;
const WIFI_CMD_GET_NETWORK = Ci.nsIWifiCommand.GET_NETWORK;
const WIFI_CMD_REMOVE_NETWORKS = Ci.nsIWifiCommand.REMOVE_NETWORKS;
const WIFI_CMD_START_ROAMING = Ci.nsIWifiCommand.START_ROAMING;
const WIFI_CMD_START_RSSI_MONITORING = Ci.nsIWifiCommand.START_RSSI_MONITORING;
const WIFI_CMD_STOP_RSSI_MONITORING = Ci.nsIWifiCommand.STOP_RSSI_MONITORING;
const WIFI_CMD_IDENTITY_RESPONSE = Ci.nsIWifiCommand.SEND_IDENTITY_RESPONSE;
const WIFI_CMD_GSM_AUTH_RESPONSE = Ci.nsIWifiCommand.SEND_GSM_AUTH_RESPONSE;
const WIFI_CMD_GSM_AUTH_FAILURE = Ci.nsIWifiCommand.SEND_GSM_AUTH_FAILURE;
const WIFI_CMD_UMTS_AUTH_RESPONSE = Ci.nsIWifiCommand.SEND_UMTS_AUTH_RESPONSE;
const WIFI_CMD_UMTS_AUTS_RESPONSE = Ci.nsIWifiCommand.SEND_UMTS_AUTS_RESPONSE;
const WIFI_CMD_UMTS_AUTH_FAILURE = Ci.nsIWifiCommand.SEND_UMTS_AUTH_FAILURE;
const WIFI_CMD_REQUEST_ANQP = Ci.nsIWifiCommand.REQUEST_ANQP;
const WIFI_CMD_SET_WPS_DETAIL = Ci.nsIWifiCommand.SET_WPS_DETAIL;
const WIFI_CMD_WPS_REGISTRAR = Ci.nsIWifiCommand.WPS_REGISTRAR;
const WIFI_CMD_WPS_PBC = Ci.nsIWifiCommand.WPS_PBC;
const WIFI_CMD_WPS_PIN_DISPLAY = Ci.nsIWifiCommand.WPS_PIN_DISPLAY;
const WIFI_CMD_WPS_CANCEL = Ci.nsIWifiCommand.WPS_CANCEL;
const WIFI_CMD_START_SOFTAP = Ci.nsIWifiCommand.START_SOFTAP;
const WIFI_CMD_STOP_SOFTAP = Ci.nsIWifiCommand.STOP_SOFTAP;
const WIFI_CMD_GET_AP_IFACE = Ci.nsIWifiCommand.GET_AP_IFACE;
const WIFI_CMD_GET_SOFTAP_STATION_NUMBER =
  Ci.nsIWifiCommand.GET_SOFTAP_STATION_NUMBER;

const WifiCommand = function(aControlMessage, aInterface, aSdkVersion) {
  var command = {};

  //-------------------------------------------------
  // Utilities.
  //-------------------------------------------------
  command.getSdkVersion = function() {
    return aSdkVersion;
  };

  //-------------------------------------------------
  // General commands.
  //-------------------------------------------------
  command.initialize = function(callback) {
    doCommand(WIFI_CMD_INITIALIZE, callback);
  };

  command.getSupportedFeatures = function(callback) {
    doCommand(WIFI_CMD_GET_SUPPORTED_FEATURES, callback);
  };

  command.getDriverModuleVersion = function(callback) {
    doCommand(WIFI_CMD_GET_MODULE_VERSION, callback);
  };

  command.getDebugLevel = function(callback) {
    doCommand(WIFI_CMD_GET_DEBUG_LEVEL, callback);
  };

  command.setDebugLevel = function(level, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_DEBUG_LEVEL,
      "debugLevel",
      level,
      callback
    );
  };

  command.setLowLatencyMode = function(enable, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_LOW_LATENCY_MODE,
      "enabled",
      enable,
      callback
    );
  };

  command.setStaHigherPriority = function(enable, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_CONCURRENCY_PRIORITY,
      "enabled",
      enable,
      callback
    );
  };

  command.startWifi = function(callback) {
    doCommand(WIFI_CMD_START_WIFI, callback);
  };

  command.stopWifi = function(callback) {
    doCommand(WIFI_CMD_STOP_WIFI, callback);
  };

  command.getMacAddress = function(callback) {
    doCommand(WIFI_CMD_GET_MAC_ADDRESS, callback);
  };

  command.getStaInterface = function(callback) {
    doCommand(WIFI_CMD_GET_STA_IFACE, callback);
  };

  command.setPowerSave = function(enable, callback) {
    doCommandWithParams(WIFI_CMD_SET_POWER_SAVE, "enabled", enable, callback);
  };

  command.setSuspendMode = function(enable, callback) {
    doCommandWithParams(WIFI_CMD_SET_SUSPEND_MODE, "enabled", enable, callback);
  };

  command.setExternalSim = function(enable, callback) {
    doCommandWithParams(WIFI_CMD_SET_EXTERNAL_SIM, "enabled", enable, callback);
  };

  command.enableAutoReconnect = function(enable, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_AUTO_RECONNECT,
      "enabled",
      enable,
      callback
    );
  };

  command.setCountryCode = function(code, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_COUNTRY_CODE,
      "countryCode",
      code,
      callback
    );
  };

  command.setBluetoothCoexistenceMode = function(mode, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_BT_COEXIST_MODE,
      "btCoexistenceMode",
      mode,
      callback
    );
  };

  command.setBluetoothCoexistenceScanMode = function(enable, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_BT_COEXIST_SCAN_MODE,
      "enabled",
      enable,
      callback
    );
  };

  command.getLinkLayerStats = function(callback) {
    doCommand(WIFI_CMD_GET_LINK_LAYER_STATS, callback);
  };

  command.getConnectionInfo = function(callback) {
    doCommand(WIFI_CMD_SIGNAL_POLL, callback);
  };

  command.setFirmwareRoaming = function(enable, callback) {
    doCommandWithParams(
      WIFI_CMD_SET_FIRMWARE_ROAMING,
      "enabled",
      enable,
      callback
    );
  };

  command.configureFirmwareRoaming = function(config, callback) {
    doCommandWithParams(
      WIFI_CMD_CONFIG_FIRMWARE_ROAMING,
      "roamingConfig",
      config,
      callback
    );
  };

  command.startScan = function(settings, callback) {
    doCommandWithParams(
      WIFI_CMD_START_SINGLE_SCAN,
      "scanSettings",
      settings,
      callback
    );
  };

  command.stopScan = function(callback) {
    doCommand(WIFI_CMD_STOP_SINGLE_SCAN, callback);
  };

  command.startPnoScan = function(settings, callback) {
    doCommandWithParams(
      WIFI_CMD_START_PNO_SCAN,
      "pnoScanSettings",
      settings,
      callback
    );
  };

  command.stopPnoScan = function(callback) {
    doCommand(WIFI_CMD_STOP_PNO_SCAN, callback);
  };

  command.getScanResults = function(type, callback) {
    doCommandWithParams(WIFI_CMD_GET_SCAN_RESULTS, "scanType", type, callback);
  };

  command.getChannelsForBand = function(band, callback) {
    doCommandWithParams(
      WIFI_CMD_GET_CHANNELS_FOR_BAND,
      "bandMask",
      band,
      callback
    );
  };

  command.startToConnect = function(config, callback) {
    doCommandWithParams(WIFI_CMD_CONNECT, "config", config, callback);
  };

  command.reconnect = function(callback) {
    doCommand(WIFI_CMD_RECONNECT, callback);
  };

  command.reassociate = function(callback) {
    doCommand(WIFI_CMD_REASSOCIATE, callback);
  };

  command.disconnect = function(callback) {
    doCommand(WIFI_CMD_DISCONNECT, callback);
  };

  command.disableNetwork = function(callback) {
    doCommand(WIFI_CMD_DISABLE_NETWORK, callback);
  };

  command.getSupplicantNetwork = function(callback) {
    doCommand(WIFI_CMD_GET_NETWORK, callback);
  };

  command.removeNetworks = function(callback) {
    doCommand(WIFI_CMD_REMOVE_NETWORKS, callback);
  };

  command.startToRoam = function(config, callback) {
    doCommandWithParams(WIFI_CMD_START_ROAMING, "config", config, callback);
  };

  command.startRssiMonitoring = function(callback) {
    doCommand(WIFI_CMD_START_RSSI_MONITORING, callback);
  };

  command.stopRssiMonitoring = function(callback) {
    doCommand(WIFI_CMD_STOP_RSSI_MONITORING, callback);
  };

  command.simIdentityResponse = function(identity, callback) {
    doCommandWithParams(
      WIFI_CMD_IDENTITY_RESPONSE,
      "identityResp",
      identity,
      callback
    );
  };

  command.simGsmAuthResponse = function(response, callback) {
    doCommandWithParams(
      WIFI_CMD_GSM_AUTH_RESPONSE,
      "gsmAuthResp",
      response,
      callback
    );
  };

  command.simGsmAuthFailure = function(callback) {
    doCommand(WIFI_CMD_GSM_AUTH_FAILURE, callback);
  };

  command.simUmtsAuthResponse = function(response, callback) {
    doCommandWithParams(
      WIFI_CMD_UMTS_AUTH_RESPONSE,
      "umtsAuthResp",
      response,
      callback
    );
  };

  command.simUmtsAutsResponse = function(response, callback) {
    doCommandWithParams(
      WIFI_CMD_UMTS_AUTS_RESPONSE,
      "umtsAutsResp",
      response,
      callback
    );
  };

  command.simUmtsAuthFailure = function(callback) {
    doCommand(WIFI_CMD_UMTS_AUTH_FAILURE, callback);
  };

  command.requestAnqp = function(data, callback) {
    doCommandWithParams(WIFI_CMD_REQUEST_ANQP, "anqpSettings", data, callback);
  };

  command.setWpsDetail = function(callback) {
    doCommand(WIFI_CMD_SET_WPS_DETAIL, callback);
  };

  command.wpsPbc = function(bssid, callback) {
    doCommandWithParams(
      WIFI_CMD_WPS_PBC,
      "wpsConfig",
      { bssid, pinCode: "" },
      callback
    );
  };

  command.wpsPin = function(bssid, pin, callback) {
    doCommandWithParams(
      WIFI_CMD_WPS_REGISTRAR,
      "wpsConfig",
      { bssid, pinCode: pin },
      callback
    );
  };

  command.wpsDisplay = function(bssid, callback) {
    doCommandWithParams(
      WIFI_CMD_WPS_PIN_DISPLAY,
      "wpsConfig",
      { bssid, pinCode: "" },
      callback
    );
  };

  command.wpsCancel = function(callback) {
    doCommand(WIFI_CMD_WPS_CANCEL, callback);
  };

  command.startSoftap = function(config, callback) {
    doCommandWithParams(
      WIFI_CMD_START_SOFTAP,
      "softapConfig",
      config,
      callback
    );
  };

  command.stopSoftap = function(callback) {
    doCommand(WIFI_CMD_STOP_SOFTAP, callback);
  };

  command.getSoftapInterface = function(callback) {
    doCommand(WIFI_CMD_GET_AP_IFACE, callback);
  };

  command.getSoftapStations = function(callback) {
    doCommand(WIFI_CMD_GET_SOFTAP_STATION_NUMBER, callback);
  };

  //----------------------------------------------------------
  // Private stuff.
  //----------------------------------------------------------

  function doCommand(cmd, callback) {
    aControlMessage({ cmd, iface: aInterface }, function(result) {
      callback(result);
    });
  }

  function doCommandWithParams(cmd, item, data, callback) {
    let msg = { cmd, iface: aInterface };
    if (item.trim().length) {
      msg[item] = data;
    }
    aControlMessage(msg, function(result) {
      callback(result);
    });
  }

  return command;
};
