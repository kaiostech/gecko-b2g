/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaCodecProxy.h"
#include <OMX_IVCommon.h>
#include <gui/Surface.h>
#include "GonkAudioDecoderManager.h"
#include "GonkMediaUtils.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaDataBase.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/openmax/OMX_Audio.h>
#include <mediadrm/ICrypto.h>
#include "MediaData.h"
#include "MediaInfo.h"
#include "XiphExtradata.h"

#define CODECCONFIG_TIMEOUT_US 40000LL
#define READ_OUTPUT_BUFFER_TIMEOUT_US 0LL

mozilla::LazyLogModule gGonkAudioDecoderManagerLog("GonkAudioDecoderManager");
#undef LOG
#undef LOGE

#define LOG(x, ...)                                              \
  MOZ_LOG(gGonkAudioDecoderManagerLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define LOGE(x, ...)                                             \
  MOZ_LOG(gGonkAudioDecoderManagerLog, mozilla::LogLevel::Error, \
          ("%p " x, this, ##__VA_ARGS__))

using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {

GonkAudioDecoderManager::GonkAudioDecoderManager(const AudioInfo& aConfig,
                                                 CDMProxy* aProxy)
    : GonkDecoderManager(aProxy),
      mConfig(aConfig),
      mAudioChannels(aConfig.mChannels),
      mAudioRate(aConfig.mRate),
      mPcmEncoding(android::kAudioEncodingPcm16bit),
      mAudioProfile(aConfig.mProfile),
      mCodecSpecificConfig(aConfig.mCodecSpecificConfig),
      mAudioCompactor(mAudioQueue) {
  MOZ_COUNT_CTOR(GonkAudioDecoderManager);
  MOZ_ASSERT(mAudioChannels);
  mMimeType = aConfig.mMimeType;
}

GonkAudioDecoderManager::~GonkAudioDecoderManager() {
  MOZ_COUNT_DTOR(GonkAudioDecoderManager);
}

RefPtr<MediaDataDecoder::InitPromise> GonkAudioDecoderManager::Init() {
  if (InitMediaCodecProxy()) {
    return InitPromise::CreateAndResolve(GetTrackType(), __func__);
  } else {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
}

bool GonkAudioDecoderManager::InitMediaCodecProxy() {
  if (!InitThreads(MediaData::Type::AUDIO_DATA)) {
    return false;
  }

  mDecoder =
      MediaCodecProxy::CreateByType(mDecodeLooper, mMimeType.get(), false);
  if (!mDecoder.get()) {
    return false;
  }
  if (!mDecoder->AllocateAudioMediaCodec()) {
    mDecoder = nullptr;
    return false;
  }
  // Fixed values
  LOG("Configure audio mime type:%s, chan no:%d, sample-rate:%d, profile:%d",
      mMimeType.get(), mAudioChannels, mAudioRate, mAudioProfile);

  sp<AMessage> format = GonkMediaUtils::GetMediaCodecConfig(&mConfig);
  status_t err = mDecoder->configure(format, nullptr, GetCrypto(), 0);
  if (err != OK) {
    return false;
  }

  return mDecoder->Prepare();
}

nsresult GonkAudioDecoderManager::CreateAudioData(
    const sp<SimpleMediaBuffer>& aBuffer, int64_t aStreamOffset) {
  if (!aBuffer || !aBuffer->Data()) {
    LOGE("Audio Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t timeUs;
  if (!aBuffer->MetaData().findInt64(kKeyTime, &timeUs)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aBuffer->Size() == 0) {
    // Some decoders may return spurious empty buffers that we just want to
    // ignore quoted from Android's AwesomePlayer.cpp
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mLastTime > timeUs) {
    LOGE("Output decoded sample time is revert. time=%ld", timeUs);
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_AVAILABLE;
  }
  mLastTime = timeUs;

  const uint8_t* data = static_cast<const uint8_t*>(aBuffer->Data());
  size_t size = aBuffer->Size();

  uint64_t frames = size / (GonkMediaUtils::GetAudioSampleSize(mPcmEncoding) *
                            mAudioChannels);

  CheckedInt64 duration = FramesToUsecs(frames, mAudioRate);
  if (!duration.isValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  mAudioCompactor.Push(
      aStreamOffset, timeUs, mAudioRate, frames, mAudioChannels,
      GonkMediaUtils::CreatePcmCopy(data, size, mAudioChannels, mPcmEncoding));
  return NS_OK;
}

nsresult GonkAudioDecoderManager::GetOutput(
    int64_t aStreamOffset, MediaDataDecoder::DecodedData& aOutData) {
  aOutData.Clear();
  if (mAudioQueue.AtEndOfStream()) {
    return NS_ERROR_DOM_MEDIA_END_OF_STREAM;
  }
  if (mAudioQueue.GetSize() > 0) {
    while (mAudioQueue.GetSize() > 0) {
      aOutData.AppendElement(mAudioQueue.PopFront());
    }
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_DOM_MEDIA_END_OF_STREAM
                                       : NS_OK;
  }

  sp<SimpleMediaBuffer> audioBuffer;
  status_t err;
  err = mDecoder->Output(&audioBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);

  auto raii = MakeScopeExit([&] { mDecoder->ReleaseMediaBuffer(audioBuffer); });

  switch (err) {
    case OK: {
      nsresult rv = CreateAudioData(audioBuffer, aStreamOffset);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case android::INFO_FORMAT_CHANGED: {
      // If the format changed, update our cached info.
      LOG("Decoder format changed");
      sp<AMessage> audioCodecFormat;

      if (mDecoder->getOutputFormat(&audioCodecFormat) != OK ||
          !audioCodecFormat) {
        return NS_ERROR_UNEXPECTED;
      }

      int32_t codec_channel_count = 0;
      int32_t codec_sample_rate = 0;
      int32_t pcmEncoding = kAudioEncodingPcm16bit;

      if (!audioCodecFormat->findInt32("channel-count", &codec_channel_count) ||
          !audioCodecFormat->findInt32("sample-rate", &codec_sample_rate)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (!audioCodecFormat->findInt32("pcm-encoding", &pcmEncoding)) {
        LOG("Failed to find pcm-encoding, assume 16 bit integer");
      }

      // Update AudioInfo
      mAudioChannels = codec_channel_count;
      mAudioRate = codec_sample_rate;
      mPcmEncoding = static_cast<AudioEncoding>(pcmEncoding);
      return GetOutput(aStreamOffset, aOutData);
    }
    case android::INFO_OUTPUT_BUFFERS_CHANGED: {
      LOG("Info Output Buffers Changed");
      if (mDecoder->UpdateOutputBuffers()) {
        return GetOutput(aStreamOffset, aOutData);
      }
      return NS_ERROR_FAILURE;
    }
    case -EAGAIN: {
      return NS_ERROR_NOT_AVAILABLE;
    }
    case android::ERROR_END_OF_STREAM: {
      LOG("Got EOS frame!");
      nsresult rv = CreateAudioData(audioBuffer, aStreamOffset);
      // CreateAudioData returns NS_ERROR_NOT_AVAILABLE due to
      // some decoders may return EOS with empty buffers that we just want to
      // ignore quoted from Android's AwesomePlayer.cpp
      if (rv != NS_ERROR_NOT_AVAILABLE) {
        NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_MEDIA_END_OF_STREAM);
        MOZ_ASSERT(mAudioQueue.GetSize() > 0);
      }
      mAudioQueue.Finish();
      break;
    }
    case -ETIMEDOUT: {
      LOG("Timeout. can try again next time");
      return NS_ERROR_UNEXPECTED;
    }
    default: {
      LOGE("Decoder failed, err=%d", err);
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mAudioQueue.GetSize() > 0) {
    while (mAudioQueue.GetSize() > 0) {
      aOutData.AppendElement(mAudioQueue.PopFront());
    }
    // Return NS_ERROR_DOM_MEDIA_END_OF_STREAM at the last sample.
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_DOM_MEDIA_END_OF_STREAM
                                       : NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void GonkAudioDecoderManager::FlushInternal() {
  LOG("FLUSH<<<");
  mAudioQueue.Reset();
  LOG(">>>FLUSH");
}

}  // namespace mozilla
