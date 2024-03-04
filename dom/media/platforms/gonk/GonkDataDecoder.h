/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_DATA_DECODER_H
#define GONK_DATA_DECODER_H

#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/MediaDefs.h>
#include <utils/StrongPointer.h>

#include "AudioCompactor.h"
#include "MediaQueue.h"
#include "PlatformDecoderModule.h"

namespace android {
class GonkCryptoInfo;
class GonkMediaCodec;
class ICrypto;
}  // namespace android

namespace mozilla {

class CDMProxy;
class MediaData;

class GonkDataDecoder final : public MediaDataDecoder {
  using DecodePromise = MediaDataDecoder::DecodePromise;
  using FlushPromise = MediaDataDecoder::FlushPromise;
  using InitPromise = MediaDataDecoder::InitPromise;

  using AMessage = android::AMessage;
  using RefBase = android::RefBase;
  using GonkCryptoInfo = android::GonkCryptoInfo;
  using GonkMediaCodec = android::GonkMediaCodec;
  using ICrypto = android::ICrypto;
  using MediaCodecBuffer = android::MediaCodecBuffer;
  using status_t = android::status_t;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GonkDataDecoder, final);

  GonkDataDecoder(const CreateDecoderParams& aParams, CDMProxy* aProxy);

  RefPtr<InitPromise> Init() override;

  RefPtr<ShutdownPromise> Shutdown() override;

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;

  RefPtr<DecodePromise> Drain() override;

  RefPtr<FlushPromise> Flush() override;

  nsCString GetDescriptionName() const override { return "GonkDataDecoder"_ns; }

  nsCString GetCodecName() const override { return "unknown"_ns; }

  bool SupportDecoderRecycling() const override;

  ConversionRequired NeedsConversion() const override {
    return ConversionRequired::kNeedAnnexB;
  }

  // The following are codec callbacks.
  bool FetchInput(const android::sp<MediaCodecBuffer>& aBuffer,
                  android::sp<RefBase>* aInputInfo,
                  android::sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                  uint32_t* aFlags);

  void Output(const android::sp<MediaCodecBuffer>& aBuffer,
              const android::sp<RefBase>& aInputInfo, int64_t aTimeUs,
              uint32_t aFlags);

  void OutputTexture(layers::TextureClient* aTexture,
                     const android::sp<RefBase>& aInputInfo, int64_t aTimeUs,
                     uint32_t aFlags);

  void NotifyOutputFormat(const android::sp<AMessage>& aFormat);

  void NotifyCodecDetails(const android::sp<AMessage>& aDetails);

  void NotifyError(status_t aErr, int32_t aActionCode);

 private:
  class CodecReplyDispatcher;

  virtual ~GonkDataDecoder();

  RefPtr<GonkDataDecoder> Self() { return this; }

  bool IsVideo() { return mConfig->IsVideo(); }

  bool IsAudio() { return mConfig->IsAudio(); }

  android::sp<ICrypto> GetCrypto();

  DecodedData FetchOutput();

  // mOutputQueue listeners
  void OutputPushed(RefPtr<MediaData>&& aSample) { OutputUpdated(); }
  void OutputFinished() { OutputUpdated(); }
  void OutputUpdated();

  UniquePtr<TrackInfo> mConfig;
  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mImageAllocator;
  RefPtr<CDMProxy> mCDMProxy;
  nsCOMPtr<nsISerialEventTarget> mThread;

  struct {
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    int32_t mStride = 0;
    int32_t mSliceHeight = 0;
    int32_t mColorFormat = 0;
    gfx::IntRect mCrop;
  } mVideoOutputFormat;

  struct {
    int32_t mChannelCount = 0;
    int32_t mSampleRate = 0;
    android::AudioEncoding mPcmEncoding = android::kAudioEncodingPcm16bit;
  } mAudioOutputFormat;

  std::atomic<bool> mSupportAdaptivePlayback = false;

  android::sp<GonkMediaCodec> mCodec;
  MediaQueue<MediaRawData> mInputQueue;
  MediaQueue<MediaData> mOutputQueue;
  AudioCompactor mAudioCompactor;

  // Event listeners for mOutputQueue
  MediaEventListener mPushListener;
  MediaEventListener mFinishListener;

  MozPromiseHolder<InitPromise> mInitPromise;
  MozPromiseHolder<ShutdownPromise> mShutdownPromise;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  MozPromiseHolder<FlushPromise> mFlushPromise;
};

}  // namespace mozilla

#endif  // GONK_DATA_DECODER_H
