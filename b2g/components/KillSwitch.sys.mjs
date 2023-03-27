/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = false;

function debug(s) {
  dump("-*- KillSwitch.js: " + s + "\n");
}

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "cpmm",
  "@mozilla.org/childprocessmessagemanager;1",
  "nsIMessageSender"
);

const KILLSWITCH_CID = "{b6eae5c6-971c-4772-89e5-5df626bf3f09}";
const KILLSWITCH_CONTRACTID = "@mozilla.org/moz-kill-switch;1";

const kEnableKillSwitch = "KillSwitch:Enable";
const kEnableKillSwitchOK = "KillSwitch:Enable:OK";
const kEnableKillSwitchKO = "KillSwitch:Enable:KO";

const kDisableKillSwitch = "KillSwitch:Disable";
const kDisableKillSwitchOK = "KillSwitch:Disable:OK";
const kDisableKillSwitchKO = "KillSwitch:Disable:KO";

export function KillSwitch() {
  this._window = null;
}

KillSwitch.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init(aWindow) {
    DEBUG && debug("init");
    this._window = aWindow;
    this.initDOMRequestHelper(this._window);
  },

  enable() {
    DEBUG && debug("KillSwitch: enable");

    lazy.cpmm.addMessageListener(kEnableKillSwitchOK, this);
    lazy.cpmm.addMessageListener(kEnableKillSwitchKO, this);
    return this.createPromise((aResolve, aReject) => {
      lazy.cpmm.sendAsyncMessage(kEnableKillSwitch, {
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject,
        }),
      });
    });
  },

  disable() {
    DEBUG && debug("KillSwitch: disable");

    lazy.cpmm.addMessageListener(kDisableKillSwitchOK, this);
    lazy.cpmm.addMessageListener(kDisableKillSwitchKO, this);
    return this.createPromise((aResolve, aReject) => {
      lazy.cpmm.sendAsyncMessage(kDisableKillSwitch, {
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject,
        }),
      });
    });
  },

  receiveMessage(message) {
    DEBUG && debug("Received: " + message.name);

    lazy.cpmm.removeMessageListener(kEnableKillSwitchOK, this);
    lazy.cpmm.removeMessageListener(kEnableKillSwitchKO, this);
    lazy.cpmm.removeMessageListener(kDisableKillSwitchOK, this);
    lazy.cpmm.removeMessageListener(kDisableKillSwitchKO, this);

    let req = this.takePromiseResolver(message.data.requestID);

    switch (message.name) {
      case kEnableKillSwitchKO:
      case kDisableKillSwitchKO:
        req.reject(false);
        break;

      case kEnableKillSwitchOK:
      case kDisableKillSwitchOK:
        req.resolve(true);
        break;

      default:
        DEBUG && debug("Unrecognized message: " + message.name);
        break;
    }
  },

  classID: Components.ID(KILLSWITCH_CID),
  contractID: KILLSWITCH_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIKillSwitch,
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsIObserver,
    Ci.nsIMessageListener,
    Ci.nsISupportsWeakReference,
  ]),
};
