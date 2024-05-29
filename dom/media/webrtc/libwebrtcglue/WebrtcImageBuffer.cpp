/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcImageBuffer.h"

#ifdef MOZ_WIDGET_GONK
#  include "GonkMediaUtils.h"
#endif

namespace mozilla {

#ifdef MOZ_WIDGET_GONK
rtc::scoped_refptr<webrtc::I420BufferInterface> ImageBuffer::GrallocToI420() {
  using android::GonkImageHandle;

  auto handle = GonkImageHandle::From(mImage);
  if (!handle) {
    return nullptr;
  }
  if (handle->Format() != GonkImageHandle::ColorFormat::I420) {
    // TODO: support other formats.
    return nullptr;
  }
  // clang-format off
  return webrtc::WrapI420Buffer(
      static_cast<int>(handle->Width()),
      static_cast<int>(handle->Height()),
      handle->Data(0), static_cast<int>(handle->Stride(0)),
      handle->Data(1), static_cast<int>(handle->Stride(1)),
      handle->Data(2), static_cast<int>(handle->Stride(2)),
      [handle] { /* keep reference alive*/ });
  // clang-format on
}
#endif

}  // namespace mozilla
