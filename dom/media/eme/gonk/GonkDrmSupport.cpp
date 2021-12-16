/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmSupport.h"

#include "GonkDrmCDMCallbackProxy.h"
#include "GonkDrmSharedData.h"
#include "GonkDrmUtils.h"
#include "mozilla/EMEUtils.h"
#include "nsISerialEventTarget.h"

#include <binder/ProcessState.h>
#include <media/stagefright/MediaErrors.h>
#include <mediadrm/IDrm.h>

namespace android {

using mozilla::dom::MediaKeyStatus;
using mozilla::dom::Optional;

const auto kGonkDrmSecurityLevel = DrmPlugin::kSecurityLevelMax;

enum class GonkDrmKeyStatus : int32_t {
  USABLE = 0,
  EXPIRED = 1,
  OUTPUT_NOT_ALLOWED = 2,
  PENDING = 3,
  INTERNAL_ERROR = 4,
  USABLE_IN_FUTURE = 5
};

static MediaKeyStatus ConvertToMediaKeyStatus(GonkDrmKeyStatus aKeyStatus) {
  switch (aKeyStatus) {
    case GonkDrmKeyStatus::USABLE:
      return MediaKeyStatus::Usable;
    case GonkDrmKeyStatus::EXPIRED:
      return MediaKeyStatus::Expired;
    case GonkDrmKeyStatus::OUTPUT_NOT_ALLOWED:
      return MediaKeyStatus::Output_restricted;
    case GonkDrmKeyStatus::PENDING:
      return MediaKeyStatus::Status_pending;
    case GonkDrmKeyStatus::INTERNAL_ERROR:
    case GonkDrmKeyStatus::USABLE_IN_FUTURE:
    default:
      return MediaKeyStatus::Internal_error;
  }
}

GonkDrmSupport::GonkDrmSupport(nsISerialEventTarget* aOwnerThread,
                               const nsAString& aKeySystem)
    : mOwnerThread(aOwnerThread), mKeySystem(aKeySystem) {}

GonkDrmSupport::~GonkDrmSupport() { GD_ASSERT(!mDrm); }

void GonkDrmSupport::Init(uint32_t aPromiseId,
                          GonkDrmCDMCallbackProxy* aCallback,
                          const sp<GonkDrmSharedData>& aSharedData) {
  GD_ASSERT(aCallback);
  GD_LOGD("%p GonkDrmSupport::Init, %s", this,
          NS_ConvertUTF16toUTF8(mKeySystem).Data());

  ProcessState::self()->startThreadPool();

  mInitPromiseId = aPromiseId;
  mCallback = aCallback;
  mSharedData = aSharedData;
  mDrm = GonkDrmUtils::MakeDrm(mKeySystem);
  if (!mDrm) {
    GD_LOGE("%p GonkDrmSupport::Init, MakeDrm failed", this);
    InitFailed();
    return;
  }

  auto err = mDrm->setListener(this);
  if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::Init, DRM setListener failed(%d)", this, err);
    InitFailed();
    return;
  }

  if (mozilla::IsWidevineKeySystem(mKeySystem)) {
    // Set security level to L3.
    err = mDrm->setPropertyString(String8("securityLevel"), String8("L3"));
    if (err != OK) {
      GD_LOGW("%p GonkDrmSupport::Init, DRM set securityLevel failed(%d)", this,
              err);
    }
    // Enable session sharing.
    err = mDrm->setPropertyString(String8("sessionSharing"), String8("enable"));
    if (err != OK) {
      GD_LOGW("%p GonkDrmSupport::Init, DRM set sessionSharing failed(%d)",
              this, err);
    }
  }

  err = OpenCryptoSession();
  if (err == ERROR_DRM_NOT_PROVISIONED) {
    StartProvisioning();
    return;
  } else if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::Init, OpenCryptoSession failed", this);
    InitFailed();
    return;
  }

  InitCompleted();
}

