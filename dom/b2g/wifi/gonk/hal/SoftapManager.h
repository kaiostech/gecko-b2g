/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SoftapManager_H
#define SoftapManager_H

#include "WifiCommon.h"
#include "SoftapManager.h"

#include <android/hidl/manager/1.0/IServiceManager.h>
#include <android/hidl/manager/1.0/IServiceNotification.h>
#include <android/hardware/wifi/hostapd/1.0/types.h>
#include <android/hardware/wifi/hostapd/1.1/IHostapd.h>
#include <android/hardware/wifi/hostapd/1.1/IHostapdCallback.h>
#include "mozilla/Mutex.h"

using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::wifi::hostapd::V1_0::HostapdStatus;
using ::android::hardware::wifi::hostapd::V1_0::HostapdStatusCode;
using ::android::hardware::wifi::hostapd::V1_0::IHostapd;
using ::android::hidl::manager::V1_0::IServiceNotification;

using IHostapd_1_1 = ::android::hardware::wifi::hostapd::V1_1::IHostapd;

BEGIN_WIFI_NAMESPACE

/**
 * Class for softap management and hostpad HIDL client implementation.
 */
class SoftapManager
    : virtual public android::hidl::manager::V1_0::IServiceNotification,
      virtual public android::hardware::wifi::hostapd::V1_1::IHostapdCallback {
 public:
  static SoftapManager* Get();
  static void CleanUp();

  // HIDL initialization
  Result_t InitInterface();
  Result_t DeinitInterface();
  bool IsInterfaceInitializing();
  bool IsInterfaceReady();

  Result_t StartSoftap(const std::string& aInterfaceName,
                       const std::string& aCountryCode,
                       SoftapConfigurationOptions* aSoftapConfig);
  Result_t StopSoftap(const std::string& aInterfaceName);

  virtual ~SoftapManager() {}

  // IServiceNotification::onRegistration
  virtual Return<void> onRegistration(const hidl_string& fqName,
                                      const hidl_string& name,
                                      bool preexisting) override;

 private:
  //......................... IHostapdCallback .........................../
  /**
   * Invoked when an asynchronous failure is encountered in one of the access
   * points added via |IHostapd.addAccessPoint|.
   *
   * @param ifaceName Name of the interface.
   */
  Return<void> onFailure(const hidl_string& ifaceName) override;

  struct ServiceManagerDeathRecipient : public hidl_death_recipient {
    explicit ServiceManagerDeathRecipient(SoftapManager* aOuter)
        : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    SoftapManager* mOuter;
  };

  struct HostapdDeathRecipient : public hidl_death_recipient {
    explicit HostapdDeathRecipient(SoftapManager* aOuter) : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    SoftapManager* mOuter;
  };

  SoftapManager();
  Result_t InitServiceManager();
  Result_t InitHostapdInterface();
  Result_t TearDownInterface();

  static mozilla::Mutex sLock;

  android::sp<android::hidl::manager::V1_0::IServiceManager> mServiceManager;
  android::sp<IHostapd_1_1> mHostapd;
  android::sp<ServiceManagerDeathRecipient> mServiceManagerDeathRecipient;
  android::sp<HostapdDeathRecipient> mHostapdDeathRecipient;
  std::string mCountryCode;

  DISALLOW_COPY_AND_ASSIGN(SoftapManager);
};

END_WIFI_NAMESPACE

#endif  // SoftapManager_H
