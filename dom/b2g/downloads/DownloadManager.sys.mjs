/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);
import { DownloadsIPC } from "resource://gre/modules/DownloadsIPC.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "volumeService",
  "@mozilla.org/telephony/volume-service;1",
  "nsIVolumeService"
);

/**
 * The content process implementations of navigator.b2g.downloadManager and its
 * DownloadObject download objects.  Uses DownloadsIPC.jsm to communicate with
 * DownloadManager.jsm in the parent process.
 */

const DEBUG = Services.prefs.getBoolPref("dom.downloads.debug", false);
function debug(aStr) {
  dump("-*- DownloadManager.js : " + aStr + "\n");
}

export function DownloadManager() {
  DEBUG && debug("DownloadManager constructor");
}

DownloadManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    DEBUG && debug("init " + aWindow?.document?.nodePrincipal?.origin);

    this.grantedForAll =
      Services.perms.testExactPermissionFromPrincipal(
        aWindow?.document?.nodePrincipal,
        "downloads"
      ) == Ci.nsIPermissionManager.ALLOW_ACTION;

    DownloadsIPC.init(
      aWindow?.document?.nodePrincipal?.origin,
      this.grantedForAll
    );

    this.initDOMRequestHelper(aWindow, [
      "Downloads:Added",
      "Downloads:Removed",
    ]);
  },

  uninit() {
    DEBUG && debug("uninit");
    downloadsCache.evict(this._window);
  },

  set ondownloadstart(aHandler) {
    this.__DOM_IMPL__.setEventHandler("ondownloadstart", aHandler);
  },

  get ondownloadstart() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadstart");
  },

  getDownloads() {
    DEBUG && debug("getDownloads");

    return this.createPromise(
      function (aResolve, aReject) {
        DownloadsIPC.getDownloads().then(
          function (aDownloads) {
            // Turn the list of download objects into DOM objects and
            // send them.
            let array = new this._window.Array();
            for (let id in aDownloads) {
              let dom = getOrCreateDownloadObject(this._window, aDownloads[id]);
              array.push(dom);
            }
            aResolve(array);
          }.bind(this),
          function () {
            aReject("GetDownloadsError");
          }
        );
      }.bind(this)
    );
  },

  clearAllDone() {
    DEBUG && debug("clearAllDone");
    // This is a void function; we just kick it off.  No promises, etc.
    DownloadsIPC.clearAllDone();
  },

  remove(aDownload) {
    DEBUG && debug("remove " + aDownload.url + " " + aDownload.id);
    return this.createPromise(
      function (aResolve, aReject) {
        if (!downloadsCache.has(this._window, aDownload.id)) {
          DEBUG && debug("no download " + aDownload.id);
          aReject("InvalidDownload");
          return;
        }

        DownloadsIPC.remove(aDownload.id).then(
          function (aResult) {
            let dom = getOrCreateDownloadObject(this._window, aResult);
            if (dom.state === "finalized") {
              aResolve(dom);
            } else {
              debug(
                `Resolve remove() with a new download object with "finalized" state. ${aResult.id} ${dom.state}`
              );
              // The state might be updated to "finalized" AFTER this remove
              // function being resolved. If the race condition happens, update
              // the state manually, since once a download becomes "finalized",
              // it will not change anymore.
              let impl = Cc["@mozilla.org/download/object;1"].createInstance(
                Ci.nsISupports
              );
              impl.wrappedJSObject._init(this._window, dom);
              impl.wrappedJSObject.state = "finalized";
              let contentDownloadObject = this._window.DownloadObject._create(
                this._window,
                impl.wrappedJSObject
              );
              aResolve(contentDownloadObject);
            }
          }.bind(this),
          function () {
            aReject("RemoveError");
          }
        );
      }.bind(this)
    );
  },

  adoptDownload(aAdoptDownloadDict) {
    // Our AdoptDownloadDict only includes simple types, which WebIDL enforces.
    // We have no object/any types so we do not need to worry about invoking
    // JSON.stringify (and it inheriting our security privileges).
    DEBUG && debug("adoptDownload");
    return this.createPromise(
      function (aResolve, aReject) {
        if (!aAdoptDownloadDict) {
          debug("DownloadObject dictionary is required!");
          aReject("InvalidDownload");
          return;
        }
        if (
          !aAdoptDownloadDict.storageName ||
          !aAdoptDownloadDict.storagePath ||
          !aAdoptDownloadDict.contentType
        ) {
          debug("Missing one of: storageName, storagePath, contentType");
          aReject("InvalidDownload");
          return;
        }

        // Convert storageName/storagePath to a local filesystem path.
        let volume;
        // getVolumeByName throws if you give it something it doesn't like
        // because XPConnect converts the NS_ERROR_NOT_AVAILABLE to an
        // exception.  So catch it.
        try {
          volume = lazy.volumeService.getVolumeByName(
            aAdoptDownloadDict.storageName
          );
        } catch (ex) {}
        if (!volume) {
          debug("Invalid storage name: " + aAdoptDownloadDict.storageName);
          aReject("InvalidDownload");
          return;
        }
        let computedPath =
          volume.mountPoint + "/" + aAdoptDownloadDict.storagePath;
        // We validate that there is actually a file at the given path in the
        // parent process in DownloadManager.js because that's where the file
        // access would actually occur either way.

        // Create a DownloadManager.jsm 'jsonDownload' style representation.
        let jsonDownload = {
          url: aAdoptDownloadDict.url,
          path: computedPath,
          contentType: aAdoptDownloadDict.contentType,
          startTime: aAdoptDownloadDict.startTime?.valueOf() || Date.now(),
          referrer: Cu.getWebIDLCallerPrincipal().origin,
        };

        DownloadsIPC.adoptDownload(jsonDownload).then(
          function (aResult) {
            let domDownload = getOrCreateDownloadObject(this._window, aResult);
            aResolve(domDownload);
          }.bind(this),
          function (aResult) {
            // This will be one of: AdoptError (generic catch-all),
            // AdoptNoSuchFile, AdoptFileIsDirectory
            aReject(aResult);
          }
        );
      }.bind(this)
    );
  },

  receiveMessage(aMessage) {
    let data = aMessage.data;
    switch (aMessage.name) {
      case "Downloads:Added":
        DEBUG && debug("Adding " + uneval(data));
        let event = new this._window.DownloadEvent("downloadstart", {
          download: getOrCreateDownloadObject(this._window, data),
        });
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
    }
  },

  classDescription: "DownloadManager",
  classID: Components.ID("{6599ff0b-dfcc-4bf6-a2ea-434410cedcb5}"),
  contractID: "@mozilla.org/download/manager;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
    Ci.nsIDOMGlobalPropertyInitializer,
  ]),
};

