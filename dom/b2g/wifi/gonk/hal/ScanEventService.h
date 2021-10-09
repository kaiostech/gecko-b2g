/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScanEventService_H
#define ScanEventService_H

#include <android/net/wifi/BnScanEvent.h>
#include <android/net/wifi/BnPnoScanEvent.h>
#include <binder/BinderService.h>

BEGIN_WIFI_NAMESPACE

class ScanEventService final
    : virtual public android::BinderService<ScanEventService>,
      virtual public android::net::wifi::BnScanEvent {
  friend android::BinderService<ScanEventService>;

 public:
  explicit ScanEventService() : android::net::wifi::BnScanEvent() {}
  virtual ~ScanEventService() = default;

  static android::sp<ScanEventService> CreateService(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback);
  static char const* getServiceName() { return "wificond.scan.event"; }

  void Cleanup() { sScanEvent = nullptr; }

  /* IScanEvent */
  android::binder::Status OnScanResultReady() override;
  android::binder::Status OnScanFailed() override;

 private:
  static android::sp<ScanEventService> sScanEvent;
  static mozilla::Mutex sLock;
};

class PnoScanEventService final
    : virtual public android::BinderService<PnoScanEventService>,
      virtual public android::net::wifi::BnPnoScanEvent {
  friend android::BinderService<PnoScanEventService>;

 public:
  explicit PnoScanEventService() : android::net::wifi::BnPnoScanEvent() {}
  virtual ~PnoScanEventService() = default;

  static android::sp<PnoScanEventService> CreateService(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback);
  static char const* getServiceName() { return "wificond.pno.event"; }

  void Cleanup() { sPnoScanEvent = nullptr; }

  /* IPnoScanEvent */
  android::binder::Status OnPnoNetworkFound() override;
  android::binder::Status OnPnoScanFailed() override;
  android::binder::Status OnPnoScanOverOffloadStarted() override;
  android::binder::Status OnPnoScanOverOffloadFailed(int32_t reason) override;

 private:
  static android::sp<PnoScanEventService> sPnoScanEvent;
  static mozilla::Mutex sLock;
};

END_WIFI_NAMESPACE

#endif  // ScanEventService_H
