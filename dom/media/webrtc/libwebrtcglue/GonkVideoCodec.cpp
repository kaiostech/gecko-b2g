/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkVideoCodec.h"

#include "mozilla/StaticPrefs_media.h"
#include "WebrtcGonkVideoCodec.h"

namespace mozilla {

WebrtcVideoEncoder* GonkVideoCodec::CreateEncoder(
    const webrtc::SdpVideoFormat& aFormat) {
  if (!StaticPrefs::media_webrtc_gonkencoder()) {
    return nullptr;
  }
  return android::WebrtcGonkVideoEncoder::Create(aFormat);
}

WebrtcVideoDecoder* GonkVideoCodec::CreateDecoder(
    const webrtc::SdpVideoFormat& aFormat) {
  if (!StaticPrefs::media_webrtc_gonkdecoder()) {
    return nullptr;
  }
  return android::WebrtcGonkVideoDecoder::Create(aFormat);
}

}  // namespace mozilla
