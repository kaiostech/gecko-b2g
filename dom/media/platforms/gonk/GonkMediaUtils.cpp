/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMediaUtils.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaCodecList.h>

#include "AudioCompactor.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "XiphExtradata.h"

namespace android {

#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sCodecLog("GonkMediaUtils");
#define LOGE(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Info, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOGV(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

GonkBufferWriter::GonkBufferWriter(const sp<MediaCodecBuffer>& aBuffer)
    : mBuffer(aBuffer) {}

void GonkBufferWriter::Clear() { mBuffer->setRange(0, 0); }

bool GonkBufferWriter::Append(const uint8_t* aData, size_t aSize) {
  if (mBuffer->offset() + mBuffer->size() + aSize > mBuffer->capacity()) {
    return false;
  }

  memcpy(mBuffer->data() + mBuffer->size(), aData, aSize);
  mBuffer->setRange(mBuffer->offset(), mBuffer->size() + aSize);
  return true;
}

/* static */
AudioEncoding GonkMediaUtils::PreferredPcmEncoding() {
#ifdef MOZ_SAMPLE_TYPE_S16
  return kAudioEncodingPcm16bit;
#else
  return kAudioEncodingPcmFloat;
#endif
}

/* static */
size_t GonkMediaUtils::GetAudioSampleSize(AudioEncoding aEncoding) {
  switch (aEncoding) {
    case kAudioEncodingPcm16bit:
      return sizeof(int16_t);
    case kAudioEncodingPcmFloat:
      return sizeof(float);
    default:
      TRESPASS("Invalid AudioEncoding %d", aEncoding);
      return 0;
  }
}

/* static */
GonkMediaUtils::PcmCopy GonkMediaUtils::CreatePcmCopy(const uint8_t* aSource,
                                                      size_t aSourceBytes,
                                                      uint32_t aChannels,
                                                      AudioEncoding aEncoding) {
  class ShortToFloat {
   public:
    ShortToFloat(const uint8_t* aSource, size_t aSourceBytes,
                 uint32_t aChannels)
        : mSource(aSource),
          mSourceBytes(aSourceBytes),
          mChannels(aChannels),
          mSourceOffset(0) {}

    uint32_t operator()(AudioDataValue* aBuffer, uint32_t aSamples) {
      size_t sourceFrames = (mSourceBytes - mSourceOffset) / SourceFrameSize();
      size_t maxFrames = std::min(sourceFrames, size_t(aSamples / mChannels));

      auto* src = reinterpret_cast<const int16_t*>(mSource + mSourceOffset);
      auto* dst = aBuffer;
      for (size_t i = 0; i < maxFrames * mChannels; i++) {
        *dst++ = mozilla::AudioSampleToFloat(*src++);
      }

      mSourceOffset += maxFrames * SourceFrameSize();
      return maxFrames;
    }

   private:
    size_t SourceFrameSize() { return sizeof(int16_t) * mChannels; }

    const uint8_t* const mSource;
    const size_t mSourceBytes;
    const uint32_t mChannels;
    size_t mSourceOffset;
  };

  if (aEncoding == PreferredPcmEncoding()) {
    return mozilla::AudioCompactor::NativeCopy(aSource, aSourceBytes,
                                               aChannels);
  } else {
    CHECK(aEncoding == kAudioEncodingPcm16bit);
    CHECK(PreferredPcmEncoding() == kAudioEncodingPcmFloat);
    return ShortToFloat(aSource, aSourceBytes, aChannels);
  }
}

/* static */
sp<AMessage> GonkMediaUtils::GetMediaCodecConfig(
    const mozilla::TrackInfo* aInfo) {
  using mozilla::AacCodecSpecificData;
  using mozilla::AudioCodecSpecificBinaryBlob;
  using mozilla::BigEndian;
  using mozilla::CheckedUint32;
  using mozilla::FlacCodecSpecificData;
  using mozilla::MediaByteBuffer;
  using mozilla::OpusCodecSpecificData;
  using mozilla::VorbisCodecSpecificData;
  using mozilla::XiphExtradataToHeaders;

  sp<AMessage> format = new AMessage();

  auto& mime = aInfo->mMimeType;
  if (mime.EqualsLiteral("video/vp8")) {
    format->setString("mime", "video/x-vnd.on2.vp8");
  } else if (mime.EqualsLiteral("video/vp9")) {
    format->setString("mime", "video/x-vnd.on2.vp9");
  } else {
    format->setString("mime", mime.get());
  }

  if (auto* info = aInfo->GetAsVideoInfo()) {
    format->setInt32("width", info->mImage.width);
    format->setInt32("height", info->mImage.height);
    if (mime.EqualsLiteral("video/mp4v-es") && info->mExtraData) {
      auto csdBuffer = ABuffer::CreateAsCopy(info->mExtraData->Elements(),
                                             info->mExtraData->Length());
      format->setBuffer("csd-0", csdBuffer);
    }
  } else if (auto* info = aInfo->GetAsAudioInfo()) {
    format->setInt32("channel-count", info->mChannels);
    format->setInt32("sample-rate", info->mRate);
    format->setInt32("aac-profile", info->mProfile);
    format->setInt32("pcm-encoding", PreferredPcmEncoding());

    auto& csd = info->mCodecSpecificConfig;
    if (mime.EqualsLiteral("audio/opus") && csd.is<OpusCodecSpecificData>()) {
      auto& opusCsd = csd.as<OpusCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = opusCsd.mHeadersBinaryBlob;
      // AOSP Opus decoder requires 1) codec specific data, 2) codec delay and
      // 3) seek preroll, but Gecko demuxer only provides codec delay, so set
      // seek preroll to 0.
      static const int32_t kOpusSampleRate = 48000;
      int64_t codecDelayNs =
          opusCsd.mContainerCodecDelayFrames * 1000000000ll / kOpusSampleRate;
      int64_t seekPreroll = 0;
      auto delayBuffer = ABuffer::CreateAsCopy(&codecDelayNs, sizeof(int64_t));
      auto prerollBuffer = ABuffer::CreateAsCopy(&seekPreroll, sizeof(int64_t));
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
      format->setBuffer("csd-1", delayBuffer);
      format->setBuffer("csd-2", prerollBuffer);
    } else if (mime.EqualsLiteral("audio/vorbis") &&
               csd.is<VorbisCodecSpecificData>()) {
      auto& vorbisCsd = csd.as<VorbisCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = vorbisCsd.mHeadersBinaryBlob;
      AutoTArray<unsigned char*, 4> headers;
      AutoTArray<size_t, 4> headerLens;
      if (!XiphExtradataToHeaders(headers, headerLens, blob->Elements(),
                                  blob->Length())) {
        LOGE("could not get Vorbis header");
        return nullptr;
      }
      for (size_t i = 0; i < headers.Length(); i++) {
        auto header = ABuffer::CreateAsCopy(headers[i], headerLens[i]);
        if (header->size() < 1) {
          continue;
        }
        switch (header->data()[0]) {
          case 0x1:  // Identification header
            format->setBuffer("csd-0", header);
            break;
          case 0x5:  // Setup header
            format->setBuffer("csd-1", header);
            break;
        }
      }
    } else if (mime.EqualsLiteral("audio/flac") &&
               csd.is<FlacCodecSpecificData>()) {
      auto& flacCsd = csd.as<FlacCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = flacCsd.mStreamInfoBinaryBlob;
      // mStreamInfoBinaryBlob contains a stream info block without its block
      // header, but AOSP FLAC decoder expects complete metadata that begins
      // with a "fLaC" stream marker and contains one or more metadata blocks
      // with the "last" flag properly set at the last block, so prepend a
      // fake header here with only one stream info block.
      nsTArray<uint8_t> meta = {0x66, 0x4c, 0x61, 0x43, 0x80, 0x00, 0x00, 0x22};
      meta.AppendElements(*blob);
      auto metaBuffer = ABuffer::CreateAsCopy(meta.Elements(), meta.Length());
      format->setBuffer("csd-0", metaBuffer);
    } else if (mime.EqualsLiteral("audio/mp4a-latm") &&
               csd.is<AacCodecSpecificData>()) {
      auto& aacCsd = csd.as<AacCodecSpecificData>();
      RefPtr<MediaByteBuffer> blob = aacCsd.mDecoderConfigDescriptorBinaryBlob;
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
    } else if (csd.is<AudioCodecSpecificBinaryBlob>()) {
      LOGD("setting generic codec config data for %s", mime.get());
      auto& audioCsd = csd.as<AudioCodecSpecificBinaryBlob>();
      RefPtr<MediaByteBuffer> blob = audioCsd.mBinaryBlob;
      auto csdBuffer = ABuffer::CreateAsCopy(blob->Elements(), blob->Length());
      format->setBuffer("csd-0", csdBuffer);
    } else {
      LOGD("no codec config data for %s", mime.get());
    }
  } else {
    return nullptr;
  }
  return format;
}

/* static */
sp<GonkCryptoInfo> GonkMediaUtils::GetCryptoInfo(
    const mozilla::MediaRawData* aSample) {
  using mozilla::CheckedUint32;
  using mozilla::CryptoScheme;

  auto& cryptoObj = aSample->mCrypto;
  if (!cryptoObj.IsEncrypted()) {
    return nullptr;
  }

  sp<GonkCryptoInfo> cryptoInfo = new GonkCryptoInfo;

  // |mPlainSizes| and |mEncryptedSizes| should have the same length. If not,
  // just pad the shorter one with zero.
  auto numSubSamples = std::max(cryptoObj.mPlainSizes.Length(),
                                cryptoObj.mEncryptedSizes.Length());
  auto& subSamples = cryptoInfo->mSubSamples;
  uint32_t totalSubSamplesSize = 0;
  for (size_t i = 0; i < numSubSamples; i++) {
    auto plainSize = cryptoObj.mPlainSizes.SafeElementAt(i, 0);
    auto encryptedSize = cryptoObj.mEncryptedSizes.SafeElementAt(i, 0);
    totalSubSamplesSize += plainSize + encryptedSize;
    subSamples.push_back({plainSize, encryptedSize});
  }

  // Size of codec specific data("CSD") for MediaCodec usage should be included
  // in the 1st plain size if it exists.
  if (!subSamples.empty()) {
    uint32_t codecSpecificDataSize = aSample->Size() - totalSubSamplesSize;
    // This shouldn't overflow as the the plain size should be UINT16_MAX at
    // most, and the CSD should never be that large. Checked int acts like a
    // diagnostic assert here to help catch if we ever have insane inputs.
    CheckedUint32 newLeadingPlainSize{subSamples[0].mNumBytesOfClearData};
    newLeadingPlainSize += codecSpecificDataSize;
    subSamples[0].mNumBytesOfClearData = newLeadingPlainSize.value();
  }

  static const int kExpectedIVLength = 16;
  cryptoInfo->mIV.assign(cryptoObj.mIV.begin(), cryptoObj.mIV.end());
  cryptoInfo->mIV.resize(kExpectedIVLength, 0);  // padding with 0
  static const int kExpectedKeyLength = 16;
  cryptoInfo->mKey.assign(cryptoObj.mKeyId.begin(), cryptoObj.mKeyId.end());
  cryptoInfo->mKey.resize(kExpectedKeyLength, 0);  // padding with 0

  cryptoInfo->mPattern.mEncryptBlocks = cryptoObj.mCryptByteBlock;
  cryptoInfo->mPattern.mSkipBlocks = cryptoObj.mSkipByteBlock;

  switch (cryptoObj.mCryptoScheme) {
    case CryptoScheme::None:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_Unencrypted;
      break;
    case CryptoScheme::Cenc:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_AES_CTR;
      break;
    case CryptoScheme::Cbcs:
      cryptoInfo->mMode = android::CryptoPlugin::kMode_AES_CBC;
      break;
  }
  return cryptoInfo;
}

static void FilterCodecs(std::vector<AString>& aCodecs, bool aAllowC2) {
  // List of problematic codecs that will be removed from aCodecs.
  static char const* const kDeniedCodecs[] = {"OMX.qti.audio.decoder.flac"};

  std::vector<AString> temp;
  for (auto& name : aCodecs) {
    if (name.startsWithIgnoreCase("c2.") && !aAllowC2) {
      continue;
    }
    bool denied = false;
    for (const char* deniedCodec : kDeniedCodecs) {
      if (name == deniedCodec) {
        denied = true;
        break;
      }
    }
    if (denied) {
      continue;
    }
    temp.push_back(name);
  }
  aCodecs.swap(temp);
}

/* static */
std::vector<AString> GonkMediaUtils::FindMatchingCodecs(const char* aMime,
                                                        bool aEncoder) {
  AString mime(aMime);
  if (mime == "video/vp8") {
    mime = "video/x-vnd.on2.vp8";
  } else if (mime == "video/vp9") {
    mime = "video/x-vnd.on2.vp9";
  }

  // The old video decoder path (GonkMediaDataDecoder/GonkVideoDecoderManager)
  // doesn't support codec2 nor any software components, because this path uses
  // a hack to acquire GraphicBuffers from ACodec and doesn't work as expected
  // on those components.
  bool allowSoftwareOrC2 =
      mime.startsWithIgnoreCase("audio/") || aEncoder ||
      mozilla::StaticPrefs::media_gonkmediacodec_video_enabled();

  uint32_t flags = allowSoftwareOrC2 ? 0 : MediaCodecList::kHardwareCodecsOnly;
  Vector<AString> matches;
  MediaCodecList::findMatchingCodecs(mime.c_str(), aEncoder, flags, &matches);

  // Convert android::Vector to std::vector.
  std::vector<AString> codecs(matches.begin(), matches.end());
  FilterCodecs(codecs, allowSoftwareOrC2);
  return codecs;
}

}  // namespace android
