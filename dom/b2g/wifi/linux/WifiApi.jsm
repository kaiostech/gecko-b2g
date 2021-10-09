/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WifiApi"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const kFinalUiStartUpTopic = "final-ui-startup";

function log(msg) {
  console.log(`WifiApiLinux: ${msg}`);
}

class WifiApi {
  constructor() {
    log(`constructor`);

    // All the messages sent by the content side from DOMWifiManager
    const messages = [
      "WifiManager:getNetworks",
      "WifiManager:getKnownNetworks",
      "WifiManager:associate",
      "WifiManager:forget",
      "WifiManager:wps",
      "WifiManager:getState",
      "WifiManager:setPowerSavingMode",
      "WifiManager:setHttpProxy",
      "WifiManager:setStaticIpMode",
      "WifiManager:importCert",
      "WifiManager:getImportedCerts",
      "WifiManager:deleteCert",
      "WifiManager:setPasspointConfig",
      "WifiManager:getPasspointConfigs",
      "WifiManager:removePasspointConfig",
      "WifiManager:setWifiEnabled",
      "WifiManager:setWifiTethering",
      "WifiManager:getSoftapStations",
      "WifiManager:setOpenNetworkNotification",
      "child-process-shutdown",
    ];

    messages.forEach(msgName => {
      Services.ppmm.addMessageListener(msgName, this);
    });

    Services.obs.addObserver(this, kFinalUiStartUpTopic);
  }

  receiveMessage(message) {
    log(`receiveMessage ${message.name}`);

    switch (message.name) {
      case "WifiManager:getState":
        return {
          // TODO: get real value from the network manager.
          enabled: true,
        };
        break;
    }
  }

  observe(subject, topic, data) {
    log(`observe ${topic}`);
  }

  get classID() {
    return "{e3feab32-322d-4e29-a754-9d06af7c8996}";
  }

  get contractID() {
    return "@mozilla.org/wifi/linux;1";
  }

  QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIObserver]);
  }
}
