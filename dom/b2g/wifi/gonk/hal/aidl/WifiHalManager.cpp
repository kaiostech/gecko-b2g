/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef LOG_TAG
#define LOG_TAG "WifiHal"

#include <utils/Log.h>
#include "WifiHalManager.h"
#include <mozilla/ClearOnShutdown.h>

using ::android::binder::Status;
using ::android::hardware::wifi::MacAddress;
using ::android::hardware::wifi::Ssid;
using ::android::hardware::wifi::StaRoamingCapabilities;
using ::android::hardware::wifi::StaRoamingConfig;
using ::android::hardware::wifi::StaRoamingState;
using ::android::hardware::wifi::WifiStatusCode;

using namespace mozilla::dom::wifi;

static StaticAutoPtr<WifiHal> sWifiHal;
mozilla::Mutex WifiHal::sLock("wifi-aidl");

bool findAnyModeSupportingConcurrencyType(
    IfaceConcurrencyType desiredType,
    const std::vector<IWifiChip::ChipMode>& modes, int* modeId) {
  for (const auto& mode : modes) {
    for (const auto& combination : mode.availableCombinations) {
      for (const auto& ifaceLimit : combination.limits) {
        const auto& ifaceTypes = ifaceLimit.types;
        if (std::find(ifaceTypes.begin(), ifaceTypes.end(), desiredType) !=
            ifaceTypes.end()) {
          *modeId = mode.id;
          return true;
        }
      }
    }
  }
  return false;
}

WifiHal::WifiHal() : mCapabilities(0) {}

WifiHal* WifiHal::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sWifiHal) {
    sWifiHal = new WifiHal();
    ClearOnShutdown(&sWifiHal);
  }
  return sWifiHal;
}

bool WifiHal::CheckWifiStarted() {
  if (mWifi == nullptr) {
    return false;
  } else {
    bool isStarted = false;
    mWifi->isStarted(&isStarted);
    return isStarted;
  }
}

