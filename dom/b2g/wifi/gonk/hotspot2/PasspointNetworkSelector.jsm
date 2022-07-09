/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { WifiConfigManager } = ChromeUtils.import(
  "resource://gre/modules/WifiConfigManager.jsm"
);
const { WifiConstants, EAPConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);
const { PasspointManager } = ChromeUtils.import(
  "resource://gre/modules/PasspointManager.jsm"
);
const { WifiConfigUtils } = ChromeUtils.import(
  "resource://gre/modules/WifiConfiguration.jsm"
);

const HOME_PROVIDER_AWARD = 100;
const INTERNET_ACCESS_AWARD = 50;
const PUBLIC_OR_PRIVATE_NETWORK_AWARDS = 4;
const PERSONAL_OR_EMERGENCY_NETWORK_AWARDS = 2;
const RESTRICTED_OR_UNKNOWN_IP_AWARDS = 1;
const UNRESTRICTED_IP_AWARDS = 2;
const WAN_PORT_DOWN_OR_CAPPED_PENALTY = 50;
const RSSI_SCORE_START = -80;
const RSSI_BUCKETWIDTH = 20;
const RSSI_LEVEL_BUCKETS = [-10, 0, 10, 20, 30, 40];
const ACTIVE_NETWORK_RSSI_BOOST = 20;
const NETWORK_TYPE_SCORES = [
  PUBLIC_OR_PRIVATE_NETWORK_AWARDS, // For Ant.Private
  PUBLIC_OR_PRIVATE_NETWORK_AWARDS, // For Ant.PrivateWithGuest
  PUBLIC_OR_PRIVATE_NETWORK_AWARDS, // For Ant.ChargeablePublic
  PUBLIC_OR_PRIVATE_NETWORK_AWARDS, // For Ant.FreePublic
  PERSONAL_OR_EMERGENCY_NETWORK_AWARDS, // For Ant.Personal
  PERSONAL_OR_EMERGENCY_NETWORK_AWARDS, // For Ant.EmergencyOnly
  0, // For Ant.Resvd6
  0, // For Ant.Resvd7
  0, // For Ant.Resvd8
  0, // For Ant.Resvd9
  0, // For Ant.Resvd10
  0, // For Ant.Resvd11
  0, // For Ant.Resvd12
  0, // For Ant.Resvd13
  0, // For Ant.TestOrExperimental
  0, // For Ant.Wildcard
];
const IPV4_SCORES = [
  0, // IPV4_NOT_AVAILABLE
  UNRESTRICTED_IP_AWARDS, // IPV4_PUBLIC
  RESTRICTED_OR_UNKNOWN_IP_AWARDS, // IPV4_PORT_RESTRICTED
  UNRESTRICTED_IP_AWARDS, // IPV4_SINGLE_NAT
  UNRESTRICTED_IP_AWARDS, // IPV4_DOUBLE_NAT
  RESTRICTED_OR_UNKNOWN_IP_AWARDS, // IPV4_PORT_RESTRICTED_AND_SINGLE_NAT
  RESTRICTED_OR_UNKNOWN_IP_AWARDS, // IPV4_PORT_RESTRICTED_AND_DOUBLE_NAT
  RESTRICTED_OR_UNKNOWN_IP_AWARDS, // IPV4_UNKNOWN
];
const IPV6_SCORES = [
  0, // IPV6_NOT_AVAILABLE
  UNRESTRICTED_IP_AWARDS, // IPV6_AVAILABLE
  RESTRICTED_OR_UNKNOWN_IP_AWARDS, // IPV6_UNKNOWN
];

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDataCallManager",
  "@mozilla.org/datacall/manager;1",
  "nsIDataCallManager"
);

const EXPORTED_SYMBOLS = ["PasspointNetworkSelector"];

var gDebug = false;

function debug(aMsg) {
  if (gDebug) {
    dump("-*- PasspointNetworkSelector: " + aMsg);
  }
}

let PasspointNetworkSelector = function PasspointNetworkSelector() {};

