/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(s) {
  //dump("-*- PermissionsManagerParent: " + s + "\n");
}

import {
  PermissionsTable,
  permissionsReverseTable,
} from "resource://gre/modules/PermissionsTable.sys.mjs";

import { PermissionsHelper } from "resource://gre/modules/PermissionsInstaller.sys.mjs";

export class PermissionsManagerParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    debug(`receiveMessage ${aMessage.name}`);
    let msg = aMessage.data;

    switch (aMessage.name) {
      case "PermissionsManager:AddPermission":
        let callerPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          msg.callerOrigin
        );
        if (
          !Services.perms.testExactPermissionFromPrincipal(
            callerPrincipal,
            "permissions"
          )
        ) {
          let errorMsg = `Add ${msg.origin}'s ${msg.type}=${msg.value} from ${msg.callerOrigin} w/o 'permissions' privileges`;
          console.error(errorMsg);
          return Promise.reject(String(errorMsg));
        }
        if (!this._internalAddPermission(msg)) {
          let errorMsg = `Change an implicit permission ${msg.origin}'s ${msg.type}=${msg.value}`;
          console.error(errorMsg);
          return Promise.reject(String(errorMsg));
        }
        return Promise.resolve();
      case "PermissionsManager:GetPermission":
        return Promise.resolve(this.getPermission(msg.origin, msg.type));
      case "PermissionsManager:IsExplicit":
        return Promise.resolve(
          this.isExplicitInPermissionsTable(msg.origin, msg.type)
        );
    }
    return undefined;
  }

  getPermission(aOrigin, aPermName) {
    let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      aOrigin
    );
    return Services.perms.testExactPermissionFromPrincipal(
      principal,
      aPermName
    );
  }

  isExplicitInPermissionsTable(aOrigin, aPermName) {
    let realPerm = permissionsReverseTable[aPermName];

    if (realPerm) {
      let appType = "pwa";
      let uri = Services.io.newURI(aOrigin);
      if (uri?.host?.endsWith(".localhost")) {
        // If the permission of core app is not specified in PermissionsTable,
        // fallback to signed app. This should align the behavior with
        // PermissionsInstaller.installPermissions.
        if (
          PermissionsHelper.isCoreApp(aOrigin) === true &&
          PermissionsTable[realPerm].core !== undefined
        ) {
          appType = "core";
        } else {
          appType = "signed";
        }
      }

      let isExplicit =
        PermissionsTable[realPerm][appType] ==
        Ci.nsIPermissionManager.PROMPT_ACTION;
      debug(`isExplicit: ${aOrigin} ${aPermName} isExplicit=${isExplicit}`);
      return isExplicit;
    }
    return false;
  }

  _isChangeAllowed(aOrigin, aPrincipal, aPermName, aAction) {
    // Bug 812289:
    // Change is allowed from a child process when all of the following
    // conditions stand true:
    //   * the action isn't "unknown" (so the change isn't a delete) if the app
    //     is installed
    //   * the permission already exists on the database
    //   * the permission is marked as explicit on the permissions table
    // Note that we *have* to check the first two conditions here because
    // permissionManager doesn't know if it's being called as a result of
    // a parent process or child process request. We could check
    // if the permission is actually explicit (and thus modifiable) or not
    // on permissionManager also but we currently don't.

    let perm = Services.perms.testExactPermissionFromPrincipal(
      aPrincipal,
      aPermName
    );
    let isExplicit = this.isExplicitInPermissionsTable(aOrigin, aPermName);
    debug(
      `_isChangeAllowed: access to ${aPrincipal.origin} ${aPermName} isExplicit=${isExplicit} got ${perm}`
    );
    return (
      aAction === "unknown" ||
      (aAction !== "unknown" &&
        perm !== Ci.nsIPermissionManager.UNKNOWN_ACTION &&
        isExplicit)
    );
  }

  _internalAddPermission(aData) {
    let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      aData.origin
    );

    let action;
    switch (aData.value) {
      case "unknown":
        action = Ci.nsIPermissionManager.UNKNOWN_ACTION;
        break;
      case "allow":
        action = Ci.nsIPermissionManager.ALLOW_ACTION;
        break;
      case "deny":
        action = Ci.nsIPermissionManager.DENY_ACTION;
        break;
      case "prompt":
        action = Ci.nsIPermissionManager.PROMPT_ACTION;
        break;
      default:
        debug(`Unsupported Action ${aData.value}`);
        action = Ci.nsIPermissionManager.UNKNOWN_ACTION;
    }

    if (
      this._isChangeAllowed(aData.origin, principal, aData.type, aData.value)
    ) {
      debug(`add: ${aData.origin} ${action}`);
      Services.perms.addFromPrincipal(principal, aData.type, action);
      return true;
    }
    debug(`add Failure: ${aData.origin} ${action}`);
    return false; // This isn't currently used, see comment on setPermission
  }
}
