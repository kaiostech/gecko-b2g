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

using namespace mozilla::dom::wifi;

#define EVENT_HOTSPOT_CLIENT_CHANGED u"HOTSPOT_CLIENT_CHANGED"_ns

android::sp<SoftapEventService> SoftapEventService::sSoftapEvent = nullptr;

SoftapEventService::SoftapEventService(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback)
    : android::net::wifi::nl80211::BnApInterfaceEventCallback(),
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

android::binder::Status SoftapEventService::onSoftApChannelSwitched(
    int32_t frequency, int32_t bandwidth) {
  WIFI_LOGI(LOG_TAG, "onSoftApChannelSwitched frequency=%d bandwith=%d",
    frequency, bandwidth);
  return android::binder::Status::ok();
}

// TODO: FIXME
android::binder::Status SoftapEventService::onConnectedClientsChanged(
    const ::android::net::wifi::nl80211::NativeWifiClient& client,
          bool isConnected) {
  WIFI_LOGI(LOG_TAG, "onConnectedClientsChanged isConnected=%s",
    isConnected ? "yes" : "no");
  return android::binder::Status::ok();
}
