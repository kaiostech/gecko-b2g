/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <filesystem>
#define LOG_TAG "SupplicantStaNetwork"

#include "SupplicantStaNetwork.h"
#include <iomanip>

using ::android::hardware::wifi::supplicant::AuthAlgMask;
using ::android::hardware::wifi::supplicant::EapMethod;
using ::android::hardware::wifi::supplicant::EapPhase2Method;
using ::android::hardware::wifi::supplicant::GroupCipherMask;
using ::android::hardware::wifi::supplicant::KeyMgmtMask;
using ::android::hardware::wifi::supplicant::NetworkRequestEapSimGsmAuthParams;
using ::android::hardware::wifi::supplicant::NetworkRequestEapSimUmtsAuthParams;
using ::android::hardware::wifi::supplicant::NetworkResponseEapSimGsmAuthParams;
using ::android::hardware::wifi::supplicant::
    NetworkResponseEapSimUmtsAuthParams;
using ::android::hardware::wifi::supplicant::PairwiseCipherMask;
using ::android::hardware::wifi::supplicant::ProtoMask;

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
android::sp<ISupplicantStaNetwork>
SupplicantStaNetwork::GetSupplicantStaNetwork() const {
  return mNetwork;
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
      WIFI_LOGW(LOG_TAG, "SetBssid fail");
      return ConvertStatusToResult(stateCode);
    }
  }

  // key management
  uint32_t keyMgmtMask;
  keyMgmtMask = config.mKeyMgmt.empty()
                    ? (uint32_t)KeyMgmtMask::NONE
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
      if (keyMgmtMask & (uint32_t)KeyMgmtMask::SAE) {
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
  Status status;
  bool noConnect = false;
  status = mNetwork->enable(noConnect);
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::DisableNetwork() {
  MOZ_ASSERT(mNetwork);
  Status status;
  status = mNetwork->disable();
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SelectNetwork() {
  MOZ_ASSERT(mNetwork);
  Status status;
  status = mNetwork->select();
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SendEapSimIdentityResponse(
    SimIdentityRespDataOptions* aIdentity) {
  MOZ_ASSERT(mNetwork);
  // TODO: sendNetworkEapIdentityResponse in AIDL needs encryptedIdentity, but I
  // don't know how to pass that.
  return false;
}

Result_t SupplicantStaNetwork::SendEapSimGsmAuthResponse(
    const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp) {
  MOZ_ASSERT(mNetwork);
  std::vector<NetworkResponseEapSimGsmAuthParams> gsmAuthParams;

  for (auto& item : aGsmAuthResp) {
    std::string kcStr = NS_ConvertUTF16toUTF8(item.mKc).get();
    std::string sresStr = NS_ConvertUTF16toUTF8(item.mSres).get();

    NetworkResponseEapSimGsmAuthParams params;
    std::array<uint8_t, 8> kc;
    std::array<uint8_t, 4> sres;
    if (ConvertHexStringToByteArray(kcStr, kc) < 0 ||
        ConvertHexStringToByteArray(sresStr, sres) < 0) {
      return nsIWifiResult::ERROR_INVALID_ARGS;
    }
    std::vector<uint8_t> kcVector(kc.begin(), kc.end());
    std::vector<uint8_t> sresVector(sres.begin(), sres.end());
    params.kc = kcVector;
    params.sres = sresVector;
    gsmAuthParams.push_back(params);
  }

  Status status;
  status = mNetwork->sendNetworkEapSimGsmAuthResponse(gsmAuthParams);
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SendEapSimGsmAuthFailure() {
  MOZ_ASSERT(mNetwork);
  Status status;
  status = mNetwork->sendNetworkEapSimGsmAuthFailure();
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAuthResponse(
    SimUmtsAuthRespDataOptions* aUmtsAuthResp) {
  MOZ_ASSERT(mNetwork);
  NetworkResponseEapSimUmtsAuthParams umtsAuthParams;

  std::string resStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mRes).get();
  std::string ikStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mIk).get();
  std::string ckStr = NS_ConvertUTF16toUTF8(aUmtsAuthResp->mCk).get();

  std::vector<uint8_t> res;
  std::vector<uint8_t> ik;
  std::vector<uint8_t> ck;
  if (ConvertHexStringToBytes(resStr, res) < 0 ||
      ConvertHexStringToBytes(ikStr, ik) < 0 ||
      ConvertHexStringToBytes(ckStr, ck) < 0) {
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }

  umtsAuthParams.res = res;
  umtsAuthParams.ik = ik;
  umtsAuthParams.ck = ck;

  Status status;
  status = mNetwork->sendNetworkEapSimUmtsAuthResponse(umtsAuthParams);
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAutsResponse(
    SimUmtsAutsRespDataOptions* aUmtsAutsResp) {
  MOZ_ASSERT(mNetwork);
  std::string autsStr = NS_ConvertUTF16toUTF8(aUmtsAutsResp->mAuts).get();

  std::array<uint8_t, 14> auts;
  if (ConvertHexStringToByteArray(autsStr, auts) < 0) {
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }
  std::vector<uint8_t> autsVector(auts.begin(), auts.end());
  Status status;
  status = mNetwork->sendNetworkEapSimUmtsAutsResponse(autsVector);
  return CHECK_SUCCESS(status.isOk());
}

Result_t SupplicantStaNetwork::SendEapSimUmtsAuthFailure() {
  MOZ_ASSERT(mNetwork);
  Status status;
  status = mNetwork->sendNetworkEapSimUmtsAuthFailure();
  return CHECK_SUCCESS(status.isOk());
}

/**
 * Internal functions to set wifi configuration.
 */
SupplicantStatusCode SupplicantStaNetwork::SetSsid(const std::string& aSsid) {
  MOZ_ASSERT(mNetwork);
  std::string ssidStr(aSsid);
  Dequote(ssidStr);

  WIFI_LOGD(LOG_TAG, "ssid => %s", ssidStr.c_str());

  uint32_t maxSsid =
      static_cast<uint32_t>(ISupplicantStaNetwork::SSID_MAX_LEN_IN_BYTES);

  std::vector<uint8_t> ssid(ssidStr.begin(), ssidStr.end());

  if (ssid.size() == 0 || ssid.size() > maxSsid) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }
  Status status;
  status = mNetwork->setSsid(ssid);
  WIFI_LOGD(LOG_TAG, "set ssid return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetBssid(const std::string& aBssid) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "bssid => %s", aBssid.c_str());

  std::array<uint8_t, 6> bssid;
  ConvertMacToByteArray(aBssid, bssid);
  std::vector<uint8_t> bssidVector(bssid.begin(), bssid.end());

  Status status;
  status = mNetwork->setBssid(bssidVector);
  WIFI_LOGD(LOG_TAG, "set bssid return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetKeyMgmt(uint32_t aKeyMgmtMask) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "key_mgmt => %d", aKeyMgmtMask);

  Status status;
  // TODO: Force convert is not a good idea. Find a way to improve.
  status = mNetwork->setKeyMgmt((KeyMgmtMask)aKeyMgmtMask);
  if (!status.isOk()) {
    WIFI_LOGD(LOG_TAG, "set key_mgmt fail.");
  }
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetSaePassword(
    const std::string& aSaePassword) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "sae => %s", aSaePassword.c_str());

  if (!mNetwork) {
    return SupplicantStatusCode::FAILURE_NETWORK_INVALID;
  }

  android::String16 saePassword(aSaePassword.c_str(),
                                static_cast<size_t>(aSaePassword.length()));

  Status status;
  // TODO: Fix v3.X branch bug, should call setSaePassword instead of
  // setPskPassphrase.
  status = mNetwork->setSaePassword(saePassword);
  if (!status.isOk()) {
    WIFI_LOGD(LOG_TAG, "set sae password fail.");
  }
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetPskPassphrase(
    const std::string& aPassphrase) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "passphrase => %s", aPassphrase.c_str());

  uint32_t minPskPassphrase = static_cast<uint32_t>(
      ISupplicantStaNetwork::PSK_PASSPHRASE_MIN_LEN_IN_BYTES);
  uint32_t maxPskPassphrase = static_cast<uint32_t>(
      ISupplicantStaNetwork::PSK_PASSPHRASE_MAX_LEN_IN_BYTES);

  if (aPassphrase.size() < minPskPassphrase ||
      aPassphrase.size() > maxPskPassphrase) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  android::String16 passphrase(aPassphrase.c_str(),
                               static_cast<size_t>(aPassphrase.length()));

  Status status;
  status = mNetwork->setPskPassphrase(passphrase);
  if (!status.isOk()) {
    WIFI_LOGD(LOG_TAG, "set psk passphrase fail.");
  }
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetPsk(const std::string& aPsk) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "psk => %s", aPsk.c_str());

  std::vector<uint8_t> psk;
  // Hex string for raw psk, convert to byte vector.
  if (ConvertHexStringToBytes(aPsk, psk) < 0) {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setPsk(psk);
  if (!status.isOk()) {
    WIFI_LOGD(LOG_TAG, "set psk fail.");
  }
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetWepKey(
    const std::array<std::string, max_wep_key_num>& aWepKeys,
    int32_t aKeyIndex) {
  MOZ_ASSERT(mNetwork);
  Status status;

  for (size_t i = 0; i < aWepKeys.size(); i++) {
    std::string keyStr = aWepKeys.at(i);

    if (!keyStr.empty()) {
      Dequote(keyStr);
      uint32_t wep40Len =
          static_cast<uint32_t>(ISupplicantStaNetwork::WEP40_KEY_LEN_IN_BYTES);
      uint32_t wep104Len =
          static_cast<uint32_t>(ISupplicantStaNetwork::WEP104_KEY_LEN_IN_BYTES);

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

      status = mNetwork->setWepKey(i, key);
      WIFI_LOGD(LOG_TAG, "set wep key idx: %zu return: %d", i, status.isOk());
    }
  }

  // TODO: Here set several WEP KEY but just check last index response then call
  if (status.isOk()) {
    status = mNetwork->setWepTxKeyIdx(aKeyIndex);
    WIFI_LOGD(LOG_TAG, "set wep key index return: %d", status.isOk());
  }
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetProto(const std::string& aProto) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "proto => %s", aProto.c_str());

  ProtoMask proto;
  if ((aProto.find("WPA") != std::string::npos)) {
    proto = ProtoMask::WPA;
  } else if ((aProto.find("RSN") != std::string::npos)) {
    proto = ProtoMask::RSN;
  } else if ((aProto.find("OSEN") != std::string::npos)) {
    proto = ProtoMask::OSEN;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setProto(proto);
  WIFI_LOGD(LOG_TAG, "set proto return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetAuthAlg(
    const std::string& aAuthAlg) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "authAlg => %s", aAuthAlg.c_str());

  AuthAlgMask authAlg;
  if ((aAuthAlg.find("OPEN") != std::string::npos)) {
    authAlg = AuthAlgMask::OPEN;
  } else if ((aAuthAlg.find("SHARED") != std::string::npos)) {
    authAlg = AuthAlgMask::SHARED;
  } else if ((aAuthAlg.find("LEAP") != std::string::npos)) {
    authAlg = AuthAlgMask::LEAP;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }
  Status status;
  status = mNetwork->setAuthAlg(authAlg);
  WIFI_LOGD(LOG_TAG, "set auth return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetGroupCipher(
    const std::string& aGroupCipher) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "groupCipher => %s", aGroupCipher.c_str());

  GroupCipherMask groupCipher;

  if ((aGroupCipher.find("WEP40") != std::string::npos)) {
    groupCipher = GroupCipherMask::WEP40;
  } else if ((aGroupCipher.find("WEP104") != std::string::npos)) {
    groupCipher = GroupCipherMask::WEP104;
  } else if ((aGroupCipher.find("TKIP") != std::string::npos)) {
    groupCipher = GroupCipherMask::TKIP;
  } else if ((aGroupCipher.find("CCMP") != std::string::npos)) {
    groupCipher = GroupCipherMask::CCMP;
  } else if ((aGroupCipher.find("GTK_NOT_USED") != std::string::npos)) {
    groupCipher = GroupCipherMask::GTK_NOT_USED;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setGroupCipher(groupCipher);
  WIFI_LOGD(LOG_TAG, "set groupCipher return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetPairwiseCipher(
    const std::string& aPairwiseCipher) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "pairwiseCipher => %s", aPairwiseCipher.c_str());

  PairwiseCipherMask pairwiseCipher;
  if ((aPairwiseCipher.find("NONE") != std::string::npos)) {
    pairwiseCipher = PairwiseCipherMask::NONE;
  } else if ((aPairwiseCipher.find("TKIP") != std::string::npos)) {
    pairwiseCipher = PairwiseCipherMask::TKIP;
  } else if ((aPairwiseCipher.find("CCMP") != std::string::npos)) {
    pairwiseCipher = PairwiseCipherMask::CCMP;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setPairwiseCipher(pairwiseCipher);
  WIFI_LOGD(LOG_TAG, "set pairwiseCipher return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetRequirePmf(bool aEnable) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "enable pmf => %d", aEnable);

  Status status;
  status = mNetwork->setRequirePmf(aEnable);
  WIFI_LOGD(LOG_TAG, "set pmf return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetIdStr(const std::string& aIdStr) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "enable idStr => %s", aIdStr.c_str());

  android::String16 idStr(aIdStr.c_str(), static_cast<size_t>(aIdStr.length()));

  Status status;
  status = mNetwork->setIdStr(idStr);
  WIFI_LOGD(LOG_TAG, "set idStr return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
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

  EapMethod eapMethod;
  if (aEapMethod == std::string("PEAP")) {
    eapMethod = EapMethod::PEAP;
  } else if (aEapMethod == std::string("TLS")) {
    eapMethod = EapMethod::TLS;
  } else if (aEapMethod == std::string("TTLS")) {
    eapMethod = EapMethod::TTLS;
  } else if (aEapMethod == std::string("PWD")) {
    eapMethod = EapMethod::PWD;
  } else if (aEapMethod == std::string("SIM")) {
    eapMethod = EapMethod::SIM;
  } else if (aEapMethod == std::string("AKA")) {
    eapMethod = EapMethod::AKA;
  } else if (aEapMethod == std::string("AKA'")) {
    eapMethod = EapMethod::AKA_PRIME;
  } else if (aEapMethod == std::string("WFA_UNAUTH_TLS")) {
    eapMethod = EapMethod::WFA_UNAUTH_TLS;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setEapMethod(eapMethod);
  WIFI_LOGD(LOG_TAG, "set eap method return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPhase2Method(
    const std::string& aPhase2) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap phase2 => %s", aPhase2.c_str());

  EapPhase2Method eapPhase2;
  if (aPhase2 == std::string("NONE")) {
    eapPhase2 = EapPhase2Method::NONE;
  } else if (aPhase2 == std::string("PAP")) {
    eapPhase2 = EapPhase2Method::PAP;
  } else if (aPhase2 == std::string("MSCHAP")) {
    eapPhase2 = EapPhase2Method::MSPAP;
  } else if (aPhase2 == std::string("MSCHAPV2")) {
    eapPhase2 = EapPhase2Method::MSPAPV2;
  } else if (aPhase2 == std::string("GTC")) {
    eapPhase2 = EapPhase2Method::GTC;
  } else if (aPhase2 == std::string("SIM")) {
    eapPhase2 = EapPhase2Method::SIM;
  } else if (aPhase2 == std::string("AKA")) {
    eapPhase2 = EapPhase2Method::AKA;
  } else if (aPhase2 == std::string("AKA'")) {
    eapPhase2 = EapPhase2Method::AKA_PRIME;
  } else {
    return SupplicantStatusCode::FAILURE_ARGS_INVALID;
  }

  Status status;
  status = mNetwork->setEapPhase2Method(eapPhase2);
  WIFI_LOGD(LOG_TAG, "set eap phase2 return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapIdentity(
    const std::string& aIdentity) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap identity => %s", aIdentity.c_str());

  std::vector<uint8_t> identity(aIdentity.begin(), aIdentity.end());

  Status status;
  status = mNetwork->setEapIdentity(identity);
  WIFI_LOGD(LOG_TAG, "set eap identity return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapAnonymousId(
    const std::string& aAnonymousId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap anonymousId => %s", aAnonymousId.c_str());

  std::vector<uint8_t> anonymousId(aAnonymousId.begin(), aAnonymousId.end());

  Status status;
  status = mNetwork->setEapAnonymousIdentity(anonymousId);
  WIFI_LOGD(LOG_TAG, "set eap anonymousId return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPassword(
    const std::string& aPassword) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap password => %s", aPassword.c_str());

  std::vector<uint8_t> password(aPassword.begin(), aPassword.end());

  Status status;
  status = mNetwork->setEapPassword(password);
  WIFI_LOGD(LOG_TAG, "set eap password return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapClientCert(
    const std::string& aClientCert) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap client certificate => %s", aClientCert.c_str());

  android::String16 clientCert(aClientCert.c_str(),
                               static_cast<size_t>(aClientCert.length()));

  Status status;
  status = mNetwork->setEapClientCert(clientCert);
  WIFI_LOGD(LOG_TAG, "set eap client certificate return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapCaCert(
    const std::string& aCaCert) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap CA certificate => %s", aCaCert.c_str());

  android::String16 caCert(aCaCert.c_str(),
                           static_cast<size_t>(aCaCert.length()));

  Status status;
  status = mNetwork->setEapCACert(caCert);
  WIFI_LOGD(LOG_TAG, "set eap CA certificate return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapCaPath(
    const std::string& aCaPath) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap CA path => %s", aCaPath.c_str());

  android::String16 caPath(aCaPath.c_str(),
                           static_cast<size_t>(aCaPath.length()));

  Status status;
  status = mNetwork->setEapCAPath(caPath);
  WIFI_LOGD(LOG_TAG, "set eap CA path return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapSubjectMatch(
    const std::string& aSubjectMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap subject match => %s", aSubjectMatch.c_str());

  android::String16 subjectMatch(aSubjectMatch.c_str(),
                                 static_cast<size_t>(aSubjectMatch.length()));

  Status status;
  status = mNetwork->setEapSubjectMatch(subjectMatch);
  WIFI_LOGD(LOG_TAG, "set eap subject match return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapEngineId(
    const std::string& aEngineId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap engine id => %s", aEngineId.c_str());

  android::String16 engineId(aEngineId.c_str(),
                             static_cast<size_t>(aEngineId.length()));

  Status status;
  status = mNetwork->setEapEngineID(engineId);
  WIFI_LOGD(LOG_TAG, "set eap engine id return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapEngine(bool aEngine) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap engine => %d", aEngine);

  Status status;
  status = mNetwork->setEapEngine(aEngine);
  WIFI_LOGD(LOG_TAG, "set eap engine return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapPrivateKeyId(
    const std::string& aPrivateKeyId) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap private key Id => %s", aPrivateKeyId.c_str());

  android::String16 privateKeyId(aPrivateKeyId.c_str(),
                                 static_cast<size_t>(aPrivateKeyId.length()));

  Status status;
  status = mNetwork->setEapPrivateKeyId(privateKeyId);
  WIFI_LOGD(LOG_TAG, "set eap private key Id return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapAltSubjectMatch(
    const std::string& aAltSubjectMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap Alt subject match => %s", aAltSubjectMatch.c_str());

  android::String16 altSubjectMatch(
      aAltSubjectMatch.c_str(), static_cast<size_t>(aAltSubjectMatch.length()));

  Status status;
  status = mNetwork->setEapAltSubjectMatch(altSubjectMatch);
  WIFI_LOGD(LOG_TAG, "set eap Alt subject match return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapDomainSuffixMatch(
    const std::string& aDomainSuffixMatch) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "eap domain suffix match => %s",
            aDomainSuffixMatch.c_str());

  android::String16 domainSuffixMatch(
      aDomainSuffixMatch.c_str(),
      static_cast<size_t>(aDomainSuffixMatch.length()));

  Status status;
  status = mNetwork->setEapDomainSuffixMatch(domainSuffixMatch);
  WIFI_LOGD(LOG_TAG, "set eap domain suffix match return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::SetEapProactiveKeyCaching(
    bool aProactiveKeyCaching) {
  MOZ_ASSERT(mNetwork);
  WIFI_LOGD(LOG_TAG, "proactive key caching => %d", aProactiveKeyCaching);

  Status status;
  status = mNetwork->setProactiveKeyCaching(aProactiveKeyCaching);
  WIFI_LOGD(LOG_TAG, "set eap proactive key caching return: %d", status.isOk());
  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

/**
 * Internal functions to get wifi configuration.
 */
SupplicantStatusCode SupplicantStaNetwork::GetSsid(std::string& aSsid) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedSsid;

  Status status;
  status = mNetwork->getSsid(&retrievedSsid);

  if (status.isOk()) {
    std::string ssidStr(retrievedSsid.begin(), retrievedSsid.end());
    aSsid = ssidStr.empty() ? "" : ssidStr;
    Quote(aSsid);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetBssid(std::string& aBssid) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedBssid;
  Status status;
  status = mNetwork->getBssid(&retrievedBssid);
  if (status.isOk()) {
    std::string bssidStr = ConvertMacToString(retrievedBssid);
    aBssid = bssidStr.empty() ? "" : bssidStr;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetKeyMgmt(
    std::string& aKeyMgmt) const {
  MOZ_ASSERT(mNetwork);

  KeyMgmtMask retrievedKeyMgmt;
  Status status;
  status = mNetwork->getKeyMgmt(&retrievedKeyMgmt);
  if (status.isOk()) {
    uint32_t mask;
    mask = ExcludeSha256KeyMgmt((uint32_t)retrievedKeyMgmt);
    aKeyMgmt = ConvertMaskToKeyMgmt(mask);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetPsk(std::string& aPsk) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedPsk;
  Status status;
  status = mNetwork->getPsk(&retrievedPsk);
  if (status.isOk()) {
    std::string pskStr = ConvertByteArrayToHexString(retrievedPsk);
    aPsk = pskStr;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetPskPassphrase(
    std::string& aPassphrase) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedPassphrase;
  Status status;
  status = mNetwork->getPskPassphrase(&retrievedPassphrase);
  if (status.isOk()) {
    aPassphrase = android::String8(retrievedPassphrase).string();
    Quote(aPassphrase);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetSaePassword(
    std::string& aSaePassword) const {
  MOZ_ASSERT(mNetwork);

  if (!mNetwork) {
    return SupplicantStatusCode::FAILURE_NETWORK_INVALID;
  }

  android::String16 retrievedPassword;
  Status status;
  status = mNetwork->getSaePassword(&retrievedPassword);
  if (status.isOk()) {
    aSaePassword = android::String8(retrievedPassword).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetWepKey(
    uint32_t aKeyIdx, std::string& aWepKey) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedKey;

  Status status;
  status = mNetwork->getWepKey(aKeyIdx, &retrievedKey);
  if (status.isOk()) {
    std::string key(retrievedKey.begin(), retrievedKey.end());
    aWepKey = key;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetWepTxKeyIndex(
    int32_t& aWepTxKeyIndex) const {
  MOZ_ASSERT(mNetwork);

  int32_t retrievedKeyIdx;
  Status status;
  status = mNetwork->getWepTxKeyIdx(&retrievedKeyIdx);

  if (status.isOk()) {
    aWepTxKeyIndex = retrievedKeyIdx;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetScanSsid(bool& aScanSsid) const {
  MOZ_ASSERT(mNetwork);

  bool scanSsid = false;
  Status status;
  status = mNetwork->getScanSsid(&scanSsid);

  if (status.isOk()) {
    aScanSsid = scanSsid;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetRequirePmf(bool& aPmf) const {
  MOZ_ASSERT(mNetwork);

  bool requirePmf = false;
  Status status;
  status = mNetwork->getRequirePmf(&requirePmf);
  if (status.isOk()) {
    aPmf = requirePmf;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetProto(std::string& aProto) const {
  MOZ_ASSERT(mNetwork);

  ProtoMask retrievedProto;
  Status status;
  status = mNetwork->getProto(&retrievedProto);
  if (status.isOk()) {
    aProto = toString(retrievedProto);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetAuthAlg(
    std::string& aAuthAlg) const {
  MOZ_ASSERT(mNetwork);

  AuthAlgMask retrievedAlg;
  Status status;
  status = mNetwork->getAuthAlg(&retrievedAlg);
  if (status.isOk()) {
    aAuthAlg = toString(retrievedAlg);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetGroupCipher(
    std::string& aGroupCipher) const {
  MOZ_ASSERT(mNetwork);

  GroupCipherMask retrievedCipher;
  Status status;
  status = mNetwork->getGroupCipher(&retrievedCipher);

  if (status.isOk()) {
    aGroupCipher = toString(retrievedCipher);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetPairwiseCipher(
    std::string& aPairwiseCipher) const {
  MOZ_ASSERT(mNetwork);

  PairwiseCipherMask retrievedCipher;
  Status status;
  status = mNetwork->getPairwiseCipher(&retrievedCipher);

  if (status.isOk()) {
    aPairwiseCipher = toString(retrievedCipher);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetIdStr(std::string& aIdStr) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedIdStr;
  Status status;
  status = mNetwork->getIdStr(&retrievedIdStr);
  if (status.isOk()) {
    aIdStr = android::String8(retrievedIdStr).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
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
  // TODO: For V3.X branch aEap is not aggained. Need to be fixed.

  EapMethod retrievedMethod;
  Status status;
  status = mNetwork->getEapMethod(&retrievedMethod);
  if (status.isOk()) {
    aEap = toString(retrievedMethod);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPhase2Method(
    std::string& aPhase2) const {
  MOZ_ASSERT(mNetwork);
  // TODO: For V3.X branch aPhase2 is not aggained. Need to be fixed.

  EapPhase2Method retrievedMethod;
  Status status;
  status = mNetwork->getEapPhase2Method(&retrievedMethod);

  if (status.isOk()) {
    aPhase2 = toString(retrievedMethod);
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapIdentity(
    std::string& aIdentity) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedIdentity;
  Status status;
  status = mNetwork->getEapIdentity(&retrievedIdentity);

  if (status.isOk()) {
    std::string id(retrievedIdentity.begin(), retrievedIdentity.end());
    aIdentity = id;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapAnonymousId(
    std::string& aAnonymousId) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedIdentity;
  Status status;
  status = mNetwork->getEapAnonymousIdentity(&retrievedIdentity);

  if (status.isOk()) {
    std::string id(retrievedIdentity.begin(), retrievedIdentity.end());
    aAnonymousId = id;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPassword(
    std::string& aPassword) const {
  MOZ_ASSERT(mNetwork);

  std::vector<uint8_t> retrievedEapPasswd;
  Status status;
  status = mNetwork->getEapPassword(&retrievedEapPasswd);

  if (status.isOk()) {
    std::string passwordStr(retrievedEapPasswd.begin(),
                            retrievedEapPasswd.end());
    aPassword = passwordStr;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapClientCert(
    std::string& aClientCert) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedCert;
  Status status;
  status = mNetwork->getIdStr(&retrievedCert);
  if (status.isOk()) {
    aClientCert = android::String8(retrievedCert).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapCaCert(
    std::string& aCaCert) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedCert;
  Status status;
  status = mNetwork->getEapCACert(&retrievedCert);
  if (status.isOk()) {
    aCaCert = android::String8(retrievedCert).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapCaPath(
    std::string& aCaPath) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedCert;
  Status status;
  status = mNetwork->getEapCAPath(&retrievedCert);
  if (status.isOk()) {
    aCaPath = android::String8(retrievedCert).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapSubjectMatch(
    std::string& aSubjectMatch) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedMatch;
  Status status;
  status = mNetwork->getEapSubjectMatch(&retrievedMatch);
  if (status.isOk()) {
    aSubjectMatch = android::String8(retrievedMatch).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapEngineId(
    std::string& aEngineId) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedId;
  Status status;
  status = mNetwork->getEapEngineId(&retrievedId);
  if (status.isOk()) {
    aEngineId = android::String8(retrievedId).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapEngine(bool& aEngine) const {
  MOZ_ASSERT(mNetwork);

  bool retrievedEapEngine = false;
  Status status;
  status = mNetwork->getEapEngine(&retrievedEapEngine);
  if (status.isOk()) {
    aEngine = retrievedEapEngine;
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapPrivateKeyId(
    std::string& aPrivateKeyId) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedKeyId;
  Status status;
  status = mNetwork->getEapPrivateKeyId(&retrievedKeyId);
  if (status.isOk()) {
    aPrivateKeyId = android::String8(retrievedKeyId).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapAltSubjectMatch(
    std::string& aAltSubjectMatch) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedMatch;
  Status status;
  status = mNetwork->getEapAltSubjectMatch(&retrievedMatch);
  if (status.isOk()) {
    aAltSubjectMatch = android::String8(retrievedMatch).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::GetEapDomainSuffixMatch(
    std::string& aDomainSuffixMatch) const {
  MOZ_ASSERT(mNetwork);

  android::String16 retrievedMatch;
  Status status;
  status = mNetwork->getEapDomainSuffixMatch(&retrievedMatch);
  if (status.isOk()) {
    aDomainSuffixMatch = android::String8(retrievedMatch).string();
  }

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

SupplicantStatusCode SupplicantStaNetwork::RegisterNetworkCallback() {
  MOZ_ASSERT(mNetwork);
  Status status;
  status = mNetwork->registerCallback(this);
  WIFI_LOGD(LOG_TAG, "register network callback return: %d", status.isOk());

  return status.isOk() ? SupplicantStatusCode::SUCCESS
                       : SupplicantStatusCode::FAILURE_UNKNOWN;
}

uint32_t SupplicantStaNetwork::IncludeSha256KeyMgmt(uint32_t aKeyMgmt) const {
  if (!mNetwork) {
    return aKeyMgmt;
  }

  uint32_t keyMgmt = aKeyMgmt;
  keyMgmt |= (keyMgmt & (uint32_t)KeyMgmtMask::WPA_PSK)
                 ? (uint32_t)KeyMgmtMask::WPA_PSK_SHA256
                 : 0x0;
  keyMgmt |= (keyMgmt & (uint32_t)KeyMgmtMask::WPA_EAP)
                 ? (uint32_t)KeyMgmtMask::WPA_EAP_SHA256
                 : 0x0;
  return keyMgmt;
}

uint32_t SupplicantStaNetwork::ExcludeSha256KeyMgmt(uint32_t aKeyMgmt) const {
  uint32_t keyMgmt = aKeyMgmt;
  keyMgmt &= ~(uint32_t)KeyMgmtMask::WPA_PSK_SHA256;
  keyMgmt &= ~(uint32_t)KeyMgmtMask::WPA_EAP_SHA256;
  return keyMgmt;
}

/**
 * Internal helper functions.
 */
uint32_t SupplicantStaNetwork::ConvertKeyMgmtToMask(
    const std::string& aKeyMgmt) {
  uint32_t mask = 0;
  mask |= (aKeyMgmt.compare("NONE") == 0) ? (uint32_t)KeyMgmtMask::NONE : 0x0;
  mask |=
      (aKeyMgmt.compare("WPA-PSK") == 0) ? (uint32_t)KeyMgmtMask::WPA_PSK : 0x0;
  mask |= (aKeyMgmt.compare("WPA2-PSK") == 0) ? (uint32_t)KeyMgmtMask::WPA_PSK
                                              : 0x0;
  mask |= (aKeyMgmt.find("WPA-EAP") != std::string::npos)
              ? (uint32_t)KeyMgmtMask::WPA_EAP
              : 0x0;
  mask |= (aKeyMgmt.find("IEEE8021X") != std::string::npos)
              ? (uint32_t)KeyMgmtMask::IEEE8021X
              : 0x0;
  mask |=
      (aKeyMgmt.compare("FT-PSK") == 0) ? (uint32_t)KeyMgmtMask::FT_PSK : 0x0;
  mask |=
      (aKeyMgmt.compare("FT-EAP") == 0) ? (uint32_t)KeyMgmtMask::FT_EAP : 0x0;
  mask |= (aKeyMgmt.compare("OSEN") == 0) ? (uint32_t)KeyMgmtMask::OSEN : 0x0;
  mask |= (aKeyMgmt.compare("WPA-EAP-SHA256") == 0)
              ? (uint32_t)KeyMgmtMask::WPA_EAP_SHA256
              : 0x0;
  mask |= (aKeyMgmt.compare("WPA-PSK-SHA256") == 0)
              ? (uint32_t)KeyMgmtMask::WPA_PSK_SHA256
              : 0x0;
  mask |= (aKeyMgmt.compare("SAE") == 0) ? (uint32_t)KeyMgmtMask::SAE : 0x0;
  mask |= (aKeyMgmt.compare("SUITE-B-192") == 0)
              ? (uint32_t)KeyMgmtMask::SUITE_B_192
              : 0x0;
  mask |= (aKeyMgmt.compare("OWE") == 0) ? (uint32_t)KeyMgmtMask::OWE : 0x0;
  mask |= (aKeyMgmt.compare("DPP") == 0) ? (uint32_t)KeyMgmtMask::DPP : 0x0;
  return mask;
}

// TODO: Should No need this anymore
uint32_t SupplicantStaNetwork::ConvertProtoToMask(const std::string& aProto) {
  uint32_t mask = 0;
  mask |= (aProto.find("WPA") != std::string::npos) ? (uint32_t)ProtoMask::WPA
                                                    : 0x0;
  mask |= (aProto.find("RSN") != std::string::npos) ? (uint32_t)ProtoMask::RSN
                                                    : 0x0;
  mask |= (aProto.find("OSEN") != std::string::npos) ? (uint32_t)ProtoMask::OSEN
                                                     : 0x0;
  return mask;
}

// TODO: Should No need this anymore
uint32_t SupplicantStaNetwork::ConvertAuthAlgToMask(
    const std::string& aAuthAlg) {
  uint32_t mask = 0;
  mask |= (aAuthAlg.find("OPEN") != std::string::npos)
              ? (uint32_t)AuthAlgMask::OPEN
              : 0x0;
  mask |= (aAuthAlg.find("SHARED") != std::string::npos)
              ? (uint32_t)AuthAlgMask::SHARED
              : 0x0;
  mask |= (aAuthAlg.find("LEAP") != std::string::npos)
              ? (uint32_t)AuthAlgMask::LEAP
              : 0x0;
  return mask;
}

// TODO: Should No need this anymore
uint32_t SupplicantStaNetwork::ConvertGroupCipherToMask(
    const std::string& aGroupCipher) {
  uint32_t mask = 0;
  mask |= (aGroupCipher.find("WEP40") != std::string::npos)
              ? (uint32_t)GroupCipherMask::WEP40
              : 0x0;
  mask |= (aGroupCipher.find("WEP104") != std::string::npos)
              ? (uint32_t)GroupCipherMask::WEP104
              : 0x0;
  mask |= (aGroupCipher.find("TKIP") != std::string::npos)
              ? (uint32_t)GroupCipherMask::TKIP
              : 0x0;
  mask |= (aGroupCipher.find("CCMP") != std::string::npos)
              ? (uint32_t)GroupCipherMask::CCMP
              : 0x0;
  mask |= (aGroupCipher.find("GTK_NOT_USED") != std::string::npos)
              ? (uint32_t)GroupCipherMask::GTK_NOT_USED
              : 0x0;
  return mask;
}

// TODO: Should No need this anymore
uint32_t SupplicantStaNetwork::ConvertPairwiseCipherToMask(
    const std::string& aPairwiseCipher) {
  uint32_t mask = 0;
  mask |= (aPairwiseCipher.find("NONE") != std::string::npos)
              ? (uint32_t)PairwiseCipherMask::NONE
              : 0x0;
  mask |= (aPairwiseCipher.find("TKIP") != std::string::npos)
              ? (uint32_t)PairwiseCipherMask::TKIP
              : 0x0;
  mask |= (aPairwiseCipher.find("CCMP") != std::string::npos)
              ? (uint32_t)PairwiseCipherMask::CCMP
              : 0x0;
  return mask;
}

std::string SupplicantStaNetwork::ConvertMaskToKeyMgmt(const uint32_t aMask) {
  std::string keyMgmt;
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::NONE) ? "NONE " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::WPA_PSK) ? "WPA-PSK " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::WPA_EAP) ? "WPA-EAP " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::IEEE8021X) ? "IEEE8021X " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::FT_PSK) ? "FT-PSK " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::FT_EAP) ? "FT-EAP " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::OSEN) ? "OSEN " : "";
  keyMgmt +=
      (aMask & (uint32_t)KeyMgmtMask::WPA_EAP_SHA256) ? "WPA-EAP-SHA256 " : "";
  keyMgmt +=
      (aMask & (uint32_t)KeyMgmtMask::WPA_PSK_SHA256) ? "WPA-PSK-SHA256 " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::SAE) ? "SAE " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::SUITE_B_192) ? "SUITE-B-192 " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::OWE) ? "OWE " : "";
  keyMgmt += (aMask & (uint32_t)KeyMgmtMask::DPP) ? "DPP " : "";
  return Trim(keyMgmt);
}

std::string SupplicantStaNetwork::ConvertMaskToProto(const uint32_t aMask) {
  std::string proto;
  proto += (aMask & (uint32_t)ProtoMask::WPA) ? "WPA " : "";
  proto += (aMask & (uint32_t)ProtoMask::RSN) ? "RSN " : "";
  proto += (aMask & (uint32_t)ProtoMask::OSEN) ? "OSEN " : "";
  return Trim(proto);
}

std::string SupplicantStaNetwork::ConvertMaskToAuthAlg(const uint32_t aMask) {
  std::string authAlg;
  authAlg += (aMask & (uint32_t)AuthAlgMask::OPEN) ? "OPEN " : "";
  authAlg += (aMask & (uint32_t)AuthAlgMask::SHARED) ? "SHARED " : "";
  authAlg += (aMask & (uint32_t)AuthAlgMask::LEAP) ? "LEAP " : "";
  return Trim(authAlg);
}

std::string SupplicantStaNetwork::ConvertMaskToGroupCipher(
    const uint32_t aMask) {
  std::string groupCipher;
  groupCipher += (aMask & (uint32_t)GroupCipherMask::WEP40) ? "WEP40 " : "";
  groupCipher += (aMask & (uint32_t)GroupCipherMask::WEP104) ? "WEP104 " : "";
  groupCipher += (aMask & (uint32_t)GroupCipherMask::TKIP) ? "TKIP " : "";
  groupCipher += (aMask & (uint32_t)GroupCipherMask::CCMP) ? "CCMP " : "";
  groupCipher +=
      (aMask & (uint32_t)GroupCipherMask::GTK_NOT_USED) ? "GTK_NOT_USED " : "";
  return Trim(groupCipher);
}

std::string SupplicantStaNetwork::ConvertMaskToPairwiseCipher(
    const uint32_t aMask) {
  std::string pairwiseCipher;
  pairwiseCipher += (aMask & (uint32_t)PairwiseCipherMask::NONE) ? "NONE " : "";
  pairwiseCipher += (aMask & (uint32_t)PairwiseCipherMask::TKIP) ? "TKIP " : "";
  pairwiseCipher += (aMask & (uint32_t)PairwiseCipherMask::CCMP) ? "CCMP " : "";
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
    default:
      return "FAILURE_UNKNOWN";
  }
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
  // TODO NO impl now, need to fix.
  return;
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
::android::binder::Status SupplicantStaNetwork::onNetworkEapSimGsmAuthRequest(
    const NetworkRequestEapSimGsmAuthParams& params) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapSimGsmAuthRequest()");

  if (params.rands.size() > 0) {
    NotifyEapSimGsmAuthRequest(params);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaNetwork::onNetworkEapSimUmtsAuthRequest(
    const NetworkRequestEapSimUmtsAuthParams& params) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapSimUmtsAuthRequest()");

  NotifyEapSimUmtsAuthRequest(params);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaNetwork::onNetworkEapIdentityRequest() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onNetworkEapIdentityRequest()");

  NotifyEapIdentityRequest();
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaNetwork::onTransitionDisable(
    TransitionDisableIndication ind) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaNetworkCallback.onTransitionDisable()");
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaNetwork::onServerCertificateAvailable(
    int32_t depth, const std::vector<uint8_t>& subject,
    const std::vector<uint8_t>& certHash,
    const std::vector<uint8_t>& certBlob) {
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onServerCertificateAvailable()");
  return ::android::binder::Status::fromStatusT(::android::OK);
}

#if ANDROID_VERSION >= 34
::android::binder::Status SupplicantStaNetwork::onPermanentIdReqDenied() {
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaNetworkCallback.onPermanentIdReqDenied()");
  return ::android::binder::Status::fromStatusT(::android::OK);
}
#endif
