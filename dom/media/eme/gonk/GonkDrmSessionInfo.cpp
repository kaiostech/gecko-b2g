/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmSessionInfo.h"

#include "GonkDrmStorageProxy.h"
#include "GonkDrmUtils.h"

// These strings are written to the storage. Be careful if need to modify it.
#define GONK_DRM_SESSION_TYPE_TEMPORARY "temporary"_ns
#define GONK_DRM_SESSION_TYPE_PERSISTENT "persistent"_ns
#define GONK_DRM_SESSION_TYPE_RELEASED "released"_ns

namespace android {

/* static */
sp<GonkDrmSessionInfo> GonkDrmSessionInfo::CreateTemporary(
    GonkDrmStorageProxy* aStorage, const Vector<uint8_t>& aDrmId) {
  nsCString emeId = GonkDrmUtils::EncodeBase64(aDrmId);
  if (emeId.IsEmpty()) {
    return nullptr;
  }
  return new GonkDrmSessionInfo(aStorage, aDrmId, emeId,
                                GONK_DRM_SESSION_TYPE_TEMPORARY);
}

/* static */
sp<GonkDrmSessionInfo> GonkDrmSessionInfo::CreatePersistent(
    GonkDrmStorageProxy* aStorage, const Vector<uint8_t>& aDrmId,
    const nsACString& aEmeId) {
  nsCString emeId(aEmeId);
  if (emeId.IsEmpty()) {
    emeId = GonkDrmUtils::GenerateUUID();
    if (emeId.IsEmpty()) {
      return nullptr;
    }
  }
  return new GonkDrmSessionInfo(aStorage, aDrmId, emeId,
                                GONK_DRM_SESSION_TYPE_PERSISTENT);
}

GonkDrmSessionInfo::GonkDrmSessionInfo(GonkDrmStorageProxy* aStorage,
                                       const Vector<uint8_t>& aDrmId,
                                       const nsACString& aEmeId,
                                       const nsACString& aSessionType)
    : mDrmId(aDrmId),
      mEmeId(aEmeId),
      mSessionType(aSessionType),
      mStorage(aStorage) {}

void GonkDrmSessionInfo::SaveToStorage(const SuccessCallback& aSuccessCb,
                                       const FailureCallback& aFailureCb) {
  if (IsTemporary()) {
    aFailureCb("cannot save temporary session"_ns);
    return;
  }

  mStorage->Add(mEmeId, mMimeType, mSessionType,
                GonkDrmUtils::EncodeBase64(mKeySetId),
                [aSuccessCb, aFailureCb](bool aSuccess) {
                  if (aSuccess) {
                    aSuccessCb();
                  } else {
                    aFailureCb("failed to save session info"_ns);
                  }
                });
}

void GonkDrmSessionInfo::LoadFromStorage(const SuccessCallback& aSuccessCb,
                                         const FailureCallback& aFailureCb) {
  if (IsTemporary()) {
    aFailureCb("cannot load temporary session"_ns);
    return;
  }

  mStorage->Get(
      mEmeId, [aSuccessCb, aFailureCb, self = sp<GonkDrmSessionInfo>(this)](
                  bool aSuccess, const nsACString& aMimeType,
                  const nsACString& aSessionType, const nsACString& aKeySetId) {
        if (aSuccess) {
          self->mMimeType = aMimeType;
          self->mSessionType = aSessionType;
          self->mKeySetId = GonkDrmUtils::DecodeBase64(aKeySetId);
          aSuccessCb();
        } else {
          aFailureCb("failed to load session info"_ns);
        }
      });
}

void GonkDrmSessionInfo::EraseFromStorage(const SuccessCallback& aSuccessCb,
                                          const FailureCallback& aFailureCb) {
  if (IsTemporary()) {
    aFailureCb("cannot erase temporary session"_ns);
    return;
  }

  mStorage->Remove(mEmeId, [aSuccessCb, aFailureCb](bool aSuccess) {
    if (aSuccess) {
      aSuccessCb();
    } else {
      aFailureCb("failed to erase session info"_ns);
    }
  });
}

void GonkDrmSessionInfo::SetMimeType(const nsACString& aMimeType) {
  mMimeType = aMimeType;
}

void GonkDrmSessionInfo::SetReleased() {
  // We can only release a persistent session.
  if (mSessionType == GONK_DRM_SESSION_TYPE_PERSISTENT) {
    mSessionType = GONK_DRM_SESSION_TYPE_RELEASED;
  }
}

void GonkDrmSessionInfo::SetKeySetId(const Vector<uint8_t>& aKeySetId) {
  mKeySetId = aKeySetId;
}

bool GonkDrmSessionInfo::IsTemporary() const {
  return mSessionType == GONK_DRM_SESSION_TYPE_TEMPORARY;
}

bool GonkDrmSessionInfo::IsReleased() const {
  return mSessionType == GONK_DRM_SESSION_TYPE_RELEASED;
}

}  // namespace android
