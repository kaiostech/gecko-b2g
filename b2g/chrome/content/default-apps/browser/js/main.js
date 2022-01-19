/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let tabs = [];
let activatedTab = null;
let commands = [
  "action-back",
  "action-forward",
  "action-go",
  "action-close",
  "action-get-bgcolor",
  "action-show-prompt",
  "action-get-screenshot",
  "action-resize",
];

function log(msg) {
  console.log(`Default browser app: ${msg}`);
}

class BrowserTab {
  constructor(document, url, browserElement) {
    this.parentDocument = document;
    this.webview = this.parentDocument.createElement("web-view");
    this.webview.openWindowInfo = null;
    if (browserElement) {
      this.webview.browserElement = browserElement;
    } else {
      this.webview.setAttribute("id", "browser");
      this.webview.setAttribute("src", url);
    }
    this.parentDocument.body.append(this.webview);
    this.parentDocument
      .getElementById("action-go")
      .toggleAttribute("disabled", false);
    this.parentDocument
      .getElementById("action-close")
      .toggleAttribute("disabled", false);
    this.parentDocument
      .getElementById("action-get-bgcolor")
      .toggleAttribute("disabled", false);
    this.parentDocument
      .getElementById("action-show-prompt")
      .toggleAttribute("disabled", false);
    this.parentDocument
      .getElementById("action-get-screenshot")
      .toggleAttribute("disabled", false);
    this.parentDocument
      .getElementById("action-resize")
      .toggleAttribute("disabled", false);
    this.show();
  }

  show() {
    if (activatedTab) {
      activatedTab.hide();
    }
    this.webview.style.display = "block";

    // Binding events
    this.webview.addEventListener("locationchange", this.updateActionsUI);
    this.webview.addEventListener("openwindow", this.openWindow);
    this.webview.addEventListener("iconchange", this.setupIcon);
    this.webview.addEventListener("titlechange", this.updateTitle);
    this.webview.addEventListener("opensearch", this.updateSearch);
    this.webview.addEventListener("manifestchange", this.manifestChange);
    this.webview.addEventListener("loadstart", this.updateLoadState);
    this.webview.addEventListener("loadend", this.updateLoadState);
    this.webview.addEventListener("contextmenu", this.showContextMenu);
    this.webview.addEventListener("scroll", this.updateScrollState);
    this.webview.addEventListener("scrollareachanged", this.updateScrollArea);

    this.parentDocument.getElementById("url").value = this.webview.src;
    this.parentDocument
      .getElementById("action-back")
      .toggleAttribute("disabled", !this.webview.canGoBack);
    this.parentDocument
      .getElementById("action-forward")
      .toggleAttribute("disabled", !this.webview.canGoForward);
    activatedTab = this;
    this.setupIcon({});
    this.updateTitle({});
    this.updateSearch({});
    this.manifestChange({});
    this.updateLoadState({});
  }

  hide() {
    if (activatedTab != this) {
      return;
    }
    this.webview.style.display = "none";
    this.webview.removeEventListener("locationchange", this.updateActionsUI);
    this.webview.removeEventListener("openwindow", this.openWindow);
    this.webview.removeEventListener("iconchange", this.setupIcon);
    this.webview.removeEventListener("loadstart", this.updateLoadState);
    this.webview.removeEventListener("loadend", this.updateLoadState);
    this.webview.removeEventListener("contextmenu", this.showContextMenu);
    this.webview.removeEventListener("scroll", this.updateScrollState);
    this.webview.removeEventListener(
      "scrollareachanged",
      this.updateScrollArea
    );

    this.parentDocument.getElementById("url").value = "";
    this.parentDocument
      .getElementById("action-back")
      .toggleAttribute("disabled", true);
    this.parentDocument
      .getElementById("action-forward")
      .toggleAttribute("disabled", true);
    this.setupIcon({});
    this.updateTitle({});
    this.updateSearch({});
    this.manifestChange({});
    this.updateLoadState({});
    activatedTab = null;
  }

  openWindow(aEvent) {
    log(
      `openWindow noreferrer=${aEvent.detail.forceNoReferrer} noopener=${aEvent.detail.forceNoOpener}`
    );
    tabs.push(
      new BrowserTab(
        activatedTab.parentDocument,
        null,
        aEvent.detail.frameElement
      )
    );
  }

  setupIcon(aEvent) {
    log("setupIcon");
    let href = aEvent?.detail?.href;
    if (activatedTab) {
      activatedTab.parentDocument.querySelector(
        "#tabs > .icon"
      ).style.display = href ? "inline" : "none";
      activatedTab.parentDocument.querySelector("#tabs > .icon").src = href;
    }
  }

  updateTitle(aEvent) {
    let title = aEvent?.detail?.title;
    log("updateTitle " + title);
    if (activatedTab) {
      activatedTab.parentDocument.querySelector(
        "#tabs > .title"
      ).innerText = title;
    }
  }

