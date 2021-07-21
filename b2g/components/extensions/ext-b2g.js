/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

function log(msg) {
  dump(`ext-b2g.js: ${msg}\n`);
}

log(`Loading...`);

ChromeUtils.defineModuleGetter(
  this,
  "WebExtensionsEmbedding",
  "resource://gre/modules/WebExtensionsEmbedding.jsm"
);

// ChromeUtils.defineModuleGetter(
//   this,
//   "GeckoViewTabBridge",
//   "resource://gre/modules/GeckoViewTab.jsm"
// );

// ChromeUtils.defineModuleGetter(
//   this,
//   "mobileWindowTracker",
//   "resource://gre/modules/GeckoViewWebExtension.jsm"
// );

// var { EventDispatcher } = ChromeUtils.import(
//   "resource://gre/modules/Messaging.jsm"
// );

var { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
var { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var { DefaultWeakMap, ExtensionError } = ExtensionUtils;

var { defineLazyGetter } = ExtensionCommon;

// global.GlobalEventDispatcher = EventDispatcher.instance;

const BrowserStatusFilter = Components.Constructor(
  "@mozilla.org/appshell/component/browser-status-filter;1",
  "nsIWebProgress",
  "addProgressListener"
);

const WINDOW_TYPE = "navigator:browser";

// We need let to break cyclic dependency
/* eslint-disable-next-line prefer-const */
let windowTracker;

/**
 * A nsIWebProgressListener for a specific XUL browser, which delegates the
 * events that it receives to a tab progress listener, and prepends the browser
 * to their arguments list.
 *
 * @param {XULElement} browser
 *        A XUL browser element.
 * @param {object} listener
 *        A tab progress listener object.
 * @param {integer} flags
 *        The web progress notification flags with which to filter events.
 */
class BrowserProgressListener {
  constructor(browser, listener, flags) {
    this.listener = listener;
    this.browser = browser;
    this.filter = new BrowserStatusFilter(this, flags);
    this.browser.addProgressListener(this.filter, flags);
  }

  /**
   * Destroy the listener, and perform any necessary cleanup.
   */
  destroy() {
    this.browser.removeProgressListener(this.filter);
    this.filter.removeProgressListener(this);
  }

  /**
   * Calls the appropriate listener in the wrapped tab progress listener, with
   * the wrapped XUL browser object as its first argument, and the additional
   * arguments in `args`.
   *
   * @param {string} method
   *        The name of the nsIWebProgressListener method which is being
   *        delegated.
   * @param {*} args
   *        The arguments to pass to the delegated listener.
   * @private
   */
  delegate(method, ...args) {
    if (this.listener[method]) {
      this.listener[method](this.browser, ...args);
    }
  }

  onLocationChange(webProgress, request, locationURI, flags) {
    const window = this.browser.ownerGlobal;
    // GeckoView windows can become popups at any moment, so we need to check
    // here
    if (!windowTracker.isBrowserWindow(window)) {
      return;
    }

    this.delegate("onLocationChange", webProgress, request, locationURI, flags);
  }
  onStateChange(webProgress, request, stateFlags, status) {
    this.delegate("onStateChange", webProgress, request, stateFlags, status);
  }
}

const PROGRESS_LISTENER_FLAGS =
  Ci.nsIWebProgress.NOTIFY_STATE_ALL | Ci.nsIWebProgress.NOTIFY_LOCATION;

class ProgressListenerWrapper {
  constructor(window, listener) {
    this.listener = new BrowserProgressListener(
      window.browser,
      listener,
      PROGRESS_LISTENER_FLAGS
    );
  }

  destroy() {
    this.listener.destroy();
  }
}

class WindowTracker extends WindowTrackerBase {
  constructor(...args) {
    log(`WindowTracker::WindowTracker ${args}`);
    super(...args);

    this.progressListeners = new DefaultWeakMap(() => new WeakMap());
  }

  isBrowserWindow(window) {
    let { documentElement } = window.document;
    // log(
    //   `WindowTracker::isBrowserWindow ${
    //     window.location
    //   } windowtype=${documentElement.getAttribute("windowtype")}`
    // );
    // TODO(fabrice): support 2nd screen which uses a different url.
    let res =
      documentElement.getAttribute("windowtype") === WINDOW_TYPE ||
      window.location == "chrome://system/content/index.html";
    // log(`  -> ${res}`);
    return res;
  }

  addProgressListener(window, listener) {
    log(`WindowTracker::addProgressListener`);
    // const listeners = this.progressListeners.get(window);
    // if (!listeners.has(listener)) {
    //   const wrapper = new ProgressListenerWrapper(window, listener);
    //   listeners.set(listener, wrapper);
    // }
  }

  removeProgressListener(window, listener) {
    log(`WindowTracker::removeProgressListener`);
    // const listeners = this.progressListeners.get(window);
    // const wrapper = listeners.get(listener);
    // if (wrapper) {
    //   wrapper.destroy();
    //   listeners.delete(listener);
    // }
  }
}

/**
 * Helper to create an event manager which listens for an event in the Android
 * global EventDispatcher, and calls the given listener function whenever the
 * event is received. That listener function receives a `fire` object,
 * which it can use to dispatch events to the extension, and an object
 * detailing the EventDispatcher event that was received.
 *
 * @param {BaseContext} context
 *        The extension context which the event manager belongs to.
 * @param {string} name
 *        The API name of the event manager, e.g.,"runtime.onMessage".
 * @param {string} event
 *        The name of the EventDispatcher event to listen for.
 * @param {function} listener
 *        The listener function to call when an EventDispatcher event is
 *        recieved.
 *
 * @returns {object} An injectable api for the new event.
 */
// global.makeGlobalEvent = function makeGlobalEvent(
//   context,
//   name,
//   event,
//   listener
// ) {
//   return new EventManager({
//     context,
//     name,
//     register: fire => {
//       const listener2 = {
//         onEvent(event, data, callback) {
//           listener(fire, data);
//         },
//       };

//       GlobalEventDispatcher.registerListener(listener2, [event]);
//       return () => {
//         GlobalEventDispatcher.unregisterListener(listener2, [event]);
//       };
//     },
//   }).api();
// };

// Return an array of <web-views> loaded with content that is not a WebExtension popup.
function getWebViewsForWindow(someWindow) {
  // Find the web-view container.
  let window = !!someWindow?.systemapp ? someWindow.systemapp : someWindow;
  if (!window) {
    return [];
  }

  // TODO(fabrice): This selector should be a pref since it depends on the system app layout.
  return Array.from(
    window.document.querySelectorAll("window-manager web-view")
  );
}

class TabTracker extends TabTrackerBase {
  constructor() {
    super();

    this._tabIds = new Map();
  }

  init() {
    log(`TabTracker::init`);

    if (this.initialized) {
      return;
    }
    this.initialized = true;

    Services.obs.addObserver((nativeTab, topic) => {
      let tabId = nativeTab._extensionId;
      log(`Observing ${topic}: ${nativeTab.localName} / ${tabId}`);
      this.emit("tab-created", { nativeTab });
      nativeTab.linkedBrowser._extensionId = tabId;
      this._tabIds.set(tabId, nativeTab);
    }, "webview-created");

    Services.obs.addObserver((nativeTab, topic) => {
      let tabId = nativeTab._extensionId;
      log(`Observing ${topic}: ${nativeTab.localName} / ${tabId}`);
      this.emit("tab-removed", { nativeTab, tabId });
      this._tabIds.delete(tabId);
    }, "webview-removed");

    Services.obs.addObserver((nativeTab, topic) => {
      let tabId = nativeTab._extensionId;
      log(`Observing ${topic}: ${nativeTab.localName} / ${tabId}`);
      this.emit("tab-activated", { nativeTab, tabId });
    }, "webview-activated");
  }

  getId(nativeTab) {
    log(`TabTracker::getId (${nativeTab}) -> ${nativeTab._extensionId}`);
    return nativeTab._extensionId;
  }

  getTab(tabId, default_ = undefined) {
    log(`TabTracker::getTab(tabId=${tabId}, default=${default_})`);
    let nativeTab = this._tabIds.get(tabId);
    if (nativeTab) {
      return nativeTab;
    }
    if (default_ !== undefined) {
      return default_;
    }

    throw new ExtensionError(`TabTracker::getTab: Invalid tab ID: ${tabId}`);
  }

  getBrowserData(browser) {
    // log(`TabTracker::getBrowserData (${browser})`);

    const window = browser.ownerGlobal;
    const windowId = windowTracker.getId(window);
    // log(`   window=${browser.ownerGlobal} windowId=${windowId}`);

    if (!windowTracker.isBrowserWindow(window)) {
      return {
        windowId,
        tabId: -1,
      };
    }

    return {
      windowId,
      tabId: browser._extensionId,
    };
  }

  get activeTab() {
    log(`TabTracker::activeTab`);

    return getWebViewsForWindow(windowTracker.topWindow).find(webView => {
      return webView.active;
    });
  }
}

windowTracker = new WindowTracker();
const tabTracker = new TabTracker();
tabTracker.init();

Object.assign(global, { tabTracker, windowTracker });

class Tab extends TabBase {
  get _favIconUrl() {
    return undefined;
  }

  get attention() {
    return false;
  }

  get audible() {
    return this.nativeTab.playingAudio;
  }

  get browser() {
    return this.nativeTab.linkedBrowser;
  }

  get discarded() {
    return this.browser.getAttribute("pending") === "true";
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.nativeTab);
  }

  get height() {
    return this.browser.clientHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isBrowserPrivate(this.browser);
  }

  get index() {
    return 0;
  }

  get mutedInfo() {
    return { muted: false };
  }

  get lastAccessed() {
    return this.nativeTab.lastTouchedAt;
  }

  get pinned() {
    return false;
  }

  get active() {
    return this.nativeTab.active;
  }

  get highlighted() {
    return this.active;
  }

  get selected() {
    return this.nativeTab.active;
  }

  get status() {
    if (this.browser.webProgress.isLoadingDocument) {
      return "loading";
    }
    return "complete";
  }

  get successorTabId() {
    return -1;
  }

  get width() {
    return this.browser.clientWidth;
  }

  get window() {
    return this.browser.ownerGlobal;
  }

  get windowId() {
    return windowTracker.getId(this.window);
  }

  // TODO: Just return false for these until properly implemented on Android.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1402924
  get isArticle() {
    return false;
  }

  get isInReaderMode() {
    return false;
  }

  get hidden() {
    return false;
  }

  get sharingState() {
    return {
      screen: undefined,
      microphone: false,
      camera: false,
    };
  }
}

// Manages tab-specific context data and dispatches tab select and close events.
class TabContext extends EventEmitter {
  constructor(getDefaultPrototype) {
    super();

    windowTracker.addListener("progress", this);

    this.getDefaultPrototype = getDefaultPrototype;
    this.tabData = new Map();
  }

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (!webProgress.isTopLevel) {
      // Only pageAction and browserAction are consuming the "location-change" event
      // to update their per-tab status, and they should only do so in response of
      // location changes related to the top level frame (See Bug 1493470 for a rationale).
      return;
    }
    const { tab } = browser.ownerGlobal;
    // fromBrowse will be false in case of e.g. a hash change or history.pushState
    const fromBrowse = !(
      flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
    );
    this.emit(
      "location-change",
      {
        id: tab.id,
        linkedBrowser: browser,
        // TODO: we don't support selected so we just alway say we are
        selected: true,
      },
      fromBrowse
    );
  }

  get(tabId) {
    if (!this.tabData.has(tabId)) {
      const data = Object.create(this.getDefaultPrototype(tabId));
      this.tabData.set(tabId, data);
    }

    return this.tabData.get(tabId);
  }

  clear(tabId) {
    this.tabData.delete(tabId);
  }

  shutdown() {
    windowTracker.removeListener("progress", this);
  }
}

