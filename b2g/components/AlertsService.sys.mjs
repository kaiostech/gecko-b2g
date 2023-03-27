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

// function debug(str) {
//   dump("=*= AlertsService.js : " + str + "\n");
// }

// -----------------------------------------------------------------------
// Alerts Service
// -----------------------------------------------------------------------

const kMessageAppNotificationSend = "app-notification-send";
const kMessageAppNotificationReturn = "app-notification-return";
const kMessageAlertNotificationSend = "alert-notification-send";
const kMessageAlertNotificationClose = "alert-notification-close";

const kTopicAlertShow = "alertshow";
const kTopicAlertFinished = "alertfinished";
const kTopicAlertClickCallback = "alertclickcallback";

export function AlertsService() {
  Services.obs.addObserver(this, "xpcom-shutdown");
  Services.cpmm.addMessageListener(kMessageAppNotificationReturn, this);
}

AlertsService.prototype = {
  classID: Components.ID("{fe33c107-82a4-41d6-8c64-5353267e04c9}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAlertsService,
    Ci.nsIAppNotificationService,
    Ci.nsIObserver,
  ]),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.cpmm.removeMessageListener(
          kMessageAppNotificationReturn,
          this
        );
        break;
    }
  },

  // nsIAlertsService
  showAlert(aAlert, aAlertListener) {
    if (!aAlert) {
      return;
    }
    Services.cpmm.sendAsyncMessage(kMessageAlertNotificationSend, {
      imageURL: aAlert.imageURL,
      title: aAlert.title,
      text: aAlert.text,
      clickable: aAlert.textClickable,
      cookie: aAlert.cookie,
      listener: aAlertListener,
      id: aAlert.name,
      dir: aAlert.dir,
      lang: aAlert.lang,
      dataStr: aAlert.data,
      inPrivateBrowsing: aAlert.inPrivateBrowsing,
    });
  },

  showAlertNotification(
    aImageUrl,
    aTitle,
    aText,
    aTextClickable,
    aCookie,
    aAlertListener,
    aName,
    aBidi,
    aLang,
    aDataStr,
    aPrincipal,
    aInPrivateBrowsing
  ) {
    let alert = Cc["@mozilla.org/alert-notification;1"].createInstance(
      Ci.nsIAlertNotification
    );

    alert.init(
      aName,
      aImageUrl,
      aTitle,
      aText,
      aTextClickable,
      aCookie,
      aBidi,
      aLang,
      aDataStr,
      aPrincipal,
      aInPrivateBrowsing
    );

    this.showAlert(alert, aAlertListener);
  },

  closeAlert(aName) {
    Services.cpmm.sendAsyncMessage(kMessageAlertNotificationClose, {
      name: aName,
    });
  },

  // nsIAppNotificationService
  showAppNotification(
    aIconURL,
    aImageURL,
    aTitle,
    aText,
    aAlertListener,
    aDetails
  ) {
    let uid =
      aDetails.id == ""
        ? "app-notif-" + Services.uuid.generateUUID()
        : aDetails.id;

    this._listeners[uid] = {
      observer: aAlertListener,
      title: aTitle,
      text: aText,
      origin: aDetails.origin,
      iconURL: aIconURL,
      imageURL: aImageURL,
      lang: aDetails.lang || undefined,
      id: aDetails.id || undefined,
      dbId: aDetails.dbId || undefined,
      dir: aDetails.dir || undefined,
      tag: aDetails.tag || undefined,
      timestamp: aDetails.timestamp || undefined,
      dataObj: aDetails.data || undefined,
      requireInteraction: aDetails.requireInteraction,
      actions: aDetails.actions || "[]",
      silent: aDetails.silent,
      mozbehavior: aDetails.mozbehavior,
      serviceWorkerRegistrationScope: aDetails.serviceWorkerRegistrationScope,
    };

    Services.cpmm.sendAsyncMessage(kMessageAppNotificationSend, {
      iconURL: aIconURL,
      imageURL: aImageURL,
      title: aTitle,
      text: aText,
      uid,
      details: aDetails,
    });
  },

  // AlertsService.js custom implementation
  _listeners: [],

  receiveMessage(aMessage) {
    let data = aMessage.data;
    let listener = this._listeners[data.uid];
    if (aMessage.name !== kMessageAppNotificationReturn || !listener) {
      return;
    }

    let topic = data.topic;
    let userAction = "";
    if (data.extra && typeof data.extra === "string") {
      userAction = data.extra;
    }

    try {
      listener.observer.observe(null, topic, userAction);
    } catch (e) {
      // The non-empty serviceWorkerRegistrationScope means the notification
      // is issued by service worker, so deal with this listener
      // via serviceWorkerManager
      if (
        listener.serviceWorkerRegistrationScope.length &&
        topic !== kTopicAlertShow
      ) {
        const scope = listener.serviceWorkerRegistrationScope;
        const originAttr = ChromeUtils.createOriginAttributesFromOrigin(scope);
        const originSuffix = ChromeUtils.originAttributesToSuffix(originAttr);
        let eventName;

        if (topic == kTopicAlertClickCallback) {
          eventName = "notificationclick";
        } else if (topic === kTopicAlertFinished) {
          eventName = "notificationclose";
        }

        if (eventName == "notificationclick") {
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
      if (topic === kTopicAlertFinished && listener.dbId) {
        lazy.notificationStorage.delete(listener.origin, listener.dbId);
      }
    }

    // we're done with this notification
    if (topic === kTopicAlertFinished) {
      delete this._listeners[data.uid];
    }
  },
};
