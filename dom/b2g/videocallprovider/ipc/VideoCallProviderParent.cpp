/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/videocallprovider/VideoCallProviderParent.h"

#include "nsServiceManagerUtils.h"
#include "nsITelephonyService.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using namespace mozilla::dom::videocallprovider;

#include <android/log.h>
#undef LOG
#define LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "VideoCallProviderParent", ##args)

/**
 * VideoCallProviderParent
 */
VideoCallProviderParent::VideoCallProviderParent(uint32_t aClientId,
                                                 uint32_t aCallIndex)
    : mClientId(aClientId), mCallIndex(aCallIndex) {
  LOG("constructor, aClientId; %d, aCallIndex: %d", aClientId, aCallIndex);
  MOZ_COUNT_CTOR(VideoCallProviderParent);

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ASSERTION(service, "This shouldn't fail!");

  nsresult rv = service->GetVideoCallProvider(aClientId, aCallIndex,
                                              getter_AddRefs(mProvider));
  if (NS_SUCCEEDED(rv) && mProvider) {
    mProvider->RegisterCallback(this);
  }
}

void VideoCallProviderParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG("deconstructor");
  if (mProvider) {
    mProvider->UnregisterCallback(this);
    mProvider = nullptr;
  }
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSetCamera(
    const int16_t& aCameraId) {
  LOG("RecvSetCamera: %d", aCameraId);

  if (mProvider) {
    mProvider->SetCamera(aCameraId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSetPreviewSurface(
    const uint16_t& aWidth, const uint16_t& aHeight) {
  LOG("RecvSetPreviewSurface aWidth: %d, aHeight: %d", aWidth, aHeight);
  // set preview surface via mHandler
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSetDisplaySurface(
    const uint16_t& aWidth, const uint16_t& aHeight) {
  LOG("RecvSetDisplaySurface aWidth: %d, aHeight: %d", aWidth, aHeight);
  // set display surface via mHandler
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSetDeviceOrientation(
    const uint16_t& aOrientation) {
  LOG("RecvSetDeviceOrientation: %d", aOrientation);
  if (mProvider) {
    mProvider->SetDeviceOrientation(aOrientation);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSetZoom(
    const float& aValue) {
  LOG("RecvSetZoom: %f", aValue);
  if (mProvider) {
    mProvider->SetZoom(aValue);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSendSessionModifyRequest(
    const nsVideoCallProfile& aFromProfile,
    const nsVideoCallProfile& aToProfile) {
  uint16_t fromState;
  uint16_t fromQuality;
  aFromProfile->GetState(&fromState);
  aFromProfile->GetQuality(&fromQuality);

  uint16_t toState;
  uint16_t toQuality;
  aToProfile->GetState(&toState);
  aToProfile->GetQuality(&toQuality);

  LOG("RecvSendSessionModifyRequest, from (quality: %d, state: %d) to "
      "{quality: %d, state: %d}",
      fromQuality, fromState, toQuality, toState);
  if (mProvider) {
    mProvider->SendSessionModifyRequest(aFromProfile, aToProfile);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoCallProviderParent::RecvSendSessionModifyResponse(
    const nsVideoCallProfile& aResponse) {
  uint16_t state;
  uint16_t quality;
  aResponse->GetState(&state);
  aResponse->GetQuality(&quality);

  LOG("RecvSendSessionModifyResponse, {quality: %d, state: %d}", quality,
      state);
  if (mProvider) {
    mProvider->SendSessionModifyResponse(aResponse);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoCallProviderParent::RecvRequestCameraCapabilities() {
  LOG("RecvRequestCameraCapabilities");
  if (mProvider) {
    mProvider->RequestCameraCapabilities();
  }
  return IPC_OK();
}

// nsIVideoCallCallback
NS_IMPL_ISUPPORTS(VideoCallProviderParent, nsIVideoCallCallback)

NS_IMETHODIMP
VideoCallProviderParent::OnReceiveSessionModifyRequest(
    nsIVideoCallProfile* aRequest) {
  LOG("OnReceiveSessionModifyRequest");
  return SendNotifyReceiveSessionModifyRequest(aRequest) ? NS_OK
                                                         : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VideoCallProviderParent::OnReceiveSessionModifyResponse(
    uint16_t status, nsIVideoCallProfile* aRequest,
    nsIVideoCallProfile* aResponse) {
  LOG("OnReceiveSessionModifyResponse");
  return SendNotifyReceiveSessionModifyResponse(status, aRequest, aResponse)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VideoCallProviderParent::OnHandleCallSessionEvent(int16_t aEvent) {
  LOG("OnHandleCallSessionEvent");
  return SendNotifyHandleCallSessionEvent(aEvent) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VideoCallProviderParent::OnChangePeerDimensions(uint16_t aWidth,
                                                uint16_t aHeight) {
  LOG("OnChangePeerDimensions");
  return SendNotifyChangePeerDimensions(aWidth, aHeight) ? NS_OK
                                                         : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VideoCallProviderParent::OnChangeCameraCapabilities(
    nsIVideoCallCameraCapabilities* aCapabilities) {
  LOG("OnChangeCameraCapabilities");
  return SendNotifyChangeCameraCapabilities(aCapabilities) ? NS_OK
                                                           : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VideoCallProviderParent::OnChangeVideoQuality(uint16_t aQuality) {
  LOG("OnChangeVideoQuality");
  return SendNotifyChangeVideoQuality(aQuality) ? NS_OK : NS_ERROR_FAILURE;
}
