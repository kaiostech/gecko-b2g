/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Subprocess: "resource://gre/modules/Subprocess.sys.mjs",
});

import {
  clearInterval,
  setInterval,
} from "resource://gre/modules/Timer.sys.mjs";

const kXpcomShutdownChangedTopic = "xpcom-shutdown";
const kScreenStateChangedTopic = "screen-state-changed";

const kScanInterval = Services.prefs.getIntPref("b2g.wifi.scan-interval", 30);

// A class encapsulating access to the NetworkManager library.
// TODO: use the actual library instead of calls to `nmcli`.
class NetworkManager {
  constructor(callbacks) {
    this.log(`constructor`);

    this.callbacks = callbacks;

    this.nmCliPath = Services.prefs.getCharPref(
      "b2g.wifi.nmcli-path",
      "/usr/bin/nmcli"
    );

    this._enabled = false;
    this.getState();
  }

  log(msg) {
    console.log(`WifiNetworkManager: ${msg}`);
  }

  get enabled() {
    return this._enabled;
  }

  parseNmOutput(line) {
    let out = [];

    let current = "";
    let len = line.length;
    let i = 0;
    while (i < len) {
      let c = line[i];
      if (c === "\\") {
        // peek on the next char to check if it's an escaped ':'
        if (line[i + 1] == ":") {
          current += ":";
          i++;
        } else {
          current += "\\";
        }
      } else if (c === ":") {
        // End of field.
        out.push(current);
        current = "";
      } else {
        current += c;
      }
      i++;
    }

    out.push(current);

    return out;
  }

