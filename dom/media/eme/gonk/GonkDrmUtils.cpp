/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmUtils.h"

#include "mozilla/Base64.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsComponentManagerUtils.h"
#include "nsIGonkDrmNetUtils.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include <mediadrm/ICrypto.h>
#include <mediadrm/IDrm.h>

#if ANDROID_VERSION >= 30
#  include <mediadrm/CryptoHal.h>
#  include <mediadrm/DrmHal.h>
#else
#  include <binder/IServiceManager.h>
#  include <mediadrm/IMediaDrmService.h>
#endif

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

  virtual ~GonkDrmProvisioningCallback() = default;

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
#if ANDROID_VERSION >= 30
  sp<IDrm> drm = new DrmHal();
#else
  sp<IServiceManager> sm = defaultServiceManager();
  sp<IBinder> binder = sm->getService(String16("media.drm"));
  sp<IMediaDrmService> service = interface_cast<IMediaDrmService>(binder);
  if (!service) {
    GD_LOGE("GonkDrmUtils::MakeDrm, unable to get media.drm service");
    return nullptr;
  }

  sp<IDrm> drm = service->makeDrm();
#endif

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

sp<ICrypto> GonkDrmUtils::MakeCrypto() {
#if ANDROID_VERSION >= 30
  sp<ICrypto> crypto = new CryptoHal();
#else
  sp<IServiceManager> sm = defaultServiceManager();
  sp<IBinder> binder = sm->getService(String16("media.drm"));
  sp<IMediaDrmService> service = interface_cast<IMediaDrmService>(binder);
  if (!service) {
    GD_LOGE("GonkDrmUtils::MakeCrypto, unable to get media.drm service");
    return nullptr;
  }

  sp<ICrypto> crypto = service->makeCrypto();
#endif

  if (!crypto ||
      (crypto->initCheck() != OK && crypto->initCheck() != NO_INIT)) {
    GD_LOGE("GonkDrmUtils::MakeCrypto, unable to create Crypto");
    return nullptr;
  }
  return crypto;
}

sp<ICrypto> GonkDrmUtils::MakeCrypto(const nsAString& aKeySystem,
                                     const Vector<uint8_t>& aSessionId) {
  sp<ICrypto> crypto = MakeCrypto();
  if (!crypto) {
    return nullptr;
  }

  auto err = crypto->createPlugin(GetKeySystemUUID(aKeySystem),
                                  aSessionId.array(), aSessionId.size());
  if (err != OK) {
    GD_LOGE("GonkDrmUtils::MakeCrypto, unable to create plugin");
    return nullptr;
  }
  return crypto;
}

bool GonkDrmUtils::IsSchemeSupported(const nsAString& aKeySystem) {
  if (!mozilla::StaticPrefs::media_b2g_mediadrm_enabled()) {
    return false;
  }

#if ANDROID_VERSION < 30
  // If mediadrmserver is disabled, MakeDrm() will be blocked for 5 seconds, so
  // a non-blocking service check here is needed.
  sp<IServiceManager> sm = defaultServiceManager();
  sp<IBinder> binder = sm->checkService(String16("media.drm"));
  if (!binder) {
    GD_LOGE("GonkDrmUtils::IsSchemeSupported, unable to get media.drm service");
    return false;
  }
#endif

  sp<IDrm> drm = MakeDrm();
  if (!drm) {
    GD_LOGE("GonkDrmUtils::IsSchemeSupported, MakeDrm failed");
    return false;
  }

  bool supported = false;
  auto err = drm->isCryptoSchemeSupported(
      GetKeySystemUUID(aKeySystem), String8(), DrmPlugin::kSecurityLevelUnknown,
      &supported);
  if (err != OK) {
    GD_LOGE(
        "GonkDrmUtils::IsSchemeSupported, DRM isCryptoSchemeSupported failed");
    return false;
  }

  return supported;
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

nsTArray<uint8_t> GonkDrmUtils::ReadNsByteArrayFromParcel(
    const Parcel* aParcel) {
  GD_ASSERT(aParcel);
  nsTArray<uint8_t> array;
  auto len = aParcel->readInt32();
  if (len > 0) {
    auto* data = static_cast<const uint8_t*>(aParcel->readInplace(len));
    array.AppendElements(data, len);
  }
  return array;
}

nsCString GonkDrmUtils::EncodeBase64(const Vector<uint8_t>& aVector) {
  nsCString base64;
  nsresult rv = mozilla::Base64Encode(
      reinterpret_cast<const char*>(aVector.array()), aVector.size(), base64);
  if (NS_FAILED(rv)) {
    GD_LOGE("EncodeBase64 failed");
    return nsCString();
  }
  return base64;
}

Vector<uint8_t> GonkDrmUtils::DecodeBase64(const nsACString& aBase64) {
  nsCString binary;
  nsresult rv = mozilla::Base64Decode(aBase64, binary);
  if (NS_FAILED(rv)) {
    GD_LOGE("DecodeBase64 failed");
    return Vector<uint8_t>();
  }
  return GonkDrmConverter::ToByteVector(binary);
}

Vector<uint8_t> GonkDrmUtils::DecodeBase64URL(const nsACString& aBase64) {
  FallibleTArray<uint8_t> binary;
  nsresult rv = mozilla::Base64URLDecode(
      aBase64, mozilla::Base64URLDecodePaddingPolicy::Reject, binary);
  if (NS_FAILED(rv)) {
    GD_LOGE("Base64URLDecode failed");
    return Vector<uint8_t>();
  }
  return GonkDrmConverter::ToByteVector(binary);
}

nsCString GonkDrmUtils::GenerateUUID() {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_FAILED(rv)) {
    GD_LOGE("GonkDrmUtils::GenerateUUID, failed to get nsIUUIDGenerator");
    return nsCString();
  }

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  if (NS_FAILED(rv)) {
    GD_LOGE("GonkDrmUtils::GenerateUUID, failed to generate UUID");
    return nsCString();
  }

  char buffer[NSID_LENGTH];
  id.ToProvidedString(buffer);
  nsCString uuid(buffer);
  uuid.StripChars("{}");
  return uuid;
}

}  // namespace android
