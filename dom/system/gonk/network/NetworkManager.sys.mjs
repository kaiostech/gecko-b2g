/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { PromiseUtils } from "resource://gre/modules/PromiseUtils.sys.mjs";
import { BinderServices } from "resource://gre/modules/BinderServices.sys.mjs";

const { libcutils } = ChromeUtils.import(
  "resource://gre/modules/systemlibs.js"
);

const NETWORKMANAGER_CID = Components.ID(
  "{1ba9346b-53b5-4660-9dc6-58f0b258d0a6}"
);

const DEFAULT_PREFERRED_NETWORK_TYPE = Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET;

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkService",
  "@mozilla.org/network/service;1",
  "nsINetworkService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gTetheringService",
  "@mozilla.org/tethering/service;1",
  "nsITetheringService"
);

const TOPIC_INTERFACE_REGISTERED = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED = "network-interface-unregistered";
const TOPIC_ACTIVE_CHANGED = "network-active-changed";
const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";
const PREF_MANAGE_OFFLINE_STATUS = "network.gonk.manage-offline-status";
const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";
const TOPIC_NETD_INTERFACE_CHANGE = "netd-interface-change";
const TOPIC_NETD_INTERFACE_REMOVE = "netd-interface-remove";
const TOPIC_NETD_NET64_PREFIX_EVENT = "on-nat64prefix-event";

const IPV4_ADDRESS_ANY = "0.0.0.0";
const IPV6_ADDRESS_ANY = "::0";

const IPV4_MAX_PREFIX_LENGTH = 32;
const IPV6_MAX_PREFIX_LENGTH = 128;

// Connection Type for Network Information API
/* eslint-disable no-unused-vars */
const CONNECTION_TYPE_CELLULAR = 0;
const CONNECTION_TYPE_BLUETOOTH = 1;
const CONNECTION_TYPE_ETHERNET = 2;
const CONNECTION_TYPE_WIFI = 3;
const CONNECTION_TYPE_OTHER = 4;
const CONNECTION_TYPE_NONE = 5;
const CONNECTION_TYPE_UNKNOWN = 6;
/* eslint-enable no-unused-vars */

const MANUAL_PROXY_CONFIGURATION =
  Ci.nsIProtocolProxyService.PROXYCONFIG_MANUAL;

const CLAT_PREFIX = "v4-";

var DEBUG = false;
function updateDebug() {
  try {
    DEBUG = DEBUG || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}
}

function debug(s) {
  if (DEBUG) {
    console.log("-*- NetworkManager: ", s, "\n");
  }
}
updateDebug();

function defineLazyRegExp(obj, name, pattern) {
  obj.__defineGetter__(name, function() {
    delete obj[name];
    return (obj[name] = new RegExp(pattern));
  });
}

function convertToDataCallType(aNetworkType) {
  switch (aNetworkType) {
    case Ci.nsINetworkInfo.NETWORK_TYPE_WIFI:
      return "wifi";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE:
      return "default";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS:
      return "mms";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL:
      return "supl";
    case Ci.nsINetworkInfo.NETWORK_TYPE_WIFI_P2P:
      return "wifip2p";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS:
      return "ims";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN:
      return "dun";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA:
      return "fota";
    case Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET:
      return "ethernet";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_HIPRI:
      return "hipri";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_CBS:
      return "cbs";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IA:
      return "ia";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_ECC:
      return "Emergency";
    case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_XCAP:
      return "xcap";
    default:
      return "unknown";
  }
}

function ExtraNetworkInterface(aNetwork) {
  this.httpproxyhost = aNetwork.httpProxyHost;
  this.httpproxyport = aNetwork.httpProxyPort;
  this.mtuValue = aNetwork.mtu;
  this.info = new ExtraNetworkInfo(aNetwork.info);
}
ExtraNetworkInterface.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsINetworkInterface]),

  get httpProxyHost() {
    return this.httpproxyhost;
  },

  get httpProxyPort() {
    return this.httpproxyport;
  },

  get mtu() {
    return this.mtuValue;
  },
};

function StackedLinkInfo(aStackedLinkInfo) {
  let ips = {};
  let prefixLengths = {};
  this.name = null;
  this.dnses = [];
  this.gateways = [];
  this.ips_length = 0;

  if (aStackedLinkInfo) {
    this.name = aStackedLinkInfo.name;
    this.ips_length = aStackedLinkInfo.getAddresses(ips, prefixLengths);
    this.ips = ips.value;
    this.prefixLengths = prefixLengths.value;
    this.dnses = aStackedLinkInfo.getDnses();
    this.gateways = aStackedLinkInfo.getGateways();
  }
}
StackedLinkInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIStackedLinkInfo]),
  name: null,

  ips: [],

  prefixLengths: [],

  dnses: [],

  gateways: [],

  ips_length: null,

  getAddresses(aIps, aPrefixLengths) {
    aIps.value = this.ips.slice();
    aPrefixLengths.value = this.prefixLengths.slice();

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
function ExtraNetworkInfo(aNetworkInfo) {
  let ips_temp = {};
  let prefixLengths_temp = {};

  this.state = aNetworkInfo.state;
  this.type = aNetworkInfo.type;
  this.name = aNetworkInfo.name;
  this.tcpbuffersizes = aNetworkInfo.tcpbuffersizes;
  this.ips_length = aNetworkInfo.getAddresses(ips_temp, prefixLengths_temp);
  this.ips = ips_temp.value;
  this.prefixLengths = prefixLengths_temp.value;
  this.gateways = aNetworkInfo.getGateways();
  this.dnses = aNetworkInfo.getDnses();
  this.serviceId = aNetworkInfo.serviceId;
  this.iccId = aNetworkInfo.iccId;
  if (this.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS) {
    this.mmsc = aNetworkInfo.mmsc;
    this.mmsProxy = aNetworkInfo.mmsProxy;
    this.mmsPort = aNetworkInfo.mmsPort;
  }
  if (this.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS) {
    this.pcscf = aNetworkInfo.getPcscf();
  }
  this.netId = aNetworkInfo.netId;
  this.meter = aNetworkInfo.meter;
  //For Clat only
  this.stackedLinkInfo = new StackedLinkInfo(aNetworkInfo.stackedLinkInfo);
}
ExtraNetworkInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINetworkInfo,
    Ci.nsIRilNetworkInfo,
    Ci.nsINat464XlatInfo,
  ]),

  /**
   * nsINetworkInfo Implementation
   */
  getAddresses(aIps, aPrefixLengths) {
    // Combine the stackedLinkInfo IP.
    if (
      this.stackedLinkInfo &&
      !!this.stackedLinkInfo.ips.length &&
      this.stackedLinkInfo.prefixLengths.length
    ) {
      aIps.value = this.ips.concat(this.stackedLinkInfo.ips);
      aPrefixLengths.value = this.prefixLengths.concat(
        this.stackedLinkInfo.prefixLengths
      );
    } else {
      aIps.value = this.ips.slice();
      aPrefixLengths.value = this.prefixLengths.slice();
    }
    return aIps.value.length;
  },

  getGateways(aCount) {
    // Combine the stackedLinkInfo gateways.
    let gateways_all = [];
    if (this.stackedLinkInfo && this.stackedLinkInfo.gateways.length) {
      if (!this.gateways.includes(this.stackedLinkInfo.gateways)) {
        gateways_all = this.gateways.concat(this.stackedLinkInfo.gateways);
      }
    } else {
      gateways_all = this.gateways.slice();
    }

    if (aCount) {
      aCount.value = gateways_all.length;
    }

    return gateways_all.slice();
  },

  getDnses(aCount) {
    // Combine the stackedLinkInfo dnses.
    let dnses_all = [];
    if (this.stackedLinkInfo && this.stackedLinkInfo.dnses.length) {
      if (!this.dnses.includes(this.stackedLinkInfo.dnses)) {
        dnses_all = this.dnses.concat(this.stackedLinkInfo.dnses);
      }
    } else {
      dnses_all = this.dnses.slice();
    }

    if (aCount) {
      aCount.value = dnses_all.length;
    }

    return dnses_all.slice();
  },

  /**
   * nsIRilNetworkInfo Implementation
   */
  getPcscf(aCount) {
    if (this.type != Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS) {
      debug("-*- NetworkManager: Error! Only IMS network can get pcscf.");
      return "";
    }
    if (aCount) {
      aCount.value = this.pcscf.length;
    }
    return this.pcscf.slice();
  },
  /**
   * nsINat464XlatInfo Implementation
   */
  stackedLinkInfo: null,

  /**
   * To reset the Networkinfo
   **/
  resetNetworkInfo() {
    this.ips_length = 0;
    this.ips = [];
    this.prefixLengths = [];
    this.gateways = [];
    this.dnses = [];
    this.pcscf = [];
    this.state = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;
    this.tcpbuffersizes = [];
  },
};

function NetworkInterfaceLinks() {
  this.resetLinks();
}
NetworkInterfaceLinks.prototype = {
  linkRoutes: null,
  gateways: null,
  interfaceName: null,
  extraRoutes: null,

  setLinks(linkRoutes, gateways, interfaceName) {
    this.linkRoutes = linkRoutes;
    this.gateways = gateways;
    this.interfaceName = interfaceName;
  },

  resetLinks() {
    this.linkRoutes = [];
    this.gateways = [];
    this.interfaceName = "";
    this.extraRoutes = [];
  },

  compareGateways(gateways) {
    if (this.gateways.length != gateways.length) {
      return false;
    }

    for (let i = 0; i < this.gateways.length; i++) {
      if (this.gateways[i] != gateways[i]) {
        return false;
      }
    }

    return true;
  },
};

