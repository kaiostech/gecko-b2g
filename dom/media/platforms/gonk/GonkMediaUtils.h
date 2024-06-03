/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_MEDIA_UTILS_H
#define GONK_MEDIA_UTILS_H

#include <media/hardware/CryptoAPI.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/MediaDefs.h>
#include <utils/RefBase.h>

#include <vector>

#include "AudioSampleFormat.h"

namespace mozilla {
class MediaRawData;
class TrackInfo;
}  // namespace mozilla

namespace android {

// A template wrapper class that allows any object to be managed by android::sp.
template <class T>
class GonkObjectHolder : public RefBase {
 public:
  template <class U>
  static sp<GonkObjectHolder> Create(U&& aObj) {
    return new GonkObjectHolder(std::forward<U>(aObj));
  }

  T& Get() { return mObj; }

 private:
  template <class U>
  GonkObjectHolder(U&& aObj) : mObj(std::forward<U>(aObj)) {}

  T mObj;
};

class GonkCryptoInfo : public RefBase {
 public:
  CryptoPlugin::Mode mMode = CryptoPlugin::kMode_Unencrypted;
  CryptoPlugin::Pattern mPattern = {0, 0};
  std::vector<CryptoPlugin::SubSample> mSubSamples;
  std::vector<uint8_t> mKey;
  std::vector<uint8_t> mIV;
};

class GonkBufferWriter {
 public:
  GonkBufferWriter(const sp<MediaCodecBuffer>& aBuffer);

  void Clear();

  bool Append(const uint8_t* aData, size_t aSize);

 private:
  sp<MediaCodecBuffer> mBuffer;
};

class GonkMediaUtils {
  using AudioDataValue = mozilla::AudioDataValue;
  using PcmCopy = std::function<uint32_t(AudioDataValue*, uint32_t)>;

 public:
  static AudioEncoding PreferredPcmEncoding();

  static size_t GetAudioSampleSize(AudioEncoding aEncoding);

  static PcmCopy CreatePcmCopy(const uint8_t* aSource, size_t aSourceBytes,
                               uint32_t aChannels, AudioEncoding aEncoding);

  static sp<AMessage> GetMediaCodecConfig(const mozilla::TrackInfo* aInfo);

  static sp<GonkCryptoInfo> GetCryptoInfo(const mozilla::MediaRawData* aSample);

  static std::vector<AString> FindMatchingCodecs(const char* aMime,
                                                 bool aEncoder);
};

}  // namespace android

#endif  // GONK_MEDIA_UTILS_H
