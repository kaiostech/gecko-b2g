/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnqpCache } = ChromeUtils.import(
  "resource://gre/modules/AnqpUtils.jsm"
);
const { WifiConfigUtils } = ChromeUtils.import(
  "resource://gre/modules/WifiConfiguration.jsm"
);
const { WifiConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);
const { PasspointConfigManager } = ChromeUtils.import(
  "resource://gre/modules/PasspointConfigManager.jsm"
);
const { PasspointProvider, PasspointConfig } = ChromeUtils.import(
  "resource://gre/modules/PasspointConfiguration.jsm"
);

const EXPORTED_SYMBOLS = ["PasspointManager"];

var gDebug = false;

const PasspointManager = (function() {
  var passpointManager = {};
  var passpointEnabled = false;
  var mAnqpCache = new AnqpCache();
  var mWifiManager = null;

  passpointManager.passpointEnabled = passpointEnabled;
  passpointManager.setDebug = setDebug;
  passpointManager.setWifiManager = setWifiManager;
  passpointManager.isPasspointSupported = isPasspointSupported;
  passpointManager.isProviderEmpty = isProviderEmpty;
  passpointManager.onANQPResponse = onANQPResponse;
  passpointManager.sweepCache = sweepCache;
  passpointManager.matchProvider = matchProvider;
  passpointManager.hasCarrierProvider = hasCarrierProvider;
  passpointManager.getAnqpElements = getAnqpElements;
  passpointManager.setConfiguration = setConfiguration;
  passpointManager.getConfigurations = getConfigurations;
  passpointManager.removeConfiguration = removeConfiguration;
  passpointManager.loadFromStore = loadFromStore;

  function debug(aMsg) {
    if (gDebug) {
      dump("-*- PasspointManager: " + aMsg);
    }
  }

  function setDebug(aDebug) {
    gDebug = aDebug;

    if (PasspointConfigManager) {
      PasspointConfigManager.setDebug(aDebug);
    }
  }

  function setWifiManager(manager) {
    mWifiManager = manager;
  }

  function isPasspointSupported() {
    return Services.prefs.getBoolPref("dom.passpoint.supported", false);
  }

  function isProviderEmpty() {
    return !PasspointConfigManager.providers;
  }

  function onANQPResponse(anqpNetworkKey, response) {
    if (!anqpNetworkKey || !response) {
      // Failed to acquire result.
      return;
    }

    // Add new entry to the cache, value is a nsIAnqpResponse object.
    mAnqpCache.addEntry(anqpNetworkKey, response);

    if (gDebug) {
      mAnqpCache.getEntry(anqpNetworkKey).dumpAnqpElements();
    }
  }

  function sweepCache() {
    mAnqpCache.sweep();
  }

  /**
   * Find all providers that contain credential to authenticate with the given AP.
   *
   * @param scanResult
   *        The networks from scan result to be matched
   * @return
   *        Return a list of matched providers pair with status
   */
  function matchProvider(scanResult) {
    let bestMatch = null;
    let allMatches = getAllMatchedProviders(scanResult);

    if (allMatches.length == 0) {
      return null;
    }

    for (let match of allMatches) {
      if (match.matchStatus == WifiConstants.PasspointMatch.HomeProvider) {
        bestMatch = match;
        break;
      }
      if (
        match.matchStatus == WifiConstants.PasspointMatch.RoamingProvider &&
        bestMatch == null
      ) {
        bestMatch = match;
      }
    }

    if (bestMatch != null) {
      debug("Matched " + scanResult.ssid + " by " + JSON.stringify(bestMatch));
    } else {
      debug("No service provider found for " + scanResult.ssid);
    }
    return bestMatch;
  }

  /**
   * Get a list of providers that can provide service through the given AP.
   *
   * @param scanResult
   *        The networks from scan result to be matched
   * @return
   *        Return a list of matched providers pair with status
   */
  function getAllMatchedProviders(scanResult) {
    let allMatches = [];
    let anqpNetworkKey = WifiConfigUtils.getAnqpNetworkKey(scanResult);
    let anqpData = mAnqpCache.getEntry(anqpNetworkKey);
    if (!anqpData) {
      let request = {
        anqpKey: anqpNetworkKey,
        bssid: scanResult.bssid,
        roamingConsortiumOIs: scanResult.passpoint.anqpOICount > 0,
        supportRelease2:
          scanResult.passpoint.anqpDomainID > WifiConstants.PasspointVersion.R1,
      };

      if (mWifiManager) {
        mWifiManager.requestAnqp(request, success => {
          debug(
            "getAllMatchedProviders, request ANQP " +
              (success ? "success" : "failed")
          );
        });
      }
      return allMatches;
    }

    let providers = PasspointConfigManager.providers;
    for (let key in providers) {
      let provider = providers[key];
      let matchStatus = provider.match(
        anqpData.getElements(),
        scanResult.passpoint.roamingConsortiums
      );

      if (
        matchStatus == WifiConstants.PasspointMatch.HomeProvider ||
        matchStatus == WifiConstants.PasspointMatch.RoamingProvider
      ) {
        allMatches.push({ provider, matchStatus });
      }
    }
    return allMatches;
  }

  function hasCarrierProvider(mccMnc, mccMncRealm) {
    if (mccMncRealm == null) {
      return false;
    }

    let providers = PasspointConfigManager.providers;
    for (let key in providers) {
      let provider = providers[key];
      if (!provider.passpointConfig.credential) {
        continue;
      }
      if (mccMncRealm == key) {
        // We already got same FQDN in providers.
        return true;
      }

      let imsiMccMnc = provider.imsi;
      if (!imsiMccMnc) {
        continue;
      }

      imsiMccMnc.replace("*", "");
      if (mccMnc == imsiMccMnc) {
        // We already got same IMSI in providers.
        return true;
      }
    }
    return false;
  }

  function getAnqpElements(scanResult) {
    let anqpNetworkKey = WifiConfigUtils.getAnqpNetworkKey(scanResult);
    let anqpData = mAnqpCache.getEntry(anqpNetworkKey);

    return anqpData ? anqpData.getElements() : null;
  }

  function setConfiguration(passpointConfig, callback) {
    let config = new PasspointConfig(passpointConfig);
    if (!config.homeSp.fqdn) {
      debug("Invalid passpoint configuration");
      callback(false);
      return;
    }

    let provider = new PasspointProvider(config);
    PasspointConfigManager.addOrUpdateProvider(provider, callback);
  }

  function getConfigurations() {
    let configs = [];
    let providers = PasspointConfigManager.providers;

    for (let key in providers) {
      let provider = providers[key];
      if (provider.passpointConfig) {
        configs.push(new PasspointConfig(provider.passpointConfig));
      }
    }
    return configs;
  }

  function removeConfiguration(fqdn, callback) {
    PasspointConfigManager.removeProvider(fqdn, callback);
  }

  function loadFromStore() {
    PasspointConfigManager.loadFromStore();
  }

  return passpointManager;
})();
