/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;
function debug(s) {
  dump("-@- RILSettingsObserver: " + s + "\n");
}

function updateDebugFlag() {
  // Read debug setting from pref
  try {
    DEBUG =
      RIL_DEBUG.DEBUG_RIL ||
      Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
  } catch (e) {}
}
updateDebugFlag();

/**
 * RILSettingsObserver
 */
const RILSettingsObserver = function() {};
RILSettingsObserver.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISettingsObserver]),

  handleSettingChanged(aName, aResult) {
    if (DEBUG) {
      debug(aName + " is set to " + aResult);
    }
  },

  /**
   * nsISettingsObserver
   */
  observeSetting(aSettingInfo) {
    if (aSettingInfo) {
      let name = aSettingInfo.name;
      let result = JSON.parse(aSettingInfo.value);
      this.handleSettingChanged(name, result);
    }
  },

  getSettingWithDefault(aKey, aDefaultValue) {
    return new Promise(resolve => {
      lazy.gSettingsManager.get(aKey, {
        resolve: setting => {
          if (DEBUG) {
            debug("get setting " + setting.name + " value: " + setting.value);
          }
          resolve({ name: setting.name, value: JSON.parse(setting.value) });
        },
        reject: () => {
          if (DEBUG) {
            debug(
              "get setting " +
                aKey +
                "failed, use default value: " +
                aDefaultValue
            );
          }
          resolve({ name: aKey, value: aDefaultValue });
        },
      });
    });
  },

  /**
   * When the setting value change would be notify by the observe function.
   * @param aKey
   *        The name of setting
   */
  addSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug("add " + aKey + " setting observer.");
      }
      lazy.gSettingsManager.addObserver(aKey, this, {
        resolve: () => {
          if (DEBUG) {
            debug("observed " + aKey + " successed.");
          }
        },
        reject: () => {
          if (DEBUG) {
            debug("observed " + aKey + " failed.");
          }
        },
      });
    }
  },

  removeSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug("remove " + aKey + " setting observer.");
      }
      lazy.gSettingsManager.removeObserver(aKey, this, {
        resolve: () => {
          if (DEBUG) {
            debug("remove observer " + aKey + " successed.");
          }
        },
        reject: () => {
          if (DEBUG) {
            debug("remove observer " + aKey + " failed.");
          }
        },
      });
    }
  },
};

const EXPORTED_SYMBOLS = ["RILSettingsObserver"];
