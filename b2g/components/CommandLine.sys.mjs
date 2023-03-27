/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Small helper to expose nsICommandLine object to chrome code

export class B2gCommandlineHandler {
  constructor() {
    this.wrappedJSObject = this;
  }

  handle(cmdLine) {
    this.cmdLine = cmdLine;
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win && win.shell) {
      win.shell.handleCmdLine();
    }
  }

  helpInfo = "";
  QueryInterface = ChromeUtils.generateQI(["nsICommandLineHandler"]);
}
