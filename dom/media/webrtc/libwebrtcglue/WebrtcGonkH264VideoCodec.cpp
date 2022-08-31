/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGonkH264VideoCodec.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <media/stagefright/MediaDefs.h>
#include <OMX_Component.h>

#include "common/browser_logging/CSFLog.h"
#include "OMXCodecWrapper.h"
#include "WebrtcGonkVideoCodec.h"

#undef LOG_TAG
#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

#define LOG_TAG "Gonk"
#define LOGE(...) CSFLogError(LOG_TAG, __VA_ARGS__)
#define LOGW(...) CSFLogWarn(LOG_TAG, __VA_ARGS__)
#define LOGI(...) CSFLogInfo(LOG_TAG, __VA_ARGS__)
#define LOGD(...) CSFLogDebug(LOG_TAG, __VA_ARGS__)
#define LOGV(...) CSFLogVerbose(LOG_TAG, __VA_ARGS__)

using android::sp;

namespace mozilla {

// Encoder.
WebrtcGonkH264VideoEncoder::WebrtcGonkH264VideoEncoder() {
  LOGD("Encoder:%p constructor", this);
  mReservation = new android::OMXCodecReservation(true);
}

int32_t WebrtcGonkH264VideoEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings, int32_t aNumOfCores,
    size_t aMaxPayloadSize) {
  LOGD("Encoder:%p initializing", this);

  if (!mReservation->ReserveOMXCodec()) {
    LOGE("Encoder:%p failed to reserve codec", this);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mEncoder = new android::WebrtcGonkVideoEncoder();
  if (mEncoder->Init(this, android::MEDIA_MIMETYPE_VIDEO_AVC) != android::OK) {
    mEncoder = nullptr;
    LOGE("Encoder:%p failed to initialize", this);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Defer configuration until 1st frame is received because this function will
  // be called more than once, and unfortunately with incorrect setting values
  // at first.
  mWidth = aCodecSettings->width;
  mHeight = aCodecSettings->height;
  mFrameRate = aCodecSettings->maxFramerate;
  mBitrateBps = aCodecSettings->startBitrate * 1000;
  // XXX handle maxpayloadsize (aka mode 0/1)

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkH264VideoEncoder::Encode(
    const webrtc::VideoFrame& aInputImage,
    const std::vector<webrtc::VideoFrameType>* aFrameTypes) {
  MOZ_ASSERT(mEncoder);

  // Have to reconfigure for resolution or framerate changes :-(
  // ~220ms initial configure on 8x10, 50-100ms for re-configure it appears
  // XXX drop frames while this is happening?
  if (aInputImage.width() < 0 || (uint32_t)aInputImage.width() != mWidth ||
      aInputImage.height() < 0 || (uint32_t)aInputImage.height() != mHeight) {
    mWidth = aInputImage.width();
    mHeight = aInputImage.height();
    mOMXReconfigure = true;
  }

  if (!mOMXConfigured || mOMXReconfigure) {
    if (mOMXConfigured) {
      LOGI("Encoder:%p reconfiguring %dx%d @ %u fps", this, mWidth, mHeight,
           mFrameRate);
      mOMXConfigured = false;
    }
    mOMXReconfigure = false;
    // XXX This can take time.  Encode() likely assumes encodes are queued
    // "quickly" and don't block the input too long.  Frames may build up.

    // XXX take from negotiated SDP in codecSpecific data
    OMX_VIDEO_AVCLEVELTYPE level = OMX_VIDEO_AVCLevel3;
    OMX_VIDEO_CONTROLRATETYPE bitrateMode = OMX_Video_ControlRateConstant;

    // Set up configuration parameters for AVC/H.264 encoder.
    sp<android::AMessage> format = new android::AMessage;
    // Fixed values
    format->setString("mime", android::MEDIA_MIMETYPE_VIDEO_AVC);
    // XXX We should only set to < infinity if we're not using any recovery RTCP
    // options However, we MUST set it to a lower value because the 8x10 rate
    // controller only changes rate at GOP boundaries.... but it also changes
    // rate on requested GOPs

    // Too long and we have very low bitrates for the first second or two...
    // plus bug 1014921 means we have to force them every ~3 seconds or less.
    format->setInt32("i-frame-interval", 4 /* seconds */);
    // See mozilla::layers::GrallocImage, supports YUV 4:2:0, CbCr width and
    // height is half that of Y
    format->setInt32("color-format", OMX_COLOR_FormatYUV420SemiPlanar);
    format->setInt32("profile", OMX_VIDEO_AVCProfileBaseline);
    format->setInt32("level", level);
    format->setInt32("bitrate-mode", bitrateMode);
    format->setInt32("store-metadata-in-buffers", 0);
    // XXX Unfortunately, 8x10 doesn't support this, but ask anyways
    format->setInt32("prepend-sps-pps-to-idr-frames", 1);
    // Input values.
    format->setInt32("width", mWidth);
    format->setInt32("height", mHeight);
    format->setInt32("stride", mWidth);
    format->setInt32("slice-height", mHeight);
    format->setInt32("frame-rate", mFrameRate);
    format->setInt32("bitrate", mBitrateBps);

    LOGI("Encoder:%p configuring %dx%d @ %d fps, rate %d bps", this, mWidth,
         mHeight, mFrameRate, mBitrateBps);
    if (mEncoder->Configure(format) != android::OK) {
      LOGE("Encoder:%p failed to configure", this);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    mOMXConfigured = true;
#ifdef OMX_IDR_NEEDED_FOR_BITRATE
    mLastIDRTime = TimeStamp::Now();
    mBitrateAtLastIDR = mBitrateBps;
#endif
  }

  if (aFrameTypes && aFrameTypes->size() &&
      ((*aFrameTypes)[0] == webrtc::VideoFrameType::kVideoFrameKey)) {
    mEncoder->RequestIDRFrame();
#ifdef OMX_IDR_NEEDED_FOR_BITRATE
    mLastIDRTime = TimeStamp::Now();
    mBitrateAtLastIDR = mBitrateBps;
  } else if (mBitrateBps != mBitrateAtLastIDR) {
    // 8x10 OMX codec requires a keyframe to shift bitrates!
    TimeStamp now = TimeStamp::Now();
    if (mLastIDRTime.IsNull()) {
      // paranoia
      mLastIDRTime = now;
    }
    int32_t timeSinceLastIDR = (now - mLastIDRTime).ToMilliseconds();

    // Balance asking for IDRs too often against direction and amount of bitrate
    // change.

    // HACK for bug 1014921: 8x10 has encode/decode mismatches that build up
    // errors if you go too long without an IDR.  In normal use, bitrate will
    // change often enough to never hit this time limit.
    if ((timeSinceLastIDR > 3000) ||
        (mBitrateBps < (mBitrateAtLastIDR * 8) / 10) ||
        (timeSinceLastIDR < 300 &&
         mBitrateBps < (mBitrateAtLastIDR * 9) / 10) ||
        (timeSinceLastIDR < 1000 &&
         mBitrateBps < (mBitrateAtLastIDR * 97) / 100) ||
        (timeSinceLastIDR >= 1000 && mBitrateBps < mBitrateAtLastIDR) ||
        (mBitrateBps > (mBitrateAtLastIDR * 15) / 10) ||
        (timeSinceLastIDR < 500 &&
         mBitrateBps > (mBitrateAtLastIDR * 13) / 10) ||
        (timeSinceLastIDR < 1000 &&
         mBitrateBps > (mBitrateAtLastIDR * 11) / 10) ||
        (timeSinceLastIDR >= 1000 && mBitrateBps > mBitrateAtLastIDR)) {
      LOGI(
          "Requesting IDR for bitrate change from %u to %u (time since last "
          "IDR %d ms)",
          mBitrateAtLastIDR, mBitrateBps, timeSinceLastIDR);

      mEncoder->RequestIDRFrame();
      mLastIDRTime = now;
      mBitrateAtLastIDR = mBitrateBps;
    }
#endif
  }

  if (mEncoder->Encode(aInputImage) != android::OK) {
    LOGE("Encoder:%p failed to encode", this);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkH264VideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  LOGD("Encoder:%p set callback:%p", this, aCallback);
  MOZ_ASSERT(aCallback);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGonkH264VideoEncoder::OnEncoded(
    webrtc::EncodedImage& aEncodedImage) {
  if (mCallback) {
    webrtc::CodecSpecificInfo info;
    info.codecType = webrtc::kVideoCodecH264;
    info.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;
    mCallback->OnEncodedImage(aEncodedImage, &info);
  }
}

int32_t WebrtcGonkH264VideoEncoder::Release() {
  LOGD("Encoder:%p releasing", this);

  if (mEncoder) {
    mEncoder->Release();
    mEncoder = nullptr;
    mReservation->ReleaseOMXCodec();
  }
  mOMXConfigured = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcGonkH264VideoEncoder::~WebrtcGonkH264VideoEncoder() {
  LOGD("Encoder:%p destructor", this);
  Release();
}

// TODO: Bug 997567. Find the way to support frame rate change.
void WebrtcGonkH264VideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& aParameters) {
  const double frameRate = aParameters.framerate_fps;
  const uint32_t bitrateBps = aParameters.bitrate.GetBitrate(0, 0);

  LOGI("Encoder:%p set bitrate:%u, frame rate:%f (%f)", this, bitrateBps,
       frameRate, mFrameRate);
  MOZ_ASSERT(mEncoder);

  // XXX Should use StageFright framerate change, perhaps only on major changes
  // of framerate.

  // Without Stagefright support, Algorithm should be:
  // if (frameRate < 50% of configured) {
  //   drop framerate to next step down that includes current framerate within
  //   50%
  // } else if (frameRate > configured) {
  //   change config to next step up that includes current framerate
  // }
#if !defined(TEST_OMX_FRAMERATE_CHANGES)
  if (frameRate > mFrameRate || frameRate < mFrameRate / 2) {
    uint32_t old_rate = mFrameRate;
    if (frameRate >= 15) {
      mFrameRate = 30;
    } else if (frameRate >= 10) {
      mFrameRate = 20;
    } else if (frameRate >= 8) {
      mFrameRate = 15;
    } else /* if (frameRate >= 5)*/ {
      // don't go lower; encoder may not be stable
      mFrameRate = 10;
    }
    if (mFrameRate < frameRate) {  // safety
      mFrameRate = frameRate;
    }
    if (old_rate != mFrameRate) {
      mOMXReconfigure = true;  // force re-configure on next frame
    }
  }
#else
  // XXX for testing, be wild!
  if (frameRate != mFrameRate) {
    mFrameRate = frameRate;
    mOMXReconfigure = true;  // force re-configure on next frame
  }
#endif

  mBitrateBps = bitrateBps;
  mEncoder->SetBitrate(mBitrateBps);
}

webrtc::VideoEncoder::EncoderInfo WebrtcGonkH264VideoEncoder::GetEncoderInfo()
    const {
  webrtc::VideoEncoder::EncoderInfo info;
  info.requested_resolution_alignment = 2;
  info.supports_native_handle = true;
  info.implementation_name = "Gonk";
  info.is_hardware_accelerated = true;
  info.supports_simulcast = false;
  return info;
}

// Decoder.
WebrtcGonkH264VideoDecoder::WebrtcGonkH264VideoDecoder() {
  LOGD("Decoder:%p constructor", this);
  mReservation = new android::OMXCodecReservation(false);
}

bool WebrtcGonkH264VideoDecoder::Configure(
    const webrtc::VideoDecoder::Settings& aCodecSettings) {
  LOGD("Decoder:%p initializing", this);

  if (!mReservation->ReserveOMXCodec()) {
    LOGE("Decoder:%p failed to reserve codec", this);
    return false;
  }

  mDecoder = new android::WebrtcGonkVideoDecoder();
  if (mDecoder->Init(this, android::MEDIA_MIMETYPE_VIDEO_AVC,
                     aCodecSettings.max_render_resolution().Width(),
                     aCodecSettings.max_render_resolution().Height()) !=
      android::OK) {
    mDecoder = nullptr;
    LOGE("Decoder:%p failed to initialize", this);
    return false;
  }
  return true;
}

int32_t WebrtcGonkH264VideoDecoder::Decode(
    const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
    int64_t aRenderTimeMs) {
  if (aInputImage.size() == 0 || !aInputImage.data()) {
    LOGW("Decoder:%p empty input data, dropping", this);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!mCodecConfigSubmitted) {
    using android::ABuffer;
    int32_t width, height;
    sp<ABuffer> au =
        ABuffer::CreateAsCopy(aInputImage.data(), aInputImage.size());
    sp<ABuffer> csd = android::MakeAVCCodecSpecificData(au, &width, &height);
    if (!csd) {
      LOGW("Decoder:%p missing codec config, dropping", this);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Make a copy of CSD data and inherits metadata from input image.
    auto csdData = webrtc::EncodedImageBuffer::Create(csd->data(), csd->size());
    webrtc::EncodedImage codecConfig(aInputImage);
    codecConfig.SetEncodedData(csdData);
    if (mDecoder->Decode(codecConfig, true, aRenderTimeMs) != android::OK) {
      LOGE("Decoder:%p failed to send codec config, dropping", this);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    mCodecConfigSubmitted = true;
  }

  if (mDecoder->Decode(aInputImage, false, aRenderTimeMs) != android::OK) {
    LOGE("Decoder:%p failed to decode", this);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkH264VideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  LOGD("Decoder:%p set callback:%p", this, aCallback);
  MOZ_ASSERT(aCallback);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkH264VideoDecoder::Release() {
  LOGD("Decoder:%p releasing", this);

  if (mDecoder) {
    mDecoder->Release();
    mDecoder = nullptr;
  }
  mReservation->ReleaseOMXCodec();
  mCodecConfigSubmitted = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcGonkH264VideoDecoder::~WebrtcGonkH264VideoDecoder() {
  LOGD("Decoder:%p destructor", this);
  Release();
}

}  // namespace mozilla
