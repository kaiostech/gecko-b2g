/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bootstrap loading of the homescreen.

document.addEventListener(
  "DOMContentLoaded",
  () => {
    let wm = document.querySelector("gaia-wm");
    wm.open_frame("resource://default-app-homescreen/index.html", {
      isHomescreen: true,
    });
  },
  { once: true }
);

(function() {
  // Grant "web-view" permission for default-app.
  Services.perms.addFromPrincipal(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "resource://default-app-browser/index.html"
    ),
    "web-view",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
})();
