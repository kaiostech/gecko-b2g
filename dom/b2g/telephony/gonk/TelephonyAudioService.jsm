/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const nsIAudioManager = Ci.nsIAudioManager;
const nsITelephonyAudioService = Ci.nsITelephonyAudioService;

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const HEADSET_STATUS_CHANGED_TOPIC = "headphones-status-changed";
const kPrefRilTelephonyTtyMode = "ril.telephony.ttyMode";

const AUDIO_STATE_NAME = [
  "PHONE_STATE_NORMAL",
  "PHONE_STATE_RINGTONE",
  "PHONE_STATE_IN_CALL",
];

var DEBUG;
function debug(s) {
  dump("TelephonyAudioService: " + s + "\n");
}

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"].getService(
      nsIAudioManager
    );
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.

    // Fake nsIAudioManager implementation so that we can run the telephony
    // code in a non-Gonk build.
    if (DEBUG) {
      debug("Using fake audio manager.");
    }
    return {
      headsetState: nsITelephonyAudioService.HEADSET_STATE_UNKNOWN,
      microphoneMuted: false,
      masterVolume: 1.0,
      masterMuted: false,
      phoneState: -1,
      _forceForUse: {},

      setForceForUse(usage, force) {
        this._forceForUse[usage] = force;
      },

      getForceForUse(usage) {
        return this._forceForUse[usage] || 0;
      },
    };
  }
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gTelephonyMessenger",
  "@mozilla.org/ril/system-messenger-helper;1",
  "nsITelephonyMessenger"
);

function TelephonyAudioService() {
  this._listeners = [];
  this._updateDebugFlag();
  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, HEADSET_STATUS_CHANGED_TOPIC);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}
TelephonyAudioService.prototype = {
  classDescription: "TelephonyAudioService",
  classID: Components.ID("{c8605499-cfec-4cb5-a082-5f1f56d42adf}"),
  contractID: "@mozilla.org/telephony/audio-service;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsITelephonyAudioService,
    Ci.nsIObserver,
  ]),

  _shutdown() {
    Services.obs.removeObserver(this, HEADSET_STATUS_CHANGED_TOPIC);
    Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  },

  _updateDebugFlag() {
    try {
      DEBUG =
        RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  _listeners: null,
  _notifyAllListeners(aMethodName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (!this._listeners.includes(listener)) {
        // Listener has been unregistered in previous run.
        continue;
      }

      let handler = listener[aMethodName];
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        debug("listener for " + aMethodName + " threw an exception: " + e);
      }
    }
  },

  /**
   * nsITelephonyAudioService interface.
   */

  get hacMode() {
    return this._hacMode;
  },

  set hacMode(aEnabled) {
    if (aEnabled == this._hacMode) {
      return;
    }
    this._hacMode = aEnabled;
    lazy.gAudioManager.setHacMode(aEnabled);
    lazy.gTelephonyMessenger.notifyHacModeChanged(aEnabled);
  },

  get microphoneMuted() {
    return lazy.gAudioManager.microphoneMuted;
  },

  set microphoneMuted(aMuted) {
    if (aMuted == this.microphoneMuted) {
      return;
    }
    lazy.gAudioManager.microphoneMuted = aMuted;
  },

  get speakerEnabled() {
    let force = lazy.gAudioManager.getForceForUse(
      nsIAudioManager.USE_COMMUNICATION
    );
    return force == nsIAudioManager.FORCE_SPEAKER;
  },

  set speakerEnabled(aEnabled) {
    if (aEnabled == this.speakerEnabled) {
      return;
    }
    let force = aEnabled
      ? nsIAudioManager.FORCE_SPEAKER
      : nsIAudioManager.FORCE_NONE;
    lazy.gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);
  },

  get ttyMode() {
    if (this._ttyMode === undefined) {
      this._ttyMode = Services.prefs.getIntPref(
        kPrefRilTelephonyTtyMode,
        Ci.nsITelephonyService.TTY_MODE_OFF
      );
    }

    return this._ttyMode;
  },

  set ttyMode(aMode) {
    try {
      Services.prefs.setIntPref(kPrefRilTelephonyTtyMode, aMode);
      this._ttyMode = aMode;
    } catch (e) {
      if (DEBUG) {
        debug("Error setting preference: " + kPrefRilTelephonyTtyMode);
      }
    }
  },

  get headsetState() {
    return lazy.gAudioManager.headsetState;
  },

  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index < 0) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.splice(index, 1);
  },

  setPhoneState(aState) {
    switch (aState) {
      case nsITelephonyAudioService.PHONE_STATE_NORMAL:
        lazy.gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
        break;
      case nsITelephonyAudioService.PHONE_STATE_RINGTONE:
        lazy.gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
        break;
      case nsITelephonyAudioService.PHONE_STATE_IN_CALL:
        lazy.gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        if (this.speakerEnabled) {
          lazy.gAudioManager.setForceForUse(
            nsIAudioManager.USE_COMMUNICATION,
            nsIAudioManager.FORCE_SPEAKER
          );
        }
        break;
      default:
        throw new Error("Unknown audioPhoneState: " + aState);
    }

    if (DEBUG) {
      debug(
        "Put audio system into " +
          AUDIO_STATE_NAME[aState] +
          ": " +
          aState +
          ", result is: " +
          lazy.gAudioManager.phoneState
      );
    }
  },

  applyTtyMode(aMode) {
    lazy.gAudioManager.setTtyMode(aMode);
  },

  /**
   * nsIObserver interface.
   */

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          this._updateDebugFlag();
        }
        break;
      case HEADSET_STATUS_CHANGED_TOPIC: {
        let headsetState = {
          unknown: nsITelephonyAudioService.HEADSET_STATE_UNKNOWN,
          off: nsITelephonyAudioService.HEADSET_STATE_OFF,
          headset: nsITelephonyAudioService.HEADSET_STATE_HEADSET,
          headphone: nsITelephonyAudioService.HEADSET_STATE_HEADPHONE,
        }[aData];

        if (headsetState === undefined) {
          debug("Invalid headsetState: " + aData);
          break;
        }

        this._notifyAllListeners("notifyHeadsetStateChanged", [headsetState]);
        break;
      }
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this._shutdown();
        break;
    }
  },
};

var EXPORTED_SYMBOLS = ["TelephonyAudioService"];
