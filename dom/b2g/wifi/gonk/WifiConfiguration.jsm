/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WifiConstants, EAPConstants } = ChromeUtils.import(
  "resource://gre/modules/WifiConstants.jsm"
);

const EXPORTED_SYMBOLS = ["ScanResult", "WifiNetwork", "WifiConfigUtils"];

var gDebug = false;

const WifiConfigUtils = (function() {
  var wifiConfigUtils = {};

  /* eslint-disable no-unused-vars */
  const EID_SSID = 0;
  const EID_SUPPORTED_RATES = 1;
  const EID_TIM = 5;
  const EID_BSS_LOAD = 11;
  const EID_ERP = 42;
  const EID_HT_CAPABILITIES = 45;
  const EID_RSN = 48;
  const EID_EXTENDED_SUPPORTED_RATES = 50;
  const EID_HT_OPERATION = 61;
  const EID_INTERWORKING = 107;
  const EID_ROAMING_CONSORTIUM = 111;
  const EID_EXTENDED_CAPS = 127;
  const EID_VHT_CAPABILITIES = 191;
  const EID_VHT_OPERATION = 192;
  const EID_VSA = 221;
  const EID_EXTENSION = 255;
  const EID_EXT_HE_CAPABILITIES = 35;

  const PROTOCOL_NONE = 0;
  const PROTOCOL_WPA = 1;
  const PROTOCOL_RSN = 2;
  const PROTOCOL_OSEN = 3;

  const KEY_MGMT_NONE = 0;
  const KEY_MGMT_PSK = 1;
  const KEY_MGMT_EAP = 2;
  const KEY_MGMT_FT_PSK = 3;
  const KEY_MGMT_FT_EAP = 4;
  const KEY_MGMT_PSK_SHA256 = 5;
  const KEY_MGMT_EAP_SHA256 = 6;
  const KEY_MGMT_OSEN = 7;
  const KEY_MGMT_SAE = 8;
  const KEY_MGMT_OWE = 9;
  const KEY_MGMT_EAP_SUITE_B_192 = 10;
  const KEY_MGMT_FT_SAE = 11;
  const KEY_MGMT_OWE_TRANSITION = 12;
  const KEY_MGMT_DPP = 13;
  const KEY_MGMT_FILS_SHA256 = 14;
  const KEY_MGMT_FILS_SHA384 = 15;

  const CIPHER_NONE = 0;
  const CIPHER_NO_GROUP_ADDRESSED = 1;
  const CIPHER_TKIP = 2;
  const CIPHER_CCMP = 3;
  const CIPHER_GCMP_256 = 4;

  const CAP_ESS_OFFSET = 1 << 0;
  const CAP_IBSS_OFFSET = 1 << 1;
  const CAP_PRIVACY_OFFSET = 1 << 4;

  const WPA_VENDOR_OUI_TYPE_ONE = 0x01f25000;
  const WPS_VENDOR_OUI_TYPE = 0x04f25000;
  const WPA_VENDOR_OUI_VERSION = 0x0001;
  const OWE_VENDOR_OUI_TYPE = 0x1c9a6f50;
  const RSNE_VERSION = 0x0001;

  const WPA_AKM_EAP = 0x01f25000;
  const WPA_AKM_PSK = 0x02f25000;

  const RSN_AKM_EAP = 0x01ac0f00;
  const RSN_AKM_PSK = 0x02ac0f00;
  const RSN_AKM_FT_EAP = 0x03ac0f00;
  const RSN_AKM_FT_PSK = 0x04ac0f00;
  const RSN_AKM_EAP_SHA256 = 0x05ac0f00;
  const RSN_AKM_PSK_SHA256 = 0x06ac0f00;
  const RSN_AKM_SAE = 0x08ac0f00;
  const RSN_AKM_FT_SAE = 0x09ac0f00;
  const RSN_AKM_OWE = 0x12ac0f00;
  const RSN_AKM_EAP_SUITE_B_192 = 0x0cac0f00;
  const WPA2_AKM_FILS_SHA256 = 0x0eac0f00;
  const WPA2_AKM_FILS_SHA384 = 0x0fac0f00;
  const WPA2_AKM_DPP = 0x029a6f50;

  const WPA_CIPHER_NONE = 0x00f25000;
  const WPA_CIPHER_TKIP = 0x02f25000;
  const WPA_CIPHER_CCMP = 0x04f25000;

  const RSN_CIPHER_NONE = 0x00ac0f00;
  const RSN_CIPHER_TKIP = 0x02ac0f00;
  const RSN_CIPHER_CCMP = 0x04ac0f00;
  const RSN_CIPHER_NO_GROUP_ADDRESSED = 0x07ac0f00;
  const RSN_CIPHER_GCMP_256 = 0x09ac0f00;

  const LITTLE_ENDIAN = 0;
  const BIG_ENDIAN = 1;
  /* eslint-enable no-unused-vars */

  const hsReleaseMap = [
    WifiConstants.PasspointVersion.R1,
    WifiConstants.PasspointVersion.R2,
    WifiConstants.PasspointVersion.R3,
  ];

  // WifiConfigUtils functions
  wifiConfigUtils.getMode = getMode;
  wifiConfigUtils.getScanResultSecurity = getScanResultSecurity;
  wifiConfigUtils.getWpsSupported = getWpsSupported;
  wifiConfigUtils.matchKeyMgmtToSecurity = matchKeyMgmtToSecurity;
  wifiConfigUtils.matchSecurityToKeyMgmt = matchSecurityToKeyMgmt;
  wifiConfigUtils.calculateSignal = calculateSignal;
  wifiConfigUtils.getNetworkKey = getNetworkKey;
  wifiConfigUtils.getAnqpNetworkKey = getAnqpNetworkKey;
  wifiConfigUtils.getRealmForMccMnc = getRealmForMccMnc;
  wifiConfigUtils.parseInformationElements = parseInformationElements;
  wifiConfigUtils.parseCapabilities = parseCapabilities;
  wifiConfigUtils.parsePasspointElements = parsePasspointElements;
  wifiConfigUtils.isCarrierEapMethod = isCarrierEapMethod;
  wifiConfigUtils.setDebug = setDebug;

  function debug(aMsg) {
    if (gDebug) {
      dump("-*- WifiConfigUtils: " + aMsg);
    }
  }

  function setDebug(aDebug) {
    gDebug = aDebug;
  }

  function byte2Int(buf, pos, len, endian) {
    let result = 0;
    let bytes = buf.splice(pos, len);

    for (let i = 0; i < bytes.length; i++) {
      if (endian == LITTLE_ENDIAN) {
        result += bytes[i] << (8 * i);
      } else {
        result += bytes[i] << (8 * (bytes.length - i - 1));
      }
    }
    pos = pos + len;
    return result;
  }

  function parseInformationElements(infoElement) {
    let pos = 0;
    let index = 0;
    let ies = [];
    while (pos < infoElement.length) {
      let ie = new InfoElement();
      // assign element id
      ie.id = infoElement[pos];
      pos = pos + 1;

      // assign element length
      let elementSize = infoElement[pos];
      pos = pos + 1;

      // assign element data
      let bytes = [];
      for (let i = 0; i < elementSize; i++) {
        bytes.push(infoElement[pos + i]);
      }
      ie.bytes = bytes;
      pos = pos + elementSize;
      ies[index++] = ie;
    }
    return ies;
  }

  function parseCapabilities(ies, beaconCap) {
    if (!ies || !beaconCap) {
      return {};
    }
    let flags = {
      protocol: [],
      keyManagement: [],
      groupCipher: [],
      pairwiseCipher: [],
      isWEP: false,
      isWPS: false,
    };
    flags.ess = beaconCap & CAP_ESS_OFFSET;
    flags.ibss = beaconCap & CAP_IBSS_OFFSET;
    let privacy = beaconCap & CAP_PRIVACY_OFFSET;

    for (let ie of ies) {
      if (ie.id == EID_RSN) {
        parseRsnElement(ie.bytes, flags);
      }
      if (ie.id == EID_VSA) {
        parseVendorOuiElement(ie.bytes, flags);
      }
    }
    flags.isWEP = !flags.protocol.length && privacy;
    return flags;
  }

  function parseRsnCipher(cipher) {
    switch (cipher) {
      case RSN_CIPHER_NONE:
        return CIPHER_NONE;
      case RSN_CIPHER_TKIP:
        return CIPHER_TKIP;
      case RSN_CIPHER_CCMP:
        return CIPHER_CCMP;
      case RSN_CIPHER_GCMP_256:
        return CIPHER_GCMP_256;
      case RSN_CIPHER_NO_GROUP_ADDRESSED:
        return CIPHER_NO_GROUP_ADDRESSED;
      default:
        return CIPHER_NONE;
    }
  }

  function parseWpaCipher(cipher) {
    switch (cipher) {
      case WPA_CIPHER_NONE:
        return CIPHER_NONE;
      case WPA_CIPHER_TKIP:
        return CIPHER_TKIP;
      case WPA_CIPHER_CCMP:
        return CIPHER_CCMP;
      default:
        return CIPHER_NONE;
    }
  }

  function parseAKM(akm) {
    switch (akm) {
      case RSN_AKM_EAP:
        return KEY_MGMT_EAP;
      case RSN_AKM_PSK:
        return KEY_MGMT_PSK;
      case RSN_AKM_FT_EAP:
        return KEY_MGMT_FT_EAP;
      case RSN_AKM_FT_PSK:
        return KEY_MGMT_FT_PSK;
      case RSN_AKM_EAP_SHA256:
        return KEY_MGMT_EAP_SHA256;
      case RSN_AKM_PSK_SHA256:
        return KEY_MGMT_PSK_SHA256;
      case WPA2_AKM_FILS_SHA256:
        return KEY_MGMT_FILS_SHA256;
      case WPA2_AKM_FILS_SHA384:
        return KEY_MGMT_FILS_SHA384;
      case WPA2_AKM_DPP:
        return KEY_MGMT_DPP;
      case RSN_AKM_SAE:
        return KEY_MGMT_SAE;
      case RSN_AKM_FT_SAE:
        return KEY_MGMT_FT_SAE;
      case RSN_AKM_OWE:
        return KEY_MGMT_OWE;
      case RSN_AKM_EAP_SUITE_B_192:
        return KEY_MGMT_EAP_SUITE_B_192;
      default:
        return KEY_MGMT_EAP;
    }
  }

  function parseRsnElement(buf, flags) {
    // RSNE format (size unit: byte)
    //
    // | Element ID | Length | Version | Group Data Cipher Suite |
    //      1           1         2                 4
    // | Pairwise Cipher Suite Count | Pairwise Cipher Suite List |
    //              2                            4 * m
    // | AKM Suite Count | AKM Suite List | RSN Capabilities |
    //          2               4 * n               2
    // | PMKID Count | PMKID List | Group Management Cipher Suite |
    //        2          16 * s                 4
    let pos = 0;
    let version = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    if (version != RSNE_VERSION) {
      return;
    }
    let groupCipher = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
    let cipherCount = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    let pairwiseCipher = [];
    let cipher = 0;
    for (let i = 0; i < cipherCount; i++) {
      cipher = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
      pairwiseCipher.push(parseRsnCipher(cipher));
    }
    let akmCount = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    let keyManagement = [];
    let akm = 0;
    for (let i = 0; i < akmCount; i++) {
      akm = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
      keyManagement.push(parseAKM(akm));
    }

    flags.protocol.push(PROTOCOL_RSN);
    flags.groupCipher.push(parseRsnCipher(groupCipher));
    flags.pairwiseCipher.push(pairwiseCipher);
    flags.keyManagement.push(keyManagement);
  }

  function parseWpaOneElement(buf, flags) {
    // WPA type 1 format (size unit: byte)
    //
    // | Element ID | Length | OUI | Type | Version |
    //      1           1       3     1        2
    // | Group Data Cipher Suite |
    //             4
    // | Pairwise Cipher Suite Count | Pairwise Cipher Suite List |
    //              2                            4 * m
    // | AKM Suite Count | AKM Suite List |
    //          2               4 * n
    let pos = 0;
    let version = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    if (version != WPA_VENDOR_OUI_VERSION) {
      return;
    }
    let groupCipher = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
    let cipherCount = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    let pairwiseCipher = [];
    let cipher = 0;
    for (let i = 0; i < cipherCount; i++) {
      cipher = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
      pairwiseCipher.push(parseWpaCipher(cipher));
    }
    let akmCount = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
    let keyManagement = [];
    let akm = 0;
    for (let i = 0; i < akmCount; i++) {
      akm = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
      if (akm == WPA_AKM_EAP) {
        keyManagement.push(KEY_MGMT_EAP);
      } else if (akm == WPA_AKM_PSK) {
        keyManagement.push(KEY_MGMT_PSK);
      }
    }
    flags.protocol.push(PROTOCOL_WPA);
    flags.groupCipher.push(parseWpaCipher(groupCipher));
    flags.pairwiseCipher.push(pairwiseCipher);
    flags.keyManagement.push(keyManagement);
  }

  function parseVendorOuiElement(buf, flags) {
    let pos = 0;
    let elementType = byte2Int(buf, pos, 4, LITTLE_ENDIAN);
    if (elementType == WPA_VENDOR_OUI_TYPE_ONE) {
      // WPA one
      parseWpaOneElement(buf, flags);
    }
    if (elementType == WPS_VENDOR_OUI_TYPE) {
      // WPS
      flags.isWPS = true;
    }
  }

  function parseInterworking(buf, element) {
    let pos = 0;
    let anOptions = byte2Int(buf, pos, 1, LITTLE_ENDIAN);

    element.ant = Object.values(WifiConstants.Ant)[anOptions & 0x0f];
    element.internet = (anOptions & 0x10) != 0;
    element.isInterworking = !!element.ant;

    if (
      buf.length != 1 &&
      buf.length != 3 &&
      buf.length != 7 &&
      buf.length != 9
    ) {
      debug("Invalid interworking element length");
      return;
    }

    // venueInfo
    if (buf.length == 3 || buf.length == 9) {
      byte2Int(buf, pos, 2, BIG_ENDIAN);
    }
    if (buf.length == 7 || buf.length == 9) {
      element.hessid = byte2Int(buf, pos, 6, BIG_ENDIAN);
    }
  }

  function parseRoamingConsortium(buf, element) {
    let pos = 0;
    element.anqpOICount = byte2Int(buf, pos, 1, LITTLE_ENDIAN);

    let oi12Length = byte2Int(buf, pos, 1, LITTLE_ENDIAN);
    let oi1Length = byte2Int(buf, pos, 1, LITTLE_ENDIAN) & 0x0f;
    let oi2Length = (oi12Length >>> 4) & 0x0f;
    let oi3Length = buf.length - 2 - oi1Length - oi2Length;

    if (oi1Length > 0) {
      element.roamingConsortiums.push(
        byte2Int(buf, pos, oi1Length, BIG_ENDIAN)
      );
      if (oi2Length > 0) {
        element.roamingConsortiums.push(
          byte2Int(buf, pos, oi2Length, BIG_ENDIAN)
        );
        if (oi3Length > 0) {
          element.roamingConsortiums.push(
            byte2Int(buf, pos, oi3Length, BIG_ENDIAN)
          );
        }
      }
    }
  }

  function parseVsaElement(buf, element) {
    let pos = 0;
    let prefix = byte2Int(buf, pos, 4, LITTLE_ENDIAN);

    if (buf.length >= 5 && prefix == WifiConstants.HS20_FRAME_PREFIX) {
      let hsConf = byte2Int(buf, pos, 1, LITTLE_ENDIAN);
      let hsVersion = (hsConf >> 4) & 0x0f;

      if (hsVersion < hsReleaseMap.length) {
        element.hsRelease = hsReleaseMap[hsVersion];
      } else {
        element.hsRelease = WifiConstants.PasspointVersion.Unknown;
      }

      if ((hsConf & WifiConstants.ANQP_DOMID_BIT) != 0) {
        if (buf.length < 7) {
          debug("Not enough element for domain ID");
          return;
        }
        element.anqpDomainID = byte2Int(buf, pos, 2, LITTLE_ENDIAN);
      }
    }
  }

  function parsePasspointElements(ies) {
    if (!ies) {
      return {};
    }
    let element = {
      ant: null,
      isInterworking: false,
      internet: false,
      hessid: 0,
      anqpOICount: 0,
      roamingConsortiums: [],
      hsRelease: WifiConstants.PasspointVersion.Unknown,
      anqpDomainID: 0,
    };

    for (let ie of ies) {
      if (ie.id == EID_INTERWORKING) {
        parseInterworking(ie.bytes, element);
      }
      if (ie.id == EID_ROAMING_CONSORTIUM) {
        parseRoamingConsortium(ie.bytes, element);
      }
      if (ie.id == EID_VSA) {
        parseVsaElement(ie.bytes, element);
      }
    }
    return element;
  }

  function parseEncryption(flags) {
    let encryptType = {
      isWPA: false,
      isRSN: false,
      isPSK: false,
      isEAP: false,
      isSAE: false,
    };

    if (!flags) {
      return encryptType;
    }

    for (let i = 0; i < flags.protocol.length; i++) {
      if (flags.protocol[i] == PROTOCOL_WPA) {
        encryptType.isWPA = true;
      } else if (flags.protocol[i] == PROTOCOL_RSN) {
        encryptType.isRSN = true;
      }
      for (let j = 0; j < flags.keyManagement[i].length; j++) {
        if (
          flags.keyManagement[i][j] == KEY_MGMT_PSK ||
          flags.keyManagement[i][j] == KEY_MGMT_PSK_SHA256
        ) {
          encryptType.isPSK = true;
        } else if (
          flags.keyManagement[i][j] == KEY_MGMT_EAP ||
          flags.keyManagement[i][j] == KEY_MGMT_EAP_SHA256
        ) {
          encryptType.isEAP = true;
        } else if (flags.keyManagement[i][j] == KEY_MGMT_SAE) {
          encryptType.isSAE = true;
        }
      }
    }
    return encryptType;
  }

  function getMode(flags) {
    if (flags.ibss) {
      return WifiConstants.MODE_IBSS;
    }
    return WifiConstants.MODE_ESS;
  }

  function isWepEncryption(network) {
    if (network.keyMgmt == "WEP" || network.security == "WEP") {
      return true;
    }
    if (
      (network.keyMgmt == "NONE" || network.security == "OPEN") &&
      network.authAlg == "OPEN SHARED"
    ) {
      return true;
    }
    return false;
  }

  function getScanResultSecurity(flags) {
    var type = "OPEN";
    if (!flags) {
      return type;
    }

    let encrypt = parseEncryption(flags);
    if (encrypt.isEAP) {
      type = "WPA-EAP";
    } else if (encrypt.isSAE) {
      type = "SAE";
    } else if (encrypt.isPSK) {
      if (encrypt.isWPA && encrypt.isRSN) {
        type = "WPA/WPA2-PSK";
      } else if (encrypt.isWPA) {
        type = "WPA-PSK";
      } else if (encrypt.isRSN) {
        type = "WPA2-PSK";
      }
    } else if (flags.isWEP) {
      type = "WEP";
    }
    return type;
  }

  function getWpsSupported(flags) {
    return flags ? flags.isWPS : false;
  }

  function matchKeyMgmtToSecurity(network) {
    let keyMgmt = network.keyMgmt;
    if (!keyMgmt) {
      return "OPEN";
    }

    if (keyMgmt == "WPA-PSK") {
      return "WPA-PSK";
    } else if (keyMgmt.includes("WPA-EAP")) {
      return "WPA-EAP";
    } else if (keyMgmt.includes("SAE")) {
      return "SAE";
    } else if (keyMgmt == "WAPI-PSK") {
      return "WAPI-PSK";
    } else if (keyMgmt == "WAPI-CERT") {
      return "WAPI-CERT";
    } else if (isWepEncryption(network)) {
      return "WEP";
    }
    return "OPEN";
  }

  function matchSecurityToKeyMgmt(network) {
    let security = network.security;
    if (!security) {
      return "NONE";
    }

    if (
      security == "WPA-PSK" ||
      security == "WPA2-PSK" ||
      security === "WPA/WPA2-PSK"
    ) {
      return "WPA-PSK";
    } else if (security.includes("WPA-EAP")) {
      return "WPA-EAP IEEE8021X";
    } else if (security.includes("SAE")) {
      return "SAE";
    } else if (security == "WAPI-PSK") {
      return "WAPI-PSK";
    } else if (security == "WAPI-CERT") {
      return "WAPI-CERT";
    } else if (isWepEncryption(network)) {
      return "WEP";
    }
    return "NONE";
  }

  function calculateSignal(strength) {
    if (strength > 0) {
      strength -= 256;
    }

    if (strength <= WifiConstants.MIN_RSSI) {
      return 0;
    }

    if (strength >= WifiConstants.MAX_RSSI) {
      return 100;
    }

    return Math.floor(
      ((strength - WifiConstants.MIN_RSSI) /
        (WifiConstants.MAX_RSSI - WifiConstants.MIN_RSSI)) *
        100
    );
  }

  function dequote(s) {
    if (s[0] != '"' || s[s.length - 1] != '"') {
      throw Components.Exception(
        "Invalid argument, not a quoted string: ",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    return s.substr(1, s.length - 2);
  }

  // Get unique key for a network, now the key is created by escape(SSID)+Security.
  // So networks of same SSID but different security mode can be identified.
  function getNetworkKey(network) {
    var ssid = "",
      encryption = "OPEN";

    if ("security" in network && network.security.length) {
      // manager network object, represents an AP
      // object structure
      // {
      //   .ssid           : SSID of AP
      //   .security       : "WPA-PSK" for WPA-PSK
      //                     "WPA-EAP" for WPA-EAP
      //                     "SAE" for SAE
      //                     "WEP" for WEP
      //                     "WAPI-PSK" for WAPI-PSK
      //                     "WAPI-CERT" for WAPI-CERT
      //                     "OPEN" for OPEN
      //   other keys
      // }
      var security = network.security;
      ssid = network.ssid;

      if (
        security === "WPA-PSK" ||
        security === "WPA2-PSK" ||
        security === "WPA/WPA2-PSK"
      ) {
        encryption = "WPA-PSK";
      } else if (security === "WPA-EAP") {
        encryption = "WPA-EAP";
      } else if (security === "SAE") {
        encryption = "SAE";
      } else if (security === "WAPI-PSK") {
        encryption = "WAPI-PSK";
      } else if (security === "WAPI-CERT") {
        encryption = "WAPI-CERT";
      } else if (isWepEncryption(network)) {
        encryption = "WEP";
      }
    } else if ("keyMgmt" in network) {
      // configure network object, represents a network
      // object structure
      // {
      //   .ssid           : SSID of network, quoted
      //   .keyMgmt        : Encryption type
      //                     "WPA-PSK" for WPA-PSK
      //                     "WPA-EAP" for WPA-EAP
      //                     "NONE" for WEP/OPEN
      //                     "WAPI-PSK" for WAPI-PSK
      //                     "WAPI-CERT" for WAPI-CERT
      //   .authAlg        : Encryption algorithm(WEP mode only)
      //                     "OPEN_SHARED" for WEP
      //   other keys
      // }
      var keyMgmt = network.keyMgmt;
      ssid = dequote(network.ssid);

      if (keyMgmt == "WPA-PSK") {
        encryption = "WPA-PSK";
      } else if (keyMgmt.includes("WPA-EAP")) {
        encryption = "WPA-EAP";
      } else if (keyMgmt == "SAE") {
        encryption = "SAE";
      } else if (keyMgmt == "WAPI-PSK") {
        encryption = "WAPI-PSK";
      } else if (keyMgmt == "WAPI-CERT") {
        encryption = "WAPI-CERT";
      } else if (isWepEncryption(network)) {
        encryption = "WEP";
      }
    }

    // ssid here must be dequoted, and it's safer to esacpe it.
    // encryption won't be empty and always be assigned one of the followings :
    // "OPEN"/"WEP"/"WPA-PSK"/"WPA-EAP"/"WAPI-PSK"/"WAPI-CERT".
    // So for a invalid network object, the returned key will be "OPEN".
    return escape(ssid) + encryption;
  }

  // Get unique key for identifying APs, anqp key is created by
  // escape(SSID)+Security+bssid+hessid+anqpDomainID.
  // So we may distinguish these passpoint AP.
  function getAnqpNetworkKey(network) {
    var ssid = network.ssid.replace(/\s/g, ""),
      bssid = network.bssid,
      hessid = network.passpoint.hessid,
      anqpDomainID = network.passpoint.anqpDomainID;

    if (anqpDomainID == 0) {
      return `${escape(ssid)}${bssid}00`;
    } else if (hessid != 0) {
      return `0${hessid}${anqpDomainID}`;
    }

    return `${escape(ssid)}00${anqpDomainID}`;
  }

  function getRealmForMccMnc(mcc, mnc) {
    if (mcc == null || mnc == null) {
      return null;
    }
    if (mnc.length === 2) {
      mnc = `0${mnc}`;
    }
    return `wlan.mnc${mnc}.mcc${mcc}.3gppnetwork.org`;
  }

  function isCarrierEapMethod(eapMethod) {
    return (
      eapMethod == EAPConstants.EAP_SIM ||
      eapMethod == EAPConstants.EAP_AKA ||
      eapMethod == EAPConstants.EAP_AKA_PRIME
    );
  }

  return wifiConfigUtils;
})();

const InfoElement = function InfoElement() {
  this.id = 0;
  this.bytes = [];

  this.__exposedProps__ = InfoElement.api;
};

InfoElement.api = {
  id: "rw",
  data: "rw",
};

const WifiNetwork = function WifiNetwork(
  ssid,
  mode,
  frequency,
  security,
  password,
  wpsSupported
) {
  this.ssid = ssid;
  this.mode = mode;
  this.frequency = frequency;
  this.security = security;
  this.wpsSupported = wpsSupported;

  if (typeof password !== "undefined") {
    this.password = password;
  }

  this.__exposedProps__ = WifiNetwork.api;
};

WifiNetwork.api = {
  ssid: "r",
  mode: "r",
  frequency: "r",
  security: "r",
  known: "r",
  connected: "r",
  wpsSupported: "r",

  password: "rw",
  psk: "rw",
  identity: "rw",
  wep: "rw",
  keyIndex: "rw",
  hidden: "rw",
  eap: "rw",
  pin: "rw",
  phase1: "rw",
  phase2: "rw",
  serverCertificate: "rw",
  userCertificate: "rw",
  simIndex: "rw",
  wapi_psk: "rw",
  pskType: "rw",
  wapiAsCertificate: "rw",
  wapiUserCertificate: "rw",
};

const ScanResult = function ScanResult(ssid, bssid, frequency, flags, signal) {
  WifiNetwork.call(
    this,
    ssid,
    WifiConfigUtils.getMode(flags),
    frequency,
    WifiConfigUtils.getScanResultSecurity(flags),
    undefined,
    WifiConfigUtils.getWpsSupported(flags)
  );
  this.bssid = bssid;
  this.signalStrength = signal;
  this.relSignalStrength = WifiConfigUtils.calculateSignal(Number(signal));
  this.is24G = this.is24GHz(frequency);
  this.is5G = this.is5GHz(frequency);

  this.__exposedProps__ = ScanResult.api;
};

ScanResult.api = {
  bssid: "r",
  signalStrength: "r",
  relSignalStrength: "r",
  connected: "r",
  is24G: "r",
  is5G: "r",
};

for (let i in WifiNetwork.api) {
  ScanResult.api[i] = WifiNetwork.api[i];
}

ScanResult.prototype = {
  is24GHz(freq) {
    return freq > 2400 && freq < 2500;
  },

  is5GHz(freq) {
    return freq > 4900 && freq < 5900;
  },
};
