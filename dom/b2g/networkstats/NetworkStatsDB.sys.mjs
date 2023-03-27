/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var DEBUG = false;
function debug(s) {
  if (DEBUG) {
    console.log("-*- NetworkStatsDB: ", s, "\n");
  }
}

import { IndexedDBHelper } from "resource://gre/modules/IndexedDBHelper.sys.mjs";

const DB_NAME = "net_stats";
const DB_VERSION = 2;
const DEPRECATED_STATS_STORE_NAME = [
  "net_stats_store", // existed in DB version 1
];

const STATS_STORE_NAME = "net_stats_store_v2";
const ALARMS_STORE_NAME = "net_alarm";

// Constant defining the maximum values allowed per interface. If more, older
// will be erased.
const VALUES_MAX_LENGTH = 6 * 30;

// Constant defining the rate of the samples. Daily.
const SAMPLE_RATE = 1000 * 60 * 60 * 24;

export const NetworkStatsDB = function NetworkStatsDB(aDebug) {
  DEBUG = aDebug;
  debug("Constructor");
  this.initDBHelper(DB_NAME, DB_VERSION, [STATS_STORE_NAME, ALARMS_STORE_NAME]);
};

NetworkStatsDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  setDebug: function setDebug(aDebug) {
    DEBUG = aDebug;
  },

  dbNewTxn: function dbNewTxn(store_name, txn_type, callback, txnCb) {
    function successCb(result) {
      txnCb(null, result);
    }
    function errorCb(error) {
      txnCb(error, null);
    }
    return this.newTxn(txn_type, store_name, callback, successCb, errorCb);
  },

  /**
   * The onupgradeneeded handler of the IDBOpenDBRequest.
   * This function is called in IndexedDBHelper open() method.
   *
   * @param {IDBTransaction} aTransaction
   *        {IDBDatabase} aDb
   *        {64-bit integer} aOldVersion The version number on local storage.
   *        {64-bit integer} aNewVersion The version number to be upgraded to.
   *
   * @note  Be careful with the database upgrade pattern.
   *        Because IndexedDB operations are performed asynchronously, we must
   *        apply a recursive approach instead of an iterative approach while
   *        upgrading versions.
   */
  upgradeSchema: function upgradeSchema(
    aTransaction,
    aDb,
    aOldVersion,
    aNewVersion
  ) {
    debug(
      "upgrade schema from: " + aOldVersion + " to " + aNewVersion + " called!"
    );
    let db = aDb;
    let objectStore;

    // An array of upgrade functions for each version.
    let upgradeSteps = [
      function upgrade0to1() {
        debug("Upgrade 0 to 1: Create object stores and indexes.");

        // Create object store for alarms.
        objectStore = db.createObjectStore(ALARMS_STORE_NAME, {
          keyPath: "id",
          autoIncrement: true,
        });
        objectStore.createIndex("originURL", "originURL", {
          unique: false,
        });
        objectStore.createIndex("alarm", ["networkId", "relativeThreshold"], {
          unique: false,
        });

        // Create object store for networkStats.
        let newObjectStore = db.createObjectStore(
          DEPRECATED_STATS_STORE_NAME[0],
          {
            keyPath: ["origin", "serviceType", "network", "timestamp"],
          }
        );
        newObjectStore.createIndex("origin", "origin", { unique: false });
        newObjectStore.createIndex("network", "network", { unique: false });
        newObjectStore.createIndex("networkType", "networkType", {
          unique: false,
        });
        newObjectStore.createIndex("serviceType", "serviceType", {
          unique: false,
        });
        newObjectStore.createIndex("timestamp", "timestamp", { unique: false });
        newObjectStore.createIndex("rxBytes", "rxBytes", { unique: false });
        newObjectStore.createIndex("txBytes", "txBytes", { unique: false });
        newObjectStore.createIndex("rxTotalBytes", "rxTotalBytes", {
          unique: false,
        });
        newObjectStore.createIndex("txTotalBytes", "txTotalBytes", {
          unique: false,
        });
        upgradeNextVersion();
      },
      function upgrade1to2() {
        if (DEBUG) {
          debug("Upgrade 1 to 2: Replace origin by manifestURL.");
        }

        let newObjectStore;
        let deprecatedName = DEPRECATED_STATS_STORE_NAME[0];
        // Create object store for networkStats.
        newObjectStore = db.createObjectStore(STATS_STORE_NAME, {
          keyPath: ["manifestURL", "serviceType", "network", "timestamp"],
        });
        newObjectStore.createIndex("network", "network", { unique: false });
        // Copy records from the current object store to the new one.
        objectStore = aTransaction.objectStore(deprecatedName);
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor) {
            db.deleteObjectStore(deprecatedName);
            // upgrade1to2 completed now.
            return;
          }
          let newStats = {};
          let oldStats = cursor.value;
          let origin = oldStats.origin;
          delete oldStats.origin;
          if (origin.endsWith(".localhost")) {
            newStats.manifestURL = origin + "/manifest.webmanifest";
          } else if (origin == "default" || origin == "") {
            newStats.manifestURL = origin;
          }
          Object.assign(newStats, oldStats);

          if (newStats.manifestURL) {
            newObjectStore.put(newStats);
          }
          cursor.continue();
        };
      },
    ];

    let index = aOldVersion;
    let outer = this;

    function upgradeNextVersion() {
      if (index == aNewVersion) {
        debug("Upgrade finished.");
        return;
      }

      try {
        var i = index++;
        debug("Upgrade step: " + i + "\n");
        upgradeSteps[i].call(outer);
      } catch (ex) {
        dump("Caught exception " + ex);
        throw ex;
      }
    }

    if (aNewVersion > upgradeSteps.length) {
      debug("No migration steps for the new version!");
      aTransaction.abort();
      return;
    }

    upgradeNextVersion();
  },

  importData: function importData(aStats) {
    let stats = {
      manifestURL: aStats.manifestURL,
      serviceType: aStats.serviceType,
      network: [aStats.networkId, aStats.networkType],
      timestamp: aStats.timestamp,
      rxBytes: aStats.rxBytes,
      txBytes: aStats.txBytes,
      rxSystemBytes: aStats.rxSystemBytes,
      txSystemBytes: aStats.txSystemBytes,
      rxTotalBytes: aStats.rxTotalBytes,
      txTotalBytes: aStats.txTotalBytes,
    };

    return stats;
  },

  exportData: function exportData(aStats) {
    let stats = {
      manifestURL: aStats.manifestURL,
      serviceType: aStats.serviceType,
      networkId: aStats.network[0],
      networkType: aStats.network[1],
      timestamp: aStats.timestamp,
      rxBytes: aStats.rxBytes,
      txBytes: aStats.txBytes,
      rxTotalBytes: aStats.rxTotalBytes,
      txTotalBytes: aStats.txTotalBytes,
    };

    return stats;
  },

  normalizeDate: function normalizeDate(aDate) {
    // Convert to UTC according to timezone and
    // filter timestamp to get SAMPLE_RATE precission
    let timestamp = aDate.getTime() - aDate.getTimezoneOffset() * 60 * 1000;
    timestamp = Math.floor(timestamp / SAMPLE_RATE) * SAMPLE_RATE;
    return timestamp;
  },

  saveStats: function saveStats(aStats, aResultCb) {
    let isAccumulative = aStats.isAccumulative;
    let timestamp = this.normalizeDate(aStats.date);

    let stats = {
      manifestURL: aStats.manifestURL,
      serviceType: aStats.serviceType,
      networkId: aStats.networkId,
      networkType: aStats.networkType,
      timestamp,
      rxBytes: isAccumulative ? 0 : aStats.rxBytes,
      txBytes: isAccumulative ? 0 : aStats.txBytes,
      rxSystemBytes: isAccumulative ? aStats.rxBytes : 0,
      txSystemBytes: isAccumulative ? aStats.txBytes : 0,
      rxTotalBytes: isAccumulative ? aStats.rxBytes : 0,
      txTotalBytes: isAccumulative ? aStats.txBytes : 0,
    };

    stats = this.importData(stats);

    this.dbNewTxn(
      STATS_STORE_NAME,
      "readwrite",
      function(aTxn, aStore) {
        debug("Filtered time: " + new Date(timestamp));
        debug("New stats: " + JSON.stringify(stats));

        let lowerFilter = [
          stats.manifestURL,
          stats.serviceType,
          stats.network,
          0,
        ];
        let upperFilter = [
          stats.manifestURL,
          stats.serviceType,
          stats.network,
          "",
        ];
        let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

        let request = aStore.openCursor(range, "prev");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (!cursor) {
            // Empty, so save first element.

            if (!isAccumulative) {
              // Update total bytes for the fist element.
              // Others will handle by processSamplesDiff function.
              stats.rxTotalBytes = stats.rxBytes;
              stats.txTotalBytes = stats.txBytes;
              this._saveStats(aTxn, aStore, stats);
              return;
            }

            // There could be a time delay between the point when the network
            // interface comes up and the point when the database is initialized.
            // In this short interval some traffic data are generated but are not
            // registered by the first sample.
            stats.rxBytes = stats.rxTotalBytes;
            stats.txBytes = stats.txTotalBytes;

            // However, if the interface is not switched on after the database is
            // initialized (dual sim use case) stats should be set to 0.
            let req = aStore.index("network").openKeyCursor(null, "nextunique");
            req.onsuccess = function onsuccess(event) {
              let cursor = event.target.result;
              if (cursor) {
                if (cursor.key[1] == stats.network[1]) {
                  stats.rxBytes = 0;
                  stats.txBytes = 0;
                  this._saveStats(aTxn, aStore, stats);
                  return;
                }

                cursor.continue();
                return;
              }

              this._saveStats(aTxn, aStore, stats);
            }.bind(this);

            return;
          }

          // There are old samples
          debug("Last value " + JSON.stringify(cursor.value));

          // Remove stats previous to now - VALUE_MAX_LENGTH
          this._removeOldStats(
            aTxn,
            aStore,
            stats.manifestURL,
            stats.serviceType,
            stats.network,
            stats.timestamp
          );

          // Process stats before save
          this._processSamplesDiff(aTxn, aStore, cursor, stats, isAccumulative);
        }.bind(this);
      }.bind(this),
      aResultCb
    );
  },

  /*
   * This function check that stats are saved in the database following the sample rate.
   * In this way is easier to find elements when stats are requested.
   */
  _processSamplesDiff: function _processSamplesDiff(
    aTxn,
    aStore,
    aLastSampleCursor,
    aNewSample,
    aIsAccumulative
  ) {
    let lastSample = aLastSampleCursor.value;

    // Get difference between last and new sample.
    let diff = (aNewSample.timestamp - lastSample.timestamp) / SAMPLE_RATE;
    if (diff % 1) {
      // diff is decimal, so some error happened because samples are stored as a multiple
      // of SAMPLE_RATE
      aTxn.abort();
      throw new Error("Error processing samples");
    }

    debug(
      "New: " +
        aNewSample.timestamp +
        " - Last: " +
        lastSample.timestamp +
        " - diff: " +
        diff
    );

    // If the incoming data has a accumulation feature, the new
    // |txBytes|/|rxBytes| is assigend by differnces between the new
    // |txTotalBytes|/|rxTotalBytes| and the last |txTotalBytes|/|rxTotalBytes|.
    // Else, if incoming data is non-accumulative, the |txBytes|/|rxBytes|
    // is the new |txBytes|/|rxBytes|.
    let rxDiff = 0;
    let txDiff = 0;
    if (aIsAccumulative) {
      rxDiff = aNewSample.rxSystemBytes - lastSample.rxSystemBytes;
      txDiff = aNewSample.txSystemBytes - lastSample.txSystemBytes;
      if (rxDiff < 0 || txDiff < 0) {
        rxDiff = aNewSample.rxSystemBytes;
        txDiff = aNewSample.txSystemBytes;
      }
      aNewSample.rxBytes = rxDiff;
      aNewSample.txBytes = txDiff;

      aNewSample.rxTotalBytes = lastSample.rxTotalBytes + rxDiff;
      aNewSample.txTotalBytes = lastSample.txTotalBytes + txDiff;
    } else {
      rxDiff = aNewSample.rxBytes;
      txDiff = aNewSample.txBytes;
    }

    if (diff == 1) {
      // New element.

      // If the incoming data is non-accumulative, the new
      // |rxTotalBytes|/|txTotalBytes| needs to be updated by adding new
      // |rxBytes|/|txBytes| to the last |rxTotalBytes|/|txTotalBytes|.
      if (!aIsAccumulative) {
        aNewSample.rxTotalBytes = aNewSample.rxBytes + lastSample.rxTotalBytes;
        aNewSample.txTotalBytes = aNewSample.txBytes + lastSample.txTotalBytes;
      }

      this._saveStats(aTxn, aStore, aNewSample);
      return;
    }
    if (diff > 1) {
      // Some samples lost. Device off during one or more samplerate periods.
      // Time or timezone changed
      // Add lost samples with 0 bytes and the actual one.
      if (diff > VALUES_MAX_LENGTH) {
        diff = VALUES_MAX_LENGTH;
      }

      let data = [];
      for (let i = diff - 2; i >= 0; i--) {
        let time = aNewSample.timestamp - SAMPLE_RATE * (i + 1);
        let sample = {
          manifestURL: aNewSample.manifestURL,
          serviceType: aNewSample.serviceType,
          network: aNewSample.network,
          timestamp: time,
          rxBytes: 0,
          txBytes: 0,
          rxSystemBytes: lastSample.rxSystemBytes,
          txSystemBytes: lastSample.txSystemBytes,
          rxTotalBytes: lastSample.rxTotalBytes,
          txTotalBytes: lastSample.txTotalBytes,
        };

        data.push(sample);
      }

      // If the incoming data is non-accumulative, the new
      // |rxTotalBytes|/|txTotalBytes| needs to be updated by adding new
      // |rxBytes|/|txBytes| to the last |rxTotalBytes|/|txTotalBytes|.
      if (!aIsAccumulative) {
        aNewSample.rxTotalBytes = aNewSample.rxBytes + lastSample.rxTotalBytes;
        aNewSample.txTotalBytes = aNewSample.txBytes + lastSample.txTotalBytes;
      }

      data.push(aNewSample);
      this._saveStats(aTxn, aStore, data);
      return;
    }
    if (diff == 0 || diff < 0) {
      // New element received before samplerate period. It means that device has
      // been restarted (or clock / timezone change).
      // Update element. If diff < 0, clock or timezone changed back. Place data
      // in the last sample.

      // Old |rxTotalBytes|/|txTotalBytes| needs to get updated by adding the
      // last |rxTotalBytes|/|txTotalBytes|.
      lastSample.rxBytes += rxDiff;
      lastSample.txBytes += txDiff;
      lastSample.rxSystemBytes = aNewSample.rxSystemBytes;
      lastSample.txSystemBytes = aNewSample.txSystemBytes;
      lastSample.rxTotalBytes += rxDiff;
      lastSample.txTotalBytes += txDiff;

      debug("Update: " + JSON.stringify(lastSample));
      aLastSampleCursor.update(lastSample);
    }
  },

  _saveStats: function _saveStats(aTxn, aStore, aNetworkStats) {
    debug("_saveStats: " + JSON.stringify(aNetworkStats));

    if (Array.isArray(aNetworkStats)) {
      let len = aNetworkStats.length - 1;
      for (let i = 0; i <= len; i++) {
        aStore.put(aNetworkStats[i]);
      }
    } else {
      aStore.put(aNetworkStats);
    }
  },

  _removeOldStats: function _removeOldStats(
    aTxn,
    aStore,
    aManifestURL,
    aServiceType,
    aNetwork,
    aDate
  ) {
    // Callback function to remove old items when new ones are added.
    let filterDate = aDate - (SAMPLE_RATE * VALUES_MAX_LENGTH - 1);

    if (filterDate < 0) {
      filterDate = 0;
    }

    let lowerFilter = [aManifestURL, aServiceType, aNetwork, 0];
    let upperFilter = [aManifestURL, aServiceType, aNetwork, filterDate];
    let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);
    let lastSample = null;
    let self = this;

    aStore.openCursor(range).onsuccess = function(event) {
      var cursor = event.target.result;
      if (cursor) {
        lastSample = cursor.value;
        cursor.delete();
        cursor.continue();
        return;
      }

      // If all samples for a network are removed, an empty sample
      // has to be saved to keep the totalBytes in order to compute
      // future samples because system counters are not set to 0.
      // Thus, if there are no samples left, the last sample removed
      // will be saved again after setting its bytes to 0.
      let request = aStore.index("network").openCursor(aNetwork);
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor && lastSample != null) {
          let timestamp = new Date();
          timestamp = self.normalizeDate(timestamp);
          lastSample.timestamp = timestamp;
          lastSample.rxBytes = 0;
          lastSample.txBytes = 0;
          self._saveStats(aTxn, aStore, lastSample);
        }
      };
    };
  },

  clearInterfaceStats: function clearInterfaceStats(aNetwork, aResultCb) {
    let network = [aNetwork.network.id, aNetwork.network.type];
    let self = this;

    // Clear and save an empty sample to keep sync with system counters
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readwrite",
      function(aTxn, aStore) {
        let sample = null;
        let request = aStore.index("network").openCursor(network, "prev");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (!sample && cursor.value.manifestURL == "") {
              sample = cursor.value;
            }

            cursor.delete();
            cursor.continue();
            return;
          }

          if (sample) {
            let timestamp = new Date();
            timestamp = self.normalizeDate(timestamp);
            sample.timestamp = timestamp;
            sample.manifestURL = "";
            sample.serviceType = "";
            sample.rxBytes = 0;
            sample.txBytes = 0;
            sample.rxTotalBytes = 0;
            sample.txTotalBytes = 0;

            self._saveStats(aTxn, aStore, sample);
          }
        };
      },
      this._resetAlarms.bind(this, aNetwork.networkId, aResultCb)
    );
  },

  clearStats: function clearStats(aNetworks, aResultCb) {
    let index = 0;
    let self = this;

    let callback = function(aError, aResult) {
      index++;

      if (!aError && index < aNetworks.length) {
        self.clearInterfaceStats(aNetworks[index], callback);
        return;
      }

      aResultCb(aError, aResult);
    };

    if (!aNetworks[index]) {
      aResultCb(null, true);
      return;
    }
    this.clearInterfaceStats(aNetworks[index], callback);
  },

  getCurrentStats: function getCurrentStats(aNetwork, aDate, aResultCb) {
    debug(
      "Get current stats for " + JSON.stringify(aNetwork) + " since " + aDate
    );

    let network = [aNetwork.id, aNetwork.type];
    if (aDate) {
      this._getCurrentStatsFromDate(network, aDate, aResultCb);
      return;
    }

    this._getCurrentStats(network, aResultCb);
  },

  _getCurrentStats: function _getCurrentStats(aNetwork, aResultCb) {
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(txn, store) {
        let request = null;
        let upperFilter = ["default", "", aNetwork, Date.now()];
        let range = IDBKeyRange.upperBound(upperFilter, false);
        let result = {
          rxBytes: 0,
          txBytes: 0,
          rxTotalBytes: 0,
          txTotalBytes: 0,
        };

        request = store.openCursor(range, "prev");

        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            result.rxBytes = result.rxTotalBytes = cursor.value.rxTotalBytes;
            result.txBytes = result.txTotalBytes = cursor.value.txTotalBytes;
          }

          txn.result = result;
        };
      },
      aResultCb
    );
  },

  _getCurrentStatsFromDate: function _getCurrentStatsFromDate(
    aNetwork,
    aDate,
    aResultCb
  ) {
    aDate = new Date(aDate);
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(txn, store) {
        let request = null;
        let start = this.normalizeDate(aDate);
        let lowerFilter = ["default", "", aNetwork, start];
        let upperFilter = ["default", "", aNetwork, Date.now()];
        let range = IDBKeyRange.upperBound(upperFilter, false);
        let result = {
          rxBytes: 0,
          txBytes: 0,
          rxTotalBytes: 0,
          txTotalBytes: 0,
        };

        request = store.openCursor(range, "prev");

        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            result.rxBytes = result.rxTotalBytes = cursor.value.rxTotalBytes;
            result.txBytes = result.txTotalBytes = cursor.value.txTotalBytes;
          }

          let timestamp = cursor.value.timestamp;
          let range = IDBKeyRange.lowerBound(lowerFilter, false);
          request = store.openCursor(range);

          request.onsuccess = function onsuccess(event) {
            let cursor = event.target.result;
            if (cursor) {
              if (cursor.value.timestamp == timestamp) {
                // There is one sample only.
                result.rxBytes = cursor.value.rxBytes;
                result.txBytes = cursor.value.txBytes;
              } else if (cursor.value.timestamp == start) {
                result.rxBytes -= cursor.value.rxTotalBytes;
                result.txBytes -= cursor.value.txTotalBytes;
              }
            }

            txn.result = result;
          };
        };
      }.bind(this),
      aResultCb
    );
  },

  find: function find(
    aResultCb,
    aManifestURL,
    aServiceType,
    aNetwork,
    aStart,
    aEnd,
    aAppManifestURL
  ) {
    let offset = new Date().getTimezoneOffset() * 60 * 1000;
    let start = this.normalizeDate(aStart);
    let end = this.normalizeDate(aEnd);

    debug(
      "Find samples for manifestURL: " +
        aManifestURL +
        " serviceType: " +
        aServiceType +
        " network: " +
        JSON.stringify(aNetwork) +
        " from " +
        start +
        " until " +
        end
    );
    debug("Start time: " + new Date(start));
    debug("End time: " + new Date(end));

    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(aTxn, aStore) {
        let network = [aNetwork.id, aNetwork.type];
        let lowerFilter = [aManifestURL, aServiceType, network, start];
        let upperFilter = [aManifestURL, aServiceType, network, end];
        let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

        let data = [];

        if (!aTxn.result) {
          aTxn.result = {};
        }
        aTxn.result.appManifestURL = aAppManifestURL;
        aTxn.result.serviceType = aServiceType;
        aTxn.result.network = aNetwork;
        aTxn.result.start = aStart;
        aTxn.result.end = aEnd;

        aStore.openCursor(range).onsuccess = function(event) {
          var cursor = event.target.result;
          if (cursor) {
            // We use rxTotalBytes/txTotalBytes instead of rxBytes/txBytes for
            // the first (oldest) sample. The rx/txTotalBytes fields record
            // accumulative usage amount, which means even if old samples were
            // expired and removed from the Database, we can still obtain the
            // correct network usage.
            if (!data.length) {
              data.push({
                rxBytes: cursor.value.rxTotalBytes,
                txBytes: cursor.value.txTotalBytes,
                date: new Date(cursor.value.timestamp + offset),
              });
            } else {
              data.push({
                rxBytes: cursor.value.rxBytes,
                txBytes: cursor.value.txBytes,
                date: new Date(cursor.value.timestamp + offset),
              });
            }
            cursor.continue();
            return;
          }

          this.fillResultSamples(start + offset, end + offset, data);
          aTxn.result.data = data;
        }.bind(this); // openCursor(range).onsuccess() callback
      }.bind(this),
      aResultCb
    );
  },

  /*
   * Fill data array (samples from database) with empty samples to match
   * requested start / end dates.
   */
  fillResultSamples: function fillResultSamples(aStart, aEnd, aData) {
    if (!aData.length) {
      aData.push({
        rxBytes: undefined,
        txBytes: undefined,
        date: new Date(aStart),
      });
    }

    while (aStart < aData[0].date.getTime()) {
      aData.unshift({
        rxBytes: undefined,
        txBytes: undefined,
        date: new Date(aData[0].date.getTime() - SAMPLE_RATE),
      });
    }

    while (aEnd > aData[aData.length - 1].date.getTime()) {
      aData.push({
        rxBytes: undefined,
        txBytes: undefined,
        date: new Date(aData[aData.length - 1].date.getTime() + SAMPLE_RATE),
      });
    }
  },

  getAvailableNetworks: function getAvailableNetworks(aResultCb) {
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(aTxn, aStore) {
        if (!aTxn.result) {
          aTxn.result = [];
        }

        let request = aStore.index("network").openKeyCursor(null, "nextunique");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            aTxn.result.push({ id: cursor.key[0], type: cursor.key[1] });
            cursor.continue();
          }
        };
      },
      aResultCb
    );
  },

  isNetworkAvailable: function isNetworkAvailable(aNetwork, aResultCb) {
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(aTxn, aStore) {
        if (!aTxn.result) {
          aTxn.result = false;
        }

        let network = [aNetwork.id, aNetwork.type];
        let request = aStore
          .index("network")
          .openKeyCursor(IDBKeyRange.only(network));
        request.onsuccess = function onsuccess(event) {
          if (event.target.result) {
            aTxn.result = true;
          }
        };
      },
      aResultCb
    );
  },

  getAvailableServiceTypes: function getAvailableServiceTypes(aResultCb) {
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(aTxn, aStore) {
        if (!aTxn.result) {
          aTxn.result = [];
        }

        let request = aStore
          .index("serviceType")
          .openKeyCursor(null, "nextunique");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor && cursor.key != "") {
            aTxn.result.push({ serviceType: cursor.key });
            cursor.continue();
          }
        };
      },
      aResultCb
    );
  },

  get sampleRate() {
    return SAMPLE_RATE;
  },

  get maxStorageSamples() {
    return VALUES_MAX_LENGTH;
  },

  logAllRecords: function logAllRecords(aResultCb) {
    this.dbNewTxn(
      STATS_STORE_NAME,
      "readonly",
      function(aTxn, aStore) {
        aStore.mozGetAll().onsuccess = function onsuccess(event) {
          aTxn.result = event.target.result;
        };
      },
      aResultCb
    );
  },

  alarmToRecord: function alarmToRecord(aAlarm) {
    let record = {
      networkId: aAlarm.networkId,
      absoluteThreshold: aAlarm.absoluteThreshold,
      relativeThreshold: aAlarm.relativeThreshold,
      startTime: aAlarm.startTime,
      data: aAlarm.data,
      originURL: aAlarm.originURL,
      pageURL: aAlarm.pageURL,
    };

    if (aAlarm.id) {
      record.id = aAlarm.id;
    }

    return record;
  },

  recordToAlarm: function recordToalarm(aRecord) {
    let alarm = {
      networkId: aRecord.networkId,
      absoluteThreshold: aRecord.absoluteThreshold,
      relativeThreshold: aRecord.relativeThreshold,
      startTime: aRecord.startTime,
      data: aRecord.data,
      originURL: aRecord.originURL,
      pageURL: aRecord.pageURL,
    };

    if (aRecord.id) {
      alarm.id = aRecord.id;
    }

    return alarm;
  },

  addAlarm: function addAlarm(aAlarm, aResultCb) {
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readwrite",
      function(txn, store) {
        debug("Going to add " + JSON.stringify(aAlarm));

        let record = this.alarmToRecord(aAlarm);
        store.put(record).onsuccess = function setResult(aEvent) {
          txn.result = aEvent.target.result;
          debug("Request successful. New record ID: " + txn.result);
        };
      }.bind(this),
      aResultCb
    );
  },

  getFirstAlarm: function getFirstAlarm(aNetworkId, aResultCb) {
    let self = this;

    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readonly",
      function(txn, store) {
        debug("Get first alarm for network " + aNetworkId);

        let lowerFilter = [aNetworkId, 0];
        let upperFilter = [aNetworkId, ""];
        let range = IDBKeyRange.bound(lowerFilter, upperFilter);

        store.index("alarm").openCursor(range).onsuccess = function onsuccess(
          event
        ) {
          let cursor = event.target.result;
          txn.result = null;
          if (cursor) {
            txn.result = self.recordToAlarm(cursor.value);
          }
        };
      },
      aResultCb
    );
  },

  removeAlarm: function removeAlarm(aAlarmId, aOriginURL, aResultCb) {
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readwrite",
      function(txn, store) {
        debug("Remove alarm " + aAlarmId);

        store.get(aAlarmId).onsuccess = function onsuccess(event) {
          let record = event.target.result;
          txn.result = false;
          if (!record || (aOriginURL && record.originURL != aOriginURL)) {
            return;
          }

          store.delete(aAlarmId);
          txn.result = true;
        };
      },
      aResultCb
    );
  },

  removeAlarms: function removeAlarms(aOriginURL, aResultCb) {
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readwrite",
      function(txn, store) {
        debug("Remove alarms of " + aOriginURL);

        store
          .index("originURL")
          .openCursor(aOriginURL).onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            cursor.delete();
            cursor.continue();
          }
        };
      },
      aResultCb
    );
  },

  updateAlarm: function updateAlarm(aAlarm, aResultCb) {
    let self = this;
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readwrite",
      function(txn, store) {
        debug("Update alarm " + aAlarm.id);

        let record = self.alarmToRecord(aAlarm);
        store.openCursor(record.id).onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          txn.result = false;
          if (cursor) {
            cursor.update(record);
            txn.result = true;
          }
        };
      },
      aResultCb
    );
  },

  getAlarms: function getAlarms(aNetworkId, aOriginURL, aResultCb) {
    let self = this;
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readonly",
      function(txn, store) {
        debug("Get alarms for " + aOriginURL);

        txn.result = [];
        store
          .index("originURL")
          .openCursor(aOriginURL).onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (!cursor) {
            return;
          }

          if (!aNetworkId || cursor.value.networkId == aNetworkId) {
            txn.result.push(self.recordToAlarm(cursor.value));
          }

          cursor.continue();
        };
      },
      aResultCb
    );
  },

  _resetAlarms: function _resetAlarms(aNetworkId, aResultCb) {
    this.dbNewTxn(
      ALARMS_STORE_NAME,
      "readwrite",
      function(txn, store) {
        debug("Reset alarms for network " + aNetworkId);

        let lowerFilter = [aNetworkId, 0];
        let upperFilter = [aNetworkId, ""];
        let range = IDBKeyRange.bound(lowerFilter, upperFilter);

        store.index("alarm").openCursor(range).onsuccess = function onsuccess(
          event
        ) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.startTime) {
              cursor.value.relativeThreshold = cursor.value.threshold;
              cursor.update(cursor.value);
            }
            cursor.continue();
          }
        };
      },
      aResultCb
    );
  },
};
