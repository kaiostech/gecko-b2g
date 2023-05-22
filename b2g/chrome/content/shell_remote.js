/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(aStr) {
  // console.log(`-*- ShellRemote.js: ${aStr}`);
}

var remoteShell = {
  get startURL() {
    let url = Services.prefs.getCharPref("b2g.multiscreen.system_remote_url");
    if (!url) {
      console.error(
        `Please set the b2g.multiscreen.system_remote_url preference properly`
      );
    }
    return url;
  },

  _started: false,
  hasStarted() {
    return this._started;
  },

  start() {
    this._started = true;

    let startURL = this.startURL;

    let systemAppFrame = document.createXULElement("browser");
    systemAppFrame.setAttribute("type", "chrome");
    systemAppFrame.setAttribute("id", "systemapp");
    systemAppFrame.setAttribute("nodefaultsrc", "true");

    document.body.appendChild(systemAppFrame);

    this.contentBrowser = systemAppFrame;

    window.addEventListener("sizemodechange", this);
    window.addEventListener("unload", this);

    debug(`Setting 2nd screen system url to ${startURL}`);

    this.contentBrowser.src = startURL;
  },

  stop() {
    window.removeEventListener("unload", this);
    window.removeEventListener("sizemodechange", this);
  },

  handleEvent(event) {
    debug(`event: ${event.type}`);

    // let content = this.contentBrowser.contentWindow;
    switch (event.type) {
      case "sizemodechange":
        // Due to bug 4657, the default behavior of video/audio playing from web
        // sites should be paused when this browser tab has sent back to
        // background or phone has flip closed.
        if (window.windowState == window.STATE_MINIMIZED) {
          this.contentBrowser.setVisible(false);
        } else {
          this.contentBrowser.setVisible(true);
        }
        break;
      case "unload":
        this.stop();
        break;
    }
  },
};

document.addEventListener(
  "DOMContentLoaded",
  () => {
    if (remoteShell.hasStarted()) {
      // Shoudl never happen!
      console.error("ShellRemote has already started but didn't initialize!!!");
      return;
    }

    remoteShell.start();
  },
  { once: true }
);
