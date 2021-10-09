/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["OpenNetworkNotifier"];

var gDebug = false;

const OpenNetworkNotifier = (function() {
  var openNetworkNotifier = {};

  // For at Least scans before showing notification.
  const MIN_NUMBER_SCANS_BEFORE_SHOW_NOTIFICATION = 3;
  const NOTIFICATION_REPEAT_DELAY_MS = 900 * 1000;
  var settingsEnabled = false;
  var notificationRepeatTime = 0;
  var numScansSinceNetworkStateChange = 0;

  openNetworkNotifier.isEnabled = isEnabled;
  openNetworkNotifier.setOpenNetworkNotifyEnabled = setOpenNetworkNotifyEnabled;
  openNetworkNotifier.handleOpenNetworkFound = handleOpenNetworkFound;
  openNetworkNotifier.clearPendingNotification = clearPendingNotification;
  openNetworkNotifier.setDebug = setDebug;

  function setDebug(aDebug) {
    gDebug = aDebug;
  }

  function debug(aMsg) {
    if (gDebug) {
      dump("-*- OpenNetworkNotifier: " + aMsg);
    }
  }

  function isEnabled() {
    return settingsEnabled;
  }

  function setOpenNetworkNotifyEnabled(enable) {
    debug("setOpenNetworkNotifyEnabled: " + enable);
    settingsEnabled = enable;
    clearPendingNotification();
  }

  function handleOpenNetworkFound() {
    if (!settingsEnabled) {
      return;
    }

    if (
      ++numScansSinceNetworkStateChange <=
      MIN_NUMBER_SCANS_BEFORE_SHOW_NOTIFICATION
    ) {
      setNotificationVisible(false);
      return;
    }
    // Ready to show notification.
    setNotificationVisible(true);
  }

  function setNotificationVisible(visible) {
    debug("setNotificationVisible: visible = " + visible);
    if (visible) {
      let now = Date.now();
      debug(
        "now = " + now + " , notificationRepeatTime = " + notificationRepeatTime
      );

      // Not enough time has passed to show the notification again
      if (now < notificationRepeatTime) {
        return;
      }
      notificationRepeatTime = now + NOTIFICATION_REPEAT_DELAY_MS;
      notify("opennetworknotification", { enabled: true });
    } else {
      notify("opennetworknotification", { enabled: false });
    }
  }

  function clearPendingNotification() {
    notificationRepeatTime = 0;
    numScansSinceNetworkStateChange = 0;
    setNotificationVisible(false);
  }

  function notify(eventName, eventObject) {
    var handler = openNetworkNotifier["on" + eventName];
    if (!handler) {
      return;
    }
    if (!eventObject) {
      eventObject = {};
    }
    handler.call(eventObject);
  }

  return openNetworkNotifier;
})();
