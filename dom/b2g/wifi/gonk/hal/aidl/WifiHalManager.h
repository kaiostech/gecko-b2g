/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiHalManager_H
#define WifiHalManager_H

// NAN define conflict
#ifdef NAN
#  undef NAN
#endif

#include "WifiCommon.h"

#include <android/hardware/wifi/IWifi.h>
#include <android/hardware/wifi/BnWifiEventCallback.h>

#include "mozilla/Mutex.h"

#define START_HAL_RETRY_TIMES 3

using ::android::hardware::wifi::IWifi;
using ::android::hardware::wifi::IfaceConcurrencyType;
using ::android::hardware::wifi::IWifiStaIface;
using ::android::hardware::wifi::StaLinkLayerStats;
using ::android::hardware::wifi::IWifiChip;
using ::android::hardware::wifi::IWifiP2pIface;
using ::android::hardware::wifi::IWifiApIface;
using ::android::hardware::wifi::WifiStatusCode;

BEGIN_WIFI_NAMESPACE

class WifiHal {
 public:
  static WifiHal* Get();
  static void CleanUp();

  bool CheckWifiStarted();
  Result_t TearDownInterface(const IfaceConcurrencyType& aType);
  Result_t GetSupportedFeatures(uint32_t& aSupportedFeatures);
  Result_t GetDriverModuleInfo(nsAString& aDriverVersion,
                               nsAString& aFirmwareVersion);
  Result_t SetLowLatencyMode(bool aEnable);

  Result_t StartWifiModule();
  Result_t StopWifiModule();
  Result_t ConfigChipAndCreateIface(const IfaceConcurrencyType& aType,
                                    std::string& aIfaceName);
  Result_t EnableLinkLayerStats();
  Result_t GetLinkLayerStats(StaLinkLayerStats& aStats);
  Result_t SetSoftapCountryCode(std::string aCountryCode);
  Result_t SetFirmwareRoaming(bool aEnable);
  Result_t ConfigureFirmwareRoaming(
      RoamingConfigurationOptions* mRoamingConfig);

  std::string GetInterfaceName(const IfaceConcurrencyType& aType);

  virtual ~WifiHal() {}

 private:

  WifiHal();
  Result_t InitWifiInterface();
  Result_t GetVendorCapabilities();
  Result_t ConfigChipByType(const android::sp<IWifiChip>& aChip,
                            const IfaceConcurrencyType& aType);
  Result_t RemoveInterfaceInternal(const IfaceConcurrencyType& aType);

  static mozilla::Mutex sLock;

  android::sp<IWifi> mWifi;
  android::sp<IWifiChip> mWifiChip;
  android::sp<IWifiStaIface> mStaIface;
  android::sp<IWifiP2pIface> mP2pIface;
  android::sp<IWifiApIface> mApIface;

  std::unordered_map<IfaceConcurrencyType, std::string> mIfaceNameMap;
  uint32_t mCapabilities;

  DISALLOW_COPY_AND_ASSIGN(WifiHal);
};

END_WIFI_NAMESPACE

#endif  // WifiHalManager_H
