/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function winlog(msg) {
  //   dump(`ext-windows.js: ${msg}\n`);
}

winlog(`loaded`);

this.windows = class extends ExtensionAPIPersistent {
  windowEventRegistrar(event, listener) {
    winlog(`windowEventRegistrar ${event}`);
    let { extension } = this;
    return ({ fire }) => {
      let listener2 = (window, ...args) => {
        if (extension.canAccessWindow(window)) {
          winlog(`firing ${event}`);
          listener(fire, window, ...args);
        }
      };

      windowTracker.addListener(event, listener2);
      return {
        unregister() {
          windowTracker.removeListener(event, listener2);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    };
  }

  PERSISTENT_EVENTS = {
    onCreated: this.windowEventRegistrar("domwindowopened", (fire, window) => {
      winlog(`domwindowopened`);
      fire.async(this.extension.windowManager.convert(window));
    }),
    onRemoved: this.windowEventRegistrar("domwindowclosed", (fire, window) => {
      winlog(`domwindowclosed`);
      fire.async(windowTracker.getId(window));
    }),
    onFocusChanged({ fire }) {
      let { extension } = this;
      // Keep track of the last windowId used to fire an onFocusChanged event
      let lastOnFocusChangedWindowId;

      let listener = event => {
        winlog(`onFocusChanged listener`);

        // Wait a tick to avoid firing a superfluous WINDOW_ID_NONE
        // event when switching focus between two Firefox windows.
        Promise.resolve().then(() => {
          let windowId = Window.WINDOW_ID_NONE;
          let window = Services.focus.activeWindow;
          if (window && extension.canAccessWindow(window)) {
            windowId = windowTracker.getId(window);
          }
          if (windowId !== lastOnFocusChangedWindowId) {
            fire.async(windowId);
            lastOnFocusChangedWindowId = windowId;
          }
        });
      };
      windowTracker.addListener("focus", listener);
      windowTracker.addListener("blur", listener);
      return {
        unregister() {
          windowTracker.removeListener("focus", listener);
          windowTracker.removeListener("blur", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    let { extension } = context;

    const { windowManager } = extension;

    return {
      windows: {
        onCreated: new EventManager({
          context,
          module: "windows",
          event: "onCreated",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "windows",
          event: "onRemoved",
          extensionApi: this,
        }).api(),

        onFocusChanged: new EventManager({
          context,
          module: "windows",
          event: "onFocusChanged",
          extensionApi: this,
        }).api(),

        get: function(windowId, getInfo) {
          winlog(`get windowId=${windowId} getInfo=${getInfo}`);
          let window = windowTracker.getWindow(windowId, context);
          if (!window || !context.canAccessWindow(window)) {
            return Promise.reject({
              message: `Invalid window ID: ${windowId}`,
            });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getCurrent: function(getInfo) {
          winlog(`getCurrent getInfo=${getInfo}`);

          let window = context.currentWindow || windowTracker.topWindow;
          if (!context.canAccessWindow(window)) {
            return Promise.reject({ message: `Invalid window` });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getLastFocused: function(getInfo) {
          winlog(`getLastFocused getInfo=${getInfo}`);

          let window = windowTracker.topWindow;
          if (!context.canAccessWindow(window)) {
            return Promise.reject({ message: `Invalid window` });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getAll: function(getInfo) {
          winlog(`getAll getInfo=${getInfo}`);

          let doNotCheckTypes =
            getInfo === null || getInfo.windowTypes === null;
          let windows = [];
          // incognito access is checked in getAll
          for (let win of windowManager.getAll()) {
            if (doNotCheckTypes || getInfo.windowTypes.includes(win.type)) {
              windows.push(win.convert(getInfo));
            }
          }
          return windows;
        },

        create: function(createData) {
          winlog("create");
          return Promise.reject();
        },

        update: function(windowId, updateInfo) {
          winlog("update");
          return Promise.reject();
        },

        remove: function(windowId) {
          winlog("remove");
          return Promise.reject();
        },
      },
    };
  }
};