class Window extends WindowBase {
  get focused() {
    return this.window.document.hasFocus();
  }

  isCurrentFor(context) {
    // In GeckoView the popup is on a separate window so the current window for
    // the popup is whatever is the topWindow.
    // TODO: Bug 1651506 use context?.viewType === "popup" instead
    // if (context?.currentWindow?.moduleManager.settings.isPopup) {
    //   return mobileWindowTracker.topWindow == this.window;
    // }
    return super.isCurrentFor(context);
  }

  get top() {
    return this.window.screenY;
  }

  get left() {
    return this.window.screenX;
  }

  get width() {
    return this.window.outerWidth;
  }

  get height() {
    return this.window.outerHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isWindowPrivate(this.window);
  }

  get alwaysOnTop() {
    return false;
  }

  get isLastFocused() {
    return this.window === windowTracker.topWindow;
  }

  get state() {
    return "fullscreen";
  }

  *getTabs() {
    log(`getTabs systemapp=${this.window.systemapp}`);

    let webViews = getWebViewsForWindow(this.window);
    log(`Found ${webViews.length} web-view`);

    let { tabManager } = this.extension;

    for (let nativeTab of webViews) {
      log(`webView: ${nativeTab.src}`);
      let tab = tabManager.getWrapper(nativeTab);
      if (tab) {
        yield tab;
      }
    }
  }

