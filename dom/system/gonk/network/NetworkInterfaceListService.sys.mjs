/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const NETWORKLISTSERVICE_CID = Components.ID(
  "{3780be6e-7012-4e53-ade6-15212fb88a0d}"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "cpmm", () => {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
});

export function NetworkInterfaceListService() {}

NetworkInterfaceListService.prototype = {
  classID: NETWORKLISTSERVICE_CID,

  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInterfaceListService]),

  getDataInterfaceList(aConditions) {
    return new NetworkInterfaceList(
      lazy.cpmm.sendSyncMessage("NetworkInterfaceList:ListInterface", {
        excludeSupl:
          (aConditions &
            Ci.nsINetworkInterfaceListService
              .LIST_NOT_INCLUDE_SUPL_INTERFACES) !=
          0,
        excludeMms:
          (aConditions &
            Ci.nsINetworkInterfaceListService
              .LIST_NOT_INCLUDE_MMS_INTERFACES) !=
          0,
        excludeIms:
          (aConditions &
            Ci.nsINetworkInterfaceListService
              .LIST_NOT_INCLUDE_IMS_INTERFACES) !=
          0,
        excludeDun:
          (aConditions &
            Ci.nsINetworkInterfaceListService
              .LIST_NOT_INCLUDE_DUN_INTERFACES) !=
          0,
        excludeFota:
          (aConditions &
            Ci.nsINetworkInterfaceListService
              .LIST_NOT_INCLUDE_FOTA_INTERFACES) !=
          0,
      })[0]
    );
  },
};

function FakeNetworkInfo(aAttributes) {
  this.state = aAttributes.state;
  this.type = aAttributes.type;
  this.name = aAttributes.name;
  this.ips = aAttributes.ips;
  this.netId = aAttributes.netId;
  this.prefixLengths = aAttributes.prefixLengths;
  this.gateways = aAttributes.gateways;
  this.dnses = aAttributes.dnses;
}
FakeNetworkInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInfo]),

  getAddresses(ips, prefixLengths) {
    ips.value = this.ips.slice();
    prefixLengths.value = this.prefixLengths.slice();

    return this.ips.length;
  },

  getGateways(count) {
    if (count) {
      count.value = this.gateways.length;
    }
    return this.gateways.slice();
  },

  getDnses(count) {
    if (count) {
      count.value = this.dnses.length;
    }
    return this.dnses.slice();
  },
};

function NetworkInterfaceList(aInterfaceLiterals) {
  this._interfaces = [];
  for (let entry of aInterfaceLiterals) {
    this._interfaces.push(new FakeNetworkInfo(entry));
  }
}

NetworkInterfaceList.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInterfaceList]),

  getNumberOfInterface() {
    return this._interfaces.length;
  },

  getInterfaceInfo(index) {
    if (!this._interfaces) {
      return null;
    }
    return this._interfaces[index];
  },
};
