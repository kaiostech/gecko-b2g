/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Log } = ChromeUtils.import("chrome://remote/content/shared/Log.jsm");
XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.MARIONETTE)
);

this.EXPORTED_SYMBOLS = ["MarionetteHelper"];

class MarionetteHelper {
  constructor(contentBrowser) {
    // system app browser
    this.browser = contentBrowser;
    this.content = contentBrowser.contentWindow;
  }

  get tabs() {
    let web_views = Array.from(
      this.content.document.querySelectorAll("web-view")
    );
    // add system app
    web_views.push(this.browser);
    return web_views;
  }

  get selectedTab() {
    let web_views = Array.from(
      this.content.document.querySelectorAll("web-view")
    );
    let active = web_views.find(tab => {
      // logger.info(`${tab.src} active=${tab.active} visible=${tab.visible}`);
      return tab.active;
    });

    // Hack: select the first tab if none is marked as active.
    if (!active && web_views.length) {
      active = web_views[0];
    }

    return active;
  }

  set selectedTab(tab) {
    logger.info(`MarionetteHelper set selectedTab to ${tab}`);
    let current = this.selectedTab;
    current.active = false;
    tab.active = true;
    this.window.dispatchEvent(new Event("TabSelected"));
  }

  removeTab(tab) {
    logger.trace(`MarionetteHelper removeTab ${tab.src}\n`);
    // Never remove the system app.
    if (tab.src == Services.prefs.getCharPref("b2g.system_startup_url")) {
      return;
    }
    tab.remove();
    tab = null;
  }

  addEventListener(eventName, handler) {
    logger.info(`MarionetteHelper add event listener for ${eventName}`);
    this.content.addEventListener(...arguments);
  }

  removeEventListener(eventName, handler) {
    logger.info(`MarionetteHelper remove event listener for ${eventName}`);
    this.content.removeEventListener(...arguments);
  }
}

this.MarionetteHelper = MarionetteHelper;
