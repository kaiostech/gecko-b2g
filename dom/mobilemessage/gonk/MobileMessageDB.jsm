/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is documented in JSDoc format. To generate the document:
 *
 * 1. Follow the instruction of JSDoc project to install it. See
 *    https://github.com/jsdoc3/jsdoc for details.
 *
 * 2. Since JSDoc does not recognize ES6 syntax and XPCOM components, you should
 *    enable the "commentsOnly" plugin in your conf.json to strip all code out
 *    before generating the document. You'll need to change source.includePattern
 *    as well to include "*.jsm" since it's not included by default. Here's a
 *    minimal example of conf.json you need:
 *
 *    {
 *      "source": {
 *        "includePattern": ".+\\.js(m)?$"
 *      },
 *      "plugins": ["plugins/commentsOnly"]
 *    }
 *
 * 3. Run jsdoc:
 *
 *    $ jsdoc -c <path-to-conf-json> -d <output-directory> MobileMessageDB.jsm
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL", function() {
  let obj = ChromeUtils.import("resource://gre/modules/ril_consts.js");
  return obj;
});

XPCOMUtils.defineLazyGetter(lazy, "SIM", function() {
  return ChromeUtils.import("resource://gre/modules/simIOHelper.js");
});

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gPhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm",
  "PhoneNumberUtils"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gContactsManager",
  "@mozilla.org/sidl-native/contacts;1",
  "nsIContactsManager"
);

const RIL_GETMESSAGESCURSOR_CID = Components.ID(
  "{484d1ad8-840e-4782-9dc4-9ebc4d914937}"
);
const RIL_GETTHREADSCURSOR_CID = Components.ID(
  "{95ee7c3e-d6f2-4ec4-ade5-0c453c036d35}"
);

const DISABLE_MMS_GROUPING_FOR_RECEIVING = false;

const DB_VERSION = 1;

/**
 * @typedef {string} MobileMessageDB.MESSAGE_STORE_NAME
 *
 * The name of the object store for messages.
 */
const MESSAGE_STORE_NAME = "sms";

/**
 * @typedef {string} MobileMessageDB.THREAD_STORE_NAME
 *
 * The name of the object store for threads.
 */
const THREAD_STORE_NAME = "thread";

/**
 * @typedef {string} MobileMessageDB.PARTICIPANT_STORE_NAME
 *
 * The name of the object store for participants.
 */
const PARTICIPANT_STORE_NAME = "participant";

/**
 * @typedef {string} MobileMessageDB.SMS_SEGMENT_STORE_NAME
 *
 * The name of the object store for incoming SMS segments.
 */
const SMS_SEGMENT_STORE_NAME = "sms-segment";

/**
 * @typedef {string} MobileMessageDB.CELLBROADCAST_STORE_NAME
 *
 * The name of the object store for cellbroadcast message.
 */
const CELLBROADCAST_STORE_NAME = "cellbroadcast";

const DELIVERY_SENDING = "sending";
const DELIVERY_SENT = "sent";
const DELIVERY_RECEIVED = "received";
const DELIVERY_NOT_DOWNLOADED = "not-downloaded";
const DELIVERY_ERROR = "error";

const DELIVERY_STATUS_NOT_APPLICABLE = "not-applicable";
const DELIVERY_STATUS_SUCCESS = "success";
const DELIVERY_STATUS_PENDING = "pending";
const DELIVERY_STATUS_ERROR = "error";

const MESSAGE_CLASS_NORMAL = "normal";

const ATTACHMENT_STATUS_NONE = "none";
const ATTACHMENT_STATUS_NOT_DOWNLOADED = "not-downloaded";
const ATTACHMENT_STATUS_DOWNLOADED = "downloaded";

// We can´t create an IDBKeyCursor with a boolean, so we need to use numbers
// instead.
const FILTER_READ_UNREAD = 0;
const FILTER_READ_READ = 1;

const READ_ONLY = "readonly";
const READ_WRITE = "readwrite";
const PREV = "prev";
const NEXT = "next";

const COLLECT_ID_END = 0;
const COLLECT_ID_ERROR = -1;
const COLLECT_TIMESTAMP_UNUSED = 0;

// Default value for integer preference "dom.sms.maxReadAheadEntries".
const DEFAULT_READ_AHEAD_ENTRIES = 7;

//FIXME
//const DEVICE_CAPABILITY_CONTROL = "device.capability.parental-control";

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileMessageService",
  "@mozilla.org/mobilemessage/mobilemessageservice;1",
  "nsIMobileMessageService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMMSService",
  "@mozilla.org/mms/gonkmmsservice;1",
  "nsIMmsService"
);

