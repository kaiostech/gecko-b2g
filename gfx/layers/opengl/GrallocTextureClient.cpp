/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxAndroidPlatform.h"
#include "libyuv.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "mozilla/layers/TextureForwarder.h"
#include "gfx2DGlue.h"

#include <ui/Fence.h>

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static bool DisableGralloc(SurfaceFormat aFormat,
                           const gfx::IntSize& aSizeHint) {
  if (gfx::gfxVars::DisableGralloc()) {
    return true;
  }
  if (aFormat == gfx::SurfaceFormat::A8) {
    return true;
  }

  return false;
}

gfx::SurfaceFormat SurfaceFormatForPixelFormat(android::PixelFormat aFormat) {
  switch (aFormat) {
    case android::PIXEL_FORMAT_RGBA_8888:
      return gfx::SurfaceFormat::R8G8B8A8;
    case android::PIXEL_FORMAT_BGRA_8888:
      return gfx::SurfaceFormat::B8G8R8A8;
    case android::PIXEL_FORMAT_RGBX_8888:
      return gfx::SurfaceFormat::R8G8B8X8;
    case android::PIXEL_FORMAT_RGB_565:
      return gfx::SurfaceFormat::R5G6B5_UINT16;
    case HAL_PIXEL_FORMAT_YV12:
      return gfx::SurfaceFormat::YUV;
    default:
      return gfx::SurfaceFormat::UNKNOWN;
  }
}

bool IsGrallocRBSwapped(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
      return true;
    default:
      return false;
  }
}

uint32_t GetAndroidFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
      return android::PIXEL_FORMAT_RGBA_8888;
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::B8G8R8X8:
      return android::PIXEL_FORMAT_RGBX_8888;
    case gfx::SurfaceFormat::R5G6B5_UINT16:
      return android::PIXEL_FORMAT_RGB_565;
    case gfx::SurfaceFormat::YUV:
      return HAL_PIXEL_FORMAT_YV12;
    case gfx::SurfaceFormat::A8:
      NS_WARNING("gralloc does not support SurfaceFormat::A8");
      return android::PIXEL_FORMAT_UNKNOWN;
    default:
      NS_WARNING("Unsupported surface format");
      return android::PIXEL_FORMAT_UNKNOWN;
  }
}

GrallocTextureData::GrallocTextureData(
    MaybeMagicGrallocBufferHandle aGrallocHandle, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, gfx::BackendType aMoz2DBackend)
    : mSize(aSize),
      mFormat(aFormat),
      mMoz2DBackend(aMoz2DBackend),
      mGrallocHandle(aGrallocHandle),
      mMappedBuffer(nullptr) {
  mGraphicBuffer = GetGraphicBufferFrom(aGrallocHandle);
  MOZ_COUNT_CTOR(GrallocTextureData);
}

GrallocTextureData::~GrallocTextureData() {
  MOZ_COUNT_DTOR(GrallocTextureData);
}

void GrallocTextureData::Deallocate(LayersIPCChannel* aAllocator) {
  MOZ_ASSERT(aAllocator);
  if (aAllocator) {
    SharedBufferManagerChild::GetSingleton()->DeallocGrallocBuffer(
        mGrallocHandle);
  }

  mGrallocHandle = null_t();
  mGraphicBuffer = nullptr;
}

void GrallocTextureData::Forget(LayersIPCChannel* aAllocator) {
  MOZ_ASSERT(aAllocator);
  if (aAllocator) {
    SharedBufferManagerChild::GetSingleton()->DropGrallocBuffer(mGrallocHandle);
  }

  mGrallocHandle = null_t();
  mGraphicBuffer = nullptr;
}

void GrallocTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasSynchronization = true;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = true;
}

bool GrallocTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  aOutDescriptor =
      SurfaceDescriptorGralloc(mGrallocHandle, gfx::IsOpaque(mFormat));
  return true;
}

void GrallocTextureData::WaitForBufferOwnership() {
  if (mReleaseFenceHandle.IsValid()) {
    RefPtr<FenceHandle::FdObj> fdObj = mReleaseFenceHandle.GetAndResetFdObj();
    android::sp<android::Fence> fence =
        new android::Fence(fdObj->GetAndResetFd());
    fence->waitForever("GrallocTextureClientOGL::Lock");
    mReleaseFenceHandle = FenceHandle();
  }
}

void GrallocTextureData::WaitForFence(FenceHandle* aFence) {}

