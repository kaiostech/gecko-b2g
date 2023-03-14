/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "SupplicantStaNetwork"

#include "SupplicantStaNetwork.h"
#include <iomanip>

constexpr uint32_t key_mgmt_none =
    (ISupplicantStaNetwork::KeyMgmtMask::NONE | 0x0);
constexpr uint32_t key_mgmt_wpa_psk =
    (ISupplicantStaNetwork::KeyMgmtMask::WPA_PSK | 0x0);
constexpr uint32_t key_mgmt_wpa_eap =
    (ISupplicantStaNetwork::KeyMgmtMask::WPA_EAP | 0x0);
constexpr uint32_t key_mgmt_ieee8021x =
    (ISupplicantStaNetwork::KeyMgmtMask::IEEE8021X | 0x0);
constexpr uint32_t key_mgmt_ft_psk =
    (ISupplicantStaNetwork::KeyMgmtMask::FT_PSK | 0x0);
constexpr uint32_t key_mgmt_ft_eap =
    (ISupplicantStaNetwork::KeyMgmtMask::FT_EAP | 0x0);
constexpr uint32_t key_mgmt_osen =
    (ISupplicantStaNetwork::KeyMgmtMask::OSEN | 0x0);

// key management supported on 1.2
constexpr uint32_t key_mgmt_wpa_eap_sha256 =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::WPA_EAP_SHA256 | 0x0);
constexpr uint32_t key_mgmt_wpa_psk_sha256 =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::WPA_PSK_SHA256 | 0x0);
constexpr uint32_t key_mgmt_sae =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::SAE | 0x0);
constexpr uint32_t key_mgmt_suite_b_192 =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::SUITE_B_192 | 0x0);
constexpr uint32_t key_mgmt_owe =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::OWE | 0x0);
constexpr uint32_t key_mgmt_dpp =
    (ISupplicantStaNetworkV1_2::KeyMgmtMask::DPP | 0x0);

constexpr uint32_t protocol_wpa = (ISupplicantStaNetwork::ProtoMask::WPA | 0x0);
constexpr uint32_t protocol_rsn = (ISupplicantStaNetwork::ProtoMask::RSN | 0x0);
constexpr uint32_t protocol_osen =
    (ISupplicantStaNetwork::ProtoMask::OSEN | 0x0);

constexpr uint32_t auth_alg_open =
    (ISupplicantStaNetwork::AuthAlgMask::OPEN | 0x0);
constexpr uint32_t auth_alg_shared =
    (ISupplicantStaNetwork::AuthAlgMask::SHARED | 0x0);
constexpr uint32_t auth_alg_leap =
    (ISupplicantStaNetwork::AuthAlgMask::LEAP | 0x0);

constexpr uint32_t group_cipher_wep40 =
    (ISupplicantStaNetwork::GroupCipherMask::WEP40 | 0x0);
constexpr uint32_t group_cipher_wep104 =
    (ISupplicantStaNetwork::GroupCipherMask::WEP104 | 0x0);
constexpr uint32_t group_cipher_tkip =
    (ISupplicantStaNetwork::GroupCipherMask::TKIP | 0x0);
constexpr uint32_t group_cipher_ccmp =
    (ISupplicantStaNetwork::GroupCipherMask::CCMP | 0x0);
constexpr uint32_t group_cipher_gtk_not_used =
    (ISupplicantStaNetwork::GroupCipherMask::GTK_NOT_USED | 0x0);

constexpr uint32_t pairwise_cipher_none =
    (ISupplicantStaNetwork::PairwiseCipherMask::NONE | 0x0);
constexpr uint32_t pairwise_cipher_tkip =
    (ISupplicantStaNetwork::PairwiseCipherMask::TKIP | 0x0);
constexpr uint32_t pairwise_cipher_ccmp =
    (ISupplicantStaNetwork::PairwiseCipherMask::CCMP | 0x0);

#define EVENT_EAP_SIM_GSM_AUTH_REQUEST u"EAP_SIM_GSM_AUTH_REQUEST"_ns
#define EVENT_EAP_SIM_UMTS_AUTH_REQUEST u"EAP_SIM_UMTS_AUTH_REQUEST"_ns
#define EVENT_EAP_SIM_IDENTITY_REQUEST u"EAP_SIM_IDENTITY_REQUEST"_ns

using namespace mozilla::dom::wifi;

mozilla::Mutex SupplicantStaNetwork::sLock("supplicant-network");

SupplicantStaNetwork::SupplicantStaNetwork(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<ISupplicantStaNetwork>& aNetwork)
    : mNetwork(aNetwork),
      mCallback(aCallback),
      mInterfaceName(aInterfaceName) {}

SupplicantStaNetwork::~SupplicantStaNetwork() {}

/**
 * Hal wrapper functions
 */
android::sp<ISupplicantStaNetworkV1_1>
SupplicantStaNetwork::GetSupplicantStaNetworkV1_1() const {
  return ISupplicantStaNetworkV1_1::castFrom(mNetwork);
}

android::sp<ISupplicantStaNetworkV1_2>
SupplicantStaNetwork::GetSupplicantStaNetworkV1_2() const {
  return ISupplicantStaNetworkV1_2::castFrom(mNetwork);
}

android::sp<ISupplicantStaNetworkV1_3>
SupplicantStaNetwork::GetSupplicantStaNetworkV1_3() const {
  return ISupplicantStaNetworkV1_3::castFrom(mNetwork);
}
/**
 * Update bssid to supplicant.
 */
Result_t SupplicantStaNetwork::UpdateBssid(const std::string& aBssid) {
  return ConvertStatusToResult(SetBssid(aBssid));
}

/**
 * Set configurations to supplicant.
 */
