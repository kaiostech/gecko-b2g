/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(aMsg) {
  //dump(`-*- PermissionsManager: ${aMsg}\n`);
}

const PERMISSIONSETTINGS_CID = Components.ID(
  "{cd2cf7a1-f4c1-487b-8c1b-1a71c7097431}"
);

export function PermissionsManager() {
  debug("Constructor");
}

PermissionsManager.prototype = {
  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    this.window = aWindow;
  },

  createPromise(aPromiseInit) {
    return new this.window.Promise(aPromiseInit);
  },

  get(aPermName, aOrigin) {
    debug(`get ${aPermName} ${aOrigin}`);
    return this.createPromise(
      function(aResolve, aReject) {
        let actor = this.window.windowGlobalChild.getActor(
          "PermissionsManager"
        );
        actor
          .getPermission({
            type: aPermName,
            origin: aOrigin,
          })
          .then(result => {
            switch (result) {
              case Ci.nsIPermissionManager.UNKNOWN_ACTION:
                aResolve("unknown");
                break;
              case Ci.nsIPermissionManager.ALLOW_ACTION:
                aResolve("allow");
                break;
              case Ci.nsIPermissionManager.DENY_ACTION:
                aResolve("deny");
                break;
              case Ci.nsIPermissionManager.PROMPT_ACTION:
                aResolve("prompt");
                break;
              default:
                aResolve("unknown");
                break;
            }
          });
      }.bind(this)
    );
  },

  isExplicit(aPermName, aOrigin) {
    return this.createPromise(
      function(aResolve, aReject) {
        let actor = this.window.windowGlobalChild.getActor(
          "PermissionsManager"
        );
        actor.isExplicit({ type: aPermName, origin: aOrigin }).then(result => {
          debug(`isExplicit: ${aPermName} ${aOrigin} ${result}`);
          aResolve(result);
        });
      }.bind(this)
    );
  },

  set(aPermName, aPermValue, aOrigin) {
    debug(`set ${aPermName} ${aOrigin} ${aPermValue}`);
    let currentPermValue = this.get(aPermName, aOrigin);
    // Check for invalid calls so that we throw an exception rather than get
    // killed by parent process
    if (
      currentPermValue === "unknown" ||
      aPermValue === "unknown" ||
      !this.isExplicit(aPermName, aOrigin)
    ) {
      let errorMsg = `PermissionsManager: ${aPermName} is an implicit permission for ${aOrigin} or the permission isn't set`;
      console.error(errorMsg);
      throw new Components.Exception(errorMsg);
    }
    let actor = this.window.windowGlobalChild.getActor("PermissionsManager");
    let callerPrincipal = this.window.document.nodePrincipal;
    let callerOrigin = callerPrincipal.origin;
    debug(`set ${aPermName} of ${aOrigin} by ${callerOrigin}`);

    actor
      .addPermission({
        callerOrigin,
        type: aPermName,
        origin: aOrigin,
        value: aPermValue,
      })
      .then(
        () => {
          debug(`set ${aPermName} from ${aOrigin} success`);
        },
        aMsg => {
          debug(`set ${aPermName} from ${aOrigin} fail: ${aMsg}`);
        }
      );
  },

  remove(aPermName, aOrigin) {
    // PermissionsManagerParent.sys.mjs handles delete when value is "unknown"
    let actor = this.window.windowGlobalChild.getActor("PermissionsManager");

    actor
      .addPermission({
        type: aPermName,
        origin: aOrigin,
        value: "unknown",
      })
      .then(
        () => {
          debug(`remove ${aPermName} from ${aOrigin} success`);
        },
        aMsg => {
          debug(`remove ${aPermName} from ${aOrigin} fail: ${aMsg}`);
        }
      );
  },

  classID: PERMISSIONSETTINGS_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};
