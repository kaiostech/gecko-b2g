/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const EXPORTED_SYMBOLS = ["DeviceStateMonitor"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const GONK_DEVICESTATEMONITOR_CID = Components.ID(
  "{6143e4bb-07a1-4917-ba91-f321b1e0a575}"
);

const kNsPrefbranchPrefchangeTopic = "nsPref:changed";
const kXpcomShutdownChangedTopic = "xpcom-shutdown";
const kScreenStateChangedTopic = "screen-state-changed";
const kPowerSupplyChangeTopic = "gonkhal-powersupply-notifier";
const kTopics = [
  kScreenStateChangedTopic,
  kXpcomShutdownChangedTopic,
  kPowerSupplyChangeTopic,
];

const RadioIndicationFilterNone = 0;
const RadioIndicationFilterSignalStrength = 1 << 0;
const RadioIndicationFilterFullNetworkState = 1 << 1;
const RadioIndicationFilterDataCallDormancyChange = 1 << 2;
const RadioIndicationFilterAll =
  RadioIndicationFilterSignalStrength |
  RadioIndicationFilterFullNetworkState |
  RadioIndicationFilterDataCallDormancyChange;

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "gRadioInterfaces", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}

  let interfaces = [];
  for (let i = 0; i < ril.numRadioInterfaces; i++) {
    interfaces.push(ril.getRadioInterface(i));
  }
  return interfaces;
});

var DEBUG = RIL_DEBUG.DEBUG_RIL;
function debug(s) {
  dump("-*- DeviceStateMonitor: " + s + "\n");
}

function updateDebugFlag() {
  // Read debug setting from pref
  DEBUG =
    RIL_DEBUG.DEBUG_RIL ||
    Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, false);
}

function DeviceStateMonitor() {
  updateDebugFlag();
  if (DEBUG) {
    debug("Create DeviceStateMonitor");
  }
  this._isScreenOn = true;
  this._isCharging = false;
  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
  kTopics.forEach(topic => {
    Services.obs.addObserver(this, topic);
  });
}

DeviceStateMonitor.prototype = {
  classID: GONK_DEVICESTATEMONITOR_CID,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  _isScreenOn: null,
  _isCharging: null,

  _setRadioIndicationFilter(aFilter) {
    lazy.gRadioInterfaces.forEach(radioInterface => {
      radioInterface.sendWorkerMessage(
        "setUnsolResponseFilter",
        { filter: aFilter },
        null
      );
    });
  },

  _handleDeviceStateChange() {
    let filter =
      this._isScreenOn || this._isCharging
        ? RadioIndicationFilterAll
        : RadioIndicationFilterNone;
    this._setRadioIndicationFilter(filter);
  },

  _handleScreenStateChange(aScreenState) {
    if (DEBUG) {
      debug("Observe " + kScreenStateChangedTopic + ": " + aScreenState);
    }
    let screenOn = aScreenState === "on";
    if (screenOn != this._isScreenOn) {
      this._isScreenOn = screenOn;
      this._handleDeviceStateChange();
    }
  },

  _handleChargingStateChange(aPropbag) {
    let propbag = aPropbag.QueryInterface(Ci.nsIPropertyBag2);
    if (DEBUG) {
      debug(
        "Observe " +
          kPowerSupplyChangeTopic +
          ": " +
          propbag.get("powerSupplyOnline")
      );
    }
    let chargingState = propbag.get("powerSupplyOnline");
    if (chargingState != this._isCharging) {
      this._isCharging = chargingState;
      this._handleDeviceStateChange();
    }
  },

  _shutdown() {
    if (DEBUG) {
      debug("shutdown DeviceStateMonitor");
    }
    Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
    kTopics.forEach(topic => {
      Services.obs.removeObserver(this, topic);
    });
  },

  /**
   * nsIObserver interface.
   */
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case kNsPrefbranchPrefchangeTopic:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
      case kScreenStateChangedTopic:
        this._handleScreenStateChange(aData);
        break;
      case kXpcomShutdownChangedTopic:
        this._shutdown();
        break;
      case kPowerSupplyChangeTopic:
        this._handleChargingStateChange(aSubject);
        break;
    }
  },
};
