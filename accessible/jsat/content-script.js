/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global content, sendAsyncMessage, addMessageListener, removeMessageListener */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

const { Logger } = ChromeUtils.importESModule(
  "resource://gre/modules/accessibility/Utils.sys.mjs"
);

XPCOMUtils.defineLazyGetter(lazy, "Presentation", function () {
  const { Presentation } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/Presentation.sys.mjs"
  );
  return Presentation;
});

XPCOMUtils.defineLazyGetter(lazy, "Utils", function () {
  const { Utils } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/Utils.sys.mjs"
  );
  return Utils;
});

XPCOMUtils.defineLazyGetter(lazy, "EventManager", function () {
  const { EventManager } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/EventManager.sys.mjs"
  );
  return EventManager;
});

XPCOMUtils.defineLazyGetter(lazy, "ContentControl", function () {
  const { ContentControl } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/ContentControl.sys.mjs"
  );
  return ContentControl;
});

XPCOMUtils.defineLazyGetter(lazy, "Roles", function () {
  const { Roles } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/Constants.sys.mjs"
  );
  return Roles;
});

XPCOMUtils.defineLazyGetter(lazy, "States", function () {
  const { States } = ChromeUtils.importESModule(
    "resource://gre/modules/accessibility/Constants.sys.mjs"
  );
  return States;
});

Logger.info("content-script.js", content.document.location);

var eventManager = null;
var contentControl = null;

function forwardToParent(aMessage) {
  // XXX: This is a silly way to make a deep copy
  let newJSON = JSON.parse(JSON.stringify(aMessage.json));
  newJSON.origin = "child";
  sendAsyncMessage(aMessage.name, newJSON);
}

function forwardToChild(aMessage, aListener, aVCPosition) {
  let acc =
    aVCPosition || lazy.Utils.getVirtualCursor(content.document).position;

  if (
    !lazy.Utils.isAliveAndVisible(acc) ||
    acc.role != lazy.Roles.INTERNAL_FRAME
  ) {
    return false;
  }

  Logger.debug(() => {
    return [
      "forwardToChild",
      Logger.accessibleToString(acc),
      aMessage.name,
      JSON.stringify(aMessage.json, null, "  "),
    ];
  });

  let mm = lazy.Utils.getMessageManager(acc.DOMNode);

  if (aListener) {
    mm.addMessageListener(aMessage.name, aListener);
  }

  // XXX: This is a silly way to make a deep copy
  let newJSON = JSON.parse(JSON.stringify(aMessage.json));
  newJSON.origin = "parent";
  if (lazy.Utils.isContentProcess) {
    // XXX: OOP content's screen offset is 0,
    // so we remove the real screen offset here.
    newJSON.x -= content.mozInnerScreenX;
    newJSON.y -= content.mozInnerScreenY;
  }
  mm.sendAsyncMessage(aMessage.name, newJSON);
  return true;
}

function activateContextMenu(aMessage) {
  let position = lazy.Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, activateContextMenu, position)) {
    let center = lazy.Utils.getBounds(position, true).center();

    let evt = content.document.createEvent("HTMLEvents");
    evt.initEvent("contextmenu", true, true);
    evt.clientX = center.x;
    evt.clientY = center.y;
    position.DOMNode.dispatchEvent(evt);
  }
}

function presentCaretChange(aText, aOldOffset, aNewOffset) {
  if (aOldOffset !== aNewOffset) {
    let msg = lazy.Presentation.textSelectionChanged(
      aText,
      aNewOffset,
      aNewOffset,
      aOldOffset,
      aOldOffset,
      true
    );
    sendAsyncMessage("AccessFu:Present", msg);
  }
}

function scroll(aMessage) {
  let position = lazy.Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, scroll, position)) {
    sendAsyncMessage("AccessFu:DoScroll", {
      bounds: lazy.Utils.getBounds(position, true),
      page: aMessage.json.page,
      horizontal: aMessage.json.horizontal,
    });
  }
}

addMessageListener("AccessFu:Start", function (m) {
  if (m.json.logLevel) {
    Logger.logLevel = Logger[m.json.logLevel];
  }

  Logger.debug("AccessFu:Start");
  if (m.json.buildApp) {
    lazy.Utils.MozBuildApp = m.json.buildApp;
  }

  addMessageListener("AccessFu:ContextMenu", activateContextMenu);
  addMessageListener("AccessFu:Scroll", scroll);

  lazy.Utils.isBrowserFrame = this.docShell.isBrowserElement || false;

  if (!contentControl) {
    contentControl = new lazy.ContentControl(this);
  }
  contentControl.start();

  if (!eventManager) {
    eventManager = new lazy.EventManager(this, contentControl);
  }
  eventManager.inTest = m.json.inTest;
  eventManager.start();

  function contentStarted() {
    let accDoc = lazy.Utils.AccRetrieval.getAccessibleFor(content.document);
    if (accDoc && !lazy.Utils.getState(accDoc).contains(lazy.States.BUSY)) {
      sendAsyncMessage("AccessFu:ContentStarted");
    } else {
      content.setTimeout(contentStarted, 0);
    }
  }

  if (m.json.inTest) {
    // During a test we want to wait for the document to finish loading for
    // consistency.
    contentStarted();
  }
});

addMessageListener("AccessFu:Stop", function (m) {
  Logger.debug("AccessFu:Stop");

  removeMessageListener("AccessFu:ContextMenu", activateContextMenu);
  removeMessageListener("AccessFu:Scroll", scroll);

  eventManager.stop();
  contentControl.stop();
});

sendAsyncMessage("AccessFu:Ready");