void GonkDrmSupport::InitFailed() {
  GD_ASSERT(mInitPromiseId);
  GD_ASSERT(mCallback);

  GD_LOGE("%p GonkDrmSupport::InitFailed", this);
  mCallback->RejectPromiseWithStateError(mInitPromiseId, "Init failed"_ns);
  Reset();
}

void GonkDrmSupport::InitCompleted() {
  GD_ASSERT(mInitPromiseId);
  GD_ASSERT(mCallback);

  GD_LOGD("%p GonkDrmSupport::InitCompleted", this);
  mCallback->CDMCreated(mInitPromiseId);
  mInitPromiseId = 0;
}

status_t GonkDrmSupport::OpenCryptoSession() {
  Vector<uint8_t> sessionId;
  auto err = mDrm->openSession(kGonkDrmSecurityLevel, sessionId);
  if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::OpenCryptoSession, DRM openSession failed(%d)",
            this, err);
    return err;
  }
  mSharedData->SetCryptoSessionId(sessionId);
  return OK;
}

void GonkDrmSupport::StartProvisioning() {
  GD_LOGD("%p GonkDrmSupport::StartProvisioning", this);

  Vector<uint8_t> request;
  String8 url;
  auto err =
      mDrm->getProvisionRequest(String8("none"), String8(), request, url);
  if (err != OK) {
    GD_LOGE(
        "%p GonkDrmSupport::StartProvisioning, DRM getProvisionRequest "
        "failed(%d)",
        this, err);
    InitFailed();
    return;
  }

  GonkDrmUtils::StartProvisioning(
      GonkDrmConverter::ToNsCString(url),
      GonkDrmConverter::ToNsCString(request),
      [self = Self()](bool aSuccess, const nsACString& aResponse) {
        self->UpdateProvisioningResponse(aSuccess, aResponse);
      });
}

void GonkDrmSupport::UpdateProvisioningResponse(bool aSuccess,
                                                const nsACString& aResponse) {
  GD_LOGD("%p GonkDrmSupport::UpdateProvisioningResponse %s", this,
          aSuccess ? "succeeded" : "failed");

  if (!aSuccess) {
    InitFailed();
    return;
  }

  Vector<uint8_t> certificate, wrappedKey;
  auto err = mDrm->provideProvisionResponse(
      GonkDrmConverter::ToByteVector(aResponse), certificate, wrappedKey);
  if (err != OK) {
    GD_LOGE(
        "%p GonkDrmSupport::UpdateProvisioningResponse, DRM "
        "provideProvisionResponse failed(%d)",
        this, err);
    InitFailed();
    return;
  }

  err = OpenCryptoSession();
  if (err != OK) {
    GD_LOGE(
        "%p GonkDrmSupport::UpdateProvisioningResponse, OpenCryptoSession "
        "failed",
        this);
    InitFailed();
    return;
  }

  InitCompleted();
}

void GonkDrmSupport::Reset() {
  if (mDrm) {
    mDrm->destroyPlugin();
    mDrm = nullptr;
  }
  if (mSharedData) {
    mSharedData->SetCryptoSessionId(Vector<uint8_t>());
    mSharedData = nullptr;
  }
  mInitPromiseId = 0;
  mCallback = nullptr;
}

void GonkDrmSupport::Shutdown() {
  GD_LOGD("%p GonkDrmSupport::Shutdown", this);
  Reset();
}

