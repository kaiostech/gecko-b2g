/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AudioOutput.h"

#include "mozilla/Logging.h"

namespace mozilla {
extern LazyLogModule gMediaDecoderLog;
}  // namespace mozilla

#define LOG(fmt, ...)                                          \
  MOZ_LOG(mozilla::gMediaDecoderLog, mozilla::LogLevel::Debug, \
          (fmt, ##__VA_ARGS__))
#define LOGV(fmt, ...)                                           \
  MOZ_LOG(mozilla::gMediaDecoderLog, mozilla::LogLevel::Verbose, \
          (fmt, ##__VA_ARGS__))

namespace android {

AudioOutput::AudioOutput(audio_session_t aSessionId, int aUid,
                         audio_stream_type_t aStreamType)
    : mCallbackCookie(nullptr),
      mCallback(nullptr),
      mCallbackData(nullptr),
      mUid(aUid),
      mSessionId(aSessionId),
      mStreamType(aStreamType) {}

AudioOutput::~AudioOutput() { Close(); }

ssize_t AudioOutput::FrameSize() const {
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->frameSize();
}

status_t AudioOutput::GetPosition(uint32_t* aPosition) const {
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->getPosition(aPosition);
}

status_t AudioOutput::SetVolume(float aVolume) {
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->setVolume(aVolume);
}

status_t AudioOutput::SetPlaybackRate(const AudioPlaybackRate& aRate) {
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->setPlaybackRate(aRate);
}

status_t AudioOutput::SetParameters(const String8& aKeyValuePairs) {
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->setParameters(aKeyValuePairs);
}

status_t AudioOutput::Open(uint32_t aSampleRate, int aChannelCount,
                           audio_channel_mask_t aChannelMask,
                           audio_format_t aFormat, AudioCallback aCb,
                           void* aCookie, audio_output_flags_t aFlags,
                           const audio_offload_info_t* aOffloadInfo) {
  mCallback = aCb;
  mCallbackCookie = aCookie;

  LOG("open(%u, %d, 0x%x, 0x%x, %d 0x%x)", aSampleRate, aChannelCount,
      aChannelMask, aFormat, mSessionId, aFlags);

  if (aChannelMask == CHANNEL_MASK_USE_CHANNEL_ORDER) {
    aChannelMask = audio_channel_out_mask_from_count(aChannelCount);
    if (0 == aChannelMask) {
      LOG("open() error, can\'t derive mask for %d audio channels",
          aChannelCount);
      return NO_INIT;
    }
  }

  sp<AudioTrack> t;
  CallbackData* newcbd = new CallbackData(this);

#if ANDROID_VERSION >= 33
  t = new AudioTrack(
      mStreamType,
      aSampleRate,
      aFormat,
      aChannelMask,
      nullptr);
#else
  t = new AudioTrack(
      mStreamType, aSampleRate, aFormat, aChannelMask,
      0,  // Offloaded tracks will get frame count from AudioFlinger
      aFlags, CallbackWrapper, newcbd,
      0,  // notification frames
      mSessionId, AudioTrack::TRANSFER_CALLBACK, aOffloadInfo, mUid);
#endif

  if ((!t.get()) || (t->initCheck() != NO_ERROR)) {
    LOG("Unable to create audio track");
    delete newcbd;
    return NO_INIT;
  }

  mCallbackData = newcbd;
  t->setVolume(1.0);

  mTrack = t;
  return NO_ERROR;
}

status_t AudioOutput::Start() {
  LOG("%s", __PRETTY_FUNCTION__);
  if (!mTrack.get()) {
    return NO_INIT;
  }
  mTrack->setVolume(1.0);
  return mTrack->start();
}

void AudioOutput::Stop() {
  LOG("%s", __PRETTY_FUNCTION__);
  if (mTrack.get()) {
    mTrack->stop();
  }
}

void AudioOutput::Flush() {
  if (mTrack.get()) {
    mTrack->flush();
  }
}

void AudioOutput::Pause() {
  if (mTrack.get()) {
    mTrack->pause();
  }
}

void AudioOutput::Close() {
  LOG("%s", __PRETTY_FUNCTION__);
  mTrack.clear();

  delete mCallbackData;
  mCallbackData = nullptr;
}

#if ANDROID_VERSION >= 33
#define BUFFER_DATA buffer->data()
#define BUFFER_SIZE buffer->size()
#else
#define BUFFER_DATA buffer->raw
#define BUFFER_SIZE buffer->size
#endif

// static
void AudioOutput::CallbackWrapper(int aEvent, void* aCookie, void* aInfo) {
  CallbackData* data = (CallbackData*)aCookie;
  data->Lock();
  AudioOutput* me = data->GetOutput();
  AudioTrack::Buffer* buffer = (AudioTrack::Buffer*)aInfo;
  if (!me) {
    // no output set, likely because the track was scheduled to be reused
    // by another player, but the format turned out to be incompatible.
    data->Unlock();
    if (buffer) {
#if ANDROID_VERSION < 33
      // FIXME: mSize is private in 33.
      buffer->size = 0;
#endif
    }
    return;
  }

  switch (aEvent) {
    case AudioTrack::EVENT_MORE_DATA: {
      size_t actualSize =
          (*me->mCallback)(me, BUFFER_DATA, BUFFER_SIZE, me->mCallbackCookie,
                           CB_EVENT_FILL_BUFFER);

      if (actualSize == 0 && BUFFER_SIZE > 0) {
        // Log when no data is returned from the callback.
        // (1) We may have no data (especially with network streaming sources).
        // (2) We may have reached the EOS and the audio track is not stopped
        //     yet.
        // Note that AwesomePlayer/AudioPlayer will only return zero size when
        // it reaches the EOS. NuPlayerRenderer will return zero when it doesn't
        // have data (it doesn't block to fill).
        //
        // This is a benign busy-wait, with the next data request generated 10
        // ms or more later; nevertheless for power reasons, we don't want to
        // see too many of these.
        LOG("Callback wrapper: empty buffer returned");
      }

#if ANDROID_VERSION < 33
      // FIXME: mSize is private in 33.
      buffer->size = actualSize;
#endif
    } break;

    case AudioTrack::EVENT_STREAM_END:
      LOG("Callback wrapper: EVENT_STREAM_END");
      (*me->mCallback)(me, nullptr /* buffer */, 0 /* size */,
                       me->mCallbackCookie, CB_EVENT_STREAM_END);
      break;

    case AudioTrack::EVENT_NEW_IAUDIOTRACK:
      LOG("Callback wrapper: EVENT_TEAR_DOWN");
      (*me->mCallback)(me, nullptr /* buffer */, 0 /* size */,
                       me->mCallbackCookie, CB_EVENT_TEAR_DOWN);
      break;

    default:
      LOG("received unknown event type: %d in Callback wrapper!", aEvent);
      break;
  }

  data->Unlock();
}

}  // namespace android
