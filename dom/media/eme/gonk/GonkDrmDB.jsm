/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const EXPORTED_SYMBOLS = ["GonkDrmDB"];

const { IndexedDBHelper } = ChromeUtils.importESModule(
  "resource://gre/modules/IndexedDBHelper.sys.mjs"
);

const GONKDRM_DB_NAME = "gonkdrm";
const GONKDRM_DB_VERSION = 1;
const GONKDRM_STORE_NAME = "sessions";

const DEBUG = Services.prefs.getBoolPref("media.b2g.mediadrm.debug", false);

function debug(msg) {
  console.log(`GonkDrmDB: ${msg}`);
}

function GonkDrmDB(origin, keySystem) {
  DEBUG && debug(`constructor, origin: ${origin}, key system: ${keySystem}`);
  this._origin = origin;
  this._keySystem = keySystem;
}

GonkDrmDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init() {
    DEBUG && debug(`init`);

    this.initDBHelper(GONKDRM_DB_NAME, GONKDRM_DB_VERSION, [
      GONKDRM_STORE_NAME,
    ]);
  },

  upgradeSchema(transaction, db, oldVersion, newVersion) {
    DEBUG && debug(`upgradeSchema`);

    let objStore = db.createObjectStore(GONKDRM_STORE_NAME, {
      keyPath: "sessionId",
      autoIncrement: false,
    });

    objStore.createIndex("origin", "origin", { unique: false });
    objStore.createIndex("storageId", ["origin", "keySystem"], {
      unique: false,
    });
    DEBUG && debug(`upgradeSchema, created object stores and indexes`);
  },

  add(session) {
    DEBUG && debug(`add, session: ${JSON.stringify(session)}`);

    if (!this._validateSession(session)) {
      return Promise.reject(new Error("invalid session"));
    }

    return this._newTxnHelper("readwrite", (txn, store) => {
      DEBUG && debug(`add, calling put`);
      txn.result = { success: false };

      store.put(session).onsuccess = function(event) {
        DEBUG && debug(`add, put request success`);
        txn.result = { success: true };
      };
    });
  },

  get(sessionId) {
    DEBUG && debug(`get, sessionId: ${sessionId}`);

    return this._newTxnHelper("readonly", (txn, store) => {
      DEBUG && debug(`get, calling get`);
      txn.result = { success: false };

      store.get(sessionId).onsuccess = event => {
        DEBUG && debug(`get, get request success`);

        let session = event.target.result;
        if (!this._validateSession(session)) {
          return;
        }

        DEBUG && debug(`get, result: ${JSON.stringify(session)}`);
        txn.result = { success: true, data: session };
      };
    });
  },

  remove(sessionId) {
    DEBUG && debug(`remove, sessionId: ${sessionId}`);

    return this._newTxnHelper("readwrite", (txn, store) => {
      DEBUG && debug(`remove, calling get`);
      txn.result = { success: false };

      store.get(sessionId).onsuccess = event => {
        DEBUG && debug(`remove, get request success, calling delete`);
        let session = event.target.result;
        if (!this._validateSession(session)) {
          return;
        }

        store.delete(sessionId).onsuccess = event => {
          DEBUG && debug(`remove, delete request success`);
          txn.result = { success: true };
        };
      };
    });
  },

  clear() {
    DEBUG && debug(`clear`);

    return this._newTxnHelper("readwrite", (txn, store) => {
      DEBUG && debug(`clear, calling openKeyCursor`);
      txn.result = { success: false };

      let index = store.index("storageId");
      let request = index.openKeyCursor([this._origin, this._keySystem]);
      request.onsuccess = () => {
        let cursor = request.result;
        if (cursor) {
          DEBUG && debug(`clear, deleting ${cursor.primaryKey}`);
          store.delete(cursor.primaryKey);
          cursor.continue();
        } else {
          DEBUG && debug(`clear, delete request success`);
          txn.result = { success: true };
        }
      };
    });
  },

  _validateSession(session) {
    if (!session) {
      DEBUG && debug(`Session not exist`);
      return false;
    }

    if (
      !("origin" in session || !("keySystem" in session)) ||
      !("sessionId" in session)
    ) {
      DEBUG &&
        debug(`Lacking crucial keys, session: ${JSON.stringify(session)}`);
      return false;
    }

    if (session.origin != this._origin) {
      DEBUG && debug(`Origin mismatched, session: ${JSON.stringify(session)}`);
      return false;
    }

    if (session.keySystem != this._keySystem) {
      DEBUG &&
        debug(`Key system mismatched, session: ${JSON.stringify(session)}`);
      return false;
    }

    return true;
  },

  _newTxnHelper(txnType, txnCb) {
    return new Promise((resolve, reject) =>
      this.newTxn(
        txnType,
        GONKDRM_STORE_NAME,
        txnCb,
        function txnSuccess(result) {
          if (result.success) {
            resolve(result.data);
          } else {
            reject(new Error("invalid result"));
          }
        },
        function txnFailure(msg) {
          reject(new Error(msg));
        }
      )
    );
  },
};
