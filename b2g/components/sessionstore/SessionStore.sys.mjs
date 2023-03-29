/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export var SessionStore = {
  updateSessionStoreFromTablistener(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aData
  ) {
    // No return value expected.
  },

  // Used by remote/cdp/domains/parent/Page.jsm
  getSessionHistory(tab, updatedCallback) {
    // Don't do anything for now.
  },

  // Used by browser-custom-element.js
  maybeExitCrashedState() {},
};

// Freeze the SessionStore object. We don't want anyone to modify it.
Object.freeze(SessionStore);
