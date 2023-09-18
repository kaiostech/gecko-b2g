/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkVideoDecoderManager_h_)
#  define GonkVideoDecoderManager_h_

#  include "nsRect.h"
#  include "GonkMediaDataDecoder.h"
#  include "mozilla/RefPtr.h"
#  include "I420ColorConverterHelper.h"
#  include "MediaCodecProxy.h"
#  include "mozilla/layers/FenceUtils.h"
#  include "mozilla/UniquePtr.h"
#  include <ui/Fence.h>

namespace android {
struct MOZ_EXPORT AString;
class GonkNativeWindow;
}  // namespace android

namespace mozilla {

namespace layers {
class TextureClient;
class TextureClientRecycleAllocator;
}  // namespace layers

class GonkVideoDecoderManager : public GonkDecoderManager {
  typedef android::MediaCodecProxy MediaCodecProxy;
  typedef mozilla::layers::TextureClient TextureClient;

 public:
  GonkVideoDecoderManager(const VideoInfo& aConfig,
                          mozilla::layers::ImageContainer* aImageContainer,
                          CDMProxy* aProxy);

  virtual ~GonkVideoDecoderManager();

  RefPtr<InitPromise> Init() override;

  nsresult GetOutput(int64_t aStreamOffset,
                     MediaDataDecoder::DecodedData& aOutData) override;

  const char* GetDescriptionName() const override {
    return "gonk video decoder";
  }

  virtual TrackType GetTrackType() const override {
    return TrackType::kAudioTrack;
  }

  virtual const TrackInfo& GetConfig() const override { return mConfig; }

  static void RecycleCallback(TextureClient* aClient, void* aClosure);

 private:
  void FlushInternal() override {
    mEOSSent = false;
    // Bug 1199809: workaround to avoid sending the graphic buffer by making a
    // copy of output buffer after calling flush(). Bug 1203859 was created to
    // reimplementing Gonk PDM on top of OpenMax IL directly. Its buffer
    // management will work better with Gecko and solve problems like this.
    mNeedsCopyBuffer = true;
  }

  void ShutdownInternal() override;

  struct FrameInfo {
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    int32_t mStride = 0;
    int32_t mSliceHeight = 0;
    int32_t mColorFormat = 0;
    int32_t mCropLeft = 0;
    int32_t mCropTop = 0;
    int32_t mCropRight = 0;
    int32_t mCropBottom = 0;
  };

  bool SetVideoFormat();

  nsresult CreateVideoData(
      const android::sp<android::SimpleMediaBuffer>& aBuffer,
      int64_t aStreamOffset, VideoData** aOutData);
  already_AddRefed<VideoData> CreateVideoDataFromGraphicBuffer(
      const android::sp<android::SimpleMediaBuffer>& aSource,
      gfx::IntRect& aPicture);
  already_AddRefed<VideoData> CreateVideoDataFromDataBuffer(
      const android::sp<android::SimpleMediaBuffer>& aSource,
      gfx::IntRect& aPicture);

  uint8_t* GetColorConverterBuffer(int32_t aWidth, int32_t aHeight);

  // For codec resource management
  void CodecReserved();
  void CodecCanceled();

  void ReleaseAllPendingVideoBuffers();
  void PostReleaseVideoBuffer(
      const android::sp<android::SimpleMediaBuffer>& aBuffer,
      layers::FenceHandle mReleaseFence);

  VideoInfo mConfig;

  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::TextureClientRecycleAllocator> mCopyAllocator;

  MozPromiseRequestHolder<android::MediaCodecProxy::CodecPromise>
      mVideoCodecRequest;
  FrameInfo mFrameInfo;

  // color converter
  android::I420ColorConverterHelper mColorConverter;
  UniquePtr<uint8_t[]> mColorConverterBuffer;
  size_t mColorConverterBufferSize;

  android::sp<android::GonkNativeWindow> mNativeWindow;

  android::sp<android::IGraphicBufferProducer> mGraphicBufferProducer;

  struct ReleaseItem {
    ReleaseItem(const android::sp<android::SimpleMediaBuffer>& aBuffer,
                layers::FenceHandle& aFence)
        : mBuffer(aBuffer), mReleaseFence(aFence) {}
    android::sp<android::SimpleMediaBuffer> mBuffer;
    layers::FenceHandle mReleaseFence;
  };
  nsTArray<ReleaseItem> mPendingReleaseItems;

  // The lock protects mPendingReleaseItems.
  Mutex mPendingReleaseItemsLock;

  // Bug 1199809: do we need to make a copy of output buffer? Used only when
  // the decoder outputs graphic buffers.
  bool mNeedsCopyBuffer;

  // MediaCodec can not handle double EOS flag before flushing it.
  // But MediaFormatReader will send Drain() again command when it is in
  // PartialDrainPending state.
  // In that case, GonkVideoDecoderManager returns EOS directly instead of sends
  // EOS flag to MediaCodec.
  // The mEOSSent flag will be reset when
  // GonkVideoDecoderManager::ProcessFlush() is called.
  bool mEOSSent;
};

}  // namespace mozilla

#endif  // GonkVideoDecoderManager_h_
