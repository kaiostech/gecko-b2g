/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { PromiseUtils } from "resource://gre/modules/PromiseUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const isGonk = AppConstants.platform === "gonk";
const hasRil = AppConstants.MOZ_B2G_RIL;

const lazy = {};

if (isGonk) {
  ChromeUtils.defineModuleGetter(
    lazy,
    "libcutils",
    "resource://gre/modules/systemlibs.js"
  );
}

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkManager",
  "@mozilla.org/network/manager;1",
  "nsINetworkManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRil",
  "@mozilla.org/ril;1",
  "nsIRadioInterfaceLayer"
);

const REQUEST_REJECT = 0;
const HTTP_CODE_OK = 200;
const HTTP_CODE_CREATED = 201;
const HTTP_CODE_REQUEST_TIMEOUT = 408;
const XHR_REQUEST_TIMEOUT = 60000;

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  return new ConsoleAPI({
    maxLogLevelPref: "toolkit.deviceUtils.loglevel",
    prefix: "DeviceUtils",
  });
});

const device_type_map = {
  default: 1000,
  feature_phone: 1000,
  phone: 2000,
  tablet: 3000,
  watch: 4000,
};

export const DeviceUtils = {
  device_info_cache: null,
  /**
   * Returns a Commercial Unit Reference which is vendor dependent.
   * For developer, a pref could be set from device config folder.
   */
  get cuRef() {
    let cuRefStr;

    try {
      cuRefStr =
        Services.prefs.getPrefType("device.commercial.ref") ==
        Ci.nsIPrefBranch.PREF_STRING
          ? Services.prefs.getCharPref("device.commercial.ref")
          : undefined;
    } catch (e) {
      lazy.console.error("get Commercial Unit Reference error=" + e);
    }
    return cuRefStr;
  },

  get iccInfo() {
    if (!hasRil) {
      return null;
    }
    let icc = lazy.gIccService.getIccByServiceId(0);
    let iccInfo =
      icc && icc.cardState !== Ci.nsIIcc.CARD_STATE_UNDETECTED && icc.iccInfo;
    if (!iccInfo && lazy.gMobileConnectionService.numItems > 1) {
      icc = lazy.gIccService.getIccByServiceId(1);
      iccInfo = icc && icc.iccInfo;
    }
    return iccInfo;
  },

  get imei() {
    if (!hasRil) {
      return null;
    }
    let mobile = lazy.gMobileConnectionService.getItemByServiceId(0);
    if (mobile && mobile.deviceIdentities) {
      return mobile.deviceIdentities.imei;
    }
    // return a dummy imei when failed to get from mobile connection.
    // TODO: it's not a good idea to return a fake one
    return "123456789012345";
  },

  get networkType() {
    if (
      !lazy.gNetworkManager.activeNetworkInfo ||
      lazy.gNetworkManager.activeNetworkInfo.type ===
        Ci.nsINetworkInfo.NETWORK_TYPE_UNKNOWN
    ) {
      return "unknown";
    }

    if (
      lazy.gNetworkManager.activeNetworkInfo.type ===
      Ci.nsINetworkInfo.NETWORK_TYPE_WIFI
    ) {
      return "wifi";
    }
    return "mobile";
  },

  getConn(id) {
    if (!hasRil) {
      return null;
    }
    let conn = lazy.gMobileConnectionService.getItemByServiceId(id);
    if (conn && conn.voice) {
      return conn.voice.network;
    }
    return null;
  },

  get networkMcc() {
    if (!hasRil) {
      return null;
    }
    let network = this.getConn(0);
    let netMcc = network && network.mcc;
    if (!netMcc && lazy.gMobileConnectionService.numItems > 1) {
      network = this.getConn(1);
      netMcc = network && network.mcc;
    }
    return !netMcc ? "" : netMcc;
  },

  get networkMnc() {
    if (!hasRil) {
      return null;
    }
    let network = this.getConn(0);
    let netMnc = network && network.mnc;
    if (!netMnc && lazy.gMobileConnectionService.numItems > 1) {
      network = this.getConn(1);
      netMnc = network && network.mnc;
    }
    return !netMnc ? "" : netMnc;
  },

  getDeviceId: function DeviceUtils_getDeviceId() {
    let deferred = PromiseUtils.defer();
    // TODO: need to check how to handle dual-SIM case.
    if (hasRil && typeof lazy.gMobileConnectionService != "undefined") {
      let conn = lazy.gMobileConnectionService.getItemByServiceId(0);
      conn.getIdentities({
        notifyGetDeviceIdentitiesRequestSuccess(aResult) {
          if (aResult.imei && parseInt(aResult.imei) !== 0) {
            deferred.resolve(aResult.imei);
          } else if (aResult.meid && parseInt(aResult.meid) !== 0) {
            deferred.resolve(aResult.meid);
          } else if (aResult.esn && parseInt(aResult.esn) !== 0) {
            deferred.resolve(aResult.esn);
          } else {
            deferred.reject();
          }
        },
      });
    } else {
      deferred.reject();
    }
    return deferred.promise;
  },

  getIccInfoArray: function DeviceUtils_getIccInfoArray() {
    if (typeof lazy.gRil === "undefined" || lazy.gIccService === "undefined") {
      return [];
    }

    let infoArray = [];
    for (let i = 0; i < lazy.gRil.numRadioInterfaces; i++) {
      let icc = lazy.gIccService.getIccByServiceId(i);
      if (icc && icc.iccInfo) {
        infoArray.push(icc.iccInfo);
      }
    }
    return infoArray;
  },

  getMobileNetworkInfoArray: function DeviceUtils_getMobileNetworkInfoArray() {
    if (typeof lazy.gMobileConnectionService === "undefined") {
      return [];
    }

    let infoArray = [];
    for (let i = 0; i < lazy.gMobileConnectionService.numItems; i++) {
      let conn = lazy.gMobileConnectionService.getItemByServiceId(i);
      if (conn && conn.voice && conn.voice.network) {
        infoArray.push(conn.voice.network);
      }
    }
    return infoArray;
  },

  getTDeviceObject: function DeviceUtils_getTDeviceObject() {
    let deferred = PromiseUtils.defer();

    // mobile network and language should be updated every time.
    let netInfoArray = this.getMobileNetworkInfoArray();
    let net_mcc = netInfoArray.length
      ? parseInt(netInfoArray[0].mcc, 10)
      : undefined;
    let net_mnc = netInfoArray.length
      ? parseInt(netInfoArray[0].mnc, 10)
      : undefined;
    let net_mcc2 =
      netInfoArray.length > 1 ? parseInt(netInfoArray[1].mcc, 10) : undefined;
    let net_mnc2 =
      netInfoArray.length > 1 ? parseInt(netInfoArray[1].mnc, 10) : undefined;
    let net_name = netInfoArray.length
      ? netInfoArray[0].shortName || netInfoArray[0].longName
      : undefined;
    let net_name2 =
      netInfoArray.length > 1
        ? netInfoArray[1].shortName || netInfoArray[1].longName
        : undefined;
    let language =
      Services.prefs.getPrefType("intl.locale.requested") ==
      Ci.nsIPrefBranch.PREF_STRING
        ? Services.prefs.getCharPref("intl.locale.requested")
        : undefined;

    if (this.device_info_cache) {
      this.device_info_cache.net_mcc = net_mcc;
      this.device_info_cache.net_mnc = net_mnc;
      this.device_info_cache.net_mcc2 = net_mcc2;
      this.device_info_cache.net_mnc2 = net_mnc2;
      // if 'spn' doesn't exist, take network name as SPN
      this.device_info_cache.icc_spn =
        this.device_info_cache.icc_spn || net_name;
      this.device_info_cache.icc_spn2 =
        this.device_info_cache.icc_spn2 || net_name2;
      this.device_info_cache.lang = language;
      deferred.resolve(this.device_info_cache);
    } else {
      this.getDeviceId().then(
        device_id => {
          if (isGonk) {
            let characteristics = lazy.libcutils.property_get(
              "ro.build.characteristics"
            );
            let device_type = characteristics
              .split(",")
              .map(function(item) {
                return item.trim();
              })
              .find(function(item) {
                return item in device_type_map;
              });
            if (!device_type) {
              device_type = "default";
            }
            let iccArray = this.getIccInfoArray();
            let device_info = {
              device_id,
              reference: this.cuRef,
              os:
                Services.prefs.getPrefType("b2g.osName") ==
                Ci.nsIPrefBranch.PREF_STRING
                  ? Services.prefs.getCharPref("b2g.osName")
                  : undefined,
              os_version:
                Services.prefs.getPrefType("b2g.version") ==
                Ci.nsIPrefBranch.PREF_STRING
                  ? Services.prefs.getCharPref("b2g.version")
                  : undefined,
              device_type: device_type_map[device_type],
              brand: lazy.libcutils.property_get("ro.product.brand"),
              model: lazy.libcutils.property_get("ro.product.name"),
              uuid:
                Services.prefs.getPrefType("app.update.custom") ==
                Ci.nsIPrefBranch.PREF_STRING
                  ? Services.prefs.getCharPref("app.update.custom")
                  : undefined,
              oem_sw_version:
                Services.prefs.getPrefType("oem.software.version") ==
                Ci.nsIPrefBranch.PREF_STRING
                  ? Services.prefs.getCharPref("oem.software.version")
                  : undefined,
              icc_mcc: iccArray.length
                ? parseInt(iccArray[0].mcc, 10)
                : undefined,
              icc_mnc: iccArray.length
                ? parseInt(iccArray[0].mnc, 10)
                : undefined,
              icc_spn: iccArray.length
                ? iccArray[0].spn || net_name
                : undefined,
              icc_mcc2:
                iccArray.length > 1 ? parseInt(iccArray[1].mcc, 10) : undefined,
              icc_mnc2:
                iccArray.length > 1 ? parseInt(iccArray[1].mnc, 10) : undefined,
              icc_spn2:
                iccArray.length > 1 ? iccArray[1].spn || net_name2 : undefined,
              net_mcc,
              net_mnc,
              net_mcc2,
              net_mnc2,
              lang: language,
              build_id: Services.appinfo.platformBuildID,
            };

            this.device_info_cache = device_info;
            deferred.resolve(device_info);
          } else {
            deferred.reject();
          }
        },
        error => {
          deferred.reject();
        }
      );
    }
    return deferred.promise;
  },

  /**
   * Fetch access token from Restricted Token Server
   *
   * @param url
   *        A Restricted Application Access URL including app_id
   *        like Push or LBS
   * @param apiKeyName
   *        An indicator for getting API key by urlFormatter
   *
   * @return Promise
   *        Returns a promise that resolves to a JSON object
   *        from server:
   *        {
   *          access_token: A JWT token for restricted access a service
   *          token_type: Token type. E.g. bearer
   *          scope: Assigned to client by restricted access token server
   *          expires_in: Time To Live of the current credential (in seconds)
   *        }
   *        Returns a promise that rejects to an error status
   */
  fetchAccessToken: function DeviceUtils_fetchAccessToken(url, apiKeyName) {
    if (
      typeof url !== "string" ||
      !url.length ||
      typeof apiKeyName !== "string" ||
      !apiKeyName.length
    ) {
      lazy.console.error("Invalid Input");
      return Promise.reject(REQUEST_REJECT);
    }

    let deferred = PromiseUtils.defer();
    this.getTDeviceObject().then(
      device_info => {
        let xhr = new XMLHttpRequest();
        try {
          xhr.open("POST", url, true);
        } catch (e) {
          deferred.reject(REQUEST_REJECT);
          return;
        }

        xhr.setRequestHeader("Content-Type", "application/json; charset=UTF-8");

        let authorizationKey;
        try {
          authorizationKey = AppConstants[apiKeyName];
        } catch (e) {
          deferred.reject(REQUEST_REJECT);
          return;
        }

        if (
          typeof authorizationKey !== "string" ||
          !authorizationKey.length ||
          authorizationKey == "no-kaios-api-key"
        ) {
          deferred.reject(REQUEST_REJECT);
          lazy.console.error("without API Key");
          return;
        }

        xhr.setRequestHeader("Authorization", "Key " + authorizationKey);

        xhr.responseType = "json";
        xhr.mozBackgroundRequest = true;
        xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS;
        // Prevent the request from reading from the cache.
        xhr.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
        // Prevent the request from writing to the cache.
        xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
        xhr.onerror = function() {
          if (xhr.status > 0) {
            deferred.reject(xhr.status);
          } else {
            // When status is 0 we don't have a valid channel
            deferred.reject(REQUEST_REJECT);
          }
          lazy.console.error(
            "An error occurred during the transaction, status: " + xhr.status
          );
        };

        xhr.timeout = XHR_REQUEST_TIMEOUT;
        xhr.ontimeout = function() {
          deferred.reject(HTTP_CODE_REQUEST_TIMEOUT);
          lazy.console.error("Fetch access token request timeout");
        };

        xhr.onload = function() {
          lazy.console.debug("Get access token returned status: " + xhr.status);
          // only accept status code 200 and 201.
          let isStatusInvalid =
            xhr.channel instanceof Ci.nsIHttpChannel &&
            xhr.status != HTTP_CODE_OK &&
            xhr.status != HTTP_CODE_CREATED;
          if (isStatusInvalid || !xhr.response || !xhr.response.access_token) {
            deferred.reject(xhr.status);
            lazy.console.debug("Response: " + JSON.stringify(xhr.response));
          } else {
            deferred.resolve(xhr.response);
          }
        };

        var requestData = JSON.stringify(device_info);
        xhr.send(requestData);
      },
      _ => {
        deferred.reject(REQUEST_REJECT);
        lazy.console.error("An error occurred during getting device info");
      }
    );

    return deferred.promise;
  },
};
