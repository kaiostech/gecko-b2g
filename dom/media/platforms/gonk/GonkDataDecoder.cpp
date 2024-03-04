/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDataDecoder.h"

#include <media/hardware/VideoAPI.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <mediadrm/ICrypto.h>

#include "GonkMediaCodec.h"
#include "GonkMediaUtils.h"
#include "ImageContainer.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/StaticPrefs_media.h"

#ifdef B2G_MEDIADRM
#  include "mozilla/GonkDrmCDMProxy.h"
#endif

using android::ABuffer;
using android::AMessage;
using android::AString;
using android::GonkBufferWriter;
using android::GonkCryptoInfo;
using android::GonkMediaCodec;
using android::GonkMediaUtils;
using android::ICrypto;
using android::MediaCodec;
using android::MediaCodecBuffer;
using android::RefBase;
using android::sp;
using android::status_t;

namespace mozilla {

#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sCodecLog("GonkDataDecoder");
#define LOGE(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Info, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOGV(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

class GonkDataDecoder::CodecReplyDispatcher {
 public:
  using Callable = GonkMediaCodec::Reply::Callable;

  CodecReplyDispatcher(nsISerialEventTarget* aThread, Callable&& aCallable)
      : mThread(aThread), mCallable(std::move(aCallable)) {}

  void operator()(status_t aErr) {
    nsresult rv = mThread->Dispatch(NS_NewRunnableFunction(
        "CodecReplyDispatcher::operator()",
        [callable = mCallable, aErr]() { callable(aErr); }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 private:
  nsCOMPtr<nsISerialEventTarget> mThread;
  Callable mCallable;
};

GonkDataDecoder::GonkDataDecoder(const CreateDecoderParams& aParams,
                                 CDMProxy* aProxy)
    : mConfig(aParams.mConfig.Clone()),
      mImageContainer(aParams.mImageContainer),
      mImageAllocator(aParams.mKnowsCompositor),
      mCDMProxy(aProxy),
      mAudioCompactor(mOutputQueue) {
  LOGD("%p constructor", this);
}

GonkDataDecoder::~GonkDataDecoder() { LOGD("%p destructor", this); }

RefPtr<GonkDataDecoder::InitPromise> GonkDataDecoder::Init() {
  using CodecCallback = GonkMediaCodec::CallbackProxy<GonkDataDecoder>;

  LOGD("%p initializing", this);
  if (mCodec) {
    LOGE("%p already initialized", this);
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                        __func__);
  }
  if (!mInitPromise.IsEmpty()) {
    LOGE("%p already initializing", this);
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                        __func__);
  }

  mThread = GetCurrentSerialEventTarget();
  mPushListener = mOutputQueue.PushEvent().Connect(
      mThread, this, &GonkDataDecoder::OutputPushed);
  mFinishListener = mOutputQueue.FinishEvent().Connect(
      mThread, this, &GonkDataDecoder::OutputFinished);

  mCodec = new GonkMediaCodec();
  mCodec->Init();

  mCodec
      ->Configure(CodecCallback::Create(this),
                  GonkMediaUtils::GetMediaCodecConfig(mConfig.get()),
                  GetCrypto(), false)
      ->Then(CodecReplyDispatcher(mThread, [self = Self(),
                                            this](status_t aErr) {
        if (aErr == android::OK) {
          LOGD("%p configure completed", this);
          mInitPromise.ResolveIfExists(mConfig->GetType(), __func__);
        } else {
          LOGE("%p configure failed", this);
          mCodec = nullptr;
          mInputQueue.Reset();
          mOutputQueue.Reset();
          mPushListener.Disconnect();
          mFinishListener.Disconnect();
          mThread = nullptr;
          mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
        }
      }));
  return mInitPromise.Ensure(__func__);
}

RefPtr<ShutdownPromise> GonkDataDecoder::Shutdown() {
  LOGD("%p shutting down", this);
  if (!mCodec) {
    LOGE("%p not initialized", this);
    return ShutdownPromise::CreateAndReject(false, __func__);
  }
  if (!mShutdownPromise.IsEmpty()) {
    LOGE("%p already shutting down", this);
    return ShutdownPromise::CreateAndReject(false, __func__);
  }

  mCodec->Shutdown()->Then(
      CodecReplyDispatcher(mThread, [self = Self(), this](status_t aErr) {
        LOGD("%p shut down completed", this);
        mCodec = nullptr;
        mInputQueue.Reset();
        mOutputQueue.Reset();
        mPushListener.Disconnect();
        mFinishListener.Disconnect();
        mThread = nullptr;
        mShutdownPromise.ResolveIfExists(true, __func__);
      }));
  return mShutdownPromise.Ensure(__func__);
}

RefPtr<GonkDataDecoder::DecodePromise> GonkDataDecoder::Decode(
    MediaRawData* aSample) {
  LOGV("%p decoding, timestamp %" PRId64, this,
       aSample->mTime.ToMicroseconds());
  if (!mCodec) {
    LOGE("%p not initialized", this);
    return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                          __func__);
  }

