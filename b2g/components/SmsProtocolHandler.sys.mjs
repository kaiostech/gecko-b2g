/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SmsProtocolHandle.js
 *
 * This file implements the URLs for SMS
 * https://www.rfc-editor.org/rfc/rfc5724.txt
 */

import { TelURIParser } from "resource:///modules/TelURIParser.sys.mjs";

import { ActivityChannel } from "resource://gre/modules/ActivityChannel.sys.mjs";

export function SmsProtocolHandler() {}

SmsProtocolHandler.prototype = {
  scheme: "sms",
  allowPort: () => false,

  newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel(aURI, aLoadInfo) {
    let number = TelURIParser.parseURI("sms", aURI.spec);
    let body = "";
    let query = aURI.spec.split("?")[1];

    if (query) {
      let params = query.split("&");
      params.forEach(function(aParam) {
        let [name, value] = aParam.split("=");
        if (name === "body") {
          body = decodeURIComponent(value);
        }
      });
    }

    if (number || body) {
      return new ActivityChannel(aURI, aLoadInfo, "sms-handler", {
        number: number || "",
        type: "websms/sms",
        body,
      });
    }

    throw Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
  },

  classID: Components.ID("{81ca20cb-0dad-4e32-8566-979c8998bd73}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler]),
};
