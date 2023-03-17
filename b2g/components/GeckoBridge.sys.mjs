/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  Preferences listed in kWatchedPrefs can be accessed by sidl.
  Please put the preferences in alphabetical order.
  When adding a new preference name, please follow the naming conventions:
  - Use dot "." to hierarchically distinguish different types of groups.
  - Use hyphen "-" to join two or more words together into a compound term.
  If the preference is already in upstream, then just follow upstream.
*/
const kWatchedPrefs = [
  "app.update.custom",
  "b2g.ril.enabled",
  "b2g.version",
  "b2g.volume.state",
  "device.bt",
  "device.cdma-apn",
  "device.commercial.ref",
  "device.dfc",
  "device.dual-lte",
  "device.flip",
  "device.fm.recorder",
  "device.gps",
  "device.group-message",
  "device.kaiaccount.enabled",
  "device.key.camera",
  "device.key.endcall",
  "device.key.volume",
  "device.mvs",
  "device.parental-control",
  "device.qwerty",
  "device.readout",
  "device.rsu",
  "device.rtt",
  "device.sim-hotswap",
  "device.storage.size",
  "device.tethering",
  "device.torch",
  "device.vilte",
  "device.volte",
  "device.vowifi",
  "device.wifi",
  "device.wifi.certified",
  "oem.software.version",
  "ril.support.primarysim.switch",
];

function log(msg) {
  dump(`GeckoBridge: ${msg}\n`);
}

export const GeckoBridge = {
  start() {
    log(`Starting`);
    try {
      this._bridge = Cc["@mozilla.org/sidl-native/bridge;1"].getService(
        Ci.nsIGeckoBridge
      );
    } catch (e) {
      log(
        `Failed to instanciate "@mozilla.org/sidl-native/bridge;1" service: ${e}`
      );
      return;
    }

    // A dynamic list to observe prefs for api-daemon.
    this._prefs = [];

    // Any of these could fail, but we don't want to stop at the first failure.
    [
      {
        prop: "_appsServiceDelegate",
        xpcom: "appsservice",
        interface: Ci.nsIAppsServiceDelegate,
      },
      {
        prop: "_powerManagerDelegate",
        xpcom: "powermanager",
        interface: Ci.nsIPowerManagerDelegate,
      },
      {
        prop: "_preferenceDelegate",
        xpcom: "preference",
        interface: Ci.nsIPreferenceDelegate,
      },

      {
        prop: "_networkManagerDelegate",
        xpcom: "networkmanager",
        interface: Ci.nsINetworkManagerDelegate,
      },
      {
        prop: "_mobileManagerDelegate",
        xpcom: "mobilemanager",
        interface: Ci.nsIMobileManagerDelegate,
      },
    ].forEach(delegate => {
      let contract_id = `@mozilla.org/sidl-native/${delegate.xpcom};1`;
      try {
        this[delegate.prop] = Cc[contract_id].getService(delegate.interface);
      } catch (e) {
        log(`Failed to create '${contract_id}' delegate: ${e}`);
      }
    });

    this.setup();
  },

  setup() {
    if (!this._bridge) {
      log(`Can't do any setup without a nsIGeckoBridge instance!`);
      return;
    }

    // Add a pref change listener for each of the watched prefs and sync the
    // initial value.
    kWatchedPrefs.forEach(pref => {
      Services.prefs.addObserver(pref, this);
      this.setPref(pref);
    });

    this.setAppsServiceDelegate();
    this.setPowerManagerDelegate();
    this.setPreferenceDelegate();
    this.setMobileManagerDelegate();
    this.setNetworkManagerDelegate();

    Services.obs.addObserver(this, "api-daemon-reconnected");
    Services.obs.addObserver(this, "preference-delegate-notify");
  },

  observe(subject, topic, data) {
    log(`received observer notification: ${topic}`);

    if (topic == "api-daemon-reconnected") {
      // When reconnecting the daemon, we need to send the pref list
      // again since it's not persisted.
      kWatchedPrefs.forEach(pref => {
        this.setPref(pref);
      });
      this._prefs.forEach(pref => {
        this.setPref(pref);
      });
    } else if (topic == "preference-delegate-notify") {
      if (!kWatchedPrefs.includes(data) && !this._prefs.includes(data)) {
        this._prefs.push(data);
        Services.prefs.addObserver(data, this);
      }
    } else {
      this.setPref(data);
    }
  },

  generateLoggingCallback(delegate) {
    return {
      resolve() {
        log(`Success setting ${delegate}`);
      },
      reject() {
        log(`Failure setting ${delegate}`);
      },
    };
  },

  setAppsServiceDelegate() {
    if (!this._appsServiceDelegate) {
      log(`Invalid appsservice delegate`);
      return;
    }

    log(`Setting AppsServiceDelegate`);
    this._bridge.setAppsServiceDelegate(
      this._appsServiceDelegate,
      this.generateLoggingCallback("AppsServiceDelegate")
    );
  },

  setMobileManagerDelegate() {
    if (!this._mobileManagerDelegate) {
      log(`Invalid mobilemanager delegate`);
      return;
    }

    log(`Setting MobileManagerDelegate`);
    this._bridge.setMobileManagerDelegate(
      this._mobileManagerDelegate,
      this.generateLoggingCallback("MobileManagerDelegate")
    );
  },

  setNetworkManagerDelegate() {
    if (!this._networkManagerDelegate) {
      log(`Invalid networkmanager delegate`);
      return;
    }

    log(`Setting NetworkManagerDelegate`);
    this._bridge.setNetworkManagerDelegate(
      this._networkManagerDelegate,
      this.generateLoggingCallback("NetworkManagerDelegate")
    );
  },

  setPowerManagerDelegate() {
    if (!this._powerManagerDelegate) {
      log(`Invalid powermanager delegate`);
      return;
    }

    log(`Setting PowerManagerDelegate`);
    this._bridge.setPowerManagerDelegate(
      this._powerManagerDelegate,
      this.generateLoggingCallback("PowerManagerDelegate")
    );
  },

  setPreferenceDelegate() {
    if (!this._preferenceDelegate) {
      log(`Invalid preference delegate`);
      return;
    }

    log(`Setting PreferenceDelegate`);
    this._bridge.setPreferenceDelegate(
      this._preferenceDelegate,
      this.generateLoggingCallback("PreferenceDelegate")
    );
  },

  setPref(pref) {
    let prefs = Services.prefs;
    let kind = prefs.getPrefType(pref);
    log(`Setting pref ${pref} (${kind})`);

    const callback = {
      resolve() {},
      reject() {
        log(`Failed to set pref ${pref}`);
      },
    };

    switch (kind) {
      case Ci.nsIPrefBranch.PREF_STRING:
        this._bridge.charPrefChanged(pref, prefs.getCharPref(pref), callback);
        break;
      case Ci.nsIPrefBranch.PREF_INT:
        this._bridge.intPrefChanged(pref, prefs.getIntPref(pref), callback);
        break;
      case Ci.nsIPrefBranch.PREF_BOOL:
        this._bridge.boolPrefChanged(pref, prefs.getBoolPref(pref), callback);
        break;
      default:
        log(`Unexpected pref type (${kind}) for ${pref}`);
    }
  },
};
