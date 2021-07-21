/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["WebExtensionsEmbedding", "mobileWindowTracker"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
});

async function filterPromptPermissions(aPermissions) {
  if (!aPermissions) {
    return [];
  }
  const promptPermissions = [];
  for (const permission of aPermissions) {
    if (!(await Extension.shouldPromptFor(permission))) {
      continue;
    }
    promptPermissions.push(permission);
  }
  return promptPermissions;
}

// From https://searchfox.org/mozilla-central/rev/a166f59fba89fc70ebfab287f4edb8e05ed4f6da/mobile/android/modules/geckoview/GeckoViewWebExtension.jsm#323
async function exportExtension(aAddon, aPermissions, aSourceURI) {
  // First, let's make sure the policy is ready if present
  let policy = WebExtensionPolicy.getByID(aAddon.id);
  if (policy?.readyPromise) {
    policy = await policy.readyPromise;
  }
  const {
    creator,
    description,
    homepageURL,
    signedState,
    name,
    icons,
    version,
    optionsURL,
    optionsType,
    isRecommended,
    blocklistState,
    userDisabled,
    embedderDisabled,
    temporarilyInstalled,
    isActive,
    isBuiltin,
    id,
  } = aAddon;
  let creatorName = null;
  let creatorURL = null;
  if (creator) {
    const { name, url } = creator;
    creatorName = name;
    creatorURL = url;
  }
  const openOptionsPageInTab = optionsType === AddonManager.OPTIONS_TYPE_TAB;
  const disabledFlags = [];
  if (userDisabled) {
    disabledFlags.push("userDisabled");
  }
  if (blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
    disabledFlags.push("blocklistDisabled");
  }
  if (embedderDisabled) {
    disabledFlags.push("appDisabled");
  }
  const baseURL = policy ? policy.getURL() : "";
  const privateBrowsingAllowed = policy ? policy.privateBrowsingAllowed : false;
  const promptPermissions = aPermissions
    ? await filterPromptPermissions(aPermissions.permissions)
    : [];
  return {
    webExtensionId: id,
    locationURI: aSourceURI != null ? aSourceURI.spec : "",
    isBuiltIn: isBuiltin,
    metaData: {
      origins: aPermissions ? aPermissions.origins : [],
      promptPermissions,
      description,
      enabled: isActive,
      temporary: temporarilyInstalled,
      disabledFlags,
      version,
      creatorName,
      creatorURL,
      homepageURL,
      name,
      optionsPageURL: optionsURL,
      openOptionsPageInTab,
      isRecommended,
      blocklistState,
      signedState,
      icons,
      baseURL,
      privateBrowsingAllowed,
    },
  };
}

class WebExtensionsEmbeddingImpl {
  constructor() {
    this.delegate = null;

    Services.obs.addObserver(this, "webextension-permission-prompt");
  }

  log(msg) {
    console.log(`ext-Embedding: ${msg}`);
  }

  error(msg) {
    console.error(`ext-Embedding: ${msg}`);
  }

  setDelegate(delegate) {
    this.log(`setDelegate`);
    this.delegate = delegate;
    this.delegate.setProvider(this);
  }

  observe(aSubject, aTopic, aData) {
    // console.log(`ext-install observe ${aTopic}`);

    switch (aTopic) {
      case "webextension-permission-prompt": {
        const { info } = aSubject.wrappedJSObject;
        const { addon, install } = info;
        this.permissionPrompt(install, addon, info);
        break;
      }
    }
  }

  async permissionPrompt(aInstall, aAddon, aInfo) {
    const { sourceURI } = aInstall;
    const { permissions } = aInfo;

    this.log(`permissionPrompt`);

    if (sourceURI.host == "shared.localhost") {
      // Always allow installation of local shared extensions without
      // prompting.
      aInfo.resolve();
    } else if (!this.delegate) {
      // When no delegate is set, reject all other installations.
      aInfo.reject();
    } else {
      // Let the delegate decide.
      const extension = await exportExtension(aAddon, permissions, sourceURI);

      this.delegate
        .permissionPrompt(extension)
        .then(aInfo.resolve, aInfo.reject);
    }
  }

  async createNewTab({
    extensionId,
    browsingContextGroupId,
    createProperties,
  } = {}) {
    this.log(`createNewTab`);
    return this.delegate.createNewTab({
      extensionId,
      browsingContextGroupId,
      createProperties,
    });
  }

  async updateTab({ nativeTab, extensionId, updateProperties } = {}) {
    this.log(`updateTab`);
    return this.delegate.updateTab({ nativeTab, extensionId, updateProperties });
  }

  // Internal methods.
  sendRequest(extension, tabId, data) {
    // this.log(`sendRequest ${extension} ${tabId} ${JSON.stringify(data)}`);

    if (!this.delegate) {
      this.error(`sendRequest: no delegate set, dropping '${data.type}'`);
      return;
    }

    switch (data.type) {
      case "BrowserAction:Update":
        this.delegate.updateBrowserAction(
          extension.id,
          extension.policy.browsingContextGroupId,
          tabId,
          data.action
        );
        break;
      default:
        this.error(`sendRequest: Unknown request: '${data.type}'`);
        break;
    }
  }

  // Provider methods.
  browserActionClick(id) {
    // TODO: hook up to the API.
    this.log(`browserActionClick ${id}`);
  }
}

const WebExtensionsEmbedding = new WebExtensionsEmbeddingImpl();

class MobileWindowTracker extends EventEmitter {
  constructor() {
    super();
    this._topWindow = null;
    this._topNonPBWindow = null;
  }

  get topWindow() {
    if (this._topWindow) {
      return this._topWindow.get();
    }
    return null;
  }

  get topNonPBWindow() {
    if (this._topNonPBWindow) {
      return this._topNonPBWindow.get();
    }
    return null;
  }

  setTabActive(aWindow, aActive) {
    const { browser, tab, docShell } = aWindow;
    tab.active = aActive;

    if (aActive) {
      this._topWindow = Cu.getWeakReference(aWindow);
      const isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
      if (!isPrivate) {
        this._topNonPBWindow = this._topWindow;
      }
      // this.emit("tab-activated", {
      //   windowId: docShell.outerWindowID,
      //   tabId: tab.id,
      //   isPrivate,
      // });
    }
  }
}

const mobileWindowTracker = new MobileWindowTracker();
