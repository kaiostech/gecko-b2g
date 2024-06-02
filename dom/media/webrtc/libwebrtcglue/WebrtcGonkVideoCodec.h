/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_GONK_VIDEO_CODEC_H_
#define WEBRTC_GONK_VIDEO_CODEC_H_

#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include "GonkMediaUtils.h"
#include "VideoConduit.h"

#include "common_video/include/bitrate_adjuster.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace mozilla::layers {
class ImageContainer;
class TextureClient;
}  // namespace mozilla::layers

namespace android {

class GonkCryptoInfo;
class GonkMediaCodec;
class MediaCodecBuffer;

class WebrtcGonkVideoEncoder : public mozilla::WebrtcVideoEncoder {
 public:
  static WebrtcGonkVideoEncoder* Create(const webrtc::SdpVideoFormat& aFormat);

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const webrtc::VideoEncoder::Settings& aSettings) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Encode(
      const webrtc::VideoFrame& aInputFrame,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t Release() override;

  void SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

  // The following are codec callbacks.
  bool FetchInput(const sp<MediaCodecBuffer>& aBuffer, sp<RefBase>* aInputInfo,
                  sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                  uint32_t* aFlags);

  void Output(const sp<MediaCodecBuffer>& aBuffer,
              const sp<RefBase>& aInputInfo, int64_t aTimeUs, uint32_t aFlags);

  void NotifyError(status_t aErr, int32_t aActionCode);

 private:
  explicit WebrtcGonkVideoEncoder(const webrtc::SdpVideoFormat& aFormat);

  virtual ~WebrtcGonkVideoEncoder();

  const char* LogTag() { return mLogTag.c_str(); }

  const std::string mLogTag;
  std::atomic<status_t> mError = OK;
  Mutex mCallbackMutex;
  webrtc::EncodedImageCallback* mCallback = nullptr;
  webrtc::SdpVideoFormat::Parameters mFormatParams;
  webrtc::CodecSpecificInfo mCodecSpecific;
  webrtc::BitrateAdjuster mBitrateAdjuster;
  webrtc::RtpTimestampUnwrapper mTimestampUnwrapper;
  uint32_t mMinBitrateBps = {0};
  uint32_t mMaxBitrateBps = {0};
  sp<GonkMediaCodec> mCodec;

  struct InputHolder : public RefBase {
    webrtc::VideoFrame mFrame;
    webrtc::VideoFrameType mFrameType;

    InputHolder(const webrtc::VideoFrame& aFrame,
                webrtc::VideoFrameType aFrameType)
        : mFrame(aFrame), mFrameType(aFrameType) {}
  };
  GonkDataQueue<InputHolder> mInputQueue;
};

class WebrtcGonkVideoDecoder : public mozilla::WebrtcVideoDecoder {
  using ImageContainer = mozilla::layers::ImageContainer;
  using TextureClient = mozilla::layers::TextureClient;

 public:
  static WebrtcGonkVideoDecoder* Create(const webrtc::SdpVideoFormat& aFormat);

  bool Configure(const webrtc::VideoDecoder::Settings& aSettings) override;

  int32_t Decode(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                 int64_t aRenderTimeMs) override;

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback) override;

  int32_t Release() override;

  DecoderInfo GetDecoderInfo() const override;

  const char* ImplementationName() const override { return "Gonk"; }

  // The following are codec callbacks.
  bool FetchInput(const sp<MediaCodecBuffer>& aBuffer, sp<RefBase>* aInputInfo,
                  sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                  uint32_t* aFlags);

  void Output(const sp<MediaCodecBuffer>& aBuffer,
              const sp<RefBase>& aInputInfo, int64_t aTimeUs, uint32_t aFlags);

  void OutputTexture(TextureClient* aTexture, const sp<RefBase>& aInputInfo,
                     int64_t aTimeUs, uint32_t aFlags);

  void NotifyError(status_t aErr, int32_t aActionCode);

 private:
  explicit WebrtcGonkVideoDecoder(const webrtc::SdpVideoFormat& aFormat);

  virtual ~WebrtcGonkVideoDecoder();

  const char* LogTag() { return mLogTag.c_str(); }

  const std::string mLogTag;
  std::atomic<status_t> mError = OK;
  bool mNeedKeyframe = true;
  Mutex mCallbackMutex;
  webrtc::DecodedImageCallback* mCallback = nullptr;
  webrtc::SdpVideoFormat::Parameters mFormatParams;
  webrtc::RtpTimestampUnwrapper mTimestampUnwrapper;
  RefPtr<ImageContainer> mImageContainer;
  sp<GonkMediaCodec> mCodec;

  using InputHolder = GonkObjectHolder<webrtc::EncodedImage>;
  GonkDataQueue<InputHolder> mInputQueue;
};

}  // namespace android

#endif  // WEBRTC_GONK_VIDEO_CODEC_H_