  updateSearch(aEvent) {
    let title = aEvent?.detail?.title;
    let href = aEvent?.detail?.href;
    log(`updateSearch ${title}:${href}`);
    if (activatedTab) {
      activatedTab.parentDocument.querySelector("#tabs > .search").innerText =
        title + " " + href;
    }
  }

  manifestChange(aEvent) {
    let href = aEvent?.detail?.href;
    log(`manifestChange ${href}`);
  }

  updateActionsUI(aEvent) {
    log(
      "updateActionsUI " +
        aEvent.detail.url +
        " " +
        aEvent.detail.canGoBack +
        " " +
        aEvent.detail.canGoForward
    );
    activatedTab.parentDocument
      .getElementById("action-back")
      .toggleAttribute("disabled", !aEvent.detail.canGoBack);
    activatedTab.parentDocument
      .getElementById("action-forward")
      .toggleAttribute("disabled", !aEvent.detail.canGoForward);
    activatedTab.parentDocument.getElementById("url").value = aEvent.detail.url;
  }

  updateLoadState(aEvent) {
    if (activatedTab) {
      activatedTab.parentDocument.querySelector(
        "#tabs > .loadstate"
      ).innerText =
        aEvent && aEvent.type == "loadstart" ? "loading" : "finished";
    }
  }

  showContextMenu(aEvent) {
    let detail = aEvent.detail;
    log(`showContextMenu`);
    if (detail.contextmenu) {
      detail.contextmenu.items.forEach(choice => {
        log(`menu item ${choice.id}`);
      });
      log(`item 0 id= ${detail.contextmenu.items[0].id}`);
      detail.contextMenuItemSelected(detail.contextmenu.items[0].id);
    }
  }

  updateScrollState(aEvent) {
    log(`scroll ${aEvent.detail.top} ${aEvent.detail.left}`);
  }

  updateScrollArea(aEvent) {
    log(`scrollareachanged ${aEvent.detail.width} ${aEvent.detail.height}`);
  }

  go() {
    log("go " + this.parentDocument.getElementById("url").value);
    this.webview.src = this.parentDocument.getElementById("url").value;
  }

  remove() {
    this.webview.remove();
    this.parentDocument
      .getElementById("action-go")
      .toggleAttribute("disabled", !tabs.length);
    this.parentDocument
      .getElementById("action-close")
      .toggleAttribute("disabled", !tabs.length);
  }

  goForward() {
    this.setupIcon({});
    this.webview.goForward();
  }

  goBack() {
    this.setupIcon({});
    this.webview.goBack();
  }

  getBgColor() {
    this.webview.getBackgroundColor().then(bgcolor => {
      log(`background color= ${bgcolor}`);
    });
  }

  getScreenshot() {
    log(`get screenshot`);
    this.webview.getScreenshot(100, 100, "image/jpeg").then(
      blob => {
        log(`got screenshot ${blob}`);
        let imageUrl = URL.createObjectURL(blob);
        this.parentDocument.querySelector("#tabs > .screenshot").src = imageUrl;
      },
      () => {
        log(`got screenshot failed`);
      }
    );
  }

  showPrompt() {
    let name = prompt("Please enter your name", "Default");
    console.log("showPrompt got name=" + name);
  }

  resizeWebView() {
    let rect = this.webview.getBoundingClientRect();
    this.webview.style.width = rect.width * 0.9 + "px";
    this.webview.style.height = rect.height * 0.9 + "px";
  }
}

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const startURL =
      "https://www.w3schools.com/jsref/tryit.asp?filename=tryjsref_win_open";
    this.go = function() {
      activatedTab.go();
    };

    this.close = function() {
      let closedTab = activatedTab;
      const index = tabs.indexOf(closedTab);
      if (index > -1) {
        tabs.splice(index, 1);
        closedTab.hide();
        if (tabs.length) {
          tabs[tabs.length - 1].show();
        }
        setTimeout(() => {
          closedTab.remove();
        }, 0);
      }
    };

    this.forward = function() {
      activatedTab.goForward();
    };

    this.back = function() {
      activatedTab.goBack();
    };

    let openBtn = document.getElementById("action-open");
    openBtn.addEventListener("click", () => {
      tabs.push(new BrowserTab(document, startURL, null));
    });

    this.getBgColor = function() {
      activatedTab.getBgColor();
    };

    this.getScreenshot = function() {
      activatedTab.getScreenshot();
    };

    this.showPrompt = function() {
      activatedTab.showPrompt();
    };

    this.resizeWebView = function() {
      activatedTab.resizeWebView();
    };

    // Binding Actions
    commands.forEach(id => {
      let btn = document.getElementById(id);
      btn.toggleAttribute("disabled", true);
      btn.addEventListener("click", this[btn.dataset.cmd]);
    });
  },
  { once: true }
);
