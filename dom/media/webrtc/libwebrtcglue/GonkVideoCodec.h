/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_VIDEO_CODEC_H_
#define GONK_VIDEO_CODEC_H_

#include "VideoConduit.h"

namespace mozilla {

class GonkVideoCodec {
 public:
  /**
   * Create encoder object for codec format |aFormat|. Return |nullptr| when
   * failed.
   */
  static WebrtcVideoEncoder* CreateEncoder(
      const webrtc::SdpVideoFormat& aFormat);

  /**
   * Create decoder object for codec format |aFormat|. Return |nullptr| when
   * failed.
   */
  static WebrtcVideoDecoder* CreateDecoder(
      const webrtc::SdpVideoFormat& aFormat);
};

}  // namespace mozilla

#endif  // GONK_VIDEO_CODEC_H_
