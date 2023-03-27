/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(aStr) {
  // dump("MultiscreenHandler: " + aStr + "\n");
}

// Multi-screen support on b2g. The following implementation will open a new
// top-level window once we receive a display connected event.
export const MultiscreenHandler = {
  topLevelWindows: new Map(),
  displays: [],

  init: function init() {
    Services.obs.addObserver(this, "display-changed");
    Services.obs.addObserver(this, "open-remote-shell-windows");
    Services.obs.addObserver(this, "xpcom-shutdown");
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "display-changed");
    Services.obs.removeObserver(this, "open-remote-shell-windows");
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "display-changed":
        this.handleDisplayChangeEvent(aSubject);
        break;
      case "open-remote-shell-windows":
        this.openTopLevelWindows();
        break;
      case "xpcom-shutdown":
        this.uninit();
        break;
    }
  },

  openTopLevelWindow: function openTopLevelWindow(aDisplay) {
    if (this.topLevelWindows.get(aDisplay.id)) {
      debug(
        "Top level window for display id: " + aDisplay.id + " has been opened."
      );
      return;
    }

    let flags =
      Services.prefs.getCharPref("toolkit.multiscreen.defaultChromeFeatures") +
      ",mozDisplayId=" +
      aDisplay.id;
    let remoteShellURL =
      Services.prefs.getCharPref("b2g.multiscreen.chrome_remote_url") +
      "#" +
      aDisplay.id;
    let win = Services.ww.openWindow(
      null,
      remoteShellURL,
      "myTopWindow" + aDisplay.id,
      flags,
      null
    );

    this.topLevelWindows.set(aDisplay.id, win);
  },

  openTopLevelWindows: function openTopLevelWindows() {
    this.displays.forEach(display => {
      this.openTopLevelWindow(display);
    });
  },

  closeTopLevelWindow: function closeTopLevelWindow(aDisplay) {
    let win = this.topLevelWindows.get(aDisplay.id);

    if (win) {
      win.close();
      this.topLevelWindows.delete(aDisplay.id);
    }
  },

  handleDisplayChangeEvent: function handleDisplayChangeEvent(aSubject) {
    let display = aSubject.QueryInterface(Ci.nsIDisplayInfo);

    if (display.connected) {
      this.displays.push(display);
    } else {
      this.closeTopLevelWindow(display);
    }
  },
};

MultiscreenHandler.init();
