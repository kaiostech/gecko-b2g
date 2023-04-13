/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SupplicantStaNetwork_H
#define SupplicantStaNetwork_H

#include "WifiCommon.h"

#include <string.h>
#include <android/hardware/wifi/supplicant/1.0/ISupplicantNetwork.h>
#include <android/hardware/wifi/supplicant/1.0/types.h>
#if ANDROID_VERSION == 30
#include <android/hardware/wifi/supplicant/1.3/ISupplicantStaNetwork.h>
#else
#include <android/hardware/wifi/supplicant/1.2/ISupplicantStaNetwork.h>
#endif

#include "mozilla/Mutex.h"

using ::android::hardware::wifi::supplicant::V1_0::ISupplicantNetwork;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaNetwork;
using ::android::hardware::wifi::supplicant::V1_0::
    ISupplicantStaNetworkCallback;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatus;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatusCode;

using ISupplicantStaNetworkV1_1 =
    ::android::hardware::wifi::supplicant::V1_1::ISupplicantStaNetwork;
using ISupplicantStaNetworkV1_2 =
    ::android::hardware::wifi::supplicant::V1_2::ISupplicantStaNetwork;
#if ANDROID_VERSION == 30
using ISupplicantStaNetworkV1_3 =
    ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaNetwork;
#endif

using RequestGsmAuthParams =
    ISupplicantStaNetworkCallback::NetworkRequestEapSimGsmAuthParams;
using RequestUmtsAuthParams =
    ISupplicantStaNetworkCallback::NetworkRequestEapSimUmtsAuthParams;

constexpr uint32_t max_wep_key_num =
    (ISupplicantStaNetwork::ParamSizeLimits::WEP_KEYS_MAX_NUM | 0x0);

BEGIN_WIFI_NAMESPACE

/**
 * Class that contains the network configurations.
 */
class NetworkConfiguration {
 public:
  NetworkConfiguration() : mNetworkId(INVALID_NETWORK_ID), mSimIndex(0) {}

  NetworkConfiguration(const NetworkConfiguration& aConfig) {
    mNetworkId = aConfig.mNetworkId;
    mSsid = aConfig.mSsid;
    mBssid = aConfig.mBssid;
    mKeyMgmt = aConfig.mKeyMgmt;
    mPsk = aConfig.mPsk;
    mWepKey0 = aConfig.mWepKey0;
    mWepKey1 = aConfig.mWepKey1;
    mWepKey2 = aConfig.mWepKey2;
    mWepKey3 = aConfig.mWepKey3;
    mWepTxKeyIndex = aConfig.mWepTxKeyIndex;
    mScanSsid = aConfig.mScanSsid;
    mPmf = aConfig.mPmf;
    mProto = aConfig.mProto;
    mAuthAlg = aConfig.mAuthAlg;
    mGroupCipher = aConfig.mGroupCipher;
    mPairwiseCipher = aConfig.mPairwiseCipher;
    mEap = aConfig.mEap;
    mPhase2 = aConfig.mPhase2;
    mIdentity = aConfig.mIdentity;
    mAnonymousId = aConfig.mAnonymousId;
    mPassword = aConfig.mPassword;
    mClientCert = aConfig.mClientCert;
    mCaCert = aConfig.mCaCert;
    mCaPath = aConfig.mCaPath;
    mSubjectMatch = aConfig.mSubjectMatch;
    mEngineId = aConfig.mEngineId;
    mEngine = aConfig.mEngine;
    mPrivateKeyId = aConfig.mPrivateKeyId;
    mAltSubjectMatch = aConfig.mAltSubjectMatch;
    mDomainSuffixMatch = aConfig.mDomainSuffixMatch;
    mProactiveKeyCaching = aConfig.mProactiveKeyCaching;
    mSimIndex = aConfig.mSimIndex;
  }

