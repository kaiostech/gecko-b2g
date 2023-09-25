/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WificondControl_H
#define WificondControl_H

#include "mozilla/Mutex.h"
#include "ScanEventService.h"
#include "SoftapEventService.h"

#include <android-base/macros.h>

#if ANDROID_VERSION >= 30
#  include <android/net/wifi/nl80211/IWificond.h>
#  include <android/net/wifi/nl80211/IWifiScannerImpl.h>

using ::android::net::wifi::nl80211::IApInterface;
using ::android::net::wifi::nl80211::IClientInterface;
using ::android::net::wifi::nl80211::IWificond;
using ::android::net::wifi::nl80211::IWifiScannerImpl;

namespace Wificond = ::android::net::wifi::nl80211;

#else

#  include <android/net/wifi/IWificond.h>
#  include <android/net/wifi/IWifiScannerImpl.h>

using ::android::net::wifi::IApInterface;
using ::android::net::wifi::IClientInterface;
using ::android::net::wifi::IWificond;
using ::android::net::wifi::IWifiScannerImpl;

namespace Wificond = ::com::android::server::wifi::wificond;
#endif

BEGIN_WIFI_NAMESPACE

class WificondControl : virtual public android::RefBase {
 public:
  WificondControl();

  static WificondControl* Get();

  Result_t InitWificondInterface();
  Result_t StartWificond();
  Result_t StopWificond();
  Result_t TearDownClientInterface(const std::string& aIfaceName);
  Result_t TearDownSoftapInterface(const std::string& aIfaceName);
  Result_t TearDownInterfaces();

  Result_t StartSupplicant();
  Result_t StopSupplicant();

  Result_t SetupClientIface(const std::string& aIfaceName,
                            const android::sp<WifiEventCallback>& aCallback);
  Result_t SetupApIface(const std::string& aIfaceName,
                        const android::sp<WifiEventCallback>& aCallback);
  Result_t StartSoftap(ConfigurationOptions* aConfig);
  Result_t StartSingleScan(ScanSettingsOptions* aScanSettings);
  Result_t StopSingleScan();
  Result_t StartPnoScan(PnoScanSettingsOptions* aPnoScanSettings);
  Result_t StopPnoScan();
  Result_t GetSingleScanResults(
      std::vector<Wificond::NativeScanResult>& aScanResults);
  Result_t GetPnoScanResults(
      std::vector<Wificond::NativeScanResult>& aPnoScanResults);
  Result_t GetChannelsForBand(uint32_t aBandMask,
                              std::vector<int32_t>& aChannels);

  Result_t SignalPoll(std::vector<int32_t>& aPollResult);
  Result_t GetSoftapStations(uint32_t& aNumStations);

  virtual ~WificondControl() {}

 private:
  class WificondDeathRecipient : public android::IBinder::DeathRecipient {
   public:
    explicit WificondDeathRecipient() {}
    virtual ~WificondDeathRecipient() {}

    virtual void binderDied(const android::wp<android::IBinder>& who);
  };

  Result_t CleanupScanEvent();
  Result_t InitiateScanEvent(const std::string& aIfaceName,
                             const android::sp<WifiEventCallback>& aCallback);

  android::sp<IWificond> mWificond;
  android::sp<IClientInterface> mClientInterface;
  android::sp<IApInterface> mApInterface;
  android::sp<IWifiScannerImpl> mScanner;
  android::sp<WificondDeathRecipient> mWificondDeathRecipient;

  // Services for event callback
  android::sp<ScanEventService> mScanEventService;
  android::sp<PnoScanEventService> mPnoScanEventService;
  android::sp<SoftapEventService> mSoftapEventService;

  DISALLOW_COPY_AND_ASSIGN(WificondControl);
};

END_WIFI_NAMESPACE

#endif  // WificondControl_H