bool GrallocTextureData::Lock(OpenMode aMode) {
  MOZ_ASSERT(!mMappedBuffer);
  // TODO: Add release fence handling
  FenceHandle* aReleaseFence = nullptr;

  uint32_t usage = 0;
  if (aMode & OpenMode::OPEN_READ) {
    usage |= GRALLOC_USAGE_SW_READ_OFTEN;
  }
  if (aMode & OpenMode::OPEN_WRITE) {
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }

  void** mappedBufferPtr = reinterpret_cast<void**>(&mMappedBuffer);

  int32_t rv = 0;
  if (aReleaseFence) {
    RefPtr<FenceHandle::FdObj> fdObj = aReleaseFence->GetAndResetFdObj();
    rv = mGraphicBuffer->lockAsync(usage, mappedBufferPtr,
                                   fdObj->GetAndResetFd());
  } else {
    rv = mGraphicBuffer->lock(usage, mappedBufferPtr);
  }

  if (rv) {
    mMappedBuffer = nullptr;
    NS_WARNING("Couldn't lock graphic buffer");
    return false;
  }

  return true;
}

void GrallocTextureData::Unlock() {
  MOZ_ASSERT(mMappedBuffer);
  mMappedBuffer = nullptr;
  mGraphicBuffer->unlock();
}

already_AddRefed<gfx::DrawTarget> GrallocTextureData::BorrowDrawTarget() {
  MOZ_ASSERT(mMappedBuffer);
  if (!mMappedBuffer) {
    return nullptr;
  }
  long byteStride = mGraphicBuffer->getStride() * BytesPerPixel(mFormat);
  return gfxPlatform::GetPlatform()->CreateDrawTargetForData(
      mMappedBuffer, mSize, byteStride, mFormat);
}

bool GrallocTextureData::BorrowMappedData(MappedTextureData& aMap) {
  if (mFormat == gfx::SurfaceFormat::YUV || !mMappedBuffer) {
    return false;
  }

  aMap.data = mMappedBuffer;
  aMap.size = mSize;
  aMap.stride = mGraphicBuffer->getStride() * BytesPerPixel(mFormat);
  aMap.format = mFormat;

  return true;
}

