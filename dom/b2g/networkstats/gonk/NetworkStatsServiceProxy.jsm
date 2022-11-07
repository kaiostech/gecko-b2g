/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetworkStatsService } = ChromeUtils.import(
  "resource://gre/modules/NetworkStatsService.jsm"
);

const NETWORKSTATSSERVICEPROXY_CID = Components.ID(
  "98fd8f69-784e-4626-aa59-56d6436a3c24"
);

const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";

var DEBUG = false;
function debug(s) {
  if (DEBUG) {
    console.log("-*- NetworkStatsServiceProxy: ", s, "\n");
  }
}

function updateDebug() {
  try {
    DEBUG = DEBUG || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}
}
updateDebug();

function NetworkStatsServiceProxy() {
  debug("Proxy started");
  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
}

NetworkStatsServiceProxy.prototype = {
  /*
   * Function called in the protocol layer (HTTP, FTP, WebSocket ...etc)
   * to pass the per-app stats to NetworkStatsService.
   */
  saveAppStats: function saveAppStats(
    aOrigin,
    aNetworkInfo,
    aRxBytes,
    aTxBytes,
    aIsAccumulative,
    aIsApp,
    aManifestURL,
    aCallback
  ) {
    if (!aNetworkInfo) {
      debug(
        "|aNetworkInfo| is not specified. Failed to save stats. Returning."
      );
      return;
    }

    debug(
      "saveAppStats: " +
        aOrigin +
        " NetworkType:" +
        aNetworkInfo.type +
        " RX:" +
        aRxBytes +
        " TX:" +
        aTxBytes +
        " Accumulative:" +
        aIsAccumulative +
        " IsApp:" +
        aIsApp +
        " Manifest URL:" +
        aManifestURL
    );

    if (!aOrigin.endsWith(".localhost")) {
      if (aOrigin == "[System Principal]") {
        aManifestURL = "http://system.localhost/manifest.webmanifest";
      } else if (!aManifestURL) {
        aManifestURL = "http://search.localhost/manifest.webmanifest";
      }
    } else if (!aManifestURL) {
      aManifestURL = aOrigin + "/manifest.webmanifest";
    }

    if (!aManifestURL.startsWith("http")) {
      debug("Invalid manifest URL : " + aManifestURL);
      return;
    }

    if (aCallback) {
      aCallback = aCallback.notify;
    }

    NetworkStatsService.saveStats(
      aManifestURL,
      "",
      aNetworkInfo,
      aRxBytes,
      aTxBytes,
      aIsAccumulative,
      aCallback
    );
  },

  /*
   * Function called in the points of different system services
   * to pass the per-service stats to NetworkStatsService.
   */
  saveServiceStats: function saveServiceStats(
    aServiceType,
    aNetworkInfo,
    aRxBytes,
    aTxBytes,
    aIsAccumulative,
    aCallback
  ) {
    if (!aNetworkInfo) {
      debug(
        "|aNetworkInfo| is not specified. Failed to save stats. Returning."
      );
      return;
    }

    debug(
      "saveServiceStats: " +
        aServiceType +
        " " +
        aNetworkInfo.type +
        " " +
        aRxBytes +
        " " +
        aTxBytes +
        " " +
        aIsAccumulative
    );

    if (aCallback) {
      aCallback = aCallback.notify;
    }

    NetworkStatsService.saveStats(
      "",
      aServiceType,
      aNetworkInfo,
      aRxBytes,
      aTxBytes,
      aIsAccumulative,
      aCallback
    );
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
        break;
    }
  },

  classID: NETWORKSTATSSERVICEPROXY_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINetworkStatsServiceProxy,
    Ci.nsIObserver,
  ]),
};

this.EXPORTED_SYMBOLS = ["NetworkStatsServiceProxy"];
