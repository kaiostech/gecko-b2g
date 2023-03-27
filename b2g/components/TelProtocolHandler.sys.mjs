/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TelProtocolHandle.js
 *
 * This file implements the URLs for Telephone Calls
 * https://www.ietf.org/rfc/rfc2806.txt
 */

import { TelURIParser } from "resource:///modules/TelURIParser.sys.mjs";

import { ActivityChannel } from "resource://gre/modules/ActivityChannel.sys.mjs";

export function TelProtocolHandler() {}

TelProtocolHandler.prototype = {
  scheme: "tel",
  allowPort: () => false,

  newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel(aURI, aLoadInfo) {
    let number = TelURIParser.parseURI("tel", aURI.spec);
    if (number) {
      return new ActivityChannel(aURI, aLoadInfo, "dial-handler", {
        number,
        type: "webtelephony/number",
      });
    }

    throw Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
  },

  classID: Components.ID("{782775dd-7351-45ea-aff1-0ffa872cfdd2}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler]),
};