  mInputQueue.Push(aSample);
  mCodec->InputUpdated();

  // 1. If the input queue level is low, resolve promise immediately to request
  //    more samples.
  // 2. If there are pending output buffers, return them immediately to avoid
  //    being late for rendering.
  if (mInputQueue.GetSize() < 2 || mOutputQueue.GetSize() > 0) {
    return DecodePromise::CreateAndResolve(FetchOutput(), __func__);
  }
  return mDecodePromise.Ensure(__func__);
}

RefPtr<GonkDataDecoder::DecodePromise> GonkDataDecoder::Drain() {
  LOGD("%p draining", this);
  if (!mCodec) {
    LOGE("%p not initialized", this);
    return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                          __func__);
  }

  mInputQueue.Finish();
  mCodec->InputUpdated();
  nsresult rv = mThread->Dispatch(NS_NewRunnableFunction(
      "GonkDataDecoder::Drain", [self = Self(), this]() { OutputUpdated(); }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
  return mDrainPromise.Ensure(__func__);
}

RefPtr<GonkDataDecoder::FlushPromise> GonkDataDecoder::Flush() {
  LOGD("%p flushing", this);
  if (!mCodec) {
    LOGE("%p not initialized", this);
    return FlushPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                         __func__);
  }
  if (!mFlushPromise.IsEmpty()) {
    LOGE("%p already flushing", this);
    return FlushPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                         __func__);
  }

  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mInputQueue.Reset();

  mCodec->Flush()->Then(
      CodecReplyDispatcher(mThread, [self = Self(), this](status_t aErr) {
        LOGD("%p flush completed", this);
        mOutputQueue.Reset();
        mFlushPromise.ResolveIfExists(true, __func__);
      }));
  return mFlushPromise.Ensure(__func__);
}

sp<ICrypto> GonkDataDecoder::GetCrypto() {
#ifdef B2G_MEDIADRM
  if (mCDMProxy) {
    return static_cast<GonkDrmCDMProxy*>(mCDMProxy.get())->CreateCrypto();
  }
#endif
  return nullptr;
}

MediaDataDecoder::DecodedData GonkDataDecoder::FetchOutput() {
  DecodedData data;
  while (mOutputQueue.GetSize() > 0) {
    *data.AppendElement() = mOutputQueue.PopFront();
  }
  return data;
}

void GonkDataDecoder::OutputUpdated() {
  if (!mDecodePromise.IsEmpty()) {
    mDecodePromise.Resolve(FetchOutput(), __func__);
  }
  if (!mDrainPromise.IsEmpty()) {
    if (mOutputQueue.GetSize() > 0) {
      // Partially drained.
      mDrainPromise.Resolve(FetchOutput(), __func__);
    } else if (mOutputQueue.AtEndOfStream()) {
      LOGD("%p drained", this);
      mDrainPromise.Resolve(DecodedData(), __func__);
    }
  }
}

struct SampleInfo final : public RefBase {
  bool mKeyframe = false;
  int64_t mOffset = -1;
  media::TimeUnit mTime;
  media::TimeUnit mTimecode;
  media::TimeUnit mDuration;

