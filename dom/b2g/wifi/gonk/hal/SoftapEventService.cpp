/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "SoftapEventService"

#include "WifiCommon.h"
#include "SoftapEventService.h"

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#include <utils/String16.h>

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::String16;

#if ANDROID_VERSION >= 30
namespace Wifi = android::net::wifi::nl80211;
#else
namespace Wifi = android::net::wifi;
#endif

using namespace mozilla::dom::wifi;

#define EVENT_HOTSPOT_CLIENT_CHANGED u"HOTSPOT_CLIENT_CHANGED"_ns

android::sp<SoftapEventService> SoftapEventService::sSoftapEvent = nullptr;

SoftapEventService::SoftapEventService(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback)
    : Wifi::BnApInterfaceEventCallback(),
      mSoftapInterfaceName(aInterfaceName),
      mCallback(aCallback) {}

android::sp<SoftapEventService> SoftapEventService::CreateService(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  if (sSoftapEvent) {
    return sSoftapEvent;
  }
  // Create new instance
  sSoftapEvent = new SoftapEventService(aInterfaceName, aCallback);

  android::sp<::android::ProcessState> ps(::android::ProcessState::self());
  if (android::defaultServiceManager()->addService(
          android::String16(SoftapEventService::GetServiceName()),
          sSoftapEvent) != android::OK) {
    WIFI_LOGE(LOG_TAG, "Failed to add service: %s",
              SoftapEventService::GetServiceName());
    sSoftapEvent = nullptr;
    return nullptr;
  }

  ps->startThreadPool();
  ps->giveThreadPoolName();
  return sSoftapEvent;
}

#if ANDROID_VERSION >= 30
android::binder::Status SoftapEventService::onSoftApChannelSwitched(
    int32_t frequency, int32_t bandwidth) {
  WIFI_LOGI(LOG_TAG, "onSoftApChannelSwitched frequency=%d bandwith=%d",
            frequency, bandwidth);
  return android::binder::Status::ok();
}

android::binder::Status SoftapEventService::onConnectedClientsChanged(
    const Wifi::NativeWifiClient& client, bool isConnected) {
  WIFI_LOGI(LOG_TAG, "onConnectedClientsChanged isConnected=%s",
            isConnected ? "yes" : "no");
  return android::binder::Status::ok();
}

#else
/**
 * Implement IApInterfaceEventCallback
 */
android::binder::Status SoftapEventService::onNumAssociatedStationsChanged(
    int32_t numStations) {
  nsCString iface(mSoftapInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_HOTSPOT_CLIENT_CHANGED);

  if (numStations < 0) {
    WIFI_LOGE(LOG_TAG, "Invalid number of associated stations %d", numStations);
  }
  event->mNumStations = (numStations < 0) ? 0 : numStations;

  INVOKE_CALLBACK(mCallback, event, iface);
  return android::binder::Status::ok();
}

android::binder::Status SoftapEventService::onSoftApChannelSwitched(
    int32_t frequency, int32_t bandwidth) {
  return android::binder::Status::ok();
}
#endif
