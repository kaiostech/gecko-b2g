/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const XRE_OS_UPDATE_APPLY_TO_DIR = "OSUpdApplyToD";
const UPDATE_ARCHIVE_DIR = "UpdArchD";
const LOCAL_DIR = "/data/local";
const UPDATES_DIR = "updates/0";
const FOTA_DIR = "updates/fota";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "um",
  "@mozilla.org/updates/update-manager;1",
  "nsIUpdateManager"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "volumeService",
  "@mozilla.org/telephony/volume-service;1",
  "nsIVolumeService"
);

XPCOMUtils.defineLazyGetter(lazy, "gExtStorage", function dp_gExtStorage() {
  return Services.env.get("EXTERNAL_STORAGE");
});

// This exists to mark the affected code for bug 828858.
const gUseSDCard = true;

const VERBOSE = 1;
var log = VERBOSE
  ? function log_dump(msg) {
      dump("DirectoryProvider: " + msg + "\n");
    }
  : function log_noop(msg) {};

export function DirectoryProvider() {}

DirectoryProvider.prototype = {
  classID: Components.ID("{9181eb7c-6f87-11e1-90b1-4f59d80dd2e5}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  _profD: null,

  getFile(prop, persistent) {
    if (AppConstants.platform === "gonk") {
      return this.getFileOnGonk(prop, persistent);
    }
    return this.getFileNotGonk(prop, persistent);
  },

  getFileOnGonk(prop, persistent) {
    if (prop == UPDATE_ARCHIVE_DIR) {
      // getUpdateDir will set persistent to false since it may toggle between
      // /data/local/ and /mnt/sdcard based on free space and/or availability
      // of the sdcard.
      // before download, check if free space is 2.1 times of update.mar
      return this.getUpdateDir(persistent, UPDATES_DIR, 2.1);
    }
    if (prop == XRE_OS_UPDATE_APPLY_TO_DIR) {
      // getUpdateDir will set persistent to false since it may toggle between
      // /data/local/ and /mnt/sdcard based on free space and/or availability
      // of the sdcard.
      // before apply, check if free space is 1.1 times of update.mar
      return this.getUpdateDir(persistent, FOTA_DIR, 1.1);
    }
    return null;
  },

  getFileNotGonk(prop, persistent) {
    if (prop == "ProfD") {
      let inParent =
        Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      if (inParent) {
        // Just bail out to use the default from toolkit.
        return null;
      }
      if (!this._profD) {
        this._profD = Services.cpmm.sendSyncMessage("getProfD", {})[0];
      }
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(this._profD);
      persistent.value = true;
      return file;
    }
    return null;
  },

  // The VolumeService only exists on the device, and not on desktop
  volumeHasFreeSpace: function dp_volumeHasFreeSpace(
    volumePath,
    requiredSpace
  ) {
    if (!volumePath) {
      return false;
    }
    if (!lazy.volumeService) {
      return false;
    }
    let volume = lazy.volumeService.createOrGetVolumeByPath(volumePath);
    if (!volume || volume.state !== Ci.nsIVolume.STATE_MOUNTED) {
      return false;
    }
    let stat = volume.getStats();
    if (!stat) {
      return false;
    }
    return requiredSpace <= stat.freeBytes;
  },

  findUpdateDirWithFreeSpace: function dp_findUpdateDirWithFreeSpace(
    requiredSpace,
    subdir
  ) {
    if (!lazy.volumeService) {
      return this.createUpdatesDir(LOCAL_DIR, subdir);
    }

    let activeUpdate = lazy.um.activeUpdate;
    if (gUseSDCard) {
      if (this.volumeHasFreeSpace(lazy.gExtStorage, requiredSpace)) {
        let extUpdateDir = this.createUpdatesDir(lazy.gExtStorage, subdir);
        if (extUpdateDir !== null) {
          return extUpdateDir;
        }
        log(
          "Warning: " +
            lazy.gExtStorage +
            " has enough free space for update " +
            activeUpdate.name +
            ", but is not writable"
        );
      }
    }

    if (this.volumeHasFreeSpace(LOCAL_DIR, requiredSpace)) {
      let localUpdateDir = this.createUpdatesDir(LOCAL_DIR, subdir);
      if (localUpdateDir !== null) {
        return localUpdateDir;
      }
      log(
        "Warning: " +
          LOCAL_DIR +
          " has enough free space for update " +
          activeUpdate.name +
          ", but is not writable"
      );
    }

    return null;
  },

  getUpdateDir: function dp_getUpdateDir(persistent, subdir, multiple) {
    let defaultUpdateDir = this.getDefaultUpdateDir();
    persistent.value = false;

    let activeUpdate = lazy.um.activeUpdate;
    if (!activeUpdate) {
      log(
        "Warning: No active update found, using default update dir: " +
          defaultUpdateDir
      );
      return defaultUpdateDir;
    }

    let selectedPatch = activeUpdate.selectedPatch;
    if (!selectedPatch) {
      log(
        "Warning: No selected patch, using default update dir: " +
          defaultUpdateDir
      );
      return defaultUpdateDir;
    }

    let requiredSpace = selectedPatch.size * multiple;
    let updateDir = this.findUpdateDirWithFreeSpace(requiredSpace, subdir);
    if (updateDir) {
      return updateDir;
    }

    // If we've gotten this far, there isn't enough free space to download the patch
    // on either external storage or /data/local. All we can do is report the
    // error and let upstream code handle it more gracefully.
    log(
      "Error: No volume found with " +
        requiredSpace +
        " bytes for downloading" +
        " update " +
        activeUpdate.name
    );
    activeUpdate.errorCode = Cr.NS_ERROR_FILE_TOO_BIG;
    return null;
  },

  createUpdatesDir: function dp_createUpdatesDir(root, subdir) {
    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(root);
    if (!dir.isWritable()) {
      log("Error: " + dir.path + " isn't writable");
      return null;
    }
    dir.appendRelativePath(subdir);
    if (dir.exists()) {
      if (dir.isDirectory() && dir.isWritable()) {
        return dir;
      }
      // subdir is either a file or isn't writable. In either case we
      // can't use it.
      log("Error: " + dir.path + " is a file or isn't writable");
      return null;
    }
    // subdir doesn't exist, and the parent is writable, so try to
    // create it. This can fail if a file named updates exists.
    try {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0770", 8));
    } catch (e) {
      // The create failed for some reason. We can't use it.
      log("Error: " + dir.path + " unable to create directory");
      return null;
    }
    return dir;
  },

  getDefaultUpdateDir: function dp_getDefaultUpdateDir() {
    let path = lazy.gExtStorage;
    if (!path) {
      path = LOCAL_DIR;
    }

    if (lazy.volumeService) {
      let extVolume = lazy.volumeService.createOrGetVolumeByPath(path);
      if (!extVolume) {
        path = LOCAL_DIR;
      }
    }

    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(path);

    if (!dir.exists() && path != LOCAL_DIR) {
      // Fallback to LOCAL_DIR if we didn't fallback earlier
      dir.initWithPath(LOCAL_DIR);

      if (!dir.exists()) {
        throw Components.Exception("", Cr.NS_ERROR_FILE_NOT_FOUND);
      }
    }

    dir.appendRelativePath("updates");
    return dir;
  },
};