XPCOMUtils.defineLazyGetter(lazy, "MMS", function() {
  let MMS = ChromeUtils.import("resource://gre/modules/MmsPduHelper.jsm");
  return MMS;
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;

/**
 * @typedef {Object} MobileMessageDB.MessageRecord
 *
 * Represents a SMS or MMS message.
 *
 * <pre>
 * +--------------------------------------------------------------------------+
 * | MessageRecord                                                            |
 * +--------------------------------------------------------------------------+
 * | id: Number (primary-key)                                                 |
 * |                                                                          |
 * | [SMS / MMS Common]                                                       |
 * | type: String                                                             |
 * | read: Number // Works as boolean, only use 0 or 1.                       |
 * | iccId: String                                                            |
 * | sender: String                                                           |
 * | delivery: String                                                         |
 * | timestamp: Number                                                        |
 * | sentTimestamp: Number                                                    |
 * |                                                                          |
 * | [Database Foreign Keys]                                                  |
 * | threadId: Number                                                         |
 * |                                                                          |
 * | [Common Indices]                                                         |
 * | threadIdIndex: Array // [threadId, timestamp]                            |
 * | deliveryIndex: Array // [delivery, timestamp]                            |
 * | readIndex: Array // [read, timpstamp]                                    |
 * | participantIdsIndex: Array of Array // [[participantId, timestamp], ...] |
 * |                                                                          |
 * | [SMS / Common Fields]                                                    |
 * | pid: Number                                                              |
 * | SMSC: String                                                             |
 * | receiver: String                                                         |
 * | encoding: Number                                                         |
 * | messageType: Number                                                      |
 * | teleservice: Number                                                      |
 * | messageClass: String                                                     |
 * | deliveryStatus: String                                                   |
 * | deliveryTimestamp: Number                                                |
 * |                                                                          |
 * | [SMS / Application Port Info]                                            |
 * | originatorPort: Number                                                   |
 * | destinationPort: Number                                                  |
 * |                                                                          |
 * | [SMS / MWI status]                                                       |
 * | mwiPresent: Boolean                                                      |
 * | mwiDiscard: Boolean                                                      |
 * | mwiMsgCount: Number                                                      |
 * | mwiActive: Boolean                                                       |
 * |                                                                          |
 * | [SMS / Message Body]                                                     |
 * | data: Array of Uint8 (available if it's 8bit encoding)                   |
 * | body: String (normal text body)                                          |
 * | fullBody: String                                                         |
 * |                                                                          |
 * | [SMS / CDMA Cellbroadcast Related]                                       |
 * | serviceCategory: Number                                                  |
 * | language: String                                                         |
 * |                                                                          |
 * | [MMS Info]                                                               |
 * | receivers: Array of String                                               |
 * | phoneNumber: String                                                      |
 * | transactionIdIndex: String                                               |
 * | envelopeIdIndex: String                                                  |
 * | isReadReportSent: Boolean                                                |
 * | deliveryInfo: Array of {                                                 |
 * |   receiver: String                                                       |
 * |   deliveryStatus: String                                                 |
 * |   deliveryTimestamp: Number                                              |
 * |   readStatus: String                                                     |
 * |   readTimestamp: Number                                                  |
 * | }                                                                        |
 * | headers: {                                                               |
 * |   x-mms-message-type: Number                                             |
 * |   x-mms-transaction-id: String                                           |
 * |   x-mms-mms-version: Number                                              |
 * |   from: {                                                                |
 * |     address: String                                                      |
 * |     type: String                                                         |
 * |   }                                                                      |
 * |   subject: String                                                        |
 * |   x-mms-message-class: String                                            |
 * |   x-mms-message-size: Number                                             |
 * |   x-mms-expiry: Number                                                   |
 * |   x-mms-content-location: {                                              |
 * |     uri: String                                                          |
 * |   }                                                                      |
 * |   to: Array of {                                                         |
 * |     address: String                                                      |
 * |     type: String                                                         |
 * |   }                                                                      |
 * |   x-mms-read-report: Boolean                                             |
 * |   x-mms-priority: Number                                                 |
 * |   message-id: String                                                     |
 * |   date: String                                                           |
 * |   x-mms-delivery-report: Boolean                                         |
 * |   content-type: {                                                        |
 * |     media: String                                                        |
 * |     params: {                                                            |
 * |       type: String                                                       |
 * |       start: String                                                      |
 * |     }                                                                    |
 * |   }                                                                      |
 * | }                                                                        |
 * | parts: Array of {                                                        |
 * |    index: Number                                                         |
 * |    headers: {                                                            |
 * |      content-type: {                                                     |
 * |        media: String                                                     |
 * |        params: {                                                         |
 * |          name: String                                                    |
 * |          charset: {                                                      |
 * |            charset: String                                               |
 * |          }                                                               |
 * |        }                                                                 |
 * |      content-length: Number                                              |
 * |      content-location: String                                            |
 * |      content-id: String                                                  |
 * |    }                                                                     |
 * |    content: String                                                       |
 * | }                                                                        |
 * +--------------------------------------------------------------------------+
 * </pre>
 */

/**
 * @typedef {Object} MobileMessageDB.ThreadRecord
 *
 * Represents a message thread.
 *
 * <pre>
 * +---------------------------------------+
 * | ThreadRecord                          |
 * +---------------------------------------+
 * | id: Number (primary-key)              |
 * | participantIds: Array of Number       |
 * | participantAddresses: Array of String |
 * | lastMessageId: Number                 |
 * | lastTimestamp: Number                 |
 * | unreadCount: Number                   |
 * | lastMessageType: String               |
 * |                                       |
 * | [SMS Only]                            |
 * | body: String                          |
 * |                                       |
 * | [MMS Only]                            |
 * | lastMessageSubject: String            |
 * | lastMessageAttachementStatus: String  |
 * +---------------------------------------+
 * </pre>
 */

/**
 * @typedef {Object} MobileMessageDB.ParticipantRecord
 *
 * Represents the mapping of a participant and one or multiple addresses.
 * (National and Int'l numbers)
 *
 * <pre>
 * +----------------------------+
 * | ParticipantRecord          |
 * +----------------------------+
 * | id: Number (primary-key)   |
 * | addresses: Array of String |
 * +----------------------------+
 * </pre>
 */

/**
 * @typedef {Object} MobileMessageDB.SmsSegmentRecord
 *
 * Represents a SMS segment.
 *
 * <pre>
 * +---------------------------------------------------------------+
 * | SmsSegmentRecord                                              |
 * +---------------------------------------------------------------+
 * | [Common Fields in SMS segment]                                |
 * | messageType: Number                                           |
 * | teleservice: Number                                           |
 * | SMSC: String                                                  |
 * | sentTimestamp: Number                                         |
 * | timestamp: Number                                             |
 * | sender: String                                                |
 * | pid: Number                                                   |
 * | encoding: Number                                              |
 * | messageClass: String                                          |
 * | iccId: String                                                 |
 * |                                                               |
 * | [Concatenation Info]                                          |
 * | segmentRef: Number                                            |
 * | segmentSeq: Number                                            |
 * | segmentMaxSeq: Number                                         |
 * |                                                               |
 * | [Application Port Info]                                       |
 * | originatorPort: Number                                        |
 * | destinationPort: Number                                       |
 * |                                                               |
 * | [MWI Status]                                                  |
 * | mwiPresent: Boolean                                           |
 * | mwiDiscard: Boolean                                           |
 * | mwiMsgCount: Number                                           |
 * | mwiActive: Boolean                                            |
 * |                                                               |
 * | [CDMA Cell Broadcast Related Fields]                          |
 * | serviceCategory: Number                                       |
 * | language: String                                              |
 * |                                                               |
 * | [Message Body]                                                |
 * | data: Array of Uint8 (available if it's 8bit encoding)        |
 * | body: String (normal text body)                               |
 * |                                                               |
 * | [Handy Fields Created by DB for Concatenation]                |
 * | id: Number (primary-key)                                      |
 * | hash: String // Use to identify the segments to the same SMS. |
 * | receivedSegments: Number                                      |
 * | segments: Array                                               |
 * +---------------------------------------------------------------+
 * </pre>
 */

/**
 * @typedef {Object} MobileMessageDB.CellbroadcastRecord
 *
 * Represents a cellbroadcast message
 *
 * <pre>
 * +---------------------------------------------------------------+
 * | CellbroadcastRecord                                           |
 * +---------------------------------------------------------------+
 * | id: Number (primary-key)                                      |
 * | serialNumber: Number                                          |
 * | messageIdentifier: Number                                     |
 * | geographicalScope: Number                                     |
 * | plmn: Number                                                  |
 * | lac: Number                                                   |
 * | cid: Number                                                   |
 * | messageCode: Number                                           |
 * | language: String                                              |
 * | timestamp: Number                                             |
 * | hash: String // Use serialNumber and messageIdentifier        |
 * |                                                               |
 * | [Message Body]                                                |
 * | data: Array of Uint8 (available if it's 8bit encoding)        |
 * | body: String                                                  |
 * |                                                               |
 * | [ETWS]                                                        |
 * | warningType: Number                                           |
 * |                                                               |
 * | [WAC]                                                         |
 * | geometries: {                                                 |
 * |   polygon: {                                                  |
 * |     type: Number                                              |
 * |     vertices: Array of {                                      |
 * |       latlng: {                                               |
 * |         lat: Number                                           |
 * |         lng: Number                                           |
 * |       }                                                       |
 * |     }                                                         |
 * |     scaledVertices: Array of {                                |
 * |       point: {                                                |
 * |         x: Number                                             |
 * |         y: Number                                             |
 * |       }                                                       |
 * |     }                                                         |
 * |     origin: {                                                 |
 * |       lat: Number                                             |
 * |       lng: Number                                             |
 * |     }                                                         |
 * |   }                                                           |
 * |   circle: {                                                   |
 * |     type: Number                                              |
 * |     center: {                                                 |
 * |       lat: Number                                             |
 * |       lng: Number                                             |
 * |     }                                                         |
 * |     radius: Number                                            |
 * |   }                                                           |
 * | }                                                             |
 * | maximumWaitingTimeSec: Number                                 |
 * +---------------------------------------------------------------+
 * </pre>
 */

/**
 * @class MobileMessageDB
 * @classdesc
 *
 * <p>
 * MobileMessageDB is used to store all SMS / MMS messages, as well as the
 * threads those messages belong to, and the participants of those messages.
 * </p>
 *
 * <p>
 * The relations between threads, messages and participants can be described as
 * the following ERD -- each thread consists of one or many messages, and
 * consists of one or many participants. A participant resolves to one or many
 * (usually up to 2) addresses -- which represent different formats of the same
 * address, for example a national number and an international number.
 * </p>
 *
 * <pre>
 *                      X
 *                     / \
 * +-----------+      /   \       +-----------+
 * |           |     /     \     /|           |
 * |  thread   |-|--|consist|--|--|participant|
 * |           |     \  of /     \|           |
 * +-----------+      \   /       +-----------+
 *       |             \ /              |
 *       -              V               -
 *       |                              |
 *       |                              |
 *       X                              X
 *      / \                            / \
 *     /   \                          /   \
 *    /     \                        /     \
 *   |consist|                      |resolve|
 *    \  of /                        \  to /
 *     \   /                          \   /
 *      \ /                            \ /
 *       V                              V
 *       |                              |
 *       |                              |
 *       -                              -
 *       |                              |
 *      /|\                            /|\
 * +-----------+                  +-----------+
 * |           |                  |           |
 * |  message  |                  |  address  |
 * |           |                  |           |
 * +-----------+                  +-----------+
 * </pre>
 *
 * <p>
 * There are 4 object stores in use: </br>
 * 1. MESSAGE_STORE: stores {@link MobileMessageDB.MessageRecord}. </br>
 * 2. THREAD_STORE: stores {@link MobileMessageDB.ThreadRecord}. </br>
 * 3. PARTICIPANT_STORE: stores {@link MobileMessageDB.ParticipantRecord}. </br>
 * 4. SMS_SEGMENT_STORE: stores partial incoming SMS segments defined in
 * {@link MobileMessageDB.SmsSegmentRecord}. The records are deleted as soon as
 * it's enough to compose a complete SMS message.
 * </p>
 *
 * <p>
 * Besides all object stores mentioned above, there was a MOST_RECENT_STORE
 * which is deprecated and no longer in use.
 * </p>
 */
const MobileMessageDB = function() {
  this._updateDebugFlag();
};
MobileMessageDB.prototype = {
  dbName: null,
  dbVersion: null,

  /**
   * Cache the DB instance.
   *
   * @member {IDBDatabase} MobileMessageDB.db
   * @private
   */
  db: null,

  /**
   * Last sms/mms object store key value in the database.
   *
   * @member {number} MobileMessageDB.lastMessageId
   * @private
   */
  lastMessageId: 0,

  _updateDebugFlag() {
    try {
      DEBUG =
        RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  getAttachmentStatus(aMessageRecord) {
    if (aMessageRecord && aMessageRecord.type == "mms") {
      if (aMessageRecord.delivery == DELIVERY_NOT_DOWNLOADED) {
        return ATTACHMENT_STATUS_NOT_DOWNLOADED;
      }
      let parts = aMessageRecord.parts;
      if (!parts) {
        return ATTACHMENT_STATUS_NONE;
      }
      for (let i = 0; i < parts.length; i++) {
        let part = parts[i];
        if (!part) {
          continue;
        }
        let partHeaders = part.headers;
        if (
          partHeaders["content-type"].media != "application/smil" &&
          partHeaders["content-type"].media != "text/plain"
        ) {
          return ATTACHMENT_STATUS_DOWNLOADED;
        }
      }
    }
    return ATTACHMENT_STATUS_NONE;
  },

  /**
   * @callback MobileMessageDB.EnsureDBCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>null</code> on success.
   * @param {IDBDatabase} aDatabase
   *        The ready-to-use database object on success.
   */

  /**
   * Prepare the database. This may include opening the database and upgrading
   * it to the latest schema version.
   *
   * @function MobileMessageDB.ensureDB
   * @param {MobileMessageDB.EnsureDBCallback} callback
   *        Function that takes an error and db argument. It is called when
   *        the database is ready to use or if an error occurs while preparing
   *        the database.
   */
  ensureDB(callback) {
    if (this.db) {
      if (DEBUG) {
        debug("ensureDB: already have a database, returning early.");
      }
      callback(null, this.db);
      return;
    }

    let self = this;
    function gotDB(db) {
      self.db = db;
      callback(null, db);
    }

    let request = indexedDB.open(this.dbName, this.dbVersion);
    request.onsuccess = function(event) {
      if (DEBUG) {
        debug("Opened database:", self.dbName, self.dbVersion);
      }
      gotDB(event.target.result);
    };
    request.onupgradeneeded = function(event) {
      if (DEBUG) {
        debug(
          "Database needs upgrade:",
          self.dbName,
          event.oldVersion,
          event.newVersion
        );
        debug(
          "Correct new database version:",
          event.newVersion == self.dbVersion
        );
      }

      let db = event.target.result;

      let currentVersion = event.oldVersion;

      function update(currentVersion) {
        if (currentVersion >= self.dbVersion) {
          if (DEBUG) {
            debug("Upgrade finished.");
          }
          return;
        }

        let next = update.bind(self, currentVersion + 1);
        if (DEBUG) {
          debug("Upgrade to version " + (currentVersion + 1));
        }
        switch (currentVersion) {
          case 0:
            self.createSchema(db, next);
            break;
          default:
            event.target.transaction.abort();
            if (DEBUG) {
              debug("unexpected db version: " + event.oldVersion);
            }
            callback(Cr.NS_ERROR_FAILURE, null);
            break;
        }
      }

      update(currentVersion);
    };
    request.onerror = function(event) {
      // TODO look at event.target.Code and change error constant accordingly.
      if (DEBUG) {
        debug("Error opening database!");
      }
      callback(Cr.NS_ERROR_FAILURE, null);
    };
    request.onblocked = function(event) {
      if (DEBUG) {
        debug("Opening database request is blocked.");
      }
      callback(Cr.NS_ERROR_FAILURE, null);
    };
  },

  /**
   * @callback MobileMessageDB.NewTxnCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>null</code> on success.
   * @param {IDBTransaction} aTransaction
   *        The transaction object to operate the indexedDB on success.
   * @param {IDBObjectStore|IDBObjectStore[]} aObjectStores
   *        The object store(s) on success. If only one object store is passed,
   *        it's passed as an <code>IDBObjectStore</code>; Otherwise, it's
   *        <code>IDBObjectStore[]</code>.
   */

  /**
   * Start a new transaction.
   *
   * @function MobileMessageDB.newTxn
   * @param {string} txn_type
   *        Type of transaction (e.g. READ_WRITE)
   * @param {MobileMessageDB.NewTxnCallback} callback
   *        Function to call when the transaction is available. It will
   *        be invoked with the transaction and opened object stores.
   * @param {string[]} [storeNames=[{@link MobileMessageDB.MESSAGE_STORE_NAME}]]
   *        Names of the stores to open.
   */
  newTxn(txn_type, callback, storeNames) {
    if (!storeNames) {
      storeNames = [MESSAGE_STORE_NAME];
    }
    if (DEBUG) {
      debug("Opening transaction for object stores: " + storeNames);
    }
    this.ensureDB(function(error, db) {
      if (error) {
        if (DEBUG) {
          debug("Could not open database: " + error);
        }
        callback(error);
        return;
      }
      let txn = db.transaction(storeNames, txn_type);
      if (DEBUG) {
        debug("Started transaction " + txn + " of type " + txn_type);
      }
      if (DEBUG) {
        txn.oncomplete = function(event) {
          debug("Transaction " + txn + " completed.");
        };
        txn.onerror = function(event) {
          // TODO check event.target.error.name and show an appropiate error
          // message according to it.
          debug(
            "Error occurred during transaction: " + event.target.error.name
          );
        };
      }
      let stores;
      if (storeNames.length == 1) {
        if (DEBUG) {
          debug("Retrieving object store " + storeNames[0]);
        }
        stores = txn.objectStore(storeNames[0]);
      } else {
        stores = [];
        for (let storeName of storeNames) {
          if (DEBUG) {
            debug("Retrieving object store " + storeName);
          }
          stores.push(txn.objectStore(storeName));
        }
      }
      callback(null, txn, stores);
    });
  },

  /**
   * @callback MobileMessageDB.InitCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>null</code> on success.
   */

  /**
   * Initialize this MobileMessageDB.
   *
   * @function MobileMessageDB.init
   * @param {string} aDbName
   *        A string name for that database.
   * @param {number} aDbVersion
   *        The version that mmdb should upgrade to. 0 for the latest version.
   * @param {MobileMessageDB.InitCallback} aCallback
   *        A function when either the initialization transaction is completed
   *        or any error occurs.  Should take only one argument -- null when
   *        initialized with success or the error object otherwise.
   */
  init(aDbName, aDbVersion, aCallback) {
    this.dbName = aDbName;
    this.dbVersion = aDbVersion || DB_VERSION;
    if (DEBUG) {
      debug("MobileMessageDB init");
    }
    let self = this;
    this.newTxn(READ_ONLY, function(error, txn, messageStore) {
      if (error) {
        if (aCallback) {
          aCallback(error);
        }
        return;
      }

      if (aCallback) {
        txn.oncomplete = function() {
          aCallback(null);
        };
      }

      // In order to get the highest key value, we open a key cursor in reverse
      // order and get only the first pointed value.
      let request = messageStore.openCursor(null, PREV);
      request.onsuccess = function(event) {
        let cursor = event.target.result;
        if (!cursor) {
          if (DEBUG) {
            debug(
              "Could not get the last key from mobile message database. " +
                "Probably empty database"
            );
          }
          return;
        }
        self.lastMessageId = cursor.key || 0;
        if (DEBUG) {
          debug("Last assigned message ID was " + self.lastMessageId);
        }
      };
      request.onerror = function(event) {
        if (DEBUG) {
          debug(
            "Could not get the last key from mobile message database " +
              event.target.error.name
          );
        }
      };
    });
  },

  /**
   * Close the MobileMessageDB.
   *
   * @function MobileMessageDB.close
   */
  close() {
    if (!this.db) {
      return;
    }

    this.db.close();
    this.db = null;
    this.lastMessageId = 0;
  },

  /**
   * Sometimes user might reboot or remove battery while sending/receiving
   * message. This function set the status of message records to error. The
   * function can be used as the callback of {@link MobileMessageDB.init}.
   *
   * @function MobileMessageDB.updatePendingTransactionToError
   * @param {number} aError
   *        The function does nothing if <code>aError</code> is not
   *        <code>null</code>.
   */
  updatePendingTransactionToError(aError) {
    if (aError) {
      return;
    }

    this.newTxn(READ_WRITE, function(error, txn, messageStore) {
      if (error) {
        return;
      }

      let deliveryIndex = messageStore.index("delivery");

      // Set all 'delivery: sending' records to 'delivery: error' and 'deliveryStatus:
      // error'.
      let keyRange = IDBKeyRange.bound(
        [DELIVERY_SENDING, 0],
        [DELIVERY_SENDING, ""]
      );
      let cursorRequestSending = deliveryIndex.openCursor(keyRange);
      cursorRequestSending.onsuccess = function(event) {
        let messageCursor = event.target.result;
        if (!messageCursor) {
          return;
        }

        let messageRecord = messageCursor.value;

        // Set delivery to error.
        if (DEBUG) {
          debug(
            "updatePendingTransactionToError: update message " +
              messageRecord.id +
              " to " +
              DELIVERY_ERROR
          );
        }
        messageRecord.delivery = DELIVERY_ERROR;
        messageRecord.deliveryIndex = [DELIVERY_ERROR, messageRecord.timestamp];

        if (messageRecord.type == "sms") {
          messageRecord.deliveryStatus = DELIVERY_STATUS_ERROR;
        } else {
          // Set delivery status to error.
          for (let i = 0; i < messageRecord.deliveryInfo.length; i++) {
            if (DEBUG) {
              debug(
                "updatePendingTransactionToError: update message " +
                  messageRecord.id +
                  " deliveryStatus from " +
                  messageRecord.deliveryInfo[i].deliveryStatus +
                  " to " +
                  DELIVERY_STATUS_ERROR
              );
            }
            messageRecord.deliveryInfo[
              i
            ].deliveryStatus = DELIVERY_STATUS_ERROR;
          }
        }

        messageCursor.update(messageRecord);
        messageCursor.continue();
      };

      // Set all 'delivery: not-downloaded' and 'deliveryStatus: pending'
      // records to 'delivery: not-downloaded' and 'deliveryStatus: error'.
      keyRange = IDBKeyRange.bound(
        [DELIVERY_NOT_DOWNLOADED, 0],
        [DELIVERY_NOT_DOWNLOADED, ""]
      );
      let cursorRequestNotDownloaded = deliveryIndex.openCursor(keyRange);
      cursorRequestNotDownloaded.onsuccess = function(event) {
        let messageCursor = event.target.result;
        if (!messageCursor) {
          return;
        }

        let messageRecord = messageCursor.value;

        // We have no "not-downloaded" SMS messages.
        if (messageRecord.type == "sms") {
          messageCursor.continue();
          return;
        }

        // Set delivery status to error.
        let deliveryInfo = messageRecord.deliveryInfo;
        if (
          deliveryInfo.length == 1 &&
          deliveryInfo[0].deliveryStatus == DELIVERY_STATUS_PENDING
        ) {
          deliveryInfo[0].deliveryStatus = DELIVERY_STATUS_ERROR;
        }

        messageCursor.update(messageRecord);
        messageCursor.continue();
      };
    });
  },

  /**
   * Create the initial database schema.
   *
   * TODO need to worry about number normalization somewhere...
   * TODO full text search on body???
   */
  createSchema(db, next) {
    // This messageStore holds the main mobile message data.
    let messageStore = db.createObjectStore(MESSAGE_STORE_NAME, {
      keyPath: "id",
    });
    messageStore.createIndex("timestamp", "timestamp", { unique: false });
    messageStore.createIndex("delivery", "deliveryIndex");
    messageStore.createIndex("read", "readIndex");
    messageStore.createIndex("threadId", "threadIdIndex");
    messageStore.createIndex("participantIds", "participantIdsIndex", {
      multiEntry: true,
    });
    messageStore.createIndex("transactionId", "transactionIdIndex", {
      unique: true,
    });
    messageStore.createIndex("envelopeId", "envelopeIdIndex", { unique: true });

    if (DEBUG) {
      debug("Created object stores and indexes");
    }

    let participantStore = db.createObjectStore(PARTICIPANT_STORE_NAME, {
      keyPath: "id",
      autoIncrement: true,
    });
    participantStore.createIndex("addresses", "addresses", {
      multiEntry: true,
    });

    let threadStore = db.createObjectStore(THREAD_STORE_NAME, {
      keyPath: "id",
      autoIncrement: true,
    });
    threadStore.createIndex("participantIds", "participantIds");
    threadStore.createIndex("lastTimestamp", "lastTimestamp");

    let smsSegmentStore = db.createObjectStore(SMS_SEGMENT_STORE_NAME, {
      keyPath: "id",
      autoIncrement: true,
    });
    smsSegmentStore.createIndex("hash", "hash", { unique: true });

    let cellBroadcastStore = db.createObjectStore(CELLBROADCAST_STORE_NAME, {
      keyPath: "id",
      autoIncrement: true,
    });
    cellBroadcastStore.createIndex("hash", "hash", { unique: true });
    next();
  },

  /**
   * Check if <code>addr1</code> matches <code>addr2</code>.
   *
   * @function MobileMessageDB.matchParsedPhoneNumbers
   * @param {string} addr1
   *        Normalized address 1.
   * @param {Object} parsedAddr1
   *        Parsed address 1.
   * @param {string} addr2
   *        Normalized address 2.
   * @param {Object} parsedAddr2
   *        Parsed address 2.
   * @return {boolean}
   *         <code>true</code> if the 2 addresses match.
   */
  matchParsedPhoneNumbers(addr1, parsedAddr1, addr2, parsedAddr2) {
    if (
      (parsedAddr1.internationalNumber &&
        parsedAddr1.internationalNumber === parsedAddr2.internationalNumber) ||
      (parsedAddr1.nationalNumber &&
        parsedAddr1.nationalNumber === parsedAddr2.nationalNumber)
    ) {
      return true;
    }

    if (parsedAddr1.countryName != parsedAddr2.countryName) {
      return false;
    }

    let ssPref = "dom.phonenumber.substringmatching." + parsedAddr1.countryName;
    if (Services.prefs.getPrefType(ssPref) != Ci.nsIPrefBranch.PREF_INT) {
      return false;
    }

    let val = Services.prefs.getIntPref(ssPref);
    return (
      addr1.length > val &&
      addr2.length > val &&
      addr1.slice(-val) === addr2.slice(-val)
    );
  },

  /**
   * Generate a <code>nsISmsMessage</code> or
   * <code>nsIMmsMessage</code> instance from a stored message record.
   *
   * @function MobileMessageDB.createDomMessageFromRecord
   * @param {MobileMessageDB.MessageRecord} aMessageRecord
   *        The stored message record.
   * @return {nsISmsMessage|nsIMmsMessage}
   */

  createDomMessageFromRecord(aMessageRecord) {
    if (DEBUG) {
      debug("createDomMessageFromRecord: " + JSON.stringify(aMessageRecord));
    }
    if (aMessageRecord.type == "sms") {
      return lazy.gMobileMessageService.createSmsMessage(
        aMessageRecord.id,
        aMessageRecord.threadId,
        aMessageRecord.iccId,
        aMessageRecord.delivery,
        aMessageRecord.deliveryStatus,
        aMessageRecord.sender,
        aMessageRecord.receiver,
        aMessageRecord.body,
        aMessageRecord.messageClass,
        aMessageRecord.timestamp,
        aMessageRecord.sentTimestamp,
        aMessageRecord.deliveryTimestamp,
        aMessageRecord.read
      );
    } else if (aMessageRecord.type == "mms") {
      let headers = aMessageRecord.headers;
      if (DEBUG) {
        debug("MMS: headers: " + JSON.stringify(headers));
      }

      let subject = headers.subject;
      if (subject == undefined) {
        subject = "";
      }

      let smil = "";
      let attachments = [];
      let parts = aMessageRecord.parts;
      if (parts) {
        for (let i = 0; i < parts.length; i++) {
          let part = parts[i];
          if (DEBUG) {
            debug("MMS: part[" + i + "]: " + JSON.stringify(part));
          }
          // Sometimes the part is incomplete because the device reboots when
          // downloading MMS. Don't need to expose this part to the content.
          if (!part) {
            continue;
          }

          let partHeaders = part.headers;
          let partContent = part.content;
          // Don't need to make the SMIL part if it's present.
          if (partHeaders["content-type"].media == "application/smil") {
            smil = partContent;
            continue;
          }
          attachments.push({
            id: partHeaders["content-id"],
            location: partHeaders["content-location"],
            content: partContent,
          });
        }
      }
      let expiryDate = 0;
      if (headers["x-mms-expiry"] != undefined) {
        expiryDate = aMessageRecord.timestamp + headers["x-mms-expiry"] * 1000;
      }
      let readReportRequested = headers["x-mms-read-report"] || false;
      let isGroup = aMessageRecord.isGroup || false;
      return lazy.gMobileMessageService.createMmsMessage(
        aMessageRecord.id,
        aMessageRecord.threadId,
        aMessageRecord.iccId,
        aMessageRecord.delivery,
        aMessageRecord.deliveryInfo,
        aMessageRecord.sender,
        aMessageRecord.receivers,
        aMessageRecord.timestamp,
        aMessageRecord.sentTimestamp,
        aMessageRecord.read,
        subject,
        smil,
        attachments,
        expiryDate,
        readReportRequested,
        isGroup
      );
    }
    if (DEBUG) {
      debug("This should not be reached");
    }
    return null;
  },

  /**
   * @callback MobileMessageDB.ParticipantRecordCallback
   * @param {MobileMessageDB.ParticipantRecord} aParticipantRecord
   *        The stored participant record.
   */

  /**
   * Create a participant record with the given addresses, and add it into the
   * participant object store immediately.
   *
   * @function MobileMessageDB.createParticipantRecord
   * @param {IDBObjectStore} aParticipantStore
   *        Object store for participants.
   * @param {string[]} aAddresses
   *        The addresses associated to the participant.
   * @param {MobileMessageDB.ParticipantRecordCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  createParticipantRecord(aParticipantStore, aAddresses, aCallback) {
    let participantRecord = { addresses: aAddresses };
    let addRequest = aParticipantStore.add(participantRecord);
    addRequest.onsuccess = function(event) {
      participantRecord.id = event.target.result;
      if (DEBUG) {
        debug("createParticipantRecord: " + JSON.stringify(participantRecord));
      }
      aCallback(participantRecord);
    };
  },

  /**
   * Find or create the participant record associated to the given PLMN address.
   *
   * @function MobileMessageDB.findParticipantRecordByPlmnAddress
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {string} aAddress
   *        The PLMN address to look up with.
   * @param {boolean} aCreate
   *        <code>true</code> to create a new participant record if not exists
   *        yet, otherwise return <code>null</code> to the callback if record
   *        not found.
   * @param {MobileMessageDB.ParticipantRecordCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  findParticipantRecordByPlmnAddress(
    aParticipantStore,
    aAddress,
    aCreate,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "findParticipantRecordByPlmnAddress(" +
          JSON.stringify(aAddress) +
          ", " +
          aCreate +
          ")"
      );
    }

    // Two types of input number to match here, international(+886987654321),
    // and local(0987654321) types. The "nationalNumber" parsed from
    // phonenumberutils will be "987654321" in this case.

    // Normalize address before searching for participant record.
    let normalizedAddress = lazy.gPhoneNumberUtils.normalize(aAddress, false);
    let allPossibleAddresses = [normalizedAddress];

    if (DEBUG) {
      debug(
        "findParticipantRecordByPlmnAddress: allPossibleAddresses = " +
          JSON.stringify(allPossibleAddresses)
      );
    }

    // Make a copy here because we may need allPossibleAddresses again.
    let needles = allPossibleAddresses.slice(0);
    let request = aParticipantStore.index("addresses").get(needles.pop());
    request.onsuccess = function onsuccess(event) {
      let participantRecord = event.target.result;
      // 1) First try matching through "addresses" index of participant store.
      //    If we're lucky, return the fetched participant record.
      if (participantRecord) {
        if (DEBUG) {
          debug(
            "findParticipantRecordByPlmnAddress: got " +
              JSON.stringify(participantRecord)
          );
        }
        aCallback(participantRecord);
        return;
      }

      // Try next possible address again.
      if (needles.length) {
        let request = aParticipantStore.index("addresses").get(needles.pop());
        request.onsuccess = onsuccess.bind(this);
        return;
      }

      // 2) Traverse throught all participants and check all alias addresses.
      aParticipantStore.openCursor().onsuccess = function(event) {
        let cursor = event.target.result;
        if (!cursor) {
          // Have traversed whole object store but still in vain.
          if (!aCreate) {
            aCallback(null);
            return;
          }

          this.createParticipantRecord(
            aParticipantStore,
            [normalizedAddress],
            aCallback
          );
          return;
        }

        let participantRecord = cursor.value;
        for (let storedAddress of participantRecord.addresses) {
          let match = lazy.gPhoneNumberUtils.match(
            normalizedAddress,
            storedAddress
          );
          if (!match) {
            // 3) Else we fail to match current stored participant record.
            continue;
          }
          // Match!
          if (aCreate) {
            // In a READ-WRITE transaction, append one more possible address for
            // this participant record.
            participantRecord.addresses = participantRecord.addresses.concat(
              allPossibleAddresses
            );
            cursor.update(participantRecord);
          }

          if (DEBUG) {
            debug(
              "findParticipantRecordByPlmnAddress: match " +
                JSON.stringify(cursor.value)
            );
          }
          aCallback(participantRecord);
          return;
        }

        // Check next participant record if available.
        cursor.continue();
      }.bind(this);
    }.bind(this);
  },

  /**
   * Find or create the participant record associated to the given address other
   * than PLMN address.
   *
   * @function MobileMessageDB.findParticipantRecordByOtherAddress
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {string} aAddress
   *        The address to look up with.
   * @param {boolean} aCreate
   *        <code>true</code> to create a new participant record if not exists
   *        yet, otherwise return <code>null</code> to the callback if record
   *        not found.
   * @param {MobileMessageDB.ParticipantRecordCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  findParticipantRecordByOtherAddress(
    aParticipantStore,
    aAddress,
    aCreate,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "findParticipantRecordByOtherAddress(" +
          JSON.stringify(aAddress) +
          ", " +
          aCreate +
          ")"
      );
    }

    // Go full match.
    let request = aParticipantStore.index("addresses").get(aAddress);
    request.onsuccess = function(event) {
      let participantRecord = event.target.result;
      if (participantRecord) {
        if (DEBUG) {
          debug(
            "findParticipantRecordByOtherAddress: got " +
              JSON.stringify(participantRecord)
          );
        }
        aCallback(participantRecord);
        return;
      }
      if (aCreate) {
        this.createParticipantRecord(aParticipantStore, [aAddress], aCallback);
        return;
      }
      aCallback(null);
    }.bind(this);
  },

  /**
   * @typedef {Object} MobileMessageDB.TypedAddress
   * @property {string} address Address
   * @property {string} type Type of the address, such as "PLMN", "IPv4",
   *           "IPv6", "email" or "Others"
   */

  /**
   * Find or create the participant record associated to the given address.
   *
   * @function MobileMessageDB.findParticipantRecordByTypedAddress
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {MobileMessageDB.TypedAddress} aTypedAddress
   *        The address to look up with.
   * @param {boolean} aCreate
   *        <code>true</code> to create a new participant record if not exists
   *        yet, otherwise return <code>null</code> to the callback if record
   *        not found.
   * @param {MobileMessageDB.ParticipantRecordCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  findParticipantRecordByTypedAddress(
    aParticipantStore,
    aTypedAddress,
    aCreate,
    aCallback
  ) {
    if (aTypedAddress.type == "PLMN") {
      this.findParticipantRecordByPlmnAddress(
        aParticipantStore,
        aTypedAddress.address,
        aCreate,
        aCallback
      );
    } else {
      this.findParticipantRecordByOtherAddress(
        aParticipantStore,
        aTypedAddress.address,
        aCreate,
        aCallback
      );
    }
  },

  /**
   * @callback MobileMessageDB.ParticipantIdsCallback
   * @param {number[]} aParticipantIds
   *        An array of participant IDs. May be <code>null</code>.
   */

  /**
   * Find or create participant records associated to the given addresses, and
   * return the IDs of the participant records to the caller through the
   * callback.
   *
   * @function MobileMessageDB.findParticipantIdsByTypedAddresses
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {MobileMessageDB.TypedAddress[]} aTypedAddresses
   *        Addresses to look up with.
   * @param {boolean} aCreate
   *        <code>true</code> to create a new participant record associates to
   *        the given addresses if not exists yet, otherwise return
   *        <code>null</code> to the callback if no record found.
   * @param {boolean} aSkipNonexistent
   *        <code>true</code> to skip the addresses not exist in the participant
   *        store, otherwise return <code>null</code> to the callback when one
   *        or more addresses not found.
   * @param {MobileMessageDB.ParticipantIdsCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  findParticipantIdsByTypedAddresses(
    aParticipantStore,
    aTypedAddresses,
    aCreate,
    aSkipNonexistent,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "findParticipantIdsByTypedAddresses(" +
          JSON.stringify(aTypedAddresses) +
          ", " +
          aCreate +
          ", " +
          aSkipNonexistent +
          ")"
      );
    }

    if (!aTypedAddresses || !aTypedAddresses.length) {
      if (DEBUG) {
        debug("findParticipantIdsByTypedAddresses: returning null");
      }
      aCallback(null);
      return;
    }

    let self = this;
    (function findParticipantId(index, result) {
      if (index >= aTypedAddresses.length) {
        // Sort numerically.
        result.sort(function(a, b) {
          return a - b;
        });
        if (DEBUG) {
          debug("findParticipantIdsByTypedAddresses: returning " + result);
        }
        aCallback(result);
        return;
      }

      self.findParticipantRecordByTypedAddress(
        aParticipantStore,
        aTypedAddresses[index++],
        aCreate,
        function(participantRecord) {
          if (!participantRecord) {
            if (!aSkipNonexistent) {
              if (DEBUG) {
                debug("findParticipantIdsByTypedAddresses: returning null");
              }
              aCallback(null);
              return;
            }
          } else if (!result.includes(participantRecord.id)) {
            result.push(participantRecord.id);
          }
          findParticipantId(index, result);
        }
      );
    })(0, []);
  },

  /**
   * @callback MobileMessageDB.ThreadRecordCallback
   * @param {MobileMessageDB.ThreadRecord} aThreadRecord
   *        The stored thread record.
   * @param {number[]} aParticipantIds
   *        IDs of participants of the thread.
   */

  /**
   * Find the thread record associated to the given address.
   *
   * @function MobileMessageDB.findThreadRecordByTypedAddresses
   * @param {IDBObjectStore} aThreadStore
   *        The object store for threads.
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {MobileMessageDB.TypedAddress[]} aTypedAddresses
   *        Addresses to look up with.
   * @param {boolean} aCreateParticipants
   *        <code>true</code> to create participant record associated to the
   *        addresses if not exist yet.
   * @param {MobileMessageDB.ThreadRecordCallback} aCallback
   *        The callback function to invoke when the request finishes.
   */
  findThreadRecordByTypedAddresses(
    aThreadStore,
    aParticipantStore,
    aTypedAddresses,
    aCreateParticipants,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "findThreadRecordByTypedAddresses(" +
          JSON.stringify(aTypedAddresses) +
          ", " +
          aCreateParticipants +
          ")"
      );
    }
    this.findParticipantIdsByTypedAddresses(
      aParticipantStore,
      aTypedAddresses,
      aCreateParticipants,
      false,
      function(participantIds) {
        if (!participantIds) {
          if (DEBUG) {
            debug("findThreadRecordByTypedAddresses: returning null");
          }
          aCallback(null, null);
          return;
        }
        // Find record from thread store.
        let request = aThreadStore.index("participantIds").get(participantIds);
        request.onsuccess = function(event) {
          let threadRecord = event.target.result;
          if (DEBUG) {
            debug(
              "findThreadRecordByTypedAddresses: return " +
                JSON.stringify(threadRecord)
            );
          }
          aCallback(threadRecord, participantIds);
        };
      }
    );
  },

  /**
   * @callback MobileMessageDB.TransactionResultCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>NS_OK</code> on success.
   * @param {nsISmsMessage|nsIMmsMessage} aDomMessage
   *        The DOM message instance of the transaction result.
   */

  /**
   * @callback MobileMessageDB.NewTxnWithCallbackRequestCallback
   * @param {Object} aCapture
   *        An output parameter. The <code>messageRecord</code> property will be
   *        set on transaction finishes.
   * @param {MobileMessageDB.MessageRecord} aCapture.messageRecord
   *        The stored message record. The property presents if the transaction
   *        finished successfully.
   * @param {IDBObjectStore|IDBObjectStore[]} aObjectStores
   *        The object store(s) on success. If only one object store is passed,
   *        it's passed as an <code>IDBObjectStore</code>; Otherwise, it's
   *        <code>IDBObjectStore[]</code>.
   */

  /**
   * Start a new transaction with default <code>oncomplete</code> /
   * <code>onabort</code> implementation on the <code>IDBTransaction</code>
   * object which redirects the error / result to <code>aCallback</code>.
   *
   * @function MobileMessageDB.newTxnWithCallback
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.TransactionResultCallback} aCallback.notify
   *        The callback function to invoke when the transaction finishes.
   * @param {MobileMessageDB.NewTxnWithCallbackRequestCallback} aFunc
   *        The callback function to invoke when the request finishes.
   * @param {string[]} [aStoreNames=[{@link MobileMessageDB.MESSAGE_STORE_NAME}]]
   *        Names of the stores to open.
   */
  newTxnWithCallback(aCallback, aFunc, aStoreNames) {
    let self = this;
    this.newTxn(
      READ_WRITE,
      function(aError, aTransaction, aStores) {
        let notifyResult = function(aRv, aMessageRecord) {
          if (!aCallback) {
            return;
          }
          let domMessage =
            aMessageRecord && self.createDomMessageFromRecord(aMessageRecord);
          aCallback.notify(aRv, domMessage);
        };

        if (aError) {
          notifyResult(aError, null);
          return;
        }

        let capture = {};
        aTransaction.oncomplete = function(event) {
          notifyResult(Cr.NS_OK, capture.messageRecord);
        };
        aTransaction.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Cr.NS_ERROR_FILE_NO_DEVICE_SPACE
              : Cr.NS_ERROR_FAILURE;
          notifyResult(error, null);
        };

        aFunc(capture, aStores);
      },
      aStoreNames
    );
  },

  /**
   * Save a message record.
   *
   * @function MobileMessageDB.saveRecord
   * @param {MobileMessageDB.MessageRecord} aMessageRecord
   *        Message record to store.
   * @param {MobileMessageDB.TypedAddress[]} aThreadParticipants
   *        Participants of the thread of the message.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.TransactionResultCallback} aCallback.notify
   *        The callback function to invoke when the transaction finishes.
   */
  saveRecord(aMessageRecord, aThreadParticipants, aCallback) {
    if (DEBUG) {
      debug("Going to store " + JSON.stringify(aMessageRecord));
    }

    let self = this;
    this.newTxn(
      READ_WRITE,
      function(error, txn, stores) {
        let notifyResult = function(aRv, aMessageRecord) {
          if (!aCallback) {
            return;
          }
          let domMessage =
            aMessageRecord && self.createDomMessageFromRecord(aMessageRecord);
          aCallback.notify(aRv, domMessage);
        };

        if (error) {
          notifyResult(error, aMessageRecord);
          return;
        }

        let deletedInfo = { messageIds: [], threadIds: [] };

        txn.oncomplete = function(event) {
          if (aMessageRecord.id > self.lastMessageId) {
            self.lastMessageId = aMessageRecord.id;
          }

          notifyResult(Cr.NS_OK, aMessageRecord);
          self.notifyDeletedInfo(deletedInfo);
        };
        txn.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Cr.NS_ERROR_FILE_NO_DEVICE_SPACE
              : Cr.NS_ERROR_FAILURE;

          notifyResult(error, aMessageRecord);
        };

        let messageStore = stores[0];
        let participantStore = stores[1];
        let threadStore = stores[2];
        self.replaceShortMessageOnSave(
          txn,
          messageStore,
          participantStore,
          threadStore,
          aMessageRecord,
          aThreadParticipants,
          deletedInfo
        );
      },
      [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME, THREAD_STORE_NAME]
    );
  },

  /**
   * @typedef {Object} MobileMessageDB.DeletedInfo
   * @property {number[]} messageIds
   *           IDs of deleted messages.
   * @property {number[]} threadIds
   *           IDs of deleted threads, which indicates all messages within the
   *           threads have been deleted.
   */

  /**
   * According to <i>3GPP 23.040 - subclause 9.2.3.9 TP-Protocol-Identifier (TP-PID)</i>,
   * if the Protocol Identifier contains a <i>Replace Short Message Type</i> or
   * <i>Return Call Message</i> code, it should replace any existing stored
   * message having the same Protocol Identifier code and originating address.
   *
   * This function checks the Protocol Identifier before saving the message
   * record to fulfill the feature.
   *
   * @function MobileMessageDB.replaceShortMessageOnSave
   * @param {IDBTransaction} aTransaction
   *        The transaction object.
   * @param {IDBObjectStore} aMessageStore
   *        The object store for messages.
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {IDBObjectStore} aThreadStore
   *        The object store for threads.
   * @param {MobileMessageDB.MessageRecord} aMessageRecord
   *        The message record to store.
   * @param {MobileMessageDB.TypedAddress[]} aThreadParticipants
   *        Participants of the thread of the message.
   * @param {MobileMessageDB.DeletedInfo} aDeletedInfo
   *        An out parameter indicating which messages have been deleted due to
   *        the replacement.
   */
  replaceShortMessageOnSave(
    aTransaction,
    aMessageStore,
    aParticipantStore,
    aThreadStore,
    aMessageRecord,
    aThreadParticipants,
    aDeletedInfo
  ) {
    let isReplaceTypePid =
      aMessageRecord.pid &&
      ((aMessageRecord.pid >= lazy.RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_1 &&
        aMessageRecord.pid <= lazy.RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_7) ||
        aMessageRecord.pid == lazy.RIL.PDU_PID_RETURN_CALL_MESSAGE);

    if (
      aMessageRecord.type != "sms" ||
      aMessageRecord.delivery != DELIVERY_RECEIVED ||
      !isReplaceTypePid
    ) {
      this.realSaveRecord(
        aTransaction,
        aMessageStore,
        aParticipantStore,
        aThreadStore,
        aMessageRecord,
        aThreadParticipants,
        aDeletedInfo
      );
      return;
    }

    // 3GPP TS 23.040 subclause 9.2.3.9 "TP-Protocol-Identifier (TP-PID)":
    //
    //   ... the MS shall check the originating address and replace any
    //   existing stored message having the same Protocol Identifier code
    //   and originating address with the new short message and other
    //   parameter values. If there is no message to be replaced, the MS
    //   shall store the message in the normal way. ... it is recommended
    //   that the SC address should not be checked by the MS."
    let self = this;
    let typedSender = {
      address: aMessageRecord.sender,
      type: lazy.MMS.Address.resolveType(aMessageRecord.sender),
    };
    this.findParticipantRecordByTypedAddress(
      aParticipantStore,
      typedSender,
      false,
      function(participantRecord) {
        if (!participantRecord) {
          self.realSaveRecord(
            aTransaction,
            aMessageStore,
            aParticipantStore,
            aThreadStore,
            aMessageRecord,
            aThreadParticipants,
            aDeletedInfo
          );
          return;
        }

        let participantId = participantRecord.id;
        let range = IDBKeyRange.bound([participantId, 0], [participantId, ""]);
        let request = aMessageStore.index("participantIds").openCursor(range);
        request.onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor) {
            self.realSaveRecord(
              aTransaction,
              aMessageStore,
              aParticipantStore,
              aThreadStore,
              aMessageRecord,
              aThreadParticipants,
              aDeletedInfo
            );
            return;
          }

          // A message record with same participantId found.
          // Verify matching criteria.
          let foundMessageRecord = cursor.value;
          if (
            foundMessageRecord.type != "sms" ||
            foundMessageRecord.sender != aMessageRecord.sender ||
            foundMessageRecord.pid != aMessageRecord.pid
          ) {
            cursor.continue();
            return;
          }

          // Match! Now replace that found message record with current one.
          aMessageRecord.id = foundMessageRecord.id;
          self.realSaveRecord(
            aTransaction,
            aMessageStore,
            aParticipantStore,
            aThreadStore,
            aMessageRecord,
            aThreadParticipants,
            aDeletedInfo
          );
        };
      }
    );
  },

  /**
   * The function where object store manipulations actually occur.
   *
   * @function MobileMessageDB.realSaveRecord
   * @param {IDBTransaction} aTransaction
   *        The transaction object.
   * @param {IDBObjectStore} aMessageStore
   *        The object store for messages.
   * @param {IDBObjectStore} aParticipantStore
   *        The object store for participants.
   * @param {IDBObjectStore} aThreadStore
   *        The object store for threads.
   * @param {MobileMessageDB.MessageRecord} aMessageRecord
   *        The message record to store.
   * @param {MobileMessageDB.TypedAddress[]} aThreadParticipants
   *        Participants of the thread of the message.
   * @param {MobileMessageDB.DeletedInfo} aDeletedInfo
   *        An out parameter indicating which messages have been deleted due to
   *        the replacement.
   */
  realSaveRecord(
    aTransaction,
    aMessageStore,
    aParticipantStore,
    aThreadStore,
    aMessageRecord,
    aThreadParticipants,
    aDeletedInfo
  ) {
    let self = this;

    this.findThreadRecordByTypedAddresses(
      aThreadStore,
      aParticipantStore,
      aThreadParticipants,
      true,
      function(threadRecord, participantIds) {
        if (!participantIds) {
          aTransaction.abort();
          return;
        }

        let isOverriding = aMessageRecord.id !== undefined;
        if (!isOverriding) {
          // |self.lastMessageId| is only updated in |txn.oncomplete|.
          aMessageRecord.id = self.lastMessageId + 1;
        }

        let timestamp = aMessageRecord.timestamp;
        let insertMessageRecord = function(threadId) {
          // Setup threadId & threadIdIndex.
          aMessageRecord.threadId = threadId;
          aMessageRecord.threadIdIndex = [threadId, timestamp];
          // Setup participantIdsIndex.
          aMessageRecord.participantIdsIndex = [];
          for (let id of participantIds) {
            aMessageRecord.participantIdsIndex.push([id, timestamp]);
          }

          if (!isOverriding) {
            // Really add to message store.
            aMessageStore.put(aMessageRecord);
            return;
          }

          // If we're going to override an old message, we need to update the
          // info of the original thread containing the overridden message.
          // To get the original thread ID and read status of the overridden
          // message record, we need to retrieve it before overriding it.
          aMessageStore.get(aMessageRecord.id).onsuccess = function(event) {
            let oldMessageRecord = event.target.result;
            aMessageStore.put(aMessageRecord);
            if (oldMessageRecord) {
              self.updateThreadByMessageChange(
                aMessageStore,
                aThreadStore,
                oldMessageRecord.threadId,
                [aMessageRecord.id],
                oldMessageRecord.read ? 0 : 1,
                aDeletedInfo
              );
            }
          };
        };

        if (threadRecord) {
          let needsUpdate = false;

          if (threadRecord.lastTimestamp <= timestamp) {
            let lastMessageSubject;
            if (aMessageRecord.type == "mms") {
              lastMessageSubject = aMessageRecord.headers.subject;
            }
            threadRecord.lastMessageSubject = lastMessageSubject || null;
            threadRecord.lastTimestamp = timestamp;
            threadRecord.body = aMessageRecord.body;
            threadRecord.lastMessageId = aMessageRecord.id;
            threadRecord.lastMessageType = aMessageRecord.type;
            threadRecord.lastMessageAttachementStatus = self.getAttachmentStatus(
              aMessageRecord
            );
            needsUpdate = true;
          }

          if (!aMessageRecord.read) {
            threadRecord.unreadCount++;
            needsUpdate = true;
          }

          if (needsUpdate) {
            threadRecord.isGroup = aMessageRecord.isGroup || false;
            aThreadStore.put(threadRecord);
          }

          insertMessageRecord(threadRecord.id);
          return;
        }

        let lastMessageSubject;
        if (aMessageRecord.type == "mms") {
          lastMessageSubject = aMessageRecord.headers.subject;
        }
        let lastMessageAttachementStatus = self.getAttachmentStatus(
          aMessageRecord
        );
        threadRecord = {
          participantIds,
          participantAddresses: aThreadParticipants.map(function(typedAddress) {
            return typedAddress.address;
          }),
          lastMessageId: aMessageRecord.id,
          lastTimestamp: timestamp,
          lastMessageSubject: lastMessageSubject || null,
          lastMessageAttachementStatus,
          body: aMessageRecord.body,
          unreadCount: aMessageRecord.read ? 0 : 1,
          lastMessageType: aMessageRecord.type,
          isGroup: aMessageRecord.isGroup,
        };
        aThreadStore.add(threadRecord).onsuccess = function(event) {
          let threadId = event.target.result;
          insertMessageRecord(threadId);
        };
      }
    );
  },

  /**
   * @typedef {Object} MobileMessageDB.MmsDeliveryInfoElement
   * @property {string} receiver
   * @property {string} deliveryStatus
   * @property {number} deliveryTimestamp
   * @property {string} readStatus
   * @property {number} readTimestamp
   */

  /**
   * @callback MobileMessageDB.ForEachMatchedMmsDeliveryInfoCallback
   * @param {MobileMessageDB.MmsDeliveryInfoElement} aElement
   *        An element of the MMS <code>deliverInfo</code> of a message record.
   */

  /**
   * Iterate all elements of <code>aDeliveryInfo</code>, check if the receiver
   * address matches <code>aNeedle</code> and invoke <code>aCallback</code> on
   * each matched element.
   *
   * @function MobileMessageDB.forEachMatchedMmsDeliveryInfo
   * @param {MobileMessageDB.MmsDeliveryInfoElement[]} aDeliveryInfo
   *        The MMS <code>deliverInfo</code> of a message record.
   * @param {string} aNeedle
   *        The receiver address to look up with.
   * @param {MobileMessageDB.ForEachMatchedMmsDeliveryInfoCallback} aCallback
   *        The callback function to invoke on each match.
   */
  forEachMatchedMmsDeliveryInfo(aDeliveryInfo, aNeedle, aCallback) {
    let typedAddress = {
      type: lazy.MMS.Address.resolveType(aNeedle),
      address: aNeedle,
    };
    let normalizedAddress;
    if (typedAddress.type === "PLMN") {
      normalizedAddress = lazy.gPhoneNumberUtils.normalize(aNeedle, false);
    }

    for (let element of aDeliveryInfo) {
      let typedStoredAddress = {
        type: lazy.MMS.Address.resolveType(element.receiver),
        address: element.receiver,
      };
      if (typedAddress.type !== typedStoredAddress.type) {
        // Not even my type.  Skip.
        continue;
      }

      if (typedAddress.address == typedStoredAddress.address) {
        // Have a direct match.
        aCallback(element);
        continue;
      }

      if (typedAddress.type !== "PLMN") {
        // Address type other than "PLMN" must have direct match.  Or, skip.
        continue;
      }

      // Both are of "PLMN" type.
      let normalizedStoredAddress = lazy.gPhoneNumberUtils.normalize(
        element.receiver,
        false
      );

      if (
        lazy.gPhoneNumberUtils.match(normalizedAddress, normalizedStoredAddress)
      ) {
        aCallback(element);
      }
    }
  },

  /**
   * Find the message of a given message ID or envelope ID. Update its
   * <code>delivery</code>, <code>deliveryStatus</code>, and
   * <code>envelopeId</code> accordingly.
   *
   * @function MobileMessageDB.updateMessageDeliveryById
   * @param {string} id
   *        If <code>type</code> is "messageId", it represents the message ID;
   *        If <code>type</code> is "envelopeId", it represents the envelope ID,
   *        which is the "x-mms-transaction-id" in the header of an MMS message.
   * @param {string} type
   *        Either "messageId" or "envelopeId".
   * @param {string} receiver
   *        The receiver address.
   * @param {string} delivery
   *        If given, it will be used to update the <code>deliveryIndex</code>
   *        property of a stored message record.
   * @param {string} deliveryStatus
   *        If given, it will be used to update the <code>deliveryStatus</code>
   *        property of a stored message record.
   * @param {string} envelopeId
   *        If given, it will be used to update the <code>envelopeIdIndex</code>
   *        property of a stored message record.
   * @param {Object} callback
   *        The object passed as <code>aCallback</code> to
   *        {@link MobileMessageDB.newTxnWithCallback}.
   */
  updateMessageDeliveryById(
    id,
    type,
    receiver,
    delivery,
    deliveryStatus,
    envelopeId,
    callback
  ) {
    if (DEBUG) {
      debug(
        "Setting message's delivery by " +
          type +
          " = " +
          id +
          " receiver: " +
          receiver +
          " delivery: " +
          delivery +
          " deliveryStatus: " +
          deliveryStatus +
          " envelopeId: " +
          envelopeId
      );
    }

    let self = this;
    this.newTxnWithCallback(callback, function(aCapture, aMessageStore) {
      let getRequest;
      if (type === "messageId") {
        getRequest = aMessageStore.get(id);
      } else if (type === "envelopeId") {
        getRequest = aMessageStore.index("envelopeId").get(id);
      }

      getRequest.onsuccess = function(event) {
        let messageRecord = event.target.result;
        if (!messageRecord) {
          if (DEBUG) {
            debug("type = " + id + " is not found");
          }
          throw Components.Exception("", Cr.NS_ERROR_FAILURE);
        }

        let isRecordUpdated = false;

        // Update |messageRecord.delivery| if needed.
        if (delivery && messageRecord.delivery != delivery) {
          messageRecord.delivery = delivery;
          messageRecord.deliveryIndex = [delivery, messageRecord.timestamp];
          isRecordUpdated = true;

          // When updating an message's delivey state to 'sent', we also update
          // its |sentTimestamp| by the current device timestamp to represent
          // when the message is successfully sent.
          if (delivery == DELIVERY_SENT) {
            messageRecord.sentTimestamp = Date.now();
          }
        }

        // Attempt to update |deliveryStatus| and |deliveryTimestamp| of:
        // - the |messageRecord| for SMS.
        // - the element(s) in |messageRecord.deliveryInfo| for MMS.
        if (deliveryStatus) {
          // A callback for updating the deliveyStatus/deliveryTimestamp of
          // each target.
          let updateFunc = function(aTarget) {
            if (aTarget.deliveryStatus == deliveryStatus) {
              return;
            }

            aTarget.deliveryStatus = deliveryStatus;

            // Update |deliveryTimestamp| if it's successfully delivered.
            if (deliveryStatus == DELIVERY_STATUS_SUCCESS) {
              aTarget.deliveryTimestamp = Date.now();
            }

            isRecordUpdated = true;
          };

          if (messageRecord.type == "sms") {
            updateFunc(messageRecord);
          } else if (messageRecord.type == "mms") {
            if (!receiver) {
              // If the receiver is specified, we only need to update the
              // element(s) in deliveryInfo that match the same receiver.
              messageRecord.deliveryInfo.forEach(updateFunc);
            } else {
              self.forEachMatchedMmsDeliveryInfo(
                messageRecord.deliveryInfo,
                receiver,
                updateFunc
              );
            }
          }
        }

        // Update |messageRecord.envelopeIdIndex| if needed.
        if (envelopeId) {
          if (messageRecord.envelopeIdIndex != envelopeId) {
            messageRecord.envelopeIdIndex = envelopeId;
            isRecordUpdated = true;
          }
        }

        aCapture.messageRecord = messageRecord;
        if (!isRecordUpdated) {
          if (DEBUG) {
            debug(
              "The values of delivery, deliveryStatus and envelopeId " +
                "don't need to be updated."
            );
          }
          return;
        }

        if (DEBUG) {
          debug("The delivery, deliveryStatus or envelopeId are updated.");
        }
        aMessageStore.put(messageRecord);
      };
    });
  },

  /**
   * Map receivers of a MMS message record to the thread participant list if MMS
   * grouping is enabled.
   *
   * @function MobileMessageDB.fillReceivedMmsThreadParticipants
   * @param {MobileMessageDB.MessageRecord} aMessage
   *        The MMS message.
   * @param {MobileMessageDB.TypedAddress[]} threadParticipants
   *        Participants to add.
   */
  fillReceivedMmsThreadParticipants(aMessage, threadParticipants) {
    let receivers = aMessage.receivers;
    // If we don't want to disable the MMS grouping for receiving, we need to
    // add the receivers (excluding the user's own number) to the participants
    // for creating the thread. Some cases might be investigated as below:
    //
    // 1. receivers.length == 0
    //    This usually happens when receiving an MMS notification indication
    //    which doesn't carry any receivers.
    // 2. receivers.length == 1
    //    If the receivers contain single phone number, we don't need to
    //    add it into participants because we know that number is our own.
    // 3. receivers.length >= 2
    //    If the receivers contain multiple phone numbers, we need to add all
    //    of them but not the user's own number into participants.
    if (DISABLE_MMS_GROUPING_FOR_RECEIVING || receivers.length < 2) {
      return;
    }
    let isSuccess = false;
    let slicedReceivers = receivers.slice();
    if (aMessage.phoneNumber) {
      var normalizedAddress = lazy.gPhoneNumberUtils.normalize(
        aMessage.phoneNumber,
        false
      );

      [aMessage.phoneNumber, normalizedAddress].forEach(function(item) {
        let foundIndex = -1;
        for (var i = 0; i < slicedReceivers.length; i++) {
          if (
            lazy.gPhoneNumberUtils.match(
              slicedReceivers[i],
              aMessage.phoneNumber
            )
          ) {
            isSuccess = true;
            foundIndex = i;
            break;
          }
        }
        if (foundIndex != -1) {
          slicedReceivers.splice(foundIndex, 1);
        }
      });
    }

    if (!isSuccess) {
      // For some SIMs we cannot retrieve the valid MSISDN (i.e. the user's
      // own phone number), so we cannot correctly exclude the user's own
      // number from the receivers, thus wrongly building the thread index.
      if (DEBUG) {
        debug("Error! Cannot strip out user's own phone number!");
      }
    }

    aMessage.receivers = slicedReceivers;
    slicedReceivers.forEach(function(aAddress) {
      threadParticipants.push({
        address: aAddress,
        type: lazy.MMS.Address.resolveType(aAddress),
      });
    });
  },

  /**
   * Update the thread when one or more messages are deleted / replaced.
   *
   * @function MobileMessageDB.updateThreadByMessageChange
   * @param {IDBObjectStore} messageStore
   *        The object store for messages.
   * @param {IDBObjectStore} threadStore
   *        The object store for threads.
   * @param {number} threadId
   *        The thread ID.
   * @param {number[]} removedMsgIds
   *        The IDs of removed messages.
   * @param {number} ignoredUnreadCount
   *        Negative offset for <code>unreadCount</code>. For example, if the
   *        <code>unreadCount</code> was 5, given
   *        <code>ignoredUnreadCount</code> to 3 causes <code>unreadCount</code>
   *        becomes 2.
   * @param {MobileMessageDB.DeletedInfo} deletedInfo
   *        An out parameter indicating if the thread is deleted after the
   *        operation.
   */
  updateThreadByMessageChange(
    messageStore,
    threadStore,
    threadId,
    removedMsgIds,
    ignoredUnreadCount,
    deletedInfo
  ) {
    let self = this;
    threadStore.get(threadId).onsuccess = function(event) {
      // This must exist.
      let threadRecord = event.target.result;
      if (DEBUG) {
        debug("Updating thread record " + JSON.stringify(threadRecord));
      }

      if (ignoredUnreadCount > 0) {
        if (DEBUG) {
          debug(
            "Updating unread count : " +
              threadRecord.unreadCount +
              " -> " +
              (threadRecord.unreadCount - ignoredUnreadCount)
          );
        }
        threadRecord.unreadCount -= ignoredUnreadCount;
      }

      if (removedMsgIds.includes(threadRecord.lastMessageId)) {
        if (DEBUG) {
          debug("MRU entry was deleted.");
        }
        // Check most recent sender/receiver.
        let range = IDBKeyRange.bound([threadId, 0], [threadId, ""]);
        let request = messageStore.index("threadId").openCursor(range, PREV);
        request.onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor) {
            if (DEBUG) {
              debug("All messages were deleted. Delete this thread.");
            }
            threadStore.delete(threadId);
            if (deletedInfo) {
              deletedInfo.threadIds.push(threadId);
            }
            return;
          }

          let nextMsg = cursor.value;
          let lastMessageSubject;
          if (nextMsg.type == "mms") {
            lastMessageSubject = nextMsg.headers.subject;
          }
          threadRecord.lastMessageSubject = lastMessageSubject || null;
          threadRecord.lastMessageAttachementStatus = self.getAttachmentStatus(
            nextMsg
          );
          threadRecord.lastMessageId = nextMsg.id;
          threadRecord.lastTimestamp = nextMsg.timestamp;
          threadRecord.body = nextMsg.body;
          threadRecord.lastMessageType = nextMsg.type;
          if (DEBUG) {
            debug("Updating mru entry: " + JSON.stringify(threadRecord));
          }
          threadStore.put(threadRecord);
        };
      } else if (ignoredUnreadCount > 0) {
        if (DEBUG) {
          debug("Shortcut, just update the unread count.");
        }
        threadStore.put(threadRecord);
      }
    };
  },

  /**
   * Notify the observers that one or more messages are deleted.
   *
   * @function MobileMessageDB.notifyDeletedInfo
   * @param {MobileMessageDB.DeletedInfo} info
   *        The IDs of deleted messages and threads.
   */
  notifyDeletedInfo(info) {
    if (
      !info ||
      (info.messageIds.length === 0 && info.threadIds.length === 0)
    ) {
      return;
    }

    let deletedInfo = lazy.gMobileMessageService.createDeletedMessageInfo(
      info.messageIds,
      info.messageIds.length,
      info.threadIds,
      info.threadIds.length
    );
    Services.obs.notifyObservers(deletedInfo, "sms-deleted");
  },

  /**
   * nsIGonkMobileMessageDatabaseService API
   */

  /**
   * Store an incoming message.
   *
   * @function MobileMessageDB.saveReceivedMessage
   * @param {MobileMessageDB.MessageRecord} aMessage
   *        The message record to store.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.TransactionResultCallback} aCallback.notify
   *        The callback function to invoke when the transaction finishes.
   */
  saveReceivedMessage(aMessage, aCallback) {
    let self = this;

    if (
      (aMessage.type != "sms" && aMessage.type != "mms") ||
      (aMessage.type == "sms" &&
        (aMessage.messageClass == undefined || aMessage.sender == undefined)) ||
      (aMessage.type == "mms" &&
        (aMessage.delivery == undefined ||
          aMessage.deliveryStatus == undefined ||
          !Array.isArray(aMessage.receivers))) ||
      aMessage.timestamp == undefined
    ) {
      if (aCallback) {
        let domMessage = aMessage && self.createDomMessageFromRecord(aMessage);
        aCallback.notify(Cr.NS_ERROR_FAILURE, domMessage);
      }
      return;
    }

    let threadParticipants;
    if (aMessage.type == "mms") {
      if (aMessage.headers.from) {
        aMessage.sender = aMessage.headers.from.address;
      } else if (aMessage.sourceAddress) {
        aMessage.sender = aMessage.sourceAddress;
      } else {
        aMessage.sender = "";
      }

      threadParticipants = [
        {
          address: aMessage.sender,
          type: lazy.MMS.Address.resolveType(aMessage.sender),
        },
      ];
      this.fillReceivedMmsThreadParticipants(aMessage, threadParticipants);
    } else {
      // SMS
      threadParticipants = [
        {
          address: aMessage.sender,
          type: lazy.MMS.Address.resolveType(aMessage.sender),
        },
      ];
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.readIndex = [FILTER_READ_UNREAD, timestamp];
    aMessage.read = FILTER_READ_UNREAD;

    // If |sentTimestamp| is not specified, use 0 as default.
    if (aMessage.sentTimestamp == undefined) {
      aMessage.sentTimestamp = 0;
    }

    if (aMessage.type == "mms") {
      aMessage.transactionIdIndex = aMessage.headers["x-mms-transaction-id"];
      aMessage.isReadReportSent = false;

      // As a receiver, we don't need to care about the delivery status of
      // others, so we put a single element with self's phone number in the
      // |deliveryInfo| array.
      aMessage.deliveryInfo = [
        {
          receiver: aMessage.phoneNumber,
          deliveryStatus: aMessage.deliveryStatus,
          deliveryTimestamp: 0,
          readStatus: lazy.MMS.DOM_READ_STATUS_NOT_APPLICABLE,
          readTimestamp: 0,
        },
      ];

      delete aMessage.deliveryStatus;
    }

    if (aMessage.type == "sms") {
      aMessage.delivery = DELIVERY_RECEIVED;
      aMessage.deliveryStatus = DELIVERY_STATUS_SUCCESS;
      aMessage.deliveryTimestamp = 0;

      if (aMessage.pid == undefined) {
        aMessage.pid = lazy.RIL.PDU_PID_DEFAULT;
      }
    }
    aMessage.deliveryIndex = [aMessage.delivery, timestamp];

    if (aMessage.type == "mms" && aMessage.headers.cc) {
      aMessage.isGroup = true;
    }

    function findBlockContactsSuccess(aResult) {
      // For block contact, there are two situations:
      // 1. Sender is blocked, should callback fail ack to SMSC/MMSC and
      //    not save database.
      // 2. Sender is not blocked, should receive and save it normally.
      if (DEBUG) {
        debug("Find block contact successfully");
      }
      if (!isJSONEmpty(aResult)) {
        if (DEBUG) {
          debug("Message blocked by block contact!");
        }
        if (aCallback) {
          let domMessage =
            aMessage && self.createDomMessageFromRecord(aMessage);
          aCallback.notify(Cr.NS_ERROR_FAILURE, domMessage);
        }
      } else {
        self.saveRecord(aMessage, threadParticipants, aCallback);
      }
    }

    function findBlockContactsFail() {
      // Still should receive the message, the find contact interface fail
      // should not effect save message flow.
      if (DEBUG) {
        debug("Find block contact fail, interface error");
      }
      self.saveRecord(aMessage, threadParticipants, aCallback);
    }

    function isJSONEmpty(result) {
      for (let key in result) {
        return false;
      }

      return true;
    }
    //FIXME: parentalControl need tobe implemented
    /*
    function findResultSuccess(results) {
      if (isJSONEmpty(results)) {
        if (DEBUG) {
          debug("The number is non contact, need block");
        }
        if (aCallback) {
          let domMessage =
            aMessage && self.createDomMessageFromRecord(aMessage);
          aCallback.notify(Cr.NS_ERROR_FAILURE, domMessage);
        }
      } else {
        self.contactDB.findBlockedNumbers(findBlockContactsSuccess,
                                          findBlockContactsFail, {
          filterBy: ['number'],
          filterValue: aMessage.sender,
          filterOp: 'fuzzyMatch'
        });
      }
    }

    function findResultFail(error) {
      self.contactDB.findBlockedNumbers(findBlockContactsSuccess,
                                        findBlockContactsFail, {
        filterBy: ['number'],
        filterValue: aMessage.sender,
        filterOp: 'fuzzyMatch'
      });
    }

    // Parent control need block non contact number, it is diferent with normal.
    let parentalControlEnabled = (parentalControlEnabled = Services.prefs.getBoolPref(
      DEVICE_CAPABILITY_CONTROL,
      false
    ));
    */

    lazy.gContactsManager.findBlockedNumbers(
      {
        filterValue: aMessage.sender,
        filterOption: Ci.nsIFilterOption.FuzzyMatch,
      },
      {
        resolve: result => {
          findBlockContactsSuccess(result);
        },
        reject: () => {
          findBlockContactsFail();
        },
      }
    );

    //FIXME: parentalControl needs to be implemented
    /*
    if (parentalControlEnabled) {
      if (DEBUG) debug("Parent control feature work well");
      let emailRegexp = /[\w.+-]+@[\w.-]+\.[a-z]{2,6}/mgi;
      if (emailRegexp.test(aMessage.sender)) {
        this.contactDB.find(findResultSuccess, findResultFail, {
          filterBy: ['email'],
          filterOp: 'equals',
          filterValue: aMessage.sender
        });
      } else {
        this.contactDB.find(findResultSuccess, findResultFail, {
          filterBy: ['tel'],
          filterOp: 'match',
          filterValue: aMessage.sender.replace(/\s+/g, '')
        });
      }
    } else {
      this.contactDB.findBlockedNumbers(findBlockContactsSuccess,
                                        findBlockContactsFail, {
        filterBy: ['number'],
        filterValue: aMessage.sender,
        filterOp: 'fuzzyMatch'
      });
    }
*/
  },

  /**
   * Store an outgoing message.
   *
   * @function MobileMessageDB.saveSendingMessage
   * @param {MobileMessageDB.MessageRecord} aMessage
   *        The message record to store.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.TransactionResultCallback} aCallback.notify
   *        The callback function to invoke when the transaction finishes.
   */
  saveSendingMessage(aMessage, aCallback) {
    let self = this;
    if (
      (aMessage.type != "sms" && aMessage.type != "mms") ||
      (aMessage.type == "sms" && aMessage.receiver == undefined) ||
      (aMessage.type == "mms" && !Array.isArray(aMessage.receivers)) ||
      aMessage.deliveryStatusRequested == undefined ||
      aMessage.timestamp == undefined
    ) {
      if (aCallback) {
        let domMessage = aMessage && self.createDomMessageFromRecord(aMessage);
        aCallback.notify(Cr.NS_ERROR_FAILURE, domMessage);
      }
      return;
    }

    // Set |aMessage.deliveryStatus|. Note that for MMS record
    // it must be an array of strings; For SMS, it's a string.
    let deliveryStatus = aMessage.deliveryStatusRequested
      ? DELIVERY_STATUS_PENDING
      : DELIVERY_STATUS_NOT_APPLICABLE;
    if (aMessage.type == "sms") {
      aMessage.deliveryStatus = deliveryStatus;
      // If |deliveryTimestamp| is not specified, use 0 as default.
      if (aMessage.deliveryTimestamp == undefined) {
        aMessage.deliveryTimestamp = 0;
      }
    } else if (aMessage.type == "mms") {
      let receivers = aMessage.receivers;
      let readStatus = aMessage.headers["x-mms-read-report"]
        ? lazy.MMS.DOM_READ_STATUS_PENDING
        : lazy.MMS.DOM_READ_STATUS_NOT_APPLICABLE;
      aMessage.deliveryInfo = [];
      for (let i = 0; i < receivers.length; i++) {
        aMessage.deliveryInfo.push({
          receiver: receivers[i],
          deliveryStatus,
          deliveryTimestamp: 0,
          readStatus,
          readTimestamp: 0,
        });
      }
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.deliveryIndex = [DELIVERY_SENDING, timestamp];
    aMessage.readIndex = [FILTER_READ_READ, timestamp];
    aMessage.delivery = DELIVERY_SENDING;
    aMessage.messageClass = MESSAGE_CLASS_NORMAL;
    aMessage.read = FILTER_READ_READ;

    // |sentTimestamp| is not available when the message is still sedning.
    aMessage.sentTimestamp = 0;

    let threadParticipants;
    if (aMessage.type == "sms") {
      threadParticipants = [
        {
          address: aMessage.receiver,
          type: lazy.MMS.Address.resolveType(aMessage.receiver),
        },
      ];
    } else if (aMessage.type == "mms") {
      threadParticipants = [];
      for (var i = 0; i < aMessage.headers.to.length; i++) {
        threadParticipants.push(aMessage.headers.to[i]);
      }
      if (aMessage.headers.cc) {
        for (i = 0; i < aMessage.headers.cc.length; i++) {
          threadParticipants.push(aMessage.headers.cc[i]);
        }
      }
    }
    this.saveRecord(aMessage, threadParticipants, aCallback);
  },

  /**
   * Update the <code>delivery</code>, <code>deliveryStatus</code>, and
   * <code>envelopeId</code> of a stored message record matching the given
   * message ID.
   *
   * @function MobileMessageDB.setMessageDeliveryByMessageId
   * @param {number} messageId
   *        The message ID.
   * @param {string} receiver
   *        The receiver address.
   * @param {string} delivery
   *        If given, it will be used to update the <code>deliveryIndex</code>
   *        property of a stored message record.
   * @param {string} deliveryStatus
   *        If given, it will be used to update the <code>deliveryStatus</code>
   *        property of a stored message record.
   * @param {string} envelopeId
   *        If given, it will be used to update the <code>envelopeIdIndex</code>
   *        property of a stored message record.
   * @param {Object} callback
   *        The object passed as <code>aCallback</code> to
   *        {@link MobileMessageDB.newTxnWithCallback}.
   */
  setMessageDeliveryByMessageId(
    messageId,
    receiver,
    delivery,
    deliveryStatus,
    envelopeId,
    callback
  ) {
    this.updateMessageDeliveryById(
      messageId,
      "messageId",
      receiver,
      delivery,
      deliveryStatus,
      envelopeId,
      callback
    );
  },

  /**
   * Update the <code>deliveryStatus</code> of the specified
   * <code>aReceiver</code> within the <code>deliveryInfo</code> of the message
   * record retrieved by the given <code>envelopeId</code>.
   *
   * @function MobileMessageDB.setMessageDeliveryStatusByEnvelopeId
   * @param {string} aEnvelopeId
   *        The envelope ID, which is the "x-mms-transaction-id" in the header
   *        of an MMS message.
   * @param {string} aReceiver
   *        The receiver address.
   * @param {string} aDeliveryStatus
   *        If given, it will be used to update the <code>deliveryStatus</code>
   *        property of a stored message record.
   * @param {Object} aCallback
   *        The object passed as <code>aCallback</code> to
   *        {@link MobileMessageDB.newTxnWithCallback}.
   */
  setMessageDeliveryStatusByEnvelopeId(
    aEnvelopeId,
    aReceiver,
    aDeliveryStatus,
    aCallback
  ) {
    this.updateMessageDeliveryById(
      aEnvelopeId,
      "envelopeId",
      aReceiver,
      null,
      aDeliveryStatus,
      null,
      aCallback
    );
  },

  /**
   * Update the <code>readStatus</code> of the specified <code>aReceiver</code>
   * within the <code>deliveryInfo</code> of the message record retrieved by the
   * given <code>envelopeId</code>.
   *
   * @function MobileMessageDB.setMessageReadStatusByEnvelopeId
   * @param {string} aEnvelopeId
   *        The envelope ID, which is the "x-mms-transaction-id" in the header
   *        of an MMS message.
   * @param {string} aReceiver
   *        The receiver address.
   * @param {string} aReadStatus
   *        The updated read status.
   * @param {Object} aCallback
   *        The object passed as <code>aCallback</code> to
   *        {@link MobileMessageDB.newTxnWithCallback}.
   */
  setMessageReadStatusByEnvelopeId(
    aEnvelopeId,
    aReceiver,
    aReadStatus,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "Setting message's read status by envelopeId = " +
          aEnvelopeId +
          ", receiver: " +
          aReceiver +
          ", readStatus: " +
          aReadStatus
      );
    }

    let self = this;
    this.newTxnWithCallback(aCallback, function(aCapture, aMessageStore) {
      let getRequest = aMessageStore.index("envelopeId").get(aEnvelopeId);
      getRequest.onsuccess = function(event) {
        let messageRecord = event.target.result;
        if (!messageRecord) {
          if (DEBUG) {
            debug("envelopeId '" + aEnvelopeId + "' not found");
          }
          throw Components.Exception("", Cr.NS_ERROR_FAILURE);
        }

        aCapture.messageRecord = messageRecord;

        let isRecordUpdated = false;
        self.forEachMatchedMmsDeliveryInfo(
          messageRecord.deliveryInfo,
          aReceiver,
          function(aEntry) {
            if (aEntry.readStatus == aReadStatus) {
              return;
            }

            aEntry.readStatus = aReadStatus;
            if (aReadStatus == lazy.MMS.DOM_READ_STATUS_SUCCESS) {
              aEntry.readTimestamp = Date.now();
            } else {
              aEntry.readTimestamp = 0;
            }
            isRecordUpdated = true;
          }
        );

        if (!isRecordUpdated) {
          if (DEBUG) {
            debug("The values of readStatus don't need to be updated.");
          }
          return;
        }

        if (DEBUG) {
          debug("The readStatus is updated.");
        }
        aMessageStore.put(messageRecord);
      };
    });
  },

  /**
   * @callback MobileMessageDB.GetMessageRecordCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>NS_OK</code> on success.
   * @param {MobileMessageDB.MessageRecord} aMessageRecord
   *        The stored message record.
   * @param {nsISmsMessage|nsIMmsMessage} aDomMessage
   *        The DOM message instance of the message record.
   */

  /**
   * Get the message record with given transaction ID.
   *
   * @function MobileMessageDB.getMessageRecordByTransactionId
   * @param {string} aTransactionId
   *        The transaction ID.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.GetMessageRecordCallback} aCallback.notify
   *        The callback function to invoke when the request finishes.
   */
  getMessageRecordByTransactionId(aTransactionId, aCallback) {
    if (DEBUG) {
      debug("Retrieving message with transaction ID " + aTransactionId);
    }
    this.newTxn(READ_ONLY, function(error, txn, messageStore) {
      if (error) {
        if (DEBUG) {
          debug(error);
        }
        aCallback.notify(error, null, null);
        return;
      }
      let request = messageStore.index("transactionId").get(aTransactionId);

      txn.oncomplete = function(event) {
        if (DEBUG) {
          debug("Transaction " + txn + " completed.");
        }
        let messageRecord = request.result;
        if (!messageRecord) {
          if (DEBUG) {
            debug("Transaction ID " + aTransactionId + " not found");
          }
          aCallback.notify(Cr.NS_ERROR_FILE_NOT_FOUND, null, null);
          return;
        }
        // In this case, we don't need a dom message. Just pass null to the
        // third argument.
        aCallback.notify(Cr.NS_OK, messageRecord, null);
      };

      txn.onerror = function(event) {
        if (DEBUG) {
          if (event.target) {
            debug("Caught error on transaction", event.target.error.name);
          }
        }
        aCallback.notify(Cr.NS_ERROR_FAILURE, null, null);
      };
    });
  },

  /**
   * Get the message record with given message ID.
   *
   * @function MobileMessageDB.getMessageRecordById
   * @param {string} aMessageID
   *        The message ID.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.GetMessageRecordCallback} aCallback.notify
   *        The callback function to invoke when the request finishes.
   */
  getMessageRecordById(aMessageId, aCallback) {
    if (DEBUG) {
      debug("Retrieving message with ID " + aMessageId);
    }
    let self = this;
    this.newTxn(READ_ONLY, function(error, txn, messageStore) {
      if (error) {
        if (DEBUG) {
          debug(error);
        }
        aCallback.notify(error, null, null);
        return;
      }
      let request = messageStore.mozGetAll(aMessageId);

      txn.oncomplete = function() {
        if (DEBUG) {
          debug("Transaction " + txn + " completed.");
        }
        if (request.result.length > 1) {
          if (DEBUG) {
            debug("Got too many results for id " + aMessageId);
          }
          aCallback.notify(Cr.NS_ERROR_UNEXPECTED, null, null);
          return;
        }
        let messageRecord = request.result[0];
        if (!messageRecord) {
          if (DEBUG) {
            debug("Message ID " + aMessageId + " not found");
          }
          aCallback.notify(Cr.NS_ERROR_FILE_NOT_FOUND, null, null);
          return;
        }
        if (messageRecord.id != aMessageId) {
          if (DEBUG) {
            debug(
              "Requested message ID (" +
                aMessageId +
                ") is " +
                "different from the one we got"
            );
          }
          aCallback.notify(Cr.NS_ERROR_UNEXPECTED, null, null);
          return;
        }
        let domMessage = self.createDomMessageFromRecord(messageRecord);
        aCallback.notify(Cr.NS_OK, messageRecord, domMessage);
      };

      txn.onerror = function(event) {
        if (DEBUG) {
          if (event.target) {
            debug("Caught error on transaction", event.target.error.name);
          }
        }
        aCallback.notify(Cr.NS_ERROR_FAILURE, null, null);
      };
    });
  },

  /**
   * Helper to translate NS errors to the error causes defined in
   * <code>nsIMobileMessageCallback</code>.
   *
   * @function MobileMessageDB.translateCrErrorToMessageCallbackError
   * @param {number} aCrError
   *        The error code defined in <code>Components.result</code>
   * @return {number}
   *         The error code defined in <code>nsIMobileMessageCallback</code>
   */
  translateCrErrorToMessageCallbackError(aCrError) {
    switch (aCrError) {
      case Cr.NS_OK:
        return Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR;
      case Cr.NS_ERROR_UNEXPECTED:
        return Ci.nsIMobileMessageCallback.UNKNOWN_ERROR;
      case Cr.NS_ERROR_FILE_NOT_FOUND:
        return Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR;
      case Cr.NS_ERROR_FILE_NO_DEVICE_SPACE:
        return Ci.nsIMobileMessageCallback.STORAGE_FULL_ERROR;
      default:
        return Ci.nsIMobileMessageCallback.INTERNAL_ERROR;
    }
  },

  /**
   * @callback MobileMessageDB.SaveSmsSegmentCallback
   * @param {number} aErrorCode
   *        The error code on failure, or <code>NS_OK</code> on success.
   * @param {MobileMessageDB.SmsSegmentRecord} aCompleteMessage
   *        The composing message. It becomes a complete message once the last
   *        segment is stored.
   */

  /**
   * Store a single SMS segment.
   *
   * @function MobileMessageDB.saveSmsSegment
   * @param {MobileMessageDB.SmsSegmentRecord} aSmsSegment
   *        Single SMS segment.
   * @param {Object} aCallback
   *        The object which includes a callback function.
   * @param {MobileMessageDB.SaveSmsSegmentCallback} aCallback.notify
   *        The callback function to invoke when the request finishes.
   */
  saveSmsSegment(aSmsSegment, aCallback) {
    let completeMessage = null;
    this.newTxn(
      READ_WRITE,
      function(error, txn, segmentStore) {
        if (error) {
          if (DEBUG) {
            debug(error);
          }
          aCallback.notify(error, null);
          return;
        }

        txn.oncomplete = function(event) {
          if (DEBUG) {
            debug("Transaction " + txn + " completed.");
          }
          if (completeMessage) {
            // Rebuild full body
            if (
              completeMessage.encoding ==
              lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET
            ) {
              // Uint8Array doesn't have `concat`, so
              // we have to merge all segments by hand.
              let fullDataLen = 0;
              for (let i = 1; i <= completeMessage.segmentMaxSeq; i++) {
                fullDataLen += completeMessage.segments[i].length;
              }

              completeMessage.fullData = new Uint8Array(fullDataLen);
              for (let d = 0, i = 1; i <= completeMessage.segmentMaxSeq; i++) {
                let data = completeMessage.segments[i];
                for (let j = 0; j < data.length; j++) {
                  completeMessage.fullData[d++] = data[j];
                }
              }
            } else {
              completeMessage.fullBody = completeMessage.segments.join("");
            }

            // Remove handy fields after completing the concatenation.
            delete completeMessage.id;
            delete completeMessage.hash;
            delete completeMessage.receivedSegments;
            delete completeMessage.segments;
          }
          aCallback.notify(Cr.NS_OK, completeMessage);
        };

        txn.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Cr.NS_ERROR_FILE_NO_DEVICE_SPACE
              : Cr.NS_ERROR_FAILURE;
          aCallback.notify(error, null);
        };

        aSmsSegment.hash =
          aSmsSegment.sender +
          ":" +
          aSmsSegment.segmentRef +
          ":" +
          aSmsSegment.segmentMaxSeq +
          ":" +
          aSmsSegment.iccId;
        let seq = aSmsSegment.segmentSeq;
        if (DEBUG) {
          debug("Saving SMS Segment: " + aSmsSegment.hash + ", seq: " + seq);
        }
        let getRequest = segmentStore.index("hash").get(aSmsSegment.hash);
        getRequest.onsuccess = function(event) {
          let segmentRecord = event.target.result;
          if (!segmentRecord) {
            if (DEBUG) {
              debug("Not found! Create a new record to store the segments.");
            }
            aSmsSegment.receivedSegments = 1;
            aSmsSegment.segments = [];
            if (
              aSmsSegment.encoding == lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET
            ) {
              aSmsSegment.segments[seq] = aSmsSegment.data;
            } else {
              aSmsSegment.segments[seq] = aSmsSegment.body;
            }

            segmentStore.add(aSmsSegment);

            return;
          }

          if (DEBUG) {
            debug(
              "Append SMS Segment into existed message object: " +
                segmentRecord.id
            );
          }

          if (segmentRecord.segments[seq]) {
            if (
              segmentRecord.encoding ==
                lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET &&
              segmentRecord.encoding == aSmsSegment.encoding &&
              segmentRecord.segments[seq].length == aSmsSegment.data.length &&
              segmentRecord.segments[seq].every(function(aElement, aIndex) {
                return aElement == aSmsSegment.data[aIndex];
              })
            ) {
              if (DEBUG) {
                debug("Got duplicated binary segment no: " + seq);
              }
              return;
            }

            if (
              segmentRecord.encoding !=
                lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET &&
              aSmsSegment.encoding !=
                lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET &&
              segmentRecord.segments[seq] == aSmsSegment.body
            ) {
              if (DEBUG) {
                debug("Got duplicated text segment no: " + seq);
              }
              return;
            }

            // Update mandatory properties to ensure that the segments could be
            // concatenated properly.
            segmentRecord.encoding = aSmsSegment.encoding;
            segmentRecord.originatorPort = aSmsSegment.originatorPort;
            segmentRecord.destinationPort = aSmsSegment.destinationPort;
            segmentRecord.teleservice = aSmsSegment.teleservice;
            // Decrease the counter for this collided segment.
            segmentRecord.receivedSegments--;
          }

          segmentRecord.timestamp = aSmsSegment.timestamp;

          if (
            segmentRecord.encoding == lazy.RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET
          ) {
            segmentRecord.segments[seq] = aSmsSegment.data;
          } else {
            segmentRecord.segments[seq] = aSmsSegment.body;
          }
          segmentRecord.receivedSegments++;
          segmentRecord.imsMessage = aSmsSegment.imsMessage;

          // The port information is only available in 1st segment for CDMA WAP Push.
          // If the segments of a WAP Push are not received in sequence
          // (e.g., SMS with seq == 1 is not the 1st segment received by the device),
          // we have to retrieve the port information from 1st segment and
          // save it into the segmentRecord.
          if (
            aSmsSegment.teleservice ===
              lazy.RIL.PDU_CDMA_MSG_TELESERVICE_ID_WAP &&
            seq === 1
          ) {
            if (
              aSmsSegment.originatorPort !==
              Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID
            ) {
              segmentRecord.originatorPort = aSmsSegment.originatorPort;
            }

            if (
              aSmsSegment.destinationPort !==
              Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID
            ) {
              segmentRecord.destinationPort = aSmsSegment.destinationPort;
            }
          }

          if (segmentRecord.receivedSegments < segmentRecord.segmentMaxSeq) {
            if (DEBUG) {
              debug("Message is incomplete.");
            }
            segmentStore.put(segmentRecord);
            return;
          }

          completeMessage = segmentRecord;

          // Delete Record in DB
          segmentStore.delete(segmentRecord.id);
        };
      },
      [SMS_SEGMENT_STORE_NAME]
    );
  },

  /**
   * Store a single cellbroadcast message
   *
   * @function MobileMessageDB.saveCellbroadcastMessage
   * @param {MobileMessageDB.CellbroadcastRecord} aCellbroadcast
   *        Single cellbroadcast message.
   * @param {MobileMessageDB.SaveCellbroadcastCallback} aCallback.notify
   *        The callback function to invoke when the request finishes.
   */
  saveCellBroadcastMessage(aCellBroadcastMessage, aCallback) {
    if (DEBUG) {
      debug(
        "Save CellBroadcast Message " + JSON.stringify(aCellBroadcastMessage)
      );
    }

    this.newTxn(
      READ_WRITE,
      function(error, txn, cellBroadcastStore) {
        if (error) {
          if (DEBUG) {
            debug(error);
          }
          aCallback.notify(error, null);
          return;
        }

        txn.oncomplete = function(event) {
          if (DEBUG) {
            debug("Transaction " + txn + " completed.");
          }
          aCallback.notify(Cr.NS_OK, aCellBroadcastMessage);
        };

        txn.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Cr.NS_ERROR_FILE_NO_DEVICE_SPACE
              : Cr.NS_ERROR_FAILURE;
          aCallback.notify(error, null);
        };

        let cellbroadcastRecord = JSON.parse(
          JSON.stringify(aCellBroadcastMessage)
        );
        cellbroadcastRecord.hash =
          aCellBroadcastMessage.serialNumber +
          ":" +
          aCellBroadcastMessage.messageId;

        if (DEBUG) {
          debug("Save CellBroadcast Message hash " + cellbroadcastRecord.hash);
        }

        cellBroadcastStore.add(cellbroadcastRecord);
      },
      [CELLBROADCAST_STORE_NAME]
    );
  },

  getCellBroadcastMessage(aSerialNumber, aMessageIdentifier, aCallback) {
    let hash = aSerialNumber + ":" + aMessageIdentifier;
    if (DEBUG) {
      debug("Get Cellbroadcast Message hash: " + hash);
    }

    this.newTxn(
      READ_ONLY,
      function(error, txn, cellBroadcastStore) {
        if (error) {
          if (DEBUG) {
            debug(error);
          }
          aCallback.notify(error, null, null);
          return;
        }
        let request = cellBroadcastStore.index("hash").get(hash);

        txn.oncomplete = function(event) {
          if (DEBUG) {
            debug("Transaction " + txn + " completed.");
          }
          let cellBroadcastRecord = request.result;
          if (!cellBroadcastRecord) {
            if (DEBUG) {
              debug("Broadcast Message: " + hash + " not found");
            }
            aCallback.notify(Cr.NS_ERROR_FILE_NOT_FOUND, null, null);
            return;
          }
          if (DEBUG) {
            debug(
              "Broadcast Message: " +
                hash +
                " founded " +
                JSON.stringify(cellBroadcastRecord)
            );
          }
          let geometries = [];
          cellBroadcastRecord.geometries.forEach(geo => {
            if (geo.type === lazy.RIL.GEOMETRY_TYPE_POLYGON) {
              geometries.push(new lazy.SIM.Polygon(geo._vertices));
            } else if (geo.type === lazy.RIL.GEOMETRY_TYPE_CIRCLE) {
              geometries.push(new lazy.SIM.Circle(geo._center, geo._radius));
            } else if (DEBUG) {
              debug("Invalid geometry type: " + geo.type);
            }
          });
          cellBroadcastRecord.geometries = geometries;

          aCallback.notify(Cr.NS_OK, cellBroadcastRecord, null);
        };

        txn.onerror = function(event) {
          if (DEBUG) {
            if (event.target) {
              debug("Caught error on transaction", event.target.error.name);
            }
          }
          aCallback.notify(Cr.NS_ERROR_FAILURE, null, null);
        };
      },
      [CELLBROADCAST_STORE_NAME]
    );
  },

  deleteCellBroadcastMessage(aSerialNumber, aMessageIdentifier, aCallback) {
    let hash = aSerialNumber + ":" + aMessageIdentifier;
    if (DEBUG) {
      debug("Delete Cellbroadcast Message hash: " + hash);
    }

    this.newTxn(
      READ_WRITE,
      function(error, txn, cellBroadcastStore) {
        if (error) {
          if (DEBUG) {
            debug(error);
          }
          aCallback.notify(error, null, null);
          return;
        }
        let request = cellBroadcastStore.index("hash").get(hash);
        let deletedRecord = {};

        txn.oncomplete = function(event) {
          if (DEBUG) {
            debug("Transaction " + txn + " completed.");
          }
          if (!deletedRecord) {
            aCallback.notify(Cr.NS_ERROR_FILE_NOT_FOUND, null, null);
            return;
          }
          aCallback.notify(Cr.NS_OK, deletedRecord, null);
        };

        txn.onerror = function(event) {
          if (DEBUG) {
            if (event.target) {
              debug("Caught error on transaction", event.target.error.name);
            }
          }
          aCallback.notify(Cr.NS_ERROR_FAILURE, null, null);
        };

        request.onsuccess = function(event) {
          let deletedRecord = event.target.result;
          if (!deletedRecord) {
            if (DEBUG) {
              debug("Broadcast Message: " + hash + " not found");
            }
          } else {
            cellBroadcastStore.delete(deletedRecord.id).onsuccess = function(
              event
            ) {
              if (DEBUG) {
                debug("Cellbroadcast Message: " + hash + " deleted");
              }
            };
          }
        };
      },
      [CELLBROADCAST_STORE_NAME]
    );
  },

  /**
   * nsIMobileMessageDatabaseService API
   */

  /**
   * Get the message record with given message ID.
   *
   * @function MobileMessageDB.getMessage
   * @param {string} aMessageID
   *        The message ID.
   * @param {nsIMobileMessageCallback} aRequest
   *        The callback object.
   */
  getMessage(aMessageId, aRequest) {
    if (DEBUG) {
      debug("Retrieving message with ID " + aMessageId);
    }
    let self = this;
    let notifyCallback = {
      notify(aRv, aMessageRecord, aDomMessage) {
        if (Cr.NS_OK == aRv) {
          aRequest.notifyMessageGot(aDomMessage);
          return;
        }
        aRequest.notifyGetMessageFailed(
          self.translateCrErrorToMessageCallbackError(aRv),
          null
        );
      },
    };
    this.getMessageRecordById(aMessageId, notifyCallback);
  },

  /**
   * Delete the message record with given message IDs.
   *
   * @function MobileMessageDB.deleteMessage
   * @param {number[]} messageIds
   *        The IDs of messages to delete.
   * @param {number} length
   *        The length of the <code>messageIds</code> array.
   * @param {nsIMobileMessageCallback} aRequest
   *        The callback object.
   */
  deleteMessage(messageIds, length, aRequest) {
    if (DEBUG) {
      debug("deleteMessage: message ids " + JSON.stringify(messageIds));
    }
    let deleted = [];
    let self = this;
    this.newTxn(
      READ_WRITE,
      function(error, txn, stores) {
        if (error) {
          if (DEBUG) {
            debug("deleteMessage: failed to open transaction");
          }
          aRequest.notifyDeleteMessageFailed(
            self.translateCrErrorToMessageCallbackError(error)
          );
          return;
        }

        let deletedInfo = { messageIds: [], threadIds: [] };

        txn.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Ci.nsIMobileMessageCallback.STORAGE_FULL_ERROR
              : Ci.nsIMobileMessageCallback.INTERNAL_ERROR;
          aRequest.notifyDeleteMessageFailed(error);
        };

        const messageStore = stores[0];
        const threadStore = stores[1];

        txn.oncomplete = function(event) {
          if (DEBUG) {
            debug("Transaction " + txn + " completed.");
          }
          aRequest.notifyMessageDeleted(deleted, length);
          self.notifyDeletedInfo(deletedInfo);
        };

        let threadsToUpdate = {};
        let numOfMessagesToDelete = length;
        let updateThreadInfo = function() {
          for (let threadId in threadsToUpdate) {
            let threadInfo = threadsToUpdate[threadId];
            self.updateThreadByMessageChange(
              messageStore,
              threadStore,
              threadInfo.threadId,
              threadInfo.removedMsgIds,
              threadInfo.ignoredUnreadCount,
              deletedInfo
            );
          }
        };

        let req = messageStore.mozGetAll();
        req.onsuccess = function(event) {
          let size = event.target.result.length;
          if (DEBUG) {
            debug("message size " + size + " equal? deleted size " + length);
          }
          if (size === length) {
            messageStore.clear();
            threadStore.clear();
            let threadsIdSet = new Set();
            for (let i = 0; i < length; i++) {
              deleted[i] = true;
              deletedInfo.messageIds.push(messageIds[i]);
              threadsIdSet.add(event.target.result[i].threadId);
            }
            deletedInfo.threadIds.push(...Array.from(threadsIdSet));
            updateThreadInfo();
          } else {
            for (let i = 0; i < length; i++) {
              let messageId = messageIds[i];
              deleted[i] = false;
              messageStore.get(messageId).onsuccess = function(
                messageIndex,
                event
              ) {
                let messageRecord = event.target.result;
                let messageId = messageIds[messageIndex];
                if (messageRecord) {
                  if (DEBUG) {
                    debug("Deleting message id " + messageId);
                  }

                  // First actually delete the message.
                  messageStore.delete(messageId).onsuccess = function(event) {
                    if (DEBUG) {
                      debug("Message id " + messageId + " deleted");
                    }

                    numOfMessagesToDelete--;
                    deleted[messageIndex] = true;
                    deletedInfo.messageIds.push(messageId);

                    // Cache thread info to be updated.
                    let threadId = messageRecord.threadId;
                    if (!threadsToUpdate[threadId]) {
                      threadsToUpdate[threadId] = {
                        threadId,
                        removedMsgIds: [messageId],
                        ignoredUnreadCount: !messageRecord.read ? 1 : 0,
                      };
                    } else {
                      let threadInfo = threadsToUpdate[threadId];
                      threadInfo.removedMsgIds.push(messageId);
                      if (!messageRecord.read) {
                        threadInfo.ignoredUnreadCount++;
                      }
                    }

                    // After all messsages are deleted, update unread count and most
                    // recent message of related threads at once.
                    if (!numOfMessagesToDelete) {
                      updateThreadInfo();
                    }
                  };
                } else {
                  if (DEBUG) {
                    debug("Message id " + messageId + " does not exist");
                  }

                  numOfMessagesToDelete--;
                  if (!numOfMessagesToDelete) {
                    updateThreadInfo();
                  }
                }
              }.bind(null, i);
            }
          }
        };
        req.onerror = function(error) {
          aRequest.notifyDeleteMessageFailed(error);
        };
      },
      [MESSAGE_STORE_NAME, THREAD_STORE_NAME]
    );
  },

  /**
   * Create a cursor to iterate on stored message records.
   *
   * @function MobileMessageDB.createMessageCursor
   * @param {boolean} aHasStartDate
   *        <code>true</code> to query only messages starts with
   *        <code>aStartDate</code>
   * @param {number} aStartDate
   *        The timestamp of start date in milliseconds.
   * @param {boolean} aHasEndDate
   *        <code>true</code> to query only messages before the
   *        <code>aEndDate</code>
   * @param {number} aEndDate
   *        The timestamp of end date in milliseconds.
   * @param {string[]} aNumbers
   *        If not <code>null</code>, query only messages with sender or
   *        receiver who's number matches one of the numbers listed in the array.
   * @param {number} aNumbersCount
   *        The length of <code>aNumbers</code> array.
   * @param {string} aDelivery
   *        If not <code>null</code>, query only messages matching the delivery
   *        value.
   * @param {boolean} aHasRead
   *        <code>true</code> to query only messages match the read value
   *        specified by <code>aRead</code>
   * @param {boolean} aRead
   *        Specify the <code>read</code> query condition.
   * @param {number} aThreadId
   *        If not <code>null</code>, query only messages in the given thread.
   * @param {boolean} aReverse
   *        <code>true</code> to reverse the order.
   * @param {nsIMobileMessageCursorCallback} aCallback
   *        The callback object used by GetMessagesCursor
   * @return {GetMessagesCursor}
   *         The cursor to iterate on messages.
   */
  createMessageCursor(
    aHasStartDate,
    aStartDate,
    aHasEndDate,
    aEndDate,
    aNumbers,
    aNumbersCount,
    aDelivery,
    aHasRead,
    aRead,
    aHasThreadId,
    aThreadId,
    aReverse,
    aCallback
  ) {
    if (DEBUG) {
      debug(
        "Creating a message cursor. Filters:" +
          " startDate: " +
          (aHasStartDate ? aStartDate : "(null)") +
          " endDate: " +
          (aHasEndDate ? aEndDate : "(null)") +
          " delivery: " +
          aDelivery +
          " numbers: " +
          (aNumbersCount ? aNumbers : "(null)") +
          " read: " +
          (aHasRead ? aRead : "(null)") +
          " threadId: " +
          (aHasThreadId ? aThreadId : "(null)") +
          " reverse: " +
          aReverse
      );
    }

    let filter = {};
    if (aHasStartDate) {
      filter.startDate = aStartDate;
    }
    if (aHasEndDate) {
      filter.endDate = aEndDate;
    }
    if (aNumbersCount) {
      filter.numbers = aNumbers.slice();
    }
    if (aDelivery !== null) {
      filter.delivery = aDelivery;
    }
    if (aHasRead) {
      filter.read = aRead;
    }
    if (aHasThreadId) {
      filter.threadId = aThreadId;
    }

    let cursor = new GetMessagesCursor(this, aCallback);

    let self = this;
    self.newTxn(
      READ_ONLY,
      function(error, txn, stores) {
        let collector = cursor.collector.idCollector;
        let collect = collector.collect.bind(collector);
        FilterSearcherHelper.transact(
          self,
          txn,
          error,
          filter,
          aReverse,
          collect
        );
      },
      [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME]
    );

    return cursor;
  },

  /**
   * Change the <code>read</code> property of a stored message record.
   *
   * @function MobileMessageDB.markMessageRead
   * @param {number} messageId
   *        The message ID.
   * @param {boolean} value
   *        The updated <code>read</code> value.
   * @param {boolean} aSendReadReport
   *        <code>true</code> to reply the read report of an incoming MMS
   *        message whose <code>isReadReportSent</code> is 'false'.
   *        Note: <code>isReadReportSent</code> will be set to 'true' no
   *        matter aSendReadReport is true or not when a message was marked
   *        from UNREAD to READ. See bug 1180470 for the new UX policy.
   * @param {nsIMobileMessageCallback} aRequest
   *        The callback object.
   */
  markMessageRead(messageId, value, aSendReadReport, aRequest) {
    if (DEBUG) {
      debug("Setting message " + messageId + " read to " + value);
    }
    let self = this;
    this.newTxn(
      READ_WRITE,
      function(error, txn, stores) {
        if (error) {
          if (DEBUG) {
            debug(error);
          }
          aRequest.notifyMarkMessageReadFailed(
            self.translateCrErrorToMessageCallbackError(error)
          );
          return;
        }

        txn.onabort = function(event) {
          if (DEBUG) {
            debug("transaction abort due to " + event.target.error.name);
          }
          let error =
            event.target.error.name === "QuotaExceededError"
              ? Ci.nsIMobileMessageCallback.STORAGE_FULL_ERROR
              : Ci.nsIMobileMessageCallback.INTERNAL_ERROR;
          aRequest.notifyMarkMessageReadFailed(error);
        };

        let messageStore = stores[0];
        let threadStore = stores[1];
        messageStore.get(messageId).onsuccess = function(event) {
          let messageRecord = event.target.result;
          if (!messageRecord) {
            if (DEBUG) {
              debug("Message ID " + messageId + " not found");
            }
            aRequest.notifyMarkMessageReadFailed(
              Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR
            );
            return;
          }

          if (messageRecord.id != messageId) {
            if (DEBUG) {
              debug(
                "Retrieve message ID (" +
                  messageId +
                  ") is " +
                  "different from the one we got"
              );
            }
            aRequest.notifyMarkMessageReadFailed(
              Ci.nsIMobileMessageCallback.UNKNOWN_ERROR
            );
            return;
          }

          // If the value to be set is the same as the current message `read`
          // value, we just notify successfully.
          if (messageRecord.read == value) {
            if (DEBUG) {
              debug("The value of messageRecord.read is already " + value);
            }
            aRequest.notifyMessageMarkedRead(messageRecord.read);
            return;
          }

          messageRecord.read = value ? FILTER_READ_READ : FILTER_READ_UNREAD;
          messageRecord.readIndex = [
            messageRecord.read,
            messageRecord.timestamp,
          ];
          let readReportMessageId, readReportTo;
          if (
            messageRecord.type == "mms" &&
            messageRecord.delivery == DELIVERY_RECEIVED &&
            messageRecord.read == FILTER_READ_READ &&
            messageRecord.headers["x-mms-read-report"] &&
            !messageRecord.isReadReportSent
          ) {
            messageRecord.isReadReportSent = true;

            if (aSendReadReport) {
              let from = messageRecord.headers.from;
              readReportTo = from && from.address;
              readReportMessageId = messageRecord.headers["message-id"];
            }
          }

          if (DEBUG) {
            debug("Message.read set to: " + value);
          }
          messageStore.put(messageRecord).onsuccess = function(event) {
            if (DEBUG) {
              debug(
                "Update successfully completed. Message: " +
                  JSON.stringify(event.target.result)
              );
            }

            // Now update the unread count.
            let threadId = messageRecord.threadId;

            threadStore.get(threadId).onsuccess = function(event) {
              let threadRecord = event.target.result;
              threadRecord.unreadCount += value ? -1 : 1;
              if (DEBUG) {
                debug(
                  "Updating unreadCount for thread id " +
                    threadId +
                    ": " +
                    (value
                      ? threadRecord.unreadCount + 1
                      : threadRecord.unreadCount - 1) +
                    " -> " +
                    threadRecord.unreadCount
                );
              }
              threadStore.put(threadRecord).onsuccess = function(event) {
                if (readReportMessageId && readReportTo) {
                  lazy.gMMSService.sendReadReport(
                    readReportMessageId,
                    readReportTo,
                    messageRecord.iccId
                  );
                }
                aRequest.notifyMessageMarkedRead(messageRecord.read);
              };
            };
          };
        };
      },
      [MESSAGE_STORE_NAME, THREAD_STORE_NAME]
    );
  },

  /**
   * Create a cursor to iterate on stored threads.
   *
   * @function MobileMessageDB.createThreadCursor
   * @param {nsIMobileMessageCursorCallback} callback
   *        The callback object used by GetMessagesCursor
   * @return {GetThreadsCursor}
   *         The cursor to iterate on threads.
   */
  createThreadCursor(callback) {
    if (DEBUG) {
      debug("Getting thread list");
    }

    let cursor = new GetThreadsCursor(this, callback);
    this.newTxn(
      READ_ONLY,
      function(error, txn, threadStore) {
        let collector = cursor.collector.idCollector;
        if (error) {
          collector.collect(null, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
          return;
        }
        txn.onerror = function(event) {
          if (DEBUG) {
            debug("Caught error on transaction ", event.target.error.name);
          }
          collector.collect(null, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
        };
        let request = threadStore
          .index("lastTimestamp")
          .openKeyCursor(null, PREV);
        request.onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (collector.collect(txn, cursor.primaryKey, cursor.key)) {
              cursor.continue();
            }
          } else {
            collector.collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
          }
        };
      },
      [THREAD_STORE_NAME]
    );

    return cursor;
  },
};

