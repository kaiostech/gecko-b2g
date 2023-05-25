/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_MEDIA_UTILS_H
#define GONK_MEDIA_UTILS_H

#include <media/hardware/CryptoAPI.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/RefBase.h>

#include <vector>

namespace mozilla {
class MediaRawData;
class TrackInfo;
}  // namespace mozilla

namespace android {

class GonkCryptoInfo : public RefBase {
 public:
  CryptoPlugin::Mode mMode = CryptoPlugin::kMode_Unencrypted;
  CryptoPlugin::Pattern mPattern = {0, 0};
  std::vector<CryptoPlugin::SubSample> mSubSamples;
  std::vector<uint8_t> mKey;
  std::vector<uint8_t> mIV;
};

class GonkMediaUtils {
 public:
  static sp<AMessage> GetMediaCodecConfig(const mozilla::TrackInfo* aInfo);

  static sp<GonkCryptoInfo> GetCryptoInfo(const mozilla::MediaRawData* aSample);
};

}  // namespace android

#endif  // GONK_MEDIA_UTILS_H
