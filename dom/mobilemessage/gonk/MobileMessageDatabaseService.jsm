/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { MobileMessageDB } = ChromeUtils.import(
  "resource://gre/modules/MobileMessageDB.jsm"
);

const GONK_MOBILEMESSAGEDATABASESERVICE_CID = Components.ID(
  "{7db05024-8038-11e4-b7fa-a3edb6f1bf0c}"
);

const DB_NAME = "sms";

/**
 * MobileMessageDatabaseService
 */
function MobileMessageDatabaseService() {
  // Prime the directory service's cache to ensure that the ProfD entry exists
  // by the time IndexedDB queries for it off the main thread. (See bug 743635.)
  Services.dirsvc.get("ProfD", Ci.nsIFile);

  let mmdb = new MobileMessageDB();
  mmdb.init(DB_NAME, 0, mmdb.updatePendingTransactionToError.bind(mmdb));
  this.mmdb = mmdb;
}
MobileMessageDatabaseService.prototype = {
  classID: GONK_MOBILEMESSAGEDATABASESERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIGonkMobileMessageDatabaseService,
    Ci.nsIMobileMessageDatabaseService,
    Ci.nsIObserver,
  ]),

  /**
   * MobileMessageDB instance.
   */
  mmdb: null,

  /**
   * nsIObserver
   */
  observe() {},

  /**
   * nsIGonkMobileMessageDatabaseService API
   */

  saveReceivedMessage(aMessage, aCallback) {
    this.mmdb.saveReceivedMessage(aMessage, aCallback);
  },

  saveSendingMessage(aMessage, aCallback) {
    this.mmdb.saveSendingMessage(aMessage, aCallback);
  },

  setMessageDeliveryByMessageId(
    aMessageId,
    aReceiver,
    aDelivery,
    aDeliveryStatus,
    aEnvelopeId,
    aCallback
  ) {
    this.mmdb.updateMessageDeliveryById(
      aMessageId,
      "messageId",
      aReceiver,
      aDelivery,
      aDeliveryStatus,
      aEnvelopeId,
      aCallback
    );
  },

  setMessageDeliveryStatusByEnvelopeId(
    aEnvelopeId,
    aReceiver,
    aDeliveryStatus,
    aCallback
  ) {
    this.mmdb.updateMessageDeliveryById(
      aEnvelopeId,
      "envelopeId",
      aReceiver,
      null,
      aDeliveryStatus,
      null,
      aCallback
    );
  },

  setMessageReadStatusByEnvelopeId(
    aEnvelopeId,
    aReceiver,
    aReadStatus,
    aCallback
  ) {
    this.mmdb.setMessageReadStatusByEnvelopeId(
      aEnvelopeId,
      aReceiver,
      aReadStatus,
      aCallback
    );
  },

  getMessageRecordByTransactionId(aTransactionId, aCallback) {
    this.mmdb.getMessageRecordByTransactionId(aTransactionId, aCallback);
  },

  getMessageRecordById(aMessageId, aCallback) {
    this.mmdb.getMessageRecordById(aMessageId, aCallback);
  },

  translateCrErrorToMessageCallbackError(aCrError) {
    return this.mmdb.translateCrErrorToMessageCallbackError(aCrError);
  },

  saveSmsSegment(aSmsSegment, aCallback) {
    this.mmdb.saveSmsSegment(aSmsSegment, aCallback);
  },

  saveCellBroadcastMessage(aCellBroadcastMessage, aCallback) {
    this.mmdb.saveCellBroadcastMessage(aCellBroadcastMessage, aCallback);
  },

  getCellBroadcastMessage(aSerialNumber, aMessageIdentifier, aCallback) {
    this.mmdb.getCellBroadcastMessage(
      aSerialNumber,
      aMessageIdentifier,
      aCallback
    );
  },

  deleteCellBroadcastMessage(aSerialNumber, aMessageIdentifier, aCallback) {
    this.mmdb.deleteCellBroadcastMessage(
      aSerialNumber,
      aMessageIdentifier,
      aCallback
    );
  },

  /**
   * nsIMobileMessageDatabaseService API
   */

  getMessage(aMessageId, aRequest) {
    this.mmdb.getMessage(aMessageId, aRequest);
  },

  deleteMessage(aMessageIds, aLength, aRequest) {
    this.mmdb.deleteMessage(aMessageIds, aLength, aRequest);
  },

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
    return this.mmdb.createMessageCursor(
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
    );
  },

  markMessageRead(aMessageId, aValue, aSendReadReport, aRequest) {
    this.mmdb.markMessageRead(aMessageId, aValue, aSendReadReport, aRequest);
  },

  createThreadCursor(aCallback) {
    return this.mmdb.createThreadCursor(aCallback);
  },
};

var EXPORTED_SYMBOLS = ["MobileMessageDatabaseService"];
