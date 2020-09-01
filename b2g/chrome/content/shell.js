/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.import("resource://gre/modules/ActivitiesService.jsm");
ChromeUtils.import("resource://gre/modules/AlarmService.jsm");
ChromeUtils.import("resource://gre/modules/DownloadService.jsm");
ChromeUtils.import("resource://gre/modules/NotificationDB.jsm");
ChromeUtils.import("resource://gre/modules/ErrorPage.jsm");
ChromeUtils.import("resource://gre/modules/InputMethodService.jsm");

XPCOMUtils.defineLazyGetter(this, "MarionetteHelper", () => {
  const { MarionetteHelper } = ChromeUtils.import(
    "chrome://b2g/content/devtools/marionette.js"
  );
  return new MarionetteHelper(shell.contentBrowser);
});

const isGonk = AppConstants.platform === "gonk";

if (isGonk) {
  XPCOMUtils.defineLazyGetter(this, "libcutils", () => {
    const { libcutils } = ChromeUtils.import(
      "resource://gre/modules/systemlibs.js"
    );
    return libcutils;
  });
}

try {
  // For external screen rendered by a native buffer, event of display-changed
  // (to tell a display is added), is fired after rendering the first drawble
  // frame. Load the handler asap in order to ensure our system observe that
  // event, and yes this is unfortunately a hack. So try not to delay loading
  // this module.
  if (isGonk && Services.prefs.getBoolPref("b2g.multiscreen.enabled")) {
    ChromeUtils.import("resource://gre/modules/MultiscreenHandler.jsm");
  }
} catch (e) {}

function debug(str) {
  console.log(`-*- Shell.js: ${str}`);
}

var shell = {
  get startURL() {
    let url = Services.prefs.getCharPref("b2g.system_startup_url");
    if (!url) {
      console.error(
        `Please set the b2g.system_startup_url preference properly`
      );
    }
    return url;
  },

  _started: false,
  hasStarted() {
    return this._started;
  },

  start() {
    this._started = true;

    // This forces the initialization of the cookie service before we hit the
    // network.
    // See bug 810209
    let cookies = Cc["@mozilla.org/cookieService;1"].getService();
    if (!cookies) {
      debug("No cookies service!");
    }

    let startURL = this.startURL;

    let systemAppFrame = document.createXULElement("browser");
    systemAppFrame.setAttribute("type", "chrome");
    systemAppFrame.setAttribute("primary", "true");
    systemAppFrame.setAttribute("id", "systemapp");
    systemAppFrame.setAttribute("forcemessagemanager", "true");
    systemAppFrame.setAttribute("nodefaultsrc", "true");

    // Identify this `<browser>` element uniquely to Marionette, devtools, etc.
    systemAppFrame.permanentKey = new (Cu.getGlobalForObject(
      Services
    ).Object)();
    systemAppFrame.linkedBrowser = systemAppFrame;

    document.body.prepend(systemAppFrame);
    window.dispatchEvent(new CustomEvent("systemappframeprepended"));

    this.contentBrowser = systemAppFrame;

    window.addEventListener("MozAfterPaint", this);
    window.addEventListener("sizemodechange", this);
    window.addEventListener("unload", this);

    let stop_url = null;

    // Listen for loading events on the system app xul:browser
    let listener = {
      onLocationChange: (webProgress, request, location, flags) => {
        // debug(`LocationChange: ${location.spec}`);
      },

      onProgressChange: () => {
        // debug(`ProgressChange`);
      },

      onSecurityChange: () => {
        // debug(`SecurityChange`);
      },

      onStateChange: (webProgress, request, stateFlags, status) => {
        // debug(`StateChange ${stateFlags}`);
        if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
          if (!stop_url) {
            stop_url = request.name;
          }
        }

        if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          // debug(`Done loading ${request.name}`);
          if (stop_url && request.name == stop_url) {
            this.contentBrowser.removeProgressListener(listener);
            this.notifyContentWindowLoaded();
          }
        }
      },

      onStatusChange: () => {
        // debug(`StatusChange`);
      },

      QueryInterface: ChromeUtils.generateQI([
        Ci.nsIWebProgressListener2,
        Ci.nsIWebProgressListener,
        Ci.nsISupportsWeakReference,
      ]),
    };
    this.contentBrowser.addProgressListener(listener);

    debug(`Setting system url to ${startURL}`);

    this.contentBrowser.src = startURL;

    try {
      ChromeUtils.import("resource://gre/modules/ExternalAPIService.jsm");
    } catch (e) {}
  },

  stop() {
    window.removeEventListener("unload", this);
    window.removeEventListener("sizemodechange", this);
  },

  handleEvent(event) {
    debug(`event: ${event.type}`);

    // let content = this.contentBrowser.contentWindow;
    switch (event.type) {
      case "sizemodechange":
        // Due to bug 4657, the default behavior of video/audio playing from web
        // sites should be paused when this browser tab has sent back to
        // background or phone has flip closed.
        if (window.windowState == window.STATE_MINIMIZED) {
          this.contentBrowser.docShellIsActive = false;
        } else {
          this.contentBrowser.docShellIsActive = true;
        }
        break;
      case "MozAfterPaint":
        window.removeEventListener("MozAfterPaint", this);
        break;
      case "unload":
        this.stop();
        break;
    }
  },

  // This gets called when window.onload fires on the System app content window,
  // which means things in <html> are parsed and statically referenced <script>s
  // and <script defer>s are loaded and run.
  notifyContentWindowLoaded() {
    debug("notifyContentWindowLoaded");
    // This will cause Gonk Widget to remove boot animation from the screen
    // and reveals the page.
    Services.obs.notifyObservers(null, "browser-ui-startup-complete");
  },
};

