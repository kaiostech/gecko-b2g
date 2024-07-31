/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef LOG_TAG
#define LOG_TAG "SoftapManager"

#include "SoftapManager.h"
#include <string.h>
#include <utils/Log.h>
#include <mozilla/ClearOnShutdown.h>

using ::android::binder::Status;
using ::android::hardware::wifi::hostapd::BandMask;
using ::android::hardware::wifi::hostapd::ChannelParams;
using ::android::hardware::wifi::hostapd::EncryptionType;
using ::android::hardware::wifi::hostapd::IfaceParams;
using ::android::hardware::wifi::hostapd::NetworkParams;

using namespace mozilla::dom::wifi;

mozilla::Mutex SoftapManager::sLock("softap");
static StaticAutoPtr<SoftapManager> sSoftapManager;

SoftapManager::SoftapManager() : mHostapd(nullptr) {}

SoftapManager* SoftapManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSoftapManager) {
    sSoftapManager = new SoftapManager();
    ClearOnShutdown(&sSoftapManager);
  }
  return sSoftapManager;
}

void SoftapManager::CleanUp() {
  if (sSoftapManager != nullptr) {
    SoftapManager::Get()->DeinitInterface();
  }
}

Result_t SoftapManager::InitInterface() {
  if (mHostapd != nullptr) {
    return nsIWifiResult::SUCCESS;
  }

  mHostapd = android::waitForVintfService<IHostapd>();
  return nsIWifiResult::SUCCESS;
}

Result_t SoftapManager::DeinitInterface() { return TearDownInterface(); }

bool SoftapManager::IsInterfaceReady() {
  MutexAutoLock lock(sLock);
  return mHostapd != nullptr;
}

Result_t SoftapManager::InitHostapdInterface() {
  MutexAutoLock lock(sLock);
  if (mHostapd != nullptr) {
    return nsIWifiResult::SUCCESS;
  }

  mHostapd = android::waitForVintfService<IHostapd>();

  if (mHostapd == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to get hostapd interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  Status status;
  status = mHostapd->registerCallback(this);
  if (!status.isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to register hostapd callback: %s",
              status.toString8().c_str());
    mHostapd = nullptr;
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t SoftapManager::TearDownInterface() {
  MutexAutoLock lock(sLock);
  if (mHostapd.get()) {
    mHostapd->terminate();
  }

  mHostapd = nullptr;
  return nsIWifiResult::SUCCESS;
}

Result_t SoftapManager::StartSoftap(const std::string& aInterfaceName,
                                    const std::string& aCountryCode,
                                    SoftapConfigurationOptions* aSoftapConfig) {
  if (aSoftapConfig == nullptr || aSoftapConfig->mSsid.IsEmpty()) {
    WIFI_LOGE(LOG_TAG, "Invalid configuration");
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }

  mCountryCode = aCountryCode;
  if (aSoftapConfig->mBand == nsISoftapConfiguration::AP_BAND_5GHZ) {
    if (mCountryCode.empty()) {
      WIFI_LOGE(LOG_TAG, "Must have country code for 5G band");
      return nsIWifiResult::ERROR_COMMAND_FAILED;
    }
  } else if (aSoftapConfig->mBand == nsISoftapConfiguration::AP_BAND_ANY) {
    // FIXME: BandMask must be set to a specific band. AP_BAND_ANY can be
    // removed or designed to choose the supported highest band or AOSP WiFi
    // AP/AP concurrency.
    WIFI_LOGE(LOG_TAG, "Softap currently does not support AP_BAND_ANY!");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (mHostapd == nullptr) {
    return InitHostapdInterface();
  }

  // Remove running softap in hostapd
  StopSoftap(aInterfaceName);

  IfaceParams ifaceParams;
  ChannelParams channelParams;
  NetworkParams networkParams;
  std::vector<ChannelParams> vecChannelParams;

  android::String16 ifaceName(aInterfaceName.c_str(),
                              static_cast<size_t>(aInterfaceName.length()));

  ifaceParams.name = ifaceName;
  ifaceParams.hwModeParams.enable80211N = aSoftapConfig->mEnable11N;
  ifaceParams.hwModeParams.enable80211AC = aSoftapConfig->mEnable11AC;
  channelParams.enableAcs = aSoftapConfig->mEnableACS;
  channelParams.acsShouldExcludeDfs = aSoftapConfig->mAcsExcludeDfs;
  channelParams.channel = aSoftapConfig->mChannel;

  if (aSoftapConfig->mBand == nsISoftapConfiguration::AP_BAND_24GHZ) {
    channelParams.bandMask = BandMask::BAND_2_GHZ;
  } else if (aSoftapConfig->mBand == nsISoftapConfiguration::AP_BAND_5GHZ) {
    channelParams.bandMask = BandMask::BAND_5_GHZ;
  }

  vecChannelParams.push_back(channelParams);
  ifaceParams.channelParams = vecChannelParams;

  std::string ssid(NS_ConvertUTF16toUTF8(aSoftapConfig->mSsid).get());
  networkParams.ssid = std::vector<uint8_t>(ssid.begin(), ssid.end());
  networkParams.isHidden = aSoftapConfig->mHidden;

  if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_NONE) {
    networkParams.encryptionType = EncryptionType::NONE;
  } else if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_WPA) {
    networkParams.encryptionType = EncryptionType::WPA;
    android::String16 psk((aSoftapConfig->mKey).get());
    networkParams.passphrase = psk;
  } else if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_WPA2) {
    networkParams.encryptionType = EncryptionType::WPA2;
    android::String16 psk((aSoftapConfig->mKey).get());
    networkParams.passphrase = psk;
  } else {
    WIFI_LOGD(LOG_TAG, "Invalid softap security type");
    networkParams.encryptionType = EncryptionType::NONE;
  }

  Status status;
  status = mHostapd->addAccessPoint(ifaceParams, networkParams);

  return CHECK_SUCCESS(status.isOk());
}

Result_t SoftapManager::StopSoftap(const std::string& aInterfaceName) {
  android::String16 ifaceName(aInterfaceName.c_str(),
                              static_cast<size_t>(aInterfaceName.length()));
  Status status;
  status = mHostapd->removeAccessPoint(ifaceName);
  return CHECK_SUCCESS(status.isOk());
}