void GonkDrmSupport::CreateSession(uint32_t aPromiseId,
                                   uint32_t aCreateSessionToken,
                                   const nsCString& aInitDataType,
                                   const nsTArray<uint8_t>& aInitData,
                                   MediaKeySessionType aSessionType) {
  GD_ASSERT(mDrm);
  GD_LOGD(
      "%p GonkDrmSupport::CreateSession, init data type %s, session type %d",
      this, aInitDataType.Data(), aSessionType);

  Vector<uint8_t> sessionId;
  auto err = mDrm->openSession(kGonkDrmSecurityLevel, sessionId);
  if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::CreateSession, DRM openSession failed(%d)",
            this, err);
    mCallback->RejectPromiseWithStateError(aPromiseId, "openSession failed"_ns);
    return;
  }

  KeyedVector<String8, String8> optionalParameters;
  DrmPlugin::KeyType keyType;

  switch (aSessionType) {
    case MediaKeySessionType::Temporary:
      keyType = DrmPlugin::kKeyType_Streaming;
      break;
    case MediaKeySessionType::Persistent_license:
      keyType = DrmPlugin::kKeyType_Offline;
      break;
    default:
      GD_LOGE("%p GonkDrmSupport::CreateSession, unsupported session type %d",
              this, aSessionType);
      mDrm->closeSession(sessionId);
      mCallback->RejectPromiseWithStateError(aPromiseId,
                                             "unsupported session type"_ns);
      return;
  }

  Vector<uint8_t> request;
  String8 defaultUrl;
  DrmPlugin::KeyRequestType keyRequestType;

  err = mDrm->getKeyRequest(
      sessionId, GonkDrmConverter::ToByteVector(aInitData),
      GonkDrmConverter::ToString8(aInitDataType), keyType, optionalParameters,
      request, defaultUrl, &keyRequestType);
  if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::CreateSession, DRM getKeyRequest failed(%d)",
            this, err);
    mDrm->closeSession(sessionId);
    mCallback->RejectPromiseWithStateError(aPromiseId,
                                           "getKeyRequest failed"_ns);
    return;
  }

  MediaKeyMessageType messageType;

  switch (keyRequestType) {
    case DrmPlugin::kKeyRequestType_Initial:
      messageType = MediaKeyMessageType::License_request;
      break;
    case DrmPlugin::kKeyRequestType_Renewal:
      messageType = MediaKeyMessageType::License_renewal;
      break;
    case DrmPlugin::kKeyRequestType_Release:
      messageType = MediaKeyMessageType::License_release;
      break;
    default:
      GD_LOGE(
          "%p GonkDrmSupport::CreateSession, unsupported key request type %d",
          this, keyRequestType);
      mDrm->closeSession(sessionId);
      mCallback->RejectPromiseWithStateError(aPromiseId,
                                             "unsupported key request type"_ns);
      return;
  }

  auto emeSessionId = GonkDrmUtils::EncodeBase64(sessionId);
  mCallback->SetSessionId(aCreateSessionToken, emeSessionId);
  mCallback->ResolvePromise(aPromiseId);
  mCallback->SessionMessage(emeSessionId, messageType,
                            GonkDrmConverter::ToNsByteArray(request));
  GD_LOGD("%p GonkDrmSupport::CreateSession, session opened: %s", this,
          emeSessionId.Data());
}

void GonkDrmSupport::UpdateSession(uint32_t aPromiseId,
                                   const nsCString& aEmeSessionId,
                                   const nsTArray<uint8_t>& aResponse) {
  GD_ASSERT(mDrm);
  GD_LOGD("%p GonkDrmSupport::UpdateSession, session ID %s", this,
          aEmeSessionId.Data());

  auto sessionId = GonkDrmUtils::DecodeBase64(aEmeSessionId);
  Vector<uint8_t> keySetId;
  auto err = mDrm->provideKeyResponse(
      sessionId, GonkDrmConverter::ToByteVector(aResponse), keySetId);
  if (err != OK) {
    GD_LOGE(
        "%p GonkDrmSupport::UpdateSession, DRM provideKeyResponse failed, err "
        "%d",
        this, err);
    mCallback->RejectPromiseWithStateError(aPromiseId,
                                           "provideKeyResponse failed"_ns);
    mCallback->SessionError(aEmeSessionId, NS_ERROR_DOM_INVALID_STATE_ERR, -1,
                            "provideKeyResponse failed"_ns);
    return;
  }

  mCallback->ResolvePromise(aPromiseId);
}

