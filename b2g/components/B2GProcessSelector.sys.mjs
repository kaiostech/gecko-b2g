/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Process selector for b2g.

export function B2GProcessSelector() {
  Services.obs.addObserver(embedderSelector => {
    this.embedderSelector = embedderSelector.wrappedJSObject;
  }, "web-embedder-set-process-selector");
}

B2GProcessSelector.prototype = {
  classID: Components.ID("{dd87f882-9d09-49e5-989d-cfaaaf4425be}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentProcessProvider]),

  provideProcess(aType, aProcesses, aMaxCount) {
    // Delegates to the embedder if possible, or just defaults to create a new process.
    if (!this.embedderSelector) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }
    return this.embedderSelector.provideProcess(
      aType,
      aProcesses.map(embeddableProcessInfo),
      aMaxCount
    );
  },

  suggestServiceWorkerProcess(aScope) {
    // If delegate is not presented, return 0 as default
    // (select a hosting content process randomly).
    if (!this.embedderSelector) {
      return 0;
    }
    return this.embedderSelector.suggestServiceWorkerProcess(aScope);
  },
};

// Provides an xpcom-free version of a nsIContentProcessInfo instance:
// - Don't expose messageManager
// - Turn nsIURI into a web compatible URL()
export function embeddableProcessInfo(processInfo) {
  let safeInfo = {
    isAlive: processInfo.isAlive,
    tabCount: processInfo.tabCount,
    willShutDown: processInfo.willShutDown,
    serviceWorkerCount: processInfo.serviceWorkerCount,
  };

  try {
    safeInfo.processId = processInfo.processId;
  } catch (e) {
    safeInfo.processId = -1;
  }

  safeInfo.tabURIs = [];
  processInfo.tabURIs.forEach(uri => {
    safeInfo.tabURIs.push(new URL(uri.spec));
  });

  safeInfo.serviceWorkerURIs = [];
  processInfo.serviceWorkerURIs.forEach(uri => {
    safeInfo.serviceWorkerURIs.push(new URL(uri.spec));
  });

  return safeInfo;
}
