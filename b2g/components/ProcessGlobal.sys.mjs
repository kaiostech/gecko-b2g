/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This code exists to be a "grab bag" of global code that needs to be
 * loaded per B2G process, but doesn't need to directly interact with
 * web content.
 *
 * (It's written as an XPCOM service because it needs to watch
 * app-startup.)
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "settings",
  "@mozilla.org/settingsService;1",
  "nsISettingsService"
);

function debug(msg) {
  log(msg);
}
function log(msg) {
  // This file implements console.log(), so use dump().
  //dump("ProcessGlobal: " + msg + "\n");
}

function formatStackFrame(aFrame) {
  let functionName = aFrame.functionName || "<anonymous>";
  return (
    "    at " +
    functionName +
    " (" +
    aFrame.filename +
    ":" +
    aFrame.lineNumber +
    ":" +
    aFrame.columnNumber +
    ")"
  );
}

function ConsoleMessage(aMsg, aLevel) {
  this.timeStamp = Date.now();
  this.msg = aMsg;

  switch (aLevel) {
    case "error":
    case "assert":
      this.logLevel = Ci.nsIConsoleMessage.error;
      break;
    case "warn":
      this.logLevel = Ci.nsIConsoleMessage.warn;
      break;
    case "log":
    case "info":
      this.logLevel = Ci.nsIConsoleMessage.info;
      break;
    default:
      this.logLevel = Ci.nsIConsoleMessage.debug;
      break;
  }
}

function toggleUnrestrictedDevtools(unrestricted) {
  Services.prefs.setBoolPref(
    "devtools.debugger.forbid-certified-apps",
    !unrestricted
  );
  Services.prefs.setBoolPref("dom.apps.developer_mode", unrestricted);
  // TODO: Remove once bug 1125916 is fixed.
  Services.prefs.setBoolPref("network.disable.ipc.security", unrestricted);
  Services.prefs.setBoolPref("dom.webcomponents.enabled", unrestricted);
  let lock = lazy.settings.createLock();
  lock.set("developer.menu.enabled", unrestricted, null);
  lock.set("devtools.unrestricted", unrestricted, null);
}

ConsoleMessage.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleMessage]),
  toString() {
    return this.msg;
  },
};

const gFactoryResetFile = "__post_reset_cmd__";

