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
#include <system/graphics.h>
#include <ui/GraphicBuffer.h>
#include <utils/RefBase.h>

#include <vector>

#include "AudioSampleFormat.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
class MediaRawData;
class TrackInfo;
}  // namespace mozilla

namespace mozilla::layers {
class Image;
class ImageContainer;
}  // namespace mozilla::layers

namespace rtc {
template <class T>
class scoped_refptr;
}

namespace webrtc {
class VideoFrameBuffer;
}

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

class GonkImageHandle final : public RefBase {
 public:
  static sp<GonkImageHandle> From(const sp<GraphicBuffer>& aBuffer,
                                  bool aWrite = false);

  static sp<GonkImageHandle> From(const sp<MediaCodecBuffer>& aBuffer,
                                  bool aWrite = false);

  static sp<GonkImageHandle> From(const RefPtr<mozilla::layers::Image>& aImage,
                                  bool aWrite = false);

  static sp<GonkImageHandle> From(
      const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& aBuffer,
      bool aWrite = false);

  enum class ColorFormat : int32_t {
    Unknown,
    Unsupported,
    I420,
    NV12,
    NV21,
  };

  bool Writable() { return mWrite; }
  ColorFormat Format() { return mFormat; }
  std::string FormatString();
  uint32_t Width(size_t aPlane = 0) { return Plane(aPlane).mWidth; }
  uint32_t Height(size_t aPlane = 0) { return Plane(aPlane).mHeight; }
  uint32_t Step(size_t aPlane) { return Plane(aPlane).mStep; }
  uint32_t Stride(size_t aPlane) { return Plane(aPlane).mStride; }
  uint32_t HSubsampling(size_t aPlane) { return Plane(aPlane).mHSubsampling; }
  uint32_t VSubsampling(size_t aPlane) { return Plane(aPlane).mVSubsampling; }
  uint8_t* Data(size_t aPlane) { return Plane(aPlane).mData; }

 private:
  struct PlaneInfo {
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint32_t mStep = 0;
    uint32_t mStride = 0;
    uint32_t mHSubsampling = 0;
    uint32_t mVSubsampling = 0;
    uint8_t* mData = nullptr;
  };

  GonkImageHandle() = default;
  virtual ~GonkImageHandle();
  status_t InitCheck();
  void GuessUnknownFormat();
  PlaneInfo& Plane(size_t aIndex);

  bool mWrite = false;
  ColorFormat mFormat = ColorFormat::Unknown;
  PlaneInfo mPlanes[3];
  std::function<void()> mOnDestruction;
};

class GonkImageUtils {
  using Image = mozilla::layers::Image;
  using ImageContainer = mozilla::layers::ImageContainer;

 public:
  static RefPtr<Image> CreateI420Image(ImageContainer* aContainer,
                                       uint32_t aWidth, uint32_t aHeight);

  template <class S>
  static RefPtr<Image> CreateI420ImageCopy(ImageContainer* aContainer,
                                           S&& aSrcBuffer, int aDegrees = 0) {
    auto srcHandle = GonkImageHandle::From(std::forward<S>(aSrcBuffer), false);
    return CreateI420ImageCopyImpl(aContainer, srcHandle, aDegrees);
  }

  template <class S, class D>
  static status_t CopyImage(S&& aSrcBuffer, D&& aDstBuffer, int aDegrees = 0) {
    auto srcHandle = GonkImageHandle::From(std::forward<S>(aSrcBuffer), false);
    auto dstHandle = GonkImageHandle::From(std::forward<D>(aDstBuffer), true);
    return CopyImageImpl(srcHandle, dstHandle, aDegrees);
  }

 private:
  static RefPtr<Image> CreateI420ImageCopyImpl(
      ImageContainer* aContainer, const sp<GonkImageHandle>& aSrcHandle,
      int aDegrees);

  static status_t CopyImageImpl(const sp<GonkImageHandle>& aSrcHandle,
                                const sp<GonkImageHandle>& aDstHandle,
                                int aDegrees);

  static status_t ConvertImage(const sp<GonkImageHandle>& aSrcHandle,
                               const sp<GonkImageHandle>& aDstHandle);

  static status_t RotateImage(const sp<GonkImageHandle>& aSrcHandle,
                              const sp<GonkImageHandle>& aDstHandle,
                              int aDegrees);
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
