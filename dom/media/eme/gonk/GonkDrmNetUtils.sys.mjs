/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = Services.prefs.getBoolPref("media.b2g.mediadrm.debug", false);

function debug(msg) {
  console.log(`GonkDrmNetUtils: ${msg}`);
}

export function GonkDrmNetUtils() {
  DEBUG && debug(`constructor`);
}

GonkDrmNetUtils.prototype = {
  startProvisioning(url, request, callback) {
    DEBUG && debug(`startProvisioning, url: ${url}, request: ${request}`);

    this._sendPostRequest(url, request)
      .then(response => {
        DEBUG && debug(`startProvisioning, response: ${response}`);
        callback.onSuccess(response);
      })
      .catch(e => {
        DEBUG && debug(`startProvisioning, ${e.message}`);
        callback.onError(e.message);
      });
  },

  async _sendPostRequest(url, request) {
    const response = await fetch(url, {
      method: "post",
      body: JSON.stringify({ signedRequest: request }),
      mode: "cors",
      headers: new Headers({
        "Content-Type": "application/json",
      }),
    });

    if (!response.ok) {
      return Promise.reject(
        new Error(`HTTP error, status: ${response.status}`)
      );
    }
    return response.text();
  },

  contractID: "@mozilla.org/gonkdrm/net-utils;1",

  classID: Components.ID("{90b089d0-c8f4-41cc-8c1a-426c3221b84b}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIGonkDrmNetUtils]),
};
