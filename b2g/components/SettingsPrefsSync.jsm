/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const isGonk = AppConstants.platform === "gonk";

if (isGonk) {
  XPCOMUtils.defineLazyGetter(this, "libcutils", function() {
    const { libcutils } = ChromeUtils.import(
      "resource://gre/modules/systemlibs.js"
    );
    return libcutils;
  });
}

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

this.EXPORTED_SYMBOLS = ["SettingsPrefsSync"];

// Returns a settings.set(...) callback that just logs errors
// with a custom message.
function settingCallback(message) {
  return {
    resolve: () => {},
    reject: () => {
      console.error(message);
    },
  };
}

class ContentBlocking {
  constructor() {
    this.contentBlockingLevel = "disabled";
    this.switchingLevel = false;
    this.PREF_CB_LEVEL = "browser.contentblocking.level";
    this.PREF_FEATURES_PREF = "browser.contentblocking.features";

    this.LEVEL_PREFS = {
      strict: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
      },
      standard: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
      },
      disabled: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
      },
    };

    this._setPrefExpectationsAndUpdate();

    Services.prefs.addObserver(
      "browser.contentblocking.features.strict",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.addObserver(
      "browser.contentblocking.features.standard",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.addObserver(
      "browser.contentblocking.features.disabled",
      this._setPrefExpectationsAndUpdate
    );
  }

  log(msg) {
    console.log(`<ContentBlocking> ${msg}`);
  }

  updateCB(level = "standard") {
    this.log("updateCB:" + level);
    this.contentBlockingLevel = level;
    this._updateCBLevel(level);
  }

  get level() {
    return this.contentBlockingLevel;
  }

  clear() {
    Services.prefs.removeObserver(
      "browser.contentblocking.features.strict",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.removeObserver(
      "browser.contentblocking.features.standard",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.removeObserver(
      "browser.contentblocking.features.disabled",
      this._setPrefExpectationsAndUpdate
    );
  }

  _setPrefExpectationsAndUpdate() {
    let levels = ["strict", "standard", "disabled"];
    for (let level of levels) {
      this._setPrefExpectations(level);
    }
  }

  _setPrefExpectations(level) {
    let rulesArray = Services.prefs.getStringPref(
      this.PREF_FEATURES_PREF + "." + level,
      ""
    );
    if (!rulesArray) {
      return;
    }

    // The prefs inside LEVEL_PREFS are initial values.
    // If the pref remains null, then it will expect the default value.
    for (let pref in this.LEVEL_PREFS[level]) {
      this.LEVEL_PREFS[level][pref] = null;
    }

    rulesArray = rulesArray.split(",");
    for (let item of rulesArray) {
      switch (item) {
        case "tp":
          this.LEVEL_PREFS[level]["privacy.trackingprotection.enabled"] = true;
          break;
        case "-tp":
          this.LEVEL_PREFS[level]["privacy.trackingprotection.enabled"] = false;
          break;
        case "tpPrivate":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.pbmode.enabled"
          ] = true;
          break;
        case "-tpPrivate":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.pbmode.enabled"
          ] = false;
          break;
        case "fp":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = true;
          break;
        case "-fp":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = false;
          break;
        case "cm":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = true;
          break;
        case "-cm":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = false;
          break;
        case "stp":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = true;
          break;
        case "-stp":
          this.LEVEL_PREFS[level][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = false;
          break;
        case "lvl2":
          this.LEVEL_PREFS[level][
            "privacy.annotate_channels.strict_list.enabled"
          ] = true;
          break;
        case "-lvl2":
          this.LEVEL_PREFS[level][
            "privacy.annotate_channels.strict_list.enabled"
          ] = false;
          break;
        case "cookieBehavior0":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehavior1":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehavior2":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehavior3":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehavior4":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehavior5":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        case "cookieBehaviorPBM0":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehaviorPBM1":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehaviorPBM2":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehaviorPBM3":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehaviorPBM4":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehaviorPBM5":
          this.LEVEL_PREFS[level]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        default:
          Cu.reportError(`Error: Unknown rule observed ${item}`);
      }
    }
  }

  _updateCBLevel(level) {
    if (this.switchingLevel) {
      return;
    }
    // Turn on switchingLevel flag, to ensure that when the individual prefs that change as a result
    // of the level change do not trigger yet another level change.
    this.switchingLevel = true;
    this._setPrefsToLevel(level);
    this.switchingLevel = false;
  }

  /**
   * Sets all user-exposed content blocking preferences to values that match the selected level.
   */
  _setPrefsToLevel(level) {
    if (!["standard", "strict", "disabled"].includes(level)) {
      return;
    }

    for (let pref in this.LEVEL_PREFS[level]) {
      let value = this.LEVEL_PREFS[level][pref];
      if (Services.prefs.prefIsLocked(pref)) {
        continue;
      }

      if (value == null) {
        Services.prefs.clearUserPref(pref);
      } else {
        switch (Services.prefs.getPrefType(pref)) {
          case Services.prefs.PREF_BOOL:
            Services.prefs.setBoolPref(pref, value);
            break;
          case Services.prefs.PREF_INT:
            Services.prefs.setIntPref(pref, value);
            break;
          case Services.prefs.PREF_STRING:
            Services.prefs.setStringPref(pref, value);
            break;
        }
      }
    }
    Services.prefs.setStringPref(this.PREF_CB_LEVEL, level);
  }

  _dumpLevel(level) {
    this.log("_dumpLevel:" + level);
    for (let pref in this.LEVEL_PREFS[level]) {
      let value = this.LEVEL_PREFS[level][pref];
      this.log(pref + ":" + value);
    }
  }

  _dumpPrefs() {
    this.log("_dumpPrefs:");
    let prefs = [
      "network.cookie.cookieBehavior",
      "network.cookie.cookieBehavior.pbmode",
      "privacy.trackingprotection.pbmode.enabled",
      "privacy.trackingprotection.enabled",
      "privacy.trackingprotection.socialtracking.enabled",
      "privacy.trackingprotection.fingerprinting.enabled",
      "privacy.trackingprotection.cryptomining.enabled",
      "privacy.annotate_channels.strict_list.enabled",
    ];

    prefs.forEach(pref => {
      if (Services.prefs.prefIsLocked(pref)) {
        return;
      }
      let value;
      switch (Services.prefs.getPrefType(pref)) {
        case Services.prefs.PREF_BOOL:
          value = Services.prefs.getBoolPref(pref);
          break;
        case Services.prefs.PREF_INT:
          value = Services.prefs.getIntPref(pref);
          break;
        case Services.prefs.PREF_STRING:
          value = Services.prefs.setStringPref(pref);
          break;
      }
      this.log(pref + ":" + value);
    });
  }
}

const kSettingsToObserve = {
  "apz.overscroll.enabled": true,
  "browser.safebrowsing.enabled": true,
  "browser.safebrowsing.malware.enabled": true,
  "debug.fps.enabled": {
    prefName: "layers.acceleration.draw-fps",
    defaultValue: false,
  },
  "debug.paint-flashing.enabled": {
    prefName: "nglayout.debug.paint_flashing",
    defaultValue: false,
  },
  "device.storage.writable.name": "sdcard",
  "gfx.layerscope.enabled": false,
  "layers.composer2d.enabled": false,
  "layers.draw-borders": false,
  "layers.draw-tile-borders": false,
  "layers.dump": false,
  "layers.effect.invert": false,
  "layers.enable-tiles": true,
  "mms.debugging.enabled": false,
  "network.debugging.enabled": false,
  "privacy.donottrackheader.enabled": false,
  "privacy.trackingprotection.enabled": false,
  "ril.debugging.enabled": false,
  "ril.mms.requestReadReport.enabled": {
    prefName: "dom.mms.requestReadReport",
    defaultValue: true,
  },
  "ril.mms.requestStatusReport.enabled": {
    prefName: "dom.mms.requestStatusReport",
    defaultValue: false,
  },
  "ril.mms.retrieval_mode": {
    prefName: "dom.mms.retrieval_mode",
    defaultValue: "manual",
  },
  "ril.sms.requestStatusReport.enabled": {
    prefName: "dom.sms.requestStatusReport",
    defaultValue: false,
  },
  "ril.sms.strict7BitEncoding.enabled": {
    prefName: "dom.sms.strict7BitEncoding",
    defaultValue: false,
  },
  "ril.sms.maxReadAheadEntries": {
    prefName: "dom.sms.maxReadAheadEntries",
    defaultValue: 7,
  },
  "debug.touch-location.enabled": {
    prefName: "touch.show_location",
    defaultValue: false,
  },
  "ui.prefers.color-scheme": {
    prefName: "ui.systemUsesDarkTheme",
    defaultValue: 0,
    valueMap: { dark: 1 },
  },
  "ui.prefers.text-size": {
    prefName: "ui.prefersTextSizeId",
    defaultValue: 0,
    valueMap: { small: 1, large: 2 },
  },
  "voice-input.enabled": {
    resetToPref: true,
  },
  "voice-input.icon-url": {
    resetToPref: true,
  },
  "voice-input.supported-types": {
    resetToPref: true,
  },
};

this.SettingsPrefsSync = {
  // Initialize the synchronization and returns a promise that
  // resolves once we have done the early stage sync.
  start(aWindow) {
    // The full set of settings observers that were added, and will be
    // removed in the xpcom-shutdown phase.
    this.window = aWindow;
    this.settingsObservers = [];

    Services.obs.addObserver(this, "xpcom-shutdown");

    return new Promise(resolve => {
      this.getSettingWithDefault("language.current", "en-US").then(setting => {
        this.updateLanguage(setting.value, true);
        resolve();
      });
    });
  },

  // Update the setting "language.current" to Services.locale.requestedLocales and
  // "intl.accept_languages" preference.
  updateLanguage(value, forceSetPref) {
    // We only request a single locale, but need an array.
    Services.locale.requestedLocales = [value];

    let prefName = "intl.accept_languages";
    let intl = [];
    try {
      intl = Services.prefs
        .getComplexValue(prefName, Ci.nsIPrefLocalizedString)
        .data.split(",")
        .map(item => item.trim(" "));
    } catch (e) {}

    if (!intl.length) {
      Services.prefs.setCharPref(prefName, value);
    } else if (intl[0] != value) {
      // Insert value at the first position, and remove possible duplicated value.
      Services.prefs.setCharPref(
        prefName,
        [value, intl.filter(item => item != value)].join(", ")
      );
    } else if (forceSetPref) {
      // Bug 830782 - Homescreen is in English instead of selected locale after
      // the first run experience.
      // In order to ensure the current intl value is reflected on the child
      // process let's always write a user value, even if this one match the
      // current localized pref value.
      Services.prefs.setCharPref(prefName, intl.join(", "));
    }
  },

  // For nsIObserver
  observe(subject, topic, data) {
    if (topic !== "xpcom-shutdown") {
      console.error(
        `SettingsPrefsSync: unexpected observer notification '${topic}'`
      );
      return;
    }

    Services.obs.removeObserver(this, "xpcom-shutdown");

    this.settingsObservers.forEach(item => {
      gSettingsManager.removeObserver(
        item.key,
        item.observer,
        settingCallback(`Failed to add observer for ${item.key}`)
      );
    });

    if (this._contentBlocking) {
      this._contentBlocking.clear();
      this._contentBlocking = null;
    }
  },

  // Helper to add a setting observer and register it for later cleanup.
  addSettingsObserver(key, observer, message) {
    gSettingsManager.addObserver(key, observer, settingCallback(message));
    this.settingsObservers.push({ key, observer });
  },

  // Helper that resolves with the setting value or the default one, and
  // takes care of hiding the JSON stringification.
  getSettingWithDefault(name, defaultValue) {
    return new Promise(resolve => {
      gSettingsManager.get(name, {
        resolve: setting => {
          resolve({ name: setting.name, value: JSON.parse(setting.value) });
        },
        reject: () => {
          resolve({ name, value: defaultValue });
        },
      });
    });
  },

  // Will run all the steps that can be delayed after startup.
  // This module makes no decision about when that should run, letting
  // the caller decide since it can have better knowledge about the system state.
  delayedInit() {
    this.deviceInfoToSettings();
    this.setUpdateTrackingId();
    this.setupAccessibility();
    this.synchronizePrefs();
    this.setupLanguageSettingObserver();
    this.setupLowPrecisionSettings();
    this._contentBlocking = new ContentBlocking();
    this.setupContentBlocking();
  },

  // Returns a Commercial Unit Reference which is vendor dependent.
  // For developer, a pref could be set from device config folder.
  get cuRef() {
    let cuRefStr;

    try {
      cuRefStr =
        Services.prefs.getPrefType("device.commercial.ref") ==
        Ci.nsIPrefBranch.PREF_STRING
          ? Services.prefs.getCharPref("device.commercial.ref")
          : undefined;
      if (!cuRefStr) {
        // TODO: Remove this pref since it will be deprecated in master and v2.5. See Bug-35271.
        cuRefStr =
          Services.prefs.getPrefType("device.cuRef.default") ==
          Ci.nsIPrefBranch.PREF_STRING
            ? Services.prefs.getCharPref("device.cuRef.default")
            : undefined;
      }
    } catch (e) {
      console.error(`get Commercial Unit Reference error: ${e}`);
    }
    return cuRefStr;
  },

  deviceInfoToSettings() {
    // MOZ_B2G_VERSION is set in b2g/moz.configure, and is output as a #define value
    // from configure.in, defaults to 1.0.0 if this value is not exist.
    let os_version = AppConstants.MOZ_B2G_VERSION;
    let os_name = AppConstants.MOZ_B2G_OS_NAME;

    // Get the hardware info and firmware revision from device properties.
    let hardware_info = null;
    let firmware_revision = null;
    let product_manufacturer = null;
    let product_model = null;
    let product_device = null;
    let build_number = null;
    let sar_info = null;
    let version_tag = null;
    let base_version = null;
    let product_fota = null;
    let cuRefStr = null;
    let build_fingerprint = null;
    if (isGonk) {
      hardware_info =
        libcutils.property_get("ro.product.model.name") ||
        libcutils.property_get("ro.hardware");
      firmware_revision = libcutils.property_get("ro.firmware_revision");
      product_manufacturer = libcutils.property_get("ro.product.manufacturer");
      product_model = libcutils.property_get("ro.product.model");
      product_device = libcutils.property_get("ro.product.device");
      build_number = libcutils.property_get("ro.build.version.incremental");
      sar_info = libcutils.property_get("ro.product.sar_value", "0");
      version_tag = libcutils.property_get("ro.product.version_tag");
      base_version = libcutils.property_get("ro.product.base_version");
      product_fota = libcutils.property_get("ro.product.fota");
      cuRefStr = this.cuRef || null;
      build_fingerprint = libcutils.property_get("ro.build.fingerprint");
    }

    // Populate deviceinfo settings,
    // copying any existing deviceinfo.os into deviceinfo.previous_os
    this.getSettingWithDefault("deviceinfo.os", null).then(setting => {
      let previous_os = setting.value || "";
      let software = os_name + " " + os_version;
      let deviceInfo = {
        build_number,
        os: os_version,
        previous_os,
        software,
        platform_version: Services.appinfo.platformVersion,
        platform_build_id: Services.appinfo.platformBuildID,
        hardware: hardware_info,
        firmware_revision,
        product_manufacturer,
        product_model,
        product_device,
        sar_value: sar_info,
        software_tag: version_tag,
        base_version,
        product_fota,
        // cu means Commercial Unit Reference
        cu: cuRefStr,
        build_fingerprint,
      };

      let settingsArray = [];
      for (let name in deviceInfo) {
        settingsArray.push({
          name: `deviceinfo.${name}`,
          // Replace null/undefined values by the empty string.
          // Maybe it would be better to not set them at all?
          value: JSON.stringify(deviceInfo[name] || ""),
        });
      }
      gSettingsManager.set(
        settingsArray,
        settingCallback("Failure saving deviceinfo settings.")
      );
    });
  },

  setUpdateTrackingId() {
    try {
      let trackingId =
        Services.prefs.getPrefType("app.update.custom") ==
          Ci.nsIPrefBranch.PREF_STRING &&
        Services.prefs.getCharPref("app.update.custom");

      // If there is no previous registered tracking ID, we generate a new one.
      // This should only happen on first usage or after changing the
      // do-not-track value from disallow to allow.
      if (!trackingId) {
        trackingId = Services.uuid
          .generateUUID()
          .toString()
          .replace(/[{}]/g, "");
        Services.prefs.setCharPref("app.update.custom", trackingId);
        gSettingsManager.set(
          [{ name: "app.update.custom", value: JSON.stringify(trackingId) }],
          settingCallback("Failure saving app.update.custom setting.")
        );
      }
    } catch (e) {
      dump("Error getting tracking ID " + e + "\n");
    }
  },

  // Attach or detach AccessFu
  updateAccessFu(value) {
    let accessibilityScope = {};
    if (!("AccessFu" in accessibilityScope)) {
      ChromeUtils.import(
        "resource://gre/modules/accessibility/AccessFu.jsm",
        accessibilityScope
      );
    }

    if (value) {
      accessibilityScope.AccessFu.attach(this.window);
    } else {
      accessibilityScope.AccessFu.detach();
    }
  },

  // Observes the accessibility.screenreader setting
  setupAccessibility() {
    this.addSettingsObserver(
      "accessibility.screenreader",
      {
        observeSetting: info => {
          if (!info) {
            return;
          }
          let value = JSON.parse(info.value);
          this.updateAccessFu(value);
        },
      },
      "Failed to add a setting observer for accessibility.screenreader"
    );

    this.getSettingWithDefault("accessibility.screenreader", false).then(
      setting => {
        this.updateAccessFu(setting.value);
      }
    );
  },

  // Observes the contentblocking settings.
  setupContentBlocking() {
    this.addSettingsObserver(
      "browser.contentblocking.level",
      {
        observeSetting: info => {
          if (!info) {
            return;
          }
          let value = JSON.parse(info.value);
          this._contentBlocking.updateCB(value);
        },
      },
      "Failed to add a setting observer for browser.contentblocking.level"
    );

    this.getSettingWithDefault(
      "browser.contentblocking.level",
      "disabled"
    ).then(setting => {
      this._contentBlocking.updateCB(setting.value);
    });
  },

  // Synchronizes a set of preferences with matching settings.
  synchronizePrefs() {
    gSettingsManager.getBatch(Object.keys(kSettingsToObserve), {
      resolve: results => {
        let syncSettings = {};
        results.forEach(result => {
          const { name, value } = result;
          syncSettings[name] = value;
        });

        for (let key in kSettingsToObserve) {
          let setting = kSettingsToObserve[key];

          // Allow setting to contain flags redefining prefName and defaultValue.
          let prefName = setting.prefName || key;
          let defaultValue = setting.defaultValue;
          if (defaultValue === undefined) {
            defaultValue = setting;
          }

          let prefs = Services.prefs;
          // If requested, reset setting value and defaultValue to the pref value.
          if (setting.resetToPref) {
            switch (prefs.getPrefType(prefName)) {
              case Ci.nsIPrefBranch.PREF_BOOL:
                defaultValue = prefs.getBoolPref(prefName);
                break;

              case Ci.nsIPrefBranch.PREF_INT:
                defaultValue = prefs.getIntPref(prefName);
                break;

              case Ci.nsIPrefBranch.PREF_STRING:
                defaultValue = prefs.getCharPref(prefName);
                break;
            }

            let setting = { name: key };
            setting.value = JSON.stringify(defaultValue);
            gSettingsManager.set(
              [setting],
              settingCallback(`Failed to set setting ${key}`)
            );
          }

          // Figure out the right setter function for this type of pref.
          let setPref;
          switch (typeof defaultValue) {
            case "boolean":
              setPref = prefs.setBoolPref.bind(prefs);
              break;
            case "number":
              setPref = prefs.setIntPref.bind(prefs);
              break;
            case "string":
              setPref = prefs.setCharPref.bind(prefs);
              break;
          }

          // Map value of setting to of prefs.
          let mapValue;
          if (setting.hasOwnProperty("valueMap")) {
            mapValue = v =>
              setting.valueMap.hasOwnProperty(v)
                ? setting.valueMap[v]
                : defaultValue;
          } else {
            mapValue = v => v;
          }

          // Sync preference value from settings before add observer
          if (!setting.resetToPref && syncSettings.hasOwnProperty(key)) {
            let value = JSON.parse(syncSettings[key]);
            setPref(prefName, mapValue(value));
          }

          // Add an observer for this setting.
          this.addSettingsObserver(
            key,
            {
              observeSetting: info => {
                if (!info) {
                  return;
                }
                let value = JSON.parse(info.value);
                setPref(prefName, mapValue(value));
              },
            },
            `Failed to add observer for ${key}`
          );
        }
      },
      reject: error => {
        console.error(
          `SettingsPrefsSync: synchronizePrefs get settings failed:${JSON.stringify(
            error
          )}`
        );
      },
    });
  },

  // Setup setting observer for language
  setupLanguageSettingObserver() {
    this.addSettingsObserver(
      "language.current",
      {
        observeSetting: info => {
          if (!info) {
            return;
          }
          let value = JSON.parse(info.value);
          this.updateLanguage(value, false);
        },
      },
      "Failed to add observer for language.current"
    );
  },
  // Setup Low-precision buffer
  setupLowPrecisionSettings() {
    // The gaia setting layers.low-precision maps to two gecko prefs
    this.addSettingsObserver(
      "layers.low-precision",
      {
        observeSetting: info => {
          if (info !== null) {
            let value = JSON.parse(info.value);
            // Update gecko from the new Gaia setting
            Services.prefs.setBoolPref("layers.low-precision-buffer", value);
            Services.prefs.setBoolPref("layers.progressive-paint", value);
          } else {
            // Update gaia setting from gecko value
            try {
              let prefValue = Services.prefs.getBoolPref(
                "layers.low-precision-buffer"
              );
              let setting = [{ "layers.low-precision": prefValue }];
              gSettingsManager.set(
                setting,
                settingCallback("Failure saving low-precision-buffer settings.")
              );
            } catch (e) {
              dump("Unable to read pref layers.low-precision-buffer: " + e);
            }
          }
        },
      },
      "Failed to add observer for low precision buffer"
    );
    // The gaia setting layers.low-opacity maps to a string gecko pref (0.5/1.0)
    this.addSettingsObserver(
      "layers.low-opacity",
      {
        observeSetting: info => {
          if (info !== null) {
            let value = JSON.parse(info.value);
            // Update gecko from the new Gaia setting
            Services.prefs.setCharPref(
              "layers.low-precision-opacity",
              value ? "0.5" : "1.0"
            );
          } else {
            // Update gaia setting from gecko value
            try {
              let prefValue = Services.prefs.getCharPref(
                "layers.low-precision-opacity"
              );
              let setting = [{ "layers.low-opacity": prefValue == "0.5" }];
              gSettingsManager.set(
                setting,
                settingCallback(
                  "Failure saving low-precision-opacity settings."
                )
              );
            } catch (e) {
              dump("Unable to read pref layers.low-precision-opacity: " + e);
            }
          }
        },
      },
      "Failed to add observer for low precision opacity"
    );
  },
};
