/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmListener.h"

#include "GonkDrmSupport.h"
#include "GonkDrmUtils.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"
#include "nsISerialEventTarget.h"
#include "nsThreadUtils.h"

#include <media/drm/DrmAPI.h>

namespace android {

using mozilla::CDMKeyInfo;
using mozilla::dom::MediaKeyStatus;
using mozilla::dom::Optional;

static MediaKeyStatus ConvertToMediaKeyStatus(DrmPlugin::KeyStatusType aType) {
  switch (aType) {
    case DrmPlugin::kKeyStatusType_Usable:
      return MediaKeyStatus::Usable;
    case DrmPlugin::kKeyStatusType_Expired:
      return MediaKeyStatus::Expired;
    case DrmPlugin::kKeyStatusType_OutputNotAllowed:
      return MediaKeyStatus::Output_restricted;
    case DrmPlugin::kKeyStatusType_StatusPending:
      return MediaKeyStatus::Status_pending;
    case DrmPlugin::kKeyStatusType_InternalError:
    case DrmPlugin::kKeyStatusType_UsableInFuture:
    default:
      return MediaKeyStatus::Internal_error;
  }
}

#if ANDROID_VERSION >= 30
void GonkDrmListener::sendEvent(DrmPlugin::EventType aEventType,
                                const hardware::hidl_vec<uint8_t>& aSessionId,
                                const hardware::hidl_vec<uint8_t>& aData) {
  GD_LOGV("%p GonkDrmListener::sendEvent, event %d", this, aEventType);

  switch (aEventType) {
    case DrmPlugin::kDrmPluginEventKeyNeeded: {
      auto sessionId = GonkDrmConverter::ToByteVector(aSessionId);
      auto initData = GonkDrmConverter::ToNsByteArray(aData);

      mOwnerThread->Dispatch(NS_NewRunnableFunction(
          "GonkDrmListener::sendEvent",
          [sessionId, data = std::move(initData), self = Self()]() {
            if (auto cdm = self->mCDM.promote()) {
              cdm->OnKeyNeeded(sessionId, data);
            }
          }));
      break;
    }
    default:
      // Ignore.
      break;
  }
}

void GonkDrmListener::sendExpirationUpdate(
    const hardware::hidl_vec<uint8_t>& aSessionId, int64_t aExpiryTimeInMS) {
  GD_LOGV("%p GonkDrmListener::sendExpirationUpdate", this);
  auto sessionId = GonkDrmConverter::ToByteVector(aSessionId);

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmListener::sendExpirationUpdate",
      [sessionId, aExpiryTimeInMS, self = Self()]() {
        if (auto cdm = self->mCDM.promote()) {
          cdm->OnExpirationUpdated(sessionId, aExpiryTimeInMS);
        }
      }));
}

void GonkDrmListener::sendKeysChange(
    const hardware::hidl_vec<uint8_t>& aSessionId,
    const std::vector<DrmKeyStatus>& aKeyStatusList, bool aHasNewUsableKey) {
  GD_LOGV("%p GonkDrmListener::sendKeysChange", this);
  auto sessionId = GonkDrmConverter::ToByteVector(aSessionId);
  nsTArray<CDMKeyInfo> keyInfoList;
  for (auto& [type, keyId] : aKeyStatusList) {
    auto keyStatus =
        ConvertToMediaKeyStatus(static_cast<DrmPlugin::KeyStatusType>(type));
    keyInfoList.EmplaceBack(GonkDrmConverter::ToNsByteArray(keyId),
                            Optional<MediaKeyStatus>(keyStatus));
  }

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmListener::sendKeysChange",
      [sessionId, keyInfos = std::move(keyInfoList), aHasNewUsableKey,
       self = Self()]() mutable {
        if (auto cdm = self->mCDM.promote()) {
          cdm->OnKeyStatusChanged(sessionId, std::move(keyInfos),
                                  aHasNewUsableKey);
        }
      }));
}

void GonkDrmListener::sendSessionLostState(
    const hardware::hidl_vec<uint8_t>& aSessionId) {
  GD_LOGV("%p GonkDrmListener::sendSessionLostState", this);
}

#else
void GonkDrmListener::notify(DrmPlugin::EventType aEventType, int aExtra,
                             const Parcel* aParcel) {
  GD_LOGV("%p GonkDrmListener::notify, event %d, extra %d, parcel %p", this,
          aEventType, aExtra, aParcel);

  switch (aEventType) {
    case DrmPlugin::kDrmPluginEventKeyNeeded: {
      auto sessionId = GonkDrmUtils::ReadByteVectorFromParcel(aParcel);
      auto initData = GonkDrmUtils::ReadNsByteArrayFromParcel(aParcel);

      mOwnerThread->Dispatch(NS_NewRunnableFunction(
          "GonkDrmListener::notify",
          [sessionId, data = std::move(initData), self = Self()]() {
            if (auto cdm = self->mCDM.promote()) {
              cdm->OnKeyNeeded(sessionId, data);
            }
          }));
      break;
    }
    case DrmPlugin::kDrmPluginEventExpirationUpdate: {
      auto sessionId = GonkDrmUtils::ReadByteVectorFromParcel(aParcel);
      auto expirationTime = aParcel->readInt64();

      mOwnerThread->Dispatch(NS_NewRunnableFunction(
          "GonkDrmListener::notify",
          [sessionId, expirationTime, self = Self()]() {
            if (auto cdm = self->mCDM.promote()) {
              cdm->OnExpirationUpdated(sessionId, expirationTime);
            }
          }));
      break;
    }
    case DrmPlugin::kDrmPluginEventKeysChange: {
      auto sessionId = GonkDrmUtils::ReadByteVectorFromParcel(aParcel);
      nsTArray<CDMKeyInfo> keyInfoList;
      for (auto num = aParcel->readInt32(); num > 0; num--) {
        auto keyId = GonkDrmUtils::ReadNsByteArrayFromParcel(aParcel);
        auto keyStatus = ConvertToMediaKeyStatus(
            static_cast<DrmPlugin::KeyStatusType>(aParcel->readInt32()));
        keyInfoList.EmplaceBack(std::move(keyId),
                                Optional<MediaKeyStatus>(keyStatus));
      }
      bool hasNewUsableKey = aParcel->readInt32();

      mOwnerThread->Dispatch(NS_NewRunnableFunction(
          "GonkDrmListener::notify",
          [sessionId, keyInfos = std::move(keyInfoList), hasNewUsableKey,
           self = Self()]() mutable {
            if (auto cdm = self->mCDM.promote()) {
              cdm->OnKeyStatusChanged(sessionId, std::move(keyInfos),
                                      hasNewUsableKey);
            }
          }));
      break;
    }
    default:
      break;
  }
}
#endif

}  // namespace android
