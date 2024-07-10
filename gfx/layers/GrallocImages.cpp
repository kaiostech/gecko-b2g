/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GrallocImages.h"

#include "GonkMediaUtils.h"
#include "libyuv.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "nsDebug.h"

using android::GonkImageHandle;

namespace mozilla {
namespace layers {

GrallocImage::GrallocImage() : RecyclingPlanarYCbCrImage(nullptr) {
  mFormat = ImageFormat::GRALLOC_PLANAR_YCBCR;
}

GrallocImage::~GrallocImage() {}

bool GrallocImage::SetData(const PlanarYCbCrData& aData) {
  MOZ_ASSERT(!mTextureClient, "TextureClient is already set");
  NS_ASSERTION(aData.YDataSize().width % 2 == 0,
               "Image should have even width");
  NS_ASSERTION(aData.YDataSize().height % 2 == 0,
               "Image should have even height");
  NS_ASSERTION(aData.mYStride % 16 == 0,
               "Image should have stride of multiple of 16 pixels");

  if (gfxPlatform::GetPlatform()->IsInGonkEmulator()) {
    // Emulator does not support HAL_PIXEL_FORMAT_YV12.
    return false;
  }

  auto allocator = ImageBridgeChild::GetSingleton();
  auto* texData = GrallocTextureData::Create(
      aData.YDataSize(), HAL_PIXEL_FORMAT_YV12, gfx::BackendType::NONE,
      GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_SW_WRITE_OFTEN |
          GraphicBuffer::USAGE_HW_TEXTURE,
      allocator);
  if (NS_WARN_IF(!texData)) {
    return false;
  }
  auto textureClient =
      MakeRefPtr<TextureClient>(texData, TextureFlags::DEFAULT, allocator);

  auto dstHandle = GonkImageHandle::From(texData->GetGraphicBuffer(), true);
  if (NS_WARN_IF(!dstHandle)) {
    return false;
  }

  int result = libyuv::I420Copy(
      aData.mYChannel, aData.mYStride, aData.mCbChannel, aData.mCbCrStride,
      aData.mCrChannel, aData.mCbCrStride, dstHandle->Data(0),
      static_cast<int>(dstHandle->Stride(0)), dstHandle->Data(1),
      static_cast<int>(dstHandle->Stride(1)), dstHandle->Data(2),
      static_cast<int>(dstHandle->Stride(2)),
      static_cast<int>(dstHandle->Width()),
      static_cast<int>(dstHandle->Height()));
  if (NS_WARN_IF(result < 0)) {
    return false;
  }

  mTextureClient = textureClient;
  mSize = aData.YPictureSize();
  return true;
}

void GrallocImage::AdoptData(TextureClient* aGraphicBuffer,
                             const gfx::IntSize& aSize) {
  mTextureClient = aGraphicBuffer;
  mSize = aSize;
}

already_AddRefed<gfx::SourceSurface> GrallocImage::GetAsSourceSurface() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  gfx::IntSize size = GetSize();
  auto format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DataSourceSurface> dest =
      gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!dest)) {
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap map(dest, gfx::DataSourceSurface::WRITE);
  if (NS_WARN_IF(!map.IsMapped())) {
    return nullptr;
  }

  auto srcHandle = GonkImageHandle::From(GetGraphicBuffer());
  if (NS_WARN_IF(!srcHandle)) {
    return nullptr;
  }

  int result = -1;
  switch (srcHandle->Format()) {
    case GonkImageHandle::ColorFormat::I420:
      result = libyuv::I420ToARGB(
          srcHandle->Data(0), static_cast<int>(srcHandle->Stride(0)),
          srcHandle->Data(1), static_cast<int>(srcHandle->Stride(1)),
          srcHandle->Data(2), static_cast<int>(srcHandle->Stride(2)),
          map.GetData(), map.GetStride(), size.Width(), size.Height());
      break;
    case GonkImageHandle::ColorFormat::NV12:
      result = libyuv::NV12ToARGB(
          srcHandle->Data(0), static_cast<int>(srcHandle->Stride(0)),
          srcHandle->Data(1), static_cast<int>(srcHandle->Stride(1)),
          map.GetData(), map.GetStride(), size.Width(), size.Height());
      break;
    case GonkImageHandle::ColorFormat::NV21:
      result = libyuv::NV21ToARGB(
          srcHandle->Data(0), static_cast<int>(srcHandle->Stride(0)),
          srcHandle->Data(2), static_cast<int>(srcHandle->Stride(2)),
          map.GetData(), map.GetStride(), size.Width(), size.Height());
      break;
    default:
      break;
  }
  if (NS_WARN_IF(result < 0)) {
    return nullptr;
  }

  return dest.forget();
}

android::sp<android::GraphicBuffer> GrallocImage::GetGraphicBuffer() const {
  if (!mTextureClient) {
    return nullptr;
  }
  return mTextureClient->GetInternalData()
      ->AsGrallocTextureData()
      ->GetGraphicBuffer();
}

TextureClient* GrallocImage::GetTextureClient(KnowsCompositor* aForwarder) {
  return mTextureClient;
}

}  // namespace layers
}  // namespace mozilla