function Nat464Xlat(aNetworkInfo) {
  this.clear();
  this.stackedLinkInfo = new StackedLinkInfo(null);
  this.ifaceName = aNetworkInfo.name;
  this.nat464Iface = null;
  this.types.push(aNetworkInfo.type);
  this.netId = aNetworkInfo.netId;
  this.nat64Prefix = null;
  this.nat64State = Ci.nsINat464XlatInfo.XLAT_STATE_IDLE;

  // Used in resolveHostname().
  defineLazyRegExp(this, "REGEXP_IPV4", "^\\d{1,3}(?:\\.\\d{1,3}){3}$");
  defineLazyRegExp(
    this,
    "REGEXP_IPV6",
    "^[\\da-fA-F]{4}(?::[\\da-fA-F]{4}){7}$"
  );
}
Nat464Xlat.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  stackedLinkInfo: null,
  ifaceName: null,
  nat464Iface: null,
  types: [],
  netId: null,
  nat64Prefix: null,
  clatdAddress: null,
  nat64State: null,

  isStarted() {
    return (
      this.nat64State == Ci.nsINat464XlatInfo.XLAT_STATE_STARTING ||
      this.nat64State == Ci.nsINat464XlatInfo.XLAT_STATE_RUNNING
    );
  },

  isStarting() {
    return this.nat64State == Ci.nsINat464XlatInfo.XLAT_STATE_STARTING;
  },

  isRunning() {
    return this.nat64State == Ci.nsINat464XlatInfo.XLAT_STATE_RUNNING;
  },

  isPrefixDiscoveryStarted() {
    return (
      this.nat64State == Ci.nsINat464XlatInfo.XLAT_STATE_DISCOVERING ||
      this.isStarted()
    );
  },

  clear() {
    this.stackedLinkInfo = null;
    this.ifaceName = null;
    this.nat464Iface = null;
    this.types = [];
    this.netId = null;
    this.nat64Prefix = null;
    this.clatdAddress = null;
    this.nat64State = null;
  },

  getNat64Prefix() {
    return this.nat64Prefix;
  },

  setupPrefix64Discovery(aEnable) {
    let self = this;
    this.nat64Debug("setupPrefix64Discovery aEnable:" + aEnable);
    // Set state discovering.
    this.nat64State = Ci.nsINat464XlatInfo.XLAT_STATE_DISCOVERING;
    lazy.gNetworkService.setupPrefix64Discovery(
      this.ifaceName,
      aEnable,
      success => {
        self.nat64Debug(
          "setupPrefix64Discovery " +
            aEnable +
            " : " +
            (success ? "success" : "fail")
        );
      }
    );
  },

  shouldRunClat() {
    let shouldRun = this.getNat64Prefix() != null;
    this.nat64Debug("shouldRun = " + shouldRun);
    return shouldRun;
  },

  requiresClat(aNetworkInfo) {
    let self = this;
    let connected =
      aNetworkInfo.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED;
    if (!connected) {
      return Promise.resolve(false);
    }

    return new Promise((aResolve, aReject) => {
      let hasIpv4 = false;
      let ips = {};
      let prefixLengths = {};
      let length = aNetworkInfo.getAddresses(ips, prefixLengths);
      for (let i = 0; i < length; i++) {
        self.nat64Debug(
          "requiresClat routes: " + ips.value[i] + "/" + prefixLengths.value[i]
        );
        if (ips.value[i].match(this.REGEXP_IPV4)) {
          hasIpv4 = true;
          break;
        }
      }
      self.nat64Debug("requiresClat hasIpv4 = " + hasIpv4);
      aResolve(!hasIpv4);
    });
  },

  start(aCallback) {
    let self = this;

    this.nat64Debug("Starting clatd");
    if (this.ifaceName == null) {
      this.nat64Debug("clatd: Can't start clatd without providing interface");
      aCallback(false);
      return;
    }

    if (this.isStarted()) {
      this.nat64Debug("clatd: already started");
      aCallback(true);
      return;
    }

    this.nat464Iface = CLAT_PREFIX + this.ifaceName;

    this.nat64Debug("Starting clatd on " + this.ifaceName);

    lazy.gNetworkService.startClatd(this.ifaceName, this.nat64Prefix, function(
      success,
      clatdAddress
    ) {
      self.nat64Debug(
        "Clatd started: " +
          (success ? "success" : "fail") +
          " address: " +
          clatdAddress
      );

      if (success) {
        // Set state starting.
        self.nat64State = Ci.nsINat464XlatInfo.XLAT_STATE_STARTING;
        self.clatdAddress = clatdAddress;
      }
      aCallback(success);
    });
  },

  stop(aCallback) {
    let self = this;
    if (!this.isStarted()) {
      this.nat64Debug("clatd: already stopped");
      aCallback(true);
      return;
    }

    this.nat64Debug("Stopping clatd");

    lazy.gNetworkService.stopClatd(this.ifaceName, success => {
      // Clean nat464Iface if the isRunning is false. Which means the clat do not start yet but turn off.
      // Whne clat start but stop in a short time, kernel will not lunch the clat interface, cause the isStarted value do not reset.
      if (!this.isRunning()) {
        this.nat64Debug("Force clean the isStarted due to clat stop.");
        this.nat464Iface = null;
      }

      this.nat64Debug("Clatd stopped : " + (success ? "success" : "fail"));
      // Set state idle.
      self.nat64State = Ci.nsINat464XlatInfo.XLAT_STATE_IDLE;
      aCallback(success);
    });
  },

  nat64Debug(aString) {
    debug("clat-" + this.ifaceName + ": " + aString);
  },
};

function CaptivePortalLanding(aNetworkType) {
  this._networkType = aNetworkType;
  this._landing = false;
}
CaptivePortalLanding.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsICaptivePortalLanding]),

  get networkType() {
    return this._networkType;
  },

  get landing() {
    return this._landing;
  },

  set landing(aVal) {
    this._landing = aVal;
  },
};

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
export function NetworkManager() {
  this.networkInterfaces = {};
  this.networkInterfaceLinks = {};
  this.networkNat464Links = {};

  try {
    this._manageOfflineStatus =
      this._manageOfflineStatus ||
      Services.prefs.getBoolPref(PREF_MANAGE_OFFLINE_STATUS);
  } catch (ex) {
    this._manageOfflineStatus = true;
  }
  Services.prefs.addObserver(PREF_MANAGE_OFFLINE_STATUS, this);
  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, TOPIC_NETD_INTERFACE_CHANGE);
  Services.obs.addObserver(this, TOPIC_NETD_INTERFACE_REMOVE);
  Services.obs.addObserver(this, TOPIC_NETD_NET64_PREFIX_EVENT);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);

  this.setAndConfigureActive();

  lazy.ppmm.addMessageListener("NetworkInterfaceList:ListInterface", this);

  // Used in resolveHostname().
  defineLazyRegExp(this, "REGEXP_IPV4", "^\\d{1,3}(?:\\.\\d{1,3}){3}$");
  defineLazyRegExp(
    this,
    "REGEXP_IPV6",
    "^[\\da-fA-F]{4}(?::[\\da-fA-F]{4}){7}$"
  );

  for (let networkType of this._captivePortalSupportTypes) {
    let captivePortalLanding = new CaptivePortalLanding(networkType);
    this._captivePortalLandings.push(captivePortalLanding);
    BinderServices.connectivity.onCaptivePortalChanged(captivePortalLanding);
  }
}

