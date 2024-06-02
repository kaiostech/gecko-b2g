/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGonkVideoCodec.h"

#include <media/stagefright/MediaCodecConstants.h>
#include <mediadrm/ICrypto.h>

#include "GonkMediaCodec.h"
#include "GrallocImages.h"
#include "ImageContainer.h"
#include "mozilla/layers/TextureClient.h"
#include "TimeUnits.h"
#include "WebrtcImageBuffer.h"

#include "api/video_codecs/h264_profile_level_id.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace android {

#undef LOG
#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sCodecLog("WebrtcGonkVideoCodec");
#define LOGE(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Info, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOGV(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

static std::string GenerateLogTag(void* aOwner, const std::string& aName,
                                  bool aEncoder) {
  std::stringstream ss;
  ss << aOwner << " [" << aName << "/" << (aEncoder ? "Enc" : "Dec") << "]";
  return ss.str();
}

static const char* GetMimeType(webrtc::VideoCodecType aCodecType) {
  switch (aCodecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      return MEDIA_MIMETYPE_VIDEO_VP8;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      return MEDIA_MIMETYPE_VIDEO_VP9;
    case webrtc::VideoCodecType::kVideoCodecAV1:
      return MEDIA_MIMETYPE_VIDEO_AV1;
    case webrtc::VideoCodecType::kVideoCodecH264:
      return MEDIA_MIMETYPE_VIDEO_AVC;
    case webrtc::VideoCodecType::kVideoCodecH265:
      return MEDIA_MIMETYPE_VIDEO_HEVC;
    default:
      return "unknown";
  }
}

// Return key frame interval in seconds.
static float GetKeyFrameInterval(const webrtc::VideoCodec* aCodecSettings) {
  int32_t keyFrameInterval = -1;
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      keyFrameInterval = aCodecSettings->H264().keyFrameInterval;
      break;
    case webrtc::VideoCodecType::kVideoCodecVP8:
      keyFrameInterval = aCodecSettings->VP8().keyFrameInterval;
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      keyFrameInterval = aCodecSettings->VP9().keyFrameInterval;
      break;
    default:
      break;
  }
  if (keyFrameInterval > 0 && aCodecSettings->maxFramerate > 0) {
    return static_cast<float>(keyFrameInterval) / aCodecSettings->maxFramerate;
  }
  return 10.f;  // default interval
}

static std::pair<int32_t, int32_t> GetAVCProfileLevel(
    const webrtc::SdpVideoFormat::Parameters& aParameters) {
  // TODO: Eveluate if there is a better default setting.
  auto value = std::make_pair(AVCProfileMain, AVCLevel31);

  if (auto profileLevel = webrtc::ParseSdpForH264ProfileLevelId(aParameters)) {
    switch (profileLevel->profile) {
      case webrtc::H264Profile::kProfileConstrainedBaseline:
        value.first = AVCProfileConstrainedBaseline;
        break;
      case webrtc::H264Profile::kProfileBaseline:
        value.first = AVCProfileBaseline;
        break;
      case webrtc::H264Profile::kProfileMain:
        value.first = AVCProfileMain;
        break;
      case webrtc::H264Profile::kProfileConstrainedHigh:
        value.first = AVCProfileConstrainedHigh;
        break;
      case webrtc::H264Profile::kProfileHigh:
        value.first = AVCProfileHigh;
        break;
      case webrtc::H264Profile::kProfilePredictiveHigh444:
        value.first = AVCProfileHigh444;
        break;
      default:
        break;
    }
    switch (profileLevel->level) {
      case webrtc::H264Level::kLevel1_b:
        value.second = AVCLevel1b;
        break;
      case webrtc::H264Level::kLevel1:
        value.second = AVCLevel1;
        break;
      case webrtc::H264Level::kLevel1_1:
        value.second = AVCLevel11;
        break;
      case webrtc::H264Level::kLevel1_2:
        value.second = AVCLevel12;
        break;
      case webrtc::H264Level::kLevel1_3:
        value.second = AVCLevel13;
        break;
      case webrtc::H264Level::kLevel2:
        value.second = AVCLevel2;
        break;
      case webrtc::H264Level::kLevel2_1:
        value.second = AVCLevel21;
        break;
      case webrtc::H264Level::kLevel2_2:
        value.second = AVCLevel22;
        break;
      case webrtc::H264Level::kLevel3:
        value.second = AVCLevel3;
        break;
      case webrtc::H264Level::kLevel3_1:
        value.second = AVCLevel31;
        break;
      case webrtc::H264Level::kLevel3_2:
        value.second = AVCLevel32;
        break;
      case webrtc::H264Level::kLevel4:
        value.second = AVCLevel4;
        break;
      case webrtc::H264Level::kLevel4_1:
        value.second = AVCLevel41;
        break;
      case webrtc::H264Level::kLevel4_2:
        value.second = AVCLevel42;
        break;
      case webrtc::H264Level::kLevel5:
        value.second = AVCLevel5;
        break;
      case webrtc::H264Level::kLevel5_1:
        value.second = AVCLevel51;
        break;
      case webrtc::H264Level::kLevel5_2:
        value.second = AVCLevel52;
        break;
      default:
        break;
    }
  }
  return value;
}

