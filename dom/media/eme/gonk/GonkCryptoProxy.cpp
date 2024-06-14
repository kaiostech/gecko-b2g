/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkCryptoProxy.h"

#include "GonkDrmSharedData.h"
#include "GonkDrmUtils.h"
#include "mozilla/EMEUtils.h"

namespace android {

GonkCryptoProxy::GonkCryptoProxy(const nsAString& aKeySystem,
                                 const sp<GonkDrmSharedData>& aSharedData)
    : mSupportsSessionSharing(mozilla::IsWidevineKeySystem(aKeySystem)),
      mSessionId(aSharedData->GetCryptoSessionId()),
      mSharedData(aSharedData),
      mCrypto(GonkDrmUtils::MakeCrypto(aKeySystem, mSessionId)) {
  GD_LOGD("%p GonkCryptoProxy::GonkCryptoProxy, %s, %s session sharing", this,
          NS_ConvertUTF16toUTF8(aKeySystem).Data(),
          mSupportsSessionSharing ? "supports" : "not support");
}

GonkCryptoProxy::~GonkCryptoProxy() {
  GD_LOGD("%p GonkCryptoProxy::~GonkCryptoProxy", this);
  if (mCrypto) {
    mCrypto->destroyPlugin();
  }
}

status_t GonkCryptoProxy::initCheck() const {
  return mCrypto ? mCrypto->initCheck() : NO_INIT;
}

bool GonkCryptoProxy::isCryptoSchemeSupported(const uint8_t uuid[16]) {
  return mCrypto ? mCrypto->isCryptoSchemeSupported(uuid) : false;
}

status_t GonkCryptoProxy::createPlugin(const uint8_t uuid[16], const void* data,
                                       size_t size) {
  // Not allowed.
  return INVALID_OPERATION;
}

status_t GonkCryptoProxy::destroyPlugin() {
  // Not allowed.
  return INVALID_OPERATION;
}

bool GonkCryptoProxy::requiresSecureDecoderComponent(const char* mime) const {
  return mCrypto ? mCrypto->requiresSecureDecoderComponent(mime) : false;
}

void GonkCryptoProxy::notifyResolution(uint32_t width, uint32_t height) {
  if (mCrypto) {
    mCrypto->notifyResolution(width, height);
  }
}

#if ANDROID_VERSION >= 34
DrmStatus GonkCryptoProxy::setMediaDrmSession(const Vector<uint8_t>& sessionId) {
  // Not allowed.
  return INVALID_OPERATION;
}
#else
status_t GonkCryptoProxy::setMediaDrmSession(const Vector<uint8_t>& sessionId) {
  // Not allowed.
  return INVALID_OPERATION;
}
#endif

void GonkCryptoProxy::UpdateMediaDrmSession(const Vector<uint8_t>& aKeyId) {
  auto sessionId = mSharedData->FindSessionByKey(aKeyId);
  if (sessionId.empty()) {
    GD_LOGE("%p GonkCryptoProxy::UpdateMediaDrmSession, session not found",
            this);
    return;
  }

  // Check if (sessionId == mSessionId).
  // android::Vector doesn't support operator==, so use std::equal() instead.
  if (std::equal(sessionId.begin(), sessionId.end(), mSessionId.begin(),
                 mSessionId.end())) {
    return;
  }

  GD_LOGD("%p GonkCryptoProxy::UpdateMediaDrmSession, set new session", this);
  mSessionId = sessionId;
  auto err = mCrypto->setMediaDrmSession(sessionId);
  if (err != OK) {
    GD_LOGE("%p GonkCryptoProxy::UpdateMediaDrmSession, failed setMediaDrmSession", this);
  }
}

ssize_t GonkCryptoProxy::decrypt(const uint8_t key[16], const uint8_t iv[16],
                                 CryptoPlugin::Mode mode,
                                 const CryptoPlugin::Pattern& pattern,
                                 const SourceBuffer& source, size_t offset,
                                 const CryptoPlugin::SubSample* subSamples,
                                 size_t numSubSamples,
                                 const DestinationBuffer& destination,
                                 AString* errorDetailMsg) {
  if (!mCrypto) {
    return NO_INIT;
  }

  if (!mSupportsSessionSharing && key) {
    Vector<uint8_t> keyId;
    keyId.appendArray(key, 16);
    UpdateMediaDrmSession(keyId);
  }

  auto ret =
      mCrypto->decrypt(key, iv, mode, pattern, source, offset, subSamples,
                       numSubSamples, destination, errorDetailMsg);
  if (ret < 0) {
    GD_LOGE("%p GonkCryptoProxy::decrypt, decrypt failed %zu", this, ret);
  }
  return ret;
}

int32_t GonkCryptoProxy::setHeap(const sp<Memory>& heap) {
  return mCrypto ? mCrypto->setHeap(heap) : NO_INIT;
}

void GonkCryptoProxy::unsetHeap(int32_t seqNum) {
  if (mCrypto) {
    mCrypto->unsetHeap(seqNum);
  }
}

#if ANDROID_VERSION >= 31
status_t GonkCryptoProxy::getLogMessages(
    Vector<drm::V1_4::LogMessage>& logs) const {
  return mCrypto ? mCrypto->getLogMessages(logs) : NO_INIT;
}
#endif

}  // namespace android