  NetworkConfiguration(const ConfigurationOptions* aConfig) {
#define COPY_FIELD_STRING(configure, field)                \
  if (!configure->field.IsEmpty()) {                       \
    field = NS_ConvertUTF16toUTF8(configure->field).get(); \
  } else {                                                 \
    field = std::string();                                 \
  }
    COPY_FIELD_STRING(aConfig, mSsid)
    COPY_FIELD_STRING(aConfig, mBssid)
    COPY_FIELD_STRING(aConfig, mKeyMgmt)
    COPY_FIELD_STRING(aConfig, mPsk)
    COPY_FIELD_STRING(aConfig, mWepKey0)
    COPY_FIELD_STRING(aConfig, mWepKey1)
    COPY_FIELD_STRING(aConfig, mWepKey2)
    COPY_FIELD_STRING(aConfig, mWepKey3)
    COPY_FIELD_STRING(aConfig, mProto)
    COPY_FIELD_STRING(aConfig, mAuthAlg)
    COPY_FIELD_STRING(aConfig, mGroupCipher)
    COPY_FIELD_STRING(aConfig, mPairwiseCipher)
    COPY_FIELD_STRING(aConfig, mEap)
    COPY_FIELD_STRING(aConfig, mPhase2)
    COPY_FIELD_STRING(aConfig, mIdentity)
    COPY_FIELD_STRING(aConfig, mAnonymousId)
    COPY_FIELD_STRING(aConfig, mPassword)
    COPY_FIELD_STRING(aConfig, mClientCert)
    COPY_FIELD_STRING(aConfig, mCaCert)
    COPY_FIELD_STRING(aConfig, mCaPath)
    COPY_FIELD_STRING(aConfig, mSubjectMatch)
    COPY_FIELD_STRING(aConfig, mEngineId)
    COPY_FIELD_STRING(aConfig, mPrivateKeyId)
    COPY_FIELD_STRING(aConfig, mAltSubjectMatch)
    COPY_FIELD_STRING(aConfig, mDomainSuffixMatch)
#undef COPY_FIELD_STRING

    mNetworkId = aConfig->mNetId;
    mWepTxKeyIndex = aConfig->mWepTxKeyIndex;
    mScanSsid = aConfig->mScanSsid;
    mPmf = aConfig->mPmf;
    mEngine = aConfig->mEngine;
    mProactiveKeyCaching = aConfig->mProactiveKeyCaching;
    mSimIndex = aConfig->mSimIndex;
  }

  void ConvertConfigurations(nsWifiConfiguration* aConfig) {
#define COPY_FIELD_TO_NSSTRING(configure, field)             \
  if (!field.empty()) {                                      \
    configure->field = NS_ConvertUTF8toUTF16(field.c_str()); \
  }
    COPY_FIELD_TO_NSSTRING(aConfig, mSsid)
    COPY_FIELD_TO_NSSTRING(aConfig, mBssid)
    COPY_FIELD_TO_NSSTRING(aConfig, mKeyMgmt)
    COPY_FIELD_TO_NSSTRING(aConfig, mPsk)
    COPY_FIELD_TO_NSSTRING(aConfig, mWepKey0)
    COPY_FIELD_TO_NSSTRING(aConfig, mWepKey1)
    COPY_FIELD_TO_NSSTRING(aConfig, mWepKey2)
    COPY_FIELD_TO_NSSTRING(aConfig, mWepKey3)
    COPY_FIELD_TO_NSSTRING(aConfig, mProto)
    COPY_FIELD_TO_NSSTRING(aConfig, mAuthAlg)
    COPY_FIELD_TO_NSSTRING(aConfig, mGroupCipher)
    COPY_FIELD_TO_NSSTRING(aConfig, mPairwiseCipher)
    COPY_FIELD_TO_NSSTRING(aConfig, mEap)
    COPY_FIELD_TO_NSSTRING(aConfig, mPhase2)
    COPY_FIELD_TO_NSSTRING(aConfig, mIdentity)
    COPY_FIELD_TO_NSSTRING(aConfig, mAnonymousId)
    COPY_FIELD_TO_NSSTRING(aConfig, mPassword)
    COPY_FIELD_TO_NSSTRING(aConfig, mClientCert)
    COPY_FIELD_TO_NSSTRING(aConfig, mCaCert)
    COPY_FIELD_TO_NSSTRING(aConfig, mCaPath)
    COPY_FIELD_TO_NSSTRING(aConfig, mSubjectMatch)
    COPY_FIELD_TO_NSSTRING(aConfig, mEngineId)
    COPY_FIELD_TO_NSSTRING(aConfig, mPrivateKeyId)
    COPY_FIELD_TO_NSSTRING(aConfig, mAltSubjectMatch)
    COPY_FIELD_TO_NSSTRING(aConfig, mDomainSuffixMatch)
#undef COPY_FIELD_TO_NSSTRING

    aConfig->mNetId = mNetworkId;
    aConfig->mWepTxKeyIndex = mWepTxKeyIndex;
    aConfig->mScanSsid = mScanSsid;
    aConfig->mPmf = mPmf;
    aConfig->mEngine = mEngine;
    aConfig->mProactiveKeyCaching = mProactiveKeyCaching;
    aConfig->mSimIndex = mSimIndex;
  }

