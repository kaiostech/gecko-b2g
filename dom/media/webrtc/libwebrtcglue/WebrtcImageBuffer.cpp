/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcImageBuffer.h"

#ifdef MOZ_WIDGET_GONK
#  include "GrallocImages.h"
#endif

namespace mozilla {

#ifdef MOZ_WIDGET_GONK
namespace {

class GraphicBufferHolder : public rtc::RefCountInterface {
 public:
  GraphicBufferHolder(android::sp<android::GraphicBuffer> aGraphicBuffer)
      : mGraphicBuffer(aGraphicBuffer) {}

 protected:
  ~GraphicBufferHolder() {
    if (mGraphicBuffer) {
      mGraphicBuffer->unlock();
    }
  }

 private:
  android::sp<android::GraphicBuffer> mGraphicBuffer;
};

}  // anonymous namespace

rtc::scoped_refptr<webrtc::I420BufferInterface> ImageBuffer::GrallocToI420() {
  RefPtr<layers::GrallocImage> grallocImage = mImage->AsGrallocImage();
  MOZ_ASSERT(grallocImage);

  android::sp<android::GraphicBuffer> graphicBuffer =
      grallocImage->GetGraphicBuffer();
  auto pixelFormat = graphicBuffer->getPixelFormat();
  MOZ_ASSERT(pixelFormat == HAL_PIXEL_FORMAT_YV12);
  if (pixelFormat != HAL_PIXEL_FORMAT_YV12) {
    return nullptr;
  }

  android_ycbcr ycbcr = {};
  auto status = graphicBuffer->lockYCbCr(
      android::GraphicBuffer::USAGE_SW_READ_MASK, &ycbcr);
  if (status != android::OK) {
    return nullptr;
  }

  rtc::scoped_refptr<GraphicBufferHolder> holder(
      new rtc::RefCountedObject<GraphicBufferHolder>(graphicBuffer));
  MOZ_ASSERT(ycbcr.chroma_step == 1);

  return webrtc::WrapI420Buffer(graphicBuffer->getWidth(),
                                graphicBuffer->getHeight(),
                                static_cast<uint8_t*>(ycbcr.y), ycbcr.ystride,
                                static_cast<uint8_t*>(ycbcr.cb), ycbcr.cstride,
                                static_cast<uint8_t*>(ycbcr.cr), ycbcr.cstride,
                                [holder] { /* keep reference alive*/ });
}
#endif

}  // namespace mozilla
