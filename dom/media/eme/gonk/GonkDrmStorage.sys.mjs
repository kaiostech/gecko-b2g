/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GonkDrmDB } from "resource://gre/modules/GonkDrmDB.sys.mjs";

const DEBUG = Services.prefs.getBoolPref("media.b2g.mediadrm.debug", false);

function debug(msg) {
  console.log(`GonkDrmStorage: ${msg}`);
}

export function GonkDrmStorage() {
  DEBUG && debug(`constructor`);
}

GonkDrmStorage.prototype = {
  init(origin, keySystem) {
    DEBUG && debug(`init ${keySystem} storage for ${origin}`);
    this._origin = origin;
    this._keySystem = keySystem;
    this._db = new GonkDrmDB(origin, keySystem);
    this._db.init();
  },

  uninit() {
    DEBUG && debug(`uninit`);
    if (this._db) {
      this._db.close();
      this._db = null;
    }
    this._origin = null;
    this._keySystem = null;
  },

  add(sessionId, mimeType, sessionType, keySetId, callback) {
    DEBUG && debug(`add ${sessionId}`);

    const session = {
      sessionId,
      mimeType,
      sessionType,
      keySetId,
      origin: this._origin,
      keySystem: this._keySystem,
      modifyTime: Date.now(),
    };

    this._db
      .add(session)
      .then(result => {
        DEBUG && debug(`add success`);
        callback.onAdd(Cr.NS_OK);
      })
      .catch(e => {
        DEBUG && debug(`add error: ${e.message}`);
        callback.onAdd(Cr.NS_ERROR_FAILURE);
      });
  },

  get(sessionId, callback) {
    DEBUG && debug(`get ${sessionId}`);
    this._db
      .get(sessionId)
      .then(session => {
        DEBUG && debug(`get success`);
        callback.onGet(
          Cr.NS_OK,
          session.mimeType,
          session.sessionType,
          session.keySetId
        );
      })
      .catch(e => {
        DEBUG && debug(`get error: ${e.message}`);
        callback.onGet(Cr.NS_ERROR_FAILURE);
      });
  },

  remove(sessionId, callback) {
    DEBUG && debug(`remove ${sessionId}`);
    this._db
      .remove(sessionId)
      .then(result => {
        DEBUG && debug(`remove success`);
        callback.onRemove(Cr.NS_OK);
      })
      .catch(e => {
        DEBUG && debug(`remove error: ${e.message}`);
        callback.onRemove(Cr.NS_ERROR_FAILURE);
      });
  },

  clear(callback) {
    DEBUG && debug(`clear`);
    this._db
      .clear()
      .then(result => {
        DEBUG && debug(`clear success`);
        callback.onClear(Cr.NS_OK);
      })
      .catch(e => {
        DEBUG && debug(`clear error: ${e.message}`);
        callback.onClear(Cr.NS_ERROR_FAILURE);
      });
  },

  contractID: "@mozilla.org/gonkdrm/storage;1",

  classID: Components.ID("{45a21db7-187a-404d-b61d-e3b2ef8ca6f7}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIGonkDrmStorage]),
};
