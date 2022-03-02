/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AlarmService } = ChromeUtils.import(
  "resource://gre/modules/AlarmService.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["AppsUtils"];

function debug(msg) {
  dump(`AppsUtils.js: ${msg}\n`);
}

this.AppsUtils = {
  clearData(url) {
    debug("clearData: " + url);
    let uri = Services.io.newURI(url);
    const kFlags =
      Ci.nsIClearDataService.CLEAR_COOKIES |
      Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
      Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
      Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
      Ci.nsIClearDataService.CLEAR_EME |
      Ci.nsIClearDataService.CLEAR_ALL_CACHES;

    // This will clear cache, cookie etc as defined in the kFlags.
    Services.clearData.deleteDataFromHost(uri.host, true, kFlags, result => {
      debug("result: " + result);
    });

    AlarmService.removeByHost(uri.host);
  },
};
