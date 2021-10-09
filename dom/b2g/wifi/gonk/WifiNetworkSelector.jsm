/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WifiConfigManager } = ChromeUtils.import(
  "resource://gre/modules/WifiConfigManager.jsm"
);
const { WifiConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);
const { SavedNetworkSelector } = ChromeUtils.import(
  "resource://gre/modules/SavedNetworkSelector.jsm"
);
const { PasspointNetworkSelector } = ChromeUtils.import(
  "resource://gre/modules/PasspointNetworkSelector.jsm"
);

const EXPORTED_SYMBOLS = ["WifiNetworkSelector"];

var gDebug = false;

function BssidDenylistStatus() {}

BssidDenylistStatus.prototype = {
  // How many times it is requested to be in deny list.
  // (association rejection trigger this)
  counter: 0,
  isDenylisted: false,
  denylistedTimeStamp: WifiConstants.INVALID_TIME_STAMP,
};

const WifiNetworkSelector = (function() {
  var wifiNetworkSelector = {};

  // Minimum time gap between last successful Network Selection and
  // new selection attempt usable only when current state is connected state.
  const MINIMUM_NETWORK_SELECTION_INTERVAL = 10 * 1000;
  const MINIMUM_LAST_USER_SELECTION_INTERVAL = 30 * 1000;

  const BSSID_DENYLIST_THRESHOLD = 3;
  const BSSID_DENYLIST_EXPIRE_TIME = 30 * 60 * 1000;

  const REASON_AP_UNABLE_TO_HANDLE_NEW_STA = 17;

  var lastNetworkSelectionTimeStamp = WifiConstants.INVALID_TIME_STAMP;
  var enableAutoJoinWhenAssociated = true;
  var bssidDenylist = new Map();
  var roamingCandidate = null;

  var savedNetworkSelector = new SavedNetworkSelector();
  var passpointNetworkSelector = new PasspointNetworkSelector();

  // Network selector would go through each of the selectors.
  // Once a candidate is found, the iterator will stop.
  var networkSelectors = [savedNetworkSelector, passpointNetworkSelector];

  // WifiNetworkSelector members
  wifiNetworkSelector.skipNetworkSelection = false;
  wifiNetworkSelector.bssidDenylist = bssidDenylist;

  // WifiNetworkSelector functions
  wifiNetworkSelector.updateBssidDenylist = updateBssidDenylist;
  wifiNetworkSelector.selectNetwork = selectNetwork;
  wifiNetworkSelector.trackBssid = trackBssid;
  wifiNetworkSelector.setDebug = setDebug;

  function setDebug(aDebug) {
    gDebug = aDebug;

    if (savedNetworkSelector) {
      savedNetworkSelector.setDebug(aDebug);
    }
    if (passpointNetworkSelector) {
      passpointNetworkSelector.setDebug(aDebug);
    }
  }

  function debug(aMsg) {
    if (gDebug) {
      dump("-*- WifiNetworkSelector: " + aMsg);
    }
  }

  function selectNetwork(scanResults, wifiState, wifiInfo, callback) {
    roamingCandidate = null;
    debug("==========start Network Selection==========");

    if (scanResults.length == 0) {
      debug("Empty connectivity scan result");
      callback(null);
      return;
    }

    // Shall we start network selection at all?
    // In this function, we will select a suitalbe network for roaming if any.
    if (!isNetworkSelectionNeeded(scanResults, wifiState, wifiInfo)) {
      callback(null);
      return;
    }

    if (roamingCandidate != null) {
      debug("Found suitable roaming candidate");

      lastNetworkSelectionTimeStamp = Date.now();
      roamingCandidate.netId = wifiInfo.networkId;
      let config = savedNetworkSelector.convertScanResultToConfiguration(
        roamingCandidate
      );
      callback(config);
      return;
    }

    var candidate = null;
    var configuredNetworks = WifiConfigManager.configuredNetworks;

    updateSavedNetworkSelectionStatus(configuredNetworks);

    var filteredResults = filterScanResults(
      scanResults,
      wifiState,
      wifiInfo.bssid
    );

    if (filteredResults.length === 0) {
      callback(null);
      return;
    }

    const selectorCallback = element => {
      candidate = element.chooseNetwork(filteredResults, wifiInfo);

      // If candidate is found, just break the loop.
      return candidate != null;
    };

    // Iterate each selector in networkSelectors.
    networkSelectors.some(selectorCallback);

    if (candidate == null) {
      debug("Can not find any suitable candidates");
      callback(null);
      return;
    }

    lastNetworkSelectionTimeStamp = Date.now();
    callback(candidate);
  }

  function isNetworkSelectionNeeded(scanResults, wifiState, wifiInfo) {
    if (wifiNetworkSelector.skipNetworkSelection) {
      debug("skipNetworkSelection flag is TRUE.");
      return false;
    }

    if (wifiState == "connected" || wifiState == "associated") {
      if (!wifiInfo) {
        return false;
      }

      // Is roaming allowed?
      if (!enableAutoJoinWhenAssociated) {
        debug(
          "Switching networks in connected state is not allowed." +
            " Skip network selection."
        );
        return false;
      }

      // Has it been at least the minimum interval since last network selection?
      if (lastNetworkSelectionTimeStamp != WifiConstants.INVALID_TIME_STAMP) {
        var gap = Date.now() - lastNetworkSelectionTimeStamp;
        if (gap < MINIMUM_NETWORK_SELECTION_INTERVAL) {
          debug(
            "Too short since last network selection: " +
              gap +
              " ms." +
              " Skip network selection"
          );
          return false;
        }
      }

      if (isCurrentNetworkSufficient(scanResults, wifiInfo)) {
        debug("Current network already sufficient. Skip network selection.");
        return false;
      }
      debug("Current connected network is not sufficient.");
      return true;
    } else if (wifiState == "disconnected") {
      return true;
    }
    debug("Wifi is neither connected or disconnected. Skip network selection");
    return false;
  }

  // find any suitable network for roaming
  function findRoamingCadidate(scanResults, wifiInfo) {
    let currentRssi = wifiInfo.rssi;
    let minDiff = 0;
    roamingCandidate = null;

    if (currentRssi < -85) {
      // ..-86 dBm
      minDiff = 1;
    } else if (currentRssi < -80) {
      // -85..-81 dBm
      minDiff = 2;
    } else if (currentRssi < -75) {
      // -80..-76 dBm
      minDiff = 3;
    } else if (currentRssi < -70) {
      // -75..-71 dBm
      minDiff = 4;
    } else if (currentRssi < 0) {
      // -70..-1 dBm
      minDiff = 5;
    } else {
      // unspecified units (not in dBm)
      minDiff = 2;
    }
    debug(
      "wifiInfo: bssid=" +
        wifiInfo.bssid +
        "; rssi=" +
        wifiInfo.rssi +
        "; minDiff=" +
        minDiff
    );

    let filterdResult = scanResults.filter(
      result =>
        result.security === wifiInfo.security &&
        result.ssid === wifiInfo.wifiSsid &&
        result.bssid !== wifiInfo.bssid &&
        result.signalStrength - wifiInfo.rssi > minDiff
    );

    if (filterdResult.length > 0) {
      roamingCandidate = filterdResult[0];
      debug(
        "roamingCandidate: bssid=" +
          roamingCandidate.bssid +
          "; signalStrength=" +
          roamingCandidate.signalStrength
      );
      return true;
    }

    return false;
  }

  function isCurrentNetworkSufficient(scanResults, wifiInfo) {
    if (wifiInfo.networkId == WifiConstants.INVALID_NETWORK_ID) {
      debug("WifiWorker in connected state but WifiInfo is not");
      return false;
    }

    debug(
      "Current connected network: " +
        wifiInfo.wifiSsid +
        " ,ID is: " +
        wifiInfo.networkId
    );

    let network = WifiConfigManager.getNetworkConfiguration(wifiInfo.networkId);
    if (!network) {
      debug("Current network is removed");
      return false;
    }

    // current network is recently user-selected.
    let lastNetwork = WifiConfigManager.getLastSelectedNetwork();
    let lastTimeStamp = WifiConfigManager.getLastSelectedTimeStamp();
    if (
      lastNetwork == wifiInfo.networkId &&
      Date.now() - lastTimeStamp < MINIMUM_LAST_USER_SELECTION_INTERVAL
    ) {
      return true;
    }

    // TODO: OSU network for Passpoint Release 2 is sufficient network.
    // if (network.osu) {
    //   return true;
    // }

    if (findRoamingCadidate(scanResults, wifiInfo)) {
      return false;
    }

    // TODO: 1. 2.4GHz networks is not qualified whenever 5GHz is available.
    //       2. Tx/Rx Success rate shall be considered.
    let currentRssi = wifiInfo.rssi;
    let hasQualifiedRssi =
      (wifiInfo.is24G && currentRssi > WifiConstants.RSSI_THRESHOLD_LOW_24G) ||
      (wifiInfo.is5G && currentRssi > WifiConstants.RSSI_THRESHOLD_LOW_5G);

    if (!hasQualifiedRssi) {
      debug(
        "Current network RSSI[" +
          currentRssi +
          "]-acceptable but not qualified."
      );
      return false;
    }

    // Open network is not qualified.
    if (wifiInfo.security == "OPEN") {
      debug("Current network is a open one");
      return false;
    }

    return true;
  }

  function filterScanResults(scanResults, wifiState, currentBssid) {
    let filteredResults = [];
    let resultsContainCurrentBssid = false;

    for (let scanResult of scanResults) {
      // skip bad scan result
      if (!scanResult.ssid || scanResult.ssid === "") {
        debug("skip bad scan result");
        continue;
      }

      if (scanResult.bssid.includes(currentBssid)) {
        resultsContainCurrentBssid = true;
      }

      var scanId = scanResult.ssid + ":" + scanResult.bssid;
      debug("scanId = " + scanId);

      // check whether this BSSID is blocked or not
      let status = bssidDenylist.get(scanResult.bssid);
      if (typeof status !== "undefined" && status.isDenylisted) {
        debug(scanId + " is in deny list.");
        continue;
      }

      let isWeak24G =
        scanResult.is24G &&
        scanResult.signalStrength < WifiConstants.RSSI_THRESHOLD_BAD_24G;
      let isWeak5G =
        scanResult.is5G &&
        scanResult.signalStrength < WifiConstants.RSSI_THRESHOLD_BAD_5G;
      // skip scan result with too weak signals
      if (isWeak24G || isWeak5G) {
        debug(
          scanId +
            "(" +
            (scanResult.is24G ? "2.4GHz" : "5GHz") +
            ")" +
            scanResult.signalStrength +
            " / "
        );
        continue;
      }
      // save the result for ongoing network selection
      filteredResults.push(scanResult);
    }

    let isConnected = wifiState == "connected" || wifiState == "associated";
    // If wifi is connected but its bssid is not in scan list,
    // we should assume that the scan is triggered without
    // all channles included. So we just skip these results.
    if (isConnected && !resultsContainCurrentBssid) {
      return [];
    }

    return filteredResults;
  }

  function trackBssid(bssid, enable, reason) {
    debug("trackBssid: " + (enable ? "enable " : "disable ") + bssid);
    if (!bssid.length) {
      return false;
    }

    if (enable) {
      return bssidDenylist.delete(bssid);
    }

    let status = bssidDenylist.get(bssid);
    if (typeof status == "undefined") {
      // first time
      status = new BssidDenylistStatus();
      bssidDenylist.set(bssid, status);
    }
    status.counter++;
    status.denylistedTimeStamp = Date.now();
    if (!status.isDenylisted) {
      if (
        status.counter >= BSSID_DENYLIST_THRESHOLD ||
        reason == REASON_AP_UNABLE_TO_HANDLE_NEW_STA
      ) {
        status.isDenylisted = true;
        return true;
      }
    }
    return false;
  }

  function updateBssidDenylist(callback) {
    let iter = bssidDenylist[Symbol.iterator]();
    let updated = false;
    for (let [bssid, status] of iter) {
      debug(
        "BSSID deny list: BSSID=" +
          bssid +
          " isDenylisted=" +
          status.isDenylisted
      );
      if (
        status.isDenylisted &&
        Date.now() - status.denylistedTimeStamp >= BSSID_DENYLIST_EXPIRE_TIME
      ) {
        bssidDenylist.delete(bssid);
        updated = true;
      }
    }
    callback(updated);
  }

  function updateSavedNetworkSelectionStatus(configuredNetworks) {
    if (Object.keys(configuredNetworks).length == 0) {
      debug("no saved network");
      return;
    }
    WifiConfigManager.tryEnableQualifiedNetwork(configuredNetworks);
  }

  return wifiNetworkSelector;
})();
