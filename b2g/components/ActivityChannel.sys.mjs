/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export let ActivityChannel = function(aURI, aLoadInfo, aName, aDetails) {
  this._activityName = aName;
  this._activityDetails = aDetails;
  this.originalURI = aURI;
  this.URI = aURI;
  this.loadInfo = aLoadInfo;
};

ActivityChannel.prototype = {
  originalURI: null,
  URI: null,
  owner: null,
  notificationCallbacks: null,
  securityInfo: null,
  contentType: null,
  contentCharset: null,
  contentLength: 0,
  contentDisposition: Ci.nsIChannel.DISPOSITION_INLINE,
  contentDispositionFilename: null,
  contentDispositionHeader: null,
  loadInfo: null,

  open() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  asyncOpen(aListener, aContext) {
    Services.cpmm.sendAsyncMessage(this._activityName, this._activityDetails);
    // Let the listener cleanup.
    aListener.onStartRequest(this);
    aListener.onStopRequest(this, Cr.NS_OK);
  },

  QueryInterface2: ChromeUtils.generateQI([Ci.nsIChannel]),
};