PasspointNetworkSelector.prototype = {
  lastUserSelectedNetwork: WifiConfigManager.getLastSelectedNetwork(),
  lastUserSelectedNetworkTimeStamp: WifiConfigManager.getLastSelectedTimeStamp(),

  setDebug(aDebug) {
    gDebug = aDebug;
  },

  chooseNetwork(results, wifiInfo) {
    var bestCandidate = null;
    var candidateList = [];
    var highestScore = Number.MIN_VALUE;

    if (!PasspointManager.isPasspointSupported()) {
      debug("passpoint not supported");
      return null;
    }

    if (!PasspointManager.passpointEnabled) {
      debug("passpoint is disabled");
      return null;
    }

    if (PasspointManager.isProviderEmpty()) {
      debug("no saved providers");
      return null;
    }

    debug("chooseNetwork ...");
    // Renew the ANQP cache.
    PasspointManager.sweepCache();

    // Filter scan results by interworking.
    let passpointScanResults = this.filterPasspointResult(results);

    // Go through each Passpoint scan result for best provider.
    for (let result of passpointScanResults) {
      let bestProvider = PasspointManager.matchProvider(result);

      if (bestProvider != null) {
        let simSlot = lazy.gDataCallManager.dataDefaultServiceId;
        let icc = lazy.gIccService.getIccByServiceId(simSlot);

        if (!icc || icc.cardState != Ci.nsIIcc.CARD_STATE_READY) {
          debug("Ignore provider since sim not ready");
          continue;
        }

        candidateList.push({
          scanResult: result,
          provider: bestProvider.provider,
          matchStatus: bestProvider.matchStatus,
        });
      }
    }

    if (candidateList.length == 0) {
      debug("No suitable Passpoint network found");
      return null;
    }

    for (let candidate of candidateList) {
      let isActiveNetwork = candidate.scanResult.ssid == wifiInfo.ssid;
      let scanResult = candidate.scanResult;
      let score = this.calculateScore(
        candidate.matchStatus == WifiConstants.PasspointMatch.HomeProvider,
        PasspointManager.getAnqpElements(scanResult),
        scanResult,
        isActiveNetwork
      );

      if (score > highestScore) {
        bestCandidate = candidate;
        highestScore = score;
      }
    }

    return this.convertProviderToConfiguration(bestCandidate);
  },

  filterPasspointResult(scanResults) {
    let filteredScanResults = [];
    for (let result of scanResults) {
      // We only care about passpoint AP.
      if (result.passpoint.isInterworking) {
        filteredScanResults.push(result);
      }
    }
    return filteredScanResults;
  },

  calculateScore(isHomeProvider, anqpElements, scanResult, isActiveNetwork) {
    let score = 0;

    if (!anqpElements || !scanResult) {
      return score;
    }

    // Evaluate match status score.
    if (isHomeProvider) {
      score += HOME_PROVIDER_AWARD;
    }

    score += (scanResult.hasInternet ? 1 : -1) * INTERNET_ACCESS_AWARD;
    score += NETWORK_TYPE_SCORES[scanResult.passpoint.ant];

    if (anqpElements) {
      let wm = anqpElements.hsWanMetrics;
      if (
        wm &&
        (wm.status != WifiConstants.HSWanMetrics.LINK_STATUS_UP || wm.capped)
      ) {
        score -= WAN_PORT_DOWN_OR_CAPPED_PENALTY;
      }

      let ipa = anqpElements.ipAvailability;
      if (ipa) {
        score +=
          ipa.ipv4Availability != null ? IPV4_SCORES[ipa.ipv4Availability] : 0;
        score +=
          ipa.ipv6Availability != null ? IPV6_SCORES[ipa.ipv6Availability] : 0;
      }
    }

    // Evaluate rssi score.
    let rssi = scanResult.signalStrength;
    if (isActiveNetwork) {
      rssi += ACTIVE_NETWORK_RSSI_BOOST;
    }

    let index = Math.floor((rssi - RSSI_SCORE_START) / RSSI_BUCKETWIDTH);

    if (index < 0) {
      index = 0;
    } else if (index > RSSI_LEVEL_BUCKETS.length - 1) {
      index = RSSI_LEVEL_BUCKETS.length - 1;
    }
    score += RSSI_LEVEL_BUCKETS[index];

    debug("Network " + scanResult.ssid + " get score: " + score);
    return score;
  },

  convertProviderToConfiguration(candidate) {
    let config = {};
    config.keyMgmt = "WPA-EAP IEEE8021X";
    config.proto = "RSN";

    let passpointConfig = candidate.provider.passpointConfig;
    config.realm = passpointConfig.credential.realm;
    config.domainSuffixMatch = passpointConfig.homeSp.fqdn;

    // TODO: Should also support TTLS/TLS for passpoint.
    {
      let methods = new Map([
        [EAPConstants.EAP_SIM, "SIM"],
        [EAPConstants.EAP_AKA, "AKA"],
        [EAPConstants.EAP_AKA_PRIME, "AKA'"],
      ]);

      if (methods.has(passpointConfig.credential.eapType)) {
        config.eap = methods.get(passpointConfig.credential.eapType);
      } else {
        debug("Unknown EAP method: " + passpointConfig.credential.eapType);
      }
      config.plmn = passpointConfig.credential.imsi;
    }

    function quote(s) {
      return '"' + s + '"';
    }

    let scanResult = candidate.scanResult;
    config.ssid = quote(scanResult.ssid);
    config.isHomeProvider =
      candidate.matchStatus == WifiConstants.PasspointMatch.HomeProvider;

    let networkKey = WifiConfigUtils.getNetworkKey(config);
    let configured = WifiConfigManager.configuredNetworks;
    if (networkKey in configured) {
      WifiConfigManager.tryEnableQualifiedNetwork(configured);
      debug("Passpoint network " + config.ssid + " is already configured");
      return null;
    }

    // Save passpoint to wifi config store.
    WifiConfigManager.addOrUpdateNetwork(config, function(success) {
      if (!success) {
        debug("Failed to add passpoint network");
      }
    });
    return configured[networkKey];
  },
};
