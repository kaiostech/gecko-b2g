/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioOffloadPlayer_h_
#define AudioOffloadPlayer_h_

#include <system/audio.h>
#include <utils/StrongPointer.h>

#include "GonkAudioSink.h"
#include "MediaOffloadPlayer.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(AudioOffloadPlayer, MediaOffloadPlayer);

class AudioOffloadPlayer : public MediaOffloadPlayer,
                           public DecoderDoctorLifeLogger<AudioOffloadPlayer> {
  using TimeUnit = media::TimeUnit;
  using TimeIntervals = media::TimeIntervals;
  using GonkAudioSink = android::GonkAudioSink;

 public:
  AudioOffloadPlayer(MediaFormatReaderInit& aInit,
                     const MediaContainerType& aType);

  // Called by GonkAudioSink when it needs data, to notify EOS or tear down
  // event
  static size_t AudioSinkCallback(GonkAudioSink* aAudioSink, void* aData,
                                  size_t aSize, void* aCookie,
                                  GonkAudioSink::cb_event_t aEvent);

 private:
  ~AudioOffloadPlayer() = default;
  virtual void InitInternal() override;
  virtual void ShutdownInternal() override;
  virtual void ResetInternal() override;
  virtual void SeekInternal(const SeekTarget& aTarget, bool aVisible) override;
  virtual bool UpdateCurrentPosition() override;
  virtual bool NeedToDeferSeek() override { return !mInitDone; }
  virtual void NotifyDataArrived() override;

  virtual void PlayStateChanged() override;
  virtual void VolumeChanged() override;
  virtual void PreservesPitchChanged() override { PlaybackSettingsChanged(); }
  virtual void PlaybackRateChanged() override { PlaybackSettingsChanged(); }

  void PlaybackSettingsChanged();
  void ReaderBufferedUpdated() { mBuffered = mReaderBuffered; }
  void OpenAudioSink();
  void SendMetadataToHal(audio_offload_info_t& aOffloadInfo);
  void MaybeStartDemuxing();
  void DemuxSamples();
  void MaybeStartDecoding();
  void DecodeAudio();
  void UpdateAudibleState();
  void Flush();

  void OnDemuxerInitDone(const MediaResult& aResult);
  void OnDemuxerInitFailed(const MediaResult& aError);
  void OnReaderMetadataRead(MetadataHolder&& aMetadata);
  void OnReaderMetadataNotRead(const MediaResult& aError);
  void OnDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnDemuxFailed(const MediaResult& aError);
  void OnAudioDecoded(RefPtr<AudioData> aAudio);
  void OnAudioNotDecoded(const MediaResult& aError);
  void OnSamplePopped(const RefPtr<MediaData>& aSample);

  // Called on GonkAudioSink callback thread.
  size_t FillAudioBuffer(void* aData, size_t aSize);
  void NotifyEOSCallback();
  void NotifyAudioTearDown();

  bool mIsOffloaded = true;
  bool mInitDone = false;
  bool mIsPlaying = false;
  bool mInputEOS = false;
  bool mNotifiedEOS = false;
  bool mSentFirstFrameLoadedEvent = false;
  TimeUnit mStartPosition = TimeUnit::Invalid();

  RefPtr<MediaDataDemuxer> mDemuxer;
  RefPtr<MediaTrackDemuxer> mTrackDemuxer;
  RefPtr<MediaFormatReader> mReader;
  RefPtr<ReaderProxy> mReaderProxy;
  WatchManager<AudioOffloadPlayer> mAudioWatchManager;
  Mirror<TimeIntervals> mReaderBuffered;
  MediaEventListener mSamplePopListener;

  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
  MozPromiseRequestHolder<MediaTrackDemuxer::SamplesPromise> mDemuxRequest;
  MozPromiseRequestHolder<MediaTrackDemuxer::SeekPromise> mDemuxerSeekRequest;
  MozPromiseRequestHolder<MediaFormatReader::MetadataPromise> mMetadataRequest;
  MozPromiseRequestHolder<MediaFormatReader::AudioDataPromise>
      mAudioDataRequest;
  MozPromiseRequestHolder<MediaFormatReader::SeekPromise> mReaderSeekRequest;

  Mutex mMutex;
  size_t mSampleOffset = 0;  // protected by mMutex
  MediaQueue<MediaData> mSampleQueue;

  audio_session_t mAudioSessionId;
  android::sp<GonkAudioSink> mAudioSink;
};

}  // namespace mozilla

#endif
