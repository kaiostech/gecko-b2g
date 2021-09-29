/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Provides support for ipfs:// and ipns:// urls.
//

"use strict";

var EXPORTED_SYMBOLS = ["IpfsProtocolHandler", "IpnsProtocolHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const GATEWAY_DOMAIN = Services.prefs.getCharPref("ipfs.gateway", "dweb.link");

// Protocol handler using a rewriting into a gateway http url.
class IpfsRedirectProtocolHandler {
  constructor(scheme) {
    this.scheme = scheme;
    // this.log(`constructor ${scheme}`);
  }

  get defaultPort() {
    return -1;
  }

  get allowPort() {
    return false;
  }

  get protocolFlags() {
    let flags = Ci.nsIProtocolHandler;
    return (
      flags.URI_NOAUTH |
      flags.URI_LOADABLE_BY_ANYONE |
      flags.URI_IS_POTENTIALLY_TRUSTWORTHY |
      flags.URI_FETCHABLE_BY_ANYONE |
      flags.URI_LOADABLE_BY_EXTENSIONS
    );
  }

  newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  }

  newChannel(aURI, aLoadInfo) {
    // this.log(`newChannel ${aURI.spec}`);

    // Mapping to gateway url:
    // ipfs://bafybeiemxf5abjwjbikoz4mc3a3dla6ual3jsgpdr4cjr3oz3evfyavhwq/wiki/Vincent_van_Gogh.html ->
    // https://bafybeiemxf5abjwjbikoz4mc3a3dla6ual3jsgpdr4cjr3oz3evfyavhwq.ipfs.dweb.link/wiki/Vincent_van_Gogh.html

    let host = this.transformHost(aURI.host);
    let gatewayUrl = `https://${host}.${this.scheme}.${GATEWAY_DOMAIN}${aURI.pathQueryRef}`;
    this.log(`gateway url: ${gatewayUrl}`)
    let channel = Services.io.newChannelFromURIWithLoadInfo(
      Services.io.newURI(gatewayUrl),
      aLoadInfo
    );
    channel.originalURI = aURI;
    return channel;
  }

  log(msg) {
    console.log(`IpfsRedirectProtocolHandler: ${msg}`);
  }
}

// ipfs:// protocol handler.
class IpfsProtocolHandler extends IpfsRedirectProtocolHandler {
  constructor() {
    super("ipfs");
  }

  transformHost(host) {
    return host;
  }
}

IpfsProtocolHandler.prototype.classID = Components.ID(
  "{e02a7c56-20e9-49a8-b436-ce3aad89c61b}"
);

IpfsProtocolHandler.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIProtocolHandler",
]);

// ipns:// protocol handler.
class IpnsProtocolHandler extends IpfsRedirectProtocolHandler {
  constructor() {
    super("ipns");
  }

  transformHost(host) {
    return host.replace(/\./g, "-");
  }
}

IpnsProtocolHandler.prototype.classID = Components.ID(
  "{210acdce-f495-4ac3-8821-59ac94d57e32}"
);

IpnsProtocolHandler.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIProtocolHandler",
]);
