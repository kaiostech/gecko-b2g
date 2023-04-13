/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SoftapEventService_H
#define SoftapEventService_H

#if ANDROID_VERSION >= 30
#include <android/net/wifi/nl80211/BnApInterfaceEventCallback.h>
#else
#include <android/net/wifi/BnApInterfaceEventCallback.h>
#endif
#include <binder/BinderService.h>

#if ANDROID_VERSION >= 30
namespace Wifi = ::android::net::wifi::nl80211;
#else
namespace Wifi = ::android::net::wifi;
#endif

BEGIN_WIFI_NAMESPACE

class SoftapEventService
    : virtual public android::BinderService<SoftapEventService>,
      virtual public Wifi::BnApInterfaceEventCallback {
 public:
  explicit SoftapEventService(const std::string& aInterfaceName,
                              const android::sp<WifiEventCallback>& aCallback);
  virtual ~SoftapEventService() = default;

  static char const* GetServiceName() { return "softap.event"; }
  static android::sp<SoftapEventService> CreateService(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback);

  void Cleanup() { sSoftapEvent = nullptr; }

#if ANDROID_VERSION <= 29
  // IApInterfaceEventCallback
  android::binder::Status onNumAssociatedStationsChanged(
      int32_t numStations) override;
  android::binder::Status onSoftApChannelSwitched(int32_t frequency,
                                                  int32_t bandwidth) override;
#endif


 private:
  static android::sp<SoftapEventService> sSoftapEvent;
  std::string mSoftapInterfaceName;

  android::sp<WifiEventCallback> mCallback;

#if ANDROID_VERSION >= 30
  android::binder::Status onSoftApChannelSwitched(int32_t frequency,
                                                  int32_t bandwidth) override;

  android::binder::Status onConnectedClientsChanged(const Wifi::NativeWifiClient& client,
                                                    bool isConnected) override;
#endif
};

END_WIFI_NAMESPACE

#endif  // SoftapEventService_H
