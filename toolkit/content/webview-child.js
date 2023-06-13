/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.defineESModuleGetters(this, {
  WebViewChild: "resource://gre/modules/WebViewChild.sys.mjs",
});

// Initialize the <web-view> specific support.
this.webViewChild = new WebViewChild();
this.webViewChild.init(this);