var FilterSearcherHelper = {
  /**
   * @param index
   *        The name of a message store index to filter on.
   * @param range
   *        A IDBKeyRange.
   * @param direction
   *        NEXT or PREV.
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param collect
   *        Result colletor function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  filterIndex(index, range, direction, txn, collect) {
    let messageStore = txn.objectStore(MESSAGE_STORE_NAME);
    let request = messageStore.index(index).openKeyCursor(range, direction);
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      // Once the cursor has retrieved all keys that matches its key range,
      // the filter search is done.
      if (cursor) {
        let timestamp = Array.isArray(cursor.key) ? cursor.key[1] : cursor.key;
        if (collect(txn, cursor.primaryKey, timestamp)) {
          cursor.continue();
        }
      } else {
        collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
      }
    };
    request.onerror = function(event) {
      if (DEBUG && event) {
        debug("IDBRequest error " + event.target.error.name);
      }
      collect(txn, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
    };
  },

  /**
   * Explicitly filter message on the timestamp index.
   *
   * @param startDate
   *        Timestamp of the starting date.
   * @param endDate
   *        Timestamp of the ending date.
   * @param direction
   *        NEXT or PREV.
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param collect
   *        Result colletor function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  filterTimestamp(startDate, endDate, direction, txn, collect) {
    let range = null;
    if (startDate != null && endDate != null) {
      range = IDBKeyRange.bound(startDate, endDate);
    } else if (startDate != null) {
      range = IDBKeyRange.lowerBound(startDate);
    } else if (endDate != null) {
      range = IDBKeyRange.upperBound(endDate);
    }
    this.filterIndex("timestamp", range, direction, txn, collect);
  },

  /**
   * Initiate a filtering transaction.
   *
   * @param mmdb
   *        A MobileMessageDB.
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param error
   *        Previous error while creating the transaction.
   * @param filter
   *        A MobileMessageFilter dictionary.
   * @param reverse
   *        A boolean value indicating whether we should filter message in
   *        reversed order.
   * @param collect
   *        Result collector function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  transact(mmdb, txn, error, filter, reverse, collect) {
    if (error) {
      // TODO look at event.target.error.name, pick appropriate error constant.
      if (DEBUG) {
        debug("IDBRequest error " + error);
      }
      collect(txn, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
      return;
    }

    let direction = reverse ? PREV : NEXT;

    // We support filtering by date range only (see `else` block below) or by
    // number/delivery status/read status with an optional date range.
    if (
      filter.delivery == null &&
      filter.numbers == null &&
      filter.read == null &&
      filter.threadId == null
    ) {
      // Filtering by date range only.
      if (DEBUG) {
        debug("filter.timestamp " + filter.startDate + ", " + filter.endDate);
      }

      this.filterTimestamp(
        filter.startDate,
        filter.endDate,
        direction,
        txn,
        collect
      );
      return;
    }

    // Numeric 0 is smaller than any time stamp, and empty string is larger
    // than all numeric values.
    let startDate = 0,
      endDate = "";
    if (filter.startDate != null) {
      startDate = filter.startDate;
    }
    if (filter.endDate != null) {
      endDate = filter.endDate;
    }

    let single, intersectionCollector;
    {
      let num = 0;
      if (filter.delivery) {
        num++;
      }
      if (filter.numbers) {
        num++;
      }
      if (filter.read != undefined) {
        num++;
      }
      if (filter.threadId != undefined) {
        num++;
      }
      single = num == 1;
    }

    if (!single) {
      intersectionCollector = new IntersectionResultsCollector(
        collect,
        reverse
      );
    }

    // Retrieve the keys from the 'delivery' index that matches the value of
    // filter.delivery.
    if (filter.delivery) {
      if (DEBUG) {
        debug("filter.delivery " + filter.delivery);
      }
      let delivery = filter.delivery;
      let range = IDBKeyRange.bound([delivery, startDate], [delivery, endDate]);
      this.filterIndex(
        "delivery",
        range,
        direction,
        txn,
        single ? collect : intersectionCollector.newContext()
      );
    }

    // Retrieve the keys from the 'read' index that matches the value of
    // filter.read.
    if (filter.read != undefined) {
      if (DEBUG) {
        debug("filter.read " + filter.read);
      }
      let read = filter.read ? FILTER_READ_READ : FILTER_READ_UNREAD;
      let range = IDBKeyRange.bound([read, startDate], [read, endDate]);
      this.filterIndex(
        "read",
        range,
        direction,
        txn,
        single ? collect : intersectionCollector.newContext()
      );
    }

    // Retrieve the keys from the 'threadId' index that matches the value of
    // filter.threadId.
    if (filter.threadId != undefined) {
      if (DEBUG) {
        debug("filter.threadId " + filter.threadId);
      }
      let threadId = filter.threadId;
      let range = IDBKeyRange.bound([threadId, startDate], [threadId, endDate]);
      this.filterIndex(
        "threadId",
        range,
        direction,
        txn,
        single ? collect : intersectionCollector.newContext()
      );
    }

    // Retrieve the keys from the 'sender' and 'receiver' indexes that
    // match the values of filter.numbers
    if (filter.numbers) {
      if (DEBUG) {
        debug("filter.numbers " + filter.numbers.join(", "));
      }

      if (!single) {
        collect = intersectionCollector.newContext();
      }

      let participantStore = txn.objectStore(PARTICIPANT_STORE_NAME);
      let typedAddresses = filter.numbers.map(function(number) {
        return {
          address: number,
          type: lazy.MMS.Address.resolveType(number),
        };
      });
      mmdb.findParticipantIdsByTypedAddresses(
        participantStore,
        typedAddresses,
        false,
        true,
        function(participantIds) {
          if (DEBUG) {
            debug(
              "findParticipantIdsByTypedAddresses return participantIds: " +
                participantIds +
                "[" +
                participantIds.length +
                "]"
            );
          }
          if (!participantIds || !participantIds.length) {
            // Oops! No such participant at all.

            collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
            return;
          }

          if (participantIds.length == 1) {
            let id = participantIds[0];
            let range = IDBKeyRange.bound([id, startDate], [id, endDate]);
            this.filterIndex("participantIds", range, direction, txn, collect);
            return;
          }

          let unionCollector = new UnionResultsCollector(collect);

          this.filterTimestamp(
            filter.startDate,
            filter.endDate,
            direction,
            txn,
            unionCollector.newTimestampContext()
          );

          for (let i = 0; i < participantIds.length; i++) {
            let id = participantIds[i];
            let range = IDBKeyRange.bound([id, startDate], [id, endDate]);
            this.filterIndex(
              "participantIds",
              range,
              direction,
              txn,
              unionCollector.newContext()
            );
          }
        }.bind(this)
      );
    }
  },
};

/**
 * Collector class for read-ahead result objects. Mmdb may now try to fetch
 * message/thread records before it's requested explicitly.
 *
 * The read ahead behavior can be controlled by an integer mozSettings entry
 * "ril.sms.maxReadAheadEntries" as well as an integer holding preference
 * "dom.sms.maxReadAheadEntries". The meanings are:
 *
 *   positive: finite read-ahead entries,
 *   0: don't read ahead unless explicitly requested, (default)
 *   negative: read ahead all IDs if possible.
 *
 * The order of ID filtering objects are now:
 *
 *   [UnionResultsCollector]
 *   +-> [IntersectionResultsCollector]
 *       +-> IDsCollector
 *           +-> ResultsCollector
 *
 * ResultsCollector has basically similar behaviour with IDsCollector. When
 * RC::squeeze() is called, either RC::drip() is called instantly if we have
 * already fetched results available, or the request is kept and IC::squeeze()
 * is called.
 *
 * When RC::collect is called by IC::drip, it proceeds to fetch the
 * corresponding record given that collected ID is neither an error nor an end
 * mark. After the message/thread record being fetched, ResultsCollector::drip
 * is called if we have pending request. Anyway, RC::maybeSqueezeIdCollector is
 * called to determine whether we need to call IC::squeeze again.
 *
 * RC::squeeze is called when nsICursorContinueCallback::handleContinue() is
 * called. ResultsCollector::drip will call to
 * nsIMobileMessageCursorCallback::notifyFoo.
 *
 * In summary, the major call paths are:
 *
 *   RC::squeeze
 *   o-> RC::drip
 *       +-> RC::notifyCallback
 *           +-> nsIMobileMessageCursorCallback::notifyFoo
 *   +-> RC::maybeSqueezeIdCollector
 *       o-> IC::squeeze
 *           o-> IC::drip
 *               +-> RC::collect
 *                   o-> RC::readAhead
 *                       +-> RC::notifyResult
 *                           o-> RC::drip ...
 *                           +-> RC::maybeSqueezeIdCollector ...
 *                   o-> RC::notifyResult ...
 */
function ResultsCollector(readAheadFunc) {
  this.idCollector = new IDsCollector();
  this.results = [];
  this.readAhead = readAheadFunc;

  this.maxReadAhead = this.maxReadAhead = Services.prefs.getIntPref(
    "dom.sms.maxReadAheadEntries",
    DEFAULT_READ_AHEAD_ENTRIES
  );
}
ResultsCollector.prototype = {
  /**
   * Underlying ID collector object.
   */
  idCollector: null,

  /**
   * An array keeping fetched result objects. Replaced by a new empty array
   * every time when |this.drip| is called.
   */
  results: null,

  /**
   * A function that takes (<txn>, <id>, <collector>). It fetches the object
   * specified by <id> and notify <collector> with that by calling
   * |<collector>.notifyResult()|. If <txn> is null, this function should
   * create a new read-only transaction itself. The returned result object may
   * be null to indicate an error during the fetch process.
   */
  readAhead: null,

  /**
   * A boolean value inidicating a readAhead call is ongoing. Set before calling
   * |this.readAhead| and reset in |this.notifyResult|.
   */
  readingAhead: false,

  /**
   * A numeric value read from preference "dom.sms.maxReadAheadEntries".
   */
  maxReadAhead: 0,

  /**
   * An active IDBTransaction object to be reused.
   */
  activeTxn: null,

  /**
   * A nsIMobileMessageCursorCallback.
   */
  requestWaiting: null,

  /**
   * A boolean value indicating either a COLLECT_ID_END or COLLECT_ID_ERROR has
   * been received.
   */
  done: false,

  /**
   * When |this.done|, it's either COLLECT_ID_END or COLLECT_ID_ERROR.
   */
  lastId: null,

  /**
   * Receive collected id from IDsCollector and fetch the correspond result
   * object if necessary.
   *
   * @param txn
   *        An IDBTransaction object. Null if there is no active transaction in
   *        IDsCollector. That is, the ID collecting transaction is completed.
   * @param id
   *        A positive numeric id, COLLECT_ID_END(0), or COLLECT_ID_ERROR(-1).
   */
  collect(txn, id) {
    if (this.done) {
      // If this callector has been terminated because of previous errors in
      // |this.readAhead|, ignore any further IDs from IDsCollector.
      return;
    }

    if (DEBUG) {
      debug("ResultsCollector::collect ID = " + id);
    }

    // Reuse the active transaction cached if IDsCollector has no active
    // transaction.
    txn = txn || this.activeTxn;

    if (id > 0) {
      this.readingAhead = true;
      this.readAhead(txn, id, this);
    } else {
      this.notifyResult(txn, id, null);
    }
  },

  /**
   * Callback function for |this.readAhead|.
   *
   * This function pushes result object to |this.results| or updates
   * |this.done|, |this.lastId| if an end mark or an error is found. Since we
   * have already a valid result entry, check |this.requestWaiting| and deal
   * with it. At last, call to |this.maybeSqueezeIdCollector| to ask more id
   * again if necessary.
   *
   * @param txn
   *        An IDBTransaction object. Null if caller has no active transaction.
   * @param id
   *        A positive numeric id, COLLECT_ID_END(0), or COLLECT_ID_ERROR(-1).
   * @param result
   *        An object associated with id. Null if |this.readAhead| failed.
   */
  notifyResult(txn, id, result) {
    if (DEBUG) {
      debug("notifyResult(txn, " + id + ", <result>)");
    }

    this.readingAhead = false;

    if (id > 0) {
      if (result != null) {
        this.results.push(result);
      } else {
        id = COLLECT_ID_ERROR;
      }
    }

    if (id <= 0) {
      this.lastId = id;
      this.done = true;
    }

    if (!this.requestWaiting) {
      if (DEBUG) {
        debug("notifyResult: cursor.continue() not called yet");
      }
    } else {
      let callback = this.requestWaiting;
      this.requestWaiting = null;

      this.drip(callback);
    }

    this.maybeSqueezeIdCollector(txn);
  },

  /**
   * Request for one more ID if necessary.
   *
   * @param txn
   *        An IDBTransaction object. Null if caller has no active transaction.
   */
  maybeSqueezeIdCollector(txn) {
    if (
      this.done || // Nothing to be read.
      this.readingAhead || // Already in progress.
      this.idCollector.requestWaiting
    ) {
      // Already requested.
      return;
    }

    let max = this.maxReadAhead;
    if (!max && this.requestWaiting) {
      // If |this.requestWaiting| is set, try to read ahead at least once.
      max = 1;
    }
    if (max >= 0 && this.results.length >= max) {
      // More-equal than <max> entries has been read. Stop.
      if (DEBUG) {
        debug("maybeSqueezeIdCollector: max " + max + " entries read. Stop.");
      }
      return;
    }

    // A hack to pass current txn to |this.collect| when it's called directly by
    // |IDsCollector.squeeze|.
    this.activeTxn = txn;
    this.idCollector.squeeze(this.collect.bind(this));
    this.activeTxn = null;
  },

  /**
   * Request to pass available results or wait.
   *
   * @param callback
   *        A nsIMobileMessageCursorCallback.
   */
  squeeze(callback) {
    if (this.requestWaiting) {
      throw new Error("Already waiting for another request!");
    }

    if (this.results.length || this.done) {
      if (DEBUG) {
        debug("squeeze results.length: " + this.results.length);
      }
      // If |this.results.length| is non-zero, we have already some results to
      // pass. Otherwise, if |this.done| evaluates to true, we have also a
      // confirmed result to pass.
      this.drip(callback);
    } else {
      this.requestWaiting = callback;
    }

    // If we called |this.drip| in the last step, the fetched results have been
    // consumed and we should ask some more for read-ahead now.
    //
    // Otherwise, kick start read-ahead again because it may be stopped
    // previously because of |this.maxReadAhead| had been reached.
    this.maybeSqueezeIdCollector(null);
  },

  /**
   * Consume fetched resutls.
   *
   * @param callback
   *        A nsIMobileMessageCursorCallback.
   */
  drip(callback) {
    let results = this.results;
    this.results = [];

    let func = this.notifyCallback.bind(this, callback, results, this.lastId);
    Services.tm.currentThread.dispatch(func, Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
   * Notify a nsIMobileMessageCursorCallback.
   *
   * @param callback
   *        A nsIMobileMessageCursorCallback.
   * @param results
   *        An array of result objects.
   * @param lastId
   *        Since we only call |this.drip| when either there are results
   *        available or the read-ahead has done, so lastId here will be
   *        COLLECT_ID_END or COLLECT_ID_ERROR when results is empty and null
   *        otherwise.
   */
  notifyCallback(callback, results, lastId) {
    if (DEBUG) {
      debug("notifyCallback(results[" + results.length + "], " + lastId + ")");
    }

    if (results.length) {
      callback.notifyCursorResult(results, results.length);
    } else if (lastId == COLLECT_ID_END) {
      callback.notifyCursorDone();
    } else {
      callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
    }
  },
};

function IDsCollector() {
  this.results = [];
  this.done = false;
}
IDsCollector.prototype = {
  results: null,
  requestWaiting: null,
  done: null,

  /**
   * Queue up passed id, reply if necessary.
   *
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param id
   *        COLLECT_ID_END(0) for no more results, COLLECT_ID_ERROR(-1) for
   *        errors and valid otherwise.
   * @param timestamp
   *        We assume this function is always called in timestamp order. So
   *        this parameter is actually unused.
   *
   * @return true if expects more. false otherwise.
   */
  collect(txn, id, timestamp) {
    if (this.done) {
      return false;
    }

    if (DEBUG) {
      debug("IDsCollector::collect ID = " + id);
    }
    // Queue up any id.
    this.results.push(id);
    if (id <= 0) {
      // No more processing on '0' or negative values passed.
      this.done = true;
    }

    if (!this.requestWaiting) {
      if (DEBUG) {
        debug("IDsCollector::squeeze() not called yet");
      }
      return !this.done;
    }

    // We assume there is only one request waiting throughout the message list
    // retrieving process. So we don't bother continuing to process further
    // waiting requests here. This assumption comes from DOMCursor::Continue()
    // implementation.
    let callback = this.requestWaiting;
    this.requestWaiting = null;

    this.drip(txn, callback);

    return !this.done;
  },

  /**
   * Callback right away with the first queued result entry if the filtering is
   * done. Or queue up the request and callback when a new entry is available.
   *
   * @param callback
   *        A callback function that accepts a numeric id.
   */
  squeeze(callback) {
    if (this.requestWaiting) {
      throw new Error("Already waiting for another request!");
    }

    if (!this.done) {
      // Database transaction ongoing, let it reply for us so that we won't get
      // blocked by the existing transaction.
      this.requestWaiting = callback;
      return;
    }

    this.drip(null, callback);
  },

  /**
   * @param txn
   *        Ongoing IDBTransaction context object or null.
   * @param callback
   *        A callback function that accepts a numeric id.
   */
  drip(txn, callback) {
    let firstId = this.results[0];
    if (firstId > 0) {
      this.results.shift();
    }
    callback(txn, firstId);
  },
};

function IntersectionResultsCollector(collect, reverse) {
  this.cascadedCollect = collect;
  this.reverse = reverse;
  this.contexts = [];
}
IntersectionResultsCollector.prototype = {
  cascadedCollect: null,
  reverse: false,
  contexts: null,

  /**
   * Queue up {id, timestamp} pairs, find out intersections and report to
   * |cascadedCollect|. Return true if it is still possible to have another match.
   */
  collect(contextIndex, txn, id, timestamp) {
    if (DEBUG) {
      debug(
        "IntersectionResultsCollector: " +
          contextIndex +
          ", " +
          id +
          ", " +
          timestamp
      );
    }

    let contexts = this.contexts;
    let context = contexts[contextIndex];

    if (id < 0) {
      // Act as no more matched records.
      id = 0;
    }
    if (!id) {
      context.done = true;

      if (!context.results.length) {
        // Already empty, can't have further intersection results.
        return this.cascadedCollect(
          txn,
          COLLECT_ID_END,
          COLLECT_TIMESTAMP_UNUSED
        );
      }

      for (let i = 0; i < contexts.length; i++) {
        if (!contexts[i].done) {
          // Don't call |this.cascadedCollect| because |context.results| might not
          // be empty, so other contexts might still have a chance here.
          return false;
        }
      }

      // It was the last processing context and is no more processing.
      return this.cascadedCollect(
        txn,
        COLLECT_ID_END,
        COLLECT_TIMESTAMP_UNUSED
      );
    }

    // Search id in other existing results. If no other results has it,
    // and A) the last timestamp is smaller-equal to current timestamp,
    // we wait for further results; either B) record timestamp is larger
    // then current timestamp or C) no more processing for a filter, then we
    // drop this id because there can't be a match anymore.
    for (let i = 0; i < contexts.length; i++) {
      if (i == contextIndex) {
        continue;
      }

      let ctx = contexts[i];
      let results = ctx.results;
      let found = false;
      for (let j = 0; j < results.length; j++) {
        let result = results[j];
        if (result.id == id) {
          found = true;
          break;
        }
        if (
          (!this.reverse && result.timestamp > timestamp) ||
          (this.reverse && result.timestamp < timestamp)
        ) {
          // B) Cannot find a match anymore. Drop.
          return true;
        }
      }

      if (!found) {
        if (ctx.done) {
          // C) Cannot find a match anymore. Drop.
          if (results.length) {
            let lastResult = results[results.length - 1];
            if (
              (!this.reverse && lastResult.timestamp >= timestamp) ||
              (this.reverse && lastResult.timestamp <= timestamp)
            ) {
              // Still have a chance to get another match. Return true.
              return true;
            }
          }

          // Impossible to find another match because all results in ctx have
          // timestamps smaller than timestamp.
          context.done = true;
          return this.cascadedCollect(
            txn,
            COLLECT_ID_END,
            COLLECT_TIMESTAMP_UNUSED
          );
        }

        // A) Pending.
        context.results.push({
          id,
          timestamp,
        });
        return true;
      }
    }

    // Now id is found in all other results. Report it.
    return this.cascadedCollect(txn, id, timestamp);
  },

  newContext() {
    let contextIndex = this.contexts.length;
    this.contexts.push({
      results: [],
      done: false,
    });
    return this.collect.bind(this, contextIndex);
  },
};

function UnionResultsCollector(collect) {
  this.cascadedCollect = collect;
  this.contexts = [
    {
      // Timestamp.
      processing: 1,
      results: [],
    },
    {
      processing: 0,
      results: [],
    },
  ];
}
UnionResultsCollector.prototype = {
  cascadedCollect: null,
  contexts: null,

  collect(contextIndex, txn, id, timestamp) {
    if (DEBUG) {
      debug(
        "UnionResultsCollector: " + contextIndex + ", " + id + ", " + timestamp
      );
    }

    let contexts = this.contexts;
    let context = contexts[contextIndex];

    if (id < 0) {
      // Act as no more matched records.
      id = 0;
    }
    if (id) {
      if (!contextIndex) {
        // Timestamp.
        context.results.push({
          id,
          timestamp,
        });
      } else {
        context.results.push(id);
      }
      return true;
    }

    context.processing -= 1;
    if (contexts[0].processing || contexts[1].processing) {
      // At least one queue is still processing, but we got here because
      // current cursor gives 0 as id meaning no more messages are
      // available. Return false here to stop further cursor.continue() calls.
      return false;
    }

    let tres = contexts[0].results;
    let qres = contexts[1].results;
    tres = tres.filter(function(element) {
      return qres.includes(element.id);
    });

    for (let i = 0; i < tres.length; i++) {
      this.cascadedCollect(txn, tres[i].id, tres[i].timestamp);
    }
    this.cascadedCollect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);

    return false;
  },

  newTimestampContext() {
    return this.collect.bind(this, 0);
  },

  newContext() {
    this.contexts[1].processing++;
    return this.collect.bind(this, 1);
  },
};

function GetMessagesCursor(mmdb, callback) {
  this.mmdb = mmdb;
  this.callback = callback;
  this.collector = new ResultsCollector(this.getMessage.bind(this));
}
GetMessagesCursor.prototype = {
  classID: RIL_GETMESSAGESCURSOR_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICursorContinueCallback]),

  mmdb: null,
  callback: null,
  collector: null,

  getMessageTxn(txn, messageStore, messageId, collector) {
    if (DEBUG) {
      debug("Fetching message " + messageId);
    }

    let getRequest = messageStore.get(messageId);
    let self = this;
    getRequest.onsuccess = function(event) {
      if (DEBUG) {
        debug("notifyNextMessageInListGot - messageId: " + messageId);
      }
      let domMessage = self.mmdb.createDomMessageFromRecord(
        event.target.result
      );
      collector.notifyResult(txn, messageId, domMessage);
    };
    getRequest.onerror = function(event) {
      // Error reporting is done in ResultsCollector.notifyCallback.
      event.stopPropagation();
      event.preventDefault();

      if (DEBUG) {
        debug("notifyCursorError - messageId: " + messageId);
      }
      collector.notifyResult(txn, messageId, null);
    };
  },

  getMessage(txn, messageId, collector) {
    // When filter transaction is not yet completed, we're called with current
    // ongoing transaction object.
    if (txn) {
      let messageStore = txn.objectStore(MESSAGE_STORE_NAME);
      this.getMessageTxn(txn, messageStore, messageId, collector);
      return;
    }

    // Or, we have to open another transaction ourselves.
    let self = this;
    this.mmdb.newTxn(
      READ_ONLY,
      function(error, txn, messageStore) {
        if (error) {
          debug("getMessage: failed to create new transaction");
          collector.notifyResult(null, messageId, null);
        } else {
          self.getMessageTxn(txn, messageStore, messageId, collector);
        }
      },
      [MESSAGE_STORE_NAME]
    );
  },

  // nsICursorContinueCallback

  handleContinue() {
    if (DEBUG) {
      debug("Getting next message in list");
    }
    this.collector.squeeze(this.callback);
  },
};