void GonkDrmSupport::CloseSession(uint32_t aPromiseId,
                                  const nsCString& aEmeSessionId) {
  GD_ASSERT(mDrm);
  GD_LOGD("%p GonkDrmSupport::CloseSession, session ID %s", this,
          aEmeSessionId.Data());

  auto sessionId = GonkDrmUtils::DecodeBase64(aEmeSessionId);
  auto err = mDrm->closeSession(sessionId);
  if (err != OK) {
    GD_LOGE("%p GonkDrmSupport::CloseSession, DRM closeSession failed(%d)",
            this, err);
    mCallback->RejectPromiseWithStateError(aPromiseId,
                                           "closeSession failed"_ns);
    return;
  }

  mSharedData->RemoveSession(sessionId);
  mCallback->ResolvePromise(aPromiseId);
  mCallback->SessionClosed(aEmeSessionId);
}

void GonkDrmSupport::SetServerCertificate(uint32_t aPromiseId,
                                          const nsTArray<uint8_t>& aCert) {
  GD_ASSERT(mDrm);
  GD_LOGD("%p GonkDrmSupport::SetServerCertificate", this);

  auto err = mDrm->setPropertyByteArray(String8("serviceCertificate"),
                                        GonkDrmConverter::ToByteVector(aCert));
  if (err != OK) {
    GD_LOGE(
        "%p GonkDrmSupport::SetServerCertificate, DRM set serviceCertificate "
        "failed(%d)",
        this, err);
    mCallback->RejectPromiseWithStateError(aPromiseId,
                                           "set serviceCertificate failed"_ns);
    return;
  }
  mCallback->ResolvePromise(aPromiseId);
}

// Called on binder thread.
void GonkDrmSupport::notify(DrmPlugin::EventType aEventType, int aExtra,
                            const Parcel* aObj) {
  GD_LOGV("%p GonkDrmSupport::notify, event %d, extra %d, parcel %p", this,
          aEventType, aExtra, aObj);

  // Make a copy of the Parcel and dispatch it to owner thread.
  std::unique_ptr<Parcel> parcel;
  if (aObj) {
    parcel.reset(new Parcel());
    parcel->appendFrom(aObj, 0, aObj->dataSize());
    parcel->setDataPosition(0);
  }

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmSupport::notify",
      [aEventType, aExtra, obj = std::move(parcel), this, self = Self()]() {
        Notify(aEventType, aExtra, obj.get());
      }));
}

void GonkDrmSupport::Notify(DrmPlugin::EventType aEventType, int aExtra,
                            const Parcel* aObj) {
  if (!mDrm) {
    GD_LOGD("%p GonkDrmSupport::Notify, already shut down", this);
    return;
  }

  if (aEventType == DrmPlugin::kDrmPluginEventKeysChange) {
    OnKeyStatusChanged(aObj);
  }
}

void GonkDrmSupport::OnKeyStatusChanged(const Parcel* aParcel) {
  GD_ASSERT(mCallback);

  if (!aParcel) {
    return;
  }

  nsTArray<CDMKeyInfo> keyInfos;
  auto sessionId = GonkDrmUtils::ReadByteVectorFromParcel(aParcel);
  for (auto num = aParcel->readInt32(); num > 0; num--) {
    auto keyId = GonkDrmUtils::ReadByteVectorFromParcel(aParcel);
    auto keyStatus = static_cast<GonkDrmKeyStatus>(aParcel->readInt32());
    keyInfos.EmplaceBack(
        GonkDrmConverter::ToNsByteArray(keyId),
        Optional<MediaKeyStatus>(ConvertToMediaKeyStatus(keyStatus)));
  }

  NotifyKeyStatus(GonkDrmUtils::EncodeBase64(sessionId), std::move(keyInfos));
}

void GonkDrmSupport::NotifyKeyStatus(const nsCString& aEmeSessionId,
                                     nsTArray<CDMKeyInfo>&& aKeyInfos) {
  for (const auto& info : aKeyInfos) {
    mSharedData->AddKey(GonkDrmUtils::DecodeBase64(aEmeSessionId),
                        GonkDrmConverter::ToByteVector(info.mKeyId));
  }
  mCallback->BatchedKeyStatusChanged(aEmeSessionId, std::move(aKeyInfos));
}

}  // namespace android
