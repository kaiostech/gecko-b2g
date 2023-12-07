/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkCryptoProxy_H
#define GonkCryptoProxy_H

#include "nsString.h"

#include <mediadrm/ICrypto.h>

namespace android {

class GonkDrmSharedData;

#if ANDROID_VERSION >= 30
class GonkCryptoProxy : public ICrypto {
  using SourceBuffer = drm::V1_0::SharedBuffer;
  using DestinationBuffer = drm::V1_0::DestinationBuffer;
  using Memory = hardware::HidlMemory;
#else
class GonkCryptoProxy : public BnCrypto {
  using SourceBuffer = ICrypto::SourceBuffer;
  using DestinationBuffer = ICrypto::DestinationBuffer;
  using Memory = IMemoryHeap;
#endif

 public:
  GonkCryptoProxy(const nsAString& aKeySystem,
                  const sp<GonkDrmSharedData>& aSharedData);

  status_t initCheck() const override;

  bool isCryptoSchemeSupported(const uint8_t uuid[16]) override;

  status_t createPlugin(const uint8_t uuid[16], const void* data,
                        size_t size) override;

  status_t destroyPlugin() override;

  bool requiresSecureDecoderComponent(const char* mime) const override;

  void notifyResolution(uint32_t width, uint32_t height) override;

  status_t setMediaDrmSession(const Vector<uint8_t>& sessionId) override;

  ssize_t decrypt(const uint8_t key[16], const uint8_t iv[16],
                  CryptoPlugin::Mode mode, const CryptoPlugin::Pattern& pattern,
                  const SourceBuffer& source, size_t offset,
                  const CryptoPlugin::SubSample* subSamples,
                  size_t numSubSamples, const DestinationBuffer& destination,
                  AString* errorDetailMsg) override;

  int32_t setHeap(const sp<Memory>& heap) override;

  void unsetHeap(int32_t seqNum) override;

#if ANDROID_VERSION >= 31
  status_t getLogMessages(Vector<drm::V1_4::LogMessage>& logs) const override;
#endif

 private:
  ~GonkCryptoProxy();

  void UpdateMediaDrmSession(const Vector<uint8_t>& aKeyId);

  bool mSupportsSessionSharing = false;
  Vector<uint8_t> mSessionId;
  sp<GonkDrmSharedData> mSharedData;
  sp<ICrypto> mCrypto;
};

}  // namespace android

#endif
