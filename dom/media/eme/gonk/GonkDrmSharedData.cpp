/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmSharedData.h"
#include "GonkDrmUtils.h"

namespace android {

void GonkDrmSharedData::SetCryptoSessionId(const Vector<uint8_t>& aSessionId) {
  Mutex::Autolock lock(mLock);
  mCryptoSessionId = aSessionId;
}

Vector<uint8_t> GonkDrmSharedData::GetCryptoSessionId() {
  Mutex::Autolock lock(mLock);
  return mCryptoSessionId;
}

void GonkDrmSharedData::AddKey(const Vector<uint8_t>& aSessionId,
                               const Vector<uint8_t>& aKeyId) {
  auto sessionId = GonkDrmConverter::ToStdByteVector(aSessionId);
  auto keyId = GonkDrmConverter::ToStdByteVector(aKeyId);

  Mutex::Autolock lock(mLock);
  mSessions[sessionId].insert(keyId);
}

Vector<uint8_t> GonkDrmSharedData::FindSessionByKey(
    const Vector<uint8_t>& aKeyId) {
  auto keyId = GonkDrmConverter::ToStdByteVector(aKeyId);

  Mutex::Autolock lock(mLock);
  for (auto& [sessionId, keyIdSet] : mSessions) {
    if (keyIdSet.find(keyId) != keyIdSet.end()) {
      return GonkDrmConverter::ToByteVector(sessionId);
    }
  }
  return Vector<uint8_t>();
}

void GonkDrmSharedData::RemoveSession(const Vector<uint8_t>& aSessionId) {
  auto sessionId = GonkDrmConverter::ToStdByteVector(aSessionId);

  Mutex::Autolock lock(mLock);
  mSessions.erase(sessionId);
}

}  // namespace android
