/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  console.log("-*- NetworkStatsServiceProxy: ", s, "\n");
}

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { NetworkStatsService } = ChromeUtils.import(
  "resource://gre/modules/NetworkStatsService.jsm"
);

const NETWORKSTATSSERVICEPROXY_CID = Components.ID(
  "98fd8f69-784e-4626-aa59-56d6436a3c24"
);

const nsINetworkStatsServiceProxy = Ci.nsINetworkStatsServiceProxy;

function NetworkStatsServiceProxy() {
  if (DEBUG) {
    debug("Proxy started");
  }
}

NetworkStatsServiceProxy.prototype = {
  /*
   * Function called in the protocol layer (HTTP, FTP, WebSocket ...etc)
   * to pass the per-app stats to NetworkStatsService.
   */
  saveAppStats: function saveAppStats(
    aOrigin,
    aNetworkInfo,
    aTimeStamp,
    aRxBytes,
    aTxBytes,
    aIsAccumulative,
    aIsApp,
    aCallback
  ) {
    if (!aNetworkInfo) {
      if (DEBUG) {
        debug(
          "|aNetworkInfo| is not specified. Failed to save stats. Returning."
        );
      }
      return;
    }

    if (DEBUG) {
      debug(
        "saveAppStats: " +
          aOrigin +
          " NetworkType:" +
          aNetworkInfo.type +
          " TimeStamp:" +
          aTimeStamp +
          " RX:" +
          aRxBytes +
          " TX:" +
          aTxBytes +
          " Accumulative:" +
          aIsAccumulative +
          " IsApp:" +
          aIsApp
      );
    }

    // Treat non app & suffix without "localhost" as browser app.
    if (!aOrigin.endsWith(".localhost") && !aIsApp) {
      aOrigin = "http://system.localhost";
    }

    if (aCallback) {
      aCallback = aCallback.notify;
    }

    NetworkStatsService.saveStats(
      aOrigin,
      "",
      aNetworkInfo,
      aTimeStamp,
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
    aTimeStamp,
    aRxBytes,
    aTxBytes,
    aIsAccumulative,
    aCallback
  ) {
    if (!aNetworkInfo) {
      if (DEBUG) {
        debug(
          "|aNetworkInfo| is not specified. Failed to save stats. Returning."
        );
      }
      return;
    }

    if (DEBUG) {
      debug(
        "saveServiceStats: " +
          aServiceType +
          " " +
          aNetworkInfo.type +
          " " +
          aTimeStamp +
          " " +
          aRxBytes +
          " " +
          aTxBytes +
          " " +
          aIsAccumulative
      );
    }

    if (aCallback) {
      aCallback = aCallback.notify;
    }

    NetworkStatsService.saveStats(
      "",
      aServiceType,
      aNetworkInfo,
      aTimeStamp,
      aRxBytes,
      aTxBytes,
      aIsAccumulative,
      aCallback
    );
  },

  classID: NETWORKSTATSSERVICEPROXY_CID,
  QueryInterface: ChromeUtils.generateQI([nsINetworkStatsServiceProxy]),
};

this.EXPORTED_SYMBOLS = ["NetworkStatsServiceProxy"];
