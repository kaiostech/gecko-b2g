/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var WSP = ChromeUtils.import("resource://gre/modules/WspPduHelper.jsm");
var WBXML = ChromeUtils.import("resource://gre/modules/WbxmlPduHelper.jsm");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "CryptoUtils",
  "resource://services-crypto/utils.js"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "CommonUtils",
  "resource://services-connon/utils.js"
);

/**
 * Public identifier for CP
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const PUBLIC_IDENTIFIER_CP = "-//WAPFORUM//DTD PROV 1.0//EN";

const PduHelper = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param contentType
   *        Content type of incoming CP message, should be "text/vnd.wap.connectivity-xml"
   *        or "application/vnd.wap.connectivity-wbxml".
   *
   * @return A message object containing attribute content and contentType.
   *         |content| will contain string of decoded CP message if successfully
   *         decoded, or raw data if failed.
   *         |contentType| will be string representing corresponding type of
   *         content.
   */
  parse: function parse_cp(data, contentType) {
    // We only need content and contentType
    let msg = {
      contentType,
    };

    /**
     * Message is compressed by WBXML, decode into string.
     *
     * @see WAP-192-WBXML-20010725-A
     */
    if (contentType === "application/vnd.wap.connectivity-wbxml") {
      let appToken = {
        publicId: PUBLIC_IDENTIFIER_CP,
        tagTokenList: CP_TAG_FIELDS,
        attrTokenList: CP_ATTRIBUTE_FIELDS,
        valueTokenList: CP_VALUE_FIELDS,
        globalTokenOverride: null,
      };

      try {
        let parseResult = WBXML.PduHelper.parse(data, appToken, msg);
        msg.content = parseResult.content;
        msg.contentType = "text/vnd.wap.connectivity-xml";
      } catch (e) {
        // Provide raw data if we failed to parse.
        msg.content = data.array;
      }

      return msg;
    }

    /**
     * Message is plain text, transform raw to string.
     */
    try {
      let stringData = WSP.Octet.decodeMultiple(data, data.array.length);
      msg.content = WSP.PduHelper.decodeStringContent(stringData, "UTF-8");
    } catch (e) {
      // Provide raw data if we failed to parse.
      msg.content = data.array;
    }
    return msg;
  },
};

/**
 * SEC type values
 *
 * @see WAP-183-ProvCont-20010724-A, clause 5.3
 */
const AUTH_SEC_TYPE = (function() {
  let names = {};
  function add(name, number) {
    names[number] = name;
  }

  add("NETWPIN", 0);
  add("USERPIN", 1);
  add("USERNETWPIN", 2);
  add("USERPINMAC", 3);

  return names;
})();

const Authenticator = {
  /**
   * Format IMSI string into GSM format
   *
   * @param imsi
   *        IMSI string
   *
   * @return IMSI in GSM format as string object
   */
  formatImsi: function formatImsi(imsi) {
    let parityByte = imsi.length & 1 ? 9 : 1;

    // Make sure length of IMSI is 15 digits.
    // @see GSM 11.11, clause 10.2.2
    let i = 0;
    for (i = 15 - imsi.length; i > 0; i--) {
      imsi += "F";
    }

    // char-by-char atoi
    let imsiValue = [];
    imsiValue.push(parityByte);
    for (i = 0; i < imsi.length; i++) {
      imsiValue.push(parseInt(imsi.substr(i, 1), 10));
    }

    // encoded IMSI
    let imsiEncoded = "";
    for (i = 0; i < imsiValue.length; i += 2) {
      imsiEncoded += String.fromCharCode(
        imsiValue[i] | (imsiValue[i + 1] << 4)
      );
    }

    return imsiEncoded;
  },

  /**
   * Perform HMAC check
   *
   * @param wbxml
   *        Uint8 typed array of raw WBXML data.
   * @param key
   *        key string for HMAC check.
   * @param mac
   *        Expected MAC value.
   *
   * @return true for valid, false for invalid.
   */
  isValid: function isValid(wbxml, key, mac) {
    let hasher = lazy.CryptoUtils.makeHMACHasher(
      Ci.nsICryptoHMAC.SHA1,
      lazy.CryptoUtils.makeHMACKey(key)
    );
    hasher.update(wbxml, wbxml.length);
    let result = lazy.CommonUtils.bytesAsHex(
      hasher.finish(false)
    ).toUpperCase();
    return mac == result;
  },

  /**
   * Perform HMAC authentication.
   *
   * @param wbxml
   *        Uint8 typed array of raw WBXML data.
   * @param sec
   *        Security method for HMAC check.
   * @param mac
   *        Expected MAC value.
   * @param getNetworkPin
   *        Callback function for getting network pin.
   *
   * @return true for valid, false for invalid.
   */
  check: function check_hmac(wbxml, sec, mac, getNetworkPin) {
    // No security set.
    if (sec == null || !mac) {
      return null;
    }

    let authInfo = {
      pass: false,
      checked: false,
      sec: AUTH_SEC_TYPE[sec],
      mac: mac.toUpperCase(),
      data: wbxml,
    };

    switch (authInfo.sec) {
      case "NETWPIN":
        let key = getNetworkPin();
        authInfo.pass = this.isValid(wbxml, key, authInfo.mac);
        authInfo.checked = true;
        return authInfo;

      case "USERPIN":
      case "USERPINMAC":
        // We can't check without USER PIN
        return authInfo;

      case "USERNETWPIN":
      default:
        return null;
    }
  },
};

