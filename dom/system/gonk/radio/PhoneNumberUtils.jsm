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

const EXPORTED_SYMBOLS = ["PhoneNumberUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.import("resource://gre/modules/systemlibs.js");

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { libcutils } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "MCC_ISO3166_TABLE",
  "resource://gre/modules/mcc_iso3166_table.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;
function debug(s) {
  if (DEBUG) {
    dump("-*- PhoneNumberutils: " + s + "\n");
  }
}

//TODO: MIN_MATCH should support customization for different project/region.
const MIN_MATCH = 7;
const MAX_PHONE_NUMBER_LENGTH = 50;
const NON_DIALABLE_CHARS = /[^,#+\*\d]/g;
const NON_DIALABLE_CHARS_ONCE = new RegExp(NON_DIALABLE_CHARS.source);
const UNICODE_DIGITS = /[\uFF10-\uFF19\u0660-\u0669\u06F0-\u06F9]/g;
const VALID_ALPHA_PATTERN = /[a-zA-Z]/g;
const LEADING_PLUS_CHARS_PATTERN = /^[+\uFF0B]+/g;
const kPropPersistDeviceCountryCode = "persist.device.countrycode";
const kPropDefaultCountryCode = "ro.default.countrycode";

// Map letters to numbers according to the ITU E.161 standard
var E161 = {
  a: 2,
  b: 2,
  c: 2,
  d: 3,
  e: 3,
  f: 3,
  g: 4,
  h: 4,
  i: 4,
  j: 5,
  k: 5,
  l: 5,
  m: 6,
  n: 6,
  o: 6,
  p: 7,
  q: 7,
  r: 7,
  s: 7,
  t: 8,
  u: 8,
  v: 8,
  w: 9,
  x: 9,
  y: 9,
  z: 9,
};

function updateDebugFlag() {
  // Read debug setting from pref
  try {
    DEBUG =
      RIL_DEBUG.DEBUG_RIL ||
      Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
  } catch (e) {}
}
updateDebugFlag();

const PhoneNumberUtils = {
  normalize(aNumber, numbersOnly) {
    if (typeof aNumber !== "string") {
      return "";
    }

    aNumber = aNumber.replace(UNICODE_DIGITS, function(ch) {
      return String.fromCharCode(48 + (ch.charCodeAt(0) & 0xf));
    });
    if (!numbersOnly) {
      aNumber = aNumber.replace(VALID_ALPHA_PATTERN, function(ch) {
        return String(E161[ch.toLowerCase()] || 0);
      });
    }
    aNumber = aNumber.replace(LEADING_PLUS_CHARS_PATTERN, "+");
    aNumber = aNumber.replace(NON_DIALABLE_CHARS, "");
    return aNumber;
  },

  isPlainPhoneNumber(aNumber) {
    if (typeof aNumber !== "string") {
      return false;
    }

    var length = aNumber.length;
    var isTooLong = length > MAX_PHONE_NUMBER_LENGTH;
    var isEmpty = length === 0;
    return !(isTooLong || isEmpty || NON_DIALABLE_CHARS_ONCE.test(aNumber));
  },

  match(aNumber1, aNumber2) {
    if (typeof aNumber1 !== "string" || typeof aNumber2 !== "string") {
      return false;
    }

    let normalizedNumber1 = this.normalize(aNumber1);
    let normalizedNumber2 = this.normalize(aNumber2);

    if (normalizedNumber1 == normalizedNumber2) {
      return true;
    }

    let index1 = normalizedNumber1.length - 1;
    let index2 = normalizedNumber2.length - 1;
    let matched = 0;

    while (index1 >= 0 && index2 >= 0) {
      if (normalizedNumber1[index1] != normalizedNumber2[index2]) {
        break;
      }
      index1--;
      index2--;
      matched++;
    }

    if (matched >= MIN_MATCH) {
      return true;
    }
    return false;
  },

  //  1. See whether we have a network mcc
  //  2. If we don't have that, look for the simcard mcc
  //  3. If we don't have that or its 0 (not activated), pick up the last used mcc
  //  4. If we don't have, use oem default country code setting

  // countrycode for oem setting
  _defaultCountryCode: libcutils.property_get(kPropDefaultCountryCode, ""),

  defaultCountryCode(aClientId) {
    let countryCodeStrings = this._defaultCountryCode;
    let countryCodes = countryCodeStrings.split(",");
    return countryCodes[aClientId];
  },

  _persistCountryCode: libcutils.property_get(
    kPropPersistDeviceCountryCode,
    ""
  ),

  persistCountryCode(aClientId) {
    let countryCodeStrings = this._persistCountryCode;
    let countryCodes = countryCodeStrings.split(",");
    if (DEBUG) {
      debug(
        "persistCountryCode[" + aClientId + "]: " + countryCodes[aClientId]
      );
    }

    return countryCodes[aClientId] || null;
  },

  _storePersistCountryCode(aCountryName) {
    libcutils.property_set(kPropPersistDeviceCountryCode, aCountryName);
  },

  storePersistCountryCode(aClientId, aCountryName) {
    let countryCodeStrings = this._persistCountryCode;
    let countryCodes = countryCodeStrings.split(",");
    countryCodes[aClientId] = aCountryName;
    if (DEBUG) {
      debug("storePersistCountryCode: " + countryCodes.join());
    }

    this._storePersistCountryCode(countryCodes.join());
  },

  getCountryName(aClientId) {
    let mcc;
    let countryName;

    // Get network mcc
    let connection = lazy.gMobileConnectionService.getItemByServiceId(
      aClientId
    );
    let voice = connection && connection.voice;
    if (voice && voice.network && voice.network.mcc) {
      mcc = voice.network.mcc;
      if (DEBUG) {
        debug("Network MCC: " + mcc);
      }
    }

    // Get SIM mcc
    let icc = lazy.gIccService.getIccByServiceId(aClientId);
    let iccInfo = icc && icc.iccInfo;
    if (!mcc && iccInfo && iccInfo.mcc) {
      mcc = iccInfo.mcc;
      if (DEBUG) {
        debug("SIM MCC: " + mcc);
      }
    }

    // Attempt to grab last known sim mcc from prefs
    if (!mcc) {
      try {
        mcc = Services.prefs.getCharPref("ril.lastKnownSimMcc");
        if (DEBUG) {
          debug("Last known MCC: " + mcc);
        }
      } catch (e) {}
    }

    countryName = lazy.MCC_ISO3166_TABLE[mcc] || this.defaultCountryCode();

    if (DEBUG) {
      debug("MCC: " + mcc + " countryName: " + countryName);
    }
    return countryName;
  },

  updateCountryNameProperty(aClient) {
    let countryName = this.getCountryName(aClient);
    if (countryName !== this.persistCountryCode(aClient)) {
      this.storePersistCountryCode(aClient, countryName);
    }
  },
};
