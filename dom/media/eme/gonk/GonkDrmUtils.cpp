/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmUtils.h"

#include "mozilla/EMEUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIGonkDrmNetUtils.h"

#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <mediadrm/IDrm.h>
#include <mediadrm/IMediaDrmService.h>

namespace mozilla {

LogModule* GetGonkDrmLog() {
  static LazyLogModule log("GonkDrm");
  return log;
}

class GonkDrmProvisioningCallback : public nsIGonkDrmProvisioningCallback {
  typedef android::GonkDrmUtils::ProvisioningCallback ProvisioningCallback;

  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  static already_AddRefed<GonkDrmProvisioningCallback> Create(
      const ProvisioningCallback& aCallback) {
    nsCOMPtr<nsISerialEventTarget> currentThread =
        GetCurrentSerialEventTarget();
    if (!currentThread) {
      GD_LOGE(
          "GonkDrmProvisioningCallback::Create, failed to get current thread.");
      return nullptr;
    }

    return MakeAndAddRef<GonkDrmProvisioningCallback>(currentThread, aCallback);
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHODIMP
  OnSuccess(const nsACString& aResponse, JSContext* cx) override {
    GD_LOGD("GonkDrmProvisioningCallback::OnSuccess");
    DispatchResponse(true, aResponse);
    return NS_OK;
  }

  NS_IMETHODIMP
  OnError(const nsACString& aMessage, JSContext* cx) override {
    GD_LOGE("GonkDrmProvisioningCallback::OnError, %s", aMessage.Data());
    DispatchResponse(true, ""_ns);
    return NS_OK;
  }

  void DispatchResponse(bool aSuccess, const nsACString& aResponse) {
    mCbThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmProvisioningCallback::DispatchResponse",
        [aSuccess, response = nsCString(aResponse), callback = mCallback]() {
          callback(aSuccess, response);
        }));
  }

 private:
  GonkDrmProvisioningCallback(nsISerialEventTarget* aCbThread,
                              const ProvisioningCallback& aCallback)
      : mCbThread(aCbThread), mCallback(aCallback) {}

  ~GonkDrmProvisioningCallback() = default;

  nsCOMPtr<nsISerialEventTarget> mCbThread;
  ProvisioningCallback mCallback;
};

NS_IMPL_ISUPPORTS(GonkDrmProvisioningCallback, nsIGonkDrmProvisioningCallback)

}  // namespace mozilla

namespace android {

void GonkDrmUtils::StartProvisioning(const nsACString& aUrl,
                                     const nsACString& aRequest,
                                     const ProvisioningCallback& aCallback) {
  using mozilla::GonkDrmProvisioningCallback;

  RefPtr<GonkDrmProvisioningCallback> callback =
      GonkDrmProvisioningCallback::Create(aCallback);
  if (!callback) {
    GD_LOGE("GonkDrmUtils::StartProvisioning, failed to create callback");
    return;
  }

  nsCOMPtr<nsISerialEventTarget> mainThread =
      mozilla::GetMainThreadSerialEventTarget();
  GD_ASSERT(mainThread);

  mainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmUtils::StartProvisioning",
      [url = nsCString(aUrl), request = nsCString(aRequest), callback]() {
        nsresult rv;
        nsCOMPtr<nsIGonkDrmNetUtils> netUtils =
            do_CreateInstance("@mozilla.org/gonkdrm/net-utils;1", &rv);
        if (NS_FAILED(rv) || !netUtils) {
          GD_LOGE("GonkDrmUtils::StartProvisioning, do_CreateInstance failed");
          callback->DispatchResponse(false, ""_ns);
          return;
        }

        GD_LOGD("GonkDrmUtils::StartProvisioning, sending request");
        netUtils->StartProvisioning(url, request, callback);
      }));
}

const uint8_t* GonkDrmUtils::GetKeySystemUUID(const nsAString& aKeySystem) {
  static const uint8_t kClearKeyUUID[16] = {0xE2, 0x71, 0x9D, 0x58, 0xA9, 0x85,
                                            0xB3, 0xC9, 0x78, 0x1A, 0xB0, 0x30,
                                            0xAF, 0x78, 0xD3, 0x0E};

  static const uint8_t kWidevineUUID[16] = {0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6,
                                            0x4A, 0xCE, 0xA3, 0xC8, 0x27, 0xDC,
                                            0xD5, 0x1D, 0x21, 0xED};

  static const uint8_t kEmptyUUID[16] = {0};

  if (mozilla::IsClearkeyKeySystem(aKeySystem)) {
    return kClearKeyUUID;
  } else if (mozilla::IsWidevineKeySystem(aKeySystem)) {
    return kWidevineUUID;
  } else {
    return kEmptyUUID;
  }
}

sp<IDrm> GonkDrmUtils::MakeDrm() {
  sp<IServiceManager> sm = defaultServiceManager();
  sp<IBinder> binder = sm->getService(String16("media.drm"));
  sp<IMediaDrmService> service = interface_cast<IMediaDrmService>(binder);
  if (!service) {
    GD_LOGE("GonkDrmUtils::MakeDrm, unable to get media.drm service");
    return nullptr;
  }

  sp<IDrm> drm = service->makeDrm();
  if (!drm || (drm->initCheck() != OK && drm->initCheck() != NO_INIT)) {
    GD_LOGE("GonkDrmUtils::MakeDrm, unable to create DRM");
    return nullptr;
  }
  return drm;
}

sp<IDrm> GonkDrmUtils::MakeDrm(const nsAString& aKeySystem) {
  sp<IDrm> drm = MakeDrm();
  if (!drm) {
    return nullptr;
  }

  auto err = drm->createPlugin(GetKeySystemUUID(aKeySystem), String8("b2g"));
  if (err != OK) {
    GD_LOGE("GonkDrmUtils::MakeDrm, unable to create plugin");
    return nullptr;
  }
  return drm;
}

Vector<uint8_t> GonkDrmUtils::ReadByteVectorFromParcel(const Parcel* aParcel) {
  GD_ASSERT(aParcel);
  Vector<uint8_t> vector;
  auto len = aParcel->readInt32();
  if (len > 0) {
    auto* data = static_cast<const uint8_t*>(aParcel->readInplace(len));
    vector.appendArray(data, len);
  }
  return vector;
}

}  // namespace android
