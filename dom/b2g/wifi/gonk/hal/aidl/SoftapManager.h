/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SoftapManager_H
#define SoftapManager_H

#include "WifiCommon.h"
#include "SoftapManager.h"

#include <android/hardware/wifi/hostapd/BnHostapdCallback.h>
#include <android/hardware/wifi/hostapd/ChannelParams.h>
#include <android/hardware/wifi/hostapd/IHostapd.h>
#include "mozilla/Mutex.h"

using ::android::hardware::wifi::hostapd::BnHostapdCallback;
using ::android::hardware::wifi::hostapd::IHostapd;

BEGIN_WIFI_NAMESPACE

/**
 * Class for softap management and hostpad AIDL client implementation.
 */
class SoftapManager : public BnHostapdCallback {
 public:
  static SoftapManager* Get();
  static void CleanUp();

  // AIDL initialization
  Result_t InitInterface();
  Result_t DeinitInterface();
  bool IsInterfaceReady();

  Result_t StartSoftap(const std::string& aInterfaceName,
                       const std::string& aCountryCode,
                       SoftapConfigurationOptions* aSoftapConfig);
  Result_t StopSoftap(const std::string& aInterfaceName);

  virtual ~SoftapManager() {}

  //......................... BnHostapdCallback .........................../
  ::android::binder::Status onApInstanceInfoChanged(
      const ::android::hardware::wifi::hostapd::ApInfo& apInfo) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }

  ::android::binder::Status onConnectedClientsChanged(
      const ::android::hardware::wifi::hostapd::ClientInfo& clientInfo)
      override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }

  ::android::binder::Status onFailure(
      const ::android::String16& ifaceName,
      const ::android::String16& instanceName) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }

  SoftapManager();
  Result_t InitHostapdInterface();
  Result_t TearDownInterface();

  static mozilla::Mutex sLock;

  android::sp<IHostapd> mHostapd;
  std::string mCountryCode;

  DISALLOW_COPY_AND_ASSIGN(SoftapManager);
};

END_WIFI_NAMESPACE

#endif  // SoftapManager_H
