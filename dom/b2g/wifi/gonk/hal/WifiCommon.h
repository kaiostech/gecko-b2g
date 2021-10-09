/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiCommon_H
#define WifiCommon_H

#include <android-base/macros.h>
#include <unordered_map>
#include <utils/RefBase.h>
#include <utils/threads.h>
#include "mozilla/dom/WifiOptionsBinding.h"
#include "hidl/HidlSupport.h"

extern bool gWifiDebug;
#define CONFIG_WIFI_DEBUG(x) (gWifiDebug = x)

#if defined(MOZ_WIDGET_GONK)
#  include <utils/Log.h>
#  define WIFI_DEBUG 1

#  if WIFI_DEBUG
#    define WIFI_LOGD(tag, args...)                            \
      do {                                                     \
        if (gWifiDebug) {                                      \
          __android_log_print(ANDROID_LOG_DEBUG, tag, ##args); \
        }                                                      \
      } while (0)
#    define WIFI_LOGI(tag, args...)                           \
      do {                                                    \
        if (gWifiDebug) {                                     \
          __android_log_print(ANDROID_LOG_INFO, tag, ##args); \
        }                                                     \
      } while (0)
#    define WIFI_LOGW(tag, args...)                           \
      do {                                                    \
        if (gWifiDebug) {                                     \
          __android_log_print(ANDROID_LOG_WARN, tag, ##args); \
        }                                                     \
      } while (0)
#    define WIFI_LOGE(tag, args...) \
      __android_log_print(ANDROID_LOG_ERROR, tag, ##args)
#  else
#    define WIFI_LOGD(tag, args...)
#    define WIFI_LOGI(tag, args...)
#    define WIFI_LOGW(tag, args...)
#    define WIFI_LOGE(tag, args...) \
      __android_log_print(ANDROID_LOG_ERROR, tag, ##args)
#  endif /* WIFI_DEBUG */

#else

#  define WIFI_LOGD(tag, args...)
#  define WIFI_LOGI(tag, args...)
#  define WIFI_LOGW(tag, args...)
#  define WIFI_LOGE(tag, args...)

#endif /* MOZ_WIDGET_GONK */

typedef uint32_t Result_t;

#define BEGIN_WIFI_NAMESPACE \
  namespace mozilla {        \
  namespace dom {            \
  namespace wifi {

#define END_WIFI_NAMESPACE \
  } /* namespace wifi */   \
  } /* namespace dom */    \
  } /* namespace mozilla */

#define INVALID_NETWORK_ID -1

#define COPY_SEQUENCE_FIELD(prop, type)                \
  if (aOther.prop.WasPassed()) {                       \
    mozilla::dom::Sequence<type> const& currentValue = \
        aOther.prop.InternalValue();                   \
    uint32_t length = currentValue.Length();           \
    for (uint32_t idx = 0; idx < length; idx++) {      \
      prop.AppendElement(currentValue[idx]);           \
    }                                                  \
  }

#define COPY_OPT_FIELD(prop, defaultValue) \
  if (aOther.prop.WasPassed()) {           \
    prop = aOther.prop.Value();            \
  } else {                                 \
    prop = defaultValue;                   \
  }

#define COPY_FIELD(prop) prop = aOther.prop;

struct ConfigurationOptions {
 public:
  ConfigurationOptions() = default;

  explicit ConfigurationOptions(const mozilla::dom::WifiConfiguration& aOther) {
    COPY_OPT_FIELD(mNetId, INVALID_NETWORK_ID)
    COPY_OPT_FIELD(mSsid, EmptyString())
    COPY_OPT_FIELD(mBssid, EmptyString())
    COPY_OPT_FIELD(mKeyMgmt, EmptyString())
    COPY_OPT_FIELD(mPsk, EmptyString())
    COPY_OPT_FIELD(mWepKey0, EmptyString())
    COPY_OPT_FIELD(mWepKey1, EmptyString())
    COPY_OPT_FIELD(mWepKey2, EmptyString())
    COPY_OPT_FIELD(mWepKey3, EmptyString())
    COPY_OPT_FIELD(mWepTxKeyIndex, 0)
    COPY_OPT_FIELD(mScanSsid, false)
    COPY_OPT_FIELD(mPmf, false)
    COPY_OPT_FIELD(mProto, EmptyString())
    COPY_OPT_FIELD(mAuthAlg, EmptyString())
    COPY_OPT_FIELD(mGroupCipher, EmptyString())
    COPY_OPT_FIELD(mPairwiseCipher, EmptyString())
    COPY_OPT_FIELD(mEap, EmptyString())
    COPY_OPT_FIELD(mPhase2, EmptyString())
    COPY_OPT_FIELD(mIdentity, EmptyString())
    COPY_OPT_FIELD(mAnonymousId, EmptyString())
    COPY_OPT_FIELD(mPassword, EmptyString())
    COPY_OPT_FIELD(mClientCert, EmptyString())
    COPY_OPT_FIELD(mCaCert, EmptyString())
    COPY_OPT_FIELD(mCaPath, EmptyString())
    COPY_OPT_FIELD(mSubjectMatch, EmptyString())
    COPY_OPT_FIELD(mEngineId, EmptyString())
    COPY_OPT_FIELD(mEngine, false)
    COPY_OPT_FIELD(mPrivateKeyId, EmptyString())
    COPY_OPT_FIELD(mAltSubjectMatch, EmptyString())
    COPY_OPT_FIELD(mDomainSuffixMatch, EmptyString())
    COPY_OPT_FIELD(mProactiveKeyCaching, false)
    COPY_OPT_FIELD(mSimIndex, 0) {}
  }

  int32_t mNetId;
  nsString mSsid;
  nsString mBssid;
  nsString mKeyMgmt;
  nsString mPsk;
  nsString mWepKey0;
  nsString mWepKey1;
  nsString mWepKey2;
  nsString mWepKey3;
  int32_t mWepTxKeyIndex;
  bool mScanSsid;
  bool mPmf;
  nsString mProto;
  nsString mAuthAlg;
  nsString mGroupCipher;
  nsString mPairwiseCipher;
  nsString mEap;
  nsString mPhase2;
  nsString mIdentity;
  nsString mAnonymousId;
  nsString mPassword;
  nsString mClientCert;
  nsString mCaCert;
  nsString mCaPath;
  nsString mSubjectMatch;
  nsString mEngineId;
  bool mEngine;
  nsString mPrivateKeyId;
  nsString mAltSubjectMatch;
  nsString mDomainSuffixMatch;
  bool mProactiveKeyCaching;
  int32_t mSimIndex;
};

struct SoftapConfigurationOptions {
 public:
  SoftapConfigurationOptions() = default;

  explicit SoftapConfigurationOptions(
      const mozilla::dom::SoftapConfiguration& aOther) {
    COPY_OPT_FIELD(mSsid, EmptyString())
    COPY_OPT_FIELD(mKeyMgmt, 0)
    COPY_OPT_FIELD(mKey, EmptyString())
    COPY_OPT_FIELD(mCountryCode, EmptyString())
    COPY_OPT_FIELD(mBand, 0)
    COPY_OPT_FIELD(mChannel, 0)
    COPY_OPT_FIELD(mHidden, false)
    COPY_OPT_FIELD(mEnable11N, false)
    COPY_OPT_FIELD(mEnable11AC, false)
    COPY_OPT_FIELD(mEnableACS, false)
    COPY_OPT_FIELD(mAcsExcludeDfs, false) {}
  }

  nsString mSsid;
  uint32_t mKeyMgmt;
  nsString mKey;
  nsString mCountryCode;
  uint32_t mBand;
  uint32_t mChannel;
  bool mHidden;
  bool mEnable11N;
  bool mEnable11AC;
  bool mEnableACS;
  bool mAcsExcludeDfs;
};

struct SupplicantDebugLevelOptions {
 public:
  SupplicantDebugLevelOptions() = default;

  explicit SupplicantDebugLevelOptions(
      const mozilla::dom::SupplicantDebugLevel& aOther) {
    COPY_OPT_FIELD(mLogLevel, 0)
    COPY_OPT_FIELD(mShowTimeStamp, false)
    COPY_OPT_FIELD(mShowKeys, false) {}
  }

  uint32_t mLogLevel;
  bool mShowTimeStamp;
  bool mShowKeys;
};

struct ScanSettingsOptions {
 public:
  ScanSettingsOptions() = default;

  explicit ScanSettingsOptions(const mozilla::dom::ScanSettings& aOther) {
    COPY_OPT_FIELD(mScanType, 2)
    COPY_SEQUENCE_FIELD(mChannels, int32_t)
    COPY_SEQUENCE_FIELD(mHiddenNetworks, nsString) {}
  }

  ScanSettingsOptions Clone() const {
    auto other = ScanSettingsOptions();
    other.mScanType = mScanType;
    other.mChannels = mChannels.Clone();
    other.mHiddenNetworks = mHiddenNetworks.Clone();
    return other;
  }

  uint32_t mScanType;
  nsTArray<int32_t> mChannels;
  nsTArray<nsString> mHiddenNetworks;
};

struct PnoNetworkOptions {
 public:
  PnoNetworkOptions() = default;

  explicit PnoNetworkOptions(const mozilla::dom::PnoNetwork& aOther) {
    COPY_OPT_FIELD(mIsHidden, false)
    COPY_OPT_FIELD(mSsid, EmptyString())
    COPY_SEQUENCE_FIELD(mFrequencies, int32_t) {}
  }

  PnoNetworkOptions Clone() const {
    auto other = PnoNetworkOptions();
    other.mIsHidden = mIsHidden;
    other.mSsid = mSsid;
    other.mFrequencies = mFrequencies.Clone();
    return other;
  }

  bool mIsHidden;
  nsString mSsid;
  nsTArray<int32_t> mFrequencies;
};

struct PnoScanSettingsOptions {
 public:
  PnoScanSettingsOptions() = default;

  explicit PnoScanSettingsOptions(const mozilla::dom::PnoScanSettings& aOther) {
    COPY_OPT_FIELD(mInterval, 0)
    COPY_OPT_FIELD(mMin2gRssi, 0)
    COPY_OPT_FIELD(mMin5gRssi, 0) {}

    if (aOther.mPnoNetworks.WasPassed()) {
      mozilla::dom::Sequence<mozilla::dom::PnoNetwork> const& currentValue =
          aOther.mPnoNetworks.InternalValue();
      for (size_t i = 0; i < currentValue.Length(); i++) {
        PnoNetworkOptions pnoNetwork(currentValue[i]);
        mPnoNetworks.AppendElement(std::move(pnoNetwork));
      }
    }
  }

  PnoScanSettingsOptions Clone() const {
    auto other = PnoScanSettingsOptions();
    other.mInterval = mInterval;
    other.mMin2gRssi = mMin2gRssi;
    other.mMin5gRssi = mMin5gRssi;
    for (size_t i = 0; i < mPnoNetworks.Length(); i++) {
      other.mPnoNetworks.AppendElement(mPnoNetworks[i].Clone());
    }
    return other;
  }

  int32_t mInterval;
  int32_t mMin2gRssi;
  int32_t mMin5gRssi;
  nsTArray<PnoNetworkOptions> mPnoNetworks;
};

struct RoamingConfigurationOptions {
 public:
  RoamingConfigurationOptions() = default;

  explicit RoamingConfigurationOptions(
      const mozilla::dom::RoamingConfiguration& aOther) {
    COPY_SEQUENCE_FIELD(mBssidDenylist, nsString)
    COPY_SEQUENCE_FIELD(mSsidAllowlist, nsString) {}
  }

  RoamingConfigurationOptions Clone() const {
    auto other = RoamingConfigurationOptions();
    other.mBssidDenylist = mBssidDenylist.Clone();
    other.mSsidAllowlist = mSsidAllowlist.Clone();
    return other;
  }

  nsTArray<nsString> mBssidDenylist;
  nsTArray<nsString> mSsidAllowlist;
};

struct SimIdentityRespDataOptions {
 public:
  SimIdentityRespDataOptions() = default;

  explicit SimIdentityRespDataOptions(
      const mozilla::dom::SimIdentityRespData& aOther) {
    COPY_OPT_FIELD(mIdentity, EmptyString()) {}
  }

  nsString mIdentity;
};

struct SimGsmAuthRespDataOptions {
 public:
  SimGsmAuthRespDataOptions() = default;

  explicit SimGsmAuthRespDataOptions(
      const mozilla::dom::SimGsmAuthRespData& aOther) {
    COPY_OPT_FIELD(mKc, EmptyString())
    COPY_OPT_FIELD(mSres, EmptyString()) {}
  }

  nsString mKc;
  nsString mSres;
};

struct SimUmtsAuthRespDataOptions {
 public:
  SimUmtsAuthRespDataOptions() = default;

  explicit SimUmtsAuthRespDataOptions(
      const mozilla::dom::SimUmtsAuthRespData& aOther) {
    COPY_OPT_FIELD(mRes, EmptyString())
    COPY_OPT_FIELD(mIk, EmptyString())
    COPY_OPT_FIELD(mCk, EmptyString()) {}
  }

  nsString mRes;
  nsString mIk;
  nsString mCk;
};

struct SimUmtsAutsRespDataOptions {
 public:
  SimUmtsAutsRespDataOptions() = default;

  explicit SimUmtsAutsRespDataOptions(
      const mozilla::dom::SimUmtsAutsRespData& aOther) {
    COPY_OPT_FIELD(mAuts, EmptyString()) {}
  }

  nsString mAuts;
};

struct AnqpRequestSettingsOptions {
 public:
  AnqpRequestSettingsOptions() = default;

  explicit AnqpRequestSettingsOptions(
      const mozilla::dom::AnqpRequestSettings& aOther) {
    COPY_OPT_FIELD(mAnqpKey, EmptyString())
    COPY_OPT_FIELD(mBssid, EmptyString())
    COPY_OPT_FIELD(mRoamingConsortiumOIs, false)
    COPY_OPT_FIELD(mSupportRelease2, false) {}
  }

  nsString mAnqpKey;
  nsString mBssid;
  bool mRoamingConsortiumOIs;
  bool mSupportRelease2;
};

struct WpsConfigurationOptions {
 public:
  WpsConfigurationOptions() = default;

  explicit WpsConfigurationOptions(
      const mozilla::dom::WpsConfiguration& aOther) {
    COPY_OPT_FIELD(mBssid, EmptyString())
    COPY_OPT_FIELD(mPinCode, EmptyString()) {}
  }

  nsString mBssid;
  nsString mPinCode;
};

// Needed to add a copy constructor to WifiCommandOptions.
struct CommandOptions {
 public:
  explicit CommandOptions(const mozilla::dom::WifiCommandOptions& aOther) {
    COPY_FIELD(mId)
    COPY_FIELD(mCmd)
    COPY_FIELD(mEnabled)
    COPY_FIELD(mCountryCode)
    COPY_FIELD(mSoftapCountryCode)
    COPY_FIELD(mBtCoexistenceMode)
    COPY_FIELD(mBandMask)
    COPY_FIELD(mScanType) {}

    mIdentityResp = SimIdentityRespDataOptions(aOther.mIdentityResp);
    mUmtsAuthResp = SimUmtsAuthRespDataOptions(aOther.mUmtsAuthResp);
    mUmtsAutsResp = SimUmtsAutsRespDataOptions(aOther.mUmtsAutsResp);
    if (aOther.mGsmAuthResp.WasPassed()) {
      mozilla::dom::Sequence<mozilla::dom::SimGsmAuthRespData> const&
          currentValue = aOther.mGsmAuthResp.InternalValue();
      for (size_t i = 0; i < currentValue.Length(); i++) {
        SimGsmAuthRespDataOptions gsmResponse(currentValue[i]);
        mGsmAuthResp.AppendElement(std::move(gsmResponse));
      }
    }

    mConfig = ConfigurationOptions(aOther.mConfig);
    mSoftapConfig = SoftapConfigurationOptions(aOther.mSoftapConfig);
    mDebugLevel = SupplicantDebugLevelOptions(aOther.mDebugLevel);
    mScanSettings = ScanSettingsOptions(aOther.mScanSettings);
    mPnoScanSettings = PnoScanSettingsOptions(aOther.mPnoScanSettings);
    mRoamingConfig = RoamingConfigurationOptions(aOther.mRoamingConfig);
    mAnqpSettings = AnqpRequestSettingsOptions(aOther.mAnqpSettings);
    mWpsConfig = WpsConfigurationOptions(aOther.mWpsConfig);
  }

  CommandOptions(const CommandOptions& aOther) {
    mId = aOther.mId;
    mCmd = aOther.mCmd;
    mEnabled = aOther.mEnabled;
    mCountryCode = aOther.mCountryCode;
    mSoftapCountryCode = aOther.mSoftapCountryCode;
    mBtCoexistenceMode = aOther.mBtCoexistenceMode;
    mBandMask = aOther.mBandMask;
    mScanType = aOther.mScanType;

    mIdentityResp = SimIdentityRespDataOptions(aOther.mIdentityResp);
    mUmtsAuthResp = SimUmtsAuthRespDataOptions(aOther.mUmtsAuthResp);
    mUmtsAutsResp = SimUmtsAutsRespDataOptions(aOther.mUmtsAutsResp);
    mGsmAuthResp = aOther.mGsmAuthResp.Clone();

    mConfig = ConfigurationOptions(aOther.mConfig);
    mSoftapConfig = SoftapConfigurationOptions(aOther.mSoftapConfig);
    mDebugLevel = SupplicantDebugLevelOptions(aOther.mDebugLevel);
    mScanSettings = aOther.mScanSettings.Clone();
    mPnoScanSettings = aOther.mPnoScanSettings.Clone();
    mRoamingConfig = aOther.mRoamingConfig.Clone();
    mAnqpSettings = AnqpRequestSettingsOptions(aOther.mAnqpSettings);
    mWpsConfig = WpsConfigurationOptions(aOther.mWpsConfig);
  }

  // All the fields, not Optional<> anymore to get copy constructors.
  uint32_t mId;
  uint32_t mCmd;

  bool mEnabled;
  nsString mCountryCode;
  nsString mSoftapCountryCode;
  uint8_t mBtCoexistenceMode;
  uint32_t mBandMask;
  uint32_t mScanType;

  SimIdentityRespDataOptions mIdentityResp;
  SimUmtsAuthRespDataOptions mUmtsAuthResp;
  SimUmtsAutsRespDataOptions mUmtsAutsResp;
  nsTArray<SimGsmAuthRespDataOptions> mGsmAuthResp;

  ConfigurationOptions mConfig;
  SoftapConfigurationOptions mSoftapConfig;
  SupplicantDebugLevelOptions mDebugLevel;
  ScanSettingsOptions mScanSettings;
  PnoScanSettingsOptions mPnoScanSettings;
  RoamingConfigurationOptions mRoamingConfig;
  AnqpRequestSettingsOptions mAnqpSettings;
  WpsConfigurationOptions mWpsConfig;
};

#undef COPY_SEQUENCE_FIELD
#undef COPY_OPT_FIELD
#undef COPY_FIELD

template <typename T>
std::string ConvertMacToString(const T& mac);

template <typename T>
std::string ConvertByteArrayToHexString(const T& in);

template <typename T>
int32_t ConvertMacToByteArray(const std::string& mac, T& out);

template <typename T>
int32_t ConvertHexStringToBytes(const std::string& in, T& out);

template <typename T>
int32_t ConvertHexStringToByteArray(const std::string& in, T& out);

int32_t ByteToInteger(std::vector<uint8_t>::const_iterator& iter,
                      uint32_t length, int32_t endian);
std::string ByteToString(std::vector<uint8_t>::const_iterator& iter,
                         uint32_t length);
std::string Quote(std::string& s);

std::string Dequote(std::string& s);

std::string Trim(std::string& s);

#define HIDL_CALL(interface, method, responseType, response)       \
  do {                                                             \
    if (interface != nullptr) {                                    \
      interface->method(                                           \
          [&](const responseType& status) { response = status; }); \
    }                                                              \
  } while (0)

#define HIDL_SET(interface, method, responseType, response, ...)       \
  do {                                                                 \
    if (interface != nullptr) {                                        \
      interface->method(__VA_ARGS__, [&](const responseType& status) { \
        response = status;                                             \
      });                                                              \
    }                                                                  \
  } while (0)

#define INVOKE_CALLBACK(callback, event, interface) \
  do {                                              \
    if (callback) {                                 \
      callback->Notify(event, interface);           \
    }                                               \
  } while (0)

#define CHECK_SUCCESS(condition) \
  (condition) ? nsIWifiResult::SUCCESS : nsIWifiResult::ERROR_COMMAND_FAILED

#endif  // WifiCommon_H
