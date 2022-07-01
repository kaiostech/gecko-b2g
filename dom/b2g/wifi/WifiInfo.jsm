/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WifiConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);

const EXPORTED_SYMBOLS = ["WifiInfo"];

/**
 * Describes the state of any Wifi connection that is active or
 * is in the process of being set up.
 */
const WifiInfo = function WifiInfo() {};
WifiInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWifiInfo]),
  supplicantState: "UNINITIALIZED",
  bssid: null,
  wifiSsid: null,
  security: null,
  networkId: WifiConstants.INVALID_NETWORK_ID,
  rssi: WifiConstants.INVALID_RSSI,
  linkSpeed: null,
  frequency: -1,
  ipAddress: -1,
  is24G: false,
  is5G: false,

  setSupplicantState(state) {
    this.supplicantState = state;
  },

  setInetAddress(address) {
    this.ipAddress = address;
  },

  setBSSID(bssid) {
    this.bssid = bssid;
  },

  setSSID(ssid) {
    this.wifiSsid = ssid;
  },

  setSecurity(security) {
    this.security = security;
  },

  setNetworkId(netId) {
    this.networkId = netId;
  },

  setRssi(rssi) {
    this.rssi = rssi;
  },

  setLinkSpeed(linkSpeed) {
    this.linkSpeed = linkSpeed;
  },

  setFrequency(frequency) {
    this.frequency = frequency;
    this.is24G = this.is24GHz();
    this.is5G = this.is5GHz();
  },

  reset() {
    this.supplicantState = "UNINITIALIZED";
    this.bssid = null;
    this.wifiSsid = null;
    this.security = null;
    this.networkId = WifiConstants.INVALID_NETWORK_ID;
    this.rssi = WifiConstants.INVALID_RSSI;
    this.linkSpeed = -1;
    this.frequency = -1;
    this.ipAddress = null;
  },

  is24GHz() {
    return this.frequency > 2400 && this.frequency < 2500;
  },

  is5GHz() {
    return this.frequency > 4900 && this.frequency < 5900;
  },
};
