/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "notificationStorage",
  "@mozilla.org/notificationStorage;1",
  "nsINotificationStorage"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "serviceWorkerManager",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

function debug(str) {
  dump("=*= AlertsHelper.jsm : " + str + "\n");
}

// const kNotificationIconSize = 128;

// const kDesktopNotificationPerm = "desktop-notification";

const kDesktopNotification = "desktop-notification";
const kDesktopNotificationShow = "desktop-notification-show";
const kDesktopNotificationClick = "desktop-notification-click";
const kDesktopNotificationClose = "desktop-notification-close";

const kTopicAlertClickCallback = "alertclickcallback";
const kTopicAlertShow = "alertshow";
const kTopicAlertFinished = "alertfinished";

const kMessageAppNotificationSend = "app-notification-send";
const kMessageAppNotificationReturn = "app-notification-return";
const kMessageAlertNotificationSend = "alert-notification-send";
const kMessageAlertNotificationClose = "alert-notification-close";

const kMessages = [
  kMessageAppNotificationSend,
  kMessageAlertNotificationSend,
  kMessageAlertNotificationClose,
];

var AlertsHelper = {
  _listeners: {},
  _embedderNotifications: {},

  init() {
    Services.obs.addObserver(this, "xpcom-shutdown");
    Services.obs.addObserver(embedderNotifications => {
      this._embedderNotifications = embedderNotifications.wrappedJSObject;
    }, "web-embedder-notifications");
    for (let message of kMessages) {
      Services.ppmm.addMessageListener(message, this);
    }
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        for (let message of kMessages) {
          Services.ppmm.removeMessageListener(message, this);
        }
        break;
    }
  },

  handleEvent(evt) {
    let detail = evt.detail;

    switch (detail.type) {
      case kDesktopNotificationShow:
      case kDesktopNotificationClick:
      case kDesktopNotificationClose:
        this.handleNotificationEvent(detail);
        break;
      default:
        debug("FIXME: Unhandled notification event: " + detail.type);
        break;
    }
  },

  handleNotificationEvent(detail) {
    if (!detail || !detail.id) {
      return;
    }

    let uid = detail.id;
    let listener = this._listeners[uid];
    if (!listener) {
      return;
    }

    let topic;
    if (detail.type === kDesktopNotificationClick) {
      topic = kTopicAlertClickCallback;
    } else if (detail.type === kDesktopNotificationShow) {
      topic = kTopicAlertShow;
    } else {
      /* kDesktopNotificationClose */
      topic = kTopicAlertFinished;
    }

    if (listener.cookie) {
      try {
        listener.observer.observe(null, topic, listener.cookie);
      } catch (e) {}
    } else {
      try {
        listener.mm.sendAsyncMessage(kMessageAppNotificationReturn, {
          uid,
          topic,
          target: listener.target,
          extra: detail.action || "",
        });
      } catch (e) {
        // The non-empty serviceWorkerRegistrationScope means the notification
        // is issued by service worker, so deal with this listener
        // via serviceWorkerManager
        if (
          listener.serviceWorkerRegistrationScope.length &&
          detail.type !== kDesktopNotificationShow
        ) {
          const scope = listener.serviceWorkerRegistrationScope;
          const originAttr = ChromeUtils.createOriginAttributesFromOrigin(
            scope
          );
          const originSuffix = ChromeUtils.originAttributesToSuffix(originAttr);
          let eventName;

          if (detail.type === kDesktopNotificationClick) {
            eventName = "notificationclick";
          } else if (detail.type === kDesktopNotificationClose) {
            eventName = "notificationclose";
          }

          if (eventName == "notificationclick") {
            let userAction = "";
            if (detail.action && typeof detail.action === "string") {
              userAction = detail.action;
            }
            lazy.serviceWorkerManager.sendNotificationClickEvent(
              originSuffix,
              scope,
              listener.dbId,
              listener.title,
              listener.dir,
              listener.lang,
              listener.text,
              listener.tag,
              listener.iconURL,
              listener.imageURL,
              listener.dataObj || undefined,
              listener.requireInteraction,
              listener.actions,
              userAction,
              listener.silent,
              listener.mozbehavior
            );
          } else if (eventName == "notificationclose") {
            lazy.serviceWorkerManager.sendNotificationCloseEvent(
              originSuffix,
              scope,
              listener.dbId,
              listener.title,
              listener.dir,
              listener.lang,
              listener.text,
              listener.tag,
              listener.iconURL,
              listener.imageURL,
              listener.dataObj || undefined,
              listener.requireInteraction,
              listener.actions,
              listener.silent,
              listener.mozbehavior
            );
          }
        }
        if (detail.type === kDesktopNotificationClose && listener.dbId) {
          lazy.notificationStorage.delete(listener.origin, listener.dbId);
        }
      }
    }

    // we"re done with this notification
    if (detail.type === kDesktopNotificationClose) {
      delete this._listeners[uid];
    }
  },

  registerListener(alertId, cookie, alertListener) {
    this._listeners[alertId] = { observer: alertListener, cookie };
  },

  registerAppListener(uid, listener) {
    this._listeners[uid] = listener;
  },

  deserializeStructuredClone(dataString) {
    if (!dataString) {
      return null;
    }
    let scContainer = Cc[
      "@mozilla.org/docshell/structured-clone-container;1"
    ].createInstance(Ci.nsIStructuredCloneContainer);

    // The maximum supported structured-clone serialization format version
    // as defined in "js/public/StructuredClone.h"
    let JS_STRUCTURED_CLONE_VERSION = 8;
    scContainer.initFromBase64(dataString, JS_STRUCTURED_CLONE_VERSION);
    let dataObj = scContainer.deserializeToJsval();

    // We have to check whether dataObj contains DOM objects (supported by
    // nsIStructuredCloneContainer, but not by Cu.cloneInto), e.g. ImageData.
    // After the structured clone callback systems will be unified, we'll not
    // have to perform this check anymore.
    try {
      Cu.cloneInto(dataObj, {});
    } catch (e) {
      dataObj = null;
    }

    return dataObj;
  },

  showNotification(
    iconURL,
    imageURL,
    title,
    text,
    textClickable,
    cookie,
    uid,
    dir,
    lang,
    dataObj,
    origin,
    timestamp,
    requireInteraction,
    actions,
    silent,
    behavior,
    serviceWorkerRegistrationScope
  ) {
    if (
      !this._embedderNotifications ||
      !this._embedderNotifications.showNotification
    ) {
      debug(`No embedder support for 'showNotification()'`);
      return;
    }
    let actionsObj;
    try {
      actionsObj = JSON.parse(actions);
    } catch (e) {}

    this._embedderNotifications.showNotification({
      type: kDesktopNotification,
      id: uid,
      icon: iconURL,
      image: imageURL,
      title,
      text,
      dir,
      lang,
      origin,
      timestamp,
      data: dataObj,
      requireInteraction,
      actions: actionsObj || [],
      silent,
      mozbehavior: behavior,
      serviceWorkerRegistrationScope,
    });
  },

  showAlertNotification(aMessage) {
    let data = aMessage.data;
    let currentListener = this._listeners[data.name];
    if (currentListener && currentListener.observer) {
      currentListener.observer.observe(
        null,
        kTopicAlertFinished,
        currentListener.cookie
      );
    }

    let dataObj = this.deserializeStructuredClone(data.dataStr);
    this.registerListener(data.name, data.cookie, data.alertListener);
    this.showNotification(
      data.imageURL,
      data.title,
      data.text,
      data.textClickable,
      data.cookie,
      data.name,
      data.dir,
      data.lang,
      dataObj,
      null,
      data.inPrivateBrowsing
    );
  },

  showAppNotification(aMessage) {
    let data = aMessage.data;
    let details = data.details;
    let dataObj = this.deserializeStructuredClone(details.data);
    let listener = {
      mm: aMessage.target,
      title: data.title,
      text: data.text,
      origin: details.origin,
      iconURL: data.iconURL,
      imageURL: data.imageURL,
      lang: details.lang || undefined,
      id: details.id || undefined,
      dbId: details.dbId || undefined,
      dir: details.dir || undefined,
      tag: details.tag || undefined,
      timestamp: details.timestamp || undefined,
      dataObj: details.data || undefined,
      requireInteraction: details.requireInteraction,
      actions: details.actions || "[]",
      silent: details.silent,
      serviceWorkerRegistrationScope: details.serviceWorkerRegistrationScope,
    };
    this.registerAppListener(data.uid, listener);
    this.showNotification(
      data.iconURL,
      data.imageURL,
      data.title,
      data.text,
      details.textClickable,
      null,
      data.uid,
      details.dir,
      details.lang,
      dataObj,
      details.origin,
      details.timestamp,
      details.requireInteraction,
      details.actions,
      details.silent,
      details.mozbehavior,
      details.serviceWorkerRegistrationScope
    );
  },

  closeAlert(name) {
    if (
      !this._embedderNotifications ||
      !this._embedderNotifications.closeNotification
    ) {
      debug(`No embedder support for 'closeNotification()'`);
      return;
    }

    this._embedderNotifications.closeNotification(name);
  },

  receiveMessage(aMessage) {
    // TODO: Need a DesktopNotification permission check in here which gecko48
    // is doing

    switch (aMessage.name) {
      case kMessageAppNotificationSend:
        this.showAppNotification(aMessage);
        break;

      case kMessageAlertNotificationSend:
        this.showAlertNotification(aMessage);
        break;

      case kMessageAlertNotificationClose:
        this.closeAlert(aMessage.data.name);
        break;
    }
  },
};

AlertsHelper.init();

export var AlertsEventHandler = {
  click: data => {
    let event = {};
    event.detail = {
      type: "desktop-notification-click",
      id: data.id,
      action: data.action || "",
    };
    AlertsHelper.handleEvent(event);
  },

  close: id => {
    let event = {};
    event.detail = {
      type: "desktop-notification-close",
      id,
    };
    AlertsHelper.handleEvent(event);
  },
};
