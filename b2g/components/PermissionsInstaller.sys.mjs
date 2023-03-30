/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = false;
function debug(aMsg) {
  if (DEBUG) {
    dump(`-*-*- PermissionsInstaller : ${aMsg}\n`);
  }
}

import {
  AllPossiblePermissions,
  PermissionsTable,
  expandPermissions,
  defaultPermissions,
} from "resource://gre/modules/PermissionsTable.sys.mjs";

const kPermManager = Ci.nsIPermissionManager;

/*eslint no-unused-vars: ["error", { "varsIgnorePattern": "[A-Z]+_ACTION" }]*/
const UNKNOWN_ACTION = kPermManager.UNKNOWN_ACTION;
const ALLOW_ACTION = kPermManager.ALLOW_ACTION;
const DENY_ACTION = kPermManager.DENY_ACTION;
const PROMPT_ACTION = kPermManager.PROMPT_ACTION;

// Stripped down permission settings helper.
export const PermissionsHelper = {
  getPrincipal(aManifestURL) {
    let uri = Services.io.newURI(aManifestURL);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    return principal;
  },

  getPermission(aPermName, aManifestURL) {
    debug(`getPermission ${aPermName} for ${aManifestURL}`);

    let principal = this.getPrincipal(aManifestURL);
    let result = Services.perms.testExactPermissionFromPrincipal(
      principal,
      aPermName
    );
    return result;
  },

  removePermission(aPermName, aManifestURL) {
    debug(`removePermission ${aPermName} for ${aManifestURL}`);

    let principal = this.getPrincipal(aManifestURL);
    Services.perms.removeFromPrincipal(principal, aPermName);
  },

  addPermission(aPermName, aManifestURL, aValue) {
    const PERM_VALUES = ["unknown", "allow", "deny", "prompt"];
    debug(
      `addPermission ${aPermName} for ${aManifestURL}: ${PERM_VALUES[aValue]}`
    );

    let principal = this.getPrincipal(aManifestURL);
    Services.perms.addFromPrincipal(principal, aPermName, aValue);
  },

  removeAllForURI(aManifestURL) {
    debug(`removeAllForURI for ${aManifestURL}`);
    let principal = this.getPrincipal(aManifestURL);
    let perms = Services.perms.getAllForPrincipal(principal);

    perms.forEach(perm => Services.perms.removePermission(perm));
  },

  addCoreApp(aOrigin) {
    debug(`add core app: ${aOrigin}`);
    if (!this.coreApps) {
      this.coreApps = {};
    }
    this.coreApps[aOrigin] = true;
  },

  isCoreApp(aOrigin) {
    return this.coreApps ? !!this.coreApps[aOrigin] : false;
  },
};

export const PermissionsInstaller = {
  /**
   * Install permissisions or remove deprecated permissions upon re-install.
   *
   * @param {object} aFeatures
   *        The set of b2g specific features from the manifest
   * @param {string} aManifestURL
   *        The url of the manifest
   * @param {boolean} aIsReinstall
   *        Indicates the app was just re-installed
   * @param {Function} aOnError
   *        A function called if an error occurs
   * @returns {void}
   */
  installPermissions(aFeatures, aManifestURL, aIsReinstall, aOnError) {
    try {
      if (!aFeatures.permissions && !aIsReinstall) {
        return;
      }

      if (aIsReinstall) {
        // Compare the original permissions against the new permissions
        // Remove any deprecated Permissions

        if (aFeatures.permissions) {
          // Expand permission names.
          let newPermNames = [];
          for (let permName in aFeatures.permissions) {
            let expandedPermNames = expandPermissions(
              permName,
              aFeatures.permissions[permName].access
            );
            newPermNames = newPermNames.concat(expandedPermNames);
          }

          for (let idx in AllPossiblePermissions) {
            let permName = AllPossiblePermissions[idx];
            let index = newPermNames.indexOf(permName);
            if (index == -1) {
              // See if the permission was installed previously.
              let permValue = PermissionsHelper.getPermission(
                permName,
                aManifestURL
              );
              if (permValue == UNKNOWN_ACTION || permValue == DENY_ACTION) {
                // All 'deny' permissions should be preserved
                continue;
              }
              // Remove the deprecated permission
              PermissionsHelper.removePermission(permName, aManifestURL);
            }
          }
        }
      }

      let appType = "pwa";
      let isCore = false;

      // Packaged apps are served from the ".localhost" domain.
      let uri = Services.io.newURI(aManifestURL);
      if (uri.host.endsWith(".localhost")) {
        appType = "signed";
        // Core apps get special treatment for some permissions.
        isCore = aFeatures.core === true;
      }

      // Add default permissions that are granted to all installed apps.
      defaultPermissions.forEach(permission => {
        PermissionsHelper.addPermission(permission, aManifestURL, ALLOW_ACTION);
      });

      if (isCore) {
        let appPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          aManifestURL
        );
        PermissionsHelper.addCoreApp(appPrincipal.origin);
      }

      for (let permName in aFeatures.permissions) {
        if (!PermissionsTable[permName]) {
          console.error(
            `PermissionsInstaller.sys.mjs: '${permName}' is not a valid app permission name.`
          );
          debug(`'${permName}' is not a valid app permission name.`);
          continue;
        }

        let expandedPermNames = expandPermissions(
          permName,
          aFeatures.permissions[permName].access
        );
        for (let idx in expandedPermNames) {
          let permValue = PermissionsTable[permName][appType];
          if (isCore && PermissionsTable[permName].core !== undefined) {
            permValue = PermissionsTable[permName].core;
          }

          if (permValue == PROMPT_ACTION) {
            // If the permission is prompt, keep the current value. This will
            // work even on a system update, with the caveat that if a
            // ALLOW/DENY permission is changed to PROMPT then the system should
            // inform the user that he can now change a permission that he could
            // not change before.
            permValue = PermissionsHelper.getPermission(
              expandedPermNames[idx],
              aManifestURL
            );
            if (permValue === UNKNOWN_ACTION) {
              permValue = PROMPT_ACTION;
              // Overwrite default prompt permission value if defaultPromptAction
              // is declared
              if (PermissionsTable[permName].defaultPromptAction) {
                let defaultPromptAction =
                  PermissionsTable[permName].defaultPromptAction;
                let index = isCore ? "core" : appType;
                if (defaultPromptAction[index] !== undefined) {
                  permValue = defaultPromptAction[index];
                }
              }
            }
          }

          PermissionsHelper.addPermission(
            expandedPermNames[idx],
            aManifestURL,
            permValue
          );

          if (aManifestURL.startsWith("http://system.localhost")) {
            let systemStartupURL = Services.prefs.getCharPref(
              "b2g.system_startup_url"
            );
            PermissionsHelper.addPermission(
              expandedPermNames[idx],
              systemStartupURL,
              permValue
            );
          }
        }
      }
    } catch (ex) {
      debug(`Caught app install permissions error for ${aManifestURL} : ${ex}`);
      console.error(ex);
      if (aOnError) {
        aOnError();
      }
    }
  },

  uninstallPermissions(aManifestURL) {
    PermissionsHelper.removeAllForURI(aManifestURL);
  },
};

ChromeUtils.registerWindowActor("PermissionsManager", {
  includeChrome: true,
  allFrames: true,
  parent: {
    moduleURI: "resource://gre/modules/PermissionsManagerParent.sys.mjs",
    messages: [
      "PermissionsManager:AddPermission",
      "PermissionsManager:GetPermission",
      "PermissionsManager:IsExplicit",
    ],
  },
  child: {
    moduleURI: "resource://gre/modules/PermissionsManagerChild.sys.mjs",
  },
});
