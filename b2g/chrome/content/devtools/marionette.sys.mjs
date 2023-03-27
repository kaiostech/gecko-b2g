/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

export class MarionetteHelper {
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
      // lazy.logger.info(`${tab.src} active=${tab.active} visible=${tab.visible}`);
      return tab.active;
    });

    // Hack: select the first tab if none is marked as active.
    if (!active && web_views.length) {
      active = web_views[0];
    }

    return active;
  }

  set selectedTab(tab) {
    lazy.logger.info(`MarionetteHelper set selectedTab to ${tab}`);
    let current = this.selectedTab;
    current.active = false;
    tab.active = true;
    this.window.dispatchEvent(new Event("TabSelected"));
  }

  removeTab(tab) {
    lazy.logger.trace(`MarionetteHelper removeTab ${tab.src}\n`);
    // Never remove the system app.
    if (tab.src == Services.prefs.getCharPref("b2g.system_startup_url")) {
      return;
    }
    tab.remove();
    tab = null;
  }

  addEventListener(eventName, handler) {
    lazy.logger.info(`MarionetteHelper add event listener for ${eventName}`);
    this.content.addEventListener(...arguments);
  }

  removeEventListener(eventName, handler) {
    lazy.logger.info(`MarionetteHelper remove event listener for ${eventName}`);
    this.content.removeEventListener(...arguments);
  }
}
