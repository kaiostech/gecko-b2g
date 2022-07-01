/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WifiConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);

const EXPORTED_SYMBOLS = ["WifiScanSettings", "WifiPnoSettings"];

/**
 * Describes WiFi scan settings.
 */
const WifiScanSettings = (function() {
  var wifiScanSettings = {};

  const SCAN_TYPE_LOW_SPAN = Ci.nsIScanSettings.SCAN_TYPE_LOW_SPAN;
  const SCAN_TYPE_LOW_POWER = Ci.nsIScanSettings.SCAN_TYPE_LOW_POWER;
  const SCAN_TYPE_HIGH_ACCURACY = Ci.nsIScanSettings.SCAN_TYPE_HIGH_ACCURACY;
  const BAND_2_4_GHZ = Ci.nsIScanSettings.BAND_2_4_GHZ;
  const BAND_5_GHZ = Ci.nsIScanSettings.BAND_5_GHZ;
  const BAND_5_GHZ_DFS = Ci.nsIScanSettings.BAND_5_GHZ_DFS;

  var bandMask = 0;
  var scanType = 0;
  var channels = [];
  var hiddenNetworks = [];

  var singleScanSettings = Object.create(null);

  wifiScanSettings.SCAN_TYPE_LOW_SPAN = SCAN_TYPE_LOW_SPAN;
  wifiScanSettings.SCAN_TYPE_LOW_POWER = SCAN_TYPE_LOW_POWER;
  wifiScanSettings.SCAN_TYPE_HIGH_ACCURACY = SCAN_TYPE_HIGH_ACCURACY;
  wifiScanSettings.BAND_2_4_GHZ = BAND_2_4_GHZ;
  wifiScanSettings.BAND_5_GHZ = BAND_5_GHZ;
  wifiScanSettings.BAND_5_GHZ_DFS = BAND_5_GHZ_DFS;

  wifiScanSettings.singleScanSettings = singleScanSettings;
  wifiScanSettings.bandMask = bandMask;
  wifiScanSettings.scanType = scanType;
  wifiScanSettings.channels = channels;
  wifiScanSettings.hiddenNetworks = hiddenNetworks;

  return wifiScanSettings;
})();

/**
 * Describes WiFi pno scan settings.
 */
const WifiPnoSettings = (function() {
  var wifiPnoSettings = {};

  const DEFAULT_PNO_INTERVAL_MS = 20 * 1000;

  var interval = DEFAULT_PNO_INTERVAL_MS;
  var min2gRssi = WifiConstants.RSSI_THRESHOLD_LOW_24G;
  var min5gRssi = WifiConstants.RSSI_THRESHOLD_LOW_5G;
  var pnoNetworks = [];

  var pnoSettings = Object.create(null);

  wifiPnoSettings.pnoSettings = pnoSettings;
  wifiPnoSettings.interval = interval;
  wifiPnoSettings.min2gRssi = min2gRssi;
  wifiPnoSettings.min5gRssi = min5gRssi;
  wifiPnoSettings.pnoNetworks = pnoNetworks;

  return wifiPnoSettings;
})();
