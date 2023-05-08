/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const NETWORKSERVICE_CID = Components.ID(
  "{48c13741-aec9-4a86-8962-432011708261}"
);

const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkWorker",
  "@mozilla.org/network/worker;1",
  "nsINetworkWorker"
);

const WIFI_CTRL_INTERFACE = "wl0.1";

var DEBUG = false;
function updateDebug() {
  try {
    DEBUG = DEBUG || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}
}

function debug(s) {
  if (DEBUG) {
    console.log("-*- NetworkService: ", s, "\n");
  }
}
updateDebug();

function Task(aId, aParams, aSetupFunction) {
  this.id = aId;
  this.params = aParams;
  this.setupFunction = aSetupFunction;
}

function NetworkWorkerRequestQueue(aNetworkService) {
  this.networkService = aNetworkService;
  this.tasks = [];
}
NetworkWorkerRequestQueue.prototype = {
  runQueue() {
    if (this.tasks.length === 0) {
      return;
    }

    let task = this.tasks[0];
    debug("run task id: " + task.id);

    if (typeof task.setupFunction === "function") {
      // If setupFunction returns false, skip sending to Network Worker but call
      // handleWorkerMessage() directly with task id, as if the response was
      // returned from Network Worker.
      if (!task.setupFunction()) {
        this.networkService.handleWorkerMessage({ id: task.id });
        return;
      }
    }

    lazy.gNetworkWorker.postMessage(task.params);
  },

  enqueue(aId, aParams, aSetupFunction) {
    debug("enqueue id: " + aId);
    this.tasks.push(new Task(aId, aParams, aSetupFunction));

    if (this.tasks.length === 1) {
      this.runQueue();
    }
  },

  dequeue(aId) {
    debug("dequeue id: " + aId);

    if (!this.tasks.length || this.tasks[0].id != aId) {
      debug("Id " + aId + " is not on top of the queue");
      return;
    }

    this.tasks.shift();
    if (this.tasks.length) {
      // Run queue on the next tick.
      Services.tm.currentThread.dispatch(() => {
        this.runQueue();
      }, Ci.nsIThread.DISPATCH_NORMAL);
    }
  },
};

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
export function NetworkService() {
  debug("Starting NetworkService.");

  let self = this;

  if (lazy.gNetworkWorker) {
    let networkListener = {
      onEvent(aEvent) {
        self.handleWorkerMessage(aEvent);
      },
    };
    lazy.gNetworkWorker.start(networkListener);
  }
  // Callbacks to invoke when a reply arrives from the net_worker.
  this.controlCallbacks = Object.create(null);

  this.addedRoutes = new Map();
  this.netWorkerRequestQueue = new NetworkWorkerRequestQueue(this);
  this.shutdown = false;
  this.trafficStats = Cc["@mozilla.org/network/trafficstats;1"].createInstance(
    Ci.nsITrafficStats
  );

  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
}

