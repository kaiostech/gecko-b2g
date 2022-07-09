/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AnqpCache", "AnqpData", "AnqpMatcher"];

const CACHE_SWEEP_INTERVAL_MILLISECONDS = 60 * 1000;
const DATA_LIFETIME_MILLISECONDS = 60 * 60 * 1000;

var gDebug = true;

function debug(aMsg) {
  if (gDebug) {
    dump("-*- ANQP: " + aMsg);
  }
}

const AnqpData = function(anqpElements) {
  if (anqpElements) {
    this._anqpElements = anqpElements;
  }
  this._expiredTime = Date.now() + DATA_LIFETIME_MILLISECONDS;
};

AnqpData.prototype = {
  _anqpElements: null,

  _expiredTime: 0,

  getElements() {
    return this._anqpElements;
  },

  isExpired(time) {
    return this._expiredTime <= time;
  },

  dumpAnqpElements() {
    let elements = this._anqpElements;
    debug(
      "bssid: " +
        elements.bssid +
        ", IP availability" +
        JSON.stringify(elements.ipAvailability) +
        ", WAN Metrics" +
        JSON.stringify(elements.hsWanMetrics)
    );

    debug("Venue name: " + JSON.stringify(elements.getVenueName()));
    debug(
      "Roaming consortium OIs: " +
        JSON.stringify(elements.getRoamingConsortiumOIs())
    );

    for (let nai of elements.getNaiRealmList()) {
      debug("Realms: " + JSON.stringify(nai.getRealms()));
      for (let eapMethod of nai.getEapMethods()) {
        debug("EAP method ID: " + eapMethod.eapMethodId);
        let authParams = eapMethod.authParams;
        if (authParams) {
          debug(
            "Auth ID: " +
              JSON.stringify(authParams.getAuthTypeId()) +
              ", Expanded EAP method: " +
              JSON.stringify(authParams.getExpandedEapMethod()) +
              ", Non EAP Inner auth: " +
              JSON.stringify(authParams.getNonEapInnerAuth()) +
              ", Inner Auth: " +
              JSON.stringify(authParams.getInnerAuth()) +
              ", Expanded Inner EAP method: " +
              JSON.stringify(authParams.getExpandedInnerEapMethod()) +
              ", Credential: " +
              JSON.stringify(authParams.getCredential()) +
              ", Tunneled Credential: " +
              JSON.stringify(authParams.getTunneledCredential()) +
              ", Vendor Specific Auth: " +
              JSON.stringify(authParams.getVendorSpecificAuth())
          );
        }
      }
    }

    for (let item of elements.getCellularNetwork()) {
      debug("Plmns: " + JSON.stringify(item.getPlmnList()));
    }
    debug("Domain: " + JSON.stringify(elements.getDomainName()));
    debug(
      "ConnectionCaps: " + JSON.stringify(elements.getConnectionCapability())
    );

    let osuProviders = elements.hsOsuProviders;
    if (osuProviders) {
      debug("OSU ssid: " + osuProviders.osuSsid);
      for (let provider of osuProviders.getProviders()) {
        debug("Provider server URI: " + provider.serverUri);
        debug("Provider NAI: " + provider.networkAccessIdentifier);
      }
    }
  },
};

const AnqpCache = function() {
  this._anqpCaches = new Map();
  this._lastSweep = Date.now();
};

AnqpCache.prototype = {
  _anqpCaches: null,

  _lastSweep: 0,

  addEntry(anqpNetworkKey, anqpElements) {
    let data = new AnqpData(anqpElements);
    this._anqpCaches.set(anqpNetworkKey, data);
  },

  getEntry(anqpNetworkKey) {
    return this._anqpCaches.get(anqpNetworkKey);
  },

  sweep() {
    let now = Date.now();

    // No need perform sweep if it's not reach threshold.
    if (now < this._lastSweep + CACHE_SWEEP_INTERVAL_MILLISECONDS) {
      return;
    }

    // Collect all expired keys.
    let expiredKeys = [];
    this._anqpCaches.forEach(function(anqpData, anqpNetworkKey, map) {
      if (anqpData.isExpired(now)) {
        expiredKeys.push(anqpNetworkKey);
      }
    });

    // Remove expired caches.
    for (let key of expiredKeys) {
      this._anqpCaches.delete(key);
    }
    this._lastSweep = now;
  },
};

