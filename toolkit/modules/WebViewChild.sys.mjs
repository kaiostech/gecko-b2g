/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContextMenuUtils: "resource://gre/modules/ContextMenuUtils.sys.mjs",
  ScreenshotUtils: "resource://gre/modules/ScreenshotUtils.sys.mjs",
});

export function WebViewChild() {}

WebViewChild.prototype = {
  // Prints arguments separated by a space and appends a new line.
  log(...args) {
    dump("WebViewChild: ");
    for (let a of args) {
      dump(a + " ");
    }
    dump("\n");
  },

  init(global) {
    this.global = global;

    lazy.ContextMenuUtils.init(this);

    let els = Services.els;

    // We are using the system group for those events so if something in the
    // content called .stopPropagation() this will still be called.
    els.addSystemEventListener(
      global,
      "DOMWindowClose",
      this.windowCloseHandler.bind(this),
      /* useCapture = */ false
    );
    els.addSystemEventListener(
      global,
      "DOMWindowCreated",
      this.windowCreatedHandler.bind(this),
      /* useCapture = */ true
    );
    els.addSystemEventListener(
      global,
      "DOMWindowResize",
      this.windowResizeHandler.bind(this),
      /* useCapture = */ false
    );
    els.addSystemEventListener(
      global,
      "contextmenu",
      this.contextmenuHandler.bind(this),
      /* useCapture = */ false
    );
    els.addSystemEventListener(
      global,
      "scroll",
      this.scrollEventHandler.bind(this),
      /* useCapture = */ false
    );
    els.addSystemEventListener(
      global,
      "MozScrolledAreaChanged",
      this.scrollAreaChangedEventHandler.bind(this),
      /* useCapture = */ false
    );

    global.addMessageListener(
      "WebView::fire-ctx-callback",
      this.recvFireCtxCallback.bind(this)
    );
    global.addMessageListener(
      "WebView::MozDOMFullscreen:Entered",
      this.recvFullscreenEntered.bind(this)
    );
    global.addMessageListener(
      "WebView::MozDOMFullscreen:Exited",
      this.recvFullscreenExited.bind(this)
    );
    global.addMessageListener(
      "WebView::ScrollTo",
      this.recvScrollTo.bind(this)
    );

    let metachangeHandler = this.metaChangeHandler.bind(this);
    global.addEventListener(
      "DOMMetaAdded",
      metachangeHandler,
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );

    global.addEventListener(
      "DOMMetaChanged",
      metachangeHandler,
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );

    global.addEventListener(
      "DOMMetaRemoved",
      metachangeHandler,
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );

    global.addEventListener(
      "DOMLinkAdded",
      this.linkAddedHandler.bind(this),
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );

    let fullscreenHandler = this.fullscreenHandler.bind(this);
    global.addEventListener(
      "MozDOMFullscreen:Request",
      fullscreenHandler,
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );
    global.addEventListener(
      "MozDOMFullscreen:Exit",
      fullscreenHandler,
      /* useCapture = */ true,
      /* wantsUntrusted = */ false
    );

    // Remote the value of the background color since the parent can't get
    // it directly in its progress listener.
    // This will be dispatched before the parent's loadend so we can use
    // this value in the loadend event handler of the <web-view> element.
    let seenLoadStart = false;
    let seenLoadEnd = false;
    let logfn = this.log;
    let progressListener = {
      QueryInterface: ChromeUtils.generateQI([
        Ci.nsIWebProgressListener,
        Ci.nsISupportsWeakReference,
      ]),

      onStateChange(webProgress, request, stateFlags, status) {
        if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
          seenLoadStart = true;
        }

        if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          let backgroundcolor = "transparent";
          try {
            backgroundcolor = global.content
              .getComputedStyle(global.content.document.body)
              .getPropertyValue("background-color");
          } catch (e) {
            logfn(`Failed to get background-color property: ${e}`);
          }
          if (seenLoadStart && !seenLoadEnd) {
            global.sendAsyncMessage("WebView::backgroundcolor", {
              backgroundcolor,
            });
            seenLoadEnd = true;
          }
        }
      },
    };

    global.docShell
      .QueryInterface(Ci.nsIWebProgress)
      .addProgressListener(
        progressListener,
        Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
      );

    // Installs a message listener for screenshot requests.
    global.addMessageListener(
      "WebView::GetScreenshot",
      this.getScreenshot.bind(this)
    );

    // Installs a message listener for background color requests.
    global.addMessageListener(
      "WebView::GetBackgroundColor",
      this.getBackgroundColor.bind(this)
    );

    // Installs a message listener for virtual cursor requests.
    global.addMessageListener(
      "WebView::GetCursorEnabled",
      this.getCursorEnabled.bind(this)
    );
    global.addMessageListener(
      "WebView::SetCursorEnable",
      this.setCursorEnable.bind(this)
    );

    // Installs a message listener for executeScript requests.
    global.addMessageListener(
      "WebView::ExecuteScript",
      this.executeScript.bind(this)
    );

    global.addMessageListener(
      "WebView::SetSelectionMode",
      this.setSelectionMode.bind(this)
    );
  },

  getBackgroundColor(message) {
    let messageName = message.data.id;

    let content = this.global.content;
    if (!content) {
      this.global.sendAsyncMessage(messageName, {
        success: false,
      });
      return;
    }

    let backgroundcolor = "transparent";
    try {
      backgroundcolor = content
        .getComputedStyle(content.document.body)
        .getPropertyValue("background-color");
      this.global.sendAsyncMessage(messageName, {
        success: true,
        result: backgroundcolor,
      });
    } catch (e) {
      this.global.sendAsyncMessage(messageName, {
        success: false,
      });
    }
  },

  getScreenshot(message) {
    let data = message.data;
    let id = data.id;
    this.log(`Taking screenshot for ${JSON.stringify(data)}`);

    let content = this.global.content;
    if (!content) {
      this.global.sendAsyncMessage(id, {
        success: false,
      });
      return;
    }

    lazy.ScreenshotUtils.getScreenshot(
      content,
      data.maxWidth,
      data.maxHeight,
      data.mimeType
    ).then(
      blob => {
        this.global.sendAsyncMessage(id, {
          success: true,
          result: blob,
        });
      },
      () => {
        this.global.sendAsyncMessage(id, {
          success: false,
        });
      }
    );
  },

  getCursorEnabled(message) {
    let messageName = message.data.id;

    let content = this.global.content;
    if (
      !content ||
      !content.navigator ||
      !content.navigator.b2g ||
      !content.navigator.b2g.virtualCursor
    ) {
      this.global.sendAsyncMessage(messageName, {
        success: false,
      });
      return;
    }
    this.global.sendAsyncMessage(messageName, {
      success: true,
      result: content.navigator.b2g.virtualCursor.enabled,
    });
  },

  setCursorEnable(message) {
    let enable = message.data.enable;
    let content = this.global.content;
    if (
      !content ||
      !content.navigator ||
      !content.navigator.b2g ||
      !content.navigator.b2g.virtualCursor
    ) {
      return;
    }
    if (content.navigator.b2g.virtualCursor.enabled == enable) {
      return;
    }
    if (enable) {
      content.navigator.b2g.virtualCursor.enable();
    } else {
      content.navigator.b2g.virtualCursor.disable();
    }
  },

  setSelectionMode(message) {
    let selectionMode = message.data.selectionMode;
    let content = this.global.content;
    if (
      !content ||
      !content.navigator ||
      !content.navigator.b2g ||
      !content.navigator.b2g.virtualCursor
    ) {
      return;
    }

    this.log(
      `setSelectionMode to ${selectionMode}, is cursor enabled? ${content.navigator.b2g.virtualCursor.enabled}`
    );

    if (selectionMode == "active") {
      content.navigator.b2g.virtualCursor.enterSelectionMode();
    } else if (selectionMode == "start") {
      content.navigator.b2g.virtualCursor.startSelection();
    } else if (selectionMode == "stop") {
      content.navigator.b2g.virtualCursor.stopSelection();
      let selection = content.document.getSelection();
      if (selection) {
        selection.empty();
      }
    } else {
      // none
      content.navigator.b2g.virtualCursor.exitSelectionMode();
    }
  },

  executeScript(message) {
    let data = message.data;
    let content = this.global.content;

    let sandbox = new Cu.Sandbox([content], {
      sandboxPrototype: content,
      sandboxName: "webview-execute-script",
      allowWaivers: false,
      sameZoneAs: content,
    });

    // Check if a value can be sent over IPC.
    const maybeSendResult = result => {
      try {
        JSON.stringify(result);
        this.global.sendAsyncMessage(data.id, { success: true, result });
      } catch (e) {
        this.global.sendAsyncMessage(data.id, {
          success: false,
          error: "invalid_json",
        });
      }
    };

    try {
      let result = Cu.evalInSandbox(data.source, sandbox, "latest");
      // If the result is a Promise, wait for it to resolve/reject to
      // properly send back the result over IPC.
      if (result instanceof sandbox.Promise) {
        result.then(
          success => {
            maybeSendResult(success);
          },
          error => {
            this.global.sendAsyncMessage(data.id, { success: false, error });
          }
        );
      } else {
        maybeSendResult(result);
      }
    } catch (e) {
      this.global.sendAsyncMessage(data.id, { success: false });
    }
  },

  // Processes the "rel" field in <link> tags and forward to specific handlers.
  linkAddedHandler(event) {
    let win = event.target.ownerGlobal;
    if (win != this.global.content) {
      return;
    }

    let iconchangeHandler = this.iconChangedHandler.bind(this);
    let handlers = {
      icon: iconchangeHandler,
      "apple-touch-icon": iconchangeHandler,
      "apple-touch-icon-precomposed": iconchangeHandler,
      search: this.openSearchHandler.bind(this),
      manifest: this.manifestChangedHandler.bind(this),
    };

    this.log(`Got linkAdded: (${event.target.href}) ${event.target.rel}`);
    event.target.rel.split(" ").forEach(function (x) {
      let token = x.toLowerCase();
      if (handlers[token]) {
        handlers[token](event);
      }
    }, this);
  },

  iconChangedHandler(event) {
    let target = event.target;
    this.log(`Got iconchanged: (${target.href})`);
    let icon = { href: target.href };
    this.maybeCopyAttribute(target, icon, "sizes");
    this.maybeCopyAttribute(target, icon, "rel");
    this.global.sendAsyncMessage("WebView::iconchange", icon);
  },

  openSearchHandler(event) {
    let target = event.target;
    this.log(`Got opensearch: (${target.href})`);

    if (target.type !== "application/opensearchdescription+xml") {
      return;
    }
    this.global.sendAsyncMessage("WebView::opensearch", {
      title: target.title,
      href: target.href,
    });
  },

  manifestChangedHandler(event) {
    let target = event.target;
    this.log(`Got manifestchanged: (${target.href})`);
    let manifest = { href: target.href };
    this.global.sendAsyncMessage("WebView::manifestchange", manifest);
  },

  metaChangeHandler(event) {
    let win = event.target.ownerGlobal;
    if (win != this.global.content) {
      return;
    }

    let name = event.target.name;
    let property = event.target.getAttributeNS(null, "property");

    if (!name && !property) {
      return;
    }

    this.log(`Got metaChanged: (${name || property}) ${event.target.content}`);

    let genericHandler = this.genericMetaHandler.bind(this);

    let handlers = {
      viewmode: genericHandler,
      "theme-color": genericHandler,
      "theme-group": genericHandler,
      "application-name": this.applicationNameChangedHandler.bind(this),
    };
    let handler = handlers[name];

    if ((property || name).match(/^og:/)) {
      name = property || name;
      handler = genericHandler;
    }

    if (handler) {
      handler(name, event.type, event.target);
    }
  },

  genericMetaHandler(name, eventType, target) {
    let meta = {
      name,
      content: target.content,
      type: eventType.replace("DOMMeta", "").toLowerCase(),
    };
    this.global.sendAsyncMessage("WebView::metachange", meta);
  },

  applicationNameChangedHandler(name, eventType, target) {
    if (eventType !== "DOMMetaAdded") {
      // Bug 1037448 - Decide what to do when <meta name="application-name">
      // changes
      return;
    }

    let meta = { name, content: target.content };

    let lang;
    let elm;

    for (
      elm = target;
      !lang && elm && elm.nodeType == target.ELEMENT_NODE;
      elm = elm.parentNode
    ) {
      if (elm.hasAttribute("lang")) {
        lang = elm.getAttribute("lang");
        continue;
      }

      if (elm.hasAttributeNS("http://www.w3.org/XML/1998/namespace", "lang")) {
        lang = elm.getAttributeNS(
          "http://www.w3.org/XML/1998/namespace",
          "lang"
        );
        continue;
      }
    }

    // No lang has been detected.
    if (!lang && elm.nodeType == target.DOCUMENT_NODE) {
      lang = elm.contentLanguage;
    }

    if (lang) {
      meta.lang = lang;
    }

    this.global.sendAsyncMessage("WebView::metachange", meta);
  },

  addMozAfterPaintHandler(callback) {
    let self = this;
    function onMozAfterPaint() {
      let uri = self.global.docShell.QueryInterface(
        Ci.nsIWebNavigation
      ).currentURI;
      if (uri.spec != "about:blank") {
        self.log(`Got afterpaint event: ${uri.spec}`);
        self.global.removeEventListener(
          "MozAfterPaint",
          onMozAfterPaint,
          /* useCapture = */ true
        );
        callback();
      }
    }
    this.global.addEventListener(
      "MozAfterPaint",
      onMozAfterPaint,
      /* useCapture = */ true
    );
    return onMozAfterPaint;
  },

  windowCreatedHandler(event) {
    let targetDocShell = event.target.defaultView.docShell;
    if (targetDocShell != this.global.docShell) {
      return;
    }

    let uri = this.global.docShell.QueryInterface(
      Ci.nsIWebNavigation
    ).currentURI;
    this.log("Window created: " + uri.spec);
    if (uri.spec != "about:blank") {
      this.addMozAfterPaintHandler(() => {
        this.global.sendAsyncMessage("WebView::documentfirstpaint");
      });
    }
  },

  windowCloseHandler(event) {
    let win = event.target;
    if (win != this.global.content || event.defaultPrevented) {
      return;
    }

    this.log("Closing window " + win);
    this.global.sendAsyncMessage("WebView::close");

    // Inform the window implementation that we handled this close ourselves.
    event.preventDefault();
  },

  windowResizeHandler(event) {
    let win = event.target;
    if (win != this.global.content || event.defaultPrevented) {
      return;
    }

    this.log("resizing window " + win);
    this.global.sendAsyncMessage("WebView::resize", {
      width: event.detail.width,
      height: event.detail.height,
    });

    // Inform the window implementation that we handled this resize ourselves.
    event.preventDefault();
  },

  contextmenuHandler(event) {
    this.log(event.type);
    if (event.defaultPrevented || event.detail.nested) {
      return;
    }
    let menuData = lazy.ContextMenuUtils.generateMenu(this, this.global, event);

    // The value returned by the contextmenu sync call is true if the embedder
    // called preventDefault() on its contextmenu event.
    //
    // We call preventDefault() on our contextmenu event if the embedder called
    // preventDefault() on /its/ contextmenu event.  This way, if the embedder
    // ignored the contextmenu event, TabChild will fire a click.
    if (this.global.sendSyncMessage("WebView::contextmenu", menuData)[0]) {
      event.preventDefault();
    } else {
      lazy.ContextMenuUtils.cancel(this);
    }
  },

  maybeCopyAttribute(src, target, attribute) {
    if (src.getAttribute(attribute)) {
      target[attribute] = src.getAttribute(attribute);
    }
  },

  recvFireCtxCallback(data) {
    this.log(`Received fireCtxCallback message: (${data.json.menuitem})`);
    lazy.ContextMenuUtils.handleContextMenuCallback(
      this,
      this.global,
      data.json.menuitem
    );
  },

  recvFullscreenEntered(data) {
    this.log(`Received WebView::MozDOMFullscreen:Entered`);
    let content = this.global.content;
    if (!content || !content.windowUtils) {
      return;
    }
    if (
      !content.windowUtils.handleFullscreenRequests() &&
      !content.document.fullscreenElement
    ) {
      this.global.sendAsyncMessage(`WebView::fullscreen::exit`, {});
      return;
    }

    let element = content.document.fullscreenElement;
    if (element && element.mozOrientationLockEnabled) {
      if (element.videoWidth > element.videoHeight) {
        content.screen.orientation.lock("landscape");
        element.mozIsOrientationLocked = true;
      }
    }
  },

  recvFullscreenExited(data) {
    this.log(`Received WebView::MozDOMFullscreen:Exited`);
    let content = this.global.content;
    if (!content || !content.windowUtils) {
      return;
    }

    let element = content.document.fullscreenElement;
    if (element && element.mozOrientationLockEnabled) {
      if (element.mozIsOrientationLocked) {
        content.screen.orientation.unlock();
        element.mozIsOrientationLocked = false;
      }
    }

    content.windowUtils.exitFullscreen();
  },

  scrollEventHandler(event) {
    let doc = event.target;
    if (doc != this.global.content.document || event.defaultPrevented) {
      return;
    }

    const win = doc.ownerGlobal;
    this.global.sendAsyncMessage("WebView::scroll", {
      top: win.scrollY,
      left: win.scrollX,
    });
  },

  scrollAreaChangedEventHandler(event) {
    let doc = event.target;
    if (doc != this.global.content.document || event.defaultPrevented) {
      return;
    }

    this.global.sendAsyncMessage("WebView::scrollareachanged", {
      width: event.width,
      height: event.height,
    });
  },

  fullscreenHandler(event) {
    this.log(`Handle ${event.type}`);
    switch (event.type) {
      case "MozDOMFullscreen:Request":
      case "MozDOMFullscreen:Exit": {
        let name = "WebView::fullscreen::";
        let type = event.type.split(":")[1].toLowerCase();
        this.log(`sending: ${name}${type}`);
        this.global.sendAsyncMessage(`${name}${type}`, {});
      }
    }
  },

  recvScrollTo(data) {
    this.log(`Received WebView::ScrollTo`);
    let content = this.global.content;

    let params = data.json;
    let options = {
      top: params.where === "top" ? 0 : content.scrollMaxY,
    };
    if (params.smooth) {
      options.behavior = "smooth";
    }
    content.scrollTo(options);
  },
};
