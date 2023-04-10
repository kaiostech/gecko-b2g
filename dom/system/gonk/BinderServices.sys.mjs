/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export var BinderServices = {};

XPCOMUtils.defineLazyGetter(BinderServices, "connectivity", function() {
  if (AppConstants.HAS_KOOST) {
    try {
      return Cc["@mozilla.org/b2g/connectivitybinderservice;1"].getService(
        Ci.nsIConnectivityBinderService
      );
    } catch (e) {}
  }
  // Sync from nsIConnectivityBinderService.idl.
  return {
    onCaptivePortalChanged(wifiState, usbState) {},
    onTetheringChanged(captivePortalLanding) {},
  };
});

XPCOMUtils.defineLazyGetter(BinderServices, "wifi", function() {
  if (AppConstants.HAS_KOOST) {
    try {
      return Cc["@mozilla.org/b2g/wifibinderservice;1"].getService(
        Ci.nsIWifiBinderService
      );
    } catch (e) {}
  }
  // Sync from nsIWifiBinderService.idl.
  return {
    onWifiStateChanged(state) {},
  };
});

XPCOMUtils.defineLazyGetter(BinderServices, "datacall", function() {
  if (AppConstants.HAS_KOOST) {
    try {
      return Cc["@mozilla.org/b2g/databinderservice;1"].getService(
        Ci.nsIDataBinderService
      );
    } catch (e) {}
  }
  // Sync from nsIDataBinderService.idl.
  return {
    onDefaultSlotIdChanged(id) {},
    onApnReady(id, types) {},
  };
});

XPCOMUtils.defineLazyServiceGetters(BinderServices, {});
