/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// A base class to use by Web Embedders, providing an ergonomic
// api over Gecko specific various hooks.
// It runs with chrome privileges in the system app.

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ChromeNotifications: "resource://gre/modules/ChromeNotifications.jsm",
  SelectionActionParent: "resource://gre/actors/SelectionActionParent.jsm",
  embeddableProcessInfo: "resource://gre/modules/B2GProcessSelector.jsm",
  WebExtensionsEmbedding: "resource://gre/modules/WebExtensionsEmbedding.jsm",
});

XPCOMUtils.defineLazyGetter(this, "Screenshot", function() {
  const { Screenshot } = ChromeUtils.import(
    "resource://gre/modules/Screenshot.jsm"
  );
  return Screenshot;
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdleService",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);

(function() {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  const { AlertsEventHandler } = ChromeUtils.import(
    "resource://gre/modules/AlertsHelper.jsm"
  );

  const systemAlerts = {
    resendAll: resendCallback => {
      ChromeNotifications.resendAllNotifications(resendCallback);
    },
    click: data => {
      AlertsEventHandler.click(data);
    },
    close: id => {
      AlertsEventHandler.close(id);
    },
  };

  const customAccessible = {
    send: domNode => {
      Services.obs.notifyObservers(domNode, "custom-accessible");
    },
    startOutput: () => {
      Services.obs.notifyObservers({}, "start-custom-access-output");
    },
    stopOutput: () => {
      Services.obs.notifyObservers({}, "stop-custom-access-output");
    },
  };

  const userIdleObserverMap = new Map();
  const userIdle = {
    addObserver: (observer, idleTime) => {
      let observerXpcom = (subject, topic, idleTime) => {
        observer(topic, idleTime);
      };
      _webembed_log(`userIdle addObserver time: ${idleTime}`);
      IdleService.addIdleObserver(observerXpcom, idleTime);
      userIdleObserverMap.set([observer, idleTime], observerXpcom);
    },
    removeObserver: (observer, idleTime) => {
      let observerXpcom, removeKey;
      for (let [key, value] of userIdleObserverMap) {
        if (key[0] === observer && key[1] === idleTime) {
          removeKey = key;
          observerXpcom = value;
          break;
        }
      }
      if (observerXpcom) {
        _webembed_log(`userIdle removeObserver time: ${idleTime}`);
        IdleService.removeIdleObserver(observerXpcom, idleTime);
        userIdleObserverMap.delete(removeKey);
      } else {
        _webembed_log(`removeObserver failed`);
      }
    },
  };

  function EditableSupport(native) {
    this.native = native;
  }

  EditableSupport.prototype = {
    setSelectedOption(index) {
      if (this.native) {
        this.native.SetSelectedOption(0, null, index);
      }
    },

    setSelectedOptions(indexes) {
      if (this.native) {
        this.native.SetSelectedOptions(0, null, indexes);
      }
    },

    removeFocus() {
      if (this.native) {
        this.native.RemoveFocus(0, null);
      }
    },
  };

  // Enable logs when according to the pref value, and listen to changes.
  let webEmbedLogEnabled = Services.prefs.getBoolPref(
    "webembed.log.enabled",
    false
  );

  function updateLogStatus() {
    webEmbedLogEnabled = Services.prefs.getBoolPref(
      "webembed.log.enabled",
      false
    );
  }

  Services.prefs.addObserver("webembed.log.enabled", updateLogStatus);
  window.document.addEventListener(
    "unload",
    () => {
      Services.prefs.removeObserver("webembed.log.enabled", updateLogStatus);
    },
    { once: true }
  );

  function _webembed_log(msg) {
    webEmbedLogEnabled && console.log(`WebEmbedder: ${msg}`);
  }

  function _webembed_error(msg) {
    console.error(`WebEmbedder: ${msg}`);
  }

  function BrowserDOMWindow(embedder) {
    _webembed_log(
      `Creating BrowserDOMWindow implementing ${Ci.nsIBrowserDOMWindow}`
    );
    this.embedder = embedder;
  }

  BrowserDOMWindow.prototype = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIBrowserDOMWindow]),

    get tabCount() {
      _webembed_log(`BrowserDOMWindow::tabCount`);

      return window.document.querySelectorAll("web-view").length;
    },

    // Returns a BrowsingContext
    openURI(aURI, aOpenWindowInfo, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
      _webembed_log(
        `BrowserDOMWindow::openURI ${aURI ? aURI.spec : "<no uri>"}`
      );
      if (this.embedder && this.embedder.browserDomWindow) {
        let res = this.embedder.browserDomWindow.openURI(
          aURI ? aURI.spec : null,
          aOpenWindowInfo,
          aWhere,
          aFlags,
          aTriggeringPrincipal,
          aCsp
        );
        if (res) {
          return res.frame.browsingContext;
        }
      }
      _webembed_error("openURI NOT IMPLEMENTED");
      throw new Error("NOT IMPLEMENTED");
    },

    // Returns a BrowsingContext
    createContentWindow(
      aURI,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp
    ) {
      _webembed_log(
        `BrowserDOMWindow::createContentWindow ${aURI ? aURI.spec : "<no uri>"}`
      );
      if (this.embedder && this.embedder.browserDomWindow) {
        let res = this.embedder.browserDomWindow.createContentWindow(
          aURI ? aURI.spec : null,
          aOpenWindowInfo,
          aWhere,
          aFlags,
          aTriggeringPrincipal,
          aCsp
        );
        if (res) {
          return res.frame.browsingContext;
        }
      }
      _webembed_error("createContentWindow NOT IMPLEMENTED");
      throw new Error("NOT IMPLEMENTED");
    },

    // Returns an Element
    openURIInFrame(aURI, aParams, aWhere, aFlags, aName) {
      _webembed_log(
        `BrowserDOMWindow::openURIInFrame ${aURI ? aURI.spec : "<no uri>"}`
      );
      if (this.embedder && this.embedder.browserDomWindow) {
        let res = this.embedder.browserDomWindow.openURIInFrame(
          aURI ? aURI.spec : null,
          aParams,
          aWhere,
          aFlags,
          aName
        );
        if (res) {
          return res.frame;
        }
      }
      _webembed_error("openURIInFrame NOT IMPLEMENTED");
      throw new Error("NOT IMPLEMENTED");
    },

    // Returns an Element
    createContentWindowInFrame(aURI, aParams, aWhere, aFlags, aName) {
      _webembed_log(
        `BrowserDOMWindow::createContentWindowInFrame ${
          aURI ? aURI.spec : "<no uri>"
        }`
      );
      if (this.embedder && this.embedder.browserDomWindow) {
        let res = this.embedder.browserDomWindow.createContentWindowInFrame(
          aURI ? aURI.spec : null,
          aParams,
          aWhere,
          aFlags,
          aName
        );
        if (res) {
          return res.frame;
        }
      }
      _webembed_error("createContentWindowInFrame NOT IMPLEMENTED");
      throw new Error("NOT IMPLEMENTED");
    },

    canClose() {
      _webembed_log(`BrowserDOMWindow::canClose`);
      if (this.embedder && this.embedder.browserDomWindow) {
        return this.embedder.browserDomWindow.canClose();
      }
      return true;
    },
  };

  class WebEmbedder extends EventTarget {
    constructor(delegates) {
      super();

      _webembed_log(`constructor in ${window}`);

      let self = this;

      let overscrollObserver = {
        observe: (_subject, topic, _data) => {
          _webembed_log(`Overscroll: ${topic}`);
          self.dispatchEvent(new CustomEvent(topic));
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
      };

      Services.obs.addObserver(overscrollObserver, "overscroll-start");
      Services.obs.addObserver(overscrollObserver, "overscroll-end");

      this.daemonManager = Cc["@mozilla.org/sidl-native/manager;1"].getService(
        Ci.nsIDaemonManager
      );
      if (this.daemonManager) {
        this.daemonManager.setObserver({
          // nsISidlConnectionObserver
          disconnected() {
            self.dispatchEvent(new CustomEvent("daemon-disconnected"));
            Services.obs.notifyObservers(null, "api-daemon-disconnected");
          },
          reconnected() {
            self.dispatchEvent(new CustomEvent("daemon-reconnected"));
            Services.obs.notifyObservers(null, "api-daemon-reconnected");
          },
        });
      } else {
        _webembed_error("Unable to get nsIDaemonManager service.");
      }

      this.browserDomWindow = delegates.windowProvider;

      this.systemAlerts = systemAlerts;
      this.customAccessible = customAccessible;
      this.userIdle = userIdle;

      Services.obs.addObserver(shell_window => {
        this.shellWindow = shell_window;
        this.eventDocument = shell_window.document;
        this.dispatchEvent(new CustomEvent("runtime-ready"));
      }, "shell-ready");

      // Hook up the process provider implementation.
      // First make sure the service was started so it can receive the observer notification.
      let pp_service = Cc["@mozilla.org/ipc/processselector;1"].getService(
        Ci.nsIContentProcessProvider
      );
      if (!pp_service) {
        _webembed_error("No ContentProcessProvider service available!");
        return;
      }

      Services.obs.notifyObservers(
        { wrappedJSObject: delegates.processSelector },
        "web-embedder-set-process-selector"
      );

      // Notify the shell that a new embedder was created and send it the window provider.
      Services.obs.notifyObservers(
        new BrowserDOMWindow(this),
        "web-embedder-created"
      );

      Services.obs.addObserver(wrappedDetail => {
        _webembed_log("receive activity-choice");
        let detail = wrappedDetail.wrappedJSObject;
        delegates.activityChooser.choseActivity(detail).then(
          choice => {
            Services.obs.notifyObservers(
              { wrappedJSObject: choice },
              "activity-choice-result"
            );
          },
          error => {
            _webembed_log(`Error in choseActivity: ${error}`);
          }
        );
      }, "activity-choice");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log("receive activity-aborted");
        this.dispatchEvent(
          new CustomEvent(topic, {
            detail: subject.wrappedJSObject,
          })
        );
      }, "activity-aborted");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log("receive activity-opened");
        this.dispatchEvent(
          new CustomEvent(topic, {
            detail: subject.wrappedJSObject,
          })
        );
      }, "activity-opened");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log("receive activity-closed");
        this.dispatchEvent(
          new CustomEvent(topic, {
            detail: subject.wrappedJSObject,
          })
        );
      }, "activity-closed");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive inputmethod-contextchange: ${data}`);
        if (data == null) {
          return;
        }
        let isFocus = data === "focus";
        let detail = {
          isFocus,
        };
        if (subject instanceof Ci.nsIInputContext) {
          detail = {
            ...detail,
            type: subject.type,
            inputType: subject.inputType,
            inputMode: subject.inputMode,
            value: subject.value,
            selectionStart: subject.selectionStart,
            selectionEnd: subject.selectionEnd,
            max: subject.max,
            min: subject.min,
            lang: subject.lang,
            voiceInputSupported: subject.voiceInputSupported,
            name: subject.name,
            choices: subject.choices,
            maxLength: subject.maxLength,
            imeGroup: subject.imeGroup,
            lastImeGroup: subject.lastImeGroup,
            activeEditable: new EditableSupport(subject.editableSupport),
          };
        }
        _webembed_log(`detail: ${JSON.stringify(detail)}`);
        if (delegates.imeHandler) {
          delegates.imeHandler.focusChanged(detail);
        }
      }, "inputmethod-contextchange");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive headphones-status-changed: ${data}`);
        this.dispatchEvent(
          new CustomEvent("headphones-status-changed", { detail: data })
        );
      }, "headphones-status-changed");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive default-volume-channel-changed: ${data}`);
        this.dispatchEvent(
          new CustomEvent("default-volume-channel-changed", { detail: data })
        );
      }, "default-volume-channel-changed");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive bluetooth-volume-change: ${data}`);
        this.dispatchEvent(
          new CustomEvent("bluetooth-volumeset", { detail: data })
        );
      }, "bluetooth-volume-change");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive mtp-state-changed: ${data}`);
        this.dispatchEvent(
          new CustomEvent("mtp-state-changed", { detail: data })
        );
      }, "mtp-state-changed");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive volume-state-changed: ${data}`);
        this.dispatchEvent(
          new CustomEvent("volume-state-changed", { detail: data })
        );
      }, "volume-state-changed");

      Services.obs.addObserver((detail, subject, data) => {
        _webembed_log(`receive almost-low-disk-space: ${data}`);
        this.dispatchEvent(
          new CustomEvent("almost-low-disk-space", { detail: data === "true" })
        );
      }, "almost-low-disk-space");

      // whether geolocation service is active
      this.geolocationActive = false;

      // whether gps chip is on
      this.gpsChipOn = false;

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive geolocation-device-events: ${data}`);
        let oldState = this.geolocationActive;
        switch (data) {
          case "GPSStarting":
            if (!this.geolocationActive) {
              this.geolocationActive = true;
              this.gpsChipOn = true;
            }
            break;
          case "GPSShutdown":
            if (this.gpsChipOn) {
              this.geolocationActive = false;
              this.gpsChipOn = false;
            }
            break;
          case "starting":
            this.geolocationActive = true;
            this.gpsChipOn = false;
            break;
          case "shutdown":
            this.geolocationActive = false;
            break;
        }
        if (this.geolocationActive != oldState) {
          this.dispatchEvent(
            new CustomEvent("geolocation-status", {
              detail: { active: this.geolocationActive },
            })
          );
        }
      }, "geolocation-device-events");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive captive-portal-login: ${data}`);
        this.dispatchEvent(
          new CustomEvent("captive-portal-login-request", {
            detail: JSON.parse(data),
          })
        );
      }, "captive-portal-login");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive captive-portal-login-abort: ${data}`);
        let obj = JSON.parse(data);
        this.dispatchEvent(
          new CustomEvent("captive-portal-login-result", {
            detail: { result: false, id: obj.id },
          })
        );
      }, "captive-portal-login-abort");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive captive-portal-login-success: ${data}`);
        let obj = JSON.parse(data);
        this.dispatchEvent(
          new CustomEvent("captive-portal-login-result", {
            detail: { result: true, id: obj.id },
          })
        );
      }, "captive-portal-login-success");

      if (delegates.notifications) {
        Services.obs.notifyObservers(
          { wrappedJSObject: delegates.notifications },
          "web-embedder-notifications"
        );
      }

      if (delegates.screenReaderProvider) {
        Services.obs.addObserver(() => {
          _webembed_log(`Get the AccessFu:Enabled`);
          Services.obs.notifyObservers(
            { wrappedJSObject: delegates.screenReaderProvider },
            "web-embedder-set-screenReader"
          );
        }, "AccessFu:Enabled");
      }

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(
          `Get the caret-state-changed ${JSON.stringify(
            subject.wrappedJSObject
          )}`
        );
        this.dispatchEvent(
          new CustomEvent(topic, {
            detail: subject.wrappedJSObject,
          })
        );
      }, "caret-state-changed");

      Services.obs.addObserver((subject, topic, data) => {
        _webembed_log(`receive b2g-sw-registration-done`);
        this.dispatchEvent(new CustomEvent("sw-registration-done"));
      }, "b2g-sw-registration-done");

      if (delegates.webExtensions) {
        WebExtensionsEmbedding.setDelegate(delegates.webExtensions);
      }
    }

    isDaemonReady() {
      return this.daemonManager ? this.daemonManager.isReady() : false;
    }

    launchPreallocatedProcess() {
      _webembed_log(`launchPreallocatedProcess`);
      return Services.appinfo.ensureContentProcess();
    }

    isGonk() {
      const { AppConstants } = ChromeUtils.import(
        "resource://gre/modules/AppConstants.jsm"
      );
      return AppConstants.platform === "gonk";
    }

    takeScreenshot() {
      _webembed_log(`takeScreenshot`);
      try {
        let file = Screenshot.get(this.shellWindow);
        _webembed_log(`takeScreenshot success and the file size: ${file.size}`);
        return file;
      } catch (e) {
        _webembed_log(`takeScreenshot error: ${e}`);
        return null;
      }
    }

    doSelectionAction(action) {
      const command = "cmd_" + action;
      _webembed_log(`doSelectionAction: ${command}`);
      SelectionActionParent.sendCommand(command);
    }

    // Proxies a subset of nsIEventListenerService, useful eg. to listen
    // to hardware keys in the system app directly.
    // We need to set the target to the shell.js document to avoid focus issues.
    addSystemEventListener(type, listener, useCapture) {
      Services.els.addSystemEventListener(
        this.eventDocument,
        type,
        listener,
        useCapture
      );
    }

    removeSystemEventListener(type, listener, useCapture) {
      Services.els.removeSystemEventListener(
        this.eventDocument,
        type,
        listener,
        useCapture
      );
    }

    launchRemoteWindows() {
      Services.obs.notifyObservers(null, "open-remote-shell-windows");
    }

    getContentProcesses() {
      return Services.appinfo.getContentProcesses().map(embeddableProcessInfo);
    }
  }

  window.WebEmbedder = WebEmbedder;
})();
