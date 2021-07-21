/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

console.log(`ext-browserAction.js loaded`);

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-b2g.js */

XPCOMUtils.defineLazyModuleGetters(this, {
  WebExtensionsEmbedding: "resource://gre/modules/WebExtensionsEmbedding.jsm",
});

const { BrowserActionBase } = ChromeUtils.import(
  "resource://gre/modules/ExtensionActions.jsm"
);

// TODO: Move to a common file
// Provides common logic between page and browser actions
class ExtensionActionHelper {
  constructor({
    tabTracker,
    windowTracker,
    tabContext,
    properties,
    extension,
  }) {
    this.tabTracker = tabTracker;
    this.windowTracker = windowTracker;
    this.tabContext = tabContext;
    this.properties = properties;
    this.extension = extension;
  }

  getTab(aTabId) {
    if (aTabId !== null) {
      return this.tabTracker.getTab(aTabId);
    }
    return null;
  }

  getWindow(aWindowId) {
    if (aWindowId !== null) {
      return this.windowTracker.getWindow(aWindowId);
    }
    return null;
  }

  extractProperties(aAction) {
    const merged = {};
    for (const p of this.properties) {
      merged[p] = aAction[p];
    }
    return merged;
  }

  sendRequest(aTabId, aData) {
    console.log(`ext-ExtensionActionHelper::sendRequest ${aTabId} ${JSON.stringify(aData)}`);
    return WebExtensionsEmbedding.sendRequest(this.extension, aTabId, aData);
  }
}

const BROWSER_ACTION_PROPERTIES = [
  "title",
  "icon",
  "popup",
  "badgeText",
  "badgeBackgroundColor",
  "badgeTextColor",
  "enabled",
  "patternMatching",
];

class BrowserAction extends BrowserActionBase {
  constructor(extension, clickDelegate) {
    const inParent =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT;
    console.log(`ext-BrowserAction::constructor inParent=${inParent}`);

    const tabContext = new TabContext(tabId => this.getContextData(null));
    super(tabContext, extension);
    this.clickDelegate = clickDelegate;
    this.helper = new ExtensionActionHelper({
      extension,
      tabTracker,
      windowTracker,
      tabContext,
      properties: BROWSER_ACTION_PROPERTIES,
    });
  }

  updateOnChange(tab) {
    console.log(`ext-BrowserAction::updateOnChange`);
    const tabId = tab ? tab.id : null;
    const action = tab
      ? this.getContextData(tab)
      : this.helper.extractProperties(this.globals);
    this.helper.sendRequest(tabId, {
      action,
      type: "BrowserAction:Update",
    });
  }

  openPopup() {
    console.log(`ext-BrowserAction::openPopup`);
    const tab = tabTracker.activeTab;
    const popupUri = this.triggerClickOrPopup(tab);
    const actionObject = this.getContextData(tab);
    const action = this.helper.extractProperties(actionObject);
    this.helper.sendRequest(tab.id, {
      action,
      type: "BrowserAction:OpenPopup",
      popupUri,
    });
  }

  triggerClickOrPopup(tab = tabTracker.activeTab) {
    return super.triggerClickOrPopup(tab);
  }

  getTab(tabId) {
    return this.helper.getTab(tabId);
  }

  getWindow(windowId) {
    return this.helper.getWindow(windowId);
  }

  dispatchClick() {
    this.clickDelegate.onClick();
  }
}

this.browserAction = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    const { extension } = this;
    this.action = new BrowserAction(extension, this);
    await this.action.loadIconData();

    // GeckoViewWebExtension.browserActions.set(extension, this.action);

    // Notify the embedder of this action
    this.action.updateOnChange(null);
  }

  onShutdown() {
    const { extension } = this;
    this.action.onShutdown();
    // GeckoViewWebExtension.browserActions.delete(extension);
  }

  onClick() {
    this.emit("click", tabTracker.activeTab);
  }

  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;
    const { action } = this;
    const namespace =
      extension.manifestVersion < 3 ? "browserAction" : "action";

    return {
      [namespace]: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          name: `${namespace}.onClicked`,
          register: fire => {
            const listener = (event, tab) => {
              fire.async(tabManager.convert(tab));
            };
            this.on("click", listener);
            return () => {
              this.off("click", listener);
            };
          },
        }).api(),

        openPopup: function() {
          action.openPopup();
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
