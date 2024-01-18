/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRALLOCIMAGES_H
#define GRALLOCIMAGES_H

#include "ImageContainer.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"
#include "mozilla/layers/FenceUtils.h"
#include "mozilla/layers/LayersSurfaces.h"

#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class TextureClient;

/**
 * The YUV format supported by Android HAL
 *
 * 4:2:0 - CbCr width and height is half that of Y.
 *
 * This format assumes
 * - an even width
 * - an even height
 * - a horizontal stride multiple of 16 pixels
 * - a vertical stride equal to the height
 *
 * y_size = stride * height
 * c_size = ALIGN(stride/2, 16) * height/2
 * size = y_size + c_size * 2
 * cr_offset = y_size
 * cb_offset = y_size + c_size
 *
 * The Image that is rendered is the picture region defined by
 * mPicX, mPicY and mPicSize. The size of the rendered image is
 * mPicSize, not mYSize or mCbCrSize.
 */
class GrallocImage : public RecyclingPlanarYCbCrImage {
 public:
  GrallocImage();

  virtual ~GrallocImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  bool SetData(const PlanarYCbCrData& aData);

  using RecyclingPlanarYCbCrImage::AdoptData;
  /**
   *  Share the SurfaceDescriptor without making the copy, in order
   *  to support functioning in all different layer managers.
   */
  void AdoptData(TextureClient* aGraphicBuffer, const gfx::IntSize& aSize);

#if defined(PRODUCT_MANUFACTURER_SPRD)
  /* From vendor/sprd/external/drivers/gpu/utgard/include/gralloc_ext_sprd.h */
  enum {
      /* OEM specific HAL formats */
      HAL_PIXEL_FORMAT_YCbCr_420_P = 0x13,
      HAL_PIXEL_FORMAT_YCbCr_420_SP = 0x15, /*OMX_COLOR_FormatYUV420SemiPlanar*/
      HAL_PIXEL_FORMAT_YCrCb_422_SP = 0x1B,
      HAL_PIXEL_FORMAT_YCrCb_420_P = 0x1C,

      // To be compatible with old GrallocImage enum
      HAL_PIXEL_FORMAT_YCbCr_422_P = 0x102,
      HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO = 0x10A,
      HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED = 0x7FA30C03,
      HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS = 0x7FA30C04,
  };
#else
  // From [android 4.0.4]/hardware/msm7k/libgralloc-qsd8k/gralloc_priv.h
  enum {
    /* OEM specific HAL formats */
    HAL_PIXEL_FORMAT_YCbCr_422_P = 0x102,
    HAL_PIXEL_FORMAT_YCbCr_420_P = 0x103,
    HAL_PIXEL_FORMAT_YCbCr_420_SP = 0x109,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO = 0x10A,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED = 0x7FA30C03,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS = 0x7FA30C04,
  };
#endif

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  android::sp<android::GraphicBuffer> GetGraphicBuffer() const;

  bool IsValid() const override { return !!mTextureClient; }

  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;

  GrallocImage* AsGrallocImage() override { return this; }

 private:
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GRALLOCIMAGES_H */
