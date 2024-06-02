/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMediaUtils.h"

#include <media/hardware/VideoAPI.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaCodecList.h>

#include "AudioCompactor.h"
#include "GrallocImages.h"
#include "libyuv.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "XiphExtradata.h"

// WebRTC headers
#include "api/video/video_frame_buffer.h"
#include "WebrtcImageBuffer.h"

namespace android {

#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sUtilsLog("GonkMediaUtils");
#define LOGE(...) MOZ_LOG(sUtilsLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(sUtilsLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(sUtilsLog, mozilla::LogLevel::Info, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(sUtilsLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOGV(...) MOZ_LOG(sUtilsLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

GonkBufferWriter::GonkBufferWriter(const sp<MediaCodecBuffer>& aBuffer)
    : mBuffer(aBuffer) {}

void GonkBufferWriter::Clear() { mBuffer->setRange(0, 0); }

bool GonkBufferWriter::Append(const uint8_t* aData, size_t aSize) {
  if (mBuffer->offset() + mBuffer->size() + aSize > mBuffer->capacity()) {
    return false;
  }

  memcpy(mBuffer->data() + mBuffer->size(), aData, aSize);
  mBuffer->setRange(mBuffer->offset(), mBuffer->size() + aSize);
  return true;
}

static GonkImageHandle::ColorFormat ConvertAndroidPixelFormat(
    PixelFormat aFormat) {
  using mozilla::layers::GrallocImage;
  using ColorFormat = GonkImageHandle::ColorFormat;

  switch (aFormat) {
    case HAL_PIXEL_FORMAT_YV12:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_P:
      return ColorFormat::I420;
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
      return ColorFormat::NV12;
    case HAL_PIXEL_FORMAT_YCRCB_420_SP:
      return ColorFormat::NV21;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return ColorFormat::Unknown;
    default:
      LOGE("unknown pixel format %d", aFormat);
      return ColorFormat::Unsupported;
  }
}

/* static */
sp<GonkImageHandle> GonkImageHandle::From(const sp<GraphicBuffer>& aBuffer,
                                          bool aWrite) {
  if (!aBuffer) {
    LOGE("null GraphicBuffer");
    return nullptr;
  }

  auto usage = aWrite ? GraphicBuffer::USAGE_SW_WRITE_OFTEN
                      : GraphicBuffer::USAGE_SW_READ_OFTEN;
  android_ycbcr yuv;
  status_t err = aBuffer->lockYCbCr(usage, &yuv);
  if (err != OK) {
    LOGE("failed to lock GraphicBuffer in %s mode", aWrite ? "write" : "read");
    return nullptr;
  }

  sp<GonkImageHandle> handle = new GonkImageHandle();
  // Auto unlock buffer in destructor.
  handle->mOnDestruction = [aBuffer]() { aBuffer->unlock(); };
  handle->mWrite = aWrite;
  handle->mFormat = ConvertAndroidPixelFormat(aBuffer->getPixelFormat());

  uint32_t hSubsampling, vSubsampling;
  uint32_t chromaWidth, chromaHeight;
  switch (handle->mFormat) {
    case ColorFormat::I420:
    case ColorFormat::NV12:
    case ColorFormat::NV21:
    // HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED is mapped to Unknown format.
    // Just let GuessUnknownFormat() guess the actual format.
    // FIXME: Assume 4:2:0 subsampling.
    case ColorFormat::Unknown:
      hSubsampling = 2;
      vSubsampling = 2;
      chromaWidth = (aBuffer->getWidth() + 1) / 2;
      chromaHeight = (aBuffer->getHeight() + 1) / 2;
      break;

    default:
      return nullptr;
  }

  handle->mPlanes[0] = {.mWidth = aBuffer->getWidth(),
                        .mHeight = aBuffer->getHeight(),
                        .mStep = 1,
                        .mStride = static_cast<uint32_t>(yuv.ystride),
                        .mHSubsampling = 1,
                        .mVSubsampling = 1,
                        .mData = static_cast<uint8_t*>(yuv.y)};
  handle->mPlanes[1] = {.mWidth = chromaWidth,
                        .mHeight = chromaHeight,
                        .mStep = static_cast<uint32_t>(yuv.chroma_step),
                        .mStride = static_cast<uint32_t>(yuv.cstride),
                        .mHSubsampling = hSubsampling,
                        .mVSubsampling = vSubsampling};
  handle->mPlanes[2] = handle->mPlanes[1];
  handle->mPlanes[1].mData = static_cast<uint8_t*>(yuv.cb);
  handle->mPlanes[2].mData = static_cast<uint8_t*>(yuv.cr);
  return handle->InitCheck() == OK ? handle : nullptr;
}

/* static */
sp<GonkImageHandle> GonkImageHandle::From(const sp<MediaCodecBuffer>& aBuffer,
                                          bool aWrite) {
  if (!aBuffer) {
    LOGE("null MediaCodecBuffer");
    return nullptr;
  }

  sp<ABuffer> imgBuf;
  if (!aBuffer->meta()->findBuffer("image-data", &imgBuf)) {
    LOGE("failed to find image-data from MediaCodecBuffer");
    return nullptr;
  }

  auto* img = reinterpret_cast<MediaImage2*>(imgBuf->data());
  if (img->mType != img->MEDIA_IMAGE_TYPE_YUV) {
    LOGE("only support MediaCodecBuffer in YUV format");
    return nullptr;
  }

  sp<GonkImageHandle> handle = new GonkImageHandle();
  // Keep a reference of the buffer.
  handle->mOnDestruction = [aBuffer]() {};
  handle->mWrite = aWrite;

  const static int indices[] = {MediaImage2::Y, MediaImage2::U, MediaImage2::V};
  for (int i = 0; i < 3; i++) {
    auto& plane = img->mPlane[indices[i]];
    CHECK_GE(plane.mColInc, 1);
    CHECK_GE(plane.mRowInc, 1);
    CHECK_GE(plane.mHorizSubsampling, 1u);
    CHECK_GE(plane.mVertSubsampling, 1u);
    handle->mPlanes[i] = {.mWidth = img->mWidth / plane.mHorizSubsampling,
                          .mHeight = img->mHeight / plane.mVertSubsampling,
                          .mStep = static_cast<uint32_t>(plane.mColInc),
                          .mStride = static_cast<uint32_t>(plane.mRowInc),
                          .mHSubsampling = plane.mHorizSubsampling,
                          .mVSubsampling = plane.mVertSubsampling,
                          .mData = aBuffer->data() + plane.mOffset};
  }

  return handle->InitCheck() == OK ? handle : nullptr;
}

/* static */
sp<GonkImageHandle> GonkImageHandle::From(
    const RefPtr<mozilla::layers::Image>& aImage, bool aWrite) {
  using mozilla::gfx::ChromaSubsampling;
  using mozilla::layers::MappedYCbCrChannelData;
  using mozilla::layers::MappedYCbCrTextureData;
  using mozilla::layers::OpenMode;
  using mozilla::layers::TextureClient;

  if (!aImage || !aImage->IsValid()) {
    LOGE("invalid Image");
    return nullptr;
  }

  if (auto* image = aImage->AsGrallocImage()) {
    return From(image->GetGraphicBuffer(), aWrite);
  }

  if (!aImage->AsPlanarYCbCrImage()) {
    LOGE("only support PlanarYCbCrImage");
    return nullptr;
  }

  RefPtr<TextureClient> texture = aImage->GetTextureClient(nullptr);
  if (!texture) {
    LOGE("null TextureClient");
    return nullptr;
  }

  auto mode = aWrite ? OpenMode::OPEN_WRITE : OpenMode::OPEN_READ;
  if (!texture->Lock(mode)) {
    LOGE("failed to lock TextureClient in %s mode", aWrite ? "write" : "read");
    return nullptr;
  }

  sp<GonkImageHandle> handle = new GonkImageHandle();
  // Auto unlock texture in destructor.
  handle->mOnDestruction = [texture]() { texture->Unlock(); };
  handle->mWrite = aWrite;

  MappedYCbCrTextureData mapped;
  if (!texture->BorrowMappedYCbCrData(mapped)) {
    LOGE("failed to borrow mapped YCbCr data");
    return nullptr;
  }

  uint32_t hSubsampling, vSubsampling;
  switch (aImage->AsPlanarYCbCrImage()->GetData()->mChromaSubsampling) {
    case ChromaSubsampling::FULL:
      hSubsampling = 1;
      vSubsampling = 1;
      break;
    case ChromaSubsampling::HALF_WIDTH:
      hSubsampling = 2;
      vSubsampling = 1;
      break;
    case ChromaSubsampling::HALF_WIDTH_AND_HEIGHT:
      hSubsampling = 2;
      vSubsampling = 2;
      break;
    default:
      return nullptr;
  }

  MappedYCbCrChannelData* channels[3] = {&mapped.y, &mapped.cb, &mapped.cr};
  for (size_t i = 0; i < 3; i++) {
    auto& plane = handle->mPlanes[i];
    auto* channel = channels[i];
    plane = {.mWidth = static_cast<uint32_t>(channel->size.Width()),
             .mHeight = static_cast<uint32_t>(channel->size.Height()),
             .mStep = static_cast<uint32_t>(channel->skip + 1),
             .mStride = static_cast<uint32_t>(channel->stride),
             .mHSubsampling = i > 0 ? hSubsampling : 1,
             .mVSubsampling = i > 0 ? vSubsampling : 1,
             .mData = channel->data};
  }
  return handle->InitCheck() == OK ? handle : nullptr;
}

/* static */
sp<GonkImageHandle> GonkImageHandle::From(
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& aBuffer, bool aWrite) {
  if (!aBuffer) {
    LOGE("null VideoFrameBuffer");
    return nullptr;
  }

  if (aBuffer->type() == webrtc::VideoFrameBuffer::Type::kNative) {
    auto* buffer = static_cast<mozilla::ImageBuffer*>(aBuffer.get());
    return From(buffer->GetNativeImage(), aWrite);
  }

  if (aWrite) {
    LOGE("only support read mode on VideoFrameBuffer");
    return nullptr;
  }

  auto buffer = aBuffer->ToI420();
  uint32_t width = static_cast<uint32_t>(buffer->width());
  uint32_t height = static_cast<uint32_t>(buffer->height());
  uint32_t chromaWidth = static_cast<uint32_t>(buffer->ChromaWidth());
  uint32_t chromaHeight = static_cast<uint32_t>(buffer->ChromaHeight());
  uint32_t chromaStep = 1;
  uint32_t hSubsampling = 2;
  uint32_t vSubsampling = 2;

  sp<GonkImageHandle> handle = new GonkImageHandle();
  // Keep a reference of the buffer.
  handle->mOnDestruction = [buffer]() {};
  handle->mWrite = aWrite;
  handle->mFormat = ColorFormat::I420;
  handle->mPlanes[0] = {.mWidth = width,
                        .mHeight = height,
                        .mStep = 1,
                        .mStride = static_cast<uint32_t>(buffer->StrideY()),
                        .mHSubsampling = 1,
                        .mVSubsampling = 1,
                        .mData = const_cast<uint8_t*>(buffer->DataY())};
  handle->mPlanes[1] = {.mWidth = chromaWidth,
                        .mHeight = chromaHeight,
                        .mStep = chromaStep,
                        .mStride = static_cast<uint32_t>(buffer->StrideU()),
                        .mHSubsampling = hSubsampling,
                        .mVSubsampling = vSubsampling,
                        .mData = const_cast<uint8_t*>(buffer->DataU())};
  handle->mPlanes[2] = {.mWidth = chromaWidth,
                        .mHeight = chromaHeight,
                        .mStep = chromaStep,
                        .mStride = static_cast<uint32_t>(buffer->StrideV()),
                        .mHSubsampling = hSubsampling,
                        .mVSubsampling = vSubsampling,
                        .mData = const_cast<uint8_t*>(buffer->DataV())};
  return handle->InitCheck() == OK ? handle : nullptr;
}

GonkImageHandle::~GonkImageHandle() {
  if (mOnDestruction) {
    mOnDestruction();
  }
}

void GonkImageHandle::GuessUnknownFormat() {
  using U32Pair = std::pair<uint32_t, uint32_t>;

  if (mFormat == ColorFormat::Unknown) {
    mFormat = ColorFormat::Unsupported;
    auto subsampling = U32Pair{HSubsampling(1), VSubsampling(1)};
    auto chromaStep = Step(1);
    auto uvDataDiff = Data(2) - Data(1);

    if (subsampling == U32Pair{2, 2}) {
      if (chromaStep == 1) {
        mFormat = ColorFormat::I420;
      } else if (chromaStep == 2) {
        if (uvDataDiff == 1) {
          mFormat = ColorFormat::NV12;
        } else if (uvDataDiff == -1) {
          mFormat = ColorFormat::NV21;
        }
      }
    }
  }
}

std::string GonkImageHandle::FormatString() {
  switch (mFormat) {
    case ColorFormat::Unsupported:
      return "Unsupported";
    case ColorFormat::Unknown:
      return "Unknown";
    case ColorFormat::I420:
      return "I420";
    case ColorFormat::NV12:
      return "NV12";
    case ColorFormat::NV21:
      return "NV21";
    default:
      TRESPASS();
  }
}

status_t GonkImageHandle::InitCheck() {
#define ENSURE_TRUE(cond)                  \
  do {                                     \
    if (!(cond)) {                         \
      LOGE("InitCheck failed: %s", #cond); \
      return ERROR_UNSUPPORTED;            \
    }                                      \
  } while (0)
#define ENSURE_GE(x, y) ENSURE_TRUE((x) >= (y))
#define ENSURE_EQ(x, y) ENSURE_TRUE((x) == (y))
#define ENSURE_NE(x, y) ENSURE_TRUE((x) != (y))

  for (auto& plane : mPlanes) {
    // Make sure each field has been filled.
    ENSURE_GE(plane.mWidth, 1);
    ENSURE_GE(plane.mHeight, 1);
    ENSURE_GE(plane.mStep, 1);
    ENSURE_GE(plane.mStride, 1);
    ENSURE_GE(plane.mHSubsampling, 1);
    ENSURE_GE(plane.mVSubsampling, 1);
    ENSURE_TRUE(plane.mData);

    ENSURE_GE(plane.mStride, plane.mWidth);
  }

  // Cb and Cr planes should have the same info, except data pointer.
  ENSURE_EQ(mPlanes[1].mWidth, mPlanes[2].mWidth);
  ENSURE_EQ(mPlanes[1].mHeight, mPlanes[2].mHeight);
  ENSURE_EQ(mPlanes[1].mStep, mPlanes[2].mStep);
  ENSURE_EQ(mPlanes[1].mStride, mPlanes[2].mStride);
  ENSURE_EQ(mPlanes[1].mHSubsampling, mPlanes[2].mHSubsampling);
  ENSURE_EQ(mPlanes[1].mVSubsampling, mPlanes[2].mVSubsampling);

  GuessUnknownFormat();
  ENSURE_NE(mFormat, ColorFormat::Unknown);
  ENSURE_NE(mFormat, ColorFormat::Unsupported);

  // Y plane should have no subsampling.
  ENSURE_EQ(mPlanes[0].mHSubsampling, 1);
  ENSURE_EQ(mPlanes[0].mVSubsampling, 1);

  auto uvDataDiff = mPlanes[2].mData - mPlanes[1].mData;
  switch (mFormat) {
    case ColorFormat::I420:
      ENSURE_EQ(mPlanes[1].mStep, 1);
      ENSURE_EQ(mPlanes[1].mHSubsampling, 2);
      ENSURE_EQ(mPlanes[1].mVSubsampling, 2);
      break;
    case ColorFormat::NV12:
      ENSURE_EQ(mPlanes[1].mStep, 2);
      ENSURE_EQ(mPlanes[1].mHSubsampling, 2);
      ENSURE_EQ(mPlanes[1].mVSubsampling, 2);
      ENSURE_EQ(uvDataDiff, 1);  // Semi-planar. Cb first.
      break;
    case ColorFormat::NV21:
      ENSURE_EQ(mPlanes[1].mStep, 2);
      ENSURE_EQ(mPlanes[1].mHSubsampling, 2);
      ENSURE_EQ(mPlanes[1].mVSubsampling, 2);
      ENSURE_EQ(uvDataDiff, -1);  // Semi-planar. Cr first.
      break;
    default:
      break;
  }
  return OK;

#undef ENSURE_TRUE
#undef ENSURE_GE
#undef ENSURE_EQ
#undef ENSURE_NE
}

GonkImageHandle::PlaneInfo& GonkImageHandle::Plane(size_t aIndex) {
  CHECK(aIndex < std::size(mPlanes));
  return mPlanes[aIndex];
}

/* static */
RefPtr<mozilla::layers::Image> GonkImageUtils::CreateI420Image(
    mozilla::layers::ImageContainer* aContainer, uint32_t aWidth,
    uint32_t aHeight) {
  if (!aContainer) {
    LOGE("null ImageContainer");
    return nullptr;
  }

  RefPtr<mozilla::layers::PlanarYCbCrImage> image =
      aContainer->CreatePlanarYCbCrImage();
  if (!image) {
    LOGE("failed to create PlanarYCbCrImage");
    return nullptr;
  }

  auto ySize = mozilla::gfx::IntSize(aWidth, aHeight);
  auto uvSize = mozilla::gfx::IntSize((aWidth + 1) / 2, (aHeight + 1) / 2);

  mozilla::layers::PlanarYCbCrData data;
  data.mYStride = ySize.Width();
  data.mCbCrStride = uvSize.Width();
  data.mPictureRect = mozilla::gfx::IntRect(0, 0, aWidth, aHeight);
  data.mChromaSubsampling =
      mozilla::gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  data.mColorDepth = mozilla::gfx::ColorDepth::COLOR_8;
  if (!image->CreateEmptyBuffer(data, ySize, uvSize)) {
    LOGE("failed to create empty I420 buffer");
    return nullptr;
  }
  return image.forget();
}

/* static */
RefPtr<mozilla::layers::Image> GonkImageUtils::CreateI420ImageCopyImpl(
    mozilla::layers::ImageContainer* aContainer,
    const sp<GonkImageHandle>& aSrcHandle, int aDegrees) {
  if (!aSrcHandle) {
    LOGE("unable to create image copy from null handle");
    return nullptr;
  }

  uint32_t width = aSrcHandle->Width();
  uint32_t height = aSrcHandle->Height();
  if (aDegrees == 90 || aDegrees == 270) {
    std::swap(width, height);
  }

  auto image = CreateI420Image(aContainer, width, height);
  if (!image) {
    return nullptr;
  }
  auto dstHandle = GonkImageHandle::From(image, true);
  status_t err = CopyImageImpl(aSrcHandle, dstHandle, aDegrees);
  return err == OK ? image : nullptr;
}

/* static */
status_t GonkImageUtils::CopyImageImpl(const sp<GonkImageHandle>& aSrcHandle,
                                       const sp<GonkImageHandle>& aDstHandle,
                                       int aDegrees) {
  if (!aSrcHandle || !aDstHandle) {
    LOGE("unable to copy image with invalid handle (%p, %p)", aSrcHandle.get(),
         aDstHandle.get());
    return BAD_VALUE;
  }
  if (!aDstHandle->Writable()) {
    LOGE("unable to copy image with non-writable destination");
    return PERMISSION_DENIED;
  }
  if (aDegrees != 0 && aDegrees != 90 && aDegrees != 180 && aDegrees != 270) {
    LOGE("unable to copy image with invalid rotation %d", aDegrees);
    return BAD_VALUE;
  }

  auto srcSize = std::make_pair(aSrcHandle->Width(), aSrcHandle->Height());
  auto dstSize = std::make_pair(aDstHandle->Width(), aDstHandle->Height());
  if (aDegrees == 90 || aDegrees == 270) {
    std::swap(dstSize.first, dstSize.second);
  }
  if (srcSize != dstSize) {
    LOGE("unable to copy image from %dx%d to %dx%d with rotation %d",
         aSrcHandle->Width(), aSrcHandle->Height(), aDstHandle->Width(),
         aDstHandle->Height(), aDegrees);
  }

  status_t err = aDegrees == 0 ? ConvertImage(aSrcHandle, aDstHandle)
                               : RotateImage(aSrcHandle, aDstHandle, aDegrees);
  if (err != OK) {
    LOGE("unable to copy image from %s to %s with rotation %d",
         aSrcHandle->FormatString().c_str(), aDstHandle->FormatString().c_str(),
         aDegrees);
  }
  return err;
}

/* static */
status_t GonkImageUtils::ConvertImage(const sp<GonkImageHandle>& aSrcHandle,
                                      const sp<GonkImageHandle>& aDstHandle) {
  using Format = GonkImageHandle::ColorFormat;

  int result = -1;
  auto formatPair = std::make_pair(aSrcHandle->Format(), aDstHandle->Format());

  if (formatPair == std::make_pair(Format::I420, Format::I420)) {
    result = libyuv::I420Copy(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV12, Format::NV12)) {
    result = libyuv::NV12Copy(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV21, Format::NV21)) {
    result = libyuv::NV21Copy(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::I420, Format::NV12)) {
    result = libyuv::I420ToNV12(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::I420, Format::NV21)) {
    result = libyuv::I420ToNV21(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV12, Format::I420)) {
    result = libyuv::NV12ToI420(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV21, Format::I420)) {
    result = libyuv::NV21ToI420(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV21, Format::NV12)) {
    result = libyuv::NV21ToNV12(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  } else if (formatPair == std::make_pair(Format::NV12, Format::NV21)) {
    // We can use NV21ToNV12 for NV12 to NV21 conversion.
    result = libyuv::NV21ToNV12(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()));
  }
  return result < 0 ? ERROR_UNSUPPORTED : OK;
}

/* static */
status_t GonkImageUtils::RotateImage(const sp<GonkImageHandle>& aSrcHandle,
                                     const sp<GonkImageHandle>& aDstHandle,
                                     int aDegrees) {
  using Format = GonkImageHandle::ColorFormat;

  int result = -1;
  auto formatPair = std::make_pair(aSrcHandle->Format(), aDstHandle->Format());

  if (formatPair == std::make_pair(Format::I420, Format::I420)) {
    result = libyuv::I420Rotate(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()),
        static_cast<libyuv::RotationMode>(aDegrees));
  } else if (formatPair == std::make_pair(Format::NV12, Format::I420)) {
    result = libyuv::NV12ToI420Rotate(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(1), static_cast<int>(aSrcHandle->Stride(1)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()),
        static_cast<libyuv::RotationMode>(aDegrees));
  } else if (formatPair == std::make_pair(Format::NV21, Format::I420)) {
    // Call NV12 function with Cb/Cr planes swapped.
    result = libyuv::NV12ToI420Rotate(
        aSrcHandle->Data(0), static_cast<int>(aSrcHandle->Stride(0)),
        aSrcHandle->Data(2), static_cast<int>(aSrcHandle->Stride(2)),
        aDstHandle->Data(0), static_cast<int>(aDstHandle->Stride(0)),
        aDstHandle->Data(2), static_cast<int>(aDstHandle->Stride(2)),
        aDstHandle->Data(1), static_cast<int>(aDstHandle->Stride(1)),
        static_cast<int>(aSrcHandle->Width()),
        static_cast<int>(aSrcHandle->Height()),
        static_cast<libyuv::RotationMode>(aDegrees));
  }
  return result < 0 ? ERROR_UNSUPPORTED : OK;
}

/* static */
AudioEncoding GonkMediaUtils::PreferredPcmEncoding() {
#ifdef MOZ_SAMPLE_TYPE_S16
  return kAudioEncodingPcm16bit;
#else
  return kAudioEncodingPcmFloat;
#endif
}

/* static */
size_t GonkMediaUtils::GetAudioSampleSize(AudioEncoding aEncoding) {
  switch (aEncoding) {
    case kAudioEncodingPcm16bit:
      return sizeof(int16_t);
    case kAudioEncodingPcmFloat:
      return sizeof(float);
    default:
      TRESPASS("Invalid AudioEncoding %d", aEncoding);
      return 0;
  }
}

/* static */
GonkMediaUtils::PcmCopy GonkMediaUtils::CreatePcmCopy(const uint8_t* aSource,
                                                      size_t aSourceBytes,
                                                      uint32_t aChannels,
                                                      AudioEncoding aEncoding) {
  class ShortToFloat {
   public:
    ShortToFloat(const uint8_t* aSource, size_t aSourceBytes,
                 uint32_t aChannels)
        : mSource(aSource),
          mSourceBytes(aSourceBytes),
          mChannels(aChannels),
          mSourceOffset(0) {}

    uint32_t operator()(AudioDataValue* aBuffer, uint32_t aSamples) {
      size_t sourceFrames = (mSourceBytes - mSourceOffset) / SourceFrameSize();
      size_t maxFrames = std::min(sourceFrames, size_t(aSamples / mChannels));

      auto* src = reinterpret_cast<const int16_t*>(mSource + mSourceOffset);
      auto* dst = aBuffer;
      for (size_t i = 0; i < maxFrames * mChannels; i++) {
        *dst++ = mozilla::AudioSampleToFloat(*src++);
      }

      mSourceOffset += maxFrames * SourceFrameSize();
      return maxFrames;
    }

   private:
    size_t SourceFrameSize() { return sizeof(int16_t) * mChannels; }

    const uint8_t* const mSource;
    const size_t mSourceBytes;
    const uint32_t mChannels;
    size_t mSourceOffset;
  };

  if (aEncoding == PreferredPcmEncoding()) {
    return mozilla::AudioCompactor::NativeCopy(aSource, aSourceBytes,
                                               aChannels);
  } else {
    CHECK(aEncoding == kAudioEncodingPcm16bit);
    CHECK(PreferredPcmEncoding() == kAudioEncodingPcmFloat);
    return ShortToFloat(aSource, aSourceBytes, aChannels);
  }
}

/* static */
sp<AMessage> GonkMediaUtils::GetMediaCodecConfig(
    const mozilla::TrackInfo* aInfo) {
  using mozilla::AacCodecSpecificData;
  using mozilla::AudioCodecSpecificBinaryBlob;
  using mozilla::BigEndian;
  using mozilla::CheckedUint32;
  using mozilla::FlacCodecSpecificData;
  using mozilla::MediaByteBuffer;
  using mozilla::OpusCodecSpecificData;
  using mozilla::VorbisCodecSpecificData;
  using mozilla::XiphExtradataToHeaders;

  sp<AMessage> format = new AMessage();

  auto& mime = aInfo->mMimeType;
  if (mime.EqualsLiteral("video/vp8")) {
    format->setString("mime", "video/x-vnd.on2.vp8");
  } else if (mime.EqualsLiteral("video/vp9")) {
    format->setString("mime", "video/x-vnd.on2.vp9");
  } else {
    format->setString("mime", mime.get());
  }

  if (auto* info = aInfo->GetAsVideoInfo()) {
    format->setInt32("width", info->mImage.width);
    format->setInt32("height", info->mImage.height);
    if (mime.EqualsLiteral("video/mp4v-es") && info->mExtraData) {
      auto csdBuffer = ABuffer::CreateAsCopy(info->mExtraData->Elements(),
                                             info->mExtraData->Length());
      format->setBuffer("csd-0", csdBuffer);
    }
  } else if (auto* info = aInfo->GetAsAudioInfo()) {
    format->setInt32("channel-count", info->mChannels);
    format->setInt32("sample-rate", info->mRate);
    format->setInt32("aac-profile", info->mProfile);
    format->setInt32("pcm-encoding", PreferredPcmEncoding());

    auto& csd = info->mCodecSpecificConfig;
    if (mime.EqualsLiteral("audio/opus") && csd.is<OpusCodecSpecificData>()) {
      auto& opusCsd = csd.as<OpusCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = opusCsd.mHeadersBinaryBlob;
      // AOSP Opus decoder requires 1) codec specific data, 2) codec delay and
      // 3) seek preroll, but Gecko demuxer only provides codec delay, so set
      // seek preroll to 0.
      static const int32_t kOpusSampleRate = 48000;
      int64_t codecDelayNs =
          opusCsd.mContainerCodecDelayFrames * 1000000000ll / kOpusSampleRate;
      int64_t seekPreroll = 0;
      auto delayBuffer = ABuffer::CreateAsCopy(&codecDelayNs, sizeof(int64_t));
      auto prerollBuffer = ABuffer::CreateAsCopy(&seekPreroll, sizeof(int64_t));
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
      format->setBuffer("csd-1", delayBuffer);
      format->setBuffer("csd-2", prerollBuffer);
    } else if (mime.EqualsLiteral("audio/vorbis") &&
               csd.is<VorbisCodecSpecificData>()) {
      auto& vorbisCsd = csd.as<VorbisCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = vorbisCsd.mHeadersBinaryBlob;
      AutoTArray<unsigned char*, 4> headers;
      AutoTArray<size_t, 4> headerLens;
      if (!XiphExtradataToHeaders(headers, headerLens, blob->Elements(),
                                  blob->Length())) {
        LOGE("could not get Vorbis header");
        return nullptr;
      }
      for (size_t i = 0; i < headers.Length(); i++) {
        auto header = ABuffer::CreateAsCopy(headers[i], headerLens[i]);
        if (header->size() < 1) {
          continue;
        }
        switch (header->data()[0]) {
          case 0x1:  // Identification header
            format->setBuffer("csd-0", header);
            break;
          case 0x5:  // Setup header
            format->setBuffer("csd-1", header);
            break;
        }
      }
    } else if (mime.EqualsLiteral("audio/flac") &&
               csd.is<FlacCodecSpecificData>()) {
      auto& flacCsd = csd.as<FlacCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = flacCsd.mStreamInfoBinaryBlob;
      // mStreamInfoBinaryBlob contains a stream info block without its block
      // header, but AOSP FLAC decoder expects complete metadata that begins
      // with a "fLaC" stream marker and contains one or more metadata blocks
      // with the "last" flag properly set at the last block, so prepend a
      // fake header here with only one stream info block.
      nsTArray<uint8_t> meta = {0x66, 0x4c, 0x61, 0x43, 0x80, 0x00, 0x00, 0x22};
      meta.AppendElements(*blob);
      auto metaBuffer = ABuffer::CreateAsCopy(meta.Elements(), meta.Length());
      format->setBuffer("csd-0", metaBuffer);
    } else if (mime.EqualsLiteral("audio/mp4a-latm") &&
               csd.is<AacCodecSpecificData>()) {
      auto& aacCsd = csd.as<AacCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = aacCsd.mDecoderConfigDescriptorBinaryBlob;
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
    } else if (csd.is<AudioCodecSpecificBinaryBlob>()) {
      LOGD("setting generic codec config data for %s", mime.get());
      auto& audioCsd = csd.as<AudioCodecSpecificBinaryBlob>();
      RefPtr<MediaByteBuffer> blob = audioCsd.mBinaryBlob;
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
    } else {
      LOGD("no codec config data for %s", mime.get());
    }
  } else {
    return nullptr;
  }
  return format;
}

/* static */
sp<GonkCryptoInfo> GonkMediaUtils::GetCryptoInfo(
    const mozilla::MediaRawData* aSample) {
  using mozilla::CheckedUint32;
  using mozilla::CryptoScheme;

  auto& cryptoObj = aSample->mCrypto;
  if (!cryptoObj.IsEncrypted()) {
    return nullptr;
  }

  sp<GonkCryptoInfo> cryptoInfo = new GonkCryptoInfo;

  // |mPlainSizes| and |mEncryptedSizes| should have the same length. If not,
  // just pad the shorter one with zero.
  auto numSubSamples = std::max(cryptoObj.mPlainSizes.Length(),
                                cryptoObj.mEncryptedSizes.Length());
  auto& subSamples = cryptoInfo->mSubSamples;
  uint32_t totalSubSamplesSize = 0;
  for (size_t i = 0; i < numSubSamples; i++) {
    auto plainSize = cryptoObj.mPlainSizes.SafeElementAt(i, 0);
    auto encryptedSize = cryptoObj.mEncryptedSizes.SafeElementAt(i, 0);
    totalSubSamplesSize += plainSize + encryptedSize;
    subSamples.push_back({plainSize, encryptedSize});
  }

  // Size of codec specific data("CSD") for MediaCodec usage should be included
  // in the 1st plain size if it exists.
  if (!subSamples.empty()) {
    uint32_t codecSpecificDataSize = aSample->Size() - totalSubSamplesSize;
    // This shouldn't overflow as the the plain size should be UINT16_MAX at
    // most, and the CSD should never be that large. Checked int acts like a
    // diagnostic assert here to help catch if we ever have insane inputs.
    CheckedUint32 newLeadingPlainSize{subSamples[0].mNumBytesOfClearData};
    newLeadingPlainSize += codecSpecificDataSize;
    subSamples[0].mNumBytesOfClearData = newLeadingPlainSize.value();
  }

  static const int kExpectedIVLength = 16;
  cryptoInfo->mIV.assign(cryptoObj.mIV.begin(), cryptoObj.mIV.end());
  cryptoInfo->mIV.resize(kExpectedIVLength, 0);  // padding with 0
  static const int kExpectedKeyLength = 16;
  cryptoInfo->mKey.assign(cryptoObj.mKeyId.begin(), cryptoObj.mKeyId.end());
  cryptoInfo->mKey.resize(kExpectedKeyLength, 0);  // padding with 0

  cryptoInfo->mPattern.mEncryptBlocks = cryptoObj.mCryptByteBlock;
  cryptoInfo->mPattern.mSkipBlocks = cryptoObj.mSkipByteBlock;

  switch (cryptoObj.mCryptoScheme) {
    case CryptoScheme::None:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_Unencrypted;
      break;
    case CryptoScheme::Cenc:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_AES_CTR;
      break;
    case CryptoScheme::Cbcs:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_AES_CBC;
      break;
  }
  return cryptoInfo;
}

static void FilterCodecs(std::vector<AString>& aCodecs, bool aAllowC2) {
  // List of problematic codecs that will be removed from aCodecs.
  static char const* const kDeniedCodecs[] = {"OMX.qti.audio.decoder.flac"};

  std::vector<AString> temp;
  for (auto& name : aCodecs) {
    if (name.startsWithIgnoreCase("c2.") && !aAllowC2) {
      continue;
    }
    bool denied = false;
    for (const char* deniedCodec : kDeniedCodecs) {
      if (name == deniedCodec) {
        denied = true;
        break;
      }
    }
    if (denied) {
      continue;
    }
    temp.push_back(name);
  }
  aCodecs.swap(temp);
}

/* static */
std::vector<AString> GonkMediaUtils::FindMatchingCodecs(const char* aMime,
                                                        bool aEncoder,
                                                        bool aAllowSoftware) {
  AString mime(aMime);
  if (mime == "video/vp8") {
    mime = "video/x-vnd.on2.vp8";
  } else if (mime == "video/vp9") {
    mime = "video/x-vnd.on2.vp9";
  }

  bool allowCodec2 = true;
  if (!mozilla::StaticPrefs::media_gonkmediacodec_video_enabled() &&
      !aEncoder && mime.startsWithIgnoreCase("video/")) {
    // We are using old video decoder path (GonkVideoDecoderManager). This path
    // uses a hack to steal GraphicBuffers from ACodec and doesn't support C2 or
    // software components.
    allowCodec2 = false;
    aAllowSoftware = false;
  }

  uint32_t flags = aAllowSoftware ? 0 : MediaCodecList::kHardwareCodecsOnly;
  Vector<AString> matches;
  MediaCodecList::findMatchingCodecs(mime.c_str(), aEncoder, flags, &matches);

  // Convert android::Vector to std::vector.
  std::vector<AString> codecs(matches.begin(), matches.end());
  FilterCodecs(codecs, allowCodec2);
  return codecs;
}

}  // namespace android
