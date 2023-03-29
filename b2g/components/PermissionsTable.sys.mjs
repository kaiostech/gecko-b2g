/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Permission access flags
const READONLY = "readonly";
const CREATEONLY = "createonly";
const READCREATE = "readcreate";
const READWRITE = "readwrite";

const ALLOW_ACTION = Ci.nsIPermissionManager.ALLOW_ACTION;
const DENY_ACTION = Ci.nsIPermissionManager.DENY_ACTION;
const PROMPT_ACTION = Ci.nsIPermissionManager.PROMPT_ACTION;

// Permissions that are granted to all installed apps.
export const defaultPermissions = [
  "vibration",
  "networkstats-perm",
  "lock-orientation",
  "push",
];

/**
 * For efficient lookup and systematic indexing, please help to arrange the
 * permission names in alphabetical order. The double quotes are only needed
 * by the names with non-alphanumeric character, such as "-", and could be
 * revised by the lint tools automatically.
 */
export const PermissionsTable = {
  "account-manager": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "account-observer-activesync": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "account-observer-google": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "account-observer-kaiaccount": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  alarms: {
    pwa: ALLOW_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-capture": {
    pwa: PROMPT_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
  },
  "audio-channel-alarm": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-channel-content": {
    pwa: ALLOW_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-channel-normal": {
    pwa: ALLOW_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-channel-notification": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-channel-publicnotification": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "audio-channel-ringer": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "audio-channel-system": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "audio-channel-telephony": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "background-sensors": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  battery: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  bluetooth: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "bluetooth-privileged": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  camera: {
    pwa: PROMPT_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
  },
  cellbroadcast: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "cloud-authorization": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  contacts: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    access: ["read", "write", "create"],
  },
  datacall: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "desktop-notification": {
    pwa: PROMPT_ACTION,
    signed: PROMPT_ACTION,
    core: PROMPT_ACTION,
    defaultPromptAction: {
      signed: ALLOW_ACTION,
      core: ALLOW_ACTION,
    },
  },
  "device-configuration": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "device-storage:apps": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
    access: ["read"],
  },
  "device-storage:apps-storage": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
    access: ["read"],
  },
  "device-storage:crashes": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
    access: ["read"],
  },
  "device-storage:music": {
    pwa: DENY_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write", "create"],
  },
  "device-storage:pictures": {
    pwa: DENY_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write", "create"],
  },
  "device-storage:sdcard": {
    pwa: DENY_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write", "create"],
  },
  "device-storage:videos": {
    pwa: DENY_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write", "create"],
  },
  downloads: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  engmode: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "eventlogger-manager": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "eventlogger-summary": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "feature-detection": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  flashlight: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  flip: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  fmradio: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  fota: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  geolocation: {
    pwa: PROMPT_ACTION,
    signed: PROMPT_ACTION,
  },
  "ime-connect": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  input: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  mobileconnection: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "networkstats-manage": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  permissions: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  power: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  powersupply: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  "process-manager": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  rsu: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  settings: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write"],
  },
  sms: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "speaker-control": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "system-time": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
    access: ["read", "write"],
  },
  systemXHR: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  "tcp-socket": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
  },
  telephony: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  tethering: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  usb: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  "video-capture": {
    pwa: PROMPT_ACTION,
    signed: PROMPT_ACTION,
    core: ALLOW_ACTION,
  },
  virtualcursor: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  voicemail: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  volumemanager: {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  wappush: {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "web-view": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  "webapps-manage": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "wifi-manage": {
    pwa: DENY_ACTION,
    signed: DENY_ACTION,
    core: ALLOW_ACTION,
  },
  "worker-activity": {
    pwa: DENY_ACTION,
    signed: ALLOW_ACTION,
    core: ALLOW_ACTION,
  },
  /**
   * Note: Please do NOT directly add new permission names at the bottom of
   * this table, try to insert them alphabetically.
   */
};

/**
 * Append access modes to the permission name as suffixes.
 *   e.g. permission name 'device-storage:sdcard' with ['read', 'write'] =
 *   ['device-storage:sdcard:read', 'device-storage:sdcard:write']
 *
 * @param {string} aPermName
 * @param {Array} aAccess
 * @returns {Array} containing access:appended permission names.
 */
export const appendAccessToPermName = (aPermName, aAccess) => {
  if (!aAccess.length) {
    return [aPermName];
  }
  return aAccess.map(function(aMode) {
    return aPermName + ":" + aMode;
  });
};

/**
 * Expand an access string into multiple permission names,
 *   e.g: permission name 'device-storage:sdcard' with 'readwrite' =
 *   ['device-storage:sdcard:read', 'device-storage:sdcard:create', 'device-storage:sdcard:write']
 *
 * @param {string} aPermName
 * @param {string} aAccess (optional)
 * @returns {Array} containing expanded permission names.
 */
export const expandPermissions = (aPermName, aAccess) => {
  if (!PermissionsTable[aPermName]) {
    let errorMsg =
      "PermissionsTable.jsm: expandPermissions: Unknown Permission: " +
      aPermName;
    console.error(errorMsg);
    dump(errorMsg);
    return [];
  }

  const tableEntry = PermissionsTable[aPermName];

  if ((!aAccess && tableEntry.access) || (aAccess && !tableEntry.access)) {
    let errorMsg =
      "PermissionsTable.jsm: expandPermissions: Invalid access for permission " +
      aPermName +
      ": " +
      aAccess +
      "\n";
    console.error(errorMsg);
    dump(errorMsg);
    return [];
  }

  let expandedPermNames = [];

  if (tableEntry.access && aAccess) {
    let requestedSuffixes = [];
    switch (aAccess) {
      case READONLY:
        requestedSuffixes.push("read");
        break;
      case CREATEONLY:
        requestedSuffixes.push("create");
        break;
      case READCREATE:
        requestedSuffixes.push("read", "create");
        break;
      case READWRITE:
        requestedSuffixes.push("read", "create", "write");
        break;
      default:
        return [];
    }

    let permArr = appendAccessToPermName(aPermName, requestedSuffixes);

    // Only add the suffixed version if the suffix exists in the table.
    for (let idx in permArr) {
      let suffix = requestedSuffixes[idx % requestedSuffixes.length];
      if (tableEntry.access.includes(suffix)) {
        expandedPermNames.push(permArr[idx]);
      }
    }
  } else if (tableEntry.substitute) {
    expandedPermNames = expandedPermNames.concat(tableEntry.substitute);
  } else {
    expandedPermNames.push(aPermName);
  }

  return expandedPermNames;
};

export const permissionsReverseTable = {};
export const AllPossiblePermissions = [];

(function() {
  // PermissionsTable as it is works well for direct searches, but not
  // so well for reverse ones (that is, if I get something like
  // device-storage:music:read or settings:read, how
  // do I know which permission it really is? Hence this table is
  // born. The idea is that
  // reverseTable[device-storage:music:read] should return
  // device-storage:music
  //
  // We also need a list of all the possible permissions for things like the
  // settingsmanager, so construct that while we're at it.
  for (let permName in PermissionsTable) {
    let permAliases = [];
    if (PermissionsTable[permName].access) {
      permAliases = expandPermissions(permName, "readwrite");
    } else if (!PermissionsTable[permName].substitute) {
      permAliases = expandPermissions(permName);
    }
    for (let i = 0; i < permAliases.length; i++) {
      permissionsReverseTable[permAliases[i]] = permName;
      AllPossiblePermissions.push(permAliases[i]);
    }
  }
})();
