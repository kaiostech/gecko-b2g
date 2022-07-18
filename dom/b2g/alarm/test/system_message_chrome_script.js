/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const systemMessenger = Cc["@mozilla.org/system-message-internal;1"].getService(
  Ci.nsISystemMessagesInternal
);

addMessageListener("trigger-register-page", function(aData) {
  systemMessenger.registerPage(
    aData.type,
    Services.io.newURI(aData.pageURL),
    Services.io.newURI(aData.manifestURL)
  );
  sendAsyncMessage("page-registered");
});
