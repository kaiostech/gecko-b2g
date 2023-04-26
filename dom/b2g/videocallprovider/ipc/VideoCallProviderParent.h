/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_videocallprovider_VideoCallProviderParent_h__
#define mozilla_dom_videocallprovider_VideoCallProviderParent_h__

#include "mozilla/dom/videocallprovider/PVideoCallProviderParent.h"
#include "nsIVideoCallProvider.h"
#include "nsIVideoCallCallback.h"

namespace mozilla {
namespace dom {
namespace videocallprovider {

class VideoCallProviderParent : public PVideoCallProviderParent,
                                public nsIVideoCallCallback {
  friend class PVideoCallProviderParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIDEOCALLCALLBACK

  explicit VideoCallProviderParent(uint32_t aClientId, uint32_t aCallIndex);

 protected:
  virtual ~VideoCallProviderParent() {
    MOZ_COUNT_DTOR(VideoCallProviderParent);
  }

  void ActorDestroy(ActorDestroyReason aWhy);

  mozilla::ipc::IPCResult RecvSetCamera(const int16_t& cameraId);

  mozilla::ipc::IPCResult RecvSetPreviewSurface(const uint16_t& aWidth,
                                                const uint16_t& aHeight);

  mozilla::ipc::IPCResult RecvSetDisplaySurface(const uint16_t& aWidth,
                                                const uint16_t& aHeight);

  mozilla::ipc::IPCResult RecvSetDeviceOrientation(
      const uint16_t& aOrientation);

  mozilla::ipc::IPCResult RecvSetZoom(const float& aValue);

  mozilla::ipc::IPCResult RecvSendSessionModifyRequest(
      const nsVideoCallProfile& aFromProfile,
      const nsVideoCallProfile& aToProfile);

  mozilla::ipc::IPCResult RecvSendSessionModifyResponse(
      const nsVideoCallProfile& aResponse);

  mozilla::ipc::IPCResult RecvRequestCameraCapabilities();

 private:
  uint32_t mClientId;
  uint32_t mCallIndex;
  nsCOMPtr<nsIVideoCallProvider> mProvider;
};

}  // namespace videocallprovider
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_videocallprovider_VideoCallProviderParent_h__
