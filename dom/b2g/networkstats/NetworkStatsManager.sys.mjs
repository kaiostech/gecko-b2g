/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";
const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";

var DEBUG = false;
function updateDebug() {
  try {
    DEBUG = DEBUG || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}
}

function debug(s) {
  if (DEBUG) {
    console.log("-*- NetworkStatsManager: ", s, "\n");
  }
}
updateDebug();

// Ensure NetworkStatsService and NetworkStatsDB are loaded in the parent process
// to receive messages from the child processes.
const isGonk = AppConstants.platform === "gonk";
var isParentProcess =
  Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
if (isParentProcess) {
  if (isGonk) {
    ChromeUtils.importESModule(
      "resource://gre/modules/NetworkStatsService.sys.mjs"
    );
  }
}

const lazy = {};
XPCOMUtils.defineLazyGetter(lazy, "cpmm", () => {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
});

// NetworkStatsData
const NETWORKSTATSDATA_CID = Components.ID(
  "{3b16fe17-5583-483a-b486-b64a3243221c}"
);

export function NetworkStatsData(aWindow, aData) {
  this.rxBytes = aData.rxBytes;
  this.txBytes = aData.txBytes;
  this.date = new aWindow.Date(aData.date.getTime());
}

NetworkStatsData.prototype = {
  classID: NETWORKSTATSDATA_CID,

  QueryInterface: ChromeUtils.generateQI([]),
};

// NetworkStatsInterface
const NETWORKSTATSINTERFACE_CONTRACTID = "@mozilla.org/networkstatsinterface;1";
const NETWORKSTATSINTERFACE_CID = Components.ID(
  "{f540615b-d803-43ff-8200-2a9d145a5645}"
);

export function NetworkStatsInterface() {
  debug("NetworkStatsInterface Constructor");
}

NetworkStatsInterface.prototype = {
  __init(aNetwork) {
    this.type = aNetwork.type;
    this.id = aNetwork.id;
  },

  classID: NETWORKSTATSINTERFACE_CID,

  contractID: NETWORKSTATSINTERFACE_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([]),
};

// NetworkStats
const NETWORKSTATS_CID = Components.ID(
  "{28904f59-8497-4ac0-904f-2af14b7fd3de}"
);

export function NetworkStats(aWindow, aStats) {
  debug("NetworkStats Constructor");
  this.appManifestURL = aStats.appManifestURL || null;
  this.serviceType = aStats.serviceType || null;
  this.network = new aWindow.NetworkStatsInterface(aStats.network);
  this.start = aStats.start ? new aWindow.Date(aStats.start.getTime()) : null;
  this.end = aStats.end ? new aWindow.Date(aStats.end.getTime()) : null;

  let samples = (this.data = new aWindow.Array());
  for (let i = 0; i < aStats.data.length; i++) {
    samples.push(
      aWindow.NetworkStatsData._create(
        aWindow,
        new NetworkStatsData(aWindow, aStats.data[i])
      )
    );
  }
}

NetworkStats.prototype = {
  classID: NETWORKSTATS_CID,

  QueryInterface: ChromeUtils.generateQI([]),

  data: {},

  getData() {
    if (this.data) {
      return this.data;
    }
    return {};
  },
};

// NetworkStatsAlarm
const NETWORKSTATSALARM_CID = Components.ID(
  "{a93ea13e-409c-4189-9b1e-95fff220be55}"
);

export function NetworkStatsAlarm(aWindow, aAlarm) {
  this.alarmId = aAlarm.id;
  this.network = new aWindow.NetworkStatsInterface(aAlarm.network);
  this.threshold = aAlarm.threshold;
  this.data = aAlarm.data;
}

NetworkStatsAlarm.prototype = {
  classID: NETWORKSTATSALARM_CID,

  QueryInterface: ChromeUtils.generateQI([]),
};

// NetworkStatsManager

const NETWORKSTATSMANAGER_CONTRACTID = "@mozilla.org/networkStatsManager;1";
const NETWORKSTATSMANAGER_CID = Components.ID(
  "{ceb874cd-cc1a-4e65-b404-cc2d3e42425f}"
);

export function NetworkStatsManager() {
  debug("Constructor");
  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
}

NetworkStatsManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  getSamples: function getSamples(aNetwork, aStart, aEnd, aOptions) {
    if (aStart > aEnd) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    // appManifestURL is used to query network statistics by app;
    // serviceType is used to query network statistics by system service.
    // It is illegal to specify both of them at the same time.
    if (aOptions.appManifestURL && aOptions.serviceType) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    }

    let appManifestURL = aOptions.appManifestURL;
    let serviceType = aOptions.serviceType;

    // TODO Bug 929410 Date object cannot correctly pass through cpmm/ppmm IPC
    // This is just a work-around by passing timestamp numbers.
    aStart = aStart.getTime();
    aEnd = aEnd.getTime();

    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:Get", {
      network: aNetwork.toJSON(),
      start: aStart,
      end: aEnd,
      appManifestURL,
      serviceType,
      id: this.getRequestId(request),
    });
    return request;
  },

  clearStats: function clearStats(aNetwork) {
    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:Clear", {
      network: aNetwork.toJSON(),
      id: this.getRequestId(request),
    });
    return request;
  },

  clearAllStats: function clearAllStats() {
    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:ClearAll", {
      id: this.getRequestId(request),
    });
    return request;
  },

  addAlarm: function addAlarm(aNetwork, aThreshold, aOptions) {
    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:SetAlarm", {
      id: this.getRequestId(request),
      data: {
        network: aNetwork.toJSON(),
        threshold: aThreshold,
        startTime: aOptions.startTime,
        data: aOptions.data,
        originURL: this.originURL,
        pageURL: this.pageURL,
      },
    });
    return request;
  },

  getAllAlarms: function getAllAlarms(aNetwork) {
    let network = null;
    if (aNetwork) {
      network = aNetwork.toJSON();
    }

    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:GetAlarms", {
      id: this.getRequestId(request),
      data: { network, originURL: this.originURL },
    });
    return request;
  },

  removeAlarms: function removeAlarms(aAlarmId) {
    if (aAlarmId == 0) {
      aAlarmId = -1;
    }

    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:RemoveAlarms", {
      id: this.getRequestId(request),
      data: { alarmId: aAlarmId, originURL: this.originURL },
    });

    return request;
  },

  getAvailableNetworks: function getAvailableNetworks() {
    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:GetAvailableNetworks", {
      id: this.getRequestId(request),
    });
    return request;
  },

  getAvailableServiceTypes: function getAvailableServiceTypes() {
    let request = this.createRequest();
    lazy.cpmm.sendAsyncMessage("NetworkStats:GetAvailableServiceTypes", {
      id: this.getRequestId(request),
    });
    return request;
  },

  get sampleRate() {
    return lazy.cpmm.sendSyncMessage("NetworkStats:SampleRate")[0];
  },

  get maxStorageAge() {
    return lazy.cpmm.sendSyncMessage("NetworkStats:MaxStorageAge")[0];
  },

  receiveMessage(aMessage) {
    debug("NetworkStatsmanager::receiveMessage: " + aMessage.name);

    let msg = aMessage.json;
    let req = this.takeRequest(msg.id);
    if (!req) {
      debug("No request stored with id " + msg.id);
      return;
    }

    switch (aMessage.name) {
      case "NetworkStats:Get:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let result = this._window.NetworkStats._create(
          this._window,
          new NetworkStats(this._window, msg.result)
        );
        debug("result: " + JSON.stringify(result));
        Services.DOMRequest.fireSuccess(req, result);
        break;

      case "NetworkStats:GetAvailableNetworks:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let networks = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          let network = new this._window.NetworkStatsInterface(msg.result[i]);
          networks.push(network);
        }

        Services.DOMRequest.fireSuccess(req, networks);
        break;

      case "NetworkStats:GetAvailableServiceTypes:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let serviceTypes = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          serviceTypes.push(msg.result[i]);
        }

        Services.DOMRequest.fireSuccess(req, serviceTypes);
        break;

      case "NetworkStats:Clear:Return":
      case "NetworkStats:ClearAll:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        Services.DOMRequest.fireSuccess(req, true);
        break;

      case "NetworkStats:SetAlarm:Return":
      case "NetworkStats:RemoveAlarms:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        Services.DOMRequest.fireSuccess(req, msg.result);
        break;

      case "NetworkStats:GetAlarms:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let alarms = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          // The WebIDL type of data is any, so we should manually clone it
          // into the content window.
          if ("data" in msg.result[i]) {
            msg.result[i].data = Cu.cloneInto(msg.result[i].data, this._window);
          }
          let alarm = new NetworkStatsAlarm(this._window, msg.result[i]);
          alarms.push(
            this._window.NetworkStatsAlarm._create(this._window, alarm)
          );
        }

        Services.DOMRequest.fireSuccess(req, alarms);
        break;

      default:
        debug("Wrong message: " + aMessage.name);
    }
  },

  init(aWindow) {
    let principal = aWindow.document.nodePrincipal;

    this.initDOMRequestHelper(aWindow, [
      "NetworkStats:Get:Return",
      "NetworkStats:GetAvailableNetworks:Return",
      "NetworkStats:GetAvailableServiceTypes:Return",
      "NetworkStats:Clear:Return",
      "NetworkStats:ClearAll:Return",
      "NetworkStats:SetAlarm:Return",
      "NetworkStats:GetAlarms:Return",
      "NetworkStats:RemoveAlarms:Return",
    ]);

    this.originURL = principal.origin;
    let isApp =
      !!this.originURL.length && this.originURL != "[System Principal]";
    if (isApp) {
      this.pageURL = principal.URI.spec;
    }

    this.window = aWindow;
  },

  // nsIObserver
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        break;
    }
  },

  // Called from DOMRequestIpcHelper
  uninit: function uninit() {
    debug("uninit call");
  },

  classID: NETWORKSTATSMANAGER_CID,
  contractID: NETWORKSTATSMANAGER_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIDOMGlobalPropertyInitializer,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),
};