Result_t WifiHal::StartWifiModule() {
  // initialize wifi hal interface at first.
  InitWifiInterface();

  if (mWifi == nullptr) {
    WIFI_LOGE(LOG_TAG, "startWifi: mWifi is null");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  int32_t triedCount = 0;
  Status status;
  WIFI_LOGD(LOG_TAG, "start wifi");
  while (triedCount <= START_HAL_RETRY_TIMES) {
    status = mWifi->start();
    if (status.isOk()) {
      if (triedCount != 0) {
        WIFI_LOGD(LOG_TAG, "start IWifi succeeded after trying %d times",
                  triedCount);
      }
      return nsIWifiResult::SUCCESS;
    } else {
      WIFI_LOGD(LOG_TAG, "Cannot start IWifi: Retrying...");
      usleep(300);
      triedCount++;
    }
  }
  WIFI_LOGE(LOG_TAG, "Cannot start IWifi after trying %d times", triedCount);
  return nsIWifiResult::ERROR_COMMAND_FAILED;
}

Result_t WifiHal::StopWifiModule() {
  if (mWifi == nullptr) {
    WIFI_LOGE(LOG_TAG, "stopWifi: mWifi is null");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  Status status;
  status = mWifi->stop();
  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::TearDownInterface(const IfaceConcurrencyType& aType) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = RemoveInterfaceInternal(aType);
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  result = StopWifiModule();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  mWifi = nullptr;
  return nsIWifiResult::SUCCESS;
}

Result_t WifiHal::InitWifiInterface() {
  MutexAutoLock lock(sLock);
  if (mWifi != nullptr) {
    return nsIWifiResult::SUCCESS;
  }

  mWifi = android::waitForVintfService<IWifi>();
  if (mWifi != nullptr) {
    // wifi hal just initialized, stop wifi in case driver is loaded.
    StopWifiModule();
  } else {
    WIFI_LOGE(LOG_TAG, "get wifi hal failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiHal::GetSupportedFeatures(uint32_t& aSupportedFeatures) {
  if (!CheckWifiStarted()) {
    // should not get capabilities while Wi-Fi is stopped
    aSupportedFeatures = 0;
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  aSupportedFeatures = mCapabilities;
  return nsIWifiResult::SUCCESS;
}

Result_t WifiHal::GetDriverModuleInfo(nsAString& aDriverVersion,
                                      nsAString& aFirmwareVersion) {
  if (!mWifiChip.get()) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  Status status;
  IWifiChip::ChipDebugInfo chipInfo = {};
  status = mWifiChip->requestChipDebugInfo(&chipInfo);

  if (status.isOk()) {
    nsString driverInfo(NS_ConvertUTF8toUTF16(
        ::android::String8(chipInfo.driverDescription).string()));
    nsString firmwareInfo(NS_ConvertUTF8toUTF16(
        ::android::String8(chipInfo.firmwareDescription).string()));

    aDriverVersion.Assign(driverInfo);
    aFirmwareVersion.Assign(firmwareInfo);
  } else {
    WIFI_LOGE(LOG_TAG, "requestChipDebugInfo failed");
  }
  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::SetLowLatencyMode(bool aEnable) {
  return nsIWifiResult::ERROR_NOT_SUPPORTED;
}

Result_t WifiHal::ConfigChipAndCreateIface(const IfaceConcurrencyType& aType,
                                           std::string& aIfaceName /* out */) {
  if (mWifi == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  Status status;
  std::vector<int> chipIds = {};

  status = mWifi->getChipIds(&chipIds);
  if (!status.isOk() || chipIds.size() == 0) {
    WIFI_LOGE(LOG_TAG, "Failed to get wifi chip ids");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  android::sp<IWifiChip> chip;
  status = mWifi->getChip(chipIds[0], &chip);
  if (!status.isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to get wifi chip");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  mWifiChip = chip;

  ConfigChipByType(mWifiChip, aType);

  if (aType == IfaceConcurrencyType::STA) {
    android::sp<IWifiStaIface> iface;
    status = mWifiChip->createStaIface(&iface);
    mStaIface = iface;
    android::String16 ifaceName;
    iface->getName(&ifaceName);
    aIfaceName = std::string(::android::String8(ifaceName).string());
  } else if (aType == IfaceConcurrencyType::P2P) {
    android::sp<IWifiP2pIface> iface;
    status = mWifiChip->createP2pIface(&iface);
    mP2pIface = iface;
    android::String16 ifaceName;
    iface->getName(&ifaceName);
    aIfaceName = std::string(::android::String8(ifaceName).string());
  } else if (aType == IfaceConcurrencyType::AP) {
    android::sp<IWifiApIface> iface;
    status = mWifiChip->createApIface(&iface);
    mApIface = iface;
    android::String16 ifaceName;
    iface->getName(&ifaceName);
    aIfaceName = std::string(::android::String8(ifaceName).string());
  } else {
    WIFI_LOGE(LOG_TAG, "invalid interface type %d", aType);
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }

  if (!status.isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to create interface, type:%d", aType);
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  // interface is ready, update module capabilities
  GetVendorCapabilities();

  mIfaceNameMap[aType] = aIfaceName;
  WIFI_LOGD(LOG_TAG, "chip configure completed");
  return nsIWifiResult::SUCCESS;
}

Result_t WifiHal::EnableLinkLayerStats() {
  if (!mStaIface.get()) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  bool debugEnabled = false;

  if ((mCapabilities & nsIWifiResult::FEATURE_LINK_LAYER_STATS) == 0) {
    WIFI_LOGE(LOG_TAG, "FEATURE_LINK_LAYER_STATS is not supported");
    return nsIWifiResult::ERROR_NOT_SUPPORTED;
  }

  Status status;
  status = mStaIface->enableLinkLayerStatsCollection(debugEnabled);

  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::GetLinkLayerStats(StaLinkLayerStats& aStats) {
  if (!mStaIface.get()) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  if ((mCapabilities & nsIWifiResult::FEATURE_LINK_LAYER_STATS) == 0) {
    WIFI_LOGE(LOG_TAG, "FEATURE_LINK_LAYER_STATS is not supported");
    return nsIWifiResult::ERROR_NOT_SUPPORTED;
  }

  Status status;
  StaLinkLayerStats link_layer_stats = {};
  status = mStaIface->getLinkLayerStats(&link_layer_stats);

  aStats = link_layer_stats;

  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::SetSoftapCountryCode(std::string aCountryCode) {
  if (aCountryCode.length() != 2) {
    WIFI_LOGE(LOG_TAG, "Invalid country code: %s", aCountryCode.c_str());
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }
  std::array<uint8_t, 2> countryCode;
  countryCode[0] = aCountryCode.at(0);
  countryCode[1] = aCountryCode.at(1);

  Status status;
  status = mApIface->setCountryCode(countryCode);

  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::SetFirmwareRoaming(bool aEnable) {
  if (mStaIface == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  Status status;
  StaRoamingState state =
      aEnable ? StaRoamingState::ENABLED : StaRoamingState::DISABLED;
  status = mStaIface->setRoamingState(state);
  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::ConfigureFirmwareRoaming(
    RoamingConfigurationOptions* mRoamingConfig) {
  // make sure firmware roaming is supported
  if ((mCapabilities & nsIWifiResult::FEATURE_CONTROL_ROAMING) == 0) {
    WIFI_LOGE(LOG_TAG, "Firmware roaming is not supported");
    return nsIWifiResult::ERROR_NOT_SUPPORTED;
  }

  if (mStaIface == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  // check firmware roaming capabilities
  Status status;
  StaRoamingCapabilities roamingCaps;
  status = mStaIface->getRoamingCapabilities(&roamingCaps);

  if (!status.isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to get roaming capabilities");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  // set the block and allow list to firmware
  StaRoamingConfig roamingConfig;
  size_t blockListSize = 0;
  size_t allowListSize = 0;
  std::vector<MacAddress> bssidBlockList;
  std::vector<Ssid> ssidAllowList;

  if (!mRoamingConfig->mBssidDenylist.IsEmpty()) {
    for (auto& item : mRoamingConfig->mBssidDenylist) {
      if (blockListSize++ > roamingCaps.maxBlocklistSize) {
        break;
      }
      std::string bssidStr = NS_ConvertUTF16toUTF8(item).get();
      std::array<uint8_t, 6> bssid;
      ConvertMacToByteArray(bssidStr, bssid);
      MacAddress blockListEntry;
      blockListEntry.data = bssid;
      bssidBlockList.push_back(blockListEntry);
    }
  }

  if (!mRoamingConfig->mSsidAllowlist.IsEmpty()) {
    for (auto& item : mRoamingConfig->mSsidAllowlist) {
      if (allowListSize++ > roamingCaps.maxAllowlistSize) {
        break;
      }
      std::string ssidStr = NS_ConvertUTF16toUTF8(item).get();
      Dequote(ssidStr);
      std::array<uint8_t, 32> ssid;
      for (size_t i = 0; i < ssid.size(); i++) {
        ssid[i] = ssidStr.at(i);
      }
      Ssid allowListEntry = {};
      allowListEntry.data = ssid;
      ssidAllowList.push_back(allowListEntry);
    }
  }

  roamingConfig.bssidBlocklist = bssidBlockList;
  roamingConfig.ssidAllowlist = ssidAllowList;

  status = mStaIface->configureRoaming(roamingConfig);
  return CHECK_SUCCESS(status.isOk());
}

std::string WifiHal::GetInterfaceName(const IfaceConcurrencyType& aType) {
  return mIfaceNameMap.at(aType);
}

Result_t WifiHal::ConfigChipByType(const android::sp<IWifiChip>& aChip,
                                   const IfaceConcurrencyType& aType) {
  Status status;
  std::vector<IWifiChip::ChipMode> chipModes;
  int modeId;
  status = aChip->getAvailableModes(&chipModes);
  if (!status.isOk()) {
    WIFI_LOGE(LOG_TAG, "getAvailableModes failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  if (!findAnyModeSupportingConcurrencyType(aType, chipModes, &modeId)) {
    WIFI_LOGE(LOG_TAG, "No mode supporting %d", aType);
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  if (!aChip->configureChip(modeId).isOk()) {
    WIFI_LOGE(LOG_TAG, "configureChip for %d failed", aType);
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiHal::RemoveInterfaceInternal(const IfaceConcurrencyType& aType) {
  Status status;
  std::string ifname = mIfaceNameMap.at(aType);
  android::String16 ifname16(ifname.c_str(), ifname.length());

  switch (aType) {
    case IfaceConcurrencyType::STA:
      status = mWifiChip->removeStaIface(ifname16);
      mStaIface = nullptr;
      break;
    case IfaceConcurrencyType::P2P:
      status = mWifiChip->removeP2pIface(ifname16);
      mP2pIface = nullptr;
      break;
    case IfaceConcurrencyType::AP:
      status = mWifiChip->removeApIface(ifname16);
      mApIface = nullptr;
      break;
    default:
      WIFI_LOGE(LOG_TAG, "Invalid interface type");
      return nsIWifiResult::ERROR_INVALID_ARGS;
  }
  return CHECK_SUCCESS(status.isOk());
}

Result_t WifiHal::GetVendorCapabilities() {
  Status status;
  int32_t chipFeatures = 0;
  int32_t staFeatures = 0;
  if (mWifiChip != nullptr) {
    status = mWifiChip->getFeatureSet(&chipFeatures);
    if (mStaIface != nullptr) {
      status = mStaIface->getFeatureSet(&staFeatures);
    }
  }

  WIFI_LOGD(LOG_TAG, "get supported features [%x][%x]", chipFeatures,
            staFeatures);
  // merge capabilities mask
  mCapabilities = staFeatures | (chipFeatures << 15);
  return CHECK_SUCCESS(status.isOk());
}
