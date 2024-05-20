/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable quotes */
"use strict";

// A <web-view> custom element, wrapping a <xul:browser>

(function () {
  const { XPCOMUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/XPCOMUtils.sys.mjs"
  );

  const lazy = {};

  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "KeyboardAppProxy",
    "@mozilla.org/keyboardAppProxy;1",
    "nsIKeyboardAppProxy"
  );

  XPCOMUtils.defineLazyGetter(lazy, "DataControlService", function () {
    try {
      return Cc["@kaiostech.com/datacontrolservice;1"].getService(
        Ci.nsIDataControlService
      );
    } catch (e) {
      return {
        updateVisible(pid, val) {},
      };
    }
  });

  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "virtualcursor",
    "@mozilla.org/virtualcursor/service;1",
    "nsIVirtualCursorService"
  );

  // Enable logs when according to the pref value, and listen to changes.
  let webViewLogEnabled = Services.prefs.getBoolPref(
    "webview.log.enabled",
    false
  );

  function defaultUserAgent() {
    if (window?.navigator?.userAgent) {
      return window.navigator.userAgent;
    }
    return Cc["@mozilla.org/network/protocol;1?name=http"].getService(
      Ci.nsIHttpProtocolHandler
    ).userAgent;
  }

  function updateLogStatus() {
    webViewLogEnabled = Services.prefs.getBoolPref(
      "webview.log.enabled",
      false
    );
  }

  Services.prefs.addObserver("webview.log.enabled", updateLogStatus);
  window.document.addEventListener(
    "unload",
    () => {
      Services.prefs.removeObserver("webview.log.enabled", updateLogStatus);
    },
    { once: true }
  );

  // The progress listener attached to the webview, managing some events.
  function ProgressListener(webview) {
    this.webview = webview;
  }

  ProgressListener.prototype = {
    log(msg) {
      webViewLogEnabled && console.log(`<web-view-listener> ${msg}`);
    },

    error(msg) {
      console.error(`<web-view-listener> ${msg}`);
    },

    dispatchEvent(name, detail) {
      this.log(`dispatching ${name}`);
      let event = new CustomEvent(`${name}`, {
        bubbles: true,
        detail,
      });
      this.webview.dispatchEvent(event);
    },

    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIWebProgressListener,
      Ci.nsISupportsWeakReference,
    ]),
    seenLoadStart: false,
    backgroundcolor: "transparent",

    set_background_color(color) {
      this.backgroundcolor = color;
    },

    onLocationChange(webProgress, request, location, flags) {
      // Only act on top-level changes.
      if (!webProgress.isTopLevel) {
        return;
      }

      this.log(`onLocationChange ${location.spec}`);

      // Ignore locationchange events which occur before the first loadstart.
      // These are usually about:blank loads we don't care about.
      if (!this.seenLoadStart) {
        this.log(`loadstart not seen yet, not dispatching locationchange`);
        return;
      }

      // Remove password from uri.
      location = Services.io.createExposableURI(location);

      this.dispatchEvent(`locationchange`, {
        url: location.spec,
        canGoBack: this.webview.canGoBack,
        canGoForward: this.webview.canGoForward,
      });
    },

    // eslint-disable-next-line complexity
    onStateChange(webProgress, request, stateFlags, status) {
      // Only act on top-level changes.
      if (!webProgress.isTopLevel) {
        return;
      }

      const CERTIFICATE_ERROR_PAGE_PREF =
        "security.alternate_certificate_error_page";
      // this.log(`onStateChange ${stateFlags}`);

      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this.dispatchEvent("loadstart");
        this.seenLoadStart = true;
      }

      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this.dispatchEvent("loadend", {
          backgroundColor: this.backgroundcolor,
        });

        switch (status) {
          case Cr.NS_OK:
          case Cr.NS_BINDING_ABORTED:
          // Ignoring NS_BINDING_ABORTED, which is set when loading page is
          // stopped.
          // falls through
          case Cr.NS_ERROR_PARSED_DATA_CACHED:
            return;

          // TODO See nsDocShell::DisplayLoadError to see what extra
          // information we should be annotating this first block of errors
          // with. Bug 1107091.
          case Cr.NS_ERROR_UNKNOWN_PROTOCOL:
            this.dispatchEvent("error", { type: "unknownProtocolFound" });
            return;
          case Cr.NS_ERROR_FILE_NOT_FOUND:
            this.dispatchEvent("error", { type: "fileNotFound" });
            return;
          case Cr.NS_ERROR_UNKNOWN_HOST:
            this.dispatchEvent("error", { type: "dnsNotFound" });
            return;
          case Cr.NS_ERROR_CONNECTION_REFUSED:
            this.dispatchEvent("error", { type: "connectionFailure" });
            return;
          case Cr.NS_ERROR_NET_INTERRUPT:
            this.dispatchEvent("error", { type: "netInterrupt" });
            return;
          case Cr.NS_ERROR_NET_TIMEOUT:
            this.dispatchEvent("error", { type: "netTimeout" });
            return;
          case Cr.NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION:
            this.dispatchEvent("error", { type: "cspBlocked" });
            return;
          case Cr.NS_ERROR_PHISHING_URI:
            this.dispatchEvent("error", { type: "deceptiveBlocked" });
            return;
          case Cr.NS_ERROR_MALWARE_URI:
            this.dispatchEvent("error", { type: "malwareBlocked" });
            return;
          case Cr.NS_ERROR_HARMFUL_URI:
            this.dispatchEvent("error", { type: "harmfulBlocked" });
            return;
          case Cr.NS_ERROR_UNWANTED_URI:
            this.dispatchEvent("error", { type: "unwantedBlocked" });
            return;
          case Cr.NS_ERROR_FORBIDDEN_URI:
            this.dispatchEvent("error", { type: "forbiddenBlocked" });
            return;
          case Cr.NS_ERROR_OFFLINE:
            this.dispatchEvent("error", { type: "offline" });
            return;
          case Cr.NS_ERROR_MALFORMED_URI:
            this.dispatchEvent("error", { type: "malformedURI" });
            return;
          case Cr.NS_ERROR_REDIRECT_LOOP:
            this.dispatchEvent("error", { type: "redirectLoop" });
            return;
          case Cr.NS_ERROR_UNKNOWN_SOCKET_TYPE:
            this.dispatchEvent("error", { type: "unknownSocketType" });
            return;
          case Cr.NS_ERROR_NET_RESET:
            this.dispatchEvent("error", { type: "netReset" });
            return;
          case Cr.NS_ERROR_DOCUMENT_NOT_CACHED:
            this.dispatchEvent("error", { type: "notCached" });
            return;
          case Cr.NS_ERROR_DOCUMENT_IS_PRINTMODE:
            this.dispatchEvent("error", { type: "isprinting" });
            return;
          case Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED:
            this.dispatchEvent("error", { type: "deniedPortAccess" });
            return;
          case Cr.NS_ERROR_UNKNOWN_PROXY_HOST:
            this.dispatchEvent("error", { type: "proxyResolveFailure" });
            return;
          case Cr.NS_ERROR_PROXY_CONNECTION_REFUSED:
            this.dispatchEvent("error", { type: "proxyConnectFailure" });
            return;
          case Cr.NS_ERROR_INVALID_CONTENT_ENCODING:
            this.dispatchEvent("error", { type: "contentEncodingFailure" });
            return;
          case Cr.NS_ERROR_REMOTE_XUL:
            this.dispatchEvent("error", { type: "remoteXUL" });
            return;
          case Cr.NS_ERROR_UNSAFE_CONTENT_TYPE:
            this.dispatchEvent("error", { type: "unsafeContentType" });
            return;
          case Cr.NS_ERROR_CORRUPTED_CONTENT:
            this.dispatchEvent("error", { type: "corruptedContentErrorv2" });
            return;
          case Cr.NS_ERROR_BLOCKED_BY_POLICY:
            this.dispatchEvent("error", { type: "blockedByPolicy" });
            return;

          default:
            // getErrorClass() will throw if the error code passed in is not a NSS
            // error code.
            try {
              let nssErrorsService = Cc[
                "@mozilla.org/nss_errors_service;1"
              ].getService(Ci.nsINSSErrorsService);
              if (
                nssErrorsService.getErrorClass(status) ==
                Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT
              ) {
                // XXX Is there a point firing the event if the error page is not
                // certerror? If yes, maybe we should add a property to the
                // event to to indicate whether there is a custom page. That would
                // let the embedder have more control over the desired behavior.
                let errorPage = Services.prefs.getCharPref(
                  CERTIFICATE_ERROR_PAGE_PREF,
                  ""
                );

                if (errorPage == "certerror") {
                  this.dispatchEvent("error", { type: "certerror" });
                  return;
                }
              }
            } catch (e) {}

            this.dispatchEvent("error", { type: "other" });
        }
      }
    },

    onSecurityChange(webProgress, request, state) {
      let securityStateDesc;
      if (state & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
        securityStateDesc = "secure";
      } else if (state & Ci.nsIWebProgressListener.STATE_IS_BROKEN) {
        securityStateDesc = "broken";
      } else if (state & Ci.nsIWebProgressListener.STATE_IS_INSECURE) {
        securityStateDesc = "insecure";
      } else {
        this.error(`Unexpected securitychange state: ${state}`);
        securityStateDesc = "???";
      }

      let mixedState;
      if (
        state & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT
      ) {
        mixedState = "blocked_mixed_active_content";
      } else if (
        state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT
      ) {
        // Note that STATE_LOADED_MIXED_ACTIVE_CONTENT implies STATE_IS_BROKEN
        mixedState = "loaded_mixed_active_content";
      }

      let extendedValidation = !!(
        state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL
      );
      var mixedContent = !!(
        state &
        (Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT |
          Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT)
      );

      this.dispatchEvent("securitychange", {
        state: securityStateDesc,
        mixedState,
        extendedValidation,
        mixedContent,
      });
    },

    onStatusChange(webProgress, request, status, message) {},
    onProgressChange(
      webProgress,
      request,
      curSelfProgress,
      maxSelfProgress,
      curTotalProgress,
      maxTotalProgress
    ) {},
  };

  const kRegisteredBrowserEvents = [
    "backgroundcolor",
    "close",
    "documentfirstpaint",
    "contextmenu",
    "iconchange",
    "manifestchange",
    "metachange",
    "opensearch",
    "pagetitlechanged",
    "processready",
    "promptpermission",
    "recordingstatus",
    "visibilitychange",
    "resize",
    "scroll",
    "scrollareachanged",
    "showmodalprompt",
  ];

  class WebView extends HTMLElement {
    constructor() {
      super();
      this._constructorInternal();
    }

    // We also export functions for content world.
    // If isCreatedByProxy is set, it mean that 'this' comes from
    // what is the LocalClass in b2g/compoments/actos/web-view.js.
    _constructorInternal(isCreatedByProxy = false) {
      this.isCreatedByProxy = isCreatedByProxy;
      if (this.isCreatedByProxy) {
        // Apply the pollyfill here for 'this' as a proxy object.
        WebView.__doPollyfill(this);
      }

      this.log("constructor");

      this.browser = null;
      this.attrs = [];

      this._pid = -1;
    }

    log(msg) {
      webViewLogEnabled && console.log(`<web-view> ${msg}`);
    }

    error(msg) {
      console.error(`<web-view> ${msg}`);
    }

    static get observedAttributes() {
      const attrs = [
        "src",
        "remote",
        "ignoreuserfocus",
        "transparent",
        "mozpasspointerevents",
      ];

      // If we are in chrome, it does no effect.
      // If we are in content, it will copy data between scopes.
      return Cu.cloneInto(attrs, this);
    }

    attributeChangedCallback(name, old_value, new_value) {
      if (old_value === new_value) {
        return;
      }

      this.log(`attribute ${name} changed from ${old_value} to ${new_value}`);
      // If we have not created the browser yet, buffer the attribute modification.
      if (!this.browser) {
        this.attrs.push({ name, old_value, new_value });
      } else {
        this.update_attr(name, old_value, new_value);
      }
    }

    update_attr(name, old_value, new_value) {
      if (!this.browser) {
        this.error(`update_attr(${name}) called with no browser available.`);
        return;
      }

      if (new_value) {
        this.browser.setAttribute(name, new_value);
      } else {
        this.browser.removeAttribute(name);
      }
    }

    connectedCallback() {
      this.log(`connectedCallback`);
      if (this.browser) {
        return;
      }
      this.log(`creating xul:browser`);

      // Creates a xul:browser with default attributes or reuse an existed one.
      if (this._browser) {
        this.browser = this._browser;
      } else {
        this.browser = document.createXULElement("browser");
      }
      // For chrome, the Browser API is defined and extended the <browser> by
      // customElements.define("browser", MozBrowser) inbrowser-custom-element.js.
      // For content, it does not allow us to extend an existed tag, so we add the API back by ourself.
      if (this.isCreatedByProxy) {
        WebView.__doPollyfillForBrowser(this.browser);
      }

      // Wait both for connectedCallback and openWindowInfo are available then
      // setup the browser.
      if (this._openWindowInfo !== undefined) {
        this.setupBrowser();
      }
    }

    setupBrowser() {
      // Identify this `<browser>` element uniquely to Marionette, devtools, etc.
      this.browser.permanentKey = new (Cu.getGlobalForObject(
        Services
      ).Object)();

      this.browser.setAttribute("src", "about:blank");
      this.browser.setAttribute("type", "content");

      // We can't set the xul:browser style as an attribute because that is rejected by the CSP.
      this.browser.style.border = "none";
      this.browser.style.width = "100%";
      this.browser.style.height = "100%";

      let src = null;

      // Apply buffered attribute changes.
      this.attrs.forEach(attr => {
        if (attr.name == "src") {
          src = attr.new_value;
          return;
        }
        this.update_attr(attr.name, attr.old_value, attr.new_value);
      });
      this.attrs = [];

      // Hack around failing to add the progress listener before construct() runs.
      this.browser.delayConnectedCallback = () => {
        return false;
      };
      this.log(`setupBrowser remote=${this.browser.getAttribute("remote")}`);
      this.browser.openWindowInfo = this._openWindowInfo;

      kRegisteredBrowserEvents.forEach(name => {
        this.browser.addEventListener(name, this);
      });

      this.appendChild(this.browser);
      this.progressListener = new ProgressListener(this);
      this.browser.addProgressListener(this.progressListener);

      let useragent = this.browser.getAttribute("useragent");
      if (useragent) {
        this.browser.browsingContext.customUserAgent = useragent;
      }

      // TODO: figure out why we can't just set `observe()` as a class method.
      // Logic here is similar to the one used in GeckoView:
      // https://searchfox.org/mozilla-central/rev/82c04b9cad5b98bdf682bd477f2b1e3071b004ad/mobile/android/modules/geckoview/GeckoViewContent.jsm#202
      let self = this;
      this.crashObserver = {
        observe(subject, topic, _data) {
          this._contentCrashed = false;
          self._contentCrashedOrKilled = true;
          const browser = subject.ownerElement;

          switch (topic) {
            case "oop-frameloader-crashed": {
              if (!browser || browser != self.browser) {
                return;
              }
              setTimeout(() => {
                if (this._contentCrashed) {
                  self.dispatchCustomEvent("error", {
                    type: "fatal",
                    reason: "content-crash",
                  });
                } else {
                  self.dispatchCustomEvent("error", {
                    type: "fatal",
                    reason: "content-kill",
                  });
                }
              }, 250);
              self.updateDCSState(false);

              lazy.virtualcursor.removeCursor(browser.frameLoader);
              break;
            }
            case "ipc:content-shutdown": {
              subject.QueryInterface(Ci.nsIPropertyBag2);
              if (subject.get("dumpID")) {
                if (
                  browser &&
                  subject.get("childID") != browser.frameLoader.childID
                ) {
                  return;
                }
                this._contentCrashed = true;
              }
              break;
            }
          }
        },
      };

      // Observe content crashes notifications.
      Services.obs.addObserver(this.crashObserver, "oop-frameloader-crashed");
      Services.obs.addObserver(this.crashObserver, "ipc:content-shutdown");

      if (
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT
      ) {
        // http-on-modify-request only works in the parent process.
        this.httpModifyRequest = {
          observe(subject, topic, _data) {
            if (topic !== "http-on-modify-request") {
              // That should never happen.
              console.error(`Unexpected topic in <web-view>: ${topic}`);
              return;
            }
            let channel = subject.QueryInterface(Ci.nsIHttpChannel);
            let frameElement =
              channel.loadInfo.browsingContext?.topFrameElement;
            if (
              channel.isMainDocumentChannel &&
              frameElement &&
              self.browser == frameElement
            ) {
              self.dispatchCustomEvent("beforelocationchange", {
                uri: channel.URI.spec,
              });
              if (self.userAgent) {
                channel.setRequestHeader("User-Agent", self.userAgent, false);
              }
            }
          },
        };

        Services.obs.addObserver(
          this.httpModifyRequest,
          "http-on-modify-request"
        );
      }
      // Set the src to load once we have setup all listeners to not miss progress events
      // like loadstart.
      src && this.browser.setAttribute("src", src);
    }

    set openWindowInfo(val) {
      this.log(`set openWindowInfo`);
      this._openWindowInfo = val;
      // Wait both for connectedCallback and openWindowInfo are available then
      // setup the browser. Check permanentKey to avoid initializing twice.
      if (this.browser && this.browser.permanentKey === undefined) {
        this.setupBrowser();
      }
    }

    get openWindowInfo() {
      return this._openWindowInfo;
    }

    set browserElement(val) {
      if (this.browser) {
        this.log(`too late to set browser element`);
      } else {
        this._browser = val;
      }
    }

    disconnectedCallback() {
      if (!this._cleanedUp) {
        this.cleanup();
      }
    }

    cleanup() {
      kRegisteredBrowserEvents.forEach(name => {
        if (this.browser) {
          this.browser.removeEventListener(name, this);
        }
      });

      this.updateDCSState(false);
      if (!this._contentCrashedOrKilled) {
        // This line causes nsFrameLoader to create a new content
        // process, or use a preallocated one, if the old content
        // process is crashed or killed.  So, skip this line for the
        // case of crashing.
        if (this.browser) {
          this.browser.removeProgressListener(this.progressListener);
        }
      }
      this.progressListener = null;

      this.browser = null;

      Services.obs.removeObserver(
        this.crashObserver,
        "oop-frameloader-crashed"
      );
      Services.obs.removeObserver(this.crashObserver, "ipc:content-shutdown");

      if (
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT
      ) {
        Services.obs.removeObserver(
          this.httpModifyRequest,
          "http-on-modify-request"
        );
      }
      this._cleanedUp = true;
    }

    dispatchCustomEvent(name, detail) {
      this.log(`dispatching ${name}`);
      let event = new CustomEvent(`${name}`, {
        bubbles: true,
        detail,
      });
      this.dispatchEvent(event);
    }

    handleEvent(event) {
      switch (event.type) {
        case "pagetitlechanged":
          this.dispatchCustomEvent("titlechange", {
            title: this.browser.contentTitle,
          });
          break;
        case "close":
        case "contextmenu":
        case "documentfirstpaint":
        case "iconchange":
        case "manifestchange":
        case "metachange":
        case "opensearch":
        case "recordingstatus":
        case "resize":
        case "scroll":
        case "scrollareachanged":
        case "showmodalprompt":
          this.dispatchCustomEvent(event.type, event.detail);
          break;
        case "backgroundcolor":
          this.progressListener.set_background_color(
            event.detail.backgroundcolor
          );
          break;
        case "processready": {
          event.stopPropagation();
          this._pid = parseInt(event.target.getAttribute("processid")) || -1;
          this.dispatchCustomEvent("processready", { processid: this._pid });
          this.updateDCSState(true);
          break;
        }
        case "promptpermission": {
          // Receive "promptpermission" event from ContentPermissionPrompt.
          // wait for the reply event from system app of event type requestId,
          // and dispatch back to ContentPermissionPrompt through this.browser.
          this.addEventListener(
            event.detail.requestId,
            e => {
              this.browser.dispatchEvent(
                new CustomEvent(event.detail.requestId, {
                  bubbles: true,
                  detail: e.detail,
                })
              );
            },
            { once: true }
          );
          break;
        }
        case "visibilitychange":
          // We dispatch this event with additional details when the web-view
          // active status changes, so we need to prevent the default event
          // from bubbling up.
          event.stopPropagation();
          break;
        default:
          this.error(`Unexpected event ${event.type}`);
      }
    }

    get frame() {
      return this.browser;
    }

    // Needed for Marionette integration.
    get linkedBrowser() {
      return this.browser;
    }

    // Returns this tab's MediaController object.
    get mediaController() {
      return this.browser.browsingContext.mediaController;
    }

    set src(url) {
      this.log(`set src to ${url}`);
      // If we are not yet connected to the DOM, add that action to the list
      // of attribute changes.
      if (!this.browser) {
        this.attrs.push({ name: "src", new_value: url });
      } else {
        this.browser.setAttribute("src", url);
      }
    }

    get src() {
      return this.browser ? this.browser.getAttribute("src") : null;
    }

    get currentURI() {
      return this.browser ? this.browser.currentURI?.spec : null;
    }

    get canGoBack() {
      return !!this.browser && this.browser.canGoBack;
    }

    goBack() {
      !!this.browser && this.browser.goBack();
    }

    get canGoForward() {
      return !!this.browser && this.browser.canGoForward;
    }

    goForward() {
      !!this.browser && this.browser.goForward();
    }

    focus() {
      this.log(`focus() browser available: ${!!this.browser}`);
      !!this.browser && this.browser.focus();
    }

    blur() {
      this.log(`blur() browser available: ${!!this.browser}`);
      !!this.browser && this.browser.blur();
    }

    get active() {
      return !!this.browser && this.browser.docShellIsActive;
    }

    set active(val) {
      if (this.browser) {
        let current = this.browser.docShellIsActive;
        this.browser.docShellIsActive = val;
        if (current !== val) {
          this.updateDCSState(val);
          this.log(`change docShellIsActive from ${current} to ${val}`);
          this.dispatchCustomEvent("visibilitychange", { visible: val });
          // "visibilitychange" event is dispatched on webview itself.
          // To notify its browser element, dispatch "webview-visibilitychange"
          // event on this.browser.
          let event = new CustomEvent("webview-visibilitychange", {
            bubbles: true,
            detail: { visible: val },
          });
          this.browser.dispatchEvent(event);
        }
      }
    }

    get visible() {
      return this.active;
    }

    set visible(val) {
      this.active = val;
    }

    // Returns a promise that resolves to the script value.
    executeScript(source) {
      this.log(`executeScript`);
      return this.browser?.webViewExecuteScript(source);
    }

    // Returns a promise that will resolve with the screenshot as a Blob.
    getScreenshot(max_width, max_height, mime_type) {
      this.log(`getScreenshot ${max_width}x${max_height}, ${mime_type}`);
      if (!this.browser) {
        return Promise.reject();
      }
      return this.browser.webViewGetScreenshot(
        max_width,
        max_height,
        mime_type
      );
    }

    // Returns a promise that will resolve with the background color.
    getBackgroundColor() {
      this.log(`getBackgroundColor `);
      if (!this.browser) {
        return Promise.reject();
      }
      return this.browser.webViewGetBackgroundColor();
    }

    reload(forced) {
      let webNav = Ci.nsIWebNavigation;
      let reloadFlags = forced
        ? webNav.LOAD_FLAGS_BYPASS_PROXY | webNav.LOAD_FLAGS_BYPASS_CACHE
        : webNav.LOAD_FLAGS_NONE;
      this.browser && this.browser.reloadWithFlags(reloadFlags);
    }

    stop() {
      this.browser && this.browser.stop();
    }

    get allowedAudioChannels() {
      return this.browser.allowedAudioChannels;
    }

    get processid() {
      return this._pid;
    }

    enableCursor() {
      this.browser.webViewSetCursorEnable(true);
    }

    disableCursor() {
      this.browser.webViewSetCursorEnable(false);
    }

    getCursorEnabled() {
      return this.browser.webViewGetCursorEnabled();
    }

    enterSelectionMode() {
      this.browser.webViewSetSelectionMode("active");
    }

    exitSelectionMode() {
      this.browser.webViewSetSelectionMode("none");
    }

    startSelection() {
      this.browser.webViewSetSelectionMode("start");
    }

    stopSelection() {
      this.browser.webViewSetSelectionMode("stop");
    }

    set fullZoom(val) {
      if (this.browser) {
        this.browser.fullZoom = val;
      }
    }

    get fullZoom() {
      return this.browser ? this.browser.fullZoom : 1;
    }

    set textZoom(val) {
      if (this.browser) {
        this.browser.textZoom = val;
      }
    }

    get textZoom() {
      return this.browser ? this.browser.textZoom : 1;
    }

    scrollToTop(smooth = true) {
      this.browser?.webViewScrollTo("top", smooth);
    }

    scrollToBottom(smooth = true) {
      this.browser?.webViewScrollTo("bottom", smooth);
    }

    updateDCSState(val) {
      lazy.DataControlService.updateVisible(this._pid, val);
    }

    activateKeyForwarding() {
      this.log(`activateKeyForwarding`);
      lazy.KeyboardAppProxy.activate(this.browser.frameLoader);
    }

    deactivateKeyForwarding() {
      this.log(`deactivateKeyForwarding`);
      lazy.KeyboardAppProxy.deactivate();
    }

    download(url) {
      this.log(`download ${url}`);
      this.browser?.webViewDownload(url);
    }

    set userAgent(value) {
      if (!this.nodePrincipal.isSystemPrincipal) {
        return;
      }
      this.log(`set userAgent`);
      this.setAttribute("useragent", value);
      // Update the User Agent with new value.
      if (!this.browser) {
        this.attrs.push({ name: "useragent", new_value: value });
      } else {
        this.browser.browsingContext.customUserAgent = value;
      }
    }

    get userAgent() {
      let default_useragent = defaultUserAgent();
      return this.browser
        ? this.browser.browsingContext.customUserAgent || default_useragent
        : this.getAttribute("useragent") || null;
    }

    set userAgentExtensions(value) {
      this.log(`set userAgentExtensions`);
      this.setAttribute("useragentextensions", value);
      // Update the User Agent with new token value.
      let useragent = defaultUserAgent() + " " + value;
      if (!this.browser) {
        this.attrs.push({ name: "useragent", new_value: useragent });
      } else {
        this.browser.browsingContext.customUserAgent = useragent;
      }
    }

    get userAgentExtensions() {
      return this.getAttribute("useragentextensions") || null;
    }

    enterModalState() {
      this.log(`EnterModalState`);
      this.browser?.enterModalState();
    }

    leaveModalState() {
      this.log(`LeaveModalState`);
      this.browser?.leaveModalState();
    }
  }

  webViewLogEnabled && console.log(`Setting up <web-view> custom element`);
  window.customElements.define("web-view", WebView);
})();
