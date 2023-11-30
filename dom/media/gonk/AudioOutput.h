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
  class AudioTrackCallback;
  class AudioTrackCallbackBase;

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
  sp<AudioTrack> mTrack;
  sp<AudioTrackCallback> mTrackCallback;
  void* mCallbackCookie;
  AudioCallback mCallback;
  audio_session_t mSessionId;
  audio_stream_type_t mStreamType;
};  // AudioOutput

}  // namespace android

#endif /* AUDIOOUTPUT_H_ */
