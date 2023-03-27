/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

// const {} = ChromeUtils.import("resource://gre/modules/DOMRequestHelper.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "GlobalSimulatorScreen",
  "resource://gre/modules/GlobalSimulatorScreen.jsm"
);

var DEBUG_PREFIX = "SimulatorScreen.js - ";
function debug() {
  //dump(DEBUG_PREFIX + Array.slice(arguments) + '\n');
}

function fireOrientationEvent(window) {
  let e = new window.Event("mozorientationchange");
  window.screen.dispatchEvent(e);
}

function hookScreen(window) {
  let nodePrincipal = window.document.nodePrincipal;
  let origin = nodePrincipal.origin;
  if (nodePrincipal.appStatus == nodePrincipal.APP_STATUS_NOT_INSTALLED) {
    // Only inject screen mock for apps
    return;
  }

  let screen = window.wrappedJSObject.screen;

  screen.mozLockOrientation = function(orientation) {
    debug("mozLockOrientation:", orientation, "from", origin);

    // Normalize and do some checks against orientation input
    if (typeof orientation == "string") {
      orientation = [orientation];
    }

    function isInvalidOrientationString(str) {
      return (
        typeof str != "string" ||
        !str.match(/^default$|^(portrait|landscape)(-(primary|secondary))?$/)
      );
    }
    if (
      !Array.isArray(orientation) ||
      orientation.some(isInvalidOrientationString)
    ) {
      console.error('Invalid orientation "', orientation, '"');
      return false;
    }

    GlobalSimulatorScreen.lock(orientation);

    return true;
  };

  screen.mozUnlockOrientation = function() {
    debug("mozOrientationUnlock from", origin);
    GlobalSimulatorScreen.unlock();
    return true;
  };

  Object.defineProperty(screen, "width", {
    get: () => GlobalSimulatorScreen.width,
  });
  Object.defineProperty(screen, "height", {
    get: () => GlobalSimulatorScreen.height,
  });
  Object.defineProperty(screen, "mozOrientation", {
    get: () => GlobalSimulatorScreen.mozOrientation,
  });
  Object.defineProperty(screen.orientation, "type", {
    get: () => GlobalSimulatorScreen.mozOrientation,
  });
}

function SimulatorScreen() {}
SimulatorScreen.prototype = {
  classID: Components.ID("{c83c02c0-5d43-4e3e-987f-9173b313e880}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
  _windows: new Map(),

  observe(subject, topic, data) {
    let windows = this._windows;
    switch (topic) {
      case "profile-after-change":
        Services.obs.addObserver(this, "document-element-inserted");
        Services.obs.addObserver(this, "simulator-orientation-change");
        Services.obs.addObserver(this, "inner-window-destroyed");
        break;

      case "document-element-inserted":
        let window = subject.defaultView;
        if (!window) {
          return;
        }

        hookScreen(window);

        let innerId = window.windowGlobalChild.innerWindowId;
        windows.set(innerId, window);
        break;

      case "inner-window-destroyed":
        windows.delete(subject.QueryInterface(Ci.nsISupportsPRUint64).data);
        break;

      case "simulator-orientation-change":
        windows.forEach(fireOrientationEvent);
        break;
    }
  },
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([SimulatorScreen]);
