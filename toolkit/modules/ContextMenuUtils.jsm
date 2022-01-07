/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContextMenuUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const kLongestReturnedString = 128;

var ContextMenuUtils = {
  log(...args) {
    dump("ContextMenuUtils: ");
    for (let a of args) {
      dump(a + " ");
    }
    dump("\n");
  },

  init(host) {
    // A cache of the menuitem dom objects keyed by the id we generate
    // and pass to the embedder
    host._ctxHandlers = {};
    // Counter of contextmenu events fired
    host._ctxCounter = 0;
  },

  cancel(host) {
    host._ctxHandlers = {};
  },

  generateMenu(host, global, event) {
    host._ctxCounter++;
    host._ctxHandlers = {};

    let elem = event.composedTarget;
    let menuData = { systemTargets: [], contextmenu: null };
    let ctxMenuId = null;
    let clipboardPlainTextOnly = Services.prefs.getBoolPref(
      "clipboard.plainTextOnly"
    );
    var copyableElements = {
      image: false,
      link: false,
      hasElements() {
        return this.image || this.link;
      },
    };

    // Set the event target as the copy image command needs it to
    // determine what was context-clicked on.
    global.docShell.contentViewer
      .QueryInterface(Ci.nsIContentViewerEdit)
      .setCommandNode(elem);

    while (elem && elem.parentNode) {
      let ctxData = this.getSystemCtxMenuData(global, elem);
      if (ctxData) {
        menuData.systemTargets.push({
          nodeName: elem.nodeName.toUpperCase(),
          data: ctxData,
        });
      }

      if (
        !ctxMenuId &&
        "hasAttribute" in elem &&
        elem.hasAttribute("contextmenu")
      ) {
        ctxMenuId = elem.getAttribute("contextmenu");
      }

      // Enable copy image/link option
      if (elem.nodeName.toUpperCase() == "IMG") {
        copyableElements.image = !clipboardPlainTextOnly;
      } else if (elem.nodeName.toUpperCase() == "A") {
        copyableElements.link = true;
      }

      elem = elem.parentNode;
    }

    if (ctxMenuId || copyableElements.hasElements()) {
      var menu = null;
      if (ctxMenuId) {
        menu = event.target.ownerDocument.getElementById(ctxMenuId);
      }
      menuData.contextmenu = this.buildMenuObj(
        host,
        menu,
        "",
        copyableElements
      );
    }

    // Pass along the position where the context menu should be located
    menuData.clientX = event.clientX;
    menuData.clientY = event.clientY;
    menuData.screenX = event.screenX;
    menuData.screenY = event.screenY;
    return menuData;
  },

  getSystemCtxMenuData(global, elem) {
    let documentURI = global.docShell.QueryInterface(Ci.nsIWebNavigation)
      .currentURI.spec;
    let content = global.content;
    if (
      (elem instanceof content.HTMLAnchorElement && elem.href) ||
      (elem instanceof content.HTMLAreaElement && elem.href)
    ) {
      return {
        uri: elem.href,
        documentURI,
        text: elem.textContent.substring(0, kLongestReturnedString),
      };
    }
    if (elem instanceof Ci.nsIImageLoadingContent && elem.currentURI) {
      return { uri: elem.currentURI.spec, documentURI };
    }
    if (elem instanceof content.HTMLImageElement) {
      return { uri: elem.src, documentURI };
    }
    if (elem instanceof content.HTMLMediaElement) {
      let hasVideo = !(
        elem.readyState >= elem.HAVE_METADATA &&
        (elem.videoWidth == 0 || elem.videoHeight == 0)
      );
      return {
        uri: elem.currentSrc || elem.src,
        hasVideo,
        documentURI,
      };
    }
    if (elem instanceof content.HTMLInputElement && elem.hasAttribute("name")) {
      // For input elements, we look for a parent <form> and if there is
      // one we return the form's method and action uri.
      let parent = elem.parentNode;
      while (parent) {
        if (
          parent instanceof content.HTMLFormElement &&
          parent.hasAttribute("action")
        ) {
          let actionHref = global.docShell
            .QueryInterface(Ci.nsIWebNavigation)
            .currentURI.resolve(parent.getAttribute("action"));
          let method = parent.hasAttribute("method")
            ? parent.getAttribute("method").toLowerCase()
            : "get";
          return {
            documentURI,
            action: actionHref,
            method,
            name: elem.getAttribute("name"),
          };
        }
        parent = parent.parentNode;
      }
    }
    return false;
  },

  buildMenuObj(host, menu, idPrefix, copyableElements) {
    let menuObj = { type: "menu", customized: false, items: [] };
    // Customized context menu
    if (menu) {
      this.maybeCopyAttribute(menu, menuObj, "label");

      for (let i = 0, child; (child = menu.children[i++]); ) {
        if (child.nodeName.toUpperCase() === "MENU") {
          menuObj.items.push(
            this.buildMenuObj(child, idPrefix + i + "_", false)
          );
        } else if (child.nodeName.toUpperCase() === "MENUITEM") {
          let id = this._ctxCounter + "_" + idPrefix + i;
          let menuitem = { id, type: "menuitem" };
          this.maybeCopyAttribute(child, menuitem, "label");
          this.maybeCopyAttribute(child, menuitem, "icon");
          host._ctxHandlers[id] = child;
          menuObj.items.push(menuitem);
        }
      }

      if (menuObj.items.length) {
        menuObj.customized = true;
      }
    }
    // Note: Display "Copy Link" first in order to make sure "Copy Image" is
    //       put together with other image options if elem is an image link.
    // "Copy Link" menu item
    if (copyableElements.link) {
      menuObj.items.push({ id: "copy-link" });
    }
    // "Copy Image" menu item
    if (copyableElements.image) {
      menuObj.items.push({ id: "copy-image" });
    }

    return menuObj;
  },

  handleContextMenuCallback(host, global, menuitem) {
    this.log(`Received handleContextMenuCallback: (${menuitem})`);

    let doCommandIfEnabled = command => {
      if (global.docShell.isCommandEnabled(command)) {
        global.docShell.doCommand(command);
      }
    };
    if (menuitem == "copy-image") {
      doCommandIfEnabled("cmd_copyImage");
    } else if (menuitem == "copy-link") {
      doCommandIfEnabled("cmd_copyLink");
    } else if (menuitem in host._ctxHandlers) {
      host._ctxHandlers[menuitem].click();
      host._ctxHandlers = {};
    } else {
      // We silently ignore if the embedder uses an incorrect id in the callback
      this.log("Ignored invalid contextmenu invocation");
    }
  },
};

Object.freeze(ContextMenuUtils);
