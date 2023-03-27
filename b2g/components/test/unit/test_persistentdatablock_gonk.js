/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ChromeUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyGetter(this, "libcutils", function() {
  ChromeUtils.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});

function run_test() {
  do_get_profile();
  ChromeUtils.import("resource://gre/modules/PersistentDataBlock.jsm");
  // We need to point to a valid partition for some of the tests. This is the /cache
  // partition in the emulator (x86-KitaKat).
  run_next_test();
}

// eslint-disable-next-line no-undef
_installTests();