/**
 * Keep track of download objects per window.
 */
var downloadsCache = {
  init() {
    this.cache = new WeakMap();
  },

  has(aWindow, aId) {
    let downloads = this.cache.get(aWindow);
    return !!(downloads && downloads[aId]);
  },

  get(aWindow, aDownload) {
    let downloads = this.cache.get(aWindow);
    if (!(downloads && downloads[aDownload.id])) {
      DEBUG && debug("Adding download " + aDownload.id + " to cache.");
      if (!downloads) {
        this.cache.set(aWindow, {});
        downloads = this.cache.get(aWindow);
      }
      // Create the object and add it to the cache.
      let impl = Cc["@mozilla.org/download/object;1"].createInstance(
        Ci.nsISupports
      );
      impl.wrappedJSObject._init(aWindow, aDownload);
      downloads[aDownload.id] = aWindow.DownloadObject._create(
        aWindow,
        impl.wrappedJSObject
      );
    }
    return downloads[aDownload.id];
  },

  evict(aWindow) {
    this.cache.delete(aWindow);
  },
};

downloadsCache.init();

/**
 * The DOM facade of a download object.
 */

function getOrCreateDownloadObject(aWindow, aDownload) {
  return downloadsCache.get(aWindow, aDownload);
}

export function DownloadObject() {
  DEBUG && debug("DownloadObject constructor ");

  this.wrappedJSObject = this;
  this.totalBytes = 0;
  this.currentBytes = 0;
  this.url = null;
  this.path = null;
  this.storageName = null;
  this.storagePath = null;
  this.contentType = null;

  /* fields that require getters/setters */
  this._error = null;
  this._startTime = new Date();
  this._state = "stopped";

  /* private fields */
  this.id = null;
}