static void InitCodecSpecficInfo(
    webrtc::CodecSpecificInfo& aInfo, const webrtc::VideoCodec* aCodecSettings,
    const webrtc::SdpVideoFormat::Parameters& aParameters) {
  aInfo.codecType = aCodecSettings->codecType;
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      aInfo.codecSpecific.H264.packetization_mode =
          aParameters.count(cricket::kH264FmtpPacketizationMode) == 1 &&
                  aParameters.at(cricket::kH264FmtpPacketizationMode) == "1"
              ? webrtc::H264PacketizationMode::NonInterleaved
              : webrtc::H264PacketizationMode::SingleNalUnit;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      MOZ_ASSERT(aCodecSettings->VP9().numberOfSpatialLayers == 1);
      aInfo.codecSpecific.VP9.flexible_mode =
          aCodecSettings->VP9().flexibleMode;
      aInfo.codecSpecific.VP9.first_frame_in_picture = true;
      break;
    }
    default:
      break;
  }
}

static void UpdateCodecSpecificInfo(webrtc::CodecSpecificInfo& aInfo,
                                    webrtc::EncodedImage& aImage) {
  bool isKeyFrame =
      aImage.FrameType() == webrtc::VideoFrameType::kVideoFrameKey;
  switch (aInfo.codecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8: {
      // See webrtc::VP8EncoderImpl::PopulateCodecSpecific().
      webrtc::CodecSpecificInfoVP8& vp8 = aInfo.codecSpecific.VP8;
      vp8.keyIdx = webrtc::kNoKeyIdx;
      // Cannot be 100% sure unless parsing significant portion of the
      // bitstream. Treat all frames as referenced just to be safe.
      vp8.nonReference = false;
      // One temporal layer only.
      vp8.temporalIdx = webrtc::kNoTemporalIdx;
      vp8.layerSync = false;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      // See webrtc::VP9EncoderImpl::PopulateCodecSpecific().
      webrtc::CodecSpecificInfoVP9& vp9 = aInfo.codecSpecific.VP9;
      vp9.inter_pic_predicted = !isKeyFrame;
      vp9.ss_data_available = isKeyFrame && !vp9.flexible_mode;
      // One temporal & spatial layer only.
      vp9.temporal_idx = webrtc::kNoTemporalIdx;
      vp9.temporal_up_switch = false;
      vp9.num_spatial_layers = 1;
      aInfo.end_of_picture = true;
      vp9.gof_idx = webrtc::kNoGofIdx;
      vp9.width[0] = aImage._encodedWidth;
      vp9.height[0] = aImage._encodedHeight;
      break;
    }
    default:
      break;
  }
}

static void GetVPXQp(const webrtc::VideoCodecType aType,
                     webrtc::EncodedImage& aImage) {
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      webrtc::vp8::GetQp(aImage.data(), aImage.size(), &(aImage.qp_));
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      webrtc::vp9::GetQp(aImage.data(), aImage.size(), &(aImage.qp_));
      break;
    default:
      break;
  }
}

/* static */
WebrtcGonkVideoEncoder* WebrtcGonkVideoEncoder::Create(
    const webrtc::SdpVideoFormat& aFormat) {
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);
  if (GonkMediaUtils::FindMatchingCodecs(GetMimeType(type), true, false)
          .empty()) {
    return nullptr;
  }
  return new WebrtcGonkVideoEncoder(aFormat);
}

WebrtcGonkVideoEncoder::WebrtcGonkVideoEncoder(
    const webrtc::SdpVideoFormat& aFormat)
    : mLogTag(GenerateLogTag(this, aFormat.name, true)),
      mFormatParams(aFormat.parameters),
      mBitrateAdjuster(0.5, 0.95) {
  LOGD("%s constructor", LogTag());
  mozilla::PodZero(&mCodecSpecific.codecSpecific);
}