  // Runs the `nmcli $parameters` command.
  async runCommand(parameters, { labels = null, ignoreExitCode = false } = {}) {
    // No color output, and terse display.
    let commonArgs = ["-c", "no", "-t"];
    let args = parameters.trim().split(" ");

    let proc = await lazy.Subprocess.call({
      command: this.nmCliPath,
      arguments: commonArgs.concat(args),
      stderr: "stdout",
    });

    let { exitCode } = await proc.wait();
    if (exitCode != 0 && !ignoreExitCode) {
      throw new Components.Exception(
        `Failed to run '${this.nmCliPath} ${parameters}': ${exitCode}`,
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    let stdout = await proc.stdout.readString();

    let lines = stdout.split(/\r\n|\n|\r/);
    let result = [];
    for (let line of lines) {
      // Each line is a set of ':' separated fields. However since fields can include
      // escaped ':' characters we can't just split() them.
      if (line.length) {
        let parsed = this.parseNmOutput(line);
        if (labels) {
          let obj = {};
          for (let i = 0; i < Math.min(labels.length, parsed.length); i++) {
            obj[labels[i]] = parsed[i];
          }
          result.push(obj);
        } else {
          result.push(line);
        }
        // this.log(`stdout: ${parsed}`);
      }
    }

    return result;
  }

  async getState() {
    // nmcli r wifi -> "enabled" or "disabled"
    let res = await this.runCommand("r wifi");
    let enabled = res[0] == "enabled";
    if (enabled !== this._enabled) {
      this._enabled = enabled;
      this.callbacks.enabledStateChange(enabled);
    }
  }

  async setEnabled(value) {
    await this.runCommand(`r wifi ${value ? "on" : "off"}`);
    // get the state to trigger wifiUp / wifiDown events as needed.
    await this.getState();
  }

  async getKnownNetworks() {
    let command = `-f TYPE,UUID,NAME con`;
    let networks = await this.runCommand(command, {
      labels: ["type", "uuid", "name"],
    });
    let res = [];
    for (let network of networks) {
      // Filter out non-wifi connections.
      if (!network.name.length || network.type !== "802-11-wireless") {
        continue;
      }

      let obj = network;

      obj.connected = false;
      obj.security = "OPEN";
      obj.ssid = network.name;
      // Needed to match the connection to forget.
      obj.bssid = network.uuid;

      res.push(obj);
    }

    return res;
  }

  async forget(network) {
    let command = `connection delete uuid ${network.bssid}`;
    await this.runCommand(command);
  }

  async getNetworks() {
    // nmcli d wifi list
    // Available fields: NAME,SSID,SSID-HEX,BSSID,MODE,CHAN,FREQ,RATE,SIGNAL,BARS,SECURITY,WPA-FLAGS,RSN-FLAGS,DEVICE,ACTIVE,IN-USE,DBUS-PATH

    let command = `-f SSID,BSSID,MODE,FREQ,SIGNAL,SECURITY,IN-USE d wifi list`;
    let networks = await this.runCommand(command, {
      labels: [
        "ssid",
        "bssid",
        "mode",
        "frequency",
        "signalStrength",
        "security",
        "connected",
      ],
    });
    let res = [];
    for (let network of networks) {
      // Convert the NetworkManager strings into valid WifiNetwork properties when needed.
      if (!network.ssid.length) {
        continue;
      }

      let obj = network;

      obj.connected = network.connected === "*";

      // TODO: better mapping of NetworkManager security to WifiManager.
      if (network.security === "") {
        obj.security = "OPEN";
      } else {
        let sec = network.security.split(" ").reverse()[0];
        if (sec.startsWith("WPA2")) {
          obj.security = "WPA2-PSK";
        } else if (sec.startsWith("WPA1")) {
          obj.security = "WPA-PSK";
        } else {
          obj.security = sec;
        }
      }

      // Frequency is expected to be a long in MHz, not a "5745 MHz" string.
      obj.frequency = parseInt(network.frequency.split(" ")[0], 10);

      if (obj.connected) {
        this.callbacks.connectedNetwork(obj);
      }
      res.push(obj);
    }

    return res;
  }

  async associate(network) {
    // nmcli d wifi connect $ssid [password $password]
    let command = `d wifi connect ${network.ssid}`;
    if (network.security !== "OPEN") {
      command += ` password ${network.password || network.psk}`;
    }
    let res = await this.runCommand(command, { ignoreExitCode: true });
    let status = res[0];
    this.log(`associate status: ${status}`);
    if (status.startsWith("Error:")) {
      throw new Error(false);
    } else {
      return true;
    }
  }

  // Enable or disable periodic scanning of network.
  // Interval is in seconds.
  configurePeriodicScan(enabled, interval) {
    this.log(`configurePeriodicScan ${enabled} ${interval}`);
    if (this.scanId) {
      clearInterval(this.scanId);
      this.scanId = null;
    }

    if (enabled) {
      this.scanId = setInterval(async () => {
        // let start = Date.now();
        // this.log(`Starting periodic scan`);
        await this.getNetworks();
        // this.log(
        //   `Periodic scan done in ${((Date.now() - start) / 1000).toFixed(1)}s`
        // );
      }, interval * 1000);
    }
  }
}

export class WifiApi {
  constructor() {
    this.log(`constructor`);

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

    Services.obs.addObserver(this, kXpcomShutdownChangedTopic);
    Services.obs.addObserver(this, kScreenStateChangedTopic);

    this._domManagers = [];
    this._domRequest = [];

    this.nm = new NetworkManager(this);
  }

  log(msg) {
    console.log(`WifiApi: ${msg}`);
  }

  error(msg) {
    console.error(`WifiApi: ${msg}`);
  }

  fireEvent(message, data) {
    this._domManagers.forEach(manager => {
      // Note: We should never have a dead message manager here because we
      // observe our child message managers shutting down, below.
      manager.sendAsyncMessage("WifiManager:" + message, data);
    });
  }

  sendMessage(message, success, data, msg) {
    // this.log(
    //   `sendMessage ${message} success=${success} ${JSON.stringify(data)}`
    // );
    try {
      msg.manager.sendAsyncMessage(
        `${message}:Return:${success ? "OK" : "NO"}`,
        {
          data,
          rid: msg.rid,
          mid: msg.mid,
        }
      );
    } catch (e) {
      this.error(`sendAsyncMessage error : ${e}`);
    }
    this.splicePendingRequest(msg);
  }

  splicePendingRequest(msg) {
    for (let i = 0; i < this._domRequest.length; i++) {
      if (this._domRequest[i].msg === msg) {
        this._domRequest.splice(i, 1);
        return;
      }
    }
  }

  receiveMessage(message) {
    let msg = message.data || {};
    msg.manager = message.target;

    this.log(`receiveMessage ${message.name} ${JSON.stringify(msg)}`);

    let managerCount = this._domManagers.length;

    // Note: By the time we receive child-process-shutdown, the child process
    // has already forgotten its permissions so we do this before the
    // permissions check.
    if (message.name === "child-process-shutdown") {
      let i;
      if ((i = this._domManagers.indexOf(msg.manager)) != -1) {
        this._domManagers.splice(i, 1);
      }
      for (i = this._domRequest.length - 1; i >= 0; i--) {
        if (this._domRequest[i].msg.manager === msg.manager) {
          this._domRequest.splice(i, 1);
        }
      }

      if (this._domManagers.length !== managerCount) {
        this.nm.configurePeriodicScan(
          this._domManagers.length !== 0,
          kScanInterval
        );
      }
      return true;
    }

    // We are interested in DOMRequests only.
    if (message.name !== "WifiManager:getState") {
      this._domRequest.push({ name: message.name, msg });
    }

    switch (message.name) {
      case "WifiManager:getState":
        if (!this._domManagers.includes(msg.manager)) {
          this._domManagers.push(msg.manager);
        }

        if (this._domManagers.length !== managerCount) {
          this.nm.configurePeriodicScan(
            this._domManagers.length !== 0,
            kScanInterval
          );
        }

        return {
          enabled: this.nm.enabled,
        };
      // break;

      case "WifiManager:setWifiEnabled":
        this.relayCommand(message.name, "setEnabled", msg);
        break;

      case "WifiManager:getNetworks":
        this.relayCommand(message.name, "getNetworks", msg);
        break;

      case "WifiManager:getKnownNetworks":
        this.relayCommand(message.name, "getKnownNetworks", msg);
        break;

      case "WifiManager:forget":
        this.relayCommand(message.name, "forget", msg);
        break;

      case "WifiManager:associate":
        this.relayCommand(message.name, "associate", msg);
        break;

      default:
        this.error(`Unsupported message: ${message.name}`);
        break;
    }

    if (this._domManagers.length !== managerCount) {
      this.nm.configurePeriodicScan(
        this._domManagers.length !== 0,
        kScanInterval
      );
    }
    return true;
  }

  async relayCommand(message, command, msg) {
    let success = true;
    let data;
    try {
      data = await this.nm[command](msg.data);
    } catch (e) {
      data = e;
      success = false;
    }

    this.sendMessage(message, success, data, msg);
  }

  observe(subject, topic, data) {
    this.log(`observe ${topic}`);

    switch (topic) {
      case kXpcomShutdownChangedTopic:
        Services.obs.removeObserver(this, kXpcomShutdownChangedTopic);
        Services.obs.removeObserver(this, kScreenStateChangedTopic);
        break;

      case kScreenStateChangedTopic:
        let enabled = data === "on";
        this.nm.configurePeriodicScan(
          enabled && this._domManagers.length !== 0,
          kScanInterval
        );
        break;
    }
  }

  get classID() {
    return "{e3feab32-322d-4e29-a754-9d06af7c8996}";
  }

  get contractID() {
    return "@mozilla.org/wifi/linux;1";
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIObserver]);
  }

  // NetworkManager callbacks.
  enabledStateChange(value) {
    this.fireEvent(value ? "wifiUp" : "wifiDown", { macAddress: null });
  }

  connectedNetwork(network) {
    this.fireEvent("onconnect", { network });
  }
}
