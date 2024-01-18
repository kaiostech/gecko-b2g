/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "gfx2DGlue.h"
#include <ui/GraphicBuffer.h>
#include "GrallocImages.h"
#include "GLLibraryEGL.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/webrender/RenderEGLImageTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "EGLImageHelpers.h"
#include "GLReadTexImageHelper.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

static gfx::SurfaceFormat SurfaceFormatForAndroidPixelFormat(
    android::PixelFormat aFormat, TextureFlags aFlags) {
  bool swapRB = bool(aFlags & TextureFlags::RB_SWAPPED);
  switch (aFormat) {
    case android::PIXEL_FORMAT_BGRA_8888:
      return swapRB ? gfx::SurfaceFormat::R8G8B8A8
                    : gfx::SurfaceFormat::B8G8R8A8;
    case android::PIXEL_FORMAT_RGBA_8888:
      return swapRB ? gfx::SurfaceFormat::B8G8R8A8
                    : gfx::SurfaceFormat::R8G8B8A8;
    case android::PIXEL_FORMAT_RGBX_8888:
      return swapRB ? gfx::SurfaceFormat::B8G8R8X8
                    : gfx::SurfaceFormat::R8G8B8X8;
    case android::PIXEL_FORMAT_RGB_565:
      return gfx::SurfaceFormat::R5G6B5_UINT16;
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return gfx::SurfaceFormat::R8G8B8A8;  // yup, use SurfaceFormat::R8G8B8A8
                                            // even though it's a YUV texture.
                                            // This is an external texture.
    default:
      if (aFormat >= 0x100 && aFormat <= 0x1FF) {
        // Reserved range for HAL specific formats.
        return gfx::SurfaceFormat::R8G8B8A8;
      } else {
        // This is not super-unreachable, there's a bunch of hypothetical pixel
        // formats we don't deal with.
        // We only want to abort in debug builds here, since if we crash here
        // we'll take down the compositor process and thus the phone. This seems
        // like undesirable behaviour. We'd rather have a subtle artifact.
        printf_stderr(" xxxxx unknow android format %i\n", (int)aFormat);
        MOZ_ASSERT(false, "Unknown Android pixel format.");
        return gfx::SurfaceFormat::UNKNOWN;
      }
  }
}

static GLenum TextureTargetForAndroidPixelFormat(android::PixelFormat aFormat) {
  switch (aFormat) {
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
    case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return LOCAL_GL_TEXTURE_EXTERNAL;
    case android::PIXEL_FORMAT_BGRA_8888:
    case android::PIXEL_FORMAT_RGBA_8888:
    case android::PIXEL_FORMAT_RGBX_8888:
    case android::PIXEL_FORMAT_RGB_565:
      return LOCAL_GL_TEXTURE_2D;
    default:
      if (aFormat >= 0x100 && aFormat <= 0x1FF) {
        // Reserved range for HAL specific formats.
        return LOCAL_GL_TEXTURE_EXTERNAL;
      } else {
        // This is not super-unreachable, there's a bunch of hypothetical pixel
        // formats we don't deal with.
        // We only want to abort in debug builds here, since if we crash here
        // we'll take down the compositor process and thus the phone. This seems
        // like undesirable behaviour. We'd rather have a subtle artifact.
        MOZ_ASSERT(false, "Unknown Android pixel format.");
        return LOCAL_GL_TEXTURE_EXTERNAL;
      }
  }
}

GrallocTextureHostOGL::GrallocTextureHostOGL(
    TextureFlags aFlags, const SurfaceDescriptorGralloc& aDescriptor)
    : TextureHost(TextureHostType::AndroidHardwareBuffer, aFlags),
      mGrallocHandle(aDescriptor),
      mSize(0, 0),
      mCropSize(0, 0),
      mFormat(gfx::SurfaceFormat::UNKNOWN),
      mEGLImage(EGL_NO_IMAGE),
      mIsOpaque(aDescriptor.isOpaque()) {
  auto graphicBuffer = GetGraphicBuffer();
  MOZ_ASSERT(graphicBuffer);

  if (graphicBuffer) {
    mFormat = SurfaceFormatForAndroidPixelFormat(
        graphicBuffer->getPixelFormat(), aFlags & TextureFlags::RB_SWAPPED);
    mSize = gfx::IntSize(graphicBuffer->getWidth(), graphicBuffer->getHeight());
    mCropSize = mSize;
  } else {
    printf_stderr("gralloc buffer is nullptr\n");
  }
}

GrallocTextureHostOGL::~GrallocTextureHostOGL() { DestroyEGLImage(); }

bool GrallocTextureHostOGL::IsValid() { return !!GetGraphicBuffer(); }

gfx::SurfaceFormat GrallocTextureHostOGL::GetFormat() const { return mFormat; }

void GrallocTextureHostOGL::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  mExternalImageId = Some(aExternalImageId);

  CreateEGLImage();
}