NetworkService.prototype = {
  classID: NETWORKSERVICE_CID,
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINetworkService,
    Ci.nsIObserver,
  ]),

  addedRoutes: null,

  shutdown: false,

  // nsIObserver

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        debug("NetworkService shutdown");
        this.shutdown = true;
        if (lazy.gNetworkWorker) {
          lazy.gNetworkWorker.shutdown();
          lazy.gNetworkWorker = null;
        }

        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        break;
    }
  },

  // Helpers

  idgen: 0,
  controlMessage(aParams, aCallback, aSetupFunction) {
    if (this.shutdown) {
      return;
    }

    let id = this.idgen++;
    aParams.id = id;
    if (aCallback) {
      this.controlCallbacks[id] = aCallback;
    }

    // For now, we use aSetupFunction to determine if this command needs to be
    // queued or not.
    if (aSetupFunction) {
      this.netWorkerRequestQueue.enqueue(id, aParams, aSetupFunction);
      return;
    }

    if (lazy.gNetworkWorker) {
      lazy.gNetworkWorker.postMessage(aParams);
    }
  },

  handleWorkerMessage(aResponse) {
    debug(
      "NetworkService received message from worker: " +
        JSON.stringify(aResponse)
    );
    let id = aResponse.id;
    if (aResponse.broadcast === true) {
      Services.obs.notifyObservers(null, aResponse.topic, aResponse.reason);
      return;
    }
    let callback = this.controlCallbacks[id];
    if (callback) {
      callback.call(this, aResponse);
      delete this.controlCallbacks[id];
    }

    this.netWorkerRequestQueue.dequeue(id);
  },

  // nsINetworkService

  getNetworkInterfaceStats(aInterface, aCallback) {
    debug("getNetworkInterfaceStats for " + JSON.stringify(aInterface));
    let interfaceStats = [];
    if (this.trafficStats) {
      let statsInfos = {};
      this.trafficStats.getStats(statsInfos);
      let now = Date.now();
      if (aInterface.length) {
        for (let i = 0; i < statsInfos.value.length; i++) {
          if (aInterface == statsInfos.value[i].name) {
            interfaceStats.push({
              name: statsInfos.value[i].name,
              rxBytes: statsInfos.value[i].rxBytes,
              txBytes: statsInfos.value[i].txBytes,
            });
            break;
          }
        }
      } else {
        statsInfos.value.forEach(function(value) {
          interfaceStats.push({
            name: value.name,
            rxBytes: value.rxBytes,
            txBytes: value.txBytes,
          });
        });
      }
      aCallback.networkStatsAvailable(true, interfaceStats, now);
      return;
    }

    // legacy parse kernel node.
    let file = new FileUtils.File("/proc/net/dev");
    if (!file) {
      aCallback.networkStatsAvailable(false, interfaceStats, Date.now());
      return;
    }

    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(file),
        loadUsingSystemPrincipal: true,
      },
      function(inputStream, status) {
        interfaceStats = [];
        let now = Date.now();

        if (Components.isSuccessCode(status)) {
          // Find record for corresponding interface.
          let statExpr = /(\S+): +(\d+) +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +(\d+) +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +\d+/;
          let data = NetUtil.readInputStreamToString(
            inputStream,
            inputStream.available()
          ).split("\n");
          for (let i = 2; i < data.length; i++) {
            let parseResult = statExpr.exec(data[i]);
            if (parseResult && aInterface == parseResult[1]) {
              interfaceStats.push({
                name: parseResult[1],
                rxBytes: parseInt(parseResult[2], 10),
                txBytes: parseInt(parseResult[3], 10),
              });
            }
          }
        }

        // netd always return success even interface doesn't exist.
        aCallback.networkStatsAvailable(true, interfaceStats, now);
      }
    );
  },

  setNetworkTetheringAlarm(aEnable, aInterface) {
    // Method called when enabling disabling tethering, it checks if there is
    // some alarm active and move from interfaceAlarm to globalAlarm because
    // interfaceAlarm doens't work in tethering scenario due to forwarding.
    debug("setNetworkTetheringAlarm for tethering" + aEnable);

    let filename = aEnable
      ? "/proc/net/xt_quota/" + aInterface + "Alert"
      : "/proc/net/xt_quota/globalAlert";

    let file = new FileUtils.File(filename);
    if (!file) {
      return;
    }

    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(file),
        loadUsingSystemPrincipal: true,
      },
      (inputStream, status) => {
        if (Components.isSuccessCode(status)) {
          let data = NetUtil.readInputStreamToString(
            inputStream,
            inputStream.available()
          ).split("\n");
          if (data) {
            let threshold = parseInt(data[0], 10);

            this._setNetworkTetheringAlarm(aEnable, aInterface, threshold);
          }
        }
      }
    );
  },

  _setNetworkTetheringAlarm(aEnable, aInterface, aThreshold, aCallback) {
    debug("_setNetworkTetheringAlarm for tethering" + aEnable);

    let command = aEnable ? "setTetheringAlarm" : "removeTetheringAlarm";

    let params = {
      cmd: command,
      ifname: aInterface,
      threshold: aThreshold,
    };

    this.controlMessage(params, function(aData) {
      let result = aData.result;
      let enableString = aEnable ? "Enable" : "Disable";
      debug(enableString + " tethering Alarm result: " + result + " reason ");
      if (aCallback) {
        aCallback.networkUsageAlarmResult(null);
      }
    });
  },

  setNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback) {
    if (!aInterfaceName) {
      aCallback.networkUsageAlarmResult(-1);
      return;
    }

    let self = this;
    this._disableNetworkInterfaceAlarm(aInterfaceName, function(aResult) {
      if (aThreshold < 0) {
        if (!aResult.result) {
          aCallback.networkUsageAlarmResult(null);
          return;
        }
        aCallback.networkUsageAlarmResult(aResult.reason);
        return;
      }

      // Check if tethering is enabled
      let params = {
        cmd: "getTetheringStatus",
      };

      self.controlMessage(params, function(aResult) {
        if (!aResult.result) {
          // Tethering disabled, set interfaceAlarm
          // TODO: We shall prevent enable same network alarm again and again.
          self._enableNetworkInterfaceAlarm(
            aInterfaceName,
            aThreshold,
            aCallback
          );
          return;
        }

        // Tethering enabled, set globalAlarm
        self._setNetworkTetheringAlarm(
          true,
          aInterfaceName,
          aThreshold,
          aCallback
        );
      });
    });
  },

  _setNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback) {
    debug(
      "setNetworkInterfaceAlarm for " +
        aInterfaceName +
        " at " +
        aThreshold +
        "bytes"
    );

    let params = {
      cmd: "setNetworkInterfaceAlarm",
      ifname: aInterfaceName,
      threshold: aThreshold,
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      if (!aResult.result) {
        aCallback.networkUsageAlarmResult("setNetworkInterfaceAlarm failed");
        return;
      }

      aCallback.networkUsageAlarmResult(null);
    });
  },

  _enableNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback) {
    debug(
      "enableNetworkInterfaceAlarm for " +
        aInterfaceName +
        " at " +
        aThreshold +
        "bytes"
    );

    let params = {
      cmd: "enableNetworkInterfaceAlarm",
      ifname: aInterfaceName,
      threshold: aThreshold,
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      if (!aResult.result) {
        aCallback.networkUsageAlarmResult("enableNetworkInterfaceAlarm failed");
        return;
      }
      this._setNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback);
    });
  },

  _disableNetworkInterfaceAlarm(aInterfaceName, aCallback) {
    debug("disableNetworkInterfaceAlarm for " + aInterfaceName);

    let params = {
      cmd: "disableNetworkInterfaceAlarm",
      ifname: aInterfaceName,
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      aCallback(aResult);
    });
  },

  resetRoutingTable(aInterfaceName, aCallback) {
    let options = {
      cmd: "removeNetworkRoute",
      ifname: aInterfaceName,
    };

    let handleAddedRoute = this.addedRoutes;
    let cleanTargetRoute = (value, key, map) => {
      if (aInterfaceName && key.includes(aInterfaceName)) {
        this.addedRoutes.delete(key);
      }
    };

    this.controlMessage(options, function(aResult) {
      if (!aResult.error) {
        handleAddedRoute.forEach(cleanTargetRoute);
      }
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  setDNS(aInterfaceName, aDnsesCount, aDnses, aCallback) {
    debug("Going to set DNS to " + aInterfaceName);
    let options = {
      cmd: "setDNS",
      ifname: aInterfaceName,
      /*
       ** Set null for this domain value. It can't get the correct host ip even add this domain value for DNS query.
       ** example: Query DNS: "www.abcd"
       ** If DNS server did not response the host ip for the "www.abcd" domain name.
       ** Device will try to query again the domain name with "www.abcd.mozilla.aInterfaceName.domain"
       ** And expect, server will response the correct host ip address.
       */
      // domain: "mozilla." + aInterfaceName + ".domain",
      domain: null,
      dnses: aDnses,
    };
    this.controlMessage(options, function(aResult) {
      aCallback.setDnsResult(aResult.error ? aResult.reason : null);
    });
  },

  /*
   * Set default network(WIFI/Cellular/Ethernet)
   */
  setDefaultNetwork(aInterfaceName, aCallback) {
    debug("Going to change default network to " + aInterfaceName);
    let options = {
      cmd: "setDefaultNetwork",
      ifname: aInterfaceName,
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  setDefaultRoute(aInterfaceName, aCount, aGateways, aCallback) {
    // NOTE: This API currently only can handle single gataway config.
    // TODO: Change the IDL interface for single gateway config.
    if (aGateways && aGateways.length > 1) {
      debug("setDefaultRoute not allow to config the muti Gateways.");
      aCallback.nativeCommandResult(false);
      return;
    }

    debug(
      "Going to change default route for " +
        aInterfaceName +
        " , aGateways=" +
        aGateways
    );
    let options = {
      cmd: "setDefaultRoute",
      ifname: aInterfaceName,
      gateways: aGateways,
    };

    let route = this._routeToString(aInterfaceName, null, null, aGateways);
    let count = this.addedRoutes.get(route);

    let setupFunc = () => {
      debug("setDefaultRoute: " + route + " -> " + count);

      // Return false if there is no need to send the command to network worker.
      if (count) {
        return false;
      }
      return true;
    };

    this.controlMessage(
      options,
      function(aResult) {
        if (!aResult.error) {
          this.addedRoutes.set(route, count ? count + 1 : 1);
        }
        aCallback.nativeCommandResult(!aResult.error);
      },
      setupFunc
    );
  },

  removeDefaultRoute(aInterfaceName, aCount, aGateways, aCallback) {
    // NOTE: This API currently only can handle single gataway config.
    // TODO: Change the IDL interface for single gateway config.
    if (aGateways && aGateways.length > 1) {
      debug("removeDefaultRoute not allow to config the muti Gateways.");
      aCallback.nativeCommandResult(false);
      return;
    }

    debug(
      "Remove default route for " + aInterfaceName + " , aGateways=" + aGateways
    );
    let options = {
      cmd: "removeDefaultRoute",
      ifname: aInterfaceName,
      gateways: aGateways,
    };

    let route = this._routeToString(aInterfaceName, null, null, aGateways);
    let count = this.addedRoutes.get(route);

    let setupFunc = () => {
      debug("removeDefaultRoute: " + route + " -> " + count);

      // Return false if there is no need to send the command to network worker.
      if (!count || count > 1) {
        return false;
      }
      return true;
    };

    this.controlMessage(
      options,
      function(aResult) {
        // Redeuce the count even the kernel fail to remove the route.
        if (count > 1) {
          this.addedRoutes.set(route, count - 1);
        } else {
          this.addedRoutes.delete(route);
        }
        aCallback.nativeCommandResult(!aResult.error);
      },
      setupFunc
    );
  },

  _routeToString(aInterfaceName, aHost, aPrefixLength, aGateway) {
    return aHost + "-" + aPrefixLength + "-" + aGateway + "-" + aInterfaceName;
  },

  modifyRoute(aAction, aInterfaceName, aHost, aPrefixLength, aGateway) {
    let command;

    switch (aAction) {
      case Ci.nsINetworkService.MODIFY_ROUTE_ADD:
        command = "addHostRoute";
        break;
      case Ci.nsINetworkService.MODIFY_ROUTE_REMOVE:
        command = "removeHostRoute";
        break;
      default:
        debug("Unknown action: " + aAction);
        return Promise.reject();
    }

    let route = this._routeToString(
      aInterfaceName,
      aHost,
      aPrefixLength,
      aGateway
    );
    let setupFunc = () => {
      let count = this.addedRoutes.get(route);
      debug(command + ": " + route + " -> " + count);

      // Return false if there is no need to send the command to network worker.
      if (
        (aAction == Ci.nsINetworkService.MODIFY_ROUTE_ADD && count) ||
        (aAction == Ci.nsINetworkService.MODIFY_ROUTE_REMOVE &&
          (!count || count > 1))
      ) {
        return false;
      }

      return true;
    };

    debug(command + " " + aHost + " on " + aInterfaceName);
    let options = {
      cmd: command,
      ifname: aInterfaceName,
      gateway: aGateway,
      prefixLength: aPrefixLength,
      ip: aHost,
    };

    return new Promise((aResolve, aReject) => {
      this.controlMessage(
        options,
        aData => {
          let count = this.addedRoutes.get(route);

          // Remove route from addedRoutes on success or failure.
          if (aAction == Ci.nsINetworkService.MODIFY_ROUTE_REMOVE) {
            if (count > 1) {
              this.addedRoutes.set(route, count - 1);
            } else {
              this.addedRoutes.delete(route);
            }
          }

          if (aData.error) {
            aReject(aData.reason);
            return;
          }

          if (aAction == Ci.nsINetworkService.MODIFY_ROUTE_ADD) {
            this.addedRoutes.set(route, count ? count + 1 : 1);
          }

          aResolve();
        },
        setupFunc
      );
    });
  },

  addSecondaryRoute(aInterfaceName, aRoute, aCallback) {
    debug("Going to add route to secondary table on " + aInterfaceName);
    let options = {
      cmd: "addSecondaryRoute",
      ifname: aInterfaceName,
      ip: aRoute.ip,
      prefix: aRoute.prefix,
      gateway: aRoute.gateway,
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  removeSecondaryRoute(aInterfaceName, aRoute, aCallback) {
    debug("Going to remove route from secondary table on " + aInterfaceName);
    let options = {
      cmd: "removeSecondaryRoute",
      ifname: aInterfaceName,
      ip: aRoute.ip,
      prefix: aRoute.prefix,
      gateway: aRoute.gateway,
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  // Enable/Disable DHCP server.
  setDhcpServer(aEnabled, aConfig, aCallback) {
    if (null === aConfig) {
      aConfig = {};
    }

    aConfig.cmd = "setDhcpServer";
    aConfig.enabled = aEnabled;

    this.controlMessage(aConfig, function(aResponse) {
      if (!aResponse.result) {
        aCallback.dhcpServerResult("Set DHCP server error");
        return;
      }
      aCallback.dhcpServerResult(null);
    });
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering(aEnable, aConfig, aCallback) {
    // config should've already contained:
    //   .ifname
    //   .internalIfname
    //   .externalIfname
    aConfig.wifictrlinterfacename = WIFI_CTRL_INTERFACE;
    aConfig.cmd = "setWifiTethering";

    // The callback function in controlMessage may not be fired immediately.
    this.controlMessage(aConfig, aData => {
      let result = aData.result;
      let reason = aData.resultReason;
      let enableString = aEnable ? "Enable" : "Disable";

      debug(
        enableString +
          " Wifi tethering result: " +
          result +
          " reason: " +
          reason
      );

      this.setNetworkTetheringAlarm(aEnable, aConfig.externalIfname);

      if (!result) {
        aCallback.wifiTetheringEnabledChange("netd command error");
      } else {
        aCallback.wifiTetheringEnabledChange(null);
      }
    });
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering(aEnable, aConfig, aMsgCallback, aCallback) {
    aConfig.cmd = "setUSBTethering";
    // The callback function in controlMessage may not be fired immediately.
    this.controlMessage(aConfig, aData => {
      let result = aData.result;
      let enableString = aEnable ? "Enable" : "Disable";

      debug(enableString + " USB tethering result: " + result);

      this.setNetworkTetheringAlarm(aEnable, aConfig.externalIfname);

      if (!result) {
        aCallback.usbTetheringEnabledChange("netd command error", aMsgCallback);
      } else {
        aCallback.usbTetheringEnabledChange(null, aMsgCallback);
      }
    });
  },

  removeUpStream(aConfig, aCallback) {
    let params = {
      cmd: "removeUpStream",
      preInternalIfname: aConfig.internalIfname,
      preExternalIfname: aConfig.externalIfname,
      ipv6Ip: aConfig.ipv6Ip,
    };
    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  updateUpStream(aType, aPrevious, aCurrent, aCallback) {
    let params = {
      cmd: "updateUpStream",
      type: aType,
      preInternalIfname: aPrevious.internalIfname,
      preExternalIfname: aPrevious.externalIfname,
      curInternalIfname: aCurrent.internalIfname,
      curExternalIfname: aCurrent.externalIfname,
      dns1: aCurrent.dns1,
      dns2: aCurrent.dns2,
      dnses: aCurrent.dnses,
      ipv6Ip: aCurrent.ipv6Ip,
    };

    this.controlMessage(params, function(aData) {
      let error = aData.error;
      debug("updateUpStream result: " + !error);
      aCallback.updateUpStreamResult(aType, !error, aData.curExternalIfname);
    });
  },

  getInterfaces(callback) {
    let params = {
      cmd: "getInterfaces",
      isAsync: true,
    };

    this.controlMessage(params, function(data) {
      debug("getInterfaces result: " + JSON.stringify(data));
      callback.getInterfacesResult(data.result, data.interfaceList);
    });
  },

  getInterfaceConfig(interfaceName, callback) {
    let params = {
      cmd: "getInterfaceConfig",
      ifname: interfaceName,
      isAsync: true,
    };

    this.controlMessage(params, function(data) {
      debug("getInterfaceConfig result: " + JSON.stringify(data));
      let result = {
        ip: data.ipAddr,
        prefix: data.prefixLength,
        link: data.flag,
        mac: data.macAddr,
      };
      callback.getInterfaceConfigResult(data.result, result);
    });
  },

  setInterfaceConfig(config, callback) {
    config.cmd = "setInterfaceConfig";
    config.isAsync = true;

    this.controlMessage(config, function(data) {
      debug("setInterfaceConfig result: " + JSON.stringify(data));
      callback.setInterfaceConfigResult(data.result);
    });
  },

  dhcpRequest(aInterfaceName, aCallback) {
    let params = {
      cmd: "dhcpRequest",
      ifname: aInterfaceName,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.dhcpRequestResult(
        !aResult.error,
        aResult.error ? null : aResult
      );
    });
  },

  stopDhcp(aInterfaceName, aCallback) {
    let params = {
      cmd: "stopDhcp",
      ifname: aInterfaceName,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  createNetwork(aInterfaceName, aNetworkType, aCallback) {
    let params = {
      cmd: "createNetwork",
      ifname: aInterfaceName,
      networkType: aNetworkType,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  destroyNetwork(aInterfaceName, aNetworkType, aCallback) {
    let params = {
      cmd: "destroyNetwork",
      ifname: aInterfaceName,
      networkType: aNetworkType,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  addInterfaceToNetwork(aNetId, aInterfaceName, aCallback) {
    let params = {
      cmd: "addInterfaceToNetwork",
      netId: aNetId,
      ifname: aInterfaceName,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  removeInterfaceToNetwork(aNetId, aInterfaceName, aCallback) {
    let params = {
      cmd: "removeInterfaceToNetwork",
      netId: aNetId,
      ifname: aInterfaceName,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  getNetId(aInterfaceName) {
    let params = {
      cmd: "getNetId",
      ifname: aInterfaceName,
    };

    return new Promise((aResolve, aReject) => {
      this.controlMessage(params, result => {
        if (!result.result) {
          aReject(result.reason);
          return;
        }
        aResolve(result.netId);
      });
    });
  },

  setIpv6Status(aInterfaceName, aEnable, aCallback) {
    debug(
      "setIpv6Status: interfaceName = " +
        aInterfaceName +
        ", enable = " +
        aEnable
    );
    let params = {
      cmd: "setIpv6Status",
      ifname: aInterfaceName,
      enable: aEnable,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  setMtu(aInterfaceName, aMtu, aCallback) {
    debug("Set MTU on " + aInterfaceName + ": " + aMtu);

    let params = {
      cmd: "setMtu",
      ifname: aInterfaceName,
      mtu: aMtu,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  startClatd(interfaceName, nat64Prefix, callback) {
    let params = {
      cmd: "startClatd",
      ifname: interfaceName,
      nat64Prefix,
    };

    this.controlMessage(params, aResult => {
      callback.startClatdResult(aResult.result, aResult.clatdAddress);
    });
  },

  stopClatd(interfaceName, callback) {
    let params = {
      cmd: "stopClatd",
      ifname: interfaceName,
    };

    this.controlMessage(params, aResult => {
      callback.nativeCommandResult(aResult.result);
    });
  },

  setupPrefix64Discovery(aInterfaceName, aEnable, aCallback) {
    let options = {
      cmd: "setupPrefix64Discovery",
      ifname: aInterfaceName,
      enable: aEnable,
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  setTcpBufferSize(aTcpBufferSizes, aCallback) {
    debug("setTcpBufferSize = " + aTcpBufferSizes);
    let params = {
      cmd: "setTcpBufferSize",
      tcpBufferSizes: aTcpBufferSizes,
    };

    this.controlMessage(params, aResult => {
      aCallback.nativeCommandResult(aResult.result);
    });
  },

  getTetherStats(aCallback) {
    let params = {
      cmd: "getTetherStats",
    };

    this.controlMessage(params, aResult => {
      debug("getTetherStats result: " + JSON.stringify(aResult.tetherStats));
      aCallback.getTetherStatsResult(aResult.result, aResult.tetherStats);
    });
  },
};