bool GrallocTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface) {
  MOZ_ASSERT(mMappedBuffer,
             "Calling TextureClient::BorrowDrawTarget without locking :(");

  if (!mMappedBuffer) {
    return false;
  }

  RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError()
        << "Failed to GetDataSurface in UpdateFromSurface (GTC).";
    return false;
  }

  gfx::SurfaceFormat format =
      SurfaceFormatForPixelFormat(mGraphicBuffer->getPixelFormat());
  if (mSize != srcSurf->GetSize() || mFormat != srcSurf->GetFormat()) {
    gfxCriticalError() << "Attempt to update texture client from a surface "
                          "with a different size or format! This: "
                       << mSize << " " << format
                       << " Other: " << srcSurf->GetSize() << " "
                       << srcSurf->GetFormat();
    return false;
  }

  long pixelStride = mGraphicBuffer->getStride();
  long byteStride = pixelStride * BytesPerPixel(format);

  DataSourceSurface::MappedSurface sourceMap;

  if (!srcSurf->Map(DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError()
        << "Failed to map source surface for UpdateFromSurface (GTC).";
    return false;
  }

  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(mMappedBuffer + byteStride * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  srcSurf->Unmap();

  return true;
}

// static
GrallocTextureData* GrallocTextureData::Create(gfx::IntSize aSize,
                                               AndroidFormat aAndroidFormat,
                                               gfx::BackendType aMoz2dBackend,
                                               uint32_t aUsage,
                                               LayersIPCChannel* aAllocator) {
  if (!aAllocator || !aAllocator->IPCOpen()) {
    return nullptr;
  }
  // TODO FIXME
  // int32_t maxSize = aAllocator->GetMaxTextureSize();
  // if (aSize.width > maxSize || aSize.height > maxSize) {
  //  return nullptr;
  //}
  gfx::SurfaceFormat format;
  switch (aAndroidFormat) {
    case android::PIXEL_FORMAT_RGBA_8888:
      format = gfx::SurfaceFormat::B8G8R8A8;
      break;
    case android::PIXEL_FORMAT_BGRA_8888:
      format = gfx::SurfaceFormat::B8G8R8A8;
      break;
    case android::PIXEL_FORMAT_RGBX_8888:
      format = gfx::SurfaceFormat::B8G8R8X8;
      break;
    case android::PIXEL_FORMAT_RGB_565:
      format = gfx::SurfaceFormat::R5G6B5_UINT16;
      break;
    case HAL_PIXEL_FORMAT_YV12:
      format = gfx::SurfaceFormat::YUV;
      break;
    default:
      format = gfx::SurfaceFormat::UNKNOWN;
  }

  if (DisableGralloc(format, aSize)) {
    return nullptr;
  }

  MaybeMagicGrallocBufferHandle handle;
  if (!SharedBufferManagerChild::GetSingleton()->AllocGrallocBuffer(
          aSize, aAndroidFormat, aUsage, &handle)) {
    return nullptr;
  }

  android::sp<android::GraphicBuffer> graphicBuffer =
      GetGraphicBufferFrom(handle);
  if (!graphicBuffer.get()) {
    return nullptr;
  }

  if (graphicBuffer->initCheck() != android::NO_ERROR) {
    return nullptr;
  }

  return new GrallocTextureData(handle, aSize, format, aMoz2dBackend);
}

// static
GrallocTextureData* GrallocTextureData::CreateForDrawing(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    gfx::BackendType aMoz2dBackend, LayersIPCChannel* aAllocator,
    TextureAllocationFlags aAllocFlags) {
  if (DisableGralloc(aFormat, aSize)) {
    return nullptr;
  }

  uint32_t usage = android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                   android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;
  auto data = GrallocTextureData::Create(aSize, GetAndroidFormat(aFormat),
                                         aMoz2dBackend, usage, aAllocator);

  if (!data) {
    return nullptr;
  }

  if ((aAllocFlags & ALLOC_CLEAR_BUFFER) ||
      (aAllocFlags & ALLOC_CLEAR_BUFFER_BLACK)) {
    if (aFormat == gfx::SurfaceFormat::B8G8R8X8) {
      uint8_t* buffer;
      android::status_t rv = data->mGraphicBuffer->lock(
          android::GraphicBuffer::USAGE_SW_WRITE_OFTEN,
          reinterpret_cast<void**>(&buffer));
      if (rv != android::OK) {
        return nullptr;
      }

      uint32_t bufSize = data->mGraphicBuffer->getStride() * aSize.height * 4;

      // Even though BGRX was requested, XRGB_UINT32 is what is meant,
      // so use 0xFF000000 to put alpha in the right place.
      libyuv::ARGBRect(buffer, bufSize, 0, 0, bufSize / sizeof(uint32_t), 1,
                       0xFF000000);
      data->mGraphicBuffer->unlock();
    }
  }

  DebugOnly<gfx::SurfaceFormat> grallocFormat =
      SurfaceFormatForPixelFormat(data->mGraphicBuffer->getPixelFormat());
  // mFormat may be different from the format the graphic buffer reports if we
  // swap the R and B channels but we should always have at least the same bytes
  // per pixel!
  MOZ_ASSERT(BytesPerPixel(data->mFormat) == BytesPerPixel(grallocFormat));

  return data;
}

TextureFlags GrallocTextureData::GetTextureFlags() const {
  TextureFlags flags = TextureFlags::WAIT_HOST_USAGE_END;
  if (IsGrallocRBSwapped(mFormat)) {
    return flags | TextureFlags::RB_SWAPPED;
  }
  return flags;
}

// static
GrallocTextureData* GrallocTextureData::CreateForYCbCr(
    gfx::IntSize aYSize, gfx::IntSize aCbCrSize, LayersIPCChannel* aAllocator) {
  MOZ_ASSERT(aYSize.width == aCbCrSize.width * 2);
  MOZ_ASSERT(aYSize.height == aCbCrSize.height * 2);
  return GrallocTextureData::Create(
      aYSize, HAL_PIXEL_FORMAT_YV12, gfx::BackendType::NONE,
      android::GraphicBuffer::USAGE_SW_READ_OFTEN, aAllocator);
}

// static
GrallocTextureData* GrallocTextureData::CreateForGLRendering(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    LayersIPCChannel* aAllocator) {
  if (aFormat == gfx::SurfaceFormat::YUV) {
    return nullptr;
  }
  uint32_t usage = android::GraphicBuffer::USAGE_HW_RENDER |
                   android::GraphicBuffer::USAGE_HW_TEXTURE;
  return GrallocTextureData::Create(aSize, GetAndroidFormat(aFormat),
                                    gfx::BackendType::NONE, usage, aAllocator);
}

TextureData* GrallocTextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  if (mFormat == gfx::SurfaceFormat::YUV) {
    return GrallocTextureData::CreateForYCbCr(mSize, mSize * 2, aAllocator);
  } else {
    return GrallocTextureData::CreateForDrawing(mSize, mFormat, mMoz2DBackend,
                                                aAllocator, aAllocFlags);
  }
}

}  // namespace layers
}  // namespace mozilla
