/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { WifiConstants, EAPConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);
const { AnqpMatcher } = ChromeUtils.import(
  "resource://gre/modules/AnqpUtils.jsm"
);

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

const EXPORTED_SYMBOLS = [
  "PasspointProvider",
  "PasspointConfig",
  "HomeSp",
  "Credential",
];

var gDebug = true;

function debug(aMsg) {
  if (gDebug) {
    dump("-*- PasspointConfiguration: " + aMsg);
  }
}
// HomeSp
const HomeSp = function(homeSp) {
  if (homeSp) {
    this.fqdn = homeSp.fqdn;
    this.friendlyName = homeSp.friendlyName;
    this.matchAllOis = homeSp.matchAllOis;
    this.matchAnyOis = homeSp.matchAnyOis;
    this.otherHomePartners = homeSp.otherHomePartners;
    this.roamingConsortiumOis = homeSp.roamingConsortiumOis;
  }
};

HomeSp.prototype = {
  fqdn: null,

  friendlyName: null,

  matchAllOis: [],

  matchAnyOis: [],

  otherHomePartners: [],

  roamingConsortiumOis: [],
};

// Credential
const Credential = function(credential) {
  if (credential) {
    this.realm = credential.realm;
    this.imsi = credential.imsi;
    this.eapType = credential.eapType;
  }
};

Credential.prototype = {
  realm: null,

  //TODO: Only implement SIM credential for Passpoint R1.
  imsi: null,

  eapType: EAPConstants.INVALID_EAP,
};

// PasspointConfig
const PasspointConfig = function(config) {
  if (config) {
    this.homeSp = new HomeSp(config.homeSp);
    this.credential = new Credential(config.credential);
  }
};

PasspointConfig.prototype = {
  homeSp: null,

  credential: null,
};

// PasspointProvider
const PasspointProvider = function(passpointConfig) {
  this.passpointConfig = new PasspointConfig(passpointConfig);
  if (this.passpointConfig.credential) {
    this.imsi = this.passpointConfig.credential.imsi;
    this.eapMethod = this.passpointConfig.credential.eapType;
  }
};

