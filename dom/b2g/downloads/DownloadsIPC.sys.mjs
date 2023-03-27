/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PromiseUtils } from "resource://gre/modules/PromiseUtils.sys.mjs";

/**
 * This module lives in the child process and receives the ipc messages
 * from the parent. It saves the download's state and redispatch changes
 * to DOM objects using an observer notification.
 *
 * This module needs to be loaded once and only once per process.
 */

const DEBUG = Services.prefs.getBoolPref("dom.downloads.debug", false);
function debug(aStr) {
  dump("-*- DownloadsIPC.jsm : " + aStr + "\n");
}

const ipcMessages = [
  "Downloads:Added",
  "Downloads:Removed",
  "Downloads:Changed",
  "Downloads:GetList:Return",
  "Downloads:Remove:Return",
  "Downloads:Pause:Return",
  "Downloads:Resume:Return",
  "Downloads:Adopt:Return",
];

export const DownloadsIPC = {
  downloads: {},

  init(aCallerOrigin, aGrantedForAll) {
    DEBUG && debug(`init ${aCallerOrigin} ${aGrantedForAll} `);
    Services.obs.addObserver(this, "xpcom-shutdown");
    ipcMessages.forEach(aMessage => {
      Services.cpmm.addMessageListener(aMessage, this);
    });

    this.callerOrigin = aCallerOrigin;
    this.grantedForAll = aGrantedForAll;
    // We need to get the list of current downloads.
    this.ready = false;
    this.getListPromises = [];
    this.downloadPromises = {};
    this.promiseId = 0;
    Services.cpmm.sendAsyncMessage("Downloads:GetList", {});
  },

  isAccessible(aDownload) {
    if (this.grantedForAll) {
      return true;
    }

    if (aDownload.referrer && aDownload.referrer === this.callerOrigin) {
      return true;
    }

    return false;
  },

  notifyChanges(aId) {
    // TODO: use the subject instead of stringifying.
    if (this.downloads[aId]) {
      DEBUG && debug("notifyChanges notifying changes for " + aId);
      Services.obs.notifyObservers(
        null,
        "downloads-state-change-" + aId,
        JSON.stringify(this.downloads[aId])
      );
    } else {
      DEBUG && debug("notifyChanges failed for " + aId);
    }
  },

  _updateDownloadsArray(aDownloads) {
    this.downloads = [];
    // We actually have an array of downloads.
    let self = this;
    aDownloads.forEach(aDownload => {
      if (!self.isAccessible(aDownload)) {
        return;
      }
      this.downloads[aDownload.id] = aDownload;
    });
  },

  receiveMessage(aMessage) {
    let download = aMessage.data;
    DEBUG && debug("message: " + aMessage.name);
    switch (aMessage.name) {
      case "Downloads:GetList:Return":
        this._updateDownloadsArray(download);

        if (!this.ready) {
          this.getListPromises.forEach(aPromise =>
            aPromise.resolve(this.downloads)
          );
          this.getListPromises.length = 0;
        }
        this.ready = true;
        break;
      case "Downloads:Added":
        if (!this.isAccessible(download)) {
          break;
        }
        this.downloads[download.id] = download;
        this.notifyChanges(download.id);
        break;
      case "Downloads:Removed":
        if (!this.isAccessible(download)) {
          break;
        }
        if (this.downloads[download.id]) {
          this.downloads[download.id] = download;
          this.notifyChanges(download.id);
          delete this.downloads[download.id];
        }
        break;
      case "Downloads:Changed":
        if (!this.isAccessible(download)) {
          break;
        }
        // Only update properties that actually changed.
        let cached = this.downloads[download.id];
        if (!cached) {
          debug("No download found for " + download.id);
          return;
        }
        let props = [
          "totalBytes",
          "currentBytes",
          "url",
          "path",
          "state",
          "contentType",
          "startTime",
        ];
        let changed = false;

        props.forEach(aProp => {
          if (download[aProp] && download[aProp] != cached[aProp]) {
            cached[aProp] = download[aProp];
            changed = true;
          }
        });

        // Updating the error property. We always get a 'state' change as
        // well.
        cached.error = download.error;

        if (changed) {
          this.notifyChanges(download.id);
        }
        break;
      case "Downloads:Remove:Return":
      case "Downloads:Pause:Return":
      case "Downloads:Resume:Return":
      case "Downloads:Adopt:Return":
        if (this.downloadPromises[download.promiseId]) {
          if (!download.error) {
            this.downloadPromises[download.promiseId].resolve(download);
          } else {
            this.downloadPromises[download.promiseId].reject(download.error);
          }
          delete this.downloadPromises[download.promiseId];
        }
        break;
    }
  },

  /**
   * Returns a promise that is resolved with the list of current downloads.
   */
  getDownloads() {
    DEBUG && debug("getDownloads");
    let deferred = PromiseUtils.defer();
    if (this.ready) {
      DEBUG && debug("Returning existing list.");
      deferred.resolve(this.downloads);
    } else {
      this.getListPromises.push(deferred);
    }
    return deferred.promise;
  },

  /**
   * Void function to trigger removal of completed downloads.
   */
  clearAllDone() {
    DEBUG && debug("clearAllDone");
    Services.cpmm.sendAsyncMessage("Downloads:ClearAllDone", {});
  },

  generatePromiseId() {
    return this.promiseId++;
  },

  remove(aId) {
    DEBUG && debug("remove " + aId);
    if (!this.downloads[aId]) {
      return Promise.reject("InvalidDownload");
    }
    let deferred = PromiseUtils.defer();
    let pId = this.generatePromiseId();
    this.downloadPromises[pId] = deferred;
    Services.cpmm.sendAsyncMessage("Downloads:Remove", {
      id: aId,
      promiseId: pId,
    });
    return deferred.promise;
  },

  pause(aId) {
    DEBUG && debug("pause " + aId);
    if (!this.downloads[aId]) {
      return Promise.reject("InvalidDownload");
    }
    let deferred = PromiseUtils.defer();
    let pId = this.generatePromiseId();
    this.downloadPromises[pId] = deferred;
    Services.cpmm.sendAsyncMessage("Downloads:Pause", {
      id: aId,
      promiseId: pId,
    });
    return deferred.promise;
  },

  resume(aId) {
    DEBUG && debug("resume " + aId);
    if (!this.downloads[aId]) {
      return Promise.reject("InvalidDownload");
    }
    let deferred = PromiseUtils.defer();
    let pId = this.generatePromiseId();
    this.downloadPromises[pId] = deferred;
    Services.cpmm.sendAsyncMessage("Downloads:Resume", {
      id: aId,
      promiseId: pId,
    });
    return deferred.promise;
  },

  adoptDownload(aJsonDownload) {
    DEBUG && debug("adoptDownload");
    let deferred = PromiseUtils.defer();
    let pId = this.generatePromiseId();
    this.downloadPromises[pId] = deferred;
    Services.cpmm.sendAsyncMessage("Downloads:Adopt", {
      jsonDownload: aJsonDownload,
      promiseId: pId,
    });
    return deferred.promise;
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      ipcMessages.forEach(aMessage => {
        Services.cpmm.removeMessageListener(aMessage, this);
      });
    }
  },
};
