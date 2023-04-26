/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

var WAP_CONSTS = ChromeUtils.import("resource://gre/modules/wap_consts.js");

const DEBUG = false; // set to true to see debug messages
const kWapSuplInitObserverTopic = "wap-supl-init";

const lazy = {};

/**
 * WAP Push decoders
 */
XPCOMUtils.defineLazyGetter(lazy, "SI", function() {
  return ChromeUtils.import("resource://gre/modules/SiPduHelper.jsm");
});

XPCOMUtils.defineLazyGetter(lazy, "SL", function() {
  return ChromeUtils.import("resource://gre/modules/SlPduHelper.jsm");
});

XPCOMUtils.defineLazyGetter(lazy, "CP", function() {
  return ChromeUtils.import("resource://gre/modules/CpPduHelper.jsm");
});

XPCOMUtils.defineLazyGetter(lazy, "WSP", function() {
  return ChromeUtils.import("resource://gre/modules/WspPduHelper.jsm");
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSystemMessenger",
  "@mozilla.org/systemmessage-service;1",
  "nsISystemMessageService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gIccService",
  "@mozilla.org/icc/iccservice;1",
  "nsIIccService"
);

XPCOMUtils.defineLazyModuleGetter(
  lazy,
  "gPhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm",
  "PhoneNumberUtils"
);

/**
 * Helpers for WAP PDU processing.
 */
const WapPushManager = {
  /**
   * Parse raw PDU data and deliver to a proper target.
   *
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Extra context for decoding.
   */
  processMessage: function processMessage(data, options) {
    try {
      lazy.WSP.PduHelper.parse(data, true, options);
      debug("options: " + JSON.stringify(options));
    } catch (ex) {
      debug("Failed to parse sessionless WSP PDU: " + ex.message);
      return;
    }

    // TODO: Support to route the push request using x-wap-application-id.
    // See 7.3. Application Addressing in WAP-235-PushOTA-20010425-a.
    // http://technical.openmobilealliance.org/tech/affiliates/wap/wap-235-pushota-20010425-a.pdf

    /**
     *
     * WAP Type            content-type                              x-wap-application-id
     * MMS                 "application/vnd.wap.mms-message"         "x-wap-application:mms.ua"
     * SI                  "text/vnd.wap.si"                         "x-wap-application:wml.ua"
     * SI(WBXML)           "application/vnd.wap.sic"                 "x-wap-application:wml.ua"
     * SL                  "text/vnd.wap.sl"                         "x-wap-application:wml.ua"
     * SL(WBXML)           "application/vnd.wap.slc"                 "x-wap-application:wml.ua"
     * Provisioning        "text/vnd.wap.connectivity-xml"           "x-wap-application:wml.ua"
     * Provisioning(WBXML) "application/vnd.wap.connectivity-wbxml"  "x-wap-application:wml.ua"
     * SUPL INIT           "application/vnd.omaloc-supl-init"        "x-oma-application:ulp.ua"
     *
     * @see http://technical.openmobilealliance.org/tech/omna/omna-wsp-content-type.aspx
     */
    let contentType = options.headers["content-type"].media;
    let msg;
    let authInfo = null;
    if (contentType === "application/vnd.wap.mms-message") {
      let mmsService = Cc["@mozilla.org/mms/gonkmmsservice;1"].getService(
        Ci.nsIMmsService
      );
      mmsService
        .QueryInterface(Ci.nsIWapPushApplication)
        .receiveWapPush(data.array, data.array.length, data.offset, options);
      return;
    } else if (
      contentType === "text/vnd.wap.si" ||
      contentType === "application/vnd.wap.sic"
    ) {
      msg = lazy.SI.PduHelper.parse(data, contentType);
    } else if (
      contentType === "text/vnd.wap.sl" ||
      contentType === "application/vnd.wap.slc"
    ) {
      msg = lazy.SL.PduHelper.parse(data, contentType);
    } else if (
      contentType === "text/vnd.wap.connectivity-xml" ||
      contentType === "application/vnd.wap.connectivity-wbxml"
    ) {
      // Apply HMAC authentication on WBXML encoded CP message.
      if (contentType === "application/vnd.wap.connectivity-wbxml") {
        let params = options.headers["content-type"].params;
        let sec = params && params.sec;
        let mac = params && params.mac;
        let octets = new Uint8Array(data.array);
        authInfo = lazy.CP.Authenticator.check(
          octets.subarray(data.offset),
          sec,
          mac,
          function getNetworkPin() {
            let icc = lazy.gIccService.getIccByServiceId(options.serviceId);
            let imsi = icc ? icc.imsi : null;
            return lazy.CP.Authenticator.formatImsi(imsi);
          }
        );
      }

      msg = lazy.CP.PduHelper.parse(data, contentType);
    } else if (contentType === "application/vnd.omaloc-supl-init") {
      let content = data.array.slice(data.offset);
      msg = {
        contentType,
        content,
      };
      Services.obs.notifyObservers(msg, kWapSuplInitObserverTopic, content);
      return;
    } else {
      // Unsupported type, provide raw data.
      msg = {
        contentType,
        content: data.array,
      };
    }

    let sender = lazy.gPhoneNumberUtils.normalize(options.sourceAddress, false);
    //let parsedSender = PhoneNumberUtils.parse(sender);
    //if (parsedSender && parsedSender.internationalNumber) {
    //  sender = parsedSender.internationalNumber;
    //}

    lazy.gSystemMessenger.broadcastMessage("wappush-received", {
      sender,
      contentType: msg.contentType,
      content: msg.content,
      authInfo,
      serviceId: options.serviceId,
    });
  },

  /**
   * @param array
   *        A Uint8Array or an octet array representing raw PDU data.
   * @param length
   *        Length of the array.
   * @param offset
   *        Offset of the array that a raw PDU data begins.
   * @param options
   *        WDP bearer information.
   */
  receiveWdpPDU: function receiveWdpPDU(array, length, offset, options) {
    if (
      options.bearer == null ||
      !options.sourceAddress ||
      options.sourcePort == null ||
      !array
    ) {
      debug("Incomplete WDP PDU");
      return;
    }

    if (options.destinationPort != WAP_CONSTS.WDP_PORT_PUSH) {
      debug("Not WAP Push port: " + options.destinationPort);
      return;
    }

    this.processMessage({ array, offset }, options);
  },
};

function debug(s) {
  if (DEBUG) {
    dump("-*- WapPushManager: " + s + "\n");
  }
}

const EXPORTED_SYMBOLS = ["WapPushManager"];