const AnqpMatcher = (function() {
  var anqpMatcher = {};

  const PLMNText = ["org", "3gppnetwork", "mcc*", "mnc*", "wlan"];

  anqpMatcher.matchDomainName = matchDomainName;
  anqpMatcher.matchRoamingConsortium = matchRoamingConsortium;
  anqpMatcher.matchNAIRealm = matchNAIRealm;
  anqpMatcher.matchThreeGPPNetwork = matchThreeGPPNetwork;
  anqpMatcher.matchMccMnc = matchMccMnc;

  function getMccMnc(domain) {
    if (domain.length != PLMNText.length) {
      return null;
    }

    // Check if domain has format as PLMNText.
    for (let i in PLMNText) {
      let text = PLMNText[i];
      text.replace("*", "");
      if (domain[i] != text) {
        return null;
      }
    }

    let prefix = domain[2].substring(3) + domain[3].substring(3);
    for (let i in prefix) {
      if (prefix.charAt(i) < "0" || prefix.charAt(i) > "9") {
        return null;
      }
    }
    return prefix;
  }

  function splitDomain(domain) {
    if (domain.endsWith(".")) {
      domain = domain.substring(0, domain.length - 1);
    }

    let at = domain.indexOf("@");
    if (at !== -1) {
      domain = domain.substring(at + 1);
    }
    return domain.toLowerCase().split(".");
  }

  /**
   * Check if second domain is a sub-domain of first one.
   *
   * @param domain
   *        The string of the first domain
   * @param subDomain
   *        The string of the second domain
   * @return
   *        True if the second domain is the sub-domain of the first
   */
  function isSubDomain(domain, subDomain) {
    if (!domain || !subDomain) {
      return false;
    }

    let domainList = splitDomain(domain);
    let subDomainList = splitDomain(subDomain);
    if (domainList.length < subDomainList.length) {
      return false;
    }

    for (let i in domainList) {
      if (domainList[i] != subDomainList[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Match domain name in ANQP element against provider FQDN.
   *
   * @param domainListElement
   *        The domain name in ANQP element
   * @param fqdn
   *        The FQDN from provider
   * @param imsi
   *        The IMSI from provider
   * @param simImsi
   *        The IMSI from inserted SIM that matched provider IMSI
   * @return
   *        True if domain name matched
   */
  function matchDomainName(domainListElement, fqdn, imsi, simImsi) {
    if (!domainListElement) {
      return false;
    }

    for (let domain of domainListElement) {
      if (isSubDomain(fqdn, domain)) {
        return true;
      }

      if (!imsi || !simImsi) {
        continue;
      }

      if (matchMccMnc(getMccMnc(splitDomain(domain)), imsi, simImsi)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Match the roaming consortium OIs in ANQP element against provider OIs.
   *
   * @param roamingConsortiumElement
   *        The roaming consortium OIs from ANQP element
   * @param providerOIs
   *        The roaming consortium OIs from provider
   * @param matchAll
   *        A boolean to indicates if all OIs must be matched
   * @return
   *        True if roaming consortium OIs matched
   */
  function matchRoamingConsortium(
    roamingConsortiumElement,
    providerOIs,
    matchAll
  ) {
    if (!roamingConsortiumElement || !providerOIs) {
      return false;
    }

    for (let oi of providerOIs) {
      if (roamingConsortiumElement.includes(oi)) {
        if (!matchAll) {
          return true;
        }
      } else if (matchAll) {
        return false;
      }
    }
    return matchAll;
  }

  /**
   * Match the NAI realm in the ANQP element against the realm from provider.
   *
   * @param naiRealmElement
   *        The NAI Realm from ANQP element
   * @param realm
   *        The realm from provider credential
   * @return
   *        True if NAI realm matched
   */
  function matchNAIRealm(naiRealmElement, realm) {
    if (!naiRealmElement || !naiRealmElement.length) {
      return false;
    }

    for (let realmData of naiRealmElement) {
      for (let realmStr of realmData.getRealms()) {
        if (isSubDomain(realm, realmStr)) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Match the 3GPP Network in the ANQP element against the SIM credential from provider.
   *
   * @param threeGPPNetworkElement
   *        The 3GPP network from ANQP element
   * @param imsi
   *        The IMSI from provider
   * @param simImsi
   *        The IMSI from inserted SIM that matched provider IMSI
   * @return
   *        True if 3GPP network matched
   */
  function matchThreeGPPNetwork(threeGPPNetworkElement, imsi, simImsi) {
    if (!threeGPPNetworkElement) {
      return false;
    }

    for (let network of threeGPPNetworkElement) {
      let plmns = network.getPlmnList();
      for (let plmn of plmns) {
        if (matchMccMnc(plmn, imsi, simImsi)) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Match a MCCMNC against the SIM credential from provider.
   *
   * @param mccMnc
   *        A string contains MCCMNC
   * @param imsiParam
   *        The IMSI from a provider
   * @param simImsi
   *        The IMSI from inserted SIM that matched provider IMSI
   * @return
   *        True if MCCMNC prefix matched
   */
  function matchMccMnc(mccMnc, imsiParam, simImsi) {
    if (!mccMnc || !imsiParam || !simImsi) {
      return false;
    }

    imsiParam.replace("*", "");
    if (mccMnc != imsiParam) {
      return false;
    }

    return simImsi.startsWith(mccMnc);
  }

  return anqpMatcher;
})();
