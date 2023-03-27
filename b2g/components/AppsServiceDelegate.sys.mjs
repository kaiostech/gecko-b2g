/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PermissionsInstaller } = ChromeUtils.import(
  "resource://gre/modules/PermissionsInstaller.jsm"
);

const { ServiceWorkerAssistant } = ChromeUtils.import(
  "resource://gre/modules/ServiceWorkerAssistant.jsm"
);

const { AppsUtils } = ChromeUtils.import(
  "resource://gre/modules/AppsUtils.jsm"
);

const DEBUG = 1;
var log = DEBUG
  ? function log_dump(msg) {
      dump("AppsServiceDelegate: " + msg + "\n");
    }
  : function log_noop(msg) {};

const inParent =
  Services.appinfo.processType === Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

export function AppsServiceDelegate() {}

AppsServiceDelegate.prototype = {
  classID: Components.ID("{a4a8d542-c877-11ea-81c6-87c0ade42646}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAppsServiceDelegate]),

  apps_list: new Map(),
  deep_links: new Map(),

  _addOrUpdateAppsList(aManifestUrl, aManifest) {
    this._registerDeepLink(aManifestUrl, aManifest);
    // Will add PWA app into the list only.
    if (Services.io.newURI(aManifestUrl).host.endsWith(".localhost")) {
      return;
    }
    // A quick check if the manifest is valid.
    if (!aManifest.start_url && !aManifest.scope) {
      return;
    }
    this.apps_list.set(aManifestUrl, aManifest.scope);
  },

  _removeFromAppsList(aManifestUrl) {
    this._unregisterDeepLink(aManifestUrl);
    this.apps_list.delete(aManifestUrl);
  },

  _registerDeepLink(aManifestUrl, aManifest) {
    if (aManifest.b2g_features && aManifest.b2g_features.deeplinks) {
      this.deep_links.set(aManifestUrl, aManifest.b2g_features.deeplinks);
    }
  },

  _unregisterDeepLink(aManifestUrl) {
    this.deep_links.delete(aManifestUrl);
  },

  _installPermissions(aFeatures, aManifestUrl, aReinstall, aState) {
    try {
      PermissionsInstaller.installPermissions(
        aFeatures,
        aManifestUrl,
        aReinstall,
        null /* onerror */
      );
    } catch (e) {
      log(`Error with PermissionsInstaller in ${aState}: ${e}`);
    }
  },

  async _processServiceWorker(aManifestUrl, aFeatures, aState) {
    try {
      switch (aState) {
        case "onBoot":
        case "onInstall":
          await ServiceWorkerAssistant.register(
            aManifestUrl,
            aFeatures,
            aState
          );
          break;
        case "onUpdate":
          await ServiceWorkerAssistant.update(aManifestUrl, aFeatures);
          break;
        case "onUninstall":
          await ServiceWorkerAssistant.unregister(aManifestUrl);
          break;
      }
    } catch (e) {
      log(`Error with ServiceWorkerAssistant in ${aState}: ${e}`);
    }
  },

  hasDeepLinks(aUrl) {
    if (!inParent) {
      // This should not happen.
      return false;
    }
    for (let [key, dlObj] of this.deep_links) {
      if (dlObj && !dlObj.paths) {
        continue;
      }
      for (let path of dlObj.paths) {
        if (aUrl.startsWith(path)) {
          let detail = {
            openedURI: aUrl,
            manifestURL: key,
          };
          Services.obs.notifyObservers(
            { wrappedJSObject: detail },
            "open-deeplink"
          );
          return true;
        }
      }
    }
    return false;
  },

  getManifestUrlByScopeUrl(aUrl) {
    if (!inParent) {
      log("Return call from non-parent process.");
      return null;
    }
    let found = null;
    this.apps_list.forEach((scope, key, map) => {
      // If there are multiple scopes for the same origin,
      // try to match the longer one.
      if (
        aUrl.startsWith(scope) &&
        (!found || scope.length > map[found].length)
      ) {
        found = key;
      }
    });
    return found;
  },

  getUa() {
    let ua = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
      Ci.nsIHttpProtocolHandler
    ).userAgent;
    log(`getUa: ${ua}`);
    return ua;
  },

  async onBoot(aManifestUrl, aManifest) {
    log(`onBoot: ${aManifestUrl}`);
    try {
      let manifest = JSON.parse(aManifest);
      // To compatible with when b2g_features only is passed.
      let features = manifest.b2g_features || manifest;
      this._installPermissions(features, aManifestUrl, false, "onBoot");
      await this._processServiceWorker(aManifestUrl, features, "onBoot");
      this._addOrUpdateAppsList(aManifestUrl, manifest);
    } catch (e) {
      log(`Error in onBoot: ${e}`);
    }
  },

  onBootDone() {
    log(`onBootDone`);
    Services.obs.notifyObservers(null, "on-boot-done");
    ServiceWorkerAssistant.waitForRegistrations();
  },

  async onClear(aManifestUrl, aType, aManifest, aCallback) {
    log(`onClear: ${aManifestUrl}: clear type: ${aType}`);

    Services.obs.notifyObservers(
      null,
      "before-clear-app-storage",
      aManifestUrl
    );

    // clearType now makes no difference since `app://` is deprecated.
    await AppsUtils.clearData(aManifestUrl);

    // clearStorage removes everything stores per origin, re-register service
    // worker to create the cache db back for used by service worker.
    if (aManifest) {
      try {
        let manifest = JSON.parse(aManifest);
        // To compatible with when b2g_features only is passed.
        let features = manifest.b2g_features || manifest;
        await ServiceWorkerAssistant.register(
          aManifestUrl,
          features,
          "onClear"
        );
      } catch (e) {
        log(`Error when trying re-register sw in onClear: ${e}`);
        aCallback.reject();
        return;
      }
    }
    aCallback.resolve();
  },

  async onInstall(aManifestUrl, aManifest, aCallback) {
    log(`onInstall: ${aManifestUrl}`);
    try {
      let manifest = JSON.parse(aManifest);
      // To compatible with when b2g_features only is passed.
      let features = manifest.b2g_features || manifest;
      this._installPermissions(features, aManifestUrl, false, "onInstall");
      await this._processServiceWorker(aManifestUrl, features, "onInstall");
      this._addOrUpdateAppsList(aManifestUrl, manifest);
      aCallback.resolve();
    } catch (e) {
      log(`Error in onInstall: ${e}`);
      aCallback.reject();
    }
  },

  async onUpdate(aManifestUrl, aManifest, aCallback) {
    log(`onUpdate: ${aManifestUrl}`);
    try {
      let manifest = JSON.parse(aManifest);
      // To compatible with when b2g_features only is passed.
      let features = manifest.b2g_features || manifest;
      this._installPermissions(features, aManifestUrl, true, "onUpdate");
      await this._processServiceWorker(aManifestUrl, features, "onUpdate");
      this._addOrUpdateAppsList(aManifestUrl, manifest);
      aCallback.resolve();
    } catch (e) {
      log(`Error in onUpdate: ${e}`);
      aCallback.reject();
    }
  },

  async onUninstall(aManifestUrl, aCallback) {
    log(`onUninstall: ${aManifestUrl}`);
    try {
      PermissionsInstaller.uninstallPermissions(aManifestUrl);
      await this._processServiceWorker(aManifestUrl, undefined, "onUninstall");
      AppsUtils.clearData(aManifestUrl);
      this._removeFromAppsList(aManifestUrl);
      aCallback.resolve();
    } catch (e) {
      log(`Error in onUninstall: ${e}`);
      aCallback.reject();
    }
  },

  onLaunch(aManifestUrl) {
    log(`onLaunch: ${aManifestUrl}`);
    Services.obs.notifyObservers(null, "on-launch-app", aManifestUrl);
  },
};