WebrtcGonkVideoEncoder::~WebrtcGonkVideoEncoder() {
  LOGD("%s destructor", LogTag());
  if (mCodec) {
    Release();
  }
}

int32_t WebrtcGonkVideoEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings,
    const webrtc::VideoEncoder::Settings& aSettings) {
  using CodecCallback = GonkMediaCodec::CallbackProxy<WebrtcGonkVideoEncoder>;

  MOZ_ASSERT(aCodecSettings);
  LOGD("%s initializing", LogTag());

  if (aCodecSettings->numberOfSimulcastStreams > 1) {
    LOGW("%s falling back to simulcast adaptor", LogTag());
    return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
  }

  if (aCodecSettings->codecType == webrtc::VideoCodecType::kVideoCodecH264 &&
      !(mFormatParams.count(cricket::kH264FmtpPacketizationMode) == 1 &&
        mFormatParams.at(cricket::kH264FmtpPacketizationMode) == "1")) {
    LOGW("%s setting max output size is not supported, falling back to SW",
         LogTag());
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  if (mCodec) {
    LOGW("%s codec already exists, release it now", LogTag());
    Release();
  }

  InitCodecSpecficInfo(mCodecSpecific, aCodecSettings, mFormatParams);
  mMaxBitrateBps = aCodecSettings->maxBitrate * 1000;
  mMinBitrateBps = aCodecSettings->minBitrate * 1000;
  mBitrateAdjuster.SetTargetBitrateBps(aCodecSettings->startBitrate * 1000);

  mCodec = new GonkMediaCodec();
  mCodec->Init();

  sp<AMessage> format = new AMessage;
  format->setString("mime", GetMimeType(aCodecSettings->codecType));
  format->setInt32("color-format", COLOR_FormatYUV420SemiPlanar);
  format->setInt32("width", aCodecSettings->width);
  format->setInt32("height", aCodecSettings->height);
  format->setInt32("frame-rate", aCodecSettings->maxFramerate);
  format->setFloat("i-frame-interval", GetKeyFrameInterval(aCodecSettings));
  format->setInt32("bitrate", mBitrateAdjuster.GetTargetBitrateBps());
  format->setInt32("bitrate-mode", BITRATE_MODE_CBR);

  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      auto [profile, level] = GetAVCProfileLevel(mFormatParams);
      format->setInt32("profile", profile);
      format->setInt32("level", level);
      format->setInt32("prepend-sps-pps-to-idr-frames", 1);
      break;
    }
    default:
      break;
  }

  status_t err =
      mCodec->Configure(CodecCallback::Create(this), format, nullptr, true)
          ->Wait();
  if (err != OK) {
    LOGE("%s failed to configure codec", LogTag());
    Release();
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
  LOGD("%s initialized", LogTag());
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  LOGD("%s register callback %p", LogTag(), aCallback);
  Mutex::Autolock lock(mCallbackMutex);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkVideoEncoder::Encode(
    const webrtc::VideoFrame& aInputFrame,
    const std::vector<webrtc::VideoFrameType>* aFrameTypes) {
  LOGV("%s encoding, %dx%d, RTP timestamp %u", LogTag(), aInputFrame.width(),
       aInputFrame.height(), aInputFrame.timestamp());

  if (!aInputFrame.size() || !aInputFrame.video_frame_buffer() ||
      aFrameTypes->empty()) {
    LOGE("%s input frame is empty, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (!mCodec) {
    LOGE("%s codec was not created, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!mCallback) {
    LOGE("%s callback was not registered, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (mError != OK) {
    LOGE("%s error occurred, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInputQueue.Push(new InputHolder(aInputFrame, aFrameTypes->at(0)));
  mCodec->InputUpdated();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkVideoEncoder::Release() {
  LOGD("%s releasing", LogTag());
  if (mCodec) {
    mCodec->Shutdown()->Wait();
    mCodec = nullptr;
  }
  mCallback = nullptr;
  mError = OK;
  mTimestampUnwrapper.Reset();
  mInputQueue.Clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGonkVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& aParameters) {
  if (!aParameters.bitrate.HasBitrate(0, 0)) {
    LOGE("%s no bitrate value to set", LogTag());
    return;
  }
  if (!mCodec) {
    LOGE("%s no codec to set bitrate", LogTag());
    return;
  }
  if (mError != OK) {
    LOGE("%s error occurred, ignoring setting bitrate", LogTag());
    return;
  }

  const uint32_t newBitrateBps = aParameters.bitrate.GetBitrate(0, 0);
  if (newBitrateBps < mMinBitrateBps || newBitrateBps > mMaxBitrateBps) {
    LOGE("%s bitrate value out of range", LogTag());
    return;
  }
  if (mBitrateAdjuster.GetAdjustedBitrateBps() == newBitrateBps) {
    return;
  }

  mBitrateAdjuster.SetTargetBitrateBps(newBitrateBps);
  LOGV("%s setting bitrate %u (%u ~ %u)", LogTag(), newBitrateBps,
       mMinBitrateBps, mMaxBitrateBps);

  sp<AMessage> msg = new AMessage();
  msg->setInt32("video-bitrate", newBitrateBps);
  mCodec->SetParameters(msg)->Wait();
}

webrtc::VideoEncoder::EncoderInfo WebrtcGonkVideoEncoder::GetEncoderInfo()
    const {
  webrtc::VideoEncoder::EncoderInfo info;
  info.requested_resolution_alignment = 2;
  info.supports_native_handle = true;
  info.implementation_name = "Gonk";
  info.is_hardware_accelerated = true;
  info.supports_simulcast = false;
  return info;
}

bool WebrtcGonkVideoEncoder::FetchInput(const sp<MediaCodecBuffer>& aBuffer,
                                        sp<RefBase>* aInputInfo,
                                        sp<GonkCryptoInfo>* aCryptoInfo,
                                        int64_t* aTimeUs, uint32_t* aFlags) {
  auto inputHolder = mInputQueue.Pop();
  if (!inputHolder) {
    return false;
  }

  auto& inputFrame = inputHolder->mFrame;
  auto frameType = inputHolder->mFrameType;
  GonkImageUtils::CopyImage(inputFrame.video_frame_buffer(), aBuffer);
  *aInputInfo = inputHolder;
  *aTimeUs = mozilla::media::TimeUnit(
                 mTimestampUnwrapper.Unwrap(inputFrame.timestamp()),
                 cricket::kVideoCodecClockrate)
                 .ToMicroseconds();
  *aFlags = frameType == webrtc::VideoFrameType::kVideoFrameKey
                ? MediaCodec::BUFFER_FLAG_SYNCFRAME
                : 0;
  LOGV("%s fetch input, flags %u, RTP timestamp %u (%" PRId64 " us)", LogTag(),
       *aFlags, inputFrame.timestamp(), *aTimeUs);
  return true;
}

void WebrtcGonkVideoEncoder::Output(const sp<MediaCodecBuffer>& aBuffer,
                                    const sp<RefBase>& aInputInfo,
                                    int64_t aTimeUs, uint32_t aFlags) {
  if (!aBuffer || !aBuffer->size()) {
    LOGW("%s empty output buffer, flags %u", LogTag(), aFlags);
    return;
  }
  if (!aInputInfo) {
    if (aFlags == MediaCodec::BUFFER_FLAG_CODECCONFIG) {
      // We asked the encoder to prepend SPS/PPS to IDR frames, the standalone
      // SPS/PPS can be ignored.
      LOGD("%s ignoring standalone coded config", LogTag());
    } else {
      LOGW("%s null input info, flags %u", LogTag(), aFlags);
    }
    return;
  }

  auto& inputFrame = static_cast<InputHolder*>(aInputInfo.get())->mFrame;
  LOGV("%s output, size %zu, flags %u, RTP timestamp %u (%" PRId64 " us)",
       LogTag(), aBuffer->size(), aFlags, inputFrame.timestamp(), aTimeUs);

  webrtc::EncodedImage image;
  image.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(aBuffer->data(), aBuffer->size()));
  image._encodedWidth = inputFrame.width();
  image._encodedHeight = inputFrame.height();
  image.SetRtpTimestamp(inputFrame.timestamp());
  image.SetFrameType((aFlags & MediaCodec::BUFFER_FLAG_SYNCFRAME)
                         ? webrtc::VideoFrameType::kVideoFrameKey
                         : webrtc::VideoFrameType::kVideoFrameDelta);
  GetVPXQp(mCodecSpecific.codecType, image);
  UpdateCodecSpecificInfo(mCodecSpecific, image);
  mBitrateAdjuster.Update(image.size());

  Mutex::Autolock lock(mCallbackMutex);
  if (mCallback) {
    mCallback->OnEncodedImage(image, &mCodecSpecific);
  } else {
    LOGW("%s callback was not registered, dropping output", LogTag());
  }
}

void WebrtcGonkVideoEncoder::NotifyError(status_t aErr, int32_t aActionCode) {
  LOGE("%s notify error: 0x%x, actionCode %d", LogTag(), aErr, aActionCode);
  // Treat ACTION_CODE_RECOVERABLE as fatal error, because the codec can only be
  // recovered with reconfiguring. Only ACTION_CODE_TRANSIENT allows
  // decoding/encoding to be retried at later time.
  if (aActionCode != ACTION_CODE_TRANSIENT) {
    mError = aErr;
  }
}

/* static */
WebrtcGonkVideoDecoder* WebrtcGonkVideoDecoder::Create(
    const webrtc::SdpVideoFormat& aFormat) {
  auto type = webrtc::PayloadStringToCodecType(aFormat.name);
  if (GonkMediaUtils::FindMatchingCodecs(GetMimeType(type), false, false)
          .empty()) {
    return nullptr;
  }
  return new WebrtcGonkVideoDecoder(aFormat);
}

WebrtcGonkVideoDecoder::WebrtcGonkVideoDecoder(
    const webrtc::SdpVideoFormat& aFormat)
    : mLogTag(GenerateLogTag(this, aFormat.name, false)),
      mFormatParams(aFormat.parameters) {}

WebrtcGonkVideoDecoder::~WebrtcGonkVideoDecoder() {
  LOGD("%s destructor", LogTag());
  if (mCodec) {
    Release();
  }
}

bool WebrtcGonkVideoDecoder::Configure(
    const webrtc::VideoDecoder::Settings& aSettings) {
  using CodecCallback = GonkMediaCodec::CallbackProxy<WebrtcGonkVideoDecoder>;

  LOGD("%s initializing", LogTag());
  if (mCodec) {
    LOGW("%s codec already exists, release it now", LogTag());
    Release();
  }

  mImageContainer =
      mozilla::MakeAndAddRef<ImageContainer>(ImageContainer::ASYNCHRONOUS);
  mCodec = new GonkMediaCodec();
  mCodec->Init();

  sp<AMessage> format = new AMessage;
  format->setString("mime", GetMimeType(aSettings.codec_type()));
  format->setInt32("width", aSettings.max_render_resolution().Width());
  format->setInt32("height", aSettings.max_render_resolution().Height());
#if ANDROID_VERSION >= 30
  format->setInt32("low-latency", 1);
#endif

  status_t err =
      mCodec->Configure(CodecCallback::Create(this), format, nullptr, false)
          ->Wait();
  if (err != OK) {
    LOGE("%s failed to configure codec", LogTag());
    Release();
    return false;
  }
  LOGD("%s initialized", LogTag());
  return true;
}

int32_t WebrtcGonkVideoDecoder::Decode(const webrtc::EncodedImage& aInputImage,
                                       bool aMissingFrames,
                                       int64_t aRenderTimeMs) {
  LOGV("%s decoding, RTP timestamp %u", LogTag(), aInputImage.RtpTimestamp());

  if (!aInputImage.data() || !aInputImage.size()) {
    LOGE("%s input image is empty, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Always start with a complete key frame.
  if (mNeedKeyframe) {
    if (aInputImage._frameType != webrtc::VideoFrameType::kVideoFrameKey) {
      LOGD("%s waiting for key frame, dropping", LogTag());
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    mNeedKeyframe = false;
  }
  if (!mCodec) {
    LOGE("%s codec was not created, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!mCallback) {
    LOGE("%s callback was not registered, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (mError != OK) {
    LOGE("%s error occurred, dropping", LogTag());
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInputQueue.Push(InputHolder::Create(aInputImage));
  mCodec->InputUpdated();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  LOGD("%s register callback %p", LogTag(), aCallback);
  Mutex::Autolock lock(mCallbackMutex);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGonkVideoDecoder::Release() {
  LOGD("%s releasing", LogTag());
  if (mCodec) {
    mCodec->Shutdown()->Wait();
    mCodec = nullptr;
  }
  mCallback = nullptr;
  mError = OK;
  mNeedKeyframe = true;
  mTimestampUnwrapper.Reset();
  mInputQueue.Clear();
  mImageContainer = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

webrtc::VideoDecoder::DecoderInfo WebrtcGonkVideoDecoder::GetDecoderInfo()
    const {
  webrtc::VideoDecoder::DecoderInfo info;
  info.implementation_name = ImplementationName();
  info.is_hardware_accelerated = true;
  return info;
}

bool WebrtcGonkVideoDecoder::FetchInput(const sp<MediaCodecBuffer>& aBuffer,
                                        sp<RefBase>* aInputInfo,
                                        sp<GonkCryptoInfo>* aCryptoInfo,
                                        int64_t* aTimeUs, uint32_t* aFlags) {
  auto inputHolder = mInputQueue.Pop();
  if (!inputHolder) {
    return false;
  }

  auto& inputImage = inputHolder->Get();
  GonkBufferWriter writer(aBuffer);
  writer.Clear();
  if (!writer.Append(inputImage.data(), inputImage.size())) {
    LOGE("%s input sample too large", LogTag());
    return false;
  }

  *aInputInfo = inputHolder;
  *aTimeUs = mozilla::media::TimeUnit(
                 mTimestampUnwrapper.Unwrap(inputImage.RtpTimestamp()),
                 cricket::kVideoCodecClockrate)
                 .ToMicroseconds();
  *aFlags = 0;
  LOGV("%s fetch input, RTP timestamp %u (%" PRId64 " us)", LogTag(),
       inputImage.RtpTimestamp(), *aTimeUs);
  return true;
}

void WebrtcGonkVideoDecoder::Output(const sp<MediaCodecBuffer>& aBuffer,
                                    const sp<RefBase>& aInputInfo,
                                    int64_t aTimeUs, uint32_t aFlags) {
  if (!aBuffer || !aBuffer->size()) {
    LOGW("%s empty output buffer, flags %u", LogTag(), aFlags);
    return;
  }
  if (!aInputInfo) {
    LOGW("%s null input info, flags %u", LogTag(), aFlags);
    return;
  }

  auto& inputImage = static_cast<InputHolder*>(aInputInfo.get())->Get();
  LOGV("%s output, flags %u, RTP timestamp %u (%" PRId64 " us)", LogTag(),
       aFlags, inputImage.RtpTimestamp(), aTimeUs);

  auto image = GonkImageUtils::CreateI420ImageCopy(mImageContainer, aBuffer);
  if (!image) {
    LOGE("%s failed to create I420 image", LogTag());
    return;
  }

  auto videoFrame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(
              rtc::make_ref_counted<mozilla::ImageBuffer>(std::move(image)))
          .set_timestamp_rtp(inputImage.RtpTimestamp())
          .set_rotation(inputImage.rotation_)
          .build();

  Mutex::Autolock lock(mCallbackMutex);
  if (mCallback) {
    mCallback->Decoded(videoFrame);
  } else {
    LOGW("%s callback was not registered, dropping output", LogTag());
  }
}

void WebrtcGonkVideoDecoder::OutputTexture(TextureClient* aTexture,
                                           const sp<RefBase>& aInputInfo,
                                           int64_t aTimeUs, uint32_t aFlags) {
  if (!aTexture) {
    LOGW("%s null output texture, flags %u", LogTag(), aFlags);
    return;
  }
  if (!aInputInfo) {
    LOGW("%s null input info, flags %u", LogTag(), aFlags);
    return;
  }

  auto& inputImage = static_cast<InputHolder*>(aInputInfo.get())->Get();
  LOGV("%s output texture #%" PRIu64 ", flags %u, RTP timestamp %u (%" PRId64
       " us)",
       LogTag(), aTexture->GetSerial(), aFlags, inputImage.RtpTimestamp(),
       aTimeUs);

  auto image = mozilla::MakeRefPtr<mozilla::layers::GrallocImage>();
  image->AdoptData(aTexture, aTexture->GetSize());

  auto videoFrame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(
              rtc::make_ref_counted<mozilla::ImageBuffer>(std::move(image)))
          .set_timestamp_rtp(inputImage.RtpTimestamp())
          .set_rotation(inputImage.rotation_)
          .build();

  Mutex::Autolock lock(mCallbackMutex);
  if (mCallback) {
    mCallback->Decoded(videoFrame);
  } else {
    LOGW("%s callback was not registered, dropping output", LogTag());
  }
}

void WebrtcGonkVideoDecoder::NotifyError(status_t aErr, int32_t aActionCode) {
  LOGE("%s notify error: 0x%x, actionCode %d", LogTag(), aErr, aActionCode);
  // Treat ACTION_CODE_RECOVERABLE as fatal error, because the codec can only be
  // recovered with reconfiguring. Only ACTION_CODE_TRANSIENT allows
  // decoding/encoding to be retried at later time.
  if (aActionCode != ACTION_CODE_TRANSIENT) {
    mError = aErr;
  }
}

}  // namespace android
