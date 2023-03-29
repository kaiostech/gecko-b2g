/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AlarmService } from "resource://gre/modules/AlarmService.sys.mjs";

function debug(msg) {
  dump(`AppsUtils.js: ${msg}\n`);
}

export const AppsUtils = {
  clearData(url) {
    return new Promise(resolve => {
      debug("clearData: " + url);
      let uri = Services.io.newURI(url);
      const kFlags =
        Ci.nsIClearDataService.CLEAR_COOKIES |
        Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
        Ci.nsIClearDataService.CLEAR_CLIENT_AUTH_REMEMBER_SERVICE |
        Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
        Ci.nsIClearDataService.CLEAR_EME |
        Ci.nsIClearDataService.CLEAR_ALL_CACHES;

      // This will clear cache, cookie etc as defined in the kFlags.
      Services.clearData.deleteDataFromHost(uri.host, true, kFlags, result => {
        debug("result: " + result);
        resolve(result);
      });

      AlarmService.removeByHost(uri.host);
    });
  },
};