/**
 * Tag tokens
 *
 * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.1
 */
const CP_TAG_FIELDS = (function() {
  let names = {};
  function add(name, codepage, number) {
    let entry = {
      name,
      number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("wap-provisioningdoc", 0, 0x05);
  add("characteristic", 0, 0x06);
  add("parm", 0, 0x07);
  // Code page 1
  add("characteristic", 1, 0x06);
  add("parm", 1, 0x07);

  return names;
})();

/**
 * Attribute Tokens
 *
 * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.2
 */
const CP_ATTRIBUTE_FIELDS = (function() {
  let names = {};
  function add(name, value, codepage, number) {
    let entry = {
      name,
      value,
      number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("name", "", 0, 0x05);
  add("value", "", 0, 0x06);
  add("name", "NAME", 0, 0x07);
  add("name", "NAP-ADDRESS", 0, 0x08);
  add("name", "NAP-ADDRTYPE", 0, 0x09);
  add("name", "CALLTYPE", 0, 0x0a);
  add("name", "VALIDUNTIL", 0, 0x0b);
  add("name", "AUTHTYPE", 0, 0x0c);
  add("name", "AUTHNAME", 0, 0x0d);
  add("name", "AUTHSECRET", 0, 0x0e);
  add("name", "LINGER", 0, 0x0f);
  add("name", "BEARER", 0, 0x10);
  add("name", "NAPID", 0, 0x11);
  add("name", "COUNTRY", 0, 0x12);
  add("name", "NETWORK", 0, 0x13);
  add("name", "INTERNET", 0, 0x14);
  add("name", "PROXY-ID", 0, 0x15);
  add("name", "PROXY-PROVIDER-ID", 0, 0x16);
  add("name", "DOMAIN", 0, 0x17);
  add("name", "PROVURL", 0, 0x18);
  add("name", "PXAUTH-TYPE", 0, 0x19);
  add("name", "PXAUTH-ID", 0, 0x1a);
  add("name", "PXAUTH-PW", 0, 0x1b);
  add("name", "STARTPAGE", 0, 0x1c);
  add("name", "BASAUTH-ID", 0, 0x1d);
  add("name", "BASAUTH-PW", 0, 0x1e);
  add("name", "PUSHENABLED", 0, 0x1f);
  add("name", "PXADDR", 0, 0x20);
  add("name", "PXADDRTYPE", 0, 0x21);
  add("name", "TO-NAPID", 0, 0x22);
  add("name", "PORTNBR", 0, 0x23);
  add("name", "SERVICE", 0, 0x24);
  add("name", "LINKSPEED", 0, 0x25);
  add("name", "DNLINKSPEED", 0, 0x26);
  add("name", "LOCAL-ADDR", 0, 0x27);
  add("name", "LOCAL-ADDRTYPE", 0, 0x28);
  add("name", "CONTEXT-ALLOW", 0, 0x29);
  add("name", "TRUST", 0, 0x2a);
  add("name", "MASTER", 0, 0x2b);
  add("name", "SID", 0, 0x2c);
  add("name", "SOC", 0, 0x2d);
  add("name", "WSP-VERSION", 0, 0x2e);
  add("name", "PHYSICAL-PROXY-ID", 0, 0x2f);
  add("name", "CLIENT-ID", 0, 0x30);
  add("name", "DELIVERY-ERR-PDU", 0, 0x31);
  add("name", "DELIVERY-ORDER", 0, 0x32);
  add("name", "TRAFFIC-CLASS", 0, 0x33);
  add("name", "MAX-SDU-SIZE", 0, 0x34);
  add("name", "MAX-BITRATE-UPLINK", 0, 0x35);
  add("name", "MAX-BITRATE-DNLINK", 0, 0x36);
  add("name", "RESIDUAL-BER", 0, 0x37);
  add("name", "SDU-ERROR-RATIO", 0, 0x38);
  add("name", "TRAFFIC-HANDL-PRIO", 0, 0x39);
  add("name", "TRANSFER-DELAY", 0, 0x3a);
  add("name", "GUARANTEED-BITRATE-UPLINK", 0, 0x3b);
  add("name", "GUARANTEED-BITRATE-DNLINK", 0, 0x3c);
  add("name", "PXADDR-FQDN", 0, 0x3d);
  add("name", "PROXY-PW", 0, 0x3e);
  add("name", "PPGAUTH-TYPE", 0, 0x3f);
  add("version", "", 0, 0x45);
  add("version", "1.0", 0, 0x46);
  add("name", "PULLENABLED", 0, 0x47);
  add("name", "DNS-ADDR", 0, 0x48);
  add("name", "MAX-NUM-RETRY", 0, 0x49);
  add("name", "FIRST-RETRY-TIMEOUT", 0, 0x4a);
  add("name", "REREG-THRESHOLD", 0, 0x4b);
  add("name", "T-BIT", 0, 0x4c);
  add("name", "AUTH-ENTITY", 0, 0x4e);
  add("name", "SPI", 0, 0x4f);
  add("type", "", 0, 0x50);
  add("type", "PXLOGICAL", 0, 0x51);
  add("type", "PXPHYSICAL", 0, 0x52);
  add("type", "PORT", 0, 0x53);
  add("type", "VALIDITY", 0, 0x54);
  add("type", "NAPDEF", 0, 0x55);
  add("type", "BOOTSTRAP", 0, 0x56);
  /*
   *  Mark out VENDORCONFIG so if it is contained in message, parse
   *  will failed and raw data is returned.
   */
  //  add("type",     "VENDORCONFIG",                 0,  0x57);
  add("type", "CLIENTIDENTITY", 0, 0x58);
  add("type", "PXAUTHINFO", 0, 0x59);
  add("type", "NAPAUTHINFO", 0, 0x5a);
  add("type", "ACCESS", 0, 0x5b);

  // Code page 1
  add("name", "", 1, 0x05);
  add("value", "", 1, 0x06);
  add("name", "NAME", 1, 0x07);
  add("name", "INTERNET", 1, 0x14);
  add("name", "STARTPAGE", 1, 0x1c);
  add("name", "TO-NAPID", 1, 0x22);
  add("name", "PORTNBR", 1, 0x23);
  add("name", "SERVICE", 1, 0x24);
  add("name", "AACCEPT", 1, 0x2e);
  add("name", "AAUTHDATA", 1, 0x2f);
  add("name", "AAUTHLEVEL", 1, 0x30);
  add("name", "AAUTHNAME", 1, 0x31);
  add("name", "AAUTHSECRET", 1, 0x32);
  add("name", "AAUTHTYPE", 1, 0x33);
  add("name", "ADDR", 1, 0x34);
  add("name", "ADDRTYPE", 1, 0x35);
  add("name", "APPID", 1, 0x36);
  add("name", "APROTOCOL", 1, 0x37);
  add("name", "PROVIDER-ID", 1, 0x38);
  add("name", "TO-PROXY", 1, 0x39);
  add("name", "URI", 1, 0x3a);
  add("name", "RULE", 1, 0x3b);
  add("type", "", 1, 0x50);
  add("type", "PORT", 1, 0x53);
  add("type", "APPLICATION", 1, 0x55);
  add("type", "APPADDR", 1, 0x56);
  add("type", "APPAUTH", 1, 0x57);
  add("type", "CLIENTIDENTITY", 1, 0x58);
  add("type", "RESOURCE", 1, 0x59);

  return names;
})();

/**
 * Value Tokens
 *
 * @see OMA-WAP-TS-ProvCont-V1_1-20090421-C, clause 7.3
 */
const CP_VALUE_FIELDS = (function() {
  let names = {};
  function add(value, codepage, number) {
    let entry = {
      value,
      number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  // Code page 0
  add("IPV4", 0, 0x85);
  add("IPV6", 0, 0x86);
  add("E164", 0, 0x87);
  add("ALPHA", 0, 0x88);
  add("APN", 0, 0x89);
  add("SCODE", 0, 0x8a);
  add("TETRA-ITSI", 0, 0x8b);
  add("MAN", 0, 0x8c);
  add("ANALOG-MODEM", 0, 0x90);
  add("V.120", 0, 0x91);
  add("V.110", 0, 0x92);
  add("X.31", 0, 0x93);
  add("BIT-TRANSPARENT", 0, 0x94);
  add("DIRECT-ASYNCHRONOUS-DATA-SERVICE", 0, 0x95);
  add("PAP", 0, 0x9a);
  add("CHAP", 0, 0x9b);
  add("HTTP-BASIC", 0, 0x9c);
  add("HTTP-DIGEST", 0, 0x9d);
  add("WTLS-SS", 0, 0x9e);
  add("MD5", 0, 0x9f); // Added in OMA, 7.3.3
  add("GSM-USSD", 0, 0xa2);
  add("GSM-SMS", 0, 0xa3);
  add("ANSI-136-GUTS", 0, 0xa4);
  add("IS-95-CDMA-SMS", 0, 0xa5);
  add("IS-95-CDMA-CSD", 0, 0xa6);
  add("IS-95-CDMA-PAC", 0, 0xa7);
  add("ANSI-136-CSD", 0, 0xa8);
  add("ANSI-136-GPRS", 0, 0xa9);
  add("GSM-CSD", 0, 0xaa);
  add("GSM-GPRS", 0, 0xab);
  add("AMPS-CDPD", 0, 0xac);
  add("PDC-CSD", 0, 0xad);
  add("PDC-PACKET", 0, 0xae);
  add("IDEN-SMS", 0, 0xaf);
  add("IDEN-CSD", 0, 0xb0);
  add("IDEN-PACKET", 0, 0xb1);
  add("FLEX/REFLEX", 0, 0xb2);
  add("PHS-SMS", 0, 0xb3);
  add("PHS-CSD", 0, 0xb4);
  add("TETRA-SDS", 0, 0xb5);
  add("TETRA-PACKET", 0, 0xb6);
  add("ANSI-136-GHOST", 0, 0xb7);
  add("MOBITEX-MPAK", 0, 0xb8);
  add("CDMA2000-1X-SIMPLE-IP", 0, 0xb9); // Added in OMA, 7.3.4
  add("CDMA2000-1X-MOBILE-IP", 0, 0xba); // Added in OMA, 7.3.4
  add("AUTOBOUDING", 0, 0xc5);
  add("CL-WSP", 0, 0xca);
  add("CO-WSP", 0, 0xcb);
  add("CL-SEC-WSP", 0, 0xcc);
  add("CO-SEC-WSP", 0, 0xcd);
  add("CL-SEC-WTA", 0, 0xce);
  add("CO-SEC-WTA", 0, 0xcf);
  add("OTA-HTTP-TO", 0, 0xd0); // Added in OMA, 7.3.6
  add("OTA-HTTP-TLS-TO", 0, 0xd1); // Added in OMA, 7.3.6
  add("OTA-HTTP-PO", 0, 0xd2); // Added in OMA, 7.3.6
  add("OTA-HTTP-TLS-PO", 0, 0xd3); // Added in OMA, 7.3.6
  add("AAA", 0, 0xe0); // Added in OMA, 7.3.8
  add("HA", 0, 0xe1); // Added in OMA, 7.3.8

  // Code page 1
  add("IPV6", 1, 0x86);
  add("E164", 1, 0x87);
  add("ALPHA", 1, 0x88);
  add("APPSRV", 1, 0x8d);
  add("OBEX", 1, 0x8e);
  add(",", 1, 0x90);
  add("HTTP-", 1, 0x91);
  add("BASIC", 1, 0x92);
  add("DIGEST", 1, 0x93);

  return names;
})();

const EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
  // HMAC Authenticator
  "Authenticator",
];
