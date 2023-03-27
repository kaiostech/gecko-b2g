/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { IndexedDBHelper } from "resource://gre/modules/IndexedDBHelper.sys.mjs";

const ALARMDB_NAME = "alarms";
const ALARMDB_VERSION = 1;
const ALARMSTORE_NAME = "alarms";

const DEBUG = Services.prefs.getBoolPref("dom.alarm.debug", false);
function debug(aMsg) {
  console.log(`AlarmDB: ${aMsg}`);
}

export const AlarmDB = function AlarmDB() {
  DEBUG && debug("AlarmDB()");
};

AlarmDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init: function init() {
    DEBUG && debug("init()");

    this.initDBHelper(ALARMDB_NAME, ALARMDB_VERSION, [ALARMSTORE_NAME]);
  },

  upgradeSchema: function upgradeSchema(
    aTransaction,
    aDb,
    aOldVersion,
    aNewVersion
  ) {
    DEBUG && debug("upgradeSchema()");

    let objStore = aDb.createObjectStore(ALARMSTORE_NAME, {
      keyPath: "id",
      autoIncrement: true,
    });

    objStore.createIndex("date", "date", { unique: false });
    objStore.createIndex("ignoreTimezone", "ignoreTimezone", { unique: false });
    objStore.createIndex("timezoneOffset", "timezoneOffset", { unique: false });
    objStore.createIndex("data", "data", { unique: false });
    objStore.createIndex("url", "url", { unique: false });

    DEBUG && debug("Created object stores and indexes");
  },

  /**
   * @param aAlarm
   *        The record to be added.
   * @param aSuccessCb
   *        Callback function to invoke with result ID.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  add: function add(aAlarm, aSuccessCb, aErrorCb) {
    DEBUG && debug("add()");

    this.newTxn(
      "readwrite",
      ALARMSTORE_NAME,
      function txnCb(aTxn, aStore) {
        DEBUG && debug("Going to add " + JSON.stringify(aAlarm));
        aStore.put(aAlarm).onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          DEBUG && debug("Request successful. New record ID: " + aTxn.result);
        };
      },
      aSuccessCb,
      aErrorCb
    );
  },

  /**
   * @param aId
   *        The ID of record to be removed.
   * @param aUrl
   *        The url of the app that alarm belongs to.
   *        If null, directly remove the ID record; otherwise,
   *        need to check if the alarm belongs to this app.
   * @param aSuccessCb
   *        Callback function to invoke with result.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  remove: function remove(aId, aUrl, aSuccessCb, aErrorCb) {
    DEBUG && debug("remove()");

    this.newTxn(
      "readwrite",
      ALARMSTORE_NAME,
      function txnCb(aTxn, aStore) {
        DEBUG && debug("Going to remove " + aId);

        // Look up the existing record and compare the url
        // to see if the alarm to be removed belongs to this app.
        aStore.get(aId).onsuccess = function doRemove(aEvent) {
          let alarm = aEvent.target.result;

          if (!alarm) {
            DEBUG && debug("Alarm doesn't exist. No need to remove it.");
            return;
          }

          if (aUrl && aUrl != alarm.url) {
            DEBUG && debug("Cannot remove the alarm added by other apps.");
            return;
          }

          aStore.delete(aId);
        };
      },
      aSuccessCb,
      aErrorCb
    );
  },

  /**
   * @param aUrl
   *        The url of the app that alarms belong to.
   *        If null, directly return all alarms; otherwise,
   *        only return the alarms that belong to this app.
   * @param aSuccessCb
   *        Callback function to invoke with result array.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  getAll: function getAll(aUrl, aSuccessCb, aErrorCb) {
    DEBUG && debug("getAll()");

    this.newTxn(
      "readonly",
      ALARMSTORE_NAME,
      function txnCb(aTxn, aStore) {
        if (!aTxn.result) {
          aTxn.result = [];
        }

        let index = aStore.index("url");
        index.mozGetAll(aUrl).onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          DEBUG &&
            debug("Request successful. Record count: " + aTxn.result.length);
        };
      },
      aSuccessCb,
      aErrorCb
    );
  },
};