Result_t SupplicantStaNetwork::SetConfiguration(
    const NetworkConfiguration& aConfig) {
  NetworkConfiguration config(aConfig);

  SupplicantStatusCode stateCode = SupplicantStatusCode::FAILURE_UNKNOWN;

  // ssid
  if (!config.mSsid.empty()) {
    stateCode = SetSsid(config.mSsid);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // bssid
  if (!config.mBssid.empty()) {
    stateCode = SetBssid(config.mBssid);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // key management
  uint32_t keyMgmtMask;
  keyMgmtMask = config.mKeyMgmt.empty()
                    ? key_mgmt_none
                    : ConvertKeyMgmtToMask(Dequote(config.mKeyMgmt));
  keyMgmtMask = IncludeSha256KeyMgmt(keyMgmtMask);
  stateCode = SetKeyMgmt(keyMgmtMask);
  if (stateCode != SupplicantStatusCode::SUCCESS) {
    return ConvertStatusToResult(stateCode);
  }

  // psk
  if (!config.mPsk.empty()) {
    if (config.mPsk.front() == '"' && config.mPsk.back() == '"') {
      Dequote(config.mPsk);
      if (keyMgmtMask & key_mgmt_sae) {
        stateCode = SetSaePassword(config.mPsk);
      } else {
        stateCode = SetPskPassphrase(config.mPsk);
      }
    } else {
      stateCode = SetPsk(config.mPsk);
    }

    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // wep key
  if (!config.mWepKey0.empty() || !config.mWepKey1.empty() ||
      !config.mWepKey2.empty() || !config.mWepKey3.empty()) {
    std::array<std::string, max_wep_key_num> wepKeys = {
        config.mWepKey0, config.mWepKey1, config.mWepKey2, config.mWepKey3};
    stateCode = SetWepKey(wepKeys, aConfig.mWepTxKeyIndex);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // proto
  if (!config.mProto.empty()) {
    stateCode = SetProto(config.mProto);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // auth algorithms
  if (!config.mAuthAlg.empty()) {
    stateCode = SetAuthAlg(config.mAuthAlg);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // group cipher
  if (!config.mGroupCipher.empty()) {
    stateCode = SetGroupCipher(config.mGroupCipher);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // pairwise cipher
  if (!config.mPairwiseCipher.empty()) {
    stateCode = SetPairwiseCipher(config.mPairwiseCipher);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // pmf
  stateCode = SetRequirePmf(config.mPmf);
  if (stateCode != SupplicantStatusCode::SUCCESS) {
    return ConvertStatusToResult(stateCode);
  }

  // eap configurations
  if (config.IsEapNetwork()) {
    stateCode = SetEapConfiguration(config);
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return ConvertStatusToResult(stateCode);
    }
  }

  // Network is configured successfully, now we can try to
  // register event callback.
  stateCode = RegisterNetworkCallback();
  if (stateCode != SupplicantStatusCode::SUCCESS) {
    return ConvertStatusToResult(stateCode);
  }

  return nsIWifiResult::SUCCESS;
}

/**
 * Load network configurations from supplicant.
 */
Result_t SupplicantStaNetwork::LoadConfiguration(
    NetworkConfiguration& aConfig) {
  if (GetSsid(aConfig.mSsid) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network ssid");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (GetBssid(aConfig.mBssid) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network bssid");
  }

  if (GetKeyMgmt(aConfig.mKeyMgmt) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network key management");
  }

  if (GetPsk(aConfig.mPsk) != SupplicantStatusCode::SUCCESS &&
      aConfig.mPsk.empty()) {
    WIFI_LOGW(LOG_TAG, "Failed to get network psk");
  } else {
    if (aConfig.mKeyMgmt.compare("SAE") == 0) {
      if (GetSaePassword(aConfig.mPsk) != SupplicantStatusCode::SUCCESS &&
          aConfig.mPsk.empty()) {
        WIFI_LOGW(LOG_TAG, "Failed to get network SAE password");
      }
    } else {
      if (GetPskPassphrase(aConfig.mPsk) != SupplicantStatusCode::SUCCESS &&
          aConfig.mPsk.empty()) {
        WIFI_LOGW(LOG_TAG, "Failed to get network passphrase");
      }
    }
  }

  if (GetWepKey(0, aConfig.mWepKey0) != SupplicantStatusCode::SUCCESS &&
      GetWepKey(1, aConfig.mWepKey1) != SupplicantStatusCode::SUCCESS &&
      GetWepKey(2, aConfig.mWepKey2) != SupplicantStatusCode::SUCCESS &&
      GetWepKey(3, aConfig.mWepKey3) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network WEP key");
  }

  if (GetWepTxKeyIndex(aConfig.mWepTxKeyIndex) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network WEP key index");
  }

  if (GetScanSsid(aConfig.mScanSsid) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network scan ssid");
  }

  if (GetRequirePmf(aConfig.mPmf) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network PMF");
  }

  if (GetProto(aConfig.mProto) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network protocol");
  }

  if (GetAuthAlg(aConfig.mAuthAlg) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network authentication algorithm");
  }

  if (GetGroupCipher(aConfig.mGroupCipher) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network group cipher");
  }

  if (GetPairwiseCipher(aConfig.mPairwiseCipher) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network pairwise cipher");
  }

  if (GetEapConfiguration(aConfig) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network EAP configuration");
  }

  return nsIWifiResult::SUCCESS;
}

Result_t SupplicantStaNetwork::EnableNetwork() {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  bool noConnect = false;
  HIDL_SET(mNetwork, enable, SupplicantStatus, response, noConnect);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::DisableNetwork() {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  HIDL_CALL(mNetwork, disable, SupplicantStatus, response);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SelectNetwork() {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  HIDL_CALL(mNetwork, select, SupplicantStatus, response);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimIdentityResponse(
    SimIdentityRespDataOptions* aIdentity) {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  std::string identityStr = NS_ConvertUTF16toUTF8(aIdentity->mIdentity).get();
  std::vector<uint8_t> identity(identityStr.begin(), identityStr.end());
  HIDL_SET(mNetwork, sendNetworkEapIdentityResponse, SupplicantStatus, response,
           identity);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimGsmAuthResponse(
    const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp) {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  std::vector<ISupplicantStaNetwork::NetworkResponseEapSimGsmAuthParams>
      gsmAuthParams;

  for (auto& item : aGsmAuthResp) {
    std::string kcStr = NS_ConvertUTF16toUTF8(item.mKc).get();
    std::string sresStr = NS_ConvertUTF16toUTF8(item.mSres).get();

    ISupplicantStaNetwork::NetworkResponseEapSimGsmAuthParams params;
    std::array<uint8_t, 8> kc;
    std::array<uint8_t, 4> sres;
    if (ConvertHexStringToByteArray(kcStr, kc) < 0 ||
        ConvertHexStringToByteArray(sresStr, sres) < 0) {
      return nsIWifiResult::ERROR_INVALID_ARGS;
    }
    params.kc = kc;
    params.sres = sres;
    gsmAuthParams.push_back(params);
  }

  HIDL_SET(mNetwork, sendNetworkEapSimGsmAuthResponse, SupplicantStatus,
           response, gsmAuthParams);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimGsmAuthFailure() {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  HIDL_CALL(mNetwork, sendNetworkEapSimGsmAuthFailure, SupplicantStatus,
            response);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAuthResponse(
    SimUmtsAuthRespDataOptions* aUmtsAuthResp) {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  ISupplicantStaNetwork::NetworkResponseEapSimUmtsAuthParams umtsAuthParams;

  std::string resStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mRes).get();
  std::string ikStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mIk).get();
  std::string ckStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mCk).get();

  std::vector<uint8_t> res;
  std::array<uint8_t, 16> ik;
  std::array<uint8_t, 16> ck;
  if (ConvertHexStringToBytes(resStr, res) < 0 ||
      ConvertHexStringToByteArray(ikStr, ik) < 0 ||
      ConvertHexStringToByteArray(ckStr, ck) < 0) {
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }
  umtsAuthParams.res = res;
  umtsAuthParams.ik = ik;
  umtsAuthParams.ck = ck;

  HIDL_SET(mNetwork, sendNetworkEapSimUmtsAuthResponse, SupplicantStatus,
           response, umtsAuthParams);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAutsResponse(
    SimUmtsAutsRespDataOptions* aUmtsAutsResp) {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  std::string autsStr = NS_ConvertUTF16toUTF8(aUmtsAutsResp->mAuts).get();

  std::array<uint8_t, 14> auts;
  if (ConvertHexStringToByteArray(autsStr, auts) < 0) {
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }

  HIDL_SET(mNetwork, sendNetworkEapSimUmtsAutsResponse, SupplicantStatus,
           response, auts);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAuthFailure() {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response;
  HIDL_CALL(mNetwork, sendNetworkEapSimUmtsAuthFailure, SupplicantStatus,
            response);
  return CHECK_SUCCESS(response.code == SupplicantStatusCode::SUCCESS);
}

/**
 * Internal functions to set wifi configuration.
 */
SupplicantStatusCode SupplicantStaNetwork::SetSsid(const std::string& aSsid) {
  MOZ_ASSERT(mNetwork);
  std::string ssidStr(aSsid);
  Dequote(ssidStr);

  WIFI_LOGD(LOG_TAG, "ssid => %s", ssidStr.c_str());

  uint32_t maxSsid = static_cast<uint32_t>(
      ISupplicantStaNetwork::ParamSizeLimits::SSID_MAX_LEN_IN_BYTES);

  SupplicantStatus response;
  std::vector<uint8_t> ssid(ssidStr.begin(), ssidStr.end());

  if (ssid.size() == 0 || ssid.size() > maxSsid) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  HIDL_SET(mNetwork, setSsid, SupplicantStatus, response, ssid);
  WIFI_LOGD(LOG_TAG, "set ssid return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetBssid(const std::string& aBssid) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "bssid => %s", aBssid.c_str());

  SupplicantStatus response;
  std::array<uint8_t, 6> bssid;
  ConvertMacToByteArray(aBssid, bssid);

  HIDL_SET(mNetwork, setBssid, SupplicantStatus, response, bssid);
  WIFI_LOGD(LOG_TAG, "set bssid return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetKeyMgmt(uint32_t aKeyMgmtMask) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "key_mgmt => %d", aKeyMgmtMask);

  android::sp<ISupplicantStaNetworkV1_3> networkV1_3 =
      GetSupplicantStaNetworkV1_3();

  SupplicantStatus response;
  if (networkV1_3.get()) {
    // Use HAL V1.3 if supported.
    HIDL_SET(networkV1_3, setKeyMgmt_1_3, SupplicantStatus, response,
             aKeyMgmtMask);
  } else {
    HIDL_SET(mNetwork, setKeyMgmt, SupplicantStatus, response, aKeyMgmtMask);
  }

  WIFI_LOGD(LOG_TAG, "set key_mgmt return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetSaePassword(
    const std::string& aSaePassword) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "sae => %s", aSaePassword.c_str());

  android::sp<ISupplicantStaNetworkV1_3> networkV1_3 =
      GetSupplicantStaNetworkV1_3();

  if (!networkV1_3.get()) {
    return SupplicantStatusCode::FAILURE_NETWORK_INVALID;
  }

  SupplicantStatus response;
  HIDL_SET(networkV1_3, setPskPassphrase, SupplicantStatus, response,
           aSaePassword);
  WIFI_LOGD(LOG_TAG, "set psk return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetPskPassphrase(
    const std::string& aPassphrase) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "passphrase => %s", aPassphrase.c_str());

  uint32_t minPskPassphrase = static_cast<uint32_t>(
      ISupplicantStaNetwork::ParamSizeLimits::PSK_PASSPHRASE_MIN_LEN_IN_BYTES);
  uint32_t maxPskPassphrase = static_cast<uint32_t>(
      ISupplicantStaNetwork::ParamSizeLimits::PSK_PASSPHRASE_MAX_LEN_IN_BYTES);

  if (aPassphrase.size() < minPskPassphrase ||
      aPassphrase.size() > maxPskPassphrase) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  SupplicantStatus response;
  HIDL_SET(mNetwork, setPskPassphrase, SupplicantStatus, response, aPassphrase);
  WIFI_LOGD(LOG_TAG, "set passphrase return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetPsk(const std::string& aPsk) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "psk => %s", aPsk.c_str());

  std::array<uint8_t, 32> psk;
  // Hex string for raw psk, convert to byte array.
  if (ConvertHexStringToByteArray(aPsk, psk) < 0) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  SupplicantStatus response;
  HIDL_SET(mNetwork, setPsk, SupplicantStatus, response, psk);
  WIFI_LOGD(LOG_TAG, "set psk return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetWepKey(
    const std::array<std::string, max_wep_key_num>& aWepKeys,
    int32_t aKeyIndex) {
  MOZ_ASSERT(mNetwork);
  SupplicantStatus response = {SupplicantStatusCode::FAILURE_UNKNOWN, ""};

  for (size_t i = 0; i < aWepKeys.size(); i++) {
    std::string keyStr = aWepKeys.at(i);

    if (!keyStr.empty()) {
      Dequote(keyStr);
      uint32_t wep40Len = static_cast<uint32_t>(
          ISupplicantStaNetwork::ParamSizeLimits::WEP40_KEY_LEN_IN_BYTES);
      uint32_t wep104Len = static_cast<uint32_t>(
          ISupplicantStaNetwork::ParamSizeLimits::WEP104_KEY_LEN_IN_BYTES);

      std::vector<uint8_t> key;
      if (keyStr.size() == wep40Len || keyStr.size() == wep104Len) {
        // Key should be ASCII characters
        key = std::vector<uint8_t>(keyStr.begin(), keyStr.end());
      } else if (keyStr.size() == wep40Len * 2 ||
                 keyStr.size() == wep104Len * 2) {
        // Key should be a hexadecimal string
        if (ConvertHexStringToBytes(keyStr, key) < 0) {
          return SupplicantStatusCode::FAILURE_ARGS_INVALID;
        }
      }

      mNetwork->setWepKey(i, key, [&](const SupplicantStatus& status) {
        response = status;
        WIFI_LOGD(LOG_TAG, "set wep key return: %s",
                  ConvertStatusToString(response.code).c_str());
      });
    }
  }

  if (response.code == SupplicantStatusCode::SUCCESS) {
    HIDL_SET(mNetwork, setWepTxKeyIdx, SupplicantStatus, response, aKeyIndex);
    WIFI_LOGD(LOG_TAG, "set wep key index return: %s",
              ConvertStatusToString(response.code).c_str());
  }
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetProto(const std::string& aProto) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "proto => %s", aProto.c_str());

  uint32_t proto = ConvertProtoToMask(aProto);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setProto, SupplicantStatus, response, proto);
  WIFI_LOGD(LOG_TAG, "set proto return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetAuthAlg(
    const std::string& aAuthAlg) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "authAlg => %s", aAuthAlg.c_str());

  uint32_t authAlg = ConvertAuthAlgToMask(aAuthAlg);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setAuthAlg, SupplicantStatus, response, authAlg);
  WIFI_LOGD(LOG_TAG, "set auth alg return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetGroupCipher(
    const std::string& aGroupCipher) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "groupCipher => %s", aGroupCipher.c_str());

  uint32_t groupCipher = ConvertGroupCipherToMask(aGroupCipher);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setGroupCipher, SupplicantStatus, response, groupCipher);
  WIFI_LOGD(LOG_TAG, "set groupCipher return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetPairwiseCipher(
    const std::string& aPairwiseCipher) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "pairwiseCipher => %s", aPairwiseCipher.c_str());

  uint32_t pairwiseCipher = ConvertPairwiseCipherToMask(aPairwiseCipher);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setPairwiseCipher, SupplicantStatus, response,
           pairwiseCipher);
  WIFI_LOGD(LOG_TAG, "set pairwiseCipher return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetRequirePmf(bool aEnable) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "enable pmf => %d", aEnable);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setRequirePmf, SupplicantStatus, response, aEnable);
  WIFI_LOGD(LOG_TAG, "set pmf return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetIdStr(const std::string& aIdStr) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "enable idStr => %d", aIdStr.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setIdStr, SupplicantStatus, response, aIdStr);
  WIFI_LOGD(LOG_TAG, "set idStr return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapConfiguration(
    const NetworkConfiguration& aConfig) {
  SupplicantStatusCode stateCode = SupplicantStatusCode::FAILURE_UNKNOWN;

  // eap method
  if (!aConfig.mEap.empty()) {
    std::string eap(aConfig.mEap);
    stateCode = SetEapMethod(Dequote(eap));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // eap phase2
  if (!aConfig.mPhase2.empty()) {
    std::string phase2(aConfig.mPhase2);
    stateCode = SetEapPhase2Method(Dequote(phase2));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // identity
  if (!aConfig.mIdentity.empty()) {
    std::string identity(aConfig.mIdentity);
    stateCode = SetEapIdentity(Dequote(identity));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // anonymous identity
  if (!aConfig.mAnonymousId.empty()) {
    std::string anonymousId(aConfig.mAnonymousId);
    stateCode = SetEapAnonymousId(Dequote(anonymousId));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // password
  if (!aConfig.mPassword.empty()) {
    std::string password(aConfig.mPassword);
    stateCode = SetEapPassword(Dequote(password));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // client certificate
  if (!aConfig.mClientCert.empty()) {
    std::string clientCert(aConfig.mClientCert);
    stateCode = SetEapClientCert(Dequote(clientCert));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // CA certificate
  if (!aConfig.mCaCert.empty()) {
    std::string caCert(aConfig.mCaCert);
    stateCode = SetEapCaCert(Dequote(caCert));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // CA path
  if (!aConfig.mCaPath.empty()) {
    std::string caPath(aConfig.mCaPath);
    stateCode = SetEapCaPath(Dequote(caPath));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // subject match
  if (!aConfig.mSubjectMatch.empty()) {
    std::string subjectMatch(aConfig.mSubjectMatch);
    stateCode = SetEapSubjectMatch(Dequote(subjectMatch));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // engine Id
  if (!aConfig.mEngineId.empty()) {
    std::string engineId(aConfig.mEngineId);
    stateCode = SetEapEngineId(Dequote(engineId));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // engine
  stateCode = SetEapEngine(aConfig.mEngine);
  if (stateCode != SupplicantStatusCode::SUCCESS) {
    return stateCode;
  }

  // private key Id
  if (!aConfig.mPrivateKeyId.empty()) {
    std::string privateKeyId(aConfig.mPrivateKeyId);
    stateCode = SetEapPrivateKeyId(Dequote(privateKeyId));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // Alt subject match
  if (!aConfig.mAltSubjectMatch.empty()) {
    std::string altSubjectMatch(aConfig.mAltSubjectMatch);
    stateCode = SetEapAltSubjectMatch(Dequote(altSubjectMatch));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // domain suffix match
  if (!aConfig.mDomainSuffixMatch.empty()) {
    std::string domainSuffixMatch(aConfig.mDomainSuffixMatch);
    stateCode = SetEapDomainSuffixMatch(Dequote(domainSuffixMatch));
    if (stateCode != SupplicantStatusCode::SUCCESS) {
      return stateCode;
    }
  }

  // proactive key caching
  stateCode = SetEapProactiveKeyCaching(aConfig.mProactiveKeyCaching);
  if (stateCode != SupplicantStatusCode::SUCCESS) {
    return stateCode;
  }

  return SupplicantStatusCode::SUCCESS;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapMethod(
    const std::string& aEapMethod) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap => %s", aEapMethod.c_str());

  ISupplicantStaNetwork::EapMethod eapMethod;
  if (aEapMethod == std::string("PEAP")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::PEAP;
  } else if (aEapMethod == std::string("TLS")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::TLS;
  } else if (aEapMethod == std::string("TTLS")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::TTLS;
  } else if (aEapMethod == std::string("PWD")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::PWD;
  } else if (aEapMethod == std::string("SIM")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::SIM;
  } else if (aEapMethod == std::string("AKA")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::AKA;
  } else if (aEapMethod == std::string("AKA'")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::AKA_PRIME;
  } else if (aEapMethod == std::string("WFA_UNAUTH_TLS")) {
    eapMethod = ISupplicantStaNetwork::EapMethod::WFA_UNAUTH_TLS;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapMethod, SupplicantStatus, response, eapMethod);
  WIFI_LOGD(LOG_TAG, "set eap method return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPhase2Method(
    const std::string& aPhase2) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap phase2 => %s", aPhase2.c_str());

  ISupplicantStaNetwork::EapPhase2Method eapPhase2;
  if (aPhase2 == std::string("NONE")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::NONE;
  } else if (aPhase2 == std::string("PAP")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::PAP;
  } else if (aPhase2 == std::string("MSCHAP")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::MSPAP;
  } else if (aPhase2 == std::string("MSCHAPV2")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::MSPAPV2;
  } else if (aPhase2 == std::string("GTC")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::GTC;
  } else if (aPhase2 == std::string("SIM")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::SIM;
  } else if (aPhase2 == std::string("AKA")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::AKA;
  } else if (aPhase2 == std::string("AKA'")) {
    eapPhase2 = ISupplicantStaNetwork::EapPhase2Method::AKA_PRIME;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapPhase2Method, SupplicantStatus, response, eapPhase2);
  WIFI_LOGD(LOG_TAG, "set eap phase2 return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapIdentity(
    const std::string& aIdentity) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap identity => %s", aIdentity.c_str());

  std::vector<uint8_t> identity(aIdentity.begin(), aIdentity.end());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapIdentity, SupplicantStatus, response, identity);
  WIFI_LOGD(LOG_TAG, "set eap identity return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapAnonymousId(
    const std::string& aAnonymousId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap anonymousId => %s", aAnonymousId.c_str());

  std::vector<uint8_t> anonymousId(aAnonymousId.begin(), aAnonymousId.end());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapAnonymousIdentity, SupplicantStatus, response,
           anonymousId);
  WIFI_LOGD(LOG_TAG, "set eap anonymousId return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPassword(
    const std::string& aPassword) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap password => %s", aPassword.c_str());

  std::vector<uint8_t> password(aPassword.begin(), aPassword.end());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapPassword, SupplicantStatus, response, password);
  WIFI_LOGD(LOG_TAG, "set eap password return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapClientCert(
    const std::string& aClientCert) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap client certificate => %s", aClientCert.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapClientCert, SupplicantStatus, response, aClientCert);
  WIFI_LOGD(LOG_TAG, "set eap client certificate return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapCaCert(
    const std::string& aCaCert) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap CA certificate => %s", aCaCert.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapCACert, SupplicantStatus, response, aCaCert);
  WIFI_LOGD(LOG_TAG, "set eap CA certificate return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapCaPath(
    const std::string& aCaPath) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap CA path => %s", aCaPath.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapCAPath, SupplicantStatus, response, aCaPath);
  WIFI_LOGD(LOG_TAG, "set eap CA path return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapSubjectMatch(
    const std::string& aSubjectMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap subject match => %s", aSubjectMatch.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapSubjectMatch, SupplicantStatus, response,
           aSubjectMatch);
  WIFI_LOGD(LOG_TAG, "set eap subject match return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapEngineId(
    const std::string& aEngineId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap engine id => %s", aEngineId.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapEngineID, SupplicantStatus, response, aEngineId);
  WIFI_LOGD(LOG_TAG, "set eap engine id return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapEngine(bool aEngine) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap engine => %d", aEngine);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapEngine, SupplicantStatus, response, aEngine);
  WIFI_LOGD(LOG_TAG, "set eap engine return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPrivateKeyId(
    const std::string& aPrivateKeyId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap private key Id => %s", aPrivateKeyId.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapPrivateKeyId, SupplicantStatus, response,
           aPrivateKeyId);
  WIFI_LOGD(LOG_TAG, "set eap private key Id return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapAltSubjectMatch(
    const std::string& aAltSubjectMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap Alt subject match => %s", aAltSubjectMatch.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapAltSubjectMatch, SupplicantStatus, response,
           aAltSubjectMatch);
  WIFI_LOGD(LOG_TAG, "set eap Alt subject match return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapDomainSuffixMatch(
    const std::string& aDomainSuffixMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap domain suffix match => %s",
            aDomainSuffixMatch.c_str());

  SupplicantStatus response;
  HIDL_SET(mNetwork, setEapDomainSuffixMatch, SupplicantStatus, response,
           aDomainSuffixMatch);
  WIFI_LOGD(LOG_TAG, "set eap domain suffix match return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapProactiveKeyCaching(
    bool aProactiveKeyCaching) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "proactive key caching => %d", aProactiveKeyCaching);

  SupplicantStatus response;
  HIDL_SET(mNetwork, setProactiveKeyCaching, SupplicantStatus, response,
           aProactiveKeyCaching);
  WIFI_LOGD(LOG_TAG, "set proactive key caching return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

/**
 * Internal functions to get wifi configuration.
 */
SupplicantStatusCode SupplicantStaNetwork::GetSsid(std::string& aSsid) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getSsid([&](const SupplicantStatus& status,
                        const ::android::hardware::hidl_vec<uint8_t>& ssid) {
    response = status;
    if (response.code == SupplicantStatusCode::SUCCESS) {
      std::string ssidStr(ssid.begin(), ssid.end());
      aSsid = ssidStr.empty() ? "" : ssidStr;
      Quote(aSsid);
    }
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetBssid(std::string& aBssid) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getBssid(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
        response = status;
        if (response.code == SupplicantStatusCode::SUCCESS) {
          std::string bssidStr = ConvertMacToString(bssid);
          aBssid = bssidStr.empty() ? "" : bssidStr;
        }
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetKeyMgmt(
    std::string& aKeyMgmt) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getKeyMgmt(
      [&](const SupplicantStatus& status,
          hidl_bitfield<ISupplicantStaNetwork::KeyMgmtMask> keyMgmtMask) {
        response = status;
        uint32_t mask;
        mask = ExcludeSha256KeyMgmt((uint32_t)keyMgmtMask);
        aKeyMgmt = ConvertMaskToKeyMgmt(mask);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetPsk(std::string& aPsk) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getPsk(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_array<uint8_t, 32>& psk) {
        response = status;
        std::string pskStr = ConvertByteArrayToHexString(psk);
        aPsk = pskStr;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetPskPassphrase(
    std::string& aPassphrase) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getPskPassphrase(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_string& passphrase) {
        response = status;
        aPassphrase = passphrase;
        Quote(aPassphrase);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetSaePassword(
    std::string& aSaePassword) const {
  MOZ_ASSERT(mNetwork);

  android::sp<ISupplicantStaNetworkV1_3> networkV1_3 =
      GetSupplicantStaNetworkV1_3();

  if (!networkV1_3.get()) {
    return SupplicantStatusCode::FAILURE_NETWORK_INVALID;
  }

  SupplicantStatus response;
  networkV1_3->getSaePassword(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_string& saePassword) {
        response = status;
        aSaePassword = saePassword;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetWepKey(
    uint32_t aKeyIdx, std::string& aWepKey) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getWepKey(
      aKeyIdx, [&](const SupplicantStatus& status,
                   const ::android::hardware::hidl_vec<uint8_t>& wepKey) {
        response = status;
        std::string key(wepKey.begin(), wepKey.end());
        aWepKey = key;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetWepTxKeyIndex(
    int32_t& aWepTxKeyIndex) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getWepTxKeyIdx(
      [&](const SupplicantStatus& status, uint32_t keyIdx) {
        response = status;
        aWepTxKeyIndex = keyIdx;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetScanSsid(bool& aScanSsid) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getScanSsid([&](const SupplicantStatus& status, bool enabled) {
    response = status;
    aScanSsid = enabled;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetRequirePmf(bool& aPmf) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getRequirePmf([&](const SupplicantStatus& status, bool enabled) {
    response = status;
    aPmf = enabled;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetProto(std::string& aProto) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getProto(
      [&](const SupplicantStatus& status,
          hidl_bitfield<ISupplicantStaNetwork::ProtoMask> protoMask) {
        response = status;
        aProto = ConvertMaskToProto(protoMask);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetAuthAlg(
    std::string& aAuthAlg) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getAuthAlg(
      [&](const SupplicantStatus& status,
          hidl_bitfield<ISupplicantStaNetwork::AuthAlgMask> authAlgMask) {
        response = status;
        aAuthAlg = ConvertMaskToAuthAlg(authAlgMask);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetGroupCipher(
    std::string& aGroupCipher) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getGroupCipher(
      [&](const SupplicantStatus& status,
          hidl_bitfield<ISupplicantStaNetwork::GroupCipherMask>
              groupCipherMask) {
        response = status;
        aGroupCipher = ConvertMaskToGroupCipher(groupCipherMask);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetPairwiseCipher(
    std::string& aPairwiseCipher) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getPairwiseCipher(
      [&](const SupplicantStatus& status,
          hidl_bitfield<ISupplicantStaNetwork::PairwiseCipherMask>
              pairwiseCipherMask) {
        response = status;
        aPairwiseCipher = ConvertMaskToPairwiseCipher(pairwiseCipherMask);
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetIdStr(std::string& aIdStr) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getIdStr([&](const SupplicantStatus& status,
                         const ::android::hardware::hidl_string& idStr) {
    response = status;
    aIdStr = idStr;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapConfiguration(
    NetworkConfiguration& aConfig) const {
  if (GetEapMethod(aConfig.mEap) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network EAP method");
  }

  if (GetEapPhase2Method(aConfig.mPhase2) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network EAP phrase2 method");
  }

  if (GetEapIdentity(aConfig.mIdentity) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network EAP identity");
  }

  if (GetEapAnonymousId(aConfig.mAnonymousId) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network anonymous id");
  }

  if (GetEapPassword(aConfig.mPassword) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network password");
  }

  if (GetEapClientCert(aConfig.mClientCert) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network client private key file path");
  }

  if (GetEapCaCert(aConfig.mCaCert) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network CA certificate file path");
  }

  if (GetEapCaPath(aConfig.mCaPath) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network CA certificate directory path ");
  }

  if (GetEapSubjectMatch(aConfig.mSubjectMatch) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network subject match");
  }

  if (GetEapEngineId(aConfig.mEngineId) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network open ssl engine id");
  }

  if (GetEapEngine(aConfig.mEngine) != SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network open ssl engine state");
  }

  if (GetEapPrivateKeyId(aConfig.mPrivateKeyId) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network private key id");
  }

  if (GetEapAltSubjectMatch(aConfig.mAltSubjectMatch) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network Alt subject match");
  }

  if (GetEapDomainSuffixMatch(aConfig.mDomainSuffixMatch) !=
      SupplicantStatusCode::SUCCESS) {
    WIFI_LOGW(LOG_TAG, "Failed to get network domain suffix match");
  }

  return SupplicantStatusCode::SUCCESS;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapMethod(
    std::string& aEap) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapMethod(
      [&](const SupplicantStatus& status,
          ISupplicantStaNetwork::EapMethod method) { response = status; });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPhase2Method(
    std::string& aPhase2) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapPhase2Method(
      [&](const SupplicantStatus& status,
          ISupplicantStaNetwork::EapPhase2Method method) {
        response = status;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapIdentity(
    std::string& aIdentity) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapIdentity(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_vec<uint8_t>& identity) {
        response = status;
        std::string id(identity.begin(), identity.end());
        aIdentity = id;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapAnonymousId(
    std::string& aAnonymousId) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapAnonymousIdentity(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_vec<uint8_t>& identity) {
        response = status;
        std::string id(identity.begin(), identity.end());
        aAnonymousId = id;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPassword(
    std::string& aPassword) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapPassword(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_vec<uint8_t>& password) {
        response = status;
        std::string passwordStr(password.begin(), password.end());
        aPassword = passwordStr;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapClientCert(
    std::string& aClientCert) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapClientCert([&](const SupplicantStatus& status,
                                 const ::android::hardware::hidl_string& path) {
    response = status;
    aClientCert = path;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapCaCert(
    std::string& aCaCert) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapCACert([&](const SupplicantStatus& status,
                             const ::android::hardware::hidl_string& path) {
    response = status;
    aCaCert = path;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapCaPath(
    std::string& aCaPath) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapCAPath([&](const SupplicantStatus& status,
                             const ::android::hardware::hidl_string& path) {
    response = status;
    aCaPath = path;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapSubjectMatch(
    std::string& aSubjectMatch) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapSubjectMatch(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_string& match) {
        response = status;
        aSubjectMatch = match;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapEngineId(
    std::string& aEngineId) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapEngineID([&](const SupplicantStatus& status,
                               const ::android::hardware::hidl_string& id) {
    response = status;
    aEngineId = id;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapEngine(bool& aEngine) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapEngine([&](const SupplicantStatus& status, bool enabled) {
    response = status;
    aEngine = enabled;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPrivateKeyId(
    std::string& aPrivateKeyId) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapPrivateKeyId([&](const SupplicantStatus& status,
                                   const ::android::hardware::hidl_string& id) {
    response = status;
    aPrivateKeyId = id;
  });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapAltSubjectMatch(
    std::string& aAltSubjectMatch) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapAltSubjectMatch(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_string& match) {
        response = status;
        aAltSubjectMatch = match;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapDomainSuffixMatch(
    std::string& aDomainSuffixMatch) const {
  MOZ_ASSERT(mNetwork);

  SupplicantStatus response;
  mNetwork->getEapDomainSuffixMatch(
      [&](const SupplicantStatus& status,
          const ::android::hardware::hidl_string& match) {
        response = status;
        aDomainSuffixMatch = match;
      });
  return response.code;
}

SupplicantStatusCode SupplicantStaNetwork::RegisterNetworkCallback() {
  SupplicantStatus response;
  HIDL_SET(mNetwork, registerCallback, SupplicantStatus, response, this);
  WIFI_LOGD(LOG_TAG, "register network callback return: %s",
            ConvertStatusToString(response.code).c_str());
  return response.code;
}

uint32_t SupplicantStaNetwork::IncludeSha256KeyMgmt(uint32_t aKeyMgmt) const {
  android::sp<ISupplicantStaNetworkV1_3> networkV1_3 =
      GetSupplicantStaNetworkV1_3();

  if (!networkV1_3.get()) {
    return aKeyMgmt;
  }

  uint32_t keyMgmt = aKeyMgmt;
  keyMgmt |= (keyMgmt & key_mgmt_wpa_psk) ? key_mgmt_wpa_psk_sha256 : 0x0;
  keyMgmt |= (keyMgmt & key_mgmt_wpa_eap) ? key_mgmt_wpa_eap_sha256 : 0x0;
  return keyMgmt;
}

uint32_t SupplicantStaNetwork::ExcludeSha256KeyMgmt(uint32_t aKeyMgmt) const {
  uint32_t keyMgmt = aKeyMgmt;
  keyMgmt &= ~key_mgmt_wpa_psk_sha256;
  keyMgmt &= ~key_mgmt_wpa_eap_sha256;
  return keyMgmt;
}

/**
 * Internal helper functions.
 */
uint32_t SupplicantStaNetwork::ConvertKeyMgmtToMask(
    const std::string& aKeyMgmt) {
  uint32_t mask = 0;
  mask |= (aKeyMgmt.compare("NONE") == 0) ? key_mgmt_none : 0x0;
  mask |= (aKeyMgmt.compare("WPA-PSK") == 0) ? key_mgmt_wpa_psk : 0x0;
  mask |= (aKeyMgmt.compare("WPA2-PSK") == 0) ? key_mgmt_wpa_psk : 0x0;
  mask |=
      (aKeyMgmt.find("WPA-EAP") != std::string::npos) ? key_mgmt_wpa_eap : 0x0;
  mask |= (aKeyMgmt.find("IEEE8021X") != std::string::npos) ? key_mgmt_ieee8021x
                                                            : 0x0;
  mask |= (aKeyMgmt.compare("FT-PSK") == 0) ? key_mgmt_ft_psk : 0x0;
  mask |= (aKeyMgmt.compare("FT-EAP") == 0) ? key_mgmt_ft_eap : 0x0;
  mask |= (aKeyMgmt.compare("OSEN") == 0) ? key_mgmt_osen : 0x0;
  mask |=
      (aKeyMgmt.compare("WPA-EAP-SHA256") == 0) ? key_mgmt_wpa_eap_sha256 : 0x0;
  mask |=
      (aKeyMgmt.compare("WPA-PSK-SHA256") == 0) ? key_mgmt_wpa_psk_sha256 : 0x0;
  mask |= (aKeyMgmt.compare("SAE") == 0) ? key_mgmt_sae : 0x0;
  mask |= (aKeyMgmt.compare("SUITE-B-192") == 0) ? key_mgmt_suite_b_192 : 0x0;
  mask |= (aKeyMgmt.compare("OWE") == 0) ? key_mgmt_owe : 0x0;
  mask |= (aKeyMgmt.compare("DPP") == 0) ? key_mgmt_dpp : 0x0;
  return mask;
}

uint32_t SupplicantStaNetwork::ConvertProtoToMask(const std::string& aProto) {
  uint32_t mask = 0;
  mask |= (aProto.find("WPA") != std::string::npos) ? protocol_wpa : 0x0;
  mask |= (aProto.find("RSN") != std::string::npos) ? protocol_rsn : 0x0;
  mask |= (aProto.find("OSEN") != std::string::npos) ? protocol_osen : 0x0;
  return mask;
}

uint32_t SupplicantStaNetwork::ConvertAuthAlgToMask(
    const std::string& aAuthAlg) {
  uint32_t mask = 0;
  mask |= (aAuthAlg.find("OPEN") != std::string::npos) ? auth_alg_open : 0x0;
  mask |=
      (aAuthAlg.find("SHARED") != std::string::npos) ? auth_alg_shared : 0x0;
  mask |= (aAuthAlg.find("LEAP") != std::string::npos) ? auth_alg_leap : 0x0;
  return mask;
}

uint32_t SupplicantStaNetwork::ConvertGroupCipherToMask(
    const std::string& aGroupCipher) {
  uint32_t mask = 0;
  mask |= (aGroupCipher.find("WEP40") != std::string::npos) ? group_cipher_wep40
                                                            : 0x0;
  mask |= (aGroupCipher.find("WEP104") != std::string::npos)
              ? group_cipher_wep104
              : 0x0;
  mask |= (aGroupCipher.find("TKIP") != std::string::npos) ? group_cipher_tkip
                                                           : 0x0;
  mask |= (aGroupCipher.find("CCMP") != std::string::npos) ? group_cipher_ccmp
                                                           : 0x0;
  mask |= (aGroupCipher.find("GTK_NOT_USED") != std::string::npos)
              ? group_cipher_gtk_not_used
              : 0x0;
  return mask;
}

uint32_t SupplicantStaNetwork::ConvertPairwiseCipherToMask(
    const std::string& aPairwiseCipher) {
  uint32_t mask = 0;
  mask |= (aPairwiseCipher.find("NONE") != std::string::npos)
              ? pairwise_cipher_none
              : 0x0;
  mask |= (aPairwiseCipher.find("TKIP") != std::string::npos)
              ? pairwise_cipher_tkip
              : 0x0;
  mask |= (aPairwiseCipher.find("CCMP") != std::string::npos)
              ? pairwise_cipher_ccmp
              : 0x0;
  return mask;
}

std::string SupplicantStaNetwork::ConvertMaskToKeyMgmt(const uint32_t aMask) {
  std::string keyMgmt;
  keyMgmt += (aMask & key_mgmt_none) ? "NONE " : "";
  keyMgmt += (aMask & key_mgmt_wpa_psk) ? "WPA-PSK " : "";
  keyMgmt += (aMask & key_mgmt_wpa_eap) ? "WPA-EAP " : "";
  keyMgmt += (aMask & key_mgmt_ieee8021x) ? "IEEE8021X " : "";
  keyMgmt += (aMask & key_mgmt_ft_psk) ? "FT-PSK " : "";
  keyMgmt += (aMask & key_mgmt_ft_eap) ? "FT-EAP " : "";
  keyMgmt += (aMask & key_mgmt_osen) ? "OSEN " : "";
  keyMgmt += (aMask & key_mgmt_wpa_eap_sha256) ? "WPA-EAP-SHA256 " : "";
  keyMgmt += (aMask & key_mgmt_wpa_psk_sha256) ? "WPA-PSK-SHA256 " : "";
  keyMgmt += (aMask & key_mgmt_sae) ? "SAE " : "";
  keyMgmt += (aMask & key_mgmt_suite_b_192) ? "SUITE-B-192 " : "";
  keyMgmt += (aMask & key_mgmt_owe) ? "OWE " : "";
  keyMgmt += (aMask & key_mgmt_dpp) ? "DPP " : "";
  return Trim(keyMgmt);
}

std::string SupplicantStaNetwork::ConvertMaskToProto(const uint32_t aMask) {
  std::string proto;
  proto += (aMask & protocol_wpa) ? "WPA " : "";
  proto += (aMask & protocol_rsn) ? "RSN " : "";
  proto += (aMask & protocol_osen) ? "OSEN " : "";
  return Trim(proto);
}

std::string SupplicantStaNetwork::ConvertMaskToAuthAlg(const uint32_t aMask) {
  std::string authAlg;
  authAlg += (aMask & auth_alg_open) ? "OPEN " : "";
  authAlg += (aMask & auth_alg_shared) ? "SHARED " : "";
  authAlg += (aMask & auth_alg_leap) ? "LEAP " : "";
  return Trim(authAlg);
}

std::string SupplicantStaNetwork::ConvertMaskToGroupCipher(
    const uint32_t aMask) {
  std::string groupCipher;
  groupCipher += (aMask & group_cipher_wep40) ? "WEP40 " : "";
  groupCipher += (aMask & group_cipher_wep104) ? "WEP104 " : "";
  groupCipher += (aMask & group_cipher_tkip) ? "TKIP " : "";
  groupCipher += (aMask & group_cipher_ccmp) ? "CCMP " : "";
  groupCipher += (aMask & group_cipher_gtk_not_used) ? "GTK_NOT_USED " : "";
  return Trim(groupCipher);
}

std::string SupplicantStaNetwork::ConvertMaskToPairwiseCipher(
    const uint32_t aMask) {
  std::string pairwiseCipher;
  pairwiseCipher += (aMask & pairwise_cipher_none) ? "NONE " : "";
  pairwiseCipher += (aMask & pairwise_cipher_tkip) ? "TKIP " : "";
  pairwiseCipher += (aMask & pairwise_cipher_ccmp) ? "CCMP " : "";
  return Trim(pairwiseCipher);
}

std::string SupplicantStaNetwork::ConvertStatusToString(
    const SupplicantStatusCode& aCode) {
  switch (aCode) {
    case SupplicantStatusCode::SUCCESS:
      return "SUCCESS";
    case SupplicantStatusCode::FAILURE_UNKNOWN:
      return "FAILURE_UNKNOWN";
    case SupplicantStatusCode::FAILURE_ARGS_INVALID:
      return "FAILURE_ARGS_INVALID";
    case SupplicantStatusCode::FAILURE_IFACE_INVALID:
      return "FAILURE_IFACE_INVALID";
    case SupplicantStatusCode::FAILURE_IFACE_UNKNOWN:
      return "FAILURE_IFACE_UNKNOWN";
    case SupplicantStatusCode::FAILURE_IFACE_EXISTS:
      return "FAILURE_IFACE_EXISTS";
    case SupplicantStatusCode::FAILURE_IFACE_DISABLED:
      return "FAILURE_IFACE_DISABLED";
    case SupplicantStatusCode::FAILURE_IFACE_NOT_DISCONNECTED:
      return "FAILURE_IFACE_NOT_DISCONNECTED";
    case SupplicantStatusCode::FAILURE_NETWORK_INVALID:
      return "FAILURE_NETWORK_INVALID";
    case SupplicantStatusCode::FAILURE_NETWORK_UNKNOWN:
      return "FAILURE_NETWORK_UNKNOWN";
  }
  return "FAILURE_UNKNOWN";
}

Result_t SupplicantStaNetwork::ConvertStatusToResult(
    const SupplicantStatusCode& aCode) {
  switch (aCode) {
    case SupplicantStatusCode::SUCCESS:
      return nsIWifiResult::SUCCESS;
    case SupplicantStatusCode::FAILURE_ARGS_INVALID:
      return nsIWifiResult::ERROR_INVALID_ARGS;
    case SupplicantStatusCode::FAILURE_IFACE_INVALID:
      return nsIWifiResult::ERROR_INVALID_INTERFACE;
    case SupplicantStatusCode::FAILURE_IFACE_EXISTS:
    case SupplicantStatusCode::FAILURE_IFACE_DISABLED:
    case SupplicantStatusCode::FAILURE_IFACE_NOT_DISCONNECTED:
    case SupplicantStatusCode::FAILURE_NETWORK_INVALID:
      return nsIWifiResult::ERROR_COMMAND_FAILED;
    case SupplicantStatusCode::FAILURE_UNKNOWN:
    case SupplicantStatusCode::FAILURE_IFACE_UNKNOWN:
    case SupplicantStatusCode::FAILURE_NETWORK_UNKNOWN:
    default:
      return nsIWifiResult::ERROR_UNKNOWN;
  }
}

void SupplicantStaNetwork::NotifyEapSimGsmAuthRequest(
    const RequestGsmAuthParams& aParams) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_EAP_SIM_GSM_AUTH_REQUEST);

  nsTArray<nsString> rands(aParams.rands.size());
  for (const auto& element : aParams.rands) {
    std::stringstream stream;
    stream << std::setw(2) << std::setfill('0') << std::hex;

    // each element is set as uint8_t[16]
    for (size_t i = 0; i < 16; i++) {
      stream << (uint32_t)element[i];
    }

    nsString gsmRand(NS_ConvertUTF8toUTF16(stream.str().c_str()));
    rands.AppendElement(gsmRand);
  }
  event->updateGsmRands(rands);
  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaNetwork::NotifyEapSimUmtsAuthRequest(
    const RequestUmtsAuthParams& aParams) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_EAP_SIM_UMTS_AUTH_REQUEST);

  std::stringstream randStream, autnStream;
  for (size_t i = 0; i < 16; i++) {
    randStream << std::setw(2) << std::setfill('0') << std::hex;
    randStream << (uint32_t)aParams.rand[i];
    autnStream << std::setw(2) << std::setfill('0') << std::hex;
    autnStream << (uint32_t)aParams.autn[i];
  }

  event->mRand = NS_ConvertUTF8toUTF16(randStream.str().c_str());
  event->mAutn = NS_ConvertUTF8toUTF16(autnStream.str().c_str());
  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaNetwork::NotifyEapIdentityRequest() {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_EAP_SIM_IDENTITY_REQUEST);
  INVOKE_CALLBACK(mCallback, event, iface);
}

/**
 * ISupplicantStaNetworkCallback implementation
 */
Return<void> SupplicantStaNetwork::onNetworkEapSimGsmAuthRequest(
    const ISupplicantStaNetworkCallback::NetworkRequestEapSimGsmAuthParams&
        params) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapSimGsmAuthRequest()");

  if (params.rands.size() > 0) {
    NotifyEapSimGsmAuthRequest(params);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaNetwork::onNetworkEapSimUmtsAuthRequest(
    const ISupplicantStaNetworkCallback::NetworkRequestEapSimUmtsAuthParams&
        params) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapSimUmtsAuthRequest()");

  NotifyEapSimUmtsAuthRequest(params);
  return android::hardware::Void();
}

Return<void> SupplicantStaNetwork::onNetworkEapIdentityRequest() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapIdentityRequest()");

  NotifyEapIdentityRequest();
  return android::hardware::Void();
}