function GetThreadsCursor(mmdb, callback) {
  this.mmdb = mmdb;
  this.callback = callback;
  this.collector = new ResultsCollector(this.getThread.bind(this));
}
GetThreadsCursor.prototype = {
  classID: RIL_GETTHREADSCURSOR_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsICursorContinueCallback]),

  mmdb: null,
  callback: null,
  collector: null,

  getThreadTxn(txn, threadStore, threadId, collector) {
    if (DEBUG) {
      debug("Fetching thread " + threadId);
    }

    let getRequest = threadStore.get(threadId);
    getRequest.onsuccess = function(event) {
      let threadRecord = event.target.result;
      if (DEBUG) {
        debug("notifyCursorResult: " + JSON.stringify(threadRecord));
      }
      let thread = lazy.gMobileMessageService.createThread(
        threadRecord.id,
        threadRecord.participantAddresses,
        threadRecord.lastTimestamp,
        threadRecord.lastMessageSubject || "",
        threadRecord.body,
        threadRecord.unreadCount,
        threadRecord.lastMessageType,
        threadRecord.isGroup,
        threadRecord.lastMessageAttachementStatus || ATTACHMENT_STATUS_NONE
      );
      collector.notifyResult(txn, threadId, thread);
    };
    getRequest.onerror = function(event) {
      // Error reporting is done in ResultsCollector.notifyCallback.
      event.stopPropagation();
      event.preventDefault();

      if (DEBUG) {
        debug("notifyCursorError - threadId: " + threadId);
      }
      collector.notifyResult(txn, threadId, null);
    };
  },

  getThread(txn, threadId, collector) {
    // When filter transaction is not yet completed, we're called with current
    // ongoing transaction object.
    if (txn) {
      let threadStore = txn.objectStore(THREAD_STORE_NAME);
      this.getThreadTxn(txn, threadStore, threadId, collector);
      return;
    }

    // Or, we have to open another transaction ourselves.
    let self = this;
    this.mmdb.newTxn(
      READ_ONLY,
      function(error, txn, threadStore) {
        if (error) {
          collector.notifyResult(null, threadId, null);
        } else {
          self.getThreadTxn(txn, threadStore, threadId, collector);
        }
      },
      [THREAD_STORE_NAME]
    );
  },

  // nsICursorContinueCallback

  handleContinue() {
    if (DEBUG) {
      debug("Getting next thread in list");
    }
    this.collector.squeeze(this.callback);
  },
};

const EXPORTED_SYMBOLS = ["MobileMessageDB"];

function debug(s) {
  //dump("MobileMessageDB: " + Array.slice(arguments).join(" ") + "\n");
  dump("MobileMessageDB: " + s + "\n");
}
