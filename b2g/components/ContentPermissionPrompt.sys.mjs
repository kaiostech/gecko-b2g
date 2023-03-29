/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(str) {
  dump("-*- ContentPermissionPrompt: " + str + "\n");
}

const PERM_VALUES = ["unknown", "allow", "deny", "prompt"];
const PROMPT_FOR_UNKNOWN = [
  "audio-capture",
  "desktop-notification",
  "geolocation",
  "video-capture",
];
const { defaultPermissions } = ChromeUtils.import(
  "resource://gre/modules/PermissionsTable.jsm"
);
// Due to privary issue, permission requests like GetUserMedia should prompt
// every time instead of providing session persistence.
const PERMISSION_NO_SESSION = ["audio-capture", "video-capture"];

/**
 * Determine if a permission should be prompt to user or not.
 *
 * @param {string} perm requested permission
 * @param {string} action the action according to principal
 * @returns {bool} true if prompt is required
 */
function shouldPrompt(perm, action) {
  return (
    action == Ci.nsIPermissionManager.PROMPT_ACTION ||
    (action == Ci.nsIPermissionManager.UNKNOWN_ACTION &&
      PROMPT_FOR_UNKNOWN.includes(perm))
  );
}

/**
 * Create the default choices for the requested permissions
 *
 * @param {object} typesInfo requested permissions
 * @returns {object} the default choices for permissions with options, return
 *                    undefined if no option in all requested permissions.
 */
function buildDefaultChoices(typesInfo) {
  let choices;
  for (let type of typesInfo) {
    if (type.options.length) {
      if (!choices) {
        choices = {};
      }
      choices[type.permission] = type.options[0];
    }
  }
  return choices;
}

/**
 * Check if the requester has default permissions of installed app.
 *
 * @param {principal} principal of the permission requester
 */
function hasDefaultPermissions(principal) {
  return defaultPermissions.every(permission => {
    let perm = Services.perms.testExactPermissionFromPrincipal(
      principal,
      permission
    );
    return perm == Ci.nsIPermissionManager.ALLOW_ACTION;
  });
}

/**
 * Update the permissions to PermissionManager
 *
 * @param {object} typesInfo Requested permissions and their properties
 * @param {bool} remember Permanently remember the decision or not
 * @param {bool} granted Granted or not
 * @param {principal} principal of the permission requester
 */
function rememberPermission(typesInfo, remember, granted, principal) {
  debug(
    `rememberPermission ${JSON.stringify(
      typesInfo
    )} remember:${remember} granted:${granted} ${principal.origin}`
  );
  let action = granted
    ? Ci.nsIPermissionManager.ALLOW_ACTION
    : Ci.nsIPermissionManager.DENY_ACTION;

  typesInfo.forEach(perm => {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1177242
    if (perm.permission == "video-capture") {
      Services.perms.addFromPrincipal(
        principal,
        "MediaManagerVideo",
        Ci.nsIPermissionManager.ALLOW_ACTION,
        Ci.nsIPermissionManager.EXPIRE_SESSION
      );
    }

    // Since only `installed apps` are listed in permission settings UI,
    // `installed apps` and `webpages` are handled differently.
    if (hasDefaultPermissions(principal)) {
      // For installed apps, set the permission permanently
      // only if `remember` is set to true
      if (remember) {
        Services.perms.addFromPrincipal(principal, perm.permission, action);
      }
    } else if (!PERMISSION_NO_SESSION.includes(perm.permission)) {
      // For webpages, we could not permanently set the permission, since
      // there is no other UI for user to revoke. However, we still set the
      // permission to this session, to avoid endless prompts.
      Services.perms.addFromPrincipal(
        principal,
        perm.permission,
        action,
        Ci.nsIPermissionManager.EXPIRE_SESSION,
        0
      );
    }
  });
}

function getRequestTarget(request) {
  // request.element is defined for OOP content, while request.window
  // is defined for In-Process content.
  if (request.element) {
    return request.element;
  } else if (
    request.window &&
    request.window.docShell &&
    request.window.docShell.chromeEventHandler
  ) {
    return request.window.docShell.chromeEventHandler;
  }
  debug(`Unexpected target: ${request.element} ${request.window}`);
  return null;
}

function sendToBrowserWindow(requestAction, request, typesInfo, callback) {
  let target = getRequestTarget(request);
  if (!target) {
    debug("!target, cancel directly");
    request.cancel();
    return;
  }

  let uuid = Services.uuid.generateUUID().toString();
  let requestId = `permission-prompt-${uuid}`;

  let permissions = {};
  for (let i in typesInfo) {
    permissions[typesInfo[i].permission] = {
      action: PERM_VALUES[typesInfo[i].action],
      options: typesInfo[i].options,
    };
  }

  if (requestAction == "prompt") {
    debug("requestAction == prompt, addEventListener on " + requestId);
    target.addEventListener(requestId, callback, { once: true });
  }

  if (target && target.dispatchEvent) {
    target.dispatchEvent(
      new CustomEvent("promptpermission", {
        bubbles: true,
        detail: {
          requestAction,
          permissions,
          requestId,
          origin: request.principal.origin,
        },
      })
    );
  } else {
    debug("Error: !target or !target.dispatchEvent");
  }
}