  std::string GetNetworkKey() { return mSsid + mKeyMgmt; }

  bool IsEapNetwork() {
    return !mKeyMgmt.empty() && !mEap.empty() &&
           (mKeyMgmt.find("WPA-EAP") != std::string::npos ||
            mKeyMgmt.find("IEEE8021X") != std::string::npos);
  }

  bool IsPskNetwork() {
    return !mKeyMgmt.empty() && mKeyMgmt.find("WPA-PSK") != std::string::npos;
  }

  bool IsSaeNetwork() {
    return !mKeyMgmt.empty() && mKeyMgmt.find("SAE") != std::string::npos;
  }

  bool IsWepNetwork() {
    return !mKeyMgmt.empty() && mKeyMgmt.find("NONE") != std::string::npos &&
           (!mWepKey0.empty() || !mWepKey1.empty() || !mWepKey2.empty() ||
            !mWepKey3.empty());
  }

  bool IsValidNetwork() { return !mSsid.empty() && !mKeyMgmt.empty(); }

  int32_t mNetworkId;
  std::string mSsid;
  std::string mBssid;
  std::string mKeyMgmt;
  std::string mPsk;
  std::string mWepKey0;
  std::string mWepKey1;
  std::string mWepKey2;
  std::string mWepKey3;
  int32_t mWepTxKeyIndex;
  bool mScanSsid;
  bool mPmf;
  std::string mProto;
  std::string mAuthAlg;
  std::string mGroupCipher;
  std::string mPairwiseCipher;
  std::string mEap;
  std::string mPhase2;
  std::string mIdentity;
  std::string mAnonymousId;
  std::string mPassword;
  std::string mClientCert;
  std::string mCaCert;
  std::string mCaPath;
  std::string mSubjectMatch;
  std::string mEngineId;
  bool mEngine;
  std::string mPrivateKeyId;
  std::string mAltSubjectMatch;
  std::string mDomainSuffixMatch;
  bool mProactiveKeyCaching;
  int32_t mSimIndex;
};

/**
 * A wrapper class that exports functions to configure each
 * ISupplicantStaNetwork.
 */
