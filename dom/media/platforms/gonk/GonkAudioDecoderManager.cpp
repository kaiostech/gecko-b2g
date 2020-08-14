/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaCodecProxy.h"
#include <OMX_IVCommon.h>
#include <gui/Surface.h>
#include <media/ICrypto.h>
#include "GonkAudioDecoderManager.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaDataBase.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/openmax/OMX_Audio.h>
#include "MediaData.h"
#include "MediaInfo.h"

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

GonkAudioDecoderManager::GonkAudioDecoderManager(const AudioInfo& aConfig)
    : mAudioChannels(aConfig.mChannels),
      mAudioRate(aConfig.mRate),
      mAudioProfile(aConfig.mProfile),
      mAudioCompactor(mAudioQueue) {
  MOZ_COUNT_CTOR(GonkAudioDecoderManager);
  MOZ_ASSERT(mAudioChannels);
  mCodecSpecificData = aConfig.mCodecSpecificConfig;
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
  status_t rv = OK;
  if (!InitLoopers(MediaData::Type::AUDIO_DATA)) {
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
  sp<AMessage> format = new AMessage;
  // Fixed values
  LOG("Configure audio mime type:%s, chan no:%d, sample-rate:%d, profile:%d",
      mMimeType.get(), mAudioChannels, mAudioRate, mAudioProfile);
  format->setString("mime", mMimeType.get());
  format->setInt32("channel-count", mAudioChannels);
  format->setInt32("sample-rate", mAudioRate);
  format->setInt32("aac-profile", mAudioProfile);
  status_t err = mDecoder->configure(format, nullptr, nullptr, 0);
  if (err != OK || !mDecoder->Prepare()) {
    return false;
  }

  if (mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    // Under some conditions, mediacodec cannot return ontime. We could get
    // EAGAIN error. Try 3 times to see whether Gecko can initialize the it.
    const int tryCount = 3;

    for (int i = 0; i < tryCount; ++i) {
      rv = mDecoder->Input(
          mCodecSpecificData->Elements(), mCodecSpecificData->Length(), 0,
          android::MediaCodec::BUFFER_FLAG_CODECCONFIG, CODECCONFIG_TIMEOUT_US);
      if (rv == OK) {
        break;
      } else if (rv != -EAGAIN) {
        LOGE("Failed to input codec specific data! rv=%x", rv);
        return false;
      }

      LOG("MediaCodec is busy %d", i);
    }
  }

  return rv == OK;
}

nsresult GonkAudioDecoderManager::CreateAudioData(MediaBuffer* aBuffer,
                                                  int64_t aStreamOffset) {
  if (!(aBuffer != nullptr && aBuffer->data() != nullptr)) {
    LOGE("Audio Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t timeUs;
  if (!aBuffer->meta_data().findInt64(kKeyTime, &timeUs)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to
    // ignore quoted from Android's AwesomePlayer.cpp
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mLastTime > timeUs) {
    LOGE("Output decoded sample time is revert. time=%lld", timeUs);
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_AVAILABLE;
  }
  mLastTime = timeUs;

  const uint8_t* data = static_cast<const uint8_t*>(aBuffer->data());
  size_t dataOffset = aBuffer->range_offset();
  size_t size = aBuffer->range_length();

  uint64_t frames = size / (2 * mAudioChannels);

  CheckedInt64 duration = FramesToUsecs(frames, mAudioRate);
  if (!duration.isValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  typedef AudioCompactor::NativeCopy OmxCopy;
  mAudioCompactor.Push(aStreamOffset, timeUs, mAudioRate, frames,
                       mAudioChannels,
                       OmxCopy(data + dataOffset, size, mAudioChannels));
  return NS_OK;
}

nsresult GonkAudioDecoderManager::GetOutput(
    int64_t aStreamOffset, MediaDataDecoder::DecodedData& aOutData) {
  aOutData.Clear();
  if (mAudioQueue.GetSize() > 0) {
    while (mAudioQueue.GetSize() > 0) {
      aOutData.AppendElement(mAudioQueue.PopFront());
    }
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_ABORT : NS_OK;
  }

  MediaBuffer* audioBuffer = nullptr;
  status_t err;
  err = mDecoder->Output(&audioBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);
  AutoReleaseMediaBuffer a(audioBuffer, mDecoder.get());

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
          audioCodecFormat == nullptr) {
        return NS_ERROR_UNEXPECTED;
      }

      int32_t codec_channel_count = 0;
      int32_t codec_sample_rate = 0;

      if (!audioCodecFormat->findInt32("channel-count", &codec_channel_count) ||
          !audioCodecFormat->findInt32("sample-rate", &codec_sample_rate)) {
        return NS_ERROR_UNEXPECTED;
      }

      // Update AudioInfo
      mAudioChannels = codec_channel_count;
      mAudioRate = codec_sample_rate;
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
      NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
      MOZ_ASSERT(mAudioQueue.GetSize() > 0);
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
    // Return NS_ERROR_ABORT at the last sample.
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_ABORT : NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void GonkAudioDecoderManager::ProcessFlush() {
  LOG("FLUSH<<<");
  mAudioQueue.Reset();
  LOG(">>>FLUSH");
  GonkDecoderManager::ProcessFlush();
}

void GonkAudioDecoderManager::ResetEOS() {
  LOG("ResetEOS(<<<");
  mAudioQueue.Reset();
  LOG(">>>ResetEOS(");
  GonkDecoderManager::ResetEOS();
}

}  // namespace mozilla