function getIsVisible(request) {
  if (request.element) {
    return request.element.docShellIsActive;
  } else if (request.window && request.window.document) {
    return request.window.document.hidden === false;
  }
  debug(`Unexpected target: ${request.element} ${request.window}`);
  return false;
}

export function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  handleExistingPermission: function handleExistingPermission(
    request,
    typesInfo
  ) {
    typesInfo.forEach(function(type) {
      type.action = Services.perms.testExactPermissionFromPrincipal(
        request.principal,
        type.permission
      );
      // Switch UNKNOWN_ACTION to PROMPT_ACTION for permissions in PROMPT_FOR_UNKNOWN.
      if (shouldPrompt(type.permission, type.action)) {
        type.action = Ci.nsIPermissionManager.PROMPT_ACTION;
      }

      if (type.permission == "video-capture") {
        Services.perms.addFromPrincipal(
          request.principal,
          "MediaManagerVideo",
          Ci.nsIPermissionManager.ALLOW_ACTION,
          Ci.nsIPermissionManager.EXPIRE_SESSION
        );
      }
    });

    // If all permissions are allowed already, call allow() without prompting.
    if (
      typesInfo.every(
        type => type.action == Ci.nsIPermissionManager.ALLOW_ACTION
      )
    ) {
      debug("all permissions in the request are allowed.");
      request.allow(buildDefaultChoices(typesInfo));
      return true;
    }

    // If some of the permissions are DENY_ACTION or UNKNOWN_ACTION,
    // call cancel() without prompting.
    if (
      typesInfo.some(type =>
        [
          Ci.nsIPermissionManager.DENY_ACTION,
          Ci.nsIPermissionManager.UNKNOWN_ACTION,
        ].includes(type.action)
      )
    ) {
      debug(
        "some of permissions in the request are denied, or !shouldPrompt()."
      );
      request.cancel();
      return true;
    }

    return false;
  },

  prompt(request) {
    // Initialize the typesInfo and set the default value.
    /**
     * typesInfo is an array of {permission, action, options} which keeps
     * the information of each permission. This arrary is initialized in
     * ContentPermissionPrompt.prompt and used among functions.
     *
     * typesInfo[].permission : permission name
     * typesInfo[].action     : the action of this permission
     * typesInfo[].options    : the array of available options for user to choose from
     */
    let typesInfo = [];
    let perms = request.types.QueryInterface(Ci.nsIArray);
    for (let idx = 0; idx < perms.length; idx++) {
      let perm = perms.queryElementAt(idx, Ci.nsIContentPermissionType);
      debug(
        `prompt request.types[${idx}]: ${perm.type} ${request.principal.origin}`
      );
      let tmp = {
        permission: perm.type,
        options: [],
        action: Ci.nsIPermissionManager.UNKNOWN_ACTION,
      };

      // Append available options, if any.
      try {
        let options = perm.options.QueryInterface(Ci.nsIArray);
        for (let i = 0; i < options.length; i++) {
          let option = options.queryElementAt(i, Ci.nsISupportsString).data;
          debug(`options[${i}]: ${option}`);
          tmp.options.push(option);
        }
      } catch (e) {
        debug(`ignore options error and continue. ${e}`);
      }
      typesInfo.push(tmp);
    }

    if (request.principal.isSystemPrincipal) {
      request.allow(buildDefaultChoices(typesInfo));
      return;
    }

    if (!typesInfo.length) {
      request.cancel();
      return;
    }

    // returns true if the request was handled
    // All type.action in typesInfo are also obtained from Services.perms in this function.
    if (this.handleExistingPermission(request, typesInfo)) {
      return;
    }

    let target = getRequestTarget(request);
    if (!target) {
      debug("!target, cancel directly");
      request.cancel();
      return;
    }

    let visibilitychangeHandler = function(event) {
      debug(`callback of ${event.type} ${JSON.stringify(event.detail)}`);
      if (!getIsVisible(request)) {
        debug("visibilitychange !getIsVisible, cancel.");
        target.removeEventListener(
          "webview-visibilitychange",
          visibilitychangeHandler
        );
        request.cancel();
        sendToBrowserWindow("cancel", request, typesInfo);
      }
    };

    if (getIsVisible(request)) {
      target.addEventListener(
        "webview-visibilitychange",
        visibilitychangeHandler
      );
      sendToBrowserWindow("prompt", request, typesInfo, event => {
        debug(`callback of ${event.type} ${JSON.stringify(event.detail)}`);
        target.removeEventListener(
          "webview-visibilitychange",
          visibilitychangeHandler
        );
        let result = event.detail;
        rememberPermission(
          typesInfo,
          result.remember,
          result.granted,
          request.principal
        );

        if (result.granted) {
          request.allow(result.choices);
        } else {
          request.cancel();
        }
      });
    } else {
      debug("target is not visible, cancel request.");
      request.cancel();
    }
  },

  classID: Components.ID("{8c719f03-afe0-4aac-91ff-6c215895d467}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionPrompt]),
};

//module initialization