class SupplicantStaNetwork
    : virtual public android::RefBase,
      virtual public android::hardware::wifi::supplicant::V1_0::
          ISupplicantStaNetworkCallback {
 public:
  explicit SupplicantStaNetwork(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<ISupplicantStaNetwork>& aNetwork);

  Result_t UpdateBssid(const std::string& aBssid);
  Result_t SetConfiguration(const NetworkConfiguration& aConfig);
  Result_t LoadConfiguration(NetworkConfiguration& aConfig);
  Result_t EnableNetwork();
  Result_t DisableNetwork();
  Result_t SelectNetwork();

  Result_t SendEapSimIdentityResponse(SimIdentityRespDataOptions* aIdentity);
  Result_t SendEapSimGsmAuthResponse(
      const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp);
  Result_t SendEapSimGsmAuthFailure();
  Result_t SendEapSimUmtsAuthResponse(
      SimUmtsAuthRespDataOptions* aUmtsAuthResp);
  Result_t SendEapSimUmtsAutsResponse(
      SimUmtsAutsRespDataOptions* aUmtsAutsResp);
  Result_t SendEapSimUmtsAuthFailure();

 private:
  virtual ~SupplicantStaNetwork();

  android::sp<ISupplicantStaNetworkV1_1> GetSupplicantStaNetworkV1_1() const;
  android::sp<ISupplicantStaNetworkV1_2> GetSupplicantStaNetworkV1_2() const;
#if ANDROID_VERSION == 30
  android::sp<ISupplicantStaNetworkV1_3> GetSupplicantStaNetworkV1_3() const;
#endif

  //..................... ISupplicantStaNetworkCallback ......................./
  /**
   * Used to request EAP GSM SIM authentication for this particular network.
   *
   * The response for the request must be sent using the corresponding
   * |ISupplicantNetwork.sendNetworkEapSimGsmAuthResponse| call.
   *
   * @param params Params associated with the request.
   */
  Return<void> onNetworkEapSimGsmAuthRequest(
      const ISupplicantStaNetworkCallback::NetworkRequestEapSimGsmAuthParams&
          params) override;
  /**
   * Used to request EAP UMTS SIM authentication for this particular network.
   *
   * The response for the request must be sent using the corresponding
   * |ISupplicantNetwork.sendNetworkEapSimUmtsAuthResponse| call.
   *
   * @param params Params associated with the request.
   */
  Return<void> onNetworkEapSimUmtsAuthRequest(
      const ISupplicantStaNetworkCallback::NetworkRequestEapSimUmtsAuthParams&
          params) override;
  /**
   * Used to request EAP Identity for this particular network.
   *
   * The response for the request must be sent using the corresponding
   * |ISupplicantNetwork.sendNetworkEapIdentityResponse| call.
   */
  Return<void> onNetworkEapIdentityRequest() override;

  SupplicantStatusCode SetSsid(const std::string& aSsid);
  SupplicantStatusCode SetBssid(const std::string& aBssid);
  SupplicantStatusCode SetKeyMgmt(uint32_t aKeyMgmtMask);
  SupplicantStatusCode SetSaePassword(const std::string& aSaePassword);
  SupplicantStatusCode SetPskPassphrase(const std::string& aPassphrase);
  SupplicantStatusCode SetPsk(const std::string& aPsk);
  SupplicantStatusCode SetWepKey(
      const std::array<std::string, max_wep_key_num>& aWepKeys,
      int32_t aKeyIndex);
  SupplicantStatusCode SetProto(const std::string& aProto);
  SupplicantStatusCode SetAuthAlg(const std::string& aAuthAlg);
  SupplicantStatusCode SetGroupCipher(const std::string& aGroupCipher);
  SupplicantStatusCode SetPairwiseCipher(const std::string& aPairwiseCipher);
  SupplicantStatusCode SetRequirePmf(bool aEnable);
  SupplicantStatusCode SetIdStr(const std::string& aIdStr);

  SupplicantStatusCode SetEapConfiguration(const NetworkConfiguration& aConfig);
  SupplicantStatusCode SetEapMethod(const std::string& aEapMethod);
  SupplicantStatusCode SetEapPhase2Method(const std::string& aPhase2);
  SupplicantStatusCode SetEapIdentity(const std::string& aIdentity);
  SupplicantStatusCode SetEapAnonymousId(const std::string& aAnonymousId);
  SupplicantStatusCode SetEapPassword(const std::string& aPassword);
  SupplicantStatusCode SetEapClientCert(const std::string& aClientCert);
  SupplicantStatusCode SetEapCaCert(const std::string& aCACert);
  SupplicantStatusCode SetEapCaPath(const std::string& aCAPath);
  SupplicantStatusCode SetEapSubjectMatch(const std::string& aSubjectMatch);
  SupplicantStatusCode SetEapEngineId(const std::string& aEngineId);
  SupplicantStatusCode SetEapEngine(bool aEngine);
  SupplicantStatusCode SetEapPrivateKeyId(const std::string& aPrivateKeyId);
  SupplicantStatusCode SetEapAltSubjectMatch(
      const std::string& aAltSubjectMatch);
  SupplicantStatusCode SetEapDomainSuffixMatch(
      const std::string& aDomainSuffixMatch);
  SupplicantStatusCode SetEapProactiveKeyCaching(bool aProactiveKeyCaching);

  SupplicantStatusCode GetSsid(std::string& aSsid) const;
  SupplicantStatusCode GetBssid(std::string& aBssid) const;
  SupplicantStatusCode GetKeyMgmt(std::string& aKeyMgmtMask) const;
  SupplicantStatusCode GetPsk(std::string& aPsk) const;
  SupplicantStatusCode GetPskPassphrase(std::string& aPassphrase) const;
  SupplicantStatusCode GetSaePassword(std::string& aSaePassword) const;
  SupplicantStatusCode GetWepKey(uint32_t aKeyIdx, std::string& aWepKey) const;
  SupplicantStatusCode GetWepTxKeyIndex(int32_t& aWepTxKeyIndex) const;
  SupplicantStatusCode GetScanSsid(bool& aScanSsid) const;
  SupplicantStatusCode GetRequirePmf(bool& aPmf) const;
  SupplicantStatusCode GetProto(std::string& aProto) const;
  SupplicantStatusCode GetAuthAlg(std::string& aAuthAlg) const;
  SupplicantStatusCode GetGroupCipher(std::string& aGroupCipher) const;
  SupplicantStatusCode GetPairwiseCipher(std::string& aPairwiseCipher) const;
  SupplicantStatusCode GetIdStr(std::string& aIdStr) const;

  SupplicantStatusCode GetEapConfiguration(NetworkConfiguration& aConfig) const;
  SupplicantStatusCode GetEapMethod(std::string& aEap) const;
  SupplicantStatusCode GetEapPhase2Method(std::string& aPhase2) const;
  SupplicantStatusCode GetEapIdentity(std::string& aIdentity) const;
  SupplicantStatusCode GetEapAnonymousId(std::string& aAnonymousId) const;
  SupplicantStatusCode GetEapPassword(std::string& aPassword) const;
  SupplicantStatusCode GetEapClientCert(std::string& aClientCert) const;
  SupplicantStatusCode GetEapCaCert(std::string& aCaCert) const;
  SupplicantStatusCode GetEapCaPath(std::string& aCaPath) const;
  SupplicantStatusCode GetEapSubjectMatch(std::string& aSubjectMatch) const;
  SupplicantStatusCode GetEapEngineId(std::string& aEngineId) const;
  SupplicantStatusCode GetEapEngine(bool& aEngine) const;
  SupplicantStatusCode GetEapPrivateKeyId(std::string& aPrivateKeyId) const;
  SupplicantStatusCode GetEapAltSubjectMatch(
      std::string& aAltSubjectMatch) const;
  SupplicantStatusCode GetEapDomainSuffixMatch(
      std::string& aDomainSuffixMatch) const;
  SupplicantStatusCode RegisterNetworkCallback();

  void NotifyEapSimGsmAuthRequest(const RequestGsmAuthParams& aParams);
  void NotifyEapSimUmtsAuthRequest(const RequestUmtsAuthParams& aParams);
  void NotifyEapIdentityRequest();

  uint32_t IncludeSha256KeyMgmt(uint32_t aKeyMgmt) const;
  uint32_t ExcludeSha256KeyMgmt(uint32_t aKeyMgmt) const;

  static uint32_t ConvertKeyMgmtToMask(const std::string& aKeyMgmt);
  static uint32_t ConvertProtoToMask(const std::string& aProto);
  static uint32_t ConvertAuthAlgToMask(const std::string& aAuthAlg);
  static uint32_t ConvertGroupCipherToMask(const std::string& aGroupCipher);
  static uint32_t ConvertPairwiseCipherToMask(
      const std::string& aPairwiseCipher);
  static std::string ConvertMaskToKeyMgmt(const uint32_t aMask);
  static std::string ConvertMaskToProto(const uint32_t aMask);
  static std::string ConvertMaskToAuthAlg(const uint32_t aMask);
  static std::string ConvertMaskToGroupCipher(const uint32_t aMask);
  static std::string ConvertMaskToPairwiseCipher(const uint32_t aMask);
  static std::string ConvertStatusToString(const SupplicantStatusCode& aCode);
  static Result_t ConvertStatusToResult(const SupplicantStatusCode& aCode);

  static mozilla::Mutex sLock;

  android::sp<ISupplicantStaNetwork> mNetwork;
  android::sp<WifiEventCallback> mCallback;
  std::string mInterfaceName;
};

END_WIFI_NAMESPACE

#endif  // SupplicantStaNetwork_H
