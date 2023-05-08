/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const WifiConstants = {
  // Set in wpa_supplicant "bssid" field if no specific AP restricted
  SUPPLICANT_BSSID_ANY: "00:00:00:00:00:00",

  // wifi state from IWifi.aidl
  WIFI_STATE_ENABLING: 1,
  WIFI_STATE_ENABLED: 2,
  WIFI_STATE_DISABLING: 3,
  WIFI_STATE_DISABLED: 4,

  INVALID_NETWORK_ID: -1,
  INVALID_TIME_STAMP: -1,
  INVALID_RSSI: -127,
  MIN_RSSI: -100,
  MAX_RSSI: -55,

  // for traffic
  FILTER_TIME_CONSTANT: 3000.0,
  FULL_SCAN_MAX_TX_RATE: 20, // android value: 8
  FULL_SCAN_MAX_RX_RATE: 40, // android value: 16

  MODE_ESS: 0,
  MODE_IBSS: 1,

  RSSI_THRESHOLD_GOOD_24G: -60,
  RSSI_THRESHOLD_LOW_24G: -70,
  RSSI_THRESHOLD_BAD_24G: -80,
  RSSI_THRESHOLD_GOOD_5G: -57,
  RSSI_THRESHOLD_LOW_5G: -68,
  RSSI_THRESHOLD_BAD_5G: -78,
  RSSI_SCORE_OFFSET: 85,
  RSSI_SCORE_SLOPE: 4,

  PROMPT_UNVALIDATED_DELAY_MS: 8000,
  WIFI_SCHEDULED_SCAN_INTERVAL: 15 * 1000,
  WIFI_ASSOCIATED_SCAN_INTERVAL: 20 * 1000,
  WIFI_MAX_SCAN_CACHED_TIME: 60 * 1000,

  NETWORK_SELECTION_ENABLED: 0,
  NETWORK_SELECTION_TEMPORARY_DISABLED: 1,
  NETWORK_SELECTION_PERMANENTLY_DISABLED: 2,

  DISABLED_BAD_LINK: 1,
  DISABLED_ASSOCIATION_REJECTION: 2,
  DISABLED_AUTHENTICATION_FAILURE: 3,
  DISABLED_DHCP_FAILURE: 4,
  DISABLED_DNS_FAILURE: 5,
  DISABLED_TLS_VERSION_MISMATCH: 6,
  DISABLED_AUTHENTICATION_NO_CREDENTIALS: 7,
  DISABLED_NO_INTERNET: 8,
  DISABLED_BY_WIFI_MANAGER: 9,
  DISABLED_BY_WRONG_PASSWORD: 10,
  DISABLED_AUTHENTICATION_NO_SUBSCRIBED: 11,
  NETWORK_SELECTION_DISABLED_MAX: 12,

  PasspointVersion: {
    R1: 1,
    R2: 2,
    R3: 3,
    Unknown: -1,
  },

  HS20_PREFIX: 0x119a6f50,
  HS20_FRAME_PREFIX: 0x109a6f50,
  ANQP_DOMID_BIT: 4,
  VENUE_INFO_LENGTH: 2,

  HSWanMetrics: {
    LINK_STATUS_RESERVED: 0,
    LINK_STATUS_UP: 1,
    LINK_STATUS_DOWN: 2,
    LINK_STATUS_TEST: 3,
  },

  Ant: {
    Private: 0,
    PrivateWithGuest: 1,
    ChargeablePublic: 2,
    FreePublic: 3,
    Personal: 4,
    EmergencyOnly: 5,
    Resvd6: 6,
    Resvd7: 7,
    Resvd8: 8,
    Resvd9: 9,
    Resvd10: 10,
    Resvd11: 11,
    Resvd12: 12,
    Resvd13: 13,
    TestOrExperimental: 14,
    Wildcard: 15,
  },

  PasspointMatch: {
    None: 0,
    Decliened: 1,
    Incomplete: 2,
    RoamingProvider: 3,
    HomeProvider: 4,
  },
};

export const EAPConstants = {
  INVALID_EAP: -1,
  EAP_MD5: 4,
  EAP_OTP: 5,
  EAP_RSA: 9,
  EAP_KEA: 11,
  EAP_KEA_VALIDATE: 12,
  EAP_TLS: 13,
  EAP_LEAP: 17,
  EAP_SIM: 18,
  EAP_TTLS: 21,
  EAP_AKA: 23,
  EAP_3Com: 24,
  EAP_MSCHAPv2: 26,
  EAP_PEAP: 29,
  EAP_POTP: 32,
  EAP_ActiontecWireless: 35,
  EAP_HTTPDigest: 38,
  EAP_SPEKE: 41,
  EAP_MOBAC: 42,
  EAP_FAST: 43,
  EAP_ZLXEAP: 44,
  EAP_Link: 45,
  EAP_PAX: 46,
  EAP_PSK: 47,
  EAP_SAKE: 48,
  EAP_IKEv2: 49,
  EAP_AKA_PRIME: 50,
  EAP_GPSK: 51,
  EAP_PWD: 52,
  EAP_EKE: 53,
  EAP_TEAP: 55,
};
