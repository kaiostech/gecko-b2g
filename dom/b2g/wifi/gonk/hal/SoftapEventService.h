/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SoftapEventService_H
#define SoftapEventService_H

#include <android/net/wifi/nl80211/BnApInterfaceEventCallback.h>
#include <binder/BinderService.h>

BEGIN_WIFI_NAMESPACE

class SoftapEventService
    : virtual public android::BinderService<SoftapEventService>,
      virtual public android::net::wifi::nl80211::BnApInterfaceEventCallback {
 public:
  explicit SoftapEventService(const std::string& aInterfaceName,
                              const android::sp<WifiEventCallback>& aCallback);
  virtual ~SoftapEventService() = default;

  static char const* GetServiceName() { return "softap.event"; }
  static android::sp<SoftapEventService> CreateService(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback);

  void Cleanup() { sSoftapEvent = nullptr; }

 private:
  static android::sp<SoftapEventService> sSoftapEvent;
  std::string mSoftapInterfaceName;

  android::sp<WifiEventCallback> mCallback;

  android::binder::Status onSoftApChannelSwitched(int32_t frequency,
                                                  int32_t bandwidth) override;

  android::binder::Status onConnectedClientsChanged(const ::android::net::wifi::nl80211::NativeWifiClient& client,
                                                    bool isConnected) override;
};

END_WIFI_NAMESPACE

#endif  // SoftapEventService_H
