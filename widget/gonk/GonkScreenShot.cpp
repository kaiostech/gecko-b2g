/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/GraphicBuffer.h>
#include <ui/GraphicTypes.h>
#include <ui/PixelFormat.h>

#include "GonkScreenShot.h"
#include "libdisplay/GonkDisplay.h"
#include "png.h"

using namespace android;

#ifdef LOG_TAG
#  undef LOG_TAG
#endif
#define LOG_TAG "GonkScreenShot"

#include <android/log.h>
#define GS_LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args)

#define COLORSPACE_UNKNOWN 0
#define COLORSPACE_SRGB 1
#define COLORSPACE_DISPLAY_P3 2

static uint32_t dataSpaceToInt(ui::Dataspace d) {
  switch (d) {
    case ui::Dataspace::V0_SRGB:
      return COLORSPACE_SRGB;
    case ui::Dataspace::DISPLAY_P3:
      return COLORSPACE_DISPLAY_P3;
    default:
      return COLORSPACE_UNKNOWN;
  }
}

static uint32_t encodeToPng(FILE* dstFp, uint32_t w, uint32_t h, uint32_t s,
                            uint32_t f, void* srcPtr) {
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_bytep row_pointer = nullptr;
  uint32_t png_format, png_bits_depth, png_bytes_pixel;

  if (f == HAL_PIXEL_FORMAT_RGB_565) {
    png_format = PNG_COLOR_TYPE_RGB;
    png_bits_depth = 8;
    png_bytes_pixel = bytesPerPixel(HAL_PIXEL_FORMAT_RGB_888);
  } else if (f == HAL_PIXEL_FORMAT_RGBA_8888) {
    png_format = PNG_COLOR_TYPE_RGB_ALPHA;
    png_bits_depth = 8;
    png_bytes_pixel = bytesPerPixel(HAL_PIXEL_FORMAT_RGBA_8888);
  } else {
    GS_LOGE("Unsupport color format %d\n", f);
    return 1;
  }

  /* initialize stuff */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    GS_LOGE("png_create_write_struct failed\n");
    return 1;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    GS_LOGE("png_create_info_struct failed\n");
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    return 1;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    GS_LOGE("Error in encoding png\n");
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    return 1;
  }

  png_init_io(png_ptr, dstFp);

  /* write header */
  png_set_IHDR(png_ptr, info_ptr, w, h, png_bits_depth, png_format,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);

  /* write bytes */
  // RGB565: need to convert to RGB888.
  if (f == HAL_PIXEL_FORMAT_RGB_565) {
    row_pointer = (png_bytep)malloc(w * png_bytes_pixel);
    for (size_t i = 0; i < h; i++) {
      png_bytep src_pointer = (png_bytep)srcPtr + (i * s * bytesPerPixel(f));
      // convert RGB565 to RGB888 for a row.
      for (size_t j = 0; j < w; j++) {
        row_pointer[j * png_bytes_pixel] = *(src_pointer + 1);
        row_pointer[j * png_bytes_pixel + 1] =
            (*(src_pointer + 1) & 0x07) << 5 | (*src_pointer & 0xe0) >> 3;
        row_pointer[j * png_bytes_pixel + 2] = (*src_pointer) << 3;
        src_pointer += 2;
      }
      png_write_row(png_ptr, row_pointer);
    }

    if (row_pointer) {
      free(row_pointer);
    }
    // RGBA8888: encode to png directly.
  } else {
    for (size_t i = 0; i < h; i++) {
      row_pointer = (png_bytep)srcPtr + (i * s * bytesPerPixel(f));
      png_write_row(png_ptr, row_pointer);
    }
  }

  /* end write */
  png_write_end(png_ptr, NULL);
  /* free write and info structure */
  png_destroy_write_struct(&png_ptr, &info_ptr);

  return 0;
}

namespace mozilla {

int GonkScreenShot::capture(uint32_t displayId, bool pngEncode,
                            const char* fileName) {
  FILE* fp = fopen(fileName, "wb");
  if (!fp) {
    GS_LOGE("Error opening file: %s (%s)\n", fileName, strerror(errno));
    return 1;
  }

  const int len = strlen(fileName);
  if (!pngEncode && len >= 4 && 0 == strcmp(fileName + len - 4, ".png")) {
    pngEncode = true;
  }

  void* base = NULL;
  uint32_t w, s, h, f;

  ui::Dataspace outDataspace = ui::Dataspace::V0_SRGB;
  sp<GraphicBuffer> outBuffer =
      GetGonkDisplay()->GetFrameBuffer((DisplayType)displayId);
  if (!outBuffer) {
    GS_LOGE("Failed to GetFrameBuffer from GonkDisplay\n");
    fclose(fp);
    return 1;
  }

  status_t result = outBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &base);
  if (base == nullptr || result != NO_ERROR) {
    String8 reason;
    if (result != NO_ERROR) {
      reason.appendFormat(" Error Code: %d", result);
    } else {
      reason = "Failed to write to buffer";
    }
    GS_LOGE("Failed to take screenshot (%s)\n", reason.c_str());
    fclose(fp);
    return 1;
  }

  w = outBuffer->getWidth();
  h = outBuffer->getHeight();
  s = outBuffer->getStride();
  f = outBuffer->getPixelFormat();

  // encode to png format.
  if (pngEncode) {
    if (encodeToPng(fp, w, h, s, f, base)) {
      outBuffer->unlock();
      fclose(fp);
      return 1;
    }
    // write as raw format.
  } else {
    uint32_t c = dataSpaceToInt(outDataspace);
    fwrite(&w, 1, 4, fp);
    fwrite(&h, 1, 4, fp);
    fwrite(&f, 1, 4, fp);
    fwrite(&c, 1, 4, fp);
    size_t Bpp = bytesPerPixel(f);
    for (size_t i = 0; i < h; i++) {
      fwrite(base, 1, w * Bpp, fp);
      base = (void*)((char*)base + s * Bpp);
    }
  }
  outBuffer->unlock();
  fclose(fp);

  return 0;
}

} /* namespace mozilla */