NetworkManager.prototype = {
  classID: NETWORKMANAGER_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINetworkManager,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),
  _stateRequests: [],

  _requestProcessing: false,

  _currentRequest: null,

  queueRequest(aMsg) {
    this._stateRequests.push({
      msg: aMsg,
    });
    this.nextRequest();
  },

  requestDone: function requestDone() {
    this._currentRequest = null;
    this._requestProcessing = false;
    this.nextRequest();
  },

  nextRequest: function nextRequest() {
    // No request to process
    if (this._stateRequests.length === 0) {
      return;
    }

    // Handling request, wait for it.
    if (this._requestProcessing) {
      return;
    }
    // Hold processing lock
    this._requestProcessing = true;

    // Find next valid request
    this._currentRequest = this._stateRequests.shift();

    this.handleRequest(this._currentRequest);
  },

  handleRequest(request) {
    let msg = request.msg;
    debug("handleRequest msg.name=" + msg.name);
    switch (msg.name) {
      case "updateNetworkInterface":
        this.compareNetworkInterface(msg.network, msg.networkId);
        break;
      case "registerNetworkInterface":
        this.onRegisterNetworkInterface(msg.network, msg.networkId);
        break;
      case "unregisterNetworkInterface":
        this.onUnregisterNetworkInterface(msg.network, msg.networkId);
        break;
      case "addHostRoute":
        this.onAddHostRoute(msg.network, msg.host).then(
          () => {
            msg.callback();
          },
          aError => {
            debug("onAddHostRoute ERROR.");
            msg.callback(aError);
          }
        );
        break;
      case "removeHostRoute":
        this.onRemoveHostRoute(msg.network, msg.host).then(
          () => {
            msg.callback();
          },
          aError => {
            debug("onRemoveHostRoute ERROR.");
            msg.callback(aError);
          }
        );
        break;
      case "interfaceLinkStateChanged":
        this.onInterfaceLinkStateChanged(msg.clatNetworkIface);
        break;
      case "interfaceRemoved":
        this.onInterfaceRemoved(msg.clatNetworkIface);
        break;
      case "nat64PrefixEvent":
        this.onNat64PrefixEvent(msg.netId, msg.prefixString, msg.prefixLength);
        break;
    }
  },

  // nsIObserver

  observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_PREF_CHANGED:
        if (data === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        } else if (data === PREF_MANAGE_OFFLINE_STATUS) {
          this._manageOfflineStatus = Services.prefs.getBoolPref(
            PREF_MANAGE_OFFLINE_STATUS
          );
          debug(
            PREF_MANAGE_OFFLINE_STATUS +
              " has changed to " +
              this._manageOfflineStatus
          );
        }
        break;

      case TOPIC_NETD_INTERFACE_CHANGE:
        {
          // Format: "<name> <up/down>"
          let token = data.split(" ");
          if (token.length < 2) {
            return;
          }

          debug("TOPIC_NETD_INTERFACE_CHANGE token=" + JSON.stringify(token));
          let iface = token[0];
          let up = token[1] == "up";
          if (up && iface.includes(CLAT_PREFIX)) {
            this.interfaceLinkStateChanged(iface);
          }
        }
        break;
      case TOPIC_NETD_INTERFACE_REMOVE:
        {
          // Format: "<name>"
          debug("TOPIC_NETD_INTERFACE_REMOVE token=" + JSON.stringify(data));
          let iface = data;
          if (iface.includes(CLAT_PREFIX)) {
            this.interfaceRemoved(iface);
          }
        }
        break;
      case TOPIC_NETD_NET64_PREFIX_EVENT:
        // Format: "<netId> add <ipv6 prefix> <prefix length>"
        debug("TOPIC_NETD_NET64_PREFIX_EVENT data=" + JSON.stringify(data));
        let value = data.split(" ");
        if (value.length < 4) {
          return;
        }
        this.nat64PrefixEvent(value[0], value[1], value[2], value[3]);
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_NETD_INTERFACE_CHANGE);
        Services.obs.removeObserver(this, TOPIC_NETD_INTERFACE_REMOVE);
        Services.obs.removeObserver(this, TOPIC_NETD_NET64_PREFIX_EVENT);
        Services.prefs.removeObserver(PREF_MANAGE_OFFLINE_STATUS, this);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        break;
    }
  },
  /* eslint-disable consistent-return */
  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "NetworkInterfaceList:ListInterface": {
        let excludeMms = aMsg.json.excludeMms;
        let excludeSupl = aMsg.json.excludeSupl;
        let excludeIms = aMsg.json.excludeIms;
        let excludeDun = aMsg.json.excludeDun;
        let excludeFota = aMsg.json.excludeFota;
        let interfaces = [];

        for (let key in this.networkInterfaces) {
          let network = this.networkInterfaces[key];
          let i = network.info;
          if (
            (i.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS &&
              excludeMms) ||
            (i.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL &&
              excludeSupl) ||
            (i.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS &&
              excludeIms) ||
            (i.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN &&
              excludeDun) ||
            (i.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA &&
              excludeFota)
          ) {
            continue;
          }

          let ips = {};
          let prefixLengths = {};
          i.getAddresses(ips, prefixLengths);

          interfaces.push({
            state: i.state,
            type: i.type,
            name: i.name,
            ips: ips.value,
            netId: i.netId,
            prefixLengths: prefixLengths.value,
            gateways: i.getGateways(),
            dnses: i.getDnses(),
          });
        }
        return interfaces;
      }
    }
  },
  /* eslint-enable consistent-return */

  getNetworkId(aNetworkInfo) {
    let id = "device";
    try {
      if (aNetworkInfo instanceof Ci.nsIRilNetworkInfo) {
        let rilInfo = aNetworkInfo.QueryInterface(Ci.nsIRilNetworkInfo);
        id = "ril" + rilInfo.serviceId;
      }
    } catch (e) {}

    return id + "-" + aNetworkInfo.type;
  },

  // nsINetworkManager

  registerNetworkInterface(network) {
    debug("registerNetworkInterface. network=" + JSON.stringify(network));
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception(
        "Argument must be nsINetworkInterface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    let networkId = this.getNetworkId(network.info);
    // Keep a copy of network in case it is modified while we are updating.
    let extNetwork = new ExtraNetworkInterface(network);
    // Send message.
    this.queueRequest({
      name: "registerNetworkInterface",
      network: extNetwork,
      networkId,
    });
  },

  onRegisterNetworkInterface(aNetwork, aNetworkId) {
    if (aNetworkId in this.networkInterfaces) {
      this.requestDone();
      throw Components.Exception(
        "Network with that type already registered!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    this.networkInterfaces[aNetworkId] = aNetwork;
    this.networkInterfaceLinks[aNetworkId] = new NetworkInterfaceLinks();

    Services.obs.notifyObservers(aNetwork.info, TOPIC_INTERFACE_REGISTERED);
    debug("Network '" + aNetworkId + "' registered.");

    // Update network info.
    this.onUpdateNetworkInterface(aNetwork, null, aNetworkId);
  },

  /* eslint-disable no-unused-vars */
  _updateSubnetRoutes(aPreNetworkInfo, aExtNetworkInfo) {
    let preIps = {};
    let prePrefixLengths = {};
    let extIps = {};
    let extPrefixLengths = {};
    let preLength;
    let extLength;
    let removedIpsLinks = [];
    let addedIpsLinks = [];

    let promises = [];

    if (aPreNetworkInfo) {
      preLength = aPreNetworkInfo.getAddresses(preIps, prePrefixLengths);
    }

    if (aExtNetworkInfo) {
      extLength = aExtNetworkInfo.getAddresses(extIps, extPrefixLengths);
    }

    function getDifference(base, target) {
      return base.filter(function(i) {
        return !target.includes(i);
      });
    }

    if (aPreNetworkInfo && aExtNetworkInfo) {
      debug("_updateSubnetRoutes for Iface=" + aExtNetworkInfo.name);
      removedIpsLinks = getDifference(preIps.value, extIps.value);
      addedIpsLinks = getDifference(extIps.value, preIps.value);
    } else if (aExtNetworkInfo) {
      debug("_updateSubnetRoutes for Iface=" + aExtNetworkInfo.name);
      addedIpsLinks = extIps.value.slice();
    } else if (aPreNetworkInfo) {
      debug("_updateSubnetRoutes for Iface=" + aPreNetworkInfo.name);
      removedIpsLinks = preIps.value.slice();
    } else {
      return Promise.resolve();
    }

    debug(
      "_updateSubnetRoutes removedIpsLinks =" +
        JSON.stringify(removedIpsLinks) +
        " ,addedIpsLinks =" +
        JSON.stringify(addedIpsLinks)
    );

    if (removedIpsLinks.length) {
      removedIpsLinks.forEach(removedIpsLink => {
        let index = preIps.value.indexOf(removedIpsLink);
        if (index > -1) {
          debug(
            "Removing subnet routes: " +
              removedIpsLink +
              "/" +
              prePrefixLengths.value[index]
          );
          promises.push(
            lazy.gNetworkService
              .modifyRoute(
                Ci.nsINetworkService.MODIFY_ROUTE_REMOVE,
                aPreNetworkInfo.name,
                removedIpsLink,
                prePrefixLengths.value[index]
              )
              .catch(aError => {
                debug(
                  "_updateSubnetRoutes _removeSubnetRoutes error: " + aError
                );
              })
          );
        }
      });
    }
    if (addedIpsLinks.length) {
      addedIpsLinks.forEach(addedIpsLink => {
        let index = extIps.value.indexOf(addedIpsLink);
        if (index > -1) {
          debug(
            "Adding subnet routes: " +
              addedIpsLink +
              "/" +
              extPrefixLengths.value[index]
          );
          promises.push(
            lazy.gNetworkService
              .modifyRoute(
                Ci.nsINetworkService.MODIFY_ROUTE_ADD,
                aExtNetworkInfo.name,
                addedIpsLink,
                extPrefixLengths.value[index]
              )
              .catch(aError => {
                debug("_updateSubnetRoutes _addSubnetRoutes error: " + aError);
              })
          );
        }
      });
    }
    return Promise.all(promises);
  },
  /* eslint-enable no-unused-vars */

  _addSubnetRoutes(aNetworkInfo) {
    let ips = {};
    let prefixLengths = {};
    let length = aNetworkInfo.getAddresses(ips, prefixLengths);
    let promises = [];

    for (let i = 0; i < length; i++) {
      debug(
        "Adding subnet routes: " + ips.value[i] + "/" + prefixLengths.value[i]
      );
      promises.push(
        lazy.gNetworkService
          .modifyRoute(
            Ci.nsINetworkService.MODIFY_ROUTE_ADD,
            aNetworkInfo.name,
            ips.value[i],
            prefixLengths.value[i]
          )
          .catch(aError => {
            debug("_addSubnetRoutes error: " + aError);
          })
      );
    }

    return Promise.all(promises);
  },

  _removeSubnetRoutes(aNetworkInfo) {
    let ips = {};
    let prefixLengths = {};
    let length = aNetworkInfo.getAddresses(ips, prefixLengths);
    let promises = [];

    for (let i = 0; i < length; i++) {
      debug(
        "Removing subnet routes: " + ips.value[i] + "/" + prefixLengths.value[i]
      );
      promises.push(
        lazy.gNetworkService
          .modifyRoute(
            Ci.nsINetworkService.MODIFY_ROUTE_REMOVE,
            aNetworkInfo.name,
            ips.value[i],
            prefixLengths.value[i]
          )
          .catch(aError => {
            debug("_removeSubnetRoutes error: " + aError);
          })
      );
    }

    return Promise.all(promises);
  },

  _isIfaceChanged(preIface, newIface) {
    if (!preIface) {
      debug("_isIfaceChanged !preIface ");
      return true;
    }

    // Check if state changes.
    if (preIface.info.state != newIface.info.state) {
      debug(
        "_isIfaceChanged state change. " +
          preIface.info.state +
          "->" +
          newIface.info.state
      );
      return true;
    }

    // Check if IP changes.
    if (preIface.info.ips_length != newIface.info.ips_length) {
      debug(
        "_isIfaceChanged ips_length change. " +
          preIface.info.ips_length +
          "->" +
          newIface.info.ips_length
      );
      return true;
    }

    for (let i in preIface.info.ips) {
      if (!newIface.info.ips.includes(preIface.info.ips[i])) {
        debug("_isIfaceChanged ip change. new ip = " + newIface.info.ips);
        return true;
      }
    }

    // Check if gateway changes.
    if (preIface.info.gateways.length != newIface.info.gateways.length) {
      debug(
        "_isIfaceChanged gateways length change. " +
          preIface.info.gateways.length +
          "->" +
          newIface.info.gateways.length
      );
      return true;
    }

    for (let i in preIface.info.gateways) {
      if (!newIface.info.gateways.includes(preIface.info.gateways[i])) {
        debug(
          "_isIfaceChanged gateways change. new gateways = " +
            newIface.info.gateways
        );
        return true;
      }
    }

    // Check if dns changes.
    if (preIface.info.dnses.length != newIface.info.dnses.length) {
      debug(
        "_isIfaceChanged dnses length change. " +
          preIface.info.dnses.length +
          "->" +
          newIface.info.dnses.length
      );
      return true;
    }

    for (let i in preIface.info.dnses) {
      if (!newIface.info.dnses.includes(preIface.info.dnses[i])) {
        debug(
          "_isIfaceChanged dnses change. new dnses = " + newIface.info.dnses
        );
        return true;
      }
    }

    // Check if mtu changes.
    if (preIface.mtu != newIface.mtu) {
      debug(
        "_isIfaceChanged mtu change. " + preIface.mtu + "->" + newIface.mtu
      );
      return true;
    }

    // Check if httpProxyHost changes.
    if (preIface.httpProxyHost != newIface.httpProxyHost) {
      debug(
        "_isIfaceChanged httpProxyHost change. " +
          preIface.httpProxyHost +
          "->" +
          newIface.httpProxyHost
      );
      return true;
    }

    // Check if httpProxyPort changes.
    if (preIface.httpProxyPort != newIface.httpProxyPort) {
      debug(
        "_isIfaceChanged httpProxyPort change. " +
          preIface.httpProxyPort +
          "->" +
          newIface.httpProxyPort
      );
      return true;
    }

    return false;
  },

  _isIfaceTcpBufferChanged(preNetworkInfo, newNetworkInfo) {
    if (!newNetworkInfo || !newNetworkInfo.tcpbuffersizes) {
      debug("no newNetworkInfo return false");
      return false;
    }

    if (!preNetworkInfo || !preNetworkInfo.tcpbuffersizes) {
      debug("no preNetworkInfo return false");
      return false;
    }

    if (preNetworkInfo.tcpbuffersizes != newNetworkInfo.tcpbuffersizes) {
      debug(
        "_isIfaceTcpBufferChanged tcpbuffersizes change. " +
          preNetworkInfo.tcpbuffersizes +
          " -> " +
          newNetworkInfo.tcpbuffersizes
      );
      return true;
    }

    return false;
  },

  updateNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception(
        "Argument must be nsINetworkInterface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    debug(
      "Network " +
        convertToDataCallType(network.info.type) +
        "/" +
        network.info.name +
        " changed state to " +
        network.info.state
    );

    let networkId = this.getNetworkId(network.info);
    // Keep a copy of network in case it is modified while we are updating.
    let extNetwork = new ExtraNetworkInterface(network);

    // Clean the new network info if the state is disconnect.
    // (For some chipset will not notify the empty network info to upper layer when DISCONNECT state.)
    if (
      extNetwork.info &&
      extNetwork.info.state == Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED
    ) {
      extNetwork.info.resetNetworkInfo();
    }

    // Send message.
    this.queueRequest({
      name: "updateNetworkInterface",
      network: extNetwork,
      networkId,
    });
  },

  compareNetworkInterface(aNetwork, aNetworkId) {
    if (!(aNetworkId in this.networkInterfaces)) {
      this.requestDone();
      throw Components.Exception(
        "No network with that type registered.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    debug(
      "Process Network " +
        convertToDataCallType(aNetwork.info.type) +
        "/" +
        aNetwork.info.name +
        " changed state to " +
        aNetwork.info.state
    );

    // Previous network information.
    let preNetwork = this.networkInterfaces[aNetworkId];

    if (!this._isIfaceChanged(preNetwork, aNetwork)) {
      debug("Identical network interfaces.");
      // Update tcpbuffersizes if needed, handle the same activeNetwork case.
      if (
        this.activeNetworkInfo &&
        aNetwork.info.type == this.activeNetworkInfo.type
      ) {
        if (this._isIfaceTcpBufferChanged(preNetwork.info, aNetwork.info)) {
          debug("Same active network update the tcpbuffersizes.");
          this._setTcpBufferSize(aNetwork.info.tcpbuffersizes);
        }
      }
      this.requestDone();
      return;
    }

    // Update networkInterfaces with latest value.
    this.networkInterfaces[aNetworkId] = aNetwork;
    // Update network info.
    this.onUpdateNetworkInterface(aNetwork, preNetwork, aNetworkId);
  },

  /* eslint-disable consistent-return */
  onUpdateNetworkInterface(aNetwork, preNetwork, aNetworkId) {
    // Latest network information.
    // Add route or connected state using extNetworkInfo.
    let extNetworkInfo = aNetwork && aNetwork.info;
    debug("extNetworkInfo=" + JSON.stringify(extNetworkInfo));

    // Previous network information.
    // Remove route or disconnect state using preNetworkInfo.
    let preNetworkInfo = preNetwork && preNetwork.info;
    debug("preNetworkInfo=" + JSON.stringify(preNetworkInfo));

    // Note that since Lollipop we need to allocate and initialize
    // something through netd, so we add createNetwork/destroyNetwork
    // to deal with that explicitly.

    switch (extNetworkInfo.state) {
      case Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED:
        this._createNetwork(extNetworkInfo.name, extNetworkInfo.type)
          //Get the netid value.
          .then(() => {
            return lazy.gNetworkService
              .getNetId(extNetworkInfo.name)
              .then(aNetId => {
                debug("Stored aNetId = " + aNetId);
                extNetworkInfo.netId = aNetId;
              });
          })
          // Remove pre-created default route and let setAndConfigureActive()
          // to set default route only on preferred network
          .then(() => {
            return this._updateSubnetRoutes(preNetworkInfo, extNetworkInfo);
          })
          .then(() => {
            return this._updateDefaultRoute(preNetworkInfo, extNetworkInfo);
          })
          // Set DNS server as early as possible to prevent from
          // premature domain name lookup.
          .then(() => {
            return this._setDNS(extNetworkInfo);
          })
          .then(() => {
            // Add gateway route for data calls
            if (!this.isNetworkTypeMobile(extNetworkInfo.type)) {
              return;
            }

            let currentInterfaceLinks = this.networkInterfaceLinks[aNetworkId];
            let newLinkRoutes = aNetwork.httpProxyHost
              ? extNetworkInfo.getDnses().concat(aNetwork.httpProxyHost)
              : extNetworkInfo.getDnses();
            // If gateways have changed, remove all old routes first.
            return this._handleGateways(
              aNetworkId,
              extNetworkInfo.getGateways()
            ).then(() =>
              currentInterfaceLinks.setLinks(
                newLinkRoutes,
                extNetworkInfo.getGateways(),
                extNetworkInfo.name
              )
            );
          })
          .then(() => {
            if (
              extNetworkInfo.type != Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN
            ) {
              return;
            }
            // Dun type is a special case where we add the default route to a
            // secondary table.
            return this.setSecondaryDefaultRoute(extNetworkInfo);
          })
          .then(() => {
            if (aNetwork.mtu <= 0) {
              return;
            }

            return this._setMtu(aNetwork);
          })
          .then(() => this.updateClat(extNetworkInfo))
          .then(() =>
            lazy.gTetheringService.onExternalConnectionChanged(extNetworkInfo)
          )
          .then(() => this.setAndConfigureActive())
          .then(() => {
            // Update data connection when Wifi connected/disconnected
            if (
              extNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI &&
              this.mRil
            ) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }

            // Probing the public network accessibility after routing table is ready
            CaptivePortalDetectionHelper.notify(
              CaptivePortalDetectionHelper.EVENT_CONNECT,
              this.activeNetworkInfo
            );
          })
          .then(() => {
            // Notify outer modules like MmsService to start the transaction after
            // the configuration of the network interface is done.
            Services.obs.notifyObservers(
              extNetworkInfo,
              TOPIC_CONNECTION_STATE_CHANGED
            );
          })
          .then(() => {
            this.requestDone();
          })
          .catch(aError => {
            debug("onUpdateNetworkInterface error: " + aError);
            this.requestDone();
          });
        break;
      case Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED:
        if (
          preNetworkInfo &&
          preNetworkInfo.state != Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED
        ) {
          // Keep the previous information but change the state to disconnect.
          preNetworkInfo.state = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;
          debug("preNetworkInfo = " + JSON.stringify(preNetworkInfo));
        } else {
          debug(
            "preNetworkInfo = undefined or already disconencted, nothing to do. Break."
          );
          this.requestDone();
          break;
        }
        Promise.resolve()
          //Used the latest networkinfo to update the clat.
          .then(() => this.updateClat(preNetworkInfo))
          .then(() => {
            if (!this.isNetworkTypeMobile(preNetworkInfo.type)) {
              return;
            }
            // Remove host route for data calls
            return this._cleanupAllHostRoutes(aNetworkId);
          })
          .then(() => {
            if (
              preNetworkInfo.type != Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN
            ) {
              return;
            }
            // Remove secondary default route for dun.
            return this.removeSecondaryDefaultRoute(preNetworkInfo);
          })
          .then(() => {
            if (
              preNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI ||
              preNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET
            ) {
              // Remove routing table in /proc/net/route
              return this._resetRoutingTable(preNetworkInfo.name);
            }
            if (this.isNetworkTypeMobile(preNetworkInfo.type)) {
              return this._updateDefaultRoute(preNetworkInfo);
            }
          })
          .then(() => {
            if (this.isNetworkTypeMobile(preNetworkInfo.type)) {
              return this._updateSubnetRoutes(preNetworkInfo);
            }
          })
          .then(() => {
            // Clear http proxy on active network.
            if (
              this.activeNetworkInfo &&
              preNetworkInfo.type == this.activeNetworkInfo.type
            ) {
              this.clearNetworkProxy();
            }

            // Abort ongoing captive portal detection on the wifi interface
            CaptivePortalDetectionHelper.notify(
              CaptivePortalDetectionHelper.EVENT_DISCONNECT,
              preNetworkInfo
            );
          })
          .then(() =>
            lazy.gTetheringService.onExternalConnectionChanged(preNetworkInfo)
          )
          .then(() => this.setAndConfigureActive())
          .then(() => {
            // Update data connection when Wifi connected/disconnected
            if (
              preNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI &&
              this.mRil
            ) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }
          })
          .then(() =>
            this._destroyNetwork(preNetworkInfo.name, preNetworkInfo.type)
          )
          .then(() => {
            // Notify outer modules like MmsService to stop the transaction after
            // the configuration of the network interface is done.
            Services.obs.notifyObservers(
              preNetworkInfo,
              TOPIC_CONNECTION_STATE_CHANGED
            );
          })
          .then(() => {
            this.requestDone();
          })
          .catch(aError => {
            debug("onUpdateNetworkInterface error: " + aError);
            this.requestDone();
          });
        break;
      default:
        debug("onUpdateNetworkInterface undefined state.");
        this.requestDone();
        break;
    }
  },
  /* eslint-enable consistent-return */

  unregisterNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception(
        "Argument must be nsINetworkInterface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    debug("unregisterNetworkInterface. network=" + JSON.stringify(network));
    let networkId = this.getNetworkId(network.info);

    // Keep a copy of network in case it is modified while we are updating.
    let extNetwork = new ExtraNetworkInterface(network);

    // Clean the new network info if the state is disconnect.
    // (For some chipset will not notify the empty network info to upper layer when DISCONNECT state.)
    if (extNetwork.info) {
      extNetwork.info.resetNetworkInfo();
    }

    // Send message.
    this.queueRequest({
      name: "unregisterNetworkInterface",
      network: extNetwork,
      networkId,
    });
  },

  onUnregisterNetworkInterface(aNetwork, aNetworkId) {
    if (!(aNetworkId in this.networkInterfaces)) {
      this.requestDone();
      throw Components.Exception(
        "No network with that type registered.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    let preNetwork = this.networkInterfaces[aNetworkId];

    delete this.networkInterfaces[aNetworkId];

    Services.obs.notifyObservers(aNetwork.info, TOPIC_INTERFACE_UNREGISTERED);
    debug("Network '" + aNetworkId + "' unregistered.");
    // Update network info.
    this.onUpdateNetworkInterface(aNetwork, preNetwork, aNetworkId);
  },

  nat64PrefixEvent(aNetId, aAdded, aPrefixString, aPrefixLength) {
    debug(
      "NAT64 prefix " +
        (aAdded ? "added" : "removed") +
        " on netId " +
        aNetId +
        " :" +
        aPrefixString +
        "/" +
        aPrefixLength
    );

    let netId = aNetId;
    let prefixString = aPrefixString;
    let prefixLength = aPrefixLength;

    if (!aAdded) {
      prefixString = null;
      prefixLength = 0;
    }

    // Send message.
    this.queueRequest({
      name: "nat64PrefixEvent",
      netId,
      prefixString,
      prefixLength,
    });
  },

  onNat64PrefixEvent(aNetId, aPrefixString, aPrefixLength) {
    // Set the prefix
    let clatIfaceLink = null;
    for (var name in this.networkNat464Links) {
      if (this.networkNat464Links[name].netId == aNetId) {
        clatIfaceLink = this.networkNat464Links[name];
        break;
      }
    }

    if (!clatIfaceLink) {
      debug("No clat interface found for netId:" + aNetId);
      this.requestDone();
      return;
    }

    if (aPrefixString) {
      clatIfaceLink.nat64Prefix = aPrefixString + "/" + aPrefixLength;
    } else {
      clatIfaceLink.nat64Prefix = null;
    }

    // Update the clat for each networkInterfaces
    for (let aNetworkId in this.networkInterfaces) {
      if (this.networkInterfaces[aNetworkId].info.netId == aNetId) {
        let clatNetworkInfo = this.networkInterfaces[aNetworkId].info;
        this.updateClat(clatNetworkInfo).then(result => {
          debug(
            "updateClat for network:" + clatNetworkInfo.name + result
              ? " Success"
              : "Fail"
          );
        });
      }
    }
    this.requestDone();
  },

  interfaceLinkStateChanged(aNat464Iface) {
    let clatIface = aNat464Iface.split(CLAT_PREFIX);

    if (!(clatIface[1] in this.networkNat464Links)) {
      this.requestDone();
      throw Components.Exception(
        "No clat with that interface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let clatIfaceLink = this.networkNat464Links[clatIface[1]];

    if (clatIfaceLink && clatIfaceLink.isStarted()) {
      debug("interfaceLinkStateChanged for clatIface=" + clatIface[1]);
      // Send message.
      this.queueRequest({
        name: "interfaceLinkStateChanged",
        clatNetworkIface: clatIfaceLink.ifaceName,
      });
    }
  },

  /* eslint-disable consistent-return */
  onInterfaceLinkStateChanged(aClatNetworkIface) {
    debug("onInterfaceLinkStateChanged aClatNetworkIface=" + aClatNetworkIface);
    if (!(aClatNetworkIface in this.networkNat464Links)) {
      this.requestDone();
      throw Components.Exception(
        "No clat with that interface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let clatIfaceLink = this.networkNat464Links[aClatNetworkIface];
    let clatAddress = [];
    let clatPrefixLength = [];

    if (clatIfaceLink.isRunning()) {
      debug("onInterfaceLinkStateChanged. Already config clat value. Skip!");
      this.requestDone();
      return;
    }

    return lazy.gNetworkService.getInterfaceConfig(
      clatIfaceLink.nat464Iface,
      function(success, result) {
        if (!success) {
          debug(
            "getInterfaceConfig fail. interface=" + clatIfaceLink.nat464Iface
          );
          this.requestDone();
          return;
        }

        if (
          result.ip &&
          result.ip.match(this.REGEXP_IPV4) &&
          result.ip !== "0.0.0.0"
        ) {
          debug(
            "interface " +
              clatIfaceLink.nat464Iface +
              " is up, IsRunning " +
              clatIfaceLink.isRunning() +
              "->true"
          );
          clatAddress.push(result.ip);
          clatPrefixLength.push(result.prefix);

          debug(
            "interface= " +
              clatIfaceLink.nat464Iface +
              " ,clatAddress =" +
              JSON.stringify(clatAddress) +
              " , clatPrefixLength =" +
              JSON.stringify(clatPrefixLength) +
              " , clatIfaceLink.netId =" +
              clatIfaceLink.netId
          );
          // Config the ifname, ip address and gateway to stackedLinkInfo.
          clatIfaceLink.stackedLinkInfo.name = clatIfaceLink.nat464Iface;
          // Although the clat interface is a point-to-point tunnel, we don't
          // point the route directly at the interface because some apps don't
          // understand routes without gateways (see, e.g., http://b/9597256
          // http://b/9597516). Instead, set the next hop of the route to the
          // clat IPv4 address itself (for those apps, it doesn't matter what
          // the IP of the gateway is, only that there is one).
          clatIfaceLink.stackedLinkInfo.gateways = clatAddress.slice();
          clatIfaceLink.stackedLinkInfo.ips = clatAddress.slice();
          clatIfaceLink.stackedLinkInfo.prefixLengths = clatPrefixLength.slice();

          // Create a network for CLAT interface and config the route.
          debug(
            "stackedLinkInfo=" + JSON.stringify(clatIfaceLink.stackedLinkInfo)
          );
          this._addInterfaceToNetwork(
            clatIfaceLink.netId,
            clatIfaceLink.stackedLinkInfo.name
          )
            .catch(aError => {
              debug("_addInterfaceToNetwork error: " + aError);
            })
            .then(() => {
              return this._updateDefaultRoute(
                null,
                clatIfaceLink.stackedLinkInfo
              );
            })
            .then(() => {
              return this._updateSubnetRoutes(
                null,
                clatIfaceLink.stackedLinkInfo
              );
            })
            .then(() => {
              // Update clat info for types
              let network;
              debug("updateClatInfoAndNotify.");
              //Set state running.
              clatIfaceLink.state = Ci.nsINat464XlatInfo.XLAT_STATE_RUNNING;
              for (var networkId in this.networkInterfaces) {
                network = this.networkInterfaces[networkId];
                if (network.info.name == aClatNetworkIface) {
                  this.updateClatInfoAndNotify(aClatNetworkIface, networkId);
                }
              }
              return Promise.resolve();
            })
            .then(() => {
              this.requestDone();
              debug(
                "onInterfaceLinkStateChanged requestDone. aClatNetworkIface=" +
                  aClatNetworkIface
              );
            });
        } else {
          debug(
            "Incorrect format for clatAddress. IsRunning " +
              clatIfaceLink.isRunning() +
              "->false"
          );
          this.requestDone();
        }
      }.bind(this)
    );
  },
  /* eslint-enable consistent-return */

  interfaceRemoved(aNat464Iface) {
    let clatIface = aNat464Iface.split(CLAT_PREFIX);

    if (!(clatIface[1] in this.networkNat464Links)) {
      this.requestDone();
      throw Components.Exception(
        "No clat with that interface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let clatIfaceLink = this.networkNat464Links[clatIface[1]];
    if (
      clatIfaceLink &&
      clatIfaceLink.isStarted() &&
      clatIfaceLink.isRunning()
    ) {
      debug("interfaceRemoved clat aNat464Iface=" + aNat464Iface);
      // Send message.
      this.queueRequest({
        name: "interfaceRemoved",
        clatNetworkIface: clatIfaceLink.ifaceName,
      });
    }
  },

  onInterfaceRemoved(aClatNetworkIface) {
    debug("onInterfaceRemoved aClatNetworkIface=" + aClatNetworkIface);
    if (!(aClatNetworkIface in this.networkNat464Links)) {
      this.requestDone();
      throw Components.Exception(
        "No clat with that interface.",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let clatIfaceLink = this.networkNat464Links[aClatNetworkIface];

    if (clatIfaceLink.isRunning() && clatIfaceLink.stackedLinkInfo) {
      debug(
        "interface " +
          clatIfaceLink.nat464Iface +
          " removed, IsRunning " +
          clatIfaceLink.isRunning() +
          "->false"
      );

      // Set state idle.
      clatIfaceLink.nat64State = Ci.nsINat464XlatInfo.XLAT_STATE_IDLE;
      this._updateSubnetRoutes(clatIfaceLink.stackedLinkInfo, null)
        .then(() => {
          return this._updateDefaultRoute(clatIfaceLink.stackedLinkInfo, null);
        })
        .then(() => {
          return this._removeInterfaceToNetwork(
            clatIfaceLink.netId,
            clatIfaceLink.stackedLinkInfo.name
          );
        })
        .catch(aError => {
          debug("_removeInterfaceToNetwork error: " + aError);
        })
        .then(() => {
          return clatIfaceLink.clear();
        })
        .then(() => {
          // Update clat info for types
          let network;
          for (var networkId in this.networkInterfaces) {
            network = this.networkInterfaces[networkId];
            if (network.info.name == aClatNetworkIface) {
              this.updateClatInfoAndNotify(aClatNetworkIface, networkId);
            }
          }
          return Promise.resolve();
        })
        .then(() => {
          this.requestDone();
          debug(
            "onInterfaceRemoved requestDone. aClatNetworkIface=" +
              aClatNetworkIface
          );
        });
    } else {
      debug("onInterfaceRemoved. Already disable for clat. Skip!");
      this.requestDone();
    }
  },

  _manageOfflineStatus: true,

  networkInterfaces: null,

  networkInterfaceLinks: null,

  networkNat464Links: null,

  _networkTypePriorityList: [
    Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET,
    Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,
    Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
  ],
  get networkTypePriorityList() {
    return this._networkTypePriorityList;
  },
  set networkTypePriorityList(val) {
    if (val.length != this._networkTypePriorityList.length) {
      throw new Error(
        "Priority list length should equal to " +
          this._networkTypePriorityList.length
      );
    }

    // Check if types in new priority list are valid and also make sure there
    // are no duplicate types.
    let list = [
      Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET,
      Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,
      Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
    ];
    while (list.length) {
      let type = list.shift();
      if (!val.includes(type)) {
        throw new Error("There is missing network type");
      }
    }

    this._networkTypePriorityList = val;
  },

  getPriority(type) {
    if (!this._networkTypePriorityList.includes(type)) {
      // 0 indicates the lowest priority.
      return 0;
    }

    return (
      this._networkTypePriorityList.length -
      this._networkTypePriorityList.indexOf(type)
    );
  },

  get allNetworkInfo() {
    let allNetworkInfo = {};

    for (let networkId in this.networkInterfaces) {
      if (this.networkInterfaces.hasOwnProperty(networkId)) {
        allNetworkInfo[networkId] = this.networkInterfaces[networkId].info;
      }
    }

    return allNetworkInfo;
  },

  _preferredNetworkType: DEFAULT_PREFERRED_NETWORK_TYPE,
  get preferredNetworkType() {
    return this._preferredNetworkType;
  },
  set preferredNetworkType(val) {
    if (
      ![
        Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,
        Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
        Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET,
      ].includes(val)
    ) {
      throw new Error("Invalid network type");
    }
    this._preferredNetworkType = val;
  },

  // Provide support captive portal connection type.
  _captivePortalSupportTypes: [Ci.nsINetworkInfo.NETWORK_TYPE_WIFI],

  _captivePortalLandings: [],

  getCaptivePortalLandings(count) {
    if (count) {
      count.value = this._captivePortalLandings.length;
    }
    return this._captivePortalLandings;
  },

  _activeNetwork: null,

  get activeNetworkInfo() {
    return this._activeNetwork && this._activeNetwork.info;
  },

  _overriddenActive: null,

  overrideActive(network) {
    if (
      ![
        Ci.nsINetworkInfo.NETWORK_TYPE_WIFI,
        Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE,
        Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET,
      ].includes(network.info.type)
    ) {
      throw new Error("Invalid network type");
    }

    this._overriddenActive = network;
    this.setAndConfigureActive();
  },

  _updateRoutes(oldLinks, newLinks, gateways, interfaceName) {
    // Returns items that are in base but not in target.
    function getDifference(base, target) {
      return base.filter(function(i) {
        return !target.includes(i);
      });
    }

    let addedLinks = getDifference(newLinks, oldLinks);
    let removedLinks = getDifference(oldLinks, newLinks);

    if (addedLinks.length === 0 && removedLinks.length === 0) {
      return Promise.resolve();
    }

    return this._setHostRoutes(
      false,
      removedLinks,
      interfaceName,
      gateways
    ).then(this._setHostRoutes(true, addedLinks, interfaceName, gateways));
  },

  _setHostRoutes(doAdd, ipAddresses, networkName, gateways) {
    let getMaxPrefixLength = aIp => {
      return aIp.match(this.REGEXP_IPV4)
        ? IPV4_MAX_PREFIX_LENGTH
        : IPV6_MAX_PREFIX_LENGTH;
    };

    let promises = [];

    ipAddresses.forEach(aIpAddress => {
      let gateway = this.selectGateway(gateways, aIpAddress);
      if (gateway) {
        // For the clat 464 case.
        let ifaceName = networkName;
        if (aIpAddress.match(this.REGEXP_IPV4)) {
          let clatIfaceLink = this.networkNat464Links[networkName];
          ifaceName =
            (clatIfaceLink != null && clatIfaceLink.nat464Iface) || networkName;
        }

        promises.push(
          doAdd
            ? lazy.gNetworkService.modifyRoute(
                Ci.nsINetworkService.MODIFY_ROUTE_ADD,
                ifaceName,
                aIpAddress,
                getMaxPrefixLength(aIpAddress),
                gateway
              )
            : lazy.gNetworkService.modifyRoute(
                Ci.nsINetworkService.MODIFY_ROUTE_REMOVE,
                ifaceName,
                aIpAddress,
                getMaxPrefixLength(aIpAddress),
                gateway
              )
        );
      }
    });

    return Promise.all(promises);
  },

  isValidatedNetwork(aNetworkInfo) {
    let isValid = false;
    try {
      isValid = this.getNetworkId(aNetworkInfo) in this.networkInterfaces;
    } catch (e) {
      debug("Invalid network interface: " + e);
    }

    return isValid;
  },

  addHostRoute(aNetworkInfo, aHost) {
    debug(
      "addHostRoute  aNetworkInfo.name=" +
        aNetworkInfo.name +
        " ,aHost=" +
        aHost
    );
    return new Promise((aResolve, aReject) => {
      let callback = aError => {
        if (aError) {
          aReject("addHostRoute failed.");
          this.requestDone();
          return;
        }
        debug("addHostRoute successed.");
        aResolve();
        this.requestDone();
      };
      this.queueRequest({
        name: "addHostRoute",
        network: aNetworkInfo,
        host: aHost,
        callback,
      });
    });
  },

  onAddHostRoute(aNetworkInfo, aHost) {
    if (!this.isValidatedNetwork(aNetworkInfo)) {
      return Promise.reject("Invalid network info.");
    }
    debug(
      "onAddHostRoute  aNetworkInfo.name=" +
        aNetworkInfo.name +
        " ,aHost=" +
        aHost
    );

    return this.resolveHostname(aNetworkInfo, aHost).then(ipAddresses => {
      let promises = [];
      let networkId = this.getNetworkId(aNetworkInfo);

      ipAddresses.forEach(aIpAddress => {
        let promise = this._setHostRoutes(
          true,
          [aIpAddress],
          aNetworkInfo.name,
          aNetworkInfo.getGateways()
        ).then(() =>
          this.networkInterfaceLinks[networkId].extraRoutes.push(aIpAddress)
        );

        promises.push(promise);
      });

      return Promise.all(promises);
    });
  },

  removeHostRoute(aNetworkInfo, aHost) {
    debug(
      "removeHostRoute  aNetworkInfo.name=" +
        aNetworkInfo.name +
        " ,aHost=" +
        aHost
    );
    return new Promise((aResolve, aReject) => {
      let callback = aError => {
        if (aError) {
          aReject("removeHostRoute failed.");
          this.requestDone();
          return;
        }
        debug("removeHostRoute successed.");
        aResolve();
        this.requestDone();
      };
      this.queueRequest({
        name: "removeHostRoute",
        network: aNetworkInfo,
        host: aHost,
        callback,
      });
    });
  },

  onRemoveHostRoute(aNetworkInfo, aHost) {
    if (!this.isValidatedNetwork(aNetworkInfo)) {
      return Promise.reject("Invalid network info.");
    }
    debug(
      "onRemoveHostRoute  aNetworkInfo.name=" +
        aNetworkInfo.name +
        " ,aHost=" +
        aHost
    );

    return this.resolveHostname(aNetworkInfo, aHost).then(ipAddresses => {
      let promises = [];
      let networkId = this.getNetworkId(aNetworkInfo);

      ipAddresses.forEach(aIpAddress => {
        let found = this.networkInterfaceLinks[networkId].extraRoutes.indexOf(
          aIpAddress
        );
        if (found < 0) {
          return; // continue
        }

        let promise = this._setHostRoutes(
          false,
          [aIpAddress],
          aNetworkInfo.name,
          aNetworkInfo.getGateways()
        ).then(
          () => {
            this.networkInterfaceLinks[networkId].extraRoutes.splice(found, 1);
          },
          () => {
            // We should remove it even if the operation failed.
            this.networkInterfaceLinks[networkId].extraRoutes.splice(found, 1);
          }
        );
        promises.push(promise);
      });

      return Promise.all(promises);
    });
  },

  isNetworkTypeSecondaryMobile(type) {
    return (
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS ||
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL ||
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS ||
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN ||
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA ||
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_HIPRI
    );
  },

  isNetworkTypeMobile(type) {
    return (
      type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE ||
      this.isNetworkTypeSecondaryMobile(type)
    );
  },

  _handleGateways(networkId, gateways) {
    let currentNetworkLinks = this.networkInterfaceLinks[networkId];
    if (
      !currentNetworkLinks.gateways.length ||
      currentNetworkLinks.compareGateways(gateways)
    ) {
      return Promise.resolve();
    }

    let currentExtraRoutes = currentNetworkLinks.extraRoutes;
    let currentInterfaceName = currentNetworkLinks.interfaceName;
    return this._cleanupAllHostRoutes(networkId).then(() => {
      // If gateways have changed, re-add extra host routes with new gateways.
      if (currentExtraRoutes.length) {
        this._setHostRoutes(
          true,
          currentExtraRoutes,
          currentInterfaceName,
          gateways
        ).then(() => {
          currentNetworkLinks.extraRoutes = currentExtraRoutes;
        });
      }
    });
  },

  _cleanupAllHostRoutes(networkId) {
    let currentNetworkLinks = this.networkInterfaceLinks[networkId];
    let hostRoutes = currentNetworkLinks.linkRoutes.concat(
      currentNetworkLinks.extraRoutes
    );

    if (hostRoutes.length === 0) {
      return Promise.resolve();
    }

    return this._setHostRoutes(
      false,
      hostRoutes,
      currentNetworkLinks.interfaceName,
      currentNetworkLinks.gateways
    )
      .catch(aError => {
        debug(
          "Error (" + aError + ") on _cleanupAllHostRoutes, keep proceeding."
        );
      })
      .then(() => currentNetworkLinks.resetLinks());
  },

  selectGateway(gateways, host) {
    for (let i = 0; i < gateways.length; i++) {
      let gateway = gateways[i];
      if (!host || !gateway) {
        continue;
      }
      if (
        (gateway.match(this.REGEXP_IPV4) && host.match(this.REGEXP_IPV4)) ||
        (gateway.includes(":") && host.includes(":"))
      ) {
        return gateway;
      }
    }
    return null;
  },

  _setSecondaryRoute(aDoAdd, aInterfaceName, aRoute) {
    return new Promise((aResolve, aReject) => {
      if (aDoAdd) {
        lazy.gNetworkService.addSecondaryRoute(
          aInterfaceName,
          aRoute,
          aSuccess => {
            if (!aSuccess) {
              aReject("addSecondaryRoute failed");
              return;
            }
            aResolve();
          }
        );
      } else {
        lazy.gNetworkService.removeSecondaryRoute(
          aInterfaceName,
          aRoute,
          aSuccess => {
            if (!aSuccess) {
              debug("removeSecondaryRoute failed");
            }
            // Always resolve.
            aResolve();
          }
        );
      }
    });
  },

  setSecondaryDefaultRoute(network) {
    let gateways = network.getGateways();
    let promises = [];

    for (let i = 0; i < gateways.length; i++) {
      let isIPv6 = gateways[i].includes(":");
      let promise;
      // Setup default route for secondary route.
      let defaultRoute = {
        ip: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY,
        prefix: 0,
        gateway: gateways[i],
      };

      if (isIPv6) {
        promise = this._setSecondaryRoute(true, network.name, defaultRoute);
      } else {
        // IPv4 need to add a host route to the gateway in the secondary
        // routing table to make the gateway reachable. Host route takes the max
        // prefix and gateway address 'any'.
        let hostRoute = {
          ip: gateways[i],
          prefix: IPV4_MAX_PREFIX_LENGTH,
          gateway: IPV4_ADDRESS_ANY,
        };
        promise = this._setSecondaryRoute(
          true,
          network.name,
          hostRoute
        ).then(() => this._setSecondaryRoute(true, network.name, defaultRoute));
      }

      promises.push(promise);
    }

    return Promise.all(promises);
  },

  removeSecondaryDefaultRoute(network) {
    let gateways = network.getGateways();
    let promises = [];

    for (let i = 0; i < gateways.length; i++) {
      let isIPv6 = gateways[i].includes(":");
      let promise;
      // Remove both default route and host route.
      let defaultRoute = {
        ip: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY,
        prefix: 0,
        gateway: gateways[i],
      };
      if (isIPv6) {
        promise = this._setSecondaryRoute(false, network.name, defaultRoute);
      } else {
        let hostRoute = {
          ip: gateways[i],
          prefix: IPV4_MAX_PREFIX_LENGTH,
          gateway: IPV4_ADDRESS_ANY,
        };
        promise = this._setSecondaryRoute(
          false,
          network.name,
          defaultRoute
        ).then(() => this._setSecondaryRoute(false, network.name, hostRoute));
      }

      promises.push(promise);
    }

    return Promise.all(promises);
  },

  updateDNSProperty(activeNetworkDnses) {
    let dnsPropPrefix = "net.dns";
    let index = 1;
    let dnsPropName = dnsPropPrefix + index;
    let dnsPropVal = libcutils.property_get(dnsPropName);
    while (dnsPropVal) {
      libcutils.property_set(dnsPropName, "");
      index++;
      dnsPropName = dnsPropPrefix + index;
      dnsPropVal = libcutils.property_get(dnsPropName);
    }

    for (let i = 0; i < activeNetworkDnses.length; i++) {
      index = i + 1;
      dnsPropName = dnsPropPrefix + index;
      libcutils.property_set(dnsPropName, activeNetworkDnses[i]);
    }
    let dnsChange = libcutils.property_get("net.dnschange", "0");
    libcutils.property_set(
      "net.dnschange",
      (parseInt(dnsChange, 10) + 1).toString()
    );
  },

  /**
   * Determine the active interface and configure it.
   */
  /* eslint-disable consistent-return */
  setAndConfigureActive() {
    debug("Evaluating whether active network needs to be changed.");
    let oldActive = this._activeNetwork;

    if (this._overriddenActive) {
      debug(
        "We have an override for the active network: " +
          this._overriddenActive.info.name
      );
      // The override was just set, so reconfigure the network.
      if (this._activeNetwork != this._overriddenActive) {
        this._activeNetwork = this._overriddenActive;
        this._setDefaultNetworkAndProxy(this._activeNetwork);
        Services.obs.notifyObservers(
          this.activeNetworkInfo,
          TOPIC_ACTIVE_CHANGED,
          this.convertActiveConnectionType(this.activeNetworkInfo)
        );
      }
      return;
    }

    // The active network is already our preferred type.
    if (
      this.activeNetworkInfo &&
      this.activeNetworkInfo.state ==
        Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED &&
      this.activeNetworkInfo.type == this._preferredNetworkType
    ) {
      debug("Active network is already our preferred type.");
      this._setDefaultNetworkAndProxy(this._activeNetwork);
      return;
    }

    // Find a suitable network interface to activate.
    this._activeNetwork = null;
    let anyConnected = false;

    for (let key in this.networkInterfaces) {
      let network = this.networkInterfaces[key];
      if (network.info.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        continue;
      }
      anyConnected = true;

      // Set active only for default connections.
      if (
        network.info.type != Ci.nsINetworkInfo.NETWORK_TYPE_WIFI &&
        network.info.type != Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE &&
        network.info.type != Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET
      ) {
        continue;
      }

      if (network.info.type == this.preferredNetworkType) {
        this._activeNetwork = network;
        debug("Found our preferred type of network: " + network.info.name);
        break;
      }

      // Initialize the active network with the first connected network.
      if (!this._activeNetwork) {
        this._activeNetwork = new ExtraNetworkInterface(network);
        continue;
      }

      // Compare the prioriy between two network types. If found incoming
      // network with higher priority, replace the active network.
      if (
        this.getPriority(this._activeNetwork.info.type) <
        this.getPriority(network.info.type)
      ) {
        this._activeNetwork = new ExtraNetworkInterface(network);
      }
    }

    return Promise.resolve()
      .then(() => {
        if (!this._activeNetwork) {
          return Promise.resolve();
        }
        return this._setDefaultNetworkAndProxy(this._activeNetwork);
      })
      .then(() => {
        if (this._activeNetwork != oldActive) {
          debug(
            "send TOPIC_ACTIVE_CHANGED activeNetworkInfo=" +
              JSON.stringify(this.activeNetworkInfo)
          );
          // Config the buffer sizes only when device has active network.
          if (this.activeNetworkInfo) {
            if (
              !oldActive ||
              this._isIfaceTcpBufferChanged(
                oldActive.info,
                this.activeNetworkInfo
              )
            ) {
              debug("activeNetwork change config the tcpbuffersizes.");
              this._setTcpBufferSize(this.activeNetworkInfo.tcpbuffersizes);
            }
          }
        }

        if (this._manageOfflineStatus) {
          Services.io.offline =
            !anyConnected &&
            lazy.gTetheringService.wifiState ===
              Ci.nsITetheringService.TETHERING_STATE_INACTIVE &&
            lazy.gTetheringService.usbState ===
              Ci.nsITetheringService.TETHERING_STATE_INACTIVE;
        }

        if (this._activeNetwork != oldActive) {
          Services.obs.notifyObservers(
            this.activeNetworkInfo,
            TOPIC_ACTIVE_CHANGED,
            this.convertActiveConnectionType(this.activeNetworkInfo)
          );
        }
        return Promise.resolve();
      });
  },
  /* eslint-enable consistent-return */

  resolveHostname(aNetworkInfo, aHostname) {
    // Sanity check for null, undefined and empty string... etc.
    if (!aHostname) {
      return Promise.reject(new Error("hostname is empty: " + aHostname));
    }

    if (
      aHostname.match(this.REGEXP_IPV4) ||
      aHostname.match(this.REGEXP_IPV6)
    ) {
      return Promise.resolve([aHostname]);
    }

    // Wrap gDNSService.asyncResolve to a promise, which
    // resolves with an array of ip addresses or rejects with
    // the reason otherwise.
    let hostResolveWrapper = () => {
      return new Promise((aResolve, aReject) => {
        // Callback for gDNSService.asyncResolve.
        let dnsListener = {
          onLookupComplete(aRequest, aRecord, aStatus) {
            if (!Components.isSuccessCode(aStatus)) {
              aReject(
                new Error(
                  "Failed to resolve '" +
                    aHostname +
                    "', with status: " +
                    aStatus
                )
              );
              return;
            }
            aRecord.QueryInterface(Ci.nsIDNSAddrRecord);

            let retval = [];
            while (aRecord.hasMore()) {
              retval.push(aRecord.getNextAddrAsString());
            }

            if (!retval.length) {
              aReject(new Error("No valid address after DNS lookup!"));
              return;
            }

            debug("hostname is resolved: " + aHostname);
            debug("Addresses: " + JSON.stringify(retval));

            aResolve(retval);
          },
        };
        debug("Calling Services.dns.asyncResolve: " + aHostname);
        Services.dns.asyncResolve(
          aHostname,
          Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
          0,
          null,
          dnsListener,
          Services.tm.mainThread,
          {}
        );
      });
    };

    return hostResolveWrapper();
  },

  _activeConnectionType: null,

  convertActiveConnectionType(aActiveNetworkInfo) {
    let activeConnectionType = null;

    if (aActiveNetworkInfo) {
      switch (aActiveNetworkInfo.type) {
        case Ci.nsINetworkInfo.NETWORK_TYPE_WIFI:
          activeConnectionType = CONNECTION_TYPE_WIFI;
          break;
        case Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE:
          activeConnectionType = CONNECTION_TYPE_CELLULAR;
          break;
        case Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET:
          activeConnectionType = CONNECTION_TYPE_ETHERNET;
          break;
        default:
          activeConnectionType = CONNECTION_TYPE_UNKNOWN;
      }
    } else {
      activeConnectionType = CONNECTION_TYPE_NONE;
    }

    if (activeConnectionType != this._activeConnectionType) {
      this._activeConnectionType = activeConnectionType;
      return activeConnectionType;
    }

    // If there's no active network type change, the function will
    // return null so that it won't trigger type change event in
    // NetworkInformation API. (Check nsAppShell.cpp)
    return null;
  },

  _setDNS(aNetworkInfo) {
    return new Promise((aResolve, aReject) => {
      let dnses = aNetworkInfo.getDnses();
      lazy.gNetworkService.setDNS(
        aNetworkInfo.name,
        dnses.length,
        dnses,
        aError => {
          if (aError) {
            aReject("setDNS failed");
            return;
          }
          aResolve();
        }
      );
    });
  },

  _setMtu(aNetwork) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.setMtu(
        aNetwork.info.name,
        aNetwork.mtu,
        aSuccess => {
          if (!aSuccess) {
            debug("setMtu failed");
          }
          // Always resolve.
          aResolve();
        }
      );
    });
  },

  _createNetwork(aInterfaceName, aNetworkType) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.createNetwork(
        aInterfaceName,
        aNetworkType,
        aSuccess => {
          if (!aSuccess) {
            aReject("createNetwork failed");
            return;
          }
          aResolve();
        }
      );
    });
  },

  _destroyNetwork(aInterfaceName, aNetworkType) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.destroyNetwork(
        aInterfaceName,
        aNetworkType,
        aSuccess => {
          if (!aSuccess) {
            debug("destroyNetwork failed");
          }
          // Always resolve.
          aResolve();
        }
      );
    });
  },

  _addInterfaceToNetwork(aNetId, aInterfaceName) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.addInterfaceToNetwork(
        aNetId,
        aInterfaceName,
        aSuccess => {
          if (!aSuccess) {
            aReject("_addInterfaceToNetwork failed");
            return;
          }
          aResolve();
        }
      );
    });
  },

  _removeInterfaceToNetwork(aNetId, aInterfaceName) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.removeInterfaceToNetwork(
        aNetId,
        aInterfaceName,
        aSuccess => {
          if (!aSuccess) {
            aReject("_removeInterfaceToNetwork failed");
            return;
          }
          aResolve();
        }
      );
    });
  },

  _resetRoutingTable(aInterfaceName) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.resetRoutingTable(aInterfaceName, aSuccess => {
        if (!aSuccess) {
          debug("resetRoutingTable failed");
        }
        // Always resolve.
        aResolve();
      });
    });
  },

  _updateDefaultRoute(aPreNetworkInfo, aExtNetworkInfo) {
    let removedLinks = [];
    let addedLinks = [];

    let promises = [];

    function getDifference(base, target) {
      return base.filter(function(i) {
        return !target.includes(i);
      });
    }

    if (aPreNetworkInfo && aExtNetworkInfo) {
      debug("_updateDefaultRoute for Iface=" + aExtNetworkInfo.name);
      removedLinks = getDifference(
        aPreNetworkInfo.getGateways(),
        aExtNetworkInfo.getGateways()
      );
      addedLinks = getDifference(
        aExtNetworkInfo.getGateways(),
        aPreNetworkInfo.getGateways()
      );
    } else if (aExtNetworkInfo) {
      debug("_updateDefaultRoute for Iface=" + aExtNetworkInfo.name);
      addedLinks = aExtNetworkInfo.getGateways();
    } else if (aPreNetworkInfo) {
      debug("_updateDefaultRoute for Iface=" + aPreNetworkInfo.name);
      removedLinks = aPreNetworkInfo.getGateways();
    } else {
      return Promise.resolve();
    }

    debug(
      "_updateDefaultRoute removedLinks=" +
        JSON.stringify(removedLinks) +
        " ,addedLinks=" +
        JSON.stringify(addedLinks)
    );

    if (removedLinks.length) {
      removedLinks.forEach(aGateway => {
        let removeDeferred = PromiseUtils.defer();
        promises.push(removeDeferred.promise);

        let removedLink = [];
        removedLink.push(aGateway);

        lazy.gNetworkService.removeDefaultRoute(
          aPreNetworkInfo.name,
          removedLink.length,
          removedLink,
          aSuccess => {
            if (!aSuccess) {
              debug(
                "_updateDefaultRoute remove failed for aGateway=" + removedLink
              );
            }
            // Always resolve.
            removeDeferred.resolve();
          }
        );
      });
    }

    if (addedLinks.length) {
      addedLinks.forEach(aGateway => {
        let setDeferred = PromiseUtils.defer();
        promises.push(setDeferred.promise);

        let addedLink = [];
        addedLink.push(aGateway);

        lazy.gNetworkService.setDefaultRoute(
          aExtNetworkInfo.name,
          addedLink.length,
          addedLink,
          aSuccess => {
            if (!aSuccess) {
              debug("_updateDefaultRoute add failed for aGateway=" + addedLink);
            }
            // Always resolve.
            setDeferred.resolve();
          }
        );
      });
    }

    return Promise.all(promises);
  },

  _setDefaultNetworkAndProxy(aNetwork) {
    return new Promise((aResolve, aReject) => {
      let networkInfo = aNetwork.info;

      lazy.gNetworkService.setDefaultNetwork(networkInfo.name, aSuccess => {
        this.setNetworkProxy(aNetwork);
        this.updateDNSProperty(networkInfo.dnses);
        aResolve();
      });
    });
  },

  _setTcpBufferSize(tcpbuffersizes) {
    return new Promise((aResolve, aReject) => {
      lazy.gNetworkService.setTcpBufferSize(tcpbuffersizes, aSuccess => {
        if (!aSuccess) {
          debug("setTcpBufferSize failed");
        }
        // Always resolve.
        aResolve();
      });
    });
  },

  updateClat(aNetworkInfo) {
    debug(
      "UpdateClat Type = " +
        convertToDataCallType(aNetworkInfo.type) +
        " , Iface =" +
        aNetworkInfo.name
    );
    if (
      aNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS ||
      (!this.isNetworkTypeMobile(aNetworkInfo.type) &&
        aNetworkInfo.type != Ci.nsINetworkInfo.NETWORK_TYPE_WIFI)
    ) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      // If no clat interface, create one.
      if (!(aNetworkInfo.name in this.networkNat464Links)) {
        this.networkNat464Links[aNetworkInfo.name] = new Nat464Xlat(
          aNetworkInfo
        );
      }

      let clatIfaceLink = this.networkNat464Links[aNetworkInfo.name];
      let wasRunning = clatIfaceLink != null && clatIfaceLink.isRunning();
      clatIfaceLink.requiresClat(aNetworkInfo).then(requireRun => {
        let shouldRun = clatIfaceLink.shouldRunClat();
        debug(
          "UpdateClat wasRunning = " +
            wasRunning +
            " , requireRun = " +
            requireRun +
            " , shouldRun = " +
            shouldRun
        );

        // Handle the muti types on the same clat Iface start request.
        if (
          wasRunning &&
          requireRun &&
          shouldRun &&
          !clatIfaceLink.types.includes(aNetworkInfo.type)
        ) {
          // Add type into the list but not re trigger clat.
          debug(
            "UpdateClat clatd already start for Iface=" +
              aNetworkInfo.name +
              " , add type =" +
              convertToDataCallType(aNetworkInfo.type) +
              " in to list."
          );
          clatIfaceLink.types.push(aNetworkInfo.type);

          // Config the clat info to networkinfo.
          aNetworkInfo.stackedLinkInfo = clatIfaceLink.stackedLinkInfo;
          aResolve();
          return;
        }

        // Handle the muti types on the same clat Iface stop request.
        if (
          wasRunning &&
          (!requireRun || !shouldRun) &&
          clatIfaceLink.types.length > 1
        ) {
          // There is other type using this clat interface.
          // Remove the type from the list but not stop the clat interface.
          debug(
            "Update clat Keep clatd for Iface=" +
              aNetworkInfo.name +
              " , remove type =" +
              convertToDataCallType(aNetworkInfo.type) +
              " from list."
          );
          clatIfaceLink.types.splice(
            clatIfaceLink.types.indexOf(aNetworkInfo.type),
            1
          );

          // Config the clat info to networkinfo.
          aNetworkInfo.stackedLinkInfo = clatIfaceLink.stackedLinkInfo;
          aResolve();
          return;
        }

        if (requireRun && !clatIfaceLink.isPrefixDiscoveryStarted()) {
          clatIfaceLink.setupPrefix64Discovery(true);
          aResolve();
        } else if (!wasRunning && requireRun && shouldRun) {
          // NAT64 prefix detected. Start clatd.
          // TODO: support the NAT64 prefix changing after it's been discovered. There is no
          // need to support this at the moment because it cannot happen without changes to
          // the Dns64Configuration code in netd.
          clatIfaceLink.start(aSuccess => {
            if (!aSuccess) {
              debug("start clatd fail.");
            }
            aResolve();
          });
        } else if (wasRunning && (!requireRun || !shouldRun)) {
          // NAT64 prefix removed. Stop clatd and go back into DISCOVERING state.
          clatIfaceLink.stop(aSuccess => {
            if (!aSuccess) {
              debug("stop clatd fail.");
            }
            aResolve();
          });
        } else {
          aResolve();
        }
      });
    });
  },

  updateClatInfoAndNotify(aIfaceName, aClatNetworkId) {
    debug(
      "updateClatInfoAndNotify aIfaceName=" +
        aIfaceName +
        " ,aClatNetworkId=" +
        aClatNetworkId
    );
    let clatIfaceLink = this.networkNat464Links[aIfaceName];
    let clatNetwork = this.networkInterfaces[aClatNetworkId];

    // Config the clat info to networkinfo.
    clatNetwork.info.stackedLinkInfo = clatIfaceLink.stackedLinkInfo;

    this.setAndConfigureActive().then(() => {
      // Notify outer modules like MmsService to start the transaction after
      // the configuration of the network interface is done.
      debug(
        "TOPIC_CONNECTION_STATE_CHANGED clat type=" +
          convertToDataCallType(clatNetwork.info.type) +
          ", networkinfo=" +
          JSON.stringify(clatNetwork.info)
      );
      Services.obs.notifyObservers(
        clatNetwork.info,
        TOPIC_CONNECTION_STATE_CHANGED
      );

      return lazy.gTetheringService.onExternalConnectionChanged(
        clatNetwork.info
      );
    });
  },

  setNetworkProxy(aNetwork) {
    try {
      if (!aNetwork.httpProxyHost || aNetwork.httpProxyHost === "") {
        // Sets direct connection to internet.
        this.clearNetworkProxy();

        debug(
          "No proxy support for " + aNetwork.info.name + " network interface."
        );
        return;
      }

      debug(
        "Going to set proxy settings for " +
          aNetwork.info.name +
          " network interface."
      );
      // Sets manual proxy configuration.
      Services.prefs.setIntPref(
        "network.proxy.type",
        MANUAL_PROXY_CONFIGURATION
      );

      // Do not use this proxy server for all protocols.
      Services.prefs.setBoolPref("network.proxy.share_proxy_settings", false);
      Services.prefs.setCharPref("network.proxy.http", aNetwork.httpProxyHost);
      Services.prefs.setCharPref("network.proxy.ssl", aNetwork.httpProxyHost);
      let port = aNetwork.httpProxyPort === 0 ? 8080 : aNetwork.httpProxyPort;
      Services.prefs.setIntPref("network.proxy.http_port", port);
      Services.prefs.setIntPref("network.proxy.ssl_port", port);
    } catch (ex) {
      debug(
        "Exception " +
          ex +
          ". Unable to set proxy setting for " +
          aNetwork.info.name +
          " network interface."
      );
    }
  },

  clearNetworkProxy() {
    debug("Going to clear all network proxy.");

    Services.prefs.clearUserPref("network.proxy.type");
    Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
    Services.prefs.clearUserPref("network.proxy.http");
    Services.prefs.clearUserPref("network.proxy.http_port");
    Services.prefs.clearUserPref("network.proxy.ssl");
    Services.prefs.clearUserPref("network.proxy.ssl_port");
  },
};