export function ProcessGlobal() {}
ProcessGlobal.prototype = {
  classID: Components.ID("{1a94c87a-5ece-4d11-91e1-d29c29f21b28}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  wipeDir(path) {
    log("wipeDir " + path);
    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(path);
    if (!dir.exists() || !dir.isDirectory()) {
      return;
    }
    let entries = dir.directoryEntries;
    while (entries.hasMoreElements()) {
      let file = entries.getNext().QueryInterface(Ci.nsIFile);
      log("Deleting " + file.path);
      try {
        file.remove(true);
      } catch (e) {}
    }
  },

  processCommandsFile(text) {
    log("processCommandsFile " + text);
    let lines = text.split("\n");
    lines.forEach(line => {
      log(line);
      let params = line.split(" ");
      switch (params[0]) {
        case "root":
          log("unrestrict devtools");
          toggleUnrestrictedDevtools(true);
          break;
        case "wipe":
          this.wipeDir(params[1]);
        // fall through
        case "normal":
          log("restrict devtools");
          toggleUnrestrictedDevtools(false);
          break;
      }
    });
  },

  cleanupAfterFactoryReset() {
    log("cleanupAfterWipe start");

    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath("/persist");
    var postResetFile = dir.exists()
      ? PathUtils.join("/persist", gFactoryResetFile)
      : PathUtils.join("/cache", gFactoryResetFile);
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(postResetFile);
    if (!file.exists()) {
      debug("No additional command.");
      return;
    }

    let promise = IOUtils.read(postResetFile);
    promise.then(
      array => {
        file.remove(false);
        let decoder = new TextDecoder();
        this.processCommandsFile(decoder.decode(array));
      },
      function onError(error) {
        debug("Error: " + error);
      }
    );

    log("cleanupAfterWipe end.");
  },

  init(inParent) {
    Services.obs.addObserver(this, "console-api-log-event");
    if (inParent) {
      this._initActor();

      Services.ppmm.addMessageListener("getProfD", () => {
        return Services.dirsvc.get("ProfD", Ci.nsIFile).path;
      });

      this.cleanupAfterFactoryReset();
    }
  },

  observe: function pg_observe(subject, topic, data) {
    let inParent =
      Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
    switch (topic) {
      case "app-startup":
        if (!inParent) {
          return;
        }
        this.init(inParent);
        break;
      case "content-process-ready-for-script":
        this.init(inParent);
        break;
      case "console-api-log-event": {
        // Pipe `console` log messages to the nsIConsoleService which
        // writes them to logcat on Gonk.
        let message = subject.wrappedJSObject;
        let args = message.arguments;
        let stackTrace = "";

        if (
          message.stacktrace &&
          (message.level == "assert" ||
            message.level == "error" ||
            message.level == "trace")
        ) {
          stackTrace = Array.prototype.map
            .call(message.stacktrace, formatStackFrame)
            .join("\n");
        } else {
          stackTrace = formatStackFrame(message);
        }

        if (stackTrace) {
          args.push("\n" + stackTrace);
        }

        let msg =
          "Content JS " +
          message.level.toUpperCase() +
          ": " +
          Array.prototype.join(args, " ");
        Services.console.logMessage(new ConsoleMessage(msg, message.level));
        break;
      }
    }
  },

  _initActor() {
    // Initialize the ActorManagerParent
    const { ActorManagerParent } = ChromeUtils.importESModule(
      "resource://gre/modules/ActorManagerParent.sys.mjs"
    );

    let JSWINDOWACTORS = {
      AudioVolumeControlOverride: {
        parent: {
          moduleURI:
            "resource://gre/actors/AudioVolumeControlOverrideParent.jsm",
        },
        child: {
          moduleURI:
            "resource://gre/actors/AudioVolumeControlOverrideChild.jsm",
          events: {
            fullscreenchange: {},
          },
        },
        allFrames: true,
      },

      SelectionAction: {
        parent: {
          moduleURI: "resource://gre/actors/SelectionActionParent.jsm",
        },
        child: {
          moduleURI: "resource://gre/actors/SelectionActionChild.jsm",
          events: {
            mozcaretstatechanged: { mozSystemGroup: true },
          },
        },
        allFrames: true,
      },

      VoiceInputStyle: {
        child: {
          moduleURI: "resource://gre/actors/VoiceInputStyleChild.jsm",
          events: {
            IMEFocus: {},
            IMEBlur: {},
          },
        },
        allFrames: true,
        includeChrome: true,
      },

      WebViewExporter: {
        child: {
          moduleURI: "resource://gre/actors/WebViewExporterChild.jsm",
          events: {
            DOMContentLoaded: { capture: true },
          },
        },
        allFrames: true,
      },

      WebViewForContent: {
        child: {
          moduleURI: "resource://gre/actors/WebViewForContentChild.jsm",
          events: {
            DOMTitleChanged: { mozSystemGroup: true },
            DOMLinkAdded: { capture: true },
            DOMMetaAdded: { capture: true },
            DOMMetaChanged: { capture: true },
            DOMMetaRemoved: { capture: true },
            scroll: { capture: true },
            MozScrolledAreaChanged: { capture: true },
            "webview-getbackgroundcolor": { capture: true },
            contextmenu: { mozSystemGroup: true },
            "webview-fire-ctx-callback": { capture: true },
            "webview-getscreenshot": { capture: true },
          },
        },
        allFrames: true,
      },

      Prompt: {
        parent: {
          moduleURI: "resource://gre/actors/PromptParent.jsm",
        },
        includeChrome: true,
        allFrames: true,
      },
    };

    ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);
  },
};