DownloadObject.prototype = {
  pause() {
    DEBUG && debug("DownloadObject pause " + this.id);
    let id = this.id;
    let self = this;
    return this.createPromise(function (aResolve, aReject) {
      DownloadsIPC.pause(id).then(function (aResult) {
        let domDownload = getOrCreateDownloadObject(self._window, aResult);
        aResolve(domDownload);
      }, aReject);
    });
  },

  resume() {
    DEBUG && debug("DownloadObject resume " + this.id);
    let id = this.id;
    let self = this;
    return this.createPromise(function (aResolve, aReject) {
      DownloadsIPC.resume(id).then(function (aResult) {
        let domDownload = getOrCreateDownloadObject(self._window, aResult);
        aResolve(domDownload);
      }, aReject);
    });
  },

  set onstatechange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onstatechange", aHandler);
  },

  get onstatechange() {
    return this.__DOM_IMPL__.getEventHandler("onstatechange");
  },

  get error() {
    return this._error;
  },

  set error(aError) {
    this._error = aError;
  },

  get startTime() {
    return this._startTime;
  },

  set startTime(aStartTime) {
    if (aStartTime instanceof Date) {
      this._startTime = aStartTime;
    } else {
      this._startTime = new Date(aStartTime);
    }
  },

  get state() {
    return this._state;
  },

  // We require a setter here to simplify the internals of the Download Manager
  // since we actually pass dummy JSON objects to the child process and update
  // them. This is the case for all other setters for read-only attributes
  // implemented in this object.
  set state(aState) {
    // We need to ensure that XPCOM consumers of this API respect the enum
    // values as well.
    if (["downloading", "stopped", "succeeded", "finalized"].includes(aState)) {
      this._state = aState;
    }
  },

  /**
   * Initialize a DownloadObject instance for the given window using the
   * 'jsonDownload' serialized format of the download encoded by
   * DownloadManager.jsm.
   */
  _init(aWindow, aDownload) {
    this._window = aWindow;
    this.id = aDownload.id;
    this._update(aDownload);
    Services.obs.addObserver(
      this,
      "downloads-state-change-" + this.id,
      /* ownsWeak */ true
    );
    DEBUG && debug("observer set for " + this.id);
  },

  /**
   * Updates the state of the object and fires the statechange event.
   */
  _update(aDownload) {
    DEBUG && debug("update " + uneval(aDownload));
    if (this.id != aDownload.id) {
      return;
    }

    let props = [
      "totalBytes",
      "currentBytes",
      "url",
      "path",
      "storageName",
      "storagePath",
      "state",
      "contentType",
      "startTime",
      "referrer",
    ];
    let changed = false;
    let changedProps = {};

    props.forEach(prop => {
      if (prop in aDownload && aDownload[prop] != this[prop]) {
        this[prop] = aDownload[prop];
        changedProps[prop] = changed = true;
      }
    });

    // When the path changes, we should update the storage name and
    // storage path used for our downloaded file in case our download
    // was re-targetted to a different storage and/or filename.
    if (changedProps.path && typeof this.path === "string") {
      let storages = this._window.navigator.b2g.getDeviceStorages("sdcard");
      let storageName;
      storages.forEach(aStorage => {
        if (
          this.path.startsWith(aStorage.storagePath) ||
          (aStorage.default && !storageName)
        ) {
          storageName = aStorage.storageName;
        }
      });
      // Now get the path for this storage area.
      if (storageName) {
        let volume = lazy.volumeService.getVolumeByName(storageName);
        if (volume) {
          // Finally, create the relative path of the file that can be used
          // later on to retrieve the file via DeviceStorage. Our path
          // needs to omit the starting '/'.
          this.storageName = storageName;
          if (this.path.startsWith(volume.mountPoint)) {
            this.storagePath = this.path.substring(
              this.path.indexOf(volume.mountPoint) +
                volume.mountPoint.length +
                1
            );
          } else {
            // The file might be in a removed or dettached volume, and does not
            // exist. Remove the leading '/' for legitimate storagePath.
            this.storagePath = this.path.startsWith("/")
              ? this.path.substring(1)
              : this.path;
          }
        }
      }
    }

    if (aDownload.error) {
      //
      // When we get a generic error failure back from the js downloads api
      // we will verify the status of device storage to see if we can't provide
      // a better error result value.
      //
      // XXX If these checks expand further, consider moving them into their
      // own function.
      //
      let result = aDownload.error.result;
      let storage = this._window.navigator.b2g.getDeviceStorage("sdcard");

      // If we don't have access to device storage we'll opt out of these
      // extra checks as they are all dependent on the state of the storage.
      if (result == Cr.NS_ERROR_FAILURE && storage) {
        // We will delay sending the notification until we've inferred which
        // error is really happening.
        changed = false;
        DEBUG &&
          debug("Attempting to infer error via device storage sanity checks.");
        // Get device storage and request availability status.
        let available = storage.available();
        available.onsuccess = function () {
          DEBUG && debug("Storage Status = '" + available.result + "'");
          let inferredError = result;
          switch (available.result) {
            case "unavailable":
              inferredError = Cr.NS_ERROR_FILE_NOT_FOUND;
              break;
            case "shared":
              inferredError = Cr.NS_ERROR_FILE_ACCESS_DENIED;
              break;
          }
          this._updateWithError(aDownload, inferredError);
        }.bind(this);
        available.onerror = function () {
          this._updateWithError(aDownload, result);
        }.bind(this);
      }

      // DOMException cannot reflect all possible download nsresult error codes,
      // so we store the error code within its [message] attribute.
      this.error = new this._window.DOMException(
        aDownload.error.result,
        "NS_ERROR_UNEXPECTED"
      );
    } else {
      this.error = null;
    }

    // The visible state has not changed, so no need to fire an event.
    if (!changed) {
      return;
    }

    this._sendStateChange();
  },

  _updateWithError(aDownload, aError) {
    this.error = new this._window.DOMException(aError, "NS_ERROR_UNEXPECTED");
    this._sendStateChange();
  },

  _sendStateChange() {
    // __DOM_IMPL__ may not be available at first update.
    if (this.__DOM_IMPL__) {
      let event = new this._window.DownloadEvent("statechange", {
        download: this.__DOM_IMPL__,
      });
      DEBUG && debug("Dispatching statechange event. state=" + this.state);
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  },

  observe(aSubject, aTopic, aData) {
    DEBUG && debug(`DownloadObject ${this.id} observe ${aTopic}`);
    if (aTopic !== "downloads-state-change-" + this.id) {
      return;
    }

    try {
      let download = JSON.parse(aData);
      // We get the start time as milliseconds, not as a Date object.
      if (download.startTime) {
        download.startTime = new Date(download.startTime);
      }
      this._update(download);
    } catch (e) {}
  },

  classDescription: "DownloadObject",
  classID: Components.ID("{ee82e19f-8ead-406f-ad91-194e77a61549}"),
  contractID: "@mozilla.org/download/object;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};