void GrallocTextureHostOGL::DeallocateSharedData() {
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }

  DestroyEGLImage();

  if (mGrallocHandle.buffer().type() !=
      MaybeMagicGrallocBufferHandle::Tnull_t) {
    MaybeMagicGrallocBufferHandle handle = mGrallocHandle.buffer();
    base::ProcessId owner;
    if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
      owner = handle.get_GrallocBufferRef().mOwner;
    } else {
      owner = handle.get_MagicGrallocBufferHandle().mRef.mOwner;
    }

    SharedBufferManagerParent::DropGrallocBuffer(owner, mGrallocHandle);
  }
}

void GrallocTextureHostOGL::ForgetSharedData() {
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }
}

void GrallocTextureHostOGL::DeallocateDeviceData() {
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }
  DestroyEGLImage();
}

already_AddRefed<gfx::DataSourceSurface> GrallocTextureHostOGL::GetAsSurface() {
  auto graphicBuffer = GetGraphicBuffer();
  if (!graphicBuffer) {
    return nullptr;
  }
  uint8_t* grallocData;
  int32_t rv = graphicBuffer->lock(GRALLOC_USAGE_SW_READ_OFTEN,
                                   reinterpret_cast<void**>(&grallocData));
  if (rv) {
    return nullptr;
  }
  RefPtr<gfx::DataSourceSurface> grallocTempSurf =
      gfx::Factory::CreateWrappingDataSourceSurface(
          grallocData,
          graphicBuffer->getStride() *
              android::bytesPerPixel(graphicBuffer->getPixelFormat()),
          GetSize(), GetFormat());
  if (!grallocTempSurf) {
    graphicBuffer->unlock();
    return nullptr;
  }
  RefPtr<gfx::DataSourceSurface> surf =
      CreateDataSourceSurfaceByCloning(grallocTempSurf);

  graphicBuffer->unlock();

  return surf.forget();
}

GLenum GetTextureTarget(gl::GLContext* aGL, android::PixelFormat aFormat) {
  MOZ_ASSERT(aGL);
  if (aGL->Renderer() == gl::GLRenderer::SGX530 ||
      aGL->Renderer() == gl::GLRenderer::SGX540) {
    // SGX has a quirk that only TEXTURE_EXTERNAL works and any other value will
    // result in black pixels when trying to draw from bound textures.
    // Unfortunately, using TEXTURE_EXTERNAL on Adreno has a terrible effect on
    // performance.
    // See Bug 950050.
    return LOCAL_GL_TEXTURE_EXTERNAL;
  } else {
    return TextureTargetForAndroidPixelFormat(aFormat);
  }
}

void GrallocTextureHostOGL::CreateEGLImage() {
  gfx::IntSize cropSize = (mCropSize != mSize) ? mCropSize : mSize;

  if (mEGLImage == EGL_NO_IMAGE) {
    auto graphicBuffer = GetGraphicBuffer();
    MOZ_ASSERT(graphicBuffer);

    mEGLImage = EGLImageCreateFromNativeBuffer(
        nullptr, graphicBuffer->getNativeBuffer(), cropSize);
  }

  if (mExternalImageId.isSome()) {
    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderEGLImageTextureHost(mEGLImage, nullptr, cropSize);
    wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                   texture.forget());
  }
}

void GrallocTextureHostOGL::DestroyEGLImage() {
  // Only called when we want to get rid of the gralloc buffer, usually
  // around the end of life of the TextureHost.
  if (mEGLImage != EGL_NO_IMAGE) {
    EGLImageDestroy(nullptr, mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

android::sp<android::GraphicBuffer> GrallocTextureHostOGL::GetGraphicBuffer() {
  return GetGraphicBufferFromDesc(mGrallocHandle);
}

void GrallocTextureHostOGL::SetCropRect(nsIntRect aCropRect) {
  MOZ_ASSERT(aCropRect.TopLeft() == gfx::IntPoint(0, 0));
  MOZ_ASSERT(!aCropRect.IsEmpty());
  MOZ_ASSERT(aCropRect.width <= mSize.width);
  MOZ_ASSERT(aCropRect.height <= mSize.height);

  gfx::IntSize cropSize(aCropRect.width, aCropRect.height);
  if (mCropSize == cropSize) {
    return;
  }

  mCropSize = cropSize;
  mGLTextureSource = nullptr;

  // Crop is changed to re-create eglImage for WebRender
  if (mExternalImageId.isSome()) {
    DestroyEGLImage();
    CreateEGLImage();
  }
}

void GrallocTextureHostOGL::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;
  auto imageType = wr::ExternalImageType::TextureHandle(
      wr::ImageBufferKind::TextureExternal);

  MOZ_ASSERT(aImageKeys.length() == 1);

  wr::ImageDescriptor descriptor(GetSize(), mFormat);
  (aResources.*method)(aImageKeys[0], descriptor, aExtID, imageType, 0);
}

void GrallocTextureHostOGL::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(aImageKeys.length() == 1);
  aBuilder.PushImage(
      aBounds, aClip, true, false, aFilter, aImageKeys[0],
      !(mFlags & TextureFlags::NON_PREMULTIPLIED),
      wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE));
}

}  // namespace layers
}  // namespace mozilla
