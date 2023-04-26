/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const GONK_VOICEMAIL_SERVICE_CID = Components.ID(
  "{c332f318-1cce-4f02-b676-bb5031d10736}"
);

const NS_MOBILE_CONNECTION_SERVICE_CONTRACTID =
  "@mozilla.org/mobileconnection/mobileconnectionservice;1";

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefDefaultServiceId = "dom.voicemail.defaultServiceId";

var DEBUG;
function debug(s) {
  dump("VoicemailService: " + s);
}

function VoicemailProvider(aServiceId) {
  this.serviceId = aServiceId;
}
VoicemailProvider.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIVoicemailProvider]),

  // nsIVoicemail interface

  serviceId: 0,

  number: null,
  displayName: null,

  hasMessages: false,
  messageCount: 0,
  returnNumber: null,
  returnMessage: null,
};

function VoicemailService() {
  // Initialize |this._providers|.
  let mcService = Cc[NS_MOBILE_CONNECTION_SERVICE_CONTRACTID].getService(
    Ci.nsIMobileConnectionService
  );
  let numItems = mcService.numItems;
  this._providers = [];
  for (let i = 0; i < numItems; i++) {
    this._providers.push(new VoicemailProvider(i));
  }

  this._listeners = [];

  // Must be initialized after |this._providers|.
  this._defaultServiceId = this._getDefaultServiceId();
  this._updateDebugFlag();

  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  Services.prefs.addObserver(kPrefDefaultServiceId, this);

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}

VoicemailService.prototype = {
  classID: GONK_VOICEMAIL_SERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIVoicemailService,
    Ci.nsIGonkVoicemailService,
    Ci.nsIObserver,
  ]),

  _defaultServiceId: null,
  _providers: null,

  _updateDebugFlag() {
    try {
      DEBUG =
        RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  _getDefaultServiceId() {
    // TODO mark and set 0 first.
    //let id = Services.prefs.getIntPref(kPrefDefaultServiceId);
    let id = 0;
    if (id >= this.numItems || id < 0) {
      id = 0;
    }

    return id;
  },

  _listeners: null,

  _notifyListeners(aMethodName, aItem) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      try {
        listener[aMethodName](listener, aItem);
      } catch (e) {
        if (DEBUG) {
          debug("listener for " + aMethodName + " threw an exception: " + e);
        }
      }
    }
  },

  /**
   * nsIVoicemailService interface
   */

  get numItems() {
    return this._providers.length;
  },

  getItemByServiceId(aServiceId) {
    let provider = this._providers[aServiceId];
    if (!provider) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }
    return provider;
  },

  getDefaultItem() {
    return this.getItemByServiceId(this._defaultServiceId);
  },

  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index < 0) {
      return Cr.NS_ERROR_FAILURE;
    }

    this._listeners.splice(index, 1);
    return Cr.NS_OK;
  },

  /**
   * nsIGonkVoicemailService interface
   */

  notifyStatusChanged(
    aServiceId,
    aHasMessages,
    aMessageCount,
    aReturnNumber,
    aReturnMessage
  ) {
    if (DEBUG) {
      debug(
        "notifyStatusChanged: " +
          JSON.stringify(Array.prototype.slice.call(arguments))
      );
    }

    let provider = this.getItemByServiceId(aServiceId);

    let changed = false;
    if (provider.hasMessages != aHasMessages) {
      provider.hasMessages = aHasMessages;
      changed = true;
    }
    if (provider.messageCount != aMessageCount) {
      provider.messageCount = aMessageCount;
      changed = true;
    } else if (aMessageCount == -1) {
      // For MWI using DCS the message count is not available
      changed = true;
    }
    if (provider.returnNumber != aReturnNumber) {
      provider.returnNumber = aReturnNumber;
      changed = true;
    }
    if (provider.returnMessage != aReturnMessage) {
      provider.returnMessage = aReturnMessage;
      changed = true;
    }

    if (changed) {
      this._notifyListeners("notifyStatusChanged", provider);
    }
  },

  notifyInfoChanged(aServiceId, aNumber, aDisplayName) {
    if (DEBUG) {
      debug(
        "notifyInfoChanged: " +
          JSON.stringify(Array.prototype.slice.call(arguments))
      );
    }

    let provider = this.getItemByServiceId(aServiceId);

    let changed = false;
    if (provider.number != aNumber) {
      provider.number = aNumber;
      changed = true;
    }
    if (provider.displayName != aDisplayName) {
      provider.displayName = aDisplayName;
      changed = true;
    }

    if (changed) {
      this._notifyListeners("notifyInfoChanged", provider);
    }
  },

  /**
   * nsIObserver interface.
   */

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          this._updateDebugFlag();
        } else if (aData === kPrefDefaultServiceId) {
          this._defaultServiceId = this._getDefaultServiceId();
        }
        break;

      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
        Services.prefs.removeObserver(kPrefDefaultServiceId, this);

        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

        // Remove all listeners.
        this._listeners = [];
        break;
    }
  },
};

var EXPORTED_SYMBOLS = ["VoicemailService"];