var CaptivePortalDetectionHelper = (function() {
  const EVENT_CONNECT = 1;
  const EVENT_DISCONNECT = 0;
  let _ongoingInterface = null;
  let _lastCaptivePortalStatus = {};
  let _available = "nsICaptivePortalDetector" in Ci;
  let getService = function() {
    return Cc["@mozilla.org/toolkit/captive-detector;1"].getService(
      Ci.nsICaptivePortalDetector
    );
  };

  let _performDetection = function(interfaceName, callback) {
    let capService = getService();
    let capCallback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICaptivePortalCallback]),
      prepare() {
        capService.finishPreparation(interfaceName);
      },
      complete(success) {
        _ongoingInterface = null;
        callback(success);
      },
    };

    // Abort any unfinished captive portal detection.
    if (_ongoingInterface != null) {
      capService.abort(_ongoingInterface);
      _ongoingInterface = null;
    }
    try {
      capService.checkCaptivePortal(interfaceName, capCallback);
      _ongoingInterface = interfaceName;
    } catch (e) {
      debug("Fail to detect captive portal due to: " + e.message);
    }
  };

  let _abort = function(interfaceName) {
    if (_ongoingInterface !== interfaceName) {
      return;
    }

    let capService = getService();
    capService.abort(_ongoingInterface);
    _ongoingInterface = null;
  };

  let _sendNotification = function(networkType, landing) {
    for (let index in NetworkManager.prototype._captivePortalLandings) {
      if (
        NetworkManager.prototype._captivePortalLandings[index].networkType !=
        networkType
      ) {
        continue;
      }

      if (
        landing ==
          NetworkManager.prototype._captivePortalLandings[index].landing ||
        (!landing &&
          !NetworkManager.prototype._captivePortalLandings[index].landing)
      ) {
        return;
      }
      NetworkManager.prototype._captivePortalLandings[index].landing = landing;
      let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag
      );
      propBag.setProperty("landing", landing);
      propBag.setProperty("networkType", networkType);
      Services.obs.notifyObservers(propBag, "captive-portal-result");
      BinderServices.connectivity.onCaptivePortalChanged(
        NetworkManager.prototype._captivePortalLandings[index]
      );
      break;
    }
  };

  return {
    EVENT_CONNECT,
    EVENT_DISCONNECT,
    notify(eventType, network) {
      if (
        !network ||
        !(network.type in NetworkManager.prototype._captivePortalSupportTypes)
      ) {
        // unsupportive network type.
        return;
      }
      switch (eventType) {
        case EVENT_CONNECT:
          // perform captive portal detection
          if (
            _available &&
            network &&
            _lastCaptivePortalStatus[network.type] != EVENT_CONNECT
          ) {
            _lastCaptivePortalStatus[network.type] = EVENT_CONNECT;
            _performDetection(network.name, function(success) {
              _sendNotification(network.type, success);
            });
          }

          break;
        case EVENT_DISCONNECT:
          if (
            _available &&
            network &&
            _lastCaptivePortalStatus[network.type] == EVENT_CONNECT
          ) {
            _lastCaptivePortalStatus[network.type] = EVENT_DISCONNECT;
            _abort(network.name);
            _sendNotification(network.type, false);
          }
          break;
      }
    },
  };
})();

XPCOMUtils.defineLazyGetter(NetworkManager.prototype, "mRil", function() {
  try {
    return Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}

  return null;
});
