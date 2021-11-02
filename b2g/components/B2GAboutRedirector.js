/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

function debug(msg) {
  console.log("B2GAboutRedirector: " + msg);
}

function netErrorURL() {
  let systemManifestURL = Services.prefs.getCharPref("b2g.system_startup_url");
  systemManifestURL = Services.io.newURI(systemManifestURL);
  let netErrorURL = Services.prefs.getCharPref("b2g.neterror.url");
  netErrorURL = Services.io.newURI(netErrorURL, null, systemManifestURL);
  return netErrorURL.spec;
}

var modules = {
  certerror: {
    uri: "chrome://b2g/content/aboutCertError.xhtml",
    privileged: true,
    hide: true,
  },
  neterror: {
    uri: netErrorURL(),
    privileged: true,
    hide: true,
  },
  reader: {
    // Use the same uri and flags as in browser/components/about/AboutRedirector.cpp
    uri: "chrome://global/content/reader/aboutReader.html",
    flags:
      Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
      Ci.nsIAboutModule.ALLOW_SCRIPT |
      Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
      Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT,
    privileged: true,
    hide: true,
  },
};

function B2GAboutRedirector() {}
B2GAboutRedirector.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
  classID: Components.ID("{920400b1-cf8f-4760-a9c4-441417b15134}"),

  _getModuleInfo(aURI) {
    try {
      let moduleName = aURI.spec
        .replace(/[?#].*/, "")
        .toLowerCase()
        .split(":")[1];
      return modules[moduleName];
    } catch (e) {
      return null;
    }
  },

  // nsIAboutModule
  getURIFlags(aURI) {
    let flags = 0;
    let moduleInfo = this._getModuleInfo(aURI);

    // If custom flags are set, just use them.
    if (moduleInfo.flags) {
      return moduleInfo.flags;
    }

    if (moduleInfo.hide) {
      flags = Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT;
    }

    return flags | Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel(aURI, aLoadInfo) {
    let moduleInfo = this._getModuleInfo(aURI);

    if (!moduleInfo) {
      return null;
    }

    var ios = Services.io;
    var newURI = ios.newURI(moduleInfo.uri);
    var channel = ios.newChannelFromURIWithLoadInfo(newURI, aLoadInfo);

    if (!moduleInfo.privileged) {
      // Setting the owner to null means that we'll go through the normal
      // path in GetChannelPrincipal and create a codebase principal based
      // on the channel's originalURI
      channel.owner = null;
    }

    channel.originalURI = aURI;

    return channel;
  },
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([B2GAboutRedirector]);
