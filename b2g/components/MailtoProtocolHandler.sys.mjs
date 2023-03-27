/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ActivityChannel } from "resource://gre/modules/ActivityChannel.sys.mjs";

export function MailtoProtocolHandler() {}

MailtoProtocolHandler.prototype = {
  scheme: "mailto",
  allowPort: () => false,

  newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel(aURI, aLoadInfo) {
    return new ActivityChannel(aURI, aLoadInfo, "mail-handler", {
      URI: aURI.spec,
      type: "mail",
    });
  },

  classID: Components.ID("{50777e53-0331-4366-a191-900999be386c}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler]),
};