  explicit SampleInfo(const MediaRawData* aSample)
      : mKeyframe(aSample->mKeyframe),
        mOffset(aSample->mOffset),
        mTime(aSample->mTime),
        mTimecode(aSample->mTimecode),
        mDuration(aSample->mDuration) {}
};

bool GonkDataDecoder::FetchInput(const sp<MediaCodecBuffer>& aBuffer,
                                 sp<RefBase>* aInputInfo,
                                 sp<GonkCryptoInfo>* aCryptoInfo,
                                 int64_t* aTimeUs, uint32_t* aFlags) {
  if (mInputQueue.AtEndOfStream()) {
    aBuffer->setRange(0, 0);
    *aFlags = MediaCodec::BUFFER_FLAG_EOS;
    LOGD("%p fetch input EOS", this);
    return true;
  }
  if (mInputQueue.GetSize() == 0) {
    nsresult rv = mThread->Dispatch(NS_NewRunnableFunction(
        "GonkDataDecoder::FetchInput", [self = Self(), this]() {
          if (!mDecodePromise.IsEmpty()) {
            LOGV("%p request more input", this);
            mDecodePromise.ResolveIfExists(FetchOutput(), __func__);
          }
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
    return false;
  }

  RefPtr<MediaRawData> sample = mInputQueue.PopFront();
  GonkBufferWriter writer(aBuffer);
  writer.Clear();
  if (!writer.Append(sample->Data(), sample->Size())) {
    LOGE("%p input sample too large", this);
    return false;
  }

  // AOSP Vorbis decoder expects "valid samples" to be appended to the data.
  if (mConfig->mMimeType.EqualsLiteral("audio/vorbis")) {
    auto duration = sample->mDuration;
    auto rate = mConfig->GetAsAudioInfo()->mRate;
    int32_t validSamples =
        duration.IsValid() ? duration.ToTicksAtRate(rate) : -1;
    if (!writer.Append(reinterpret_cast<uint8_t*>(&validSamples),
                       sizeof(int32_t))) {
      LOGE("%p unable to append valid-samples", this);
      return false;
    }
  }

  *aInputInfo = new SampleInfo(sample);
  *aTimeUs = sample->mTime.ToMicroseconds();

#ifdef B2G_MEDIADRM
  *aCryptoInfo = GonkMediaUtils::GetCryptoInfo(sample);
#endif
  LOGV("%p fetch input %" PRId64, this, *aTimeUs);
  return true;
};

void GonkDataDecoder::Output(const sp<MediaCodecBuffer>& aBuffer,
                             const sp<RefBase>& aInputInfo, int64_t aTimeUs,
                             uint32_t aFlags) {
  using android::MediaImage2;

  auto checkEosOnExit = MakeScopeExit([this, aFlags]() {
    if (aFlags & MediaCodec::BUFFER_FLAG_EOS) {
      LOGD("%p saw output EOS", this);
      mOutputQueue.Finish();
    }
  });

  if (!aBuffer || aBuffer->size() == 0) {
    // GonkMediaCodec may notify EOS with an empty buffer.
    return;
  }

  LOGV("%p output timestamp %" PRId64, this, aTimeUs);

  if (!aInputInfo) {
    LOGE("%p null input info", this);
    return;
  }

  sp<SampleInfo> sampleInfo = static_cast<SampleInfo*>(aInputInfo.get());
  if (IsVideo()) {
    // See the usage of image-data in MediaCodec_sanity_test.cpp.
    sp<ABuffer> imgBuf;
    if (!aBuffer->meta()->findBuffer("image-data", &imgBuf)) {
      LOGE("%p, failed to find image-data", this);
      return;
    }

    auto* img = reinterpret_cast<MediaImage2*>(imgBuf->data());
    if (img->mType != img->MEDIA_IMAGE_TYPE_YUV) {
      LOGE("%p only support YUV format", this);
      return;
    }

    CHECK_EQ(img->mWidth, static_cast<uint32_t>(mVideoOutputFormat.mWidth));
    CHECK_EQ(img->mHeight, static_cast<uint32_t>(mVideoOutputFormat.mHeight));
    CHECK_EQ(img->mPlane[img->Y].mRowInc, mVideoOutputFormat.mStride);

    VideoData::YCbCrBuffer yuv;
    const static int srcIndices[] = {MediaImage2::Y, MediaImage2::U,
                                     MediaImage2::V};
    for (int i = 0; i < 3; i++) {
      auto& dstPlane = yuv.mPlanes[i];
      auto& srcPlane = img->mPlane[srcIndices[i]];
      CHECK_GE(srcPlane.mHorizSubsampling, 1u);
      CHECK_GE(srcPlane.mVertSubsampling, 1u);
      CHECK_GE(srcPlane.mColInc, 1);
      dstPlane.mData = aBuffer->data() + srcPlane.mOffset;
      dstPlane.mWidth = img->mWidth / srcPlane.mHorizSubsampling;
      dstPlane.mHeight = img->mHeight / srcPlane.mVertSubsampling;
      dstPlane.mStride = srcPlane.mRowInc;
      dstPlane.mSkip = srcPlane.mColInc - 1;
    }

    auto uSubsampling = std::make_pair(img->mPlane[img->U].mHorizSubsampling,
                                       img->mPlane[img->U].mVertSubsampling);
    auto vSubsampling = std::make_pair(img->mPlane[img->V].mHorizSubsampling,
                                       img->mPlane[img->V].mVertSubsampling);
    CHECK(uSubsampling == vSubsampling);
    if (uSubsampling == std::make_pair(1u, 1u)) {
      yuv.mChromaSubsampling = gfx::ChromaSubsampling::FULL;
    } else if (uSubsampling == std::make_pair(2u, 1u)) {
      yuv.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH;
    } else if (uSubsampling == std::make_pair(2u, 2u)) {
      yuv.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
    } else {
      TRESPASS();
    }
    yuv.mYUVColorSpace = gfx::YUVColorSpace::Default;

    RefPtr<MediaData> data = VideoData::CreateAndCopyData(
        *mConfig->GetAsVideoInfo(), mImageContainer, sampleInfo->mOffset,
        media::TimeUnit::FromMicroseconds(aTimeUs), sampleInfo->mDuration, yuv,
        sampleInfo->mKeyframe, sampleInfo->mTimecode, mVideoOutputFormat.mCrop,
        mImageAllocator);

    mOutputQueue.Push(data);
  } else {
    auto channels = mAudioOutputFormat.mChannelCount;
    auto rate = mAudioOutputFormat.mSampleRate;
    auto encoding = mAudioOutputFormat.mPcmEncoding;
    auto* data = aBuffer->data();
    auto size = aBuffer->size();
    auto frames =
        size / (GonkMediaUtils::GetAudioSampleSize(encoding) * channels);

    CheckedInt64 duration = FramesToUsecs(frames, rate);
    if (!duration.isValid()) {
      return;
    }

    mAudioCompactor.Push(
        sampleInfo->mOffset, aTimeUs, rate, frames, channels,
        GonkMediaUtils::CreatePcmCopy(data, size, channels, encoding));
  }
}

void GonkDataDecoder::OutputTexture(layers::TextureClient* aTexture,
                                    const sp<RefBase>& aInputInfo,
                                    int64_t aTimeUs, uint32_t aFlags) {
  CHECK(IsVideo());

  auto checkEosOnExit = MakeScopeExit([this, aFlags]() {
    if (aFlags & MediaCodec::BUFFER_FLAG_EOS) {
      LOGD("%p saw output EOS", this);
      mOutputQueue.Finish();
    }
  });

  if (!aTexture) {
    // GonkMediaCodec may notify EOS with null texture.
    return;
  }

  LOGV("%p output texture #%" PRIu64 ", timestamp %" PRId64, this,
       aTexture->GetSerial(), aTimeUs);

  if (!aInputInfo) {
    LOGE("%p null input info", this);
    return;
  }

  sp<SampleInfo> sampleInfo = static_cast<SampleInfo*>(aInputInfo.get());

  RefPtr<VideoData> data = VideoData::CreateAndCopyData(
      *mConfig->GetAsVideoInfo(), mImageContainer, sampleInfo->mOffset,
      media::TimeUnit::FromMicroseconds(aTimeUs), sampleInfo->mDuration,
      aTexture, sampleInfo->mKeyframe, sampleInfo->mTimecode,
      mVideoOutputFormat.mCrop);
  mOutputQueue.Push(data);
}

void GonkDataDecoder::NotifyOutputFormat(const sp<AMessage>& aFormat) {
  if (IsVideo()) {
    int32_t width, height;
    int32_t stride, sliceHeight;
    int32_t cropLeft, cropTop, cropRight, cropBottom;
    int32_t colorFormat;

    if (!aFormat->findInt32("width", &width)) {
      LOGE("%p failed to find width", this);
      return;
    }

    if (!aFormat->findInt32("height", &height)) {
      LOGE("%p failed to find height", this);
      return;
    }

    if (!aFormat->findInt32("stride", &stride)) {
      stride = width;
    }

    if (!aFormat->findInt32("slice-height", &sliceHeight)) {
      sliceHeight = height;
    }

    if (!aFormat->findRect("crop", &cropLeft, &cropTop, &cropRight,
                           &cropBottom)) {
      LOGE("%p failed to find crop", this);
      return;
    }

    if (!aFormat->findInt32("color-format", &colorFormat)) {
      LOGE("%p failed to find color_format", this);
      return;
    }

    mVideoOutputFormat.mWidth = width;
    mVideoOutputFormat.mHeight = height;
    mVideoOutputFormat.mStride = stride;
    mVideoOutputFormat.mSliceHeight = sliceHeight;
    mVideoOutputFormat.mColorFormat = colorFormat;
    mVideoOutputFormat.mCrop = gfx::IntRect(
        cropLeft, cropTop, cropRight - cropLeft + 1, cropBottom - cropTop + 1);
    LOGI("%p video output format changed, width %d, height %d, color %d", this,
         mVideoOutputFormat.mCrop.Width(), mVideoOutputFormat.mCrop.Height(),
         colorFormat);
  } else {
    int32_t channelCount = 0;
    int32_t sampleRate = 0;
    int32_t pcmEncoding = android::kAudioEncodingPcm16bit;

    if (!aFormat->findInt32("channel-count", &channelCount)) {
      LOGE("%p failed to find channel-count", this);
      return;
    }

    if (!aFormat->findInt32("sample-rate", &sampleRate)) {
      LOGE("%p failed to find sample-rate", this);
      return;
    }

    if (!aFormat->findInt32("pcm-encoding", &pcmEncoding)) {
      LOGI("%p failed to find pcm-encoding, assume 16 bit integer", this);
    }

    mAudioOutputFormat.mChannelCount = channelCount;
    mAudioOutputFormat.mSampleRate = sampleRate;
    mAudioOutputFormat.mPcmEncoding =
        static_cast<android::AudioEncoding>(pcmEncoding);
    LOGI("%p audio output format changed, channels %d, rate %d, encoding %d",
         this, channelCount, sampleRate, pcmEncoding);
  }
}

void GonkDataDecoder::NotifyCodecDetails(const sp<AMessage>& aDetails) {
  int32_t required;
  /* atomic */ mSupportAdaptivePlayback =
      aDetails->findInt32("feature-adaptive-playback", &required);
}

void GonkDataDecoder::NotifyError(status_t aErr, int32_t aActionCode) {
  LOGE("%p notify error: 0x%x, actionCode %d", this, aErr, aActionCode);
  nsresult err = aActionCode == android::ACTION_CODE_FATAL
                     ? NS_ERROR_DOM_MEDIA_FATAL_ERR
                     : NS_ERROR_DOM_MEDIA_DECODE_ERR;
  nsresult rv = mThread->Dispatch(NS_NewRunnableFunction(
      "GonkDataDecoder::NotifyError", [self = Self(), this, err]() {
        mDecodePromise.RejectIfExists(MediaResult(err), __func__);
        mDrainPromise.RejectIfExists(MediaResult(err), __func__);
      }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

bool GonkDataDecoder::SupportDecoderRecycling() const {
  return StaticPrefs::media_gonkmediacodec_recycling_enabled() &&
         mSupportAdaptivePlayback;
}

}  // namespace mozilla
