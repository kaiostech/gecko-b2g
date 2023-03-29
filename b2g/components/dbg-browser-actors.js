/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env commonjs */

"use strict";
/**
 * Fennec-specific actors with changes for B2G support.
 */

const { RootActor } = require("devtools/server/actors/root");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");
const {
  BrowserTabList,
  BrowserAddonList,
  sendShutdownEvent,
} = require("devtools/server/actors/webbrowser");
const {
  ServiceWorkerRegistrationActorList,
} = require("devtools/server/actors/worker/service-worker-registration-list");
const {
  WorkerDescriptorActorList,
} = require("devtools/server/actors/worker/worker-descriptor-actor-list");

const { ProcessActorList } = require("devtools/server/actors/process");

/**
 * Construct a root actor appropriate for use in a server running in a
 * browser on Android. The returned root actor:
 * - respects the factories registered with ActorRegistry.addGlobalActor,
 * - uses a MobileTabList to supply tab actors,
 * - sends all navigator:browser window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * @param {object} aConnection DevToolsServerConnection
 *        The conection to the client.
 */
exports.createRootActor = function createRootActor(aConnection) {
  const parameters = {
    tabList: new MobileTabList(aConnection),
    addonList: new BrowserAddonList(aConnection),
    workerList: new WorkerDescriptorActorList(aConnection, {}),
    serviceWorkerRegistrationList: new ServiceWorkerRegistrationActorList(
      aConnection
    ),
    processList: new ProcessActorList(),
    globalActorFactories: ActorRegistry.globalActorFactories,
    onShutdown: sendShutdownEvent,
  };
  return new RootActor(aConnection, parameters);
};

/**
 * A live list of BrowserTabActors representing the current browser tabs,
 * to be provided to the root actor to answer 'listTabs' requests.
 *
 * This object also takes care of listening for TabClose events and
 * onCloseWindow notifications, and exiting the BrowserTabActors concerned.
 *
 * (See the documentation for RootActor for the definition of the "live
 * list" interface.)
 *
 * @param {object} aConnection DevToolsServerConnection
 *     The connection in which this list's tab actors may participate.
 *
 * @see BrowserTabList for more a extensive description of how tab list objects
 *      work.
 */
function MobileTabList(aConnection) {
  BrowserTabList.call(this, aConnection);
}

MobileTabList.prototype = Object.create(BrowserTabList.prototype);

MobileTabList.prototype.constructor = MobileTabList;

MobileTabList.prototype._getSelectedBrowser = function(aWindow) {
  // Always return the system app as "selected browser" for now.
  // TODO: switch to the current foreground app.
  return aWindow.document.getElementById("systemapp");
};

MobileTabList.prototype._getChildren = function(aWindow) {
  // aWindow is the chrome shell window, so we build a list with all apps included in the system app.
  // Note: including the system app itself in the list doesn't work but the system app is accessible
  // by inspecting the Main Process.

  let system_app = aWindow.document.getElementById("systemapp");
  if (!system_app) {
    return [];
  }

  let web_views = system_app.contentWindow.document.querySelectorAll(
    "web-view"
  );

  let res = [];
  for (let web_view of web_views) {
    res.push(web_view.browser);
  }

  return res;
};
