/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "ScanEventService"

#include "WifiCommon.h"
#include "ScanEventService.h"

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#include <utils/String16.h>

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::interface_cast;
using ::android::String16;

using namespace mozilla::dom::wifi;

static std::string gInterfaceName;
static android::sp<WifiEventCallback> gCallback;

/* event name */
#define EVENT_SCAN_RESULT_READY u"SCAN_RESULT_READY"_ns
#define EVENT_SCAN_RESULT_FAILED u"SCAN_RESULT_FAILED"_ns
#define EVENT_PNO_SCAN_FOUND u"PNO_SCAN_FOUND"_ns
#define EVENT_PNO_SCAN_FAILED u"PNO_SCAN_FAILED"_ns

/**
 * ScanEventService
 */
mozilla::Mutex ScanEventService::sLock("scan_lock");
android::sp<ScanEventService> ScanEventService::sScanEvent = nullptr;

android::sp<ScanEventService> ScanEventService::CreateService(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  if (sScanEvent) {
    return sScanEvent;
  }

  gInterfaceName = aInterfaceName;
  if (aCallback.get()) {
    gCallback = aCallback;
  }

  // Create new instance
  sScanEvent = new ScanEventService();

  if (BinderService<ScanEventService>::publish() != android::OK) {
    WIFI_LOGE(LOG_TAG, "Failed to add IScanEvent service");
    sScanEvent = nullptr;
    return nullptr;
  }
  android::ProcessState::self()->startThreadPool();
  android::ProcessState::self()->giveThreadPoolName();
  return sScanEvent;
}

/**
 * Implement IScanEvent
 */
android::binder::Status ScanEventService::OnScanResultReady() {
  MutexAutoLock lock(sLock);

  nsCString iface(gInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SCAN_RESULT_READY);

  INVOKE_CALLBACK(gCallback, event, iface);
  return android::binder::Status::ok();
}

android::binder::Status ScanEventService::OnScanFailed() {
  MutexAutoLock lock(sLock);

  nsCString iface(gInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SCAN_RESULT_FAILED);

  INVOKE_CALLBACK(gCallback, event, iface);
  return android::binder::Status::ok();
}

/**
 * PnoScanEventService
 */
mozilla::Mutex PnoScanEventService::sLock("pno_lock");
android::sp<PnoScanEventService> PnoScanEventService::sPnoScanEvent = nullptr;

android::sp<PnoScanEventService> PnoScanEventService::CreateService(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  if (sPnoScanEvent) {
    return sPnoScanEvent;
  }

  gInterfaceName = aInterfaceName;
  if (aCallback.get()) {
    gCallback = aCallback;
  }

  // Create new instance
  sPnoScanEvent = new PnoScanEventService();

  if (BinderService<PnoScanEventService>::publish() != android::OK) {
    WIFI_LOGE(LOG_TAG, "Failed to add IPnoScanEvent service");
    sPnoScanEvent = nullptr;
    return nullptr;
  }
  android::ProcessState::self()->startThreadPool();
  android::ProcessState::self()->giveThreadPoolName();
  return sPnoScanEvent;
}

/**
 * Implement IPnoScanEvent
 */
android::binder::Status PnoScanEventService::OnPnoNetworkFound() {
  MutexAutoLock lock(sLock);

  nsCString iface(gInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_PNO_SCAN_FOUND);

  INVOKE_CALLBACK(gCallback, event, iface);
  return android::binder::Status::ok();
}

android::binder::Status PnoScanEventService::OnPnoScanFailed() {
  MutexAutoLock lock(sLock);

  nsCString iface(gInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_PNO_SCAN_FAILED);

  INVOKE_CALLBACK(gCallback, event, iface);
  return android::binder::Status::ok();
}

#if ANDROID_VERSION <= 29
android::binder::Status PnoScanEventService::OnPnoScanOverOffloadStarted() {
  return android::binder::Status::ok();
}

android::binder::Status PnoScanEventService::OnPnoScanOverOffloadFailed(
    int32_t reason) {
  return android::binder::Status::ok();
}
#endif
