/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_GONK_H264_VIDEO_CODEC_H_
#define WEBRTC_GONK_H264_VIDEO_CODEC_H_

#include <utils/RefBase.h>

#include "VideoConduit.h"
#include "WebrtcGonkVideoCodec.h"

namespace android {
class OMXCodecReservation;
}  // namespace android

namespace mozilla {

#define OMX_IDR_NEEDED_FOR_BITRATE 0

class WebrtcGonkH264VideoEncoder
    : public WebrtcVideoEncoder,
      public android::WebrtcGonkVideoEncoder::Callback {
 public:
  WebrtcGonkH264VideoEncoder();

  ~WebrtcGonkH264VideoEncoder();

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     int32_t aNumOfCores, size_t aMaxPayloadSize) override;

  int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Release() override;

  void SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

  void OnEncoded(webrtc::EncodedImage& aEncodedImage) override;

 private:
  android::sp<android::WebrtcGonkVideoEncoder> mEncoder;
  android::sp<android::OMXCodecReservation> mReservation;

  webrtc::EncodedImageCallback* mCallback = nullptr;
  uint32_t mWidth = 0;
  uint32_t mHeight = 0;
  double mFrameRate = 0;
  uint32_t mBitrateBps = 0;
#ifdef OMX_IDR_NEEDED_FOR_BITRATE
  uint32_t mBitrateAtLastIDR = 0;
  TimeStamp mLastIDRTime;
#endif
  bool mOMXConfigured = false;
  bool mOMXReconfigure = false;
};

class WebrtcGonkH264VideoDecoder
    : public WebrtcVideoDecoder,
      public android::WebrtcGonkVideoDecoder::Callback {
 public:
  WebrtcGonkH264VideoDecoder();

  ~WebrtcGonkH264VideoDecoder();

  bool Configure(const webrtc::VideoDecoder::Settings& settings) override;

  int32_t Decode(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                 int64_t aRenderTimeMs = -1) override;

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback) override;

  int32_t Release() override;

  const char* ImplementationName() const override { return "Gonk"; }

  void OnDecoded(webrtc::VideoFrame& aVideoFrame) override {
    if (mCallback) {
      mCallback->Decoded(aVideoFrame);
    }
  }

 private:
  android::sp<android::WebrtcGonkVideoDecoder> mDecoder;
  android::sp<android::OMXCodecReservation> mReservation;
  webrtc::DecodedImageCallback* mCallback = nullptr;
  bool mCodecConfigSubmitted = false;
};

}  // namespace mozilla

#endif  // WEBRTC_GONK_H264_VIDEO_CODEC_H_
