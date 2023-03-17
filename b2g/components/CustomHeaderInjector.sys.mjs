/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "DeviceUtils",
  "resource://gre/modules/DeviceUtils.jsm"
);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const hasRil = AppConstants.MOZ_B2G_RIL;

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileConnectionService",
  "@mozilla.org/mobileconnection/mobileconnectionservice;1",
  "nsIMobileConnectionService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

const kTopicActiveNetwork = "network-active-changed";
const kTopicPrefChange = "nsPref:changed";
const kPrefDefaultServiceId = "dom.telephony.defaultServiceId";

const DEBUG = false;
function debug(msg) {
  dump("-* CustomHeaderInjector *- " + msg + "\n");
}

export const CustomHeaderInjector = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIMobileConnectionListener,
    Ci.nsIIccListener,
  ]),

  _customHeader: {},
  _deviceInfo: {},
  _serviceHosts: [],
  _defaultServiceId: null,
  init() {
    this._customHeader.name = Services.prefs.getCharPref(
      "network.http.customheader.name",
      ""
    );
    let hosts = Services.prefs.getCharPref(
      "network.http.customheader.hosts",
      ""
    );
    this._serviceHosts = hosts.split(",");

    DEBUG &&
      debug(`Hosts require injection: ${JSON.stringify(this._serviceHosts)}`);

    this._defaultServiceId = Services.prefs.getIntPref(
      kPrefDefaultServiceId,
      0
    );
    this.initCustomHeaderValue();

    Services.obs.addObserver(this, "http-on-modify-request");
    Services.obs.addObserver(this, kTopicActiveNetwork);
    Services.obs.addObserver(this, kTopicPrefChange);

    if (hasRil) {
      lazy.gIccService
        .getIccByServiceId(this._defaultServiceId)
        .registerListener(this);
      lazy.gMobileConnectionService
        .getItemByServiceId(this._defaultServiceId)
        .registerListener(this);
    }
  },

  buildCustomHeader() {
    this._customHeader.value = Services.prefs.getCharPref(
      "network.http.customheader.version",
      ""
    );
    [
      "device_uid",
      "com_ref",
      "sim_mnc",
      "sim_mcc",
      "net_mnc",
      "net_mcc",
      "net_type",
    ].forEach(prop => {
      this._customHeader.value += `;${this._deviceInfo[prop]}`;
    });
  },

  initCustomHeaderValue() {
    // Get SIM mnc and mcc.
    let iccInfo = lazy.DeviceUtils.iccInfo;
    this._deviceInfo.sim_mnc = iccInfo && iccInfo.mnc ? iccInfo.mnc : "";
    this._deviceInfo.sim_mcc = iccInfo && iccInfo.mcc ? iccInfo.mcc : "";

    // Get Network mnc and mcc.
    this._deviceInfo.net_mnc = lazy.DeviceUtils.networkMnc;
    this._deviceInfo.net_mcc = lazy.DeviceUtils.networkMcc;

    // Get Network connection type.
    this._deviceInfo.net_type = lazy.DeviceUtils.networkType;

    // Get commercial reference.
    let cuRef = lazy.DeviceUtils.cuRef;
    cuRef = !cuRef ? "" : cuRef.replace(/;/g, "\\u003B");
    this._deviceInfo.com_ref = cuRef;

    let promises = [];

    // Get Device Id.
    this._deviceInfo.device_uid = "";
    promises.push(
      lazy.DeviceUtils.getDeviceId().then(deviceid => {
        this._deviceInfo.device_uid = !deviceid
          ? ""
          : deviceid.replace(/;/g, "\\u003B");
      })
    );

    Promise.allSettled(promises).then(() => {
      this.buildCustomHeader();
    });
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "http-on-modify-request": {
        let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        const found = this._serviceHosts.find(host =>
          channel.URI.host.endsWith(host)
        );
        if (channel.URI.schemeIs("https") && found) {
          DEBUG && debug(`Inject custom header for connection...`);
          DEBUG && debug(` host: ${channel.URI.host}`);
          DEBUG && debug(` header value: ${this._customHeader.value}`);
          channel.setRequestHeader(
            this._customHeader.name,
            this._customHeader.value,
            false
          );
        }
        break;
      }
      case kTopicActiveNetwork: {
        this.updateCustomHeaderNetValue();
        break;
      }

      case kTopicPrefChange: {
        if (aData === kPrefDefaultServiceId) {
          this._defaultServiceId = Services.prefs.getIntPref(
            kPrefDefaultServiceId,
            0
          );
        }
        break;
      }
    }
  },

  updateCustomHeaderNetValue() {
    // Network mnc, network mcc, network connection type may varies.
    // Otherwise, no need to rebuild the custom header.
    // Get Network mnc and mcc.
    let netMnc = lazy.DeviceUtils.networkMnc;
    let netMcc = lazy.DeviceUtils.networkMcc;

    // Get Network connection type.
    let netType = lazy.DeviceUtils.networkType;

    if (
      netMnc !== this._deviceInfo.net_mnc ||
      netMcc !== this._deviceInfo.net_mcc ||
      netType !== this._deviceInfo.net_type
    ) {
      this._deviceInfo.net_mnc = netMnc;
      this._deviceInfo.net_mcc = netMcc;
      this._deviceInfo.net_type = netType;
      this.buildCustomHeader();
    }
  },

  updateCustomHeaderIccValue() {
    let iccInfo = lazy.DeviceUtils.iccInfo;
    // Get SIM mnc and mcc.
    let simMnc = iccInfo && iccInfo.mnc ? iccInfo.mnc : "";
    let simMcc = iccInfo && iccInfo.mcc ? iccInfo.mcc : "";
    if (
      this._deviceInfo.sim_mnc !== simMnc ||
      this._deviceInfo.sim_mcc !== simMcc
    ) {
      this._deviceInfo.sim_mnc = simMnc;
      this._deviceInfo.sim_mcc = simMcc;
      this.buildCustomHeader();
    }
  },

  // nsIMobileConnectionListener
  notifyVoiceChanged() {
    this.updateCustomHeaderNetValue();
  },
  notifyDataChanged() {},
  notifyDataError(_message) {},
  notifyCFStateChanged(
    _action,
    _reason,
    _number,
    _timeSeconds,
    _serviceClass
  ) {},
  notifyEmergencyCbModeChanged(_active, _timeoutMs) {},
  notifyOtaStatusChanged(_status) {},
  notifyRadioStateChanged() {},
  notifyClirModeChanged(_mode) {},
  notifyLastKnownNetworkChanged() {},
  notifyLastKnownHomeNetworkChanged() {},
  notifyNetworkSelectionModeChanged() {},
  notifyDeviceIdentitiesChanged() {},
  notifySignalStrengthChanged() {},
  notifyModemRestart(_reason) {},

  // nsIIccListener
  notifyStkCommand(_aStkProactiveCmd) {},
  notifyStkSessionEnd() {},
  notifyCardStateChanged() {
    // the trick here is that sim mcc/mnc is very likely available if card state get ready.
    // icc info change callback is triggerred frequently.
    this.updateCustomHeaderIccValue();
  },
  notifyIccInfoChanged() {},
  notifyIsimInfoChanged() {},
};

CustomHeaderInjector.init();