  *getHighlightedTabs() {
    yield this.activeTab;
  }

  get activeTab() {
    let activeWebView = getWebViewsForWindow(this.window).find(webView => {
      return webView.active;
    });

    if (activeWebView) {
      const { tabManager } = this.extension;
      return tabManager.getWrapper(activeWebView);
    }

    log(`Window::activeTab: no active tab found!`);
  }

  getTabAtIndex(index) {
    if (index == 0) {
      return this.activeTab;
    }
  }
}

Object.assign(global, { Tab, TabContext, Window });

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    const nativeTab = tabTracker.getTab(tabId, default_);

    if (nativeTab) {
      return this.getWrapper(nativeTab);
    }
    return default_;
  }

  addActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.addActiveTabPermission(nativeTab);
  }

  revokeActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.revokeActiveTabPermission(nativeTab);
  }

  canAccessTab(nativeTab) {
    return (
      this.extension.privateBrowsingAllowed ||
      !PrivateBrowsingUtils.isBrowserPrivate(nativeTab.linkedBrowser)
    );
  }

  wrapTab(nativeTab) {
    return new Tab(this.extension, nativeTab, nativeTab._extensionId);
  }
}

class WindowManager extends WindowManagerBase {
  get(windowId, context) {
    const window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  *getAll(context) {
    for (const window of windowTracker.browserWindows()) {
      if (!this.canAccessWindow(window, context)) {
        continue;
      }
      const wrapped = this.getWrapper(window);
      if (wrapped) {
        yield wrapped;
      }
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}

// eslint-disable-next-line mozilla/balanced-listeners
extensions.on("startup", (type, extension) => {
  defineLazyGetter(extension, "tabManager", () => new TabManager(extension));
  defineLazyGetter(
    extension,
    "windowManager",
    () => new WindowManager(extension)
  );
});

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
const getSender = (extension, target, sender) => {
  let tabId = -1;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (ChromeUtils.getClassName(target) == "XULFrameElement") {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId != null && tabId >= 0) {
    const tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
};

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    const window = context.xulBrowser.ownerGlobal;
    // GeckoViewTabBridge.closeTab({
    //   window,
    //   extensionId: context.extension.id,
    // });
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = async extension => {
  const { options_ui } = extension.manifest;
  const extensionId = extension.id;

  if (options_ui.open_in_tab) {
    // Delegate new tab creation and open the options page in the new tab.
    const tab = await WebExtensionsEmbedding.createNewTab({
      extensionId,
      createProperties: {
        url: options_ui.page,
        active: true,
      },
    });

    const { linkedBrowser } = tab;
    const flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    linkedBrowser.loadURI(options_ui.page, {
      flags,
      triggeringPrincipal: extension.principal,
    });

    const newWindow = linkedBrowser.ownerGlobal;
    // mobileWindowTracker.setTabActive(newWindow, true);
    return;
  }

  // Delegate option page handling to the app.
  // return GeckoViewTabBridge.openOptionsPage(extensionId);
};
