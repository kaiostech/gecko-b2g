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

#ifndef AUDIOOUTPUT_H_
#define AUDIOOUTPUT_H_

#include <media/AudioTrack.h>
#include <utils/Mutex.h>

#include "base/basictypes.h"
#include "GonkAudioSink.h"

namespace android {

/**
 * Stripped version of Android MediaPlayerService::AudioOutput class. Android
 * MediaPlayer uses AudioOutput as a wrapper to handle AudioTrack. Similarly to
 * ease handling offloaded tracks, part of AudioOutput is used here
 */
class AudioOutput : public GonkAudioSink {
  class CallbackData;

 public:
  AudioOutput(audio_session_t aSessionId, audio_stream_type_t aStreamType);
  virtual ~AudioOutput();

  ssize_t FrameSize() const override;
  status_t GetPosition(uint32_t* aPosition) const override;
  status_t SetVolume(float aVolume) override;
  status_t SetPlaybackRate(const AudioPlaybackRate& aRate) override;
  status_t SetParameters(const String8& aKeyValuePairs) override;

  // Creates an offloaded audio track with the given parameters
  // TODO: Try to recycle audio tracks instead of creating new audio tracks
  // every time
  status_t Open(uint32_t aSampleRate, int aChannelCount,
                audio_channel_mask_t aChannelMask, audio_format_t aFormat,
                AudioCallback aCb, void* aCookie,
                audio_output_flags_t aFlags = AUDIO_OUTPUT_FLAG_NONE,
                const audio_offload_info_t* aOffloadInfo = nullptr) override;

  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;

 private:
  static void CallbackWrapper(int aEvent, void* aMe, void* aInfo);

  sp<AudioTrack> mTrack;
  void* mCallbackCookie;
  AudioCallback mCallback;
  CallbackData* mCallbackData;

  // Session id given by AudioSystem and used to create audio track.
  audio_session_t mSessionId;

  audio_stream_type_t mStreamType;

  // CallbackData is what is passed to the AudioTrack as the "user" data.
  // We need to be able to target this to a different Output on the fly,
  // so we can't use the Output itself for this.
  class CallbackData {
   public:
    CallbackData(AudioOutput* aCookie) { mData = aCookie; }
    AudioOutput* GetOutput() { return mData; }
    void SetOutput(AudioOutput* aNewcookie) { mData = aNewcookie; }
    // Lock/Unlock are used by the callback before accessing the payload of
    // this object
    void Lock() { mLock.lock(); }
    void Unlock() { mLock.unlock(); }

   private:
    AudioOutput* mData;
    mutable Mutex mLock;
    DISALLOW_EVIL_CONSTRUCTORS(CallbackData);
  };
};  // AudioOutput

}  // namespace android

#endif /* AUDIOOUTPUT_H_ */