function toggle_bool_pref(name) {
  let current = Services.prefs.getBoolPref(name);
  Services.prefs.setBoolPref(name, !current);
  debug(`${name} is now ${!current}`);
}

document.addEventListener(
  "DOMContentLoaded",
  () => {
    if (shell.hasStarted()) {
      // Should never happen!
      console.error("Shell has already started but didn't initialize!!!");
      return;
    }

    document.addEventListener(
      "keydown",
      event => {
        if (event.key == "AudioVolumeUp") {
          console.log("Toggling GPU profiler display");
          toggle_bool_pref("gfx.webrender.debug.profiler");
          toggle_bool_pref("gfx.webrender.debug.compact-profiler");
        }
      },
      true
    );

    // eslint-disable-next-line no-undef
    RemoteDebugger.init(window);

    Services.obs.addObserver(browserWindowImpl => {
      debug("New web embedder created.");
      window.browserDOMWindow = browserWindowImpl;

      // Notify the the shell is ready at the next event loop tick to
      // let the embedder user a chance to add event listeners.
      window.setTimeout(() => {
        Services.obs.notifyObservers(window, "shell-ready");
      }, 0);
    }, "web-embedder-created");

    // Initialize Marionette server
    Services.tm.idleDispatchToMainThread(() => {
      Services.obs.notifyObservers(null, "marionette-startup-requested");
    });

    // Start the SIDL <-> Gecko bridge.
    const { GeckoBridge } = ChromeUtils.import(
      "resource://gre/modules/GeckoBridge.jsm"
    );
    GeckoBridge.start();

    // Start the Settings <-> Preferences synchronizer.
    const { SettingsPrefsSync } = ChromeUtils.import(
      "resource://gre/modules/SettingsPrefsSync.jsm"
    );
    SettingsPrefsSync.start(window).then(() => {
      // TODO: check if there is a better time to run delayedInit()
      // for the overall OS startup, like when the homescreen is ready.
      window.setTimeout(() => {
        SettingsPrefsSync.delayedInit();
      }, 10000);
    });

    shell.start();
  },
  { once: true }
);

// Install the self signed certificate for locally served apps.
function setup_local_https() {
  const { LocalDomains } = ChromeUtils.import(
    "resource://gre/modules/LocalDomains.jsm"
  );

  LocalDomains.init();
  if (LocalDomains.scan()) {
    debug(`Updating local domains list to: ${LocalDomains.get()}`);
    LocalDomains.update();
  }
}

// We need to set this up early to be able to launch the homescreen.
setup_local_https();
