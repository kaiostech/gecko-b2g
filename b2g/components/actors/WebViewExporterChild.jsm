/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebViewExporterChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

function IsValidKey(aKey) {
  const blockList = [
    "constructor",
    "customInterfaces",
    "dispatchEvent",
    "getCustomInterfaceCallbac",
    "length",
    "name",
    "prototype",
    "QueryInterface",
  ];
  return !aKey.startsWith("__") && !blockList.includes(aKey);
}

function BuildProperties(aObj) {
  const properties = Object.getOwnPropertyDescriptors(aObj);
  const propertiesFiltered = {};
  Object.getOwnPropertyNames(properties).forEach(key => {
    if (IsValidKey(key)) {
      propertiesFiltered[key] = properties[key];
    }
  });
  return propertiesFiltered;
}

function DispatchToCurrentThread(f) {
  Services.tm.currentThread.dispatch(f, Ci.nsIThread.DISPATCH_NORMAL);
}

class WebViewExporterChild extends JSWindowActorChild {
  exportCustomElements() {
    // Prepare scope data to grab the class defined by customElements.js.
    this.window = {
      document: this.contentWindow.document,
      customElements: {
        define: (aTag, aClass) => (this.customElements.classes[aTag] = aClass),
      },
      addEventListener: this.contentWindow.addEventListener.bind(
        this.contentWindow
      ),
    };
    this.window.window = this.window;

    this.customElements = {
      setElementCreationCallback: (aTag, aCallback) => aCallback(),
      classes: {},
    };

    // customElement.js will import
    //  <browser> at browser-custom-element.js
    //  and <web-view> at web-view.js.
    try {
      Services.scriptloader.loadSubScript(
        "chrome://global/content/customElements.js",
        this
      );
    } catch (exception) {}

    // Prepare the prototype we want to export.
    const classWebView = this.customElements.classes["web-view"];
    const classBrowser = this.customElements.classes.browser;

    // Add closures
    classWebView.__doPollyfill = aObj => {
      Object.defineProperties(
        aObj,
        Object.getOwnPropertyDescriptors(classWebView.prototype)
      );
    };
    classWebView.__doPollyfillForBrowser = aBrowser => {
      Object.defineProperties(
        aBrowser,
        Object.getOwnPropertyDescriptors(classBrowser.prototype)
      );
      aBrowser.construct();
    };
    classWebView.prototype.__getActor = () => {
      return this; // Note: 'this' is the actor itself.
    };

    // Add non-closures
    classWebView.prototype.__dispatchEventImpl =
      classWebView.prototype.dispatchEvent;
    classWebView.prototype.dispatchEvent = function(aEvent) {
      // It is not allowed to dispatch events in MainThread,
      // this needs to run asynchronously.
      DispatchToCurrentThread(() => {
        // When creating a CustomEvent object,
        // we must create the object form the same window.
        const window = this.__getActor().contentWindow;
        const event = new window.CustomEvent(
          aEvent.type,
          Cu.cloneInto(
            {
              bubbles: aEvent.bubbles,
              detail: aEvent.detail,
            },
            window,
            {
              cloneFunctions: true,
            }
          )
        );
        this.__dispatchEventImpl(event);
      });
    };

    // Remove the APIs which aren't supported in the nested web-view.
    delete classWebView.prototype.enableCursor;
    delete classWebView.prototype.disableCursor;
    delete classWebView.prototype.getCursorEnabled;
    delete classWebView.prototype.activateKeyForwarding;
    delete classWebView.prototype.deactivateKeyForwarding;
    delete classWebView.prototype.enterModalState;
    delete classWebView.prototype.leaveModalState;

    // Now nested webview is inprocess and doesn't own a message manager.
    // Instead of messages, we use events when using a nested web-view.
    classBrowser.prototype.webViewGetBackgroundColor = function() {
      let id = `WebView::ReturnBackgroundColor::${this.webViewRequestId}`;
      this.webViewRequestId += 1;

      return new this.ownerGlobal.Promise((resolve, reject) => {
        const window = this.contentWindow;
        this.addEventListener(
          id,
          function got_backgroundColor(event) {
            let detail = event.detail;
            if (detail.success) {
              resolve(detail.result);
            } else {
              reject();
            }
          },
          { once: true }
        );
        const event = new window.CustomEvent(
          "webview-getbackgroundcolor",
          Cu.cloneInto(
            {
              detail: {
                id,
              },
              bubbles: true,
            },
            window
          )
        );
        window.dispatchEvent(event);
      });
    };

    classBrowser.prototype.webViewGetScreenshot = function(
      maxWidth,
      maxHeight,
      mimeType
    ) {
      let id = `WebView::ReturnScreenshot::${this.webViewRequestId}`;
      this.webViewRequestId += 1;

      return new this.ownerGlobal.Promise((resolve, reject) => {
        const window = this.contentWindow;
        this.addEventListener(
          id,
          event => {
            let detail = event.detail;
            if (detail.success) {
              resolve(detail.result);
            } else {
              reject();
            }
          },
          { once: true }
        );
        const event = new window.CustomEvent(
          "webview-getscreenshot",
          Cu.cloneInto(
            {
              detail: {
                id,
                maxWidth,
                maxHeight,
                mimeType,
              },
              bubbles: true,
            },
            window
          )
        );
        window.dispatchEvent(event);
      });
    };

    classWebView.prototype._getter = function(aName) {
      return this[aName];
    };
    classWebView.prototype._setter = function(aName, aValue) {
      this[aName] = aValue;
    };

    const webViewClassData = {
      properties: BuildProperties(classWebView.prototype),
      staticProperties: BuildProperties(classWebView),
    };

    Cu.exportFunction(
      () => {
        return Cu.cloneInto(webViewClassData, this.contentWindow, {
          cloneFunctions: true,
        });
      },
      this.contentWindow,
      { defineAs: `getWebViewClassData` }
    );

    // Do export in page script.
    Services.scriptloader.loadSubScript(
      "chrome://b2g/content/browser/web-view.js",
      this.contentWindow
    );

    delete this.customElements;
  }

  hasPermission(aType) {
    const permission = Services.perms.testPermissionFromPrincipal(
      this.document.nodePrincipal,
      aType
    );
    return permission == Services.perms.ALLOW_ACTION;
  }

  actorCreated() {
    if (
      this.document.nodePrincipal.isSystemPrincipal ||
      !this.hasPermission("web-view")
    ) {
      return;
    }
    console.log(`create webview exporter for ${this.document.URL}`);
    this.exportCustomElements();
  }

  handleEvent(aEvent) {}

  receiveMessage(message) {}
}
