/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaCodecProxy.h"
#include <OMX_IVCommon.h>
#include <gui/Surface.h>
#include <media/ICrypto.h>
#include "GonkVideoDecoderManager.h"
#include "GrallocImages.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#include "nsThreadUtils.h"
#include "Layers.h"
#include "mozilla/Logging.h"
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/AString.h>
#include "GonkNativeWindow.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/Preferences.h"

#define CODECCONFIG_TIMEOUT_US 40000LL
#define READ_OUTPUT_BUFFER_TIMEOUT_US 0LL

mozilla::LazyLogModule gGonkVideoDecoderManagerLog("GonkVideoDecoderManager");
#undef LOG
#undef LOGE

#define LOG(x, ...)                                              \
  MOZ_LOG(gGonkVideoDecoderManagerLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define LOGE(x, ...)                                             \
  MOZ_LOG(gGonkVideoDecoderManagerLog, mozilla::LogLevel::Error, \
          ("%p " x, this, ##__VA_ARGS__))

#define LOGE_STATIC(x, ...)                                      \
  MOZ_LOG(gGonkVideoDecoderManagerLog, mozilla::LogLevel::Error, \
          (x, ##__VA_ARGS__))

using namespace mozilla::layers;
using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {

// The maximum height and width of the video on Gonk platform.
// Used for sanitizing the memory allocation of video frame buffers.
// The maximum resolution we target on KaiOS platform in FullHD(1080).
static const uint32_t GONK_MAX_VIDEO_WIDTH = 1920;
static const uint32_t GONK_MAX_VIDEO_HEIGHT = 1080;

class GonkTextureClientAllocationHelper
    : public layers::ITextureClientAllocationHelper {
 public:
  GonkTextureClientAllocationHelper(uint32_t aGrallocFormat, gfx::IntSize aSize)
      : ITextureClientAllocationHelper(gfx::SurfaceFormat::UNKNOWN, aSize,
                                       BackendSelector::Content,
                                       TextureFlags::DEALLOCATE_CLIENT,
                                       ALLOC_DISALLOW_BUFFERTEXTURECLIENT),
        mGrallocFormat(aGrallocFormat) {}

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override {
    uint32_t usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                     android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                     android::GraphicBuffer::USAGE_HW_TEXTURE;

    RefPtr<ImageBridgeChild> allocator = ImageBridgeChild::GetSingleton().get();
    GrallocTextureData* texData = GrallocTextureData::Create(
        mSize, mGrallocFormat, gfx::BackendType::NONE, usage, allocator);
    if (!texData) {
      return nullptr;
    }
    sp<GraphicBuffer> graphicBuffer = texData->GetGraphicBuffer();
    if (!graphicBuffer.get()) {
      return nullptr;
    }
    RefPtr<TextureClient> textureClient = TextureClient::CreateWithData(
        texData, TextureFlags::DEALLOCATE_CLIENT, allocator);
    return textureClient.forget();
  }

  bool IsCompatible(TextureClient* aTextureClient) override {
    if (!aTextureClient) {
      return false;
    }
    sp<GraphicBuffer> graphicBuffer =
        static_cast<GrallocTextureData*>(aTextureClient->GetInternalData())
            ->GetGraphicBuffer();
    if (!graphicBuffer.get() ||
        static_cast<uint32_t>(graphicBuffer->getPixelFormat()) !=
            mGrallocFormat ||
        aTextureClient->GetSize() != mSize) {
      return false;
    }
    return true;
  }

 private:
  uint32_t mGrallocFormat;
};

GonkVideoDecoderManager::GonkVideoDecoderManager(
    const VideoInfo& aConfig, mozilla::layers::ImageContainer* aImageContainer)
    : mConfig(aConfig),
      mImageContainer(aImageContainer),
      mColorConverterBufferSize(0),
      mPendingReleaseItemsLock(
          "GonkVideoDecoderManager::mPendingReleaseItemsLock"),
      mNeedsCopyBuffer(false) {
  MOZ_COUNT_CTOR(GonkVideoDecoderManager);
}

GonkVideoDecoderManager::~GonkVideoDecoderManager() {
  MOZ_COUNT_DTOR(GonkVideoDecoderManager);
}

nsresult GonkVideoDecoderManager::Shutdown() {
  mVideoCodecRequest.DisconnectIfExists();
  return GonkDecoderManager::Shutdown();
}

RefPtr<MediaDataDecoder::InitPromise> GonkVideoDecoderManager::Init() {
  mNeedsCopyBuffer = false;

  uint32_t maxWidth, maxHeight;
  char propValue[PROPERTY_VALUE_MAX];
  // Get maximum size preference from b2g.js
  Preferences::AddUintVarCache(&maxWidth, "gonk.video.max_video_decode_width",
                               -1);
  Preferences::AddUintVarCache(&maxHeight, "gonk.video.max_video_decode_height",
                               -1);
  maxWidth = -1 == maxWidth ? GONK_MAX_VIDEO_WIDTH : maxWidth;
  maxHeight = -1 == maxHeight ? GONK_MAX_VIDEO_HEIGHT : maxHeight;
  if (uint32_t(mConfig.mImage.width * mConfig.mImage.height) >
      maxWidth * maxHeight) {
    LOGE("Video resolution exceeds hw codec capability");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  // Validate the container-reported frame and pictureRect sizes. This ensures
  // that our video frame creation code doesn't overflow.
  if (!IsValidVideoRegion(mConfig.mImage, mConfig.ImageRect(),
                          mConfig.mDisplay)) {
    LOGE("It is not a valid region");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  mReaderTaskQueue = static_cast<TaskQueue*>(AbstractThread::GetCurrent());
  MOZ_ASSERT(mReaderTaskQueue);

  if (mDecodeLooper.get() != nullptr) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  if (!InitLoopers(MediaData::Type::VIDEO_DATA)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  RefPtr<InitPromise> p = mInitPromise.Ensure(__func__);
  android::sp<GonkVideoDecoderManager> self = this;
  mDecoder = MediaCodecProxy::CreateByType(mDecodeLooper,
                                           mConfig.mMimeType.get(), false);

  uint32_t capability = MediaCodecProxy::kEmptyCapability;
  if (mDecoder->getCapability(&capability) == OK &&
      (capability & MediaCodecProxy::kCanExposeGraphicBuffer)) {
    sp<IGonkGraphicBufferConsumer> consumer;
    GonkBufferQueue::createBufferQueue(&mGraphicBufferProducer, &consumer);
    mNativeWindow = new GonkNativeWindow(consumer);
  }
  mDecoder->AsyncAllocateVideoMediaCodec()
      ->Then(
          mReaderTaskQueue, __func__,
          [self](bool) -> void {
            self->mVideoCodecRequest.Complete();
            self->CodecReserved();
          },
          [self](bool) -> void {
            self->mVideoCodecRequest.Complete();
            self->CodecCanceled();
          })
      ->Track(mVideoCodecRequest);
  return p;
}

nsresult GonkVideoDecoderManager::CreateVideoData(MediaBuffer* aBuffer,
                                                  int64_t aStreamOffset,
                                                  VideoData** v) {
  *v = nullptr;
  RefPtr<VideoData> data;
  int64_t timeUs;
  int32_t keyFrame;

  if (aBuffer == nullptr) {
    LOGE("Video Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  AutoReleaseMediaBuffer autoRelease(aBuffer, mDecoder.get());

  if (!aBuffer->meta_data().findInt64(kKeyTime, &timeUs)) {
    LOGE("Decoder did not return frame time");
    return NS_ERROR_UNEXPECTED;
  }

  if (mLastTime > timeUs) {
    LOGE("Output decoded sample time is revert. time=%lld", timeUs);
    return NS_ERROR_NOT_AVAILABLE;
  }
  mLastTime = timeUs;

  if (aBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to
    // ignore quoted from Android's AwesomePlayer.cpp
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aBuffer->meta_data().findInt32(kKeyIsSyncFrame, &keyFrame)) {
    keyFrame = 0;
  }
  gfx::IntRect picture =
      mConfig.ScaledImageRect(mFrameInfo.mWidth, mFrameInfo.mHeight);
  data = CreateVideoDataFromGraphicBuffer(aBuffer, picture);
  if (data) {
    if (!mNeedsCopyBuffer) {
      // RecycleCallback() will be responsible for release the buffer.
      autoRelease.forget();
    }
    mNeedsCopyBuffer = false;
  } else {
    data = CreateVideoDataFromDataBuffer(aBuffer, picture);
  }
  if (!data) {
    return NS_ERROR_UNEXPECTED;
  }
  // Fill necessary info.
  data->mOffset = aStreamOffset;
  data->mTime = media::TimeUnit::FromMicroseconds(timeUs);
  data->mKeyframe = keyFrame;
  data.forget(v);
  return NS_OK;
}

// Copy pixels from one planar YUV to another.
static void CopyYUV(PlanarYCbCrData& aSource, PlanarYCbCrData& aDestination) {
  // Fill Y plane.
  uint8_t* srcY = aSource.mYChannel;
  gfx::IntSize ySize = aSource.mYSize;
  uint8_t* destY = aDestination.mYChannel;
  // Y plane.
  for (int i = 0; i < ySize.height; i++) {
    memcpy(destY, srcY, ySize.width);
    srcY += aSource.mYStride;
    destY += aDestination.mYStride;
  }

  // Fill UV plane.
  // Line start
  uint8_t* srcU = aSource.mCbChannel;
  uint8_t* srcV = aSource.mCrChannel;
  uint8_t* destU = aDestination.mCbChannel;
  uint8_t* destV = aDestination.mCrChannel;

  gfx::IntSize uvSize = aSource.mCbCrSize;
  for (int i = 0; i < uvSize.height; i++) {
    uint8_t* su = srcU;
    uint8_t* sv = srcV;
    uint8_t* du = destU;
    uint8_t* dv = destV;
    for (int j = 0; j < uvSize.width; j++) {
      *du++ = *su++;
      *dv++ = *sv++;
      // Move to next pixel.
      su += aSource.mCbSkip;
      sv += aSource.mCrSkip;
      du += aDestination.mCbSkip;
      dv += aDestination.mCrSkip;
    }
    // Move to next line.
    srcU += aSource.mCbCrStride;
    srcV += aSource.mCbCrStride;
    destU += aDestination.mCbCrStride;
    destV += aDestination.mCbCrStride;
  }
}

static void CopyNV12(uint8_t* aSrc, uint8_t* aDst, uint32_t aWidth,
                     uint32_t aHeight, uint32_t aSrcYStride,
                     uint32_t aDstYStride, uint32_t aSrcYScanlines,
                     uint32_t aDstYScanlines) {
  // Copy Y plane.
  uint8_t* s = aSrc;
  uint8_t* d = aDst;
  for (uint32_t i = 0; i < aHeight; i++) {
    memcpy(d, s, aWidth);
    s += aSrcYStride;
    d += aDstYStride;
  }

  // Copy UV plane.
  uint32_t uvHeight = (aHeight + 1) / 2;
  s = aSrc + aSrcYStride * aSrcYScanlines;
  d = aDst + aDstYStride * aDstYScanlines;
  for (uint32_t i = 0; i < uvHeight; i++) {
    memcpy(d, s, aWidth);
    s += aSrcYStride;
    d += aDstYStride;
  }
}

inline static void CopyNV12(uint8_t* aSrc, uint8_t* aDst, uint32_t aWidth,
                            uint32_t aHeight, uint32_t aSrcYStride,
                            uint32_t aDstYStride) {
  CopyNV12(aSrc, aDst, aWidth, aHeight, aSrcYStride, aDstYStride,
           /* aSrcYScanlines = */ aHeight,
           /* aDstYScanlines = */ aHeight);
}

inline static int Align(int aX, int aAlign) {
  return (aX + aAlign - 1) & ~(aAlign - 1);
}

// Venus formats are doucmented in kernel/include/media/msm_media_info.h:
// * Y_Stride : Width aligned to 128
// * UV_Stride : Width aligned to 128
// * Y_Scanlines: Height aligned to 32
// * UV_Scanlines: Height/2 aligned to 16
// * Total size = align((Y_Stride * Y_Scanlines
// *          + UV_Stride * UV_Scanlines + 4096), 4096)
inline static void CopyVenus(uint8_t* aSrc, uint8_t* aDst, uint32_t aWidth,
                             uint32_t aHeight) {
  uint32_t yStride = Align(aWidth, 128);
  uint32_t yScanlines = Align(aHeight, 32);
  CopyNV12(aSrc, aDst, aWidth, aHeight,
           /* aSrcYStride = */ yStride,
           /* aDstYStride = */ yStride,
           /* aSrcYScanlines = */ yScanlines,
           /* aDstYScanlines = */ yScanlines);
}

static void CopyGraphicBuffer(GraphicBuffer* aSource,
                              sp<GraphicBuffer>& aDestination) {
  void* srcPtr = nullptr;
  aSource->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &srcPtr);
  void* destPtr = nullptr;
  aDestination->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, &destPtr);
  MOZ_ASSERT(srcPtr && destPtr);

  // Build PlanarYCbCrData for source buffer.
  PlanarYCbCrData srcData;
  switch (aSource->getPixelFormat()) {
    case HAL_PIXEL_FORMAT_YV12: {
      // Android YV12 format is defined in system/core/include/system/graphics.h
      srcData.mYChannel = static_cast<uint8_t*>(srcPtr);
      srcData.mYSkip = 0;
      srcData.mYSize.width = aSource->getWidth();
      srcData.mYSize.height = aSource->getHeight();
      srcData.mYStride = aSource->getStride();
      // 4:2:0.
      srcData.mCbCrSize.width = srcData.mYSize.width / 2;
      srcData.mCbCrSize.height = srcData.mYSize.height / 2;
      srcData.mCrChannel =
          srcData.mYChannel + (srcData.mYStride * srcData.mYSize.height);
      // Aligned to 16 bytes boundary.
      srcData.mCbCrStride = Align(srcData.mYStride / 2, 16);
      srcData.mCrSkip = 0;
      srcData.mCbChannel =
          srcData.mCrChannel + (srcData.mCbCrStride * srcData.mCbCrSize.height);
      srcData.mCbSkip = 0;

      // Build PlanarYCbCrData for destination buffer.
      PlanarYCbCrData destData;
      destData.mYChannel = static_cast<uint8_t*>(destPtr);
      destData.mYSkip = 0;
      destData.mYSize.width = aDestination->getWidth();
      destData.mYSize.height = aDestination->getHeight();
      destData.mYStride = aDestination->getStride();
      // 4:2:0.
      destData.mCbCrSize.width = destData.mYSize.width / 2;
      destData.mCbCrSize.height = destData.mYSize.height / 2;
      destData.mCrChannel =
          destData.mYChannel + (destData.mYStride * destData.mYSize.height);
      // Aligned to 16 bytes boundary.
      destData.mCbCrStride = Align(destData.mYStride / 2, 16);
      destData.mCrSkip = 0;
      destData.mCbChannel = destData.mCrChannel +
                            (destData.mCbCrStride * destData.mCbCrSize.height);
      destData.mCbSkip = 0;

      CopyYUV(srcData, destData);
      break;
    }
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:  // NV21, shared the same copy function
                                         // with NV12
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:  // NV12
      CopyNV12(static_cast<uint8_t*>(srcPtr), static_cast<uint8_t*>(destPtr),
               aSource->getWidth(), aSource->getHeight(), aSource->getStride(),
               aDestination->getStride());
      break;
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
      CopyVenus(static_cast<uint8_t*>(srcPtr), static_cast<uint8_t*>(destPtr),
                aSource->getWidth(), aSource->getHeight());
      break;
    default:
      LOGE_STATIC(
          "Unsupported input gralloc image type. Should never be here. "
          "PixelFormat: 0x%08x",
          aSource->getPixelFormat());
  }

  aSource->unlock();
  aDestination->unlock();
}

already_AddRefed<VideoData>
GonkVideoDecoderManager::CreateVideoDataFromGraphicBuffer(
    MediaBuffer* aSource, gfx::IntRect& aPicture) {
  GraphicBuffer* srcBuffer = nullptr;
  if (!aSource->meta_data().findPointer(MediaCodecProxy::kKeyGraphicBuffer,
                                        (void**)&srcBuffer) ||
      (srcBuffer == nullptr)) {
    LOG("Can't get GraphicBuffer from MediaBuffer, try to use normal buffer.");
    return nullptr;
  }

  LOG("CreateVideoDataFromGraphicBuffer(), GraphicBuffer:%p", srcBuffer);

  RefPtr<TextureClient> textureClient;

  if (mNeedsCopyBuffer) {
    // Copy buffer contents for bug 1199809.
    if (!mCopyAllocator) {
      mCopyAllocator = new TextureClientRecycleAllocator(
          ImageBridgeChild::GetSingleton().get());
    }
    if (!mCopyAllocator) {
      LOGE("Create buffer allocator failed!");
      return nullptr;
    }

    gfx::IntSize size(srcBuffer->getWidth(), srcBuffer->getHeight());
    GonkTextureClientAllocationHelper helper(srcBuffer->getPixelFormat(), size);
    textureClient = mCopyAllocator->CreateOrRecycle(helper);
    if (!textureClient) {
      LOGE("Copy buffer allocation failed!");
      return nullptr;
    }

    sp<GraphicBuffer> destBuffer =
        static_cast<GrallocTextureData*>(textureClient->GetInternalData())
            ->GetGraphicBuffer();

    CopyGraphicBuffer(srcBuffer, destBuffer);
  } else {
    textureClient = mNativeWindow->getTextureClientFromBuffer(srcBuffer);
    textureClient->SetRecycleCallback(GonkVideoDecoderManager::RecycleCallback,
                                      this);
    static_cast<GrallocTextureData*>(textureClient->GetInternalData())
        ->SetMediaBuffer(aSource);
  }

  RefPtr<VideoData> data = VideoData::CreateAndCopyData(
      mConfig, mImageContainer,
      0,                                     // Filled later by caller.
      media::TimeUnit::FromMicroseconds(0),  // Filled later by caller.
      media::TimeUnit::FromMicroseconds(
          1),  // No way to pass sample duration from muxer to
               // OMX codec, so we hardcode the duration here.
      textureClient,
      false,  // Filled later by caller.
      media::TimeUnit::FromMicroseconds(-1), aPicture);
  return data.forget();
}

already_AddRefed<VideoData>
GonkVideoDecoderManager::CreateVideoDataFromDataBuffer(MediaBuffer* aSource,
                                                       gfx::IntRect& aPicture) {
  if (!aSource->data()) {
    LOGE("No data in Video Buffer!");
    return nullptr;
  }
  uint8_t* yuv420p_buffer = (uint8_t*)aSource->data();
  int32_t stride = mFrameInfo.mStride;
  int32_t slice_height = mFrameInfo.mSliceHeight;

  // Converts to OMX_COLOR_FormatYUV420Planar
  if (mFrameInfo.mColorFormat != OMX_COLOR_FormatYUV420Planar) {
    ARect crop;
    crop.top = 0;
    crop.bottom = mFrameInfo.mHeight;
    crop.left = 0;
    crop.right = mFrameInfo.mWidth;
    yuv420p_buffer =
        GetColorConverterBuffer(mFrameInfo.mWidth, mFrameInfo.mHeight);
    if (mColorConverter.convertDecoderOutputToI420(
            aSource->data(), mFrameInfo.mWidth, mFrameInfo.mHeight, crop,
            yuv420p_buffer) != OK) {
      LOGE("Color conversion failed!");
      return nullptr;
    }
    stride = mFrameInfo.mWidth;
    slice_height = mFrameInfo.mHeight;
  }

  size_t yuv420p_y_size = stride * slice_height;
  size_t yuv420p_u_size = ((stride + 1) / 2) * ((slice_height + 1) / 2);
  uint8_t* yuv420p_y = yuv420p_buffer;
  uint8_t* yuv420p_u = yuv420p_y + yuv420p_y_size;
  uint8_t* yuv420p_v = yuv420p_u + yuv420p_u_size;

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = yuv420p_y;
  b.mPlanes[0].mWidth = mFrameInfo.mWidth;
  b.mPlanes[0].mHeight = mFrameInfo.mHeight;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = yuv420p_u;
  b.mPlanes[1].mWidth = (mFrameInfo.mWidth + 1) / 2;
  b.mPlanes[1].mHeight = (mFrameInfo.mHeight + 1) / 2;
  b.mPlanes[1].mStride = (stride + 1) / 2;
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = yuv420p_v;
  b.mPlanes[2].mWidth = (mFrameInfo.mWidth + 1) / 2;
  b.mPlanes[2].mHeight = (mFrameInfo.mHeight + 1) / 2;
  b.mPlanes[2].mStride = (stride + 1) / 2;
  b.mPlanes[2].mSkip = 0;

  RefPtr<VideoData> data = VideoData::CreateAndCopyData(
      mConfig, mImageContainer,
      0,                                     // Filled later by caller.
      media::TimeUnit::FromMicroseconds(0),  // Filled later by caller.
      media::TimeUnit::FromMicroseconds(1),  // We don't know the duration.
      b,
      0,  // Filled later by caller.
      media::TimeUnit::FromMicroseconds(-1), aPicture);

  return data.forget();
}

bool GonkVideoDecoderManager::SetVideoFormat() {
  // read video metadata from MediaCodec
  sp<AMessage> codecFormat;
  if (mDecoder->getOutputFormat(&codecFormat) == OK) {
    AString mime;
    int32_t width = 0;
    int32_t height = 0;
    int32_t stride = 0;
    int32_t slice_height = 0;
    int32_t color_format = 0;

    if (!codecFormat->findString("mime", &mime)) {
      LOGE("Failed to find mime from MediaCodec.");
      return false;
    }

    if (!codecFormat->findInt32("width", &width)) {
      LOGE("Failed to find width from MediaCodec.");
      return false;
    }

    if (!codecFormat->findInt32("height", &height)) {
      LOGE("Failed to find height from MediaCodec.");
      return false;
    }

    if (!codecFormat->findInt32("stride", &stride)) {
      LOG("Failed to find stride from MediaCodec.");
      return false;
    }

    if (!codecFormat->findInt32("slice-height", &slice_height)) {
      LOG("Failed to find slice_height from MediaCodec.");
      return false;
    }

    if (!codecFormat->findInt32("color-format", &color_format)) {
      LOG("Failed to find color_format from MediaCodec.");
      return false;
    }

    LOG("Format from MediaCodec: width:%d, height:%d, "
        "stride:%d,slice_height:%d, color_format:%d",
        width, height, stride, slice_height, color_format);

    mFrameInfo.mWidth = width;
    mFrameInfo.mHeight = height;
    mFrameInfo.mStride = stride;
    mFrameInfo.mSliceHeight = slice_height;
    mFrameInfo.mColorFormat = color_format;

    nsIntSize displaySize(width, height);
    if (!IsValidVideoRegion(mConfig.mDisplay,
                            mConfig.ScaledImageRect(width, height),
                            displaySize)) {
      LOGE("It is not a valid region");
      return false;
    }
    return true;
  }
  LOGE("Fail to get output format");
  return false;
}

// Blocks until decoded sample is produced by the deoder.
nsresult GonkVideoDecoderManager::GetOutput(
    int64_t aStreamOffset, MediaDataDecoder::DecodedData& aOutData) {
  aOutData.Clear();
  status_t err;
  if (mDecoder == nullptr) {
    LOGE("Decoder is not inited");
    return NS_ERROR_UNEXPECTED;
  }
  MediaBuffer* outputBuffer = nullptr;
  err = mDecoder->Output(&outputBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);
  switch (err) {
    case OK: {
      RefPtr<VideoData> data;
      nsresult rv =
          CreateVideoData(outputBuffer, aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // Decoder outputs a empty video buffer, try again
        return NS_ERROR_NOT_AVAILABLE;
      } else if (rv != NS_OK || data == nullptr) {
        LOGE("Failed to create VideoData");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData.AppendElement(data);
      return NS_OK;
    }
    case android::INFO_FORMAT_CHANGED: {
      // If the format changed, update our cached info.
      LOG("Decoder format changed");
      if (!SetVideoFormat()) {
        return NS_ERROR_UNEXPECTED;
      }
      return GetOutput(aStreamOffset, aOutData);
    }
    case android::INFO_OUTPUT_BUFFERS_CHANGED: {
      if (mDecoder->UpdateOutputBuffers()) {
        return GetOutput(aStreamOffset, aOutData);
      }
      LOGE("Fails to update output buffers!");
      return NS_ERROR_FAILURE;
    }
    case -EAGAIN: {
      //      LOGE("Need to try again!");
      return NS_ERROR_NOT_AVAILABLE;
    }
    case android::ERROR_END_OF_STREAM: {
      LOGE("Got the EOS frame!");
      RefPtr<VideoData> data;
      nsresult rv =
          CreateVideoData(outputBuffer, aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // For EOS, no need to do any thing.
        return NS_ERROR_ABORT;
      }
      if (rv != NS_OK || data == nullptr) {
        LOGE("Failed to create video data");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData.AppendElement(data);
      return NS_ERROR_ABORT;
    }
    case -ETIMEDOUT: {
      LOGE("Timeout. can try again next time");
      return NS_ERROR_UNEXPECTED;
    }
    default: {
      LOGE("Decoder failed, err=%d", err);
      return NS_ERROR_UNEXPECTED;
    }
  }
}

void GonkVideoDecoderManager::CodecReserved() {
  if (mInitPromise.IsEmpty()) {
    return;
  }
  LOG("CodecReserved");
  sp<AMessage> format = new AMessage;
  sp<Surface> surface;
  status_t rv = OK;
  // Fixed values
  LOG("Configure video mime type: %s, width:%d, height:%d",
      mConfig.mMimeType.get(), mConfig.mImage.width, mConfig.mImage.height);
  format->setString("mime", mConfig.mMimeType.get());
  format->setInt32("width", mConfig.mImage.width);
  format->setInt32("height", mConfig.mImage.height);
  // Set the "moz-use-undequeued-bufs" to use the undeque buffers to accelerate
  // the video decoding.
  format->setInt32("moz-use-undequeued-bufs", 1);
  if (mNativeWindow != nullptr) {
    surface = new Surface(mGraphicBufferProducer);
  }
  mDecoder->configure(format, surface, nullptr, 0);
  mDecoder->Prepare();

  if (mConfig.mMimeType.EqualsLiteral("video/mp4v-es")) {
    rv = mDecoder->Input(mConfig.mCodecSpecificConfig->Elements(),
                         mConfig.mCodecSpecificConfig->Length(), 0,
                         android::MediaCodec::BUFFER_FLAG_CODECCONFIG,
                         CODECCONFIG_TIMEOUT_US);
  }

  if (rv != OK) {
    LOGE("Failed to configure codec!!!!");
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }

  mInitPromise.Resolve(TrackType::kVideoTrack, __func__);
}

void GonkVideoDecoderManager::CodecCanceled() {
  LOG("CodecCanceled");
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

// Called on GonkDecoderManager::mTaskLooper thread.
void GonkVideoDecoderManager::onMessageReceived(const sp<AMessage>& aMessage) {
  switch (aMessage->what()) {
    case kNotifyPostReleaseBuffer: {
      ReleaseAllPendingVideoBuffers();
      break;
    }

    default: {
      GonkDecoderManager::onMessageReceived(aMessage);
      break;
    }
  }
}

uint8_t* GonkVideoDecoderManager::GetColorConverterBuffer(int32_t aWidth,
                                                          int32_t aHeight) {
  // Allocate a temporary YUV420Planer buffer.
  size_t yuv420p_y_size = aWidth * aHeight;
  size_t yuv420p_u_size = ((aWidth + 1) / 2) * ((aHeight + 1) / 2);
  size_t yuv420p_v_size = yuv420p_u_size;
  size_t yuv420p_size = yuv420p_y_size + yuv420p_u_size + yuv420p_v_size;
  if (mColorConverterBufferSize != yuv420p_size) {
    mColorConverterBuffer = MakeUnique<uint8_t[]>(yuv420p_size);
    mColorConverterBufferSize = yuv420p_size;
  }
  return mColorConverterBuffer.get();
}

/* static */
void GonkVideoDecoderManager::RecycleCallback(TextureClient* aClient,
                                              void* aClosure) {
  MOZ_ASSERT(aClient && !aClient->IsDead());
  GonkVideoDecoderManager* videoManager =
      static_cast<GonkVideoDecoderManager*>(aClosure);
  GrallocTextureData* client =
      static_cast<GrallocTextureData*>(aClient->GetInternalData());
  aClient->ClearRecycleCallback();
  FenceHandle handle =
      aClient->GetInternalData()->GetAndResetReleaseFenceHandle();
  videoManager->PostReleaseVideoBuffer(client->GetMediaBuffer(), handle);
}

void GonkVideoDecoderManager::PostReleaseVideoBuffer(
    android::MediaBuffer* aBuffer, FenceHandle aReleaseFence) {
  {
    MutexAutoLock autoLock(mPendingReleaseItemsLock);
    if (aBuffer) {
      mPendingReleaseItems.AppendElement(ReleaseItem(aBuffer, aReleaseFence));
    }
  }

  sp<AMessage> notify = new AMessage(kNotifyPostReleaseBuffer, this);
  notify->post();
}

void GonkVideoDecoderManager::ReleaseAllPendingVideoBuffers() {
  nsTArray<ReleaseItem> releasingItems;
  {
    MutexAutoLock autoLock(mPendingReleaseItemsLock);
    releasingItems.AppendElements(mPendingReleaseItems);
    mPendingReleaseItems.Clear();
  }

  // Free all pending video buffers without holding mPendingReleaseItemsLock.
  size_t size = releasingItems.Length();
  for (size_t i = 0; i < size; i++) {
    RefPtr<FenceHandle::FdObj> fdObj =
        releasingItems[i].mReleaseFence.GetAndResetFdObj();
    sp<android::Fence> fence = new android::Fence(fdObj->GetAndResetFd());
    fence->waitForever("GonkVideoDecoderManager");
    mDecoder->ReleaseMediaBuffer(releasingItems[i].mBuffer);
  }
  releasingItems.Clear();
}

}  // namespace mozilla
