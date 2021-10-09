/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "SoftapManager"

#include "SoftapManager.h"
#include <string.h>
#include <utils/Log.h>
#include <mozilla/ClearOnShutdown.h>

using namespace mozilla::dom::wifi;

mozilla::Mutex SoftapManager::sLock("softap");
static StaticAutoPtr<SoftapManager> sSoftapManager;

void SoftapManager::ServiceManagerDeathRecipient::serviceDied(
    uint64_t, const android::wp<IBase>&) {
  WIFI_LOGE(LOG_TAG, "IServiceManager HAL died, cleanup instance.");
  MutexAutoLock lock(sLock);

  if (mOuter != nullptr) {
    mOuter->mServiceManager = nullptr;
  }
}

void SoftapManager::HostapdDeathRecipient::serviceDied(
    uint64_t, const android::wp<IBase>&) {
  WIFI_LOGE(LOG_TAG, "IHostapd HAL died, cleanup instance.");
  MutexAutoLock lock(sLock);

  if (mOuter != nullptr) {
    mOuter->mHostapd = nullptr;
  }
}

SoftapManager::SoftapManager()
    : mServiceManager(nullptr),
      mHostapd(nullptr),
      mServiceManagerDeathRecipient(nullptr),
      mHostapdDeathRecipient(nullptr) {}

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

  Result_t result = InitServiceManager();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  // start hostapd daemon through lazy HAL.
  IHostapd_1_1::getService();
  return nsIWifiResult::SUCCESS;
}

Result_t SoftapManager::DeinitInterface() { return TearDownInterface(); }

bool SoftapManager::IsInterfaceInitializing() {
  MutexAutoLock lock(sLock);
  return mServiceManager != nullptr;
}

bool SoftapManager::IsInterfaceReady() {
  MutexAutoLock lock(sLock);
  return mHostapd != nullptr;
}

Result_t SoftapManager::InitServiceManager() {
  MutexAutoLock lock(sLock);
  if (mServiceManager != nullptr) {
    // service already existed.
    return nsIWifiResult::SUCCESS;
  }

  mHostapd = nullptr;
  mServiceManager =
      ::android::hidl::manager::V1_0::IServiceManager::getService();
  if (mServiceManager == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to get HIDL service manager");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (mServiceManagerDeathRecipient == nullptr) {
    mServiceManagerDeathRecipient =
        new ServiceManagerDeathRecipient(sSoftapManager);
  }
  Return<bool> linked =
      mServiceManager->linkToDeath(mServiceManagerDeathRecipient, 0);
  if (!linked || !linked.isOk()) {
    WIFI_LOGE(LOG_TAG, "Error on linkToDeath to IServiceManager");
    mServiceManager = nullptr;
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  // interface name android.hardware.wifi.hostapd@1.1::IHostapd
  if (!mServiceManager->registerForNotifications(IHostapd_1_1::descriptor, "",
                                                 this)) {
    WIFI_LOGE(LOG_TAG, "Failed to register for notifications to IHostapd");
    mServiceManager = nullptr;
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t SoftapManager::InitHostapdInterface() {
  MutexAutoLock lock(sLock);
  if (mHostapd != nullptr) {
    return nsIWifiResult::SUCCESS;
  }

  mHostapd = IHostapd_1_1::getService();
  if (mHostapd == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to get hostapd interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (mHostapdDeathRecipient == nullptr) {
    mHostapdDeathRecipient = new HostapdDeathRecipient(sSoftapManager);
  }

  if (mHostapdDeathRecipient != nullptr) {
    Return<bool> linked =
        mHostapd->linkToDeath(mHostapdDeathRecipient, 0 /*cookie*/);
    if (!linked || !linked.isOk()) {
      WIFI_LOGE(LOG_TAG, "Failed to link to hostapd hal death notifications");
      mHostapd = nullptr;
      return nsIWifiResult::ERROR_COMMAND_FAILED;
    }
  }

  HostapdStatus response;
  mHostapd->registerCallback(
      this, [&](const HostapdStatus& status) { response = status; });
  if (response.code != HostapdStatusCode::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to register hostapd callback: %d, reason: %s",
              response.code, response.debugMessage.c_str());
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
  }

  if (mHostapd == nullptr) {
    return InitHostapdInterface();
  }

  // Remove running softap in hostapd
  StopSoftap(aInterfaceName);

  IHostapd::IfaceParams ifaceParams;
  IHostapd::NetworkParams networkParams;

  ifaceParams.ifaceName = aInterfaceName;
  ifaceParams.hwModeParams.enable80211N = aSoftapConfig->mEnable11N;
  ifaceParams.hwModeParams.enable80211AC = aSoftapConfig->mEnable11AC;
  ifaceParams.channelParams.enableAcs = aSoftapConfig->mEnableACS;
  ifaceParams.channelParams.acsShouldExcludeDfs = aSoftapConfig->mAcsExcludeDfs;
  ifaceParams.channelParams.channel = aSoftapConfig->mChannel;
  ifaceParams.channelParams.band =
      (android::hardware::wifi::hostapd::V1_0::IHostapd::Band)
          aSoftapConfig->mBand;
  // Use 1.1 version for addAccessPoint().
  IHostapd_1_1::IfaceParams ifaceParams_1_1;
  ifaceParams_1_1.V1_0 = ifaceParams;

  std::string ssid(NS_ConvertUTF16toUTF8(aSoftapConfig->mSsid).get());
  networkParams.ssid = std::vector<uint8_t>(ssid.begin(), ssid.end());
  networkParams.isHidden = aSoftapConfig->mHidden;

  if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_NONE) {
    networkParams.encryptionType = IHostapd::EncryptionType::NONE;
  } else if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_WPA) {
    networkParams.encryptionType = IHostapd::EncryptionType::WPA;
    std::string psk(NS_ConvertUTF16toUTF8(aSoftapConfig->mKey).get());
    networkParams.pskPassphrase = psk;
  } else if (aSoftapConfig->mKeyMgmt == nsISoftapConfiguration::SECURITY_WPA2) {
    networkParams.encryptionType = IHostapd::EncryptionType::WPA2;
    std::string psk(NS_ConvertUTF16toUTF8(aSoftapConfig->mKey).get());
    networkParams.pskPassphrase = psk;
  } else {
    WIFI_LOGD(LOG_TAG, "Invalid softap security type");
    networkParams.encryptionType = IHostapd::EncryptionType::NONE;
  }

  HostapdStatus response;
  mHostapd->addAccessPoint_1_1(
      ifaceParams_1_1, networkParams,
      [&](const HostapdStatus& status) { response = status; });
  return CHECK_SUCCESS(response.code == HostapdStatusCode::SUCCESS);
}

Result_t SoftapManager::StopSoftap(const std::string& aInterfaceName) {
  HostapdStatus response;
  HIDL_SET(mHostapd, removeAccessPoint, HostapdStatus, response,
           aInterfaceName);
  return CHECK_SUCCESS(response.code == HostapdStatusCode::SUCCESS);
}

/**
 * IServiceNotification
 */
Return<void> SoftapManager::onRegistration(const hidl_string& fqName,
                                           const hidl_string& name,
                                           bool preexisting) {
  // start to initialize hostapd hidl interface.
  if (InitHostapdInterface() != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to initialize hostapd interface");
    mHostapd = nullptr;
  }
  return Return<void>();
}

/**
 * IHostapdCallback implementation
 */
Return<void> SoftapManager::onFailure(const hidl_string& ifaceName) {
  WIFI_LOGD(LOG_TAG, "IHostapdCallback::onFailure()");
  return android::hardware::Void();
}