PasspointProvider.prototype = {
  passpointConfig: null,

  imsi: null,

  authParam: null,

  eapMethod: EAPConstants.INVALID_EAP,

  getMatchingSimImsi(imsi) {
    let simSlot = lazy.gDataCallManager.dataDefaultServiceId;
    let icc = lazy.gIccService.getIccByServiceId(simSlot);

    if (!icc || !icc.iccInfo || !icc.iccInfo.imsi) {
      debug("Invalid icc info");
      return null;
    }

    let imsiPrefix = imsi.replace("*", "");
    if (icc.iccInfo.imsi.startsWith(imsiPrefix)) {
      return icc.iccInfo.imsi;
    }
    return null;
  },

  /**
   * Match provider OIs against Roaming Consortiums OIs.
   *
   * @param providerOis
   *        Roaming Consortiums OIs from provider
   * @param roamingConsortiumElement
   *        Roaming Consortiums OIs from ANQP element
   * @param roamingConsortium
   *        Roaming Consortiums OIs from scan result
   * @param matchAll
   *        A boolean to indicate if all providerOis must match the RCOIs elements
   * @return
   *        Return true if matched, otherwise return false
   */
  matchOis(providerOis, roamingConsortiumElement, roamingConsortium, matchAll) {
    // ANQP Roaming Consortium OI matching.
    if (
      AnqpMatcher.matchRoamingConsortium(
        roamingConsortiumElement,
        providerOis,
        matchAll
      )
    ) {
      debug("ANQP RCOI match" + JSON.stringify(roamingConsortiumElement));
      return true;
    }

    // AP Roaming Consortium OI matching.
    if (!roamingConsortium || !providerOis) {
      return false;
    }

    // Roaming Consortium OI information element matching.
    for (let apOi of roamingConsortium) {
      let matched = false;
      for (let providerOi of providerOis) {
        if (apOi == providerOi) {
          debug("AP RCOI match: " + apOi);
          if (!matchAll) {
            return true;
          }
          matched = true;
          break;
        }
      }
      if (matchAll && !matched) {
        return false;
      }
    }
    return matchAll;
  },

  /**
   * Match provider FQDN and Roaming Consortiums OIs against ANQP elements.
   *
   * @param anqpElements
   *        The ANQP element from access point
   * @param roamingConsortiums
   *        Roaming consortium information from scan result
   * @param matchingSimImsi
   *        The IMSI from inserted SIM that matched provider IMSI
   * @return
   *        Return the status in WifiConstants.PasspointMatch
   */
  matchFqdnAndRcoi(anqpElements, roamingConsortiums, matchingSimImsi) {
    // Domain name matching.
    if (
      AnqpMatcher.matchDomainName(
        anqpElements.getDomainName(),
        this.passpointConfig.homeSp.fqdn,
        this.imsi,
        matchingSimImsi
      )
    ) {
      debug(
        "Domain name " +
          this.passpointConfig.homeSp.fqdn +
          " match: HomeProvider"
      );
      return WifiConstants.PasspointMatch.HomeProvider;
    }

    // Other Home Partners matching.
    if (this.passpointConfig.homeSp.otherHomePartners) {
      for (let otherHomePartner of this.passpointConfig.homeSp
        .otherHomePartners) {
        if (
          AnqpMatcher.matchDomainName(
            anqpElements.getDomainName(),
            otherHomePartner,
            null,
            null
          )
        ) {
          debug(
            "Other Home Partner " + otherHomePartner + " match: HomeProvider"
          );
          return WifiConstants.PasspointMatch.HomeProvider;
        }
      }
    }

    // HomeOI matching.
    if (this.passpointConfig.homeSp.matchAllOis) {
      if (
        this.matchOis(
          this.passpointConfig.homeSp.matchAllOis,
          anqpElements.getRoamingConsortiumOIs(),
          roamingConsortiums,
          true
        )
      ) {
        debug("All HomeOI RCOI match: HomeProvider");
        return WifiConstants.PasspointMatch.HomeProvider;
      }
    } else if (this.passpointConfig.homeSp.matchAnyOis) {
      if (
        this.matchOis(
          this.passpointConfig.homeSp.matchAllOis,
          anqpElements.getRoamingConsortiumOIs(),
          roamingConsortiums,
          false
        )
      ) {
        debug("Any HomeOI RCOI match: HomeProvider");
        return WifiConstants.PasspointMatch.HomeProvider;
      }
    }

    // Roaming Consortium OI matching.
    if (
      this.matchOis(
        this.passpointConfig.homeSp.roamingConsortiumOis,
        anqpElements.getRoamingConsortiumOIs(),
        roamingConsortiums,
        false
      )
    ) {
      debug("ANQP RCOI match: RoamingProvider");
      return WifiConstants.PasspointMatch.RoamingProvider;
    }

    debug("No domain name or RCOI match");
    return WifiConstants.PasspointMatch.None;
  },

  /**
   * Match this provider with ANQP element from AP.
   *
   * @param anqpElements
   *        The ANQP element from access point
   * @param roamingConsortiums
   *        Roaming consortium information from scan result
   * @return
   *        Return the status in WifiConstants.PasspointMatch
   */
  match(anqpElements, roamingConsortiums) {
    let matchingSimImsi = null;
    if (this.passpointConfig.credential) {
      matchingSimImsi = this.getMatchingSimImsi(this.imsi);
      if (!matchingSimImsi) {
        debug(
          "No SIM card with IMSI " +
            this.passpointConfig.credential.imsi +
            "is installed"
        );
        return WifiConstants.PasspointMatch.None;
      }
    }

    // Match FQDN for Home provider or RCOI(s) for Roaming provider
    let providerMatch = this.matchFqdnAndRcoi(
      anqpElements,
      roamingConsortiums,
      matchingSimImsi
    );

    // 3GPP Network matching.
    if (
      providerMatch == WifiConstants.PasspointMatch.None &&
      AnqpMatcher.matchThreeGPPNetwork(
        anqpElements.getCellularNetwork(),
        this.imsi,
        matchingSimImsi
      )
    ) {
      debug("Final RoamingProvider match with 3GPP network");
      return WifiConstants.PasspointMatch.RoamingProvider;
    }

    // Perform NAI Realm matching.
    let realmMatch = AnqpMatcher.matchNAIRealm(
      anqpElements.getNaiRealmList(),
      this.passpointConfig.credential.realm
    );

    // In case of Auth mismatch, demote provider match.
    if (!realmMatch) {
      debug("No NAI realm match, final match: " + providerMatch);
      return providerMatch;
    }

    debug("NAI realm match with " + this.passpointConfig.credential.realm);
    if (providerMatch == WifiConstants.PasspointMatch.None) {
      providerMatch = WifiConstants.PasspointMatch.RoamingProvider;
    }

    debug("Final match: " + providerMatch);
    return providerMatch;
  },
};
