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

#if ANDROID_VERSION >= 33
#  include <mediautils/Synchronization.h>
#endif

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

// Common code shared by new callback and legacy callback.
class AudioOutput::AudioTrackCallbackBase {
 protected:
  size_t OnMoreData(void* aBuffer, size_t aSize) {
    LOG("AudioTrackCallback: EVENT_MORE_DATA");
    Mutex::Autolock lock(mLock);
    sp<AudioOutput> me = GetOutput();
    if (!me) {
      return 0;
    }

    size_t actualSize = (*me->mCallback)(
        me.get(), aBuffer, aSize, me->mCallbackCookie, CB_EVENT_FILL_BUFFER);

    // Log when no data is returned from the callback.
    // (1) We may have no data (especially with network streaming sources).
    // (2) We may have reached the EOS and the audio track is not stopped yet.
    // Note that AwesomePlayer/AudioPlayer will only return zero size when it
    // reaches the EOS. NuPlayerRenderer will return zero when it doesn't have
    // data (it doesn't block to fill).
    //
    // This is a benign busy-wait, with the next data request generated 10 ms or
    // more later; nevertheless for power reasons, we don't want to see too many
    // of these.

    if (actualSize == 0 && aSize > 0) {
      LOG("AudioTrackCallback: empty buffer returned");
    }
    return actualSize;
  }

  void OnUnderrun() { LOG("AudioTrackCallback: EVENT_UNDERRUN (discarded)"); }

  void OnStreamEnd() {
    Mutex::Autolock lock(mLock);
    sp<AudioOutput> me = GetOutput();
    if (!me) {
      return;
    }
    LOG("AudioTrackCallback: deliver EVENT_STREAM_END");
    (*me->mCallback)(me.get(), nullptr /* buffer */, 0 /* size */,
                     me->mCallbackCookie, CB_EVENT_STREAM_END);
  }

  void OnNewIAudioTrack() {
    Mutex::Autolock lock(mLock);
    sp<AudioOutput> me = GetOutput();
    if (!me) {
      return;
    }
    LOG("AudioTrackCallback: deliver EVENT_TEAR_DOWN");
    (*me->mCallback)(me.get(), nullptr /* buffer */, 0 /* size */,
                     me->mCallbackCookie, CB_EVENT_TEAR_DOWN);
  }

  virtual sp<AudioOutput> GetOutput() const = 0;

 private:
  mutable Mutex mLock;
};

#if ANDROID_VERSION >= 33
// New AudioTrack callback which implements IAudioTrackCallback interface.
class AudioOutput::AudioTrackCallback final
    : public AudioOutput::AudioTrackCallbackBase,
      public AudioTrack::IAudioTrackCallback {
 public:
  size_t onMoreData(const AudioTrack::Buffer& aBuffer) override {
    return OnMoreData(aBuffer.data(), aBuffer.size());
  }

  void onUnderrun() override { OnUnderrun(); }

  void onNewIAudioTrack() override { OnNewIAudioTrack(); }

  void onStreamEnd() override { OnStreamEnd(); }

  explicit AudioTrackCallback(const wp<AudioOutput>& aCookie) {
    mData = aCookie;
  }

 private:
  mediautils::atomic_wp<AudioOutput> mData;
  sp<AudioOutput> GetOutput() const override { return mData.load().promote(); }
  DISALLOW_EVIL_CONSTRUCTORS(AudioTrackCallback);
};

#else
// Legacy AudioTrack callback uses C-style function pointer. Although AOSP 13
// also supports legacy callback, AudioTrack::Buffer size is no longer
// modifiable and there is no way to report actual data size when handling
// EVENT_MORE_DATA. Therefore on AOSP 13 we should always use the new callback
// above.
class AudioOutput::AudioTrackCallback final
    : public AudioOutput::AudioTrackCallbackBase,
      public RefBase {
 public:
  static void LegacyCallback(int aEvent, void* aCookie, void* aInfo) {
    auto* thiz = static_cast<AudioTrackCallback*>(aCookie);
    switch (aEvent) {
      case AudioTrack::EVENT_MORE_DATA: {
        auto* buffer = static_cast<AudioTrack::Buffer*>(aInfo);
        size_t actualSize = thiz->OnMoreData(buffer->raw, buffer->size);
        buffer->size = actualSize;
        break;
      }
      case AudioTrack::EVENT_UNDERRUN:
        thiz->OnUnderrun();
        break;
      case AudioTrack::EVENT_STREAM_END:
        thiz->OnStreamEnd();
        break;
      case AudioTrack::EVENT_NEW_IAUDIOTRACK:
        thiz->OnNewIAudioTrack();
        break;
      default:
        LOG("AudioTrackCallback: event %d", aEvent);
        break;
    }
  }

  explicit AudioTrackCallback(const wp<AudioOutput>& aCookie) {
    mData = aCookie;
  }

 private:
  // There is no atomic_wp defined in earlier version of AOSP, but it's fine
  // to use wp here because when using legacy callback, AudioTrack doesn't hold
  // a reference to the callback object. The callback object is always destroyed
  // along with AudioOutput, and at that moment, the AudioTrack is already
  // stopped, so mData is guaranteed to be not accessed by two threads
  // concurrently.
  wp<AudioOutput> mData;
  sp<AudioOutput> GetOutput() const override { return mData.promote(); }
  DISALLOW_EVIL_CONSTRUCTORS(AudioTrackCallback);
};
#endif

AudioOutput::AudioOutput(audio_session_t aSessionId,
                         audio_stream_type_t aStreamType)
    : mCallbackCookie(nullptr),
      mCallback(nullptr),
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

  sp<AudioTrack> track;
  sp<AudioTrackCallback> trackCallback = new AudioTrackCallback(this);

  // Offloaded tracks will get frame count from AudioFlinger.
#if ANDROID_VERSION >= 33
  track = new AudioTrack(mStreamType, aSampleRate, aFormat, aChannelMask, 0,
                         aFlags, trackCallback, 0, mSessionId,
                         AudioTrack::TRANSFER_CALLBACK, aOffloadInfo);
#else
  track =
      new AudioTrack(mStreamType, aSampleRate, aFormat, aChannelMask, 0, aFlags,
                     AudioTrackCallback::LegacyCallback, trackCallback.get(), 0,
                     mSessionId, AudioTrack::TRANSFER_CALLBACK, aOffloadInfo);
#endif

  if (!track || track->initCheck() != OK) {
    LOG("Unable to create audio track");
    return NO_INIT;
  }

  track->setVolume(1.0);
  mTrack = track;
  mTrackCallback = trackCallback;
  return OK;
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
  mTrack = nullptr;
  mTrackCallback = nullptr;
}

}  // namespace android
