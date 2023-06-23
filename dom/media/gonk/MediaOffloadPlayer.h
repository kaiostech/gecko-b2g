/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaOffloadPlayer_h_
#define MediaOffloadPlayer_h_

#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "nsIURI.h"

namespace mozilla {

DDLoggedTypeDeclName(MediaOffloadPlayer);

class MediaOffloadPlayer : public DecoderDoctorLifeLogger<MediaOffloadPlayer> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaOffloadPlayer)

 public:
  typedef MediaDecoderOwner::NextFrameStatus NextFrameStatus;

  static RefPtr<MediaOffloadPlayer> Create(MediaDecoder* aDecoder,
                                           MediaFormatReaderInit& aInit,
                                           nsIURI* aURI);

  /*
   * APIs from both MediaDecoderStateMachine and MediaFormatReader.
   */
  TaskQueue* OwnerThread() const { return mTaskQueue; }

  /*
   * APIs from MediaDecoderStateMachine.
   */
  nsresult Init(MediaDecoder* aDecoder);
  RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaDecoderStateMachineDebugInfo& aInfo);
  RefPtr<MediaDecoder::SeekPromise> InvokeSeek(const SeekTarget& aTarget);
  RefPtr<ShutdownPromise> BeginShutdown();

  void DispatchSetPlaybackRate(double aPlaybackRate);
  void DispatchSetFragmentEndTime(const media::TimeUnit& aEndTime) {}
  void DispatchCanPlayThrough(bool aCanPlayThrough) {}
  void DispatchIsLiveStream(bool aIsLiveStream) {}

  size_t SizeOfVideoQueue() const { return 0; }
  size_t SizeOfAudioQueue() const { return 0; }

  void SetVideoDecodeMode(VideoDecodeMode aMode) {}
  RefPtr<GenericPromise> InvokeSetSink(RefPtr<AudioDeviceInfo> aSink);
  void InvokeSuspendMediaSink() {}
  void InvokeResumeMediaSink() {}

  MediaEventSourceExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                      MediaDecoderEventVisibility>&
  MetadataLoadedEvent() {
    return mMetadataLoadedEvent;
  }
  MediaEventSourceExc<UniquePtr<MediaInfo>, MediaDecoderEventVisibility>&
  FirstFrameLoadedEvent() {
    return mFirstFrameLoadedEvent;
  }
  MediaEventSource<MediaPlaybackEvent>& OnPlaybackEvent() {
    return mOnPlaybackEvent;
  }
  MediaEventSource<MediaResult>& OnPlaybackErrorEvent() {
    return mOnPlaybackErrorEvent;
  }
  MediaEventSource<DecoderDoctorEvent>& OnDecoderDoctorEvent() {
    return mOnDecoderDoctorEvent;
  }
  MediaEventProducer<VideoInfo, AudioInfo>& OnTrackInfoUpdatedEvent() {
    return mOnTrackInfoUpdatedEvent;
  }
  MediaEventSource<NextFrameStatus>& OnNextFrameStatus() {
    return mOnNextFrameStatus;
  }
  MediaEventSourceExc<RefPtr<VideoFrameContainer>>&
  OnSecondaryVideoContainerInstalled() {
    return mOnSecondaryVideoContainerInstalled;
  }
  TimedMetadataEventSource& TimedMetadataEvent() { return mTimedMetadataEvent; }
  MediaEventSource<void>& OnMediaNotSeekable() { return mOnMediaNotSeekable; }

  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() {
    return &mBuffered;
  }
  AbstractCanonical<media::NullableTimeUnit>* CanonicalDuration() {
    return &mDuration;
  }
  AbstractCanonical<media::TimeUnit>* CanonicalCurrentPosition() {
    return &mCurrentPosition;
  }
  AbstractCanonical<bool>* CanonicalIsAudioDataAudible() {
    return &mIsAudioDataAudible;
  }

  /*
   * APIs from MediaFormatReader.
   */
  virtual void NotifyDataArrived() {}
  RefPtr<SetCDMPromise> SetCDMProxy(CDMProxy* aProxy);
  bool IsCDMProxySupported(CDMProxy* aProxy);
  void UpdateCompositor(already_AddRefed<layers::KnowsCompositor> aCompositor);
  RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaFormatReaderDebugInfo& aInfo);

  MediaEventSource<nsTArray<uint8_t>, nsString>& OnEncrypted() {
    return mOnEncrypted;
  }
  MediaEventSource<void>& OnWaitingForKey() { return mOnWaitingForKey; }
  MediaEventSource<MediaResult>& OnDecodeWarning() { return mOnDecodeWarning; }
  MediaEventSource<VideoInfo>& OnStoreDecoderBenchmark() {
    return mOnStoreDecoderBenchmark;
  }

 private:
  // Extend SeekJob struct by adding mVisible flag.
  struct SeekObject : public SeekJob {
    SeekObject() = default;
    SeekObject(SeekObject&& aOther) = default;
    SeekObject& operator=(SeekObject&& aOther) = default;
    bool mVisible = true;
  };

  void InitializationTask(MediaDecoder* aDecoder);
  RefPtr<ShutdownPromise> Shutdown();
  void StartDormantTimer();
  void EnterDormant();
  void ExitDormant();

  RefPtr<TaskQueue> mTaskQueue;
  WatchManager<MediaOffloadPlayer> mWatchManager;
  DelayedScheduler mCurrentPositionTimer;
  DelayedScheduler mDormantTimer;
  SeekObject mCurrentSeek;
  SeekObject mPendingSeek;
  bool mInDormant = false;

 protected:
  MediaOffloadPlayer(MediaFormatReaderInit& aInit);
  virtual ~MediaOffloadPlayer();
  bool OnTaskQueue() const { return OwnerThread()->IsCurrentThreadIn(); }
  RefPtr<MediaDecoder::SeekPromise> HandleSeek(const SeekTarget& aTarget,
                                               bool aVisible);
  bool FirePendingSeekIfExists();
  bool Seeking() { return mCurrentSeek.Exists(); }
  void NotifySeeked(bool aSuccess);
  void UpdateCurrentPositionPeriodically();

  const RefPtr<VideoFrameContainer> mVideoFrameContainer;
  const RefPtr<MediaResource> mResource;
  double mPlaybackRate = 1.0;
  bool mTransportSeekable = false;
  dom::AudioChannel mAudioChannel = dom::AudioChannel::Normal;
  nsCString mContainerType;
  MediaInfo mInfo;

  /*
   * Methods implemented by derived classes.
   */
  virtual void InitInternal() = 0;
  virtual void ShutdownInternal() = 0;
  virtual void ResetInternal() = 0;
  virtual void SeekInternal(const SeekTarget& aTarget, bool aVisible) = 0;
  // Return true to schedule next update.
  virtual bool UpdateCurrentPosition() = 0;
  virtual bool NeedToDeferSeek() = 0;

  /*
   * Event notifier.
   */
  void NotifyMediaNotSeekable();
  void NotifyMetadataLoaded(UniquePtr<MediaInfo> aInfo,
                            UniquePtr<MetadataTags> aTags);
  void NotifyFirstFrameLoaded(UniquePtr<MediaInfo> aInfo);
  void NotifyPlaybackEvent(MediaPlaybackEvent aEvent);
  void NotifyPlaybackError(MediaResult aError);
  void NotifyNextFrameStatus(NextFrameStatus aStatus);
  void NotifyTrackInfoUpdated();

  /*
   * Canonicals.
   */
  Canonical<media::TimeIntervals> mBuffered;
  Canonical<media::NullableTimeUnit> mDuration;
  Canonical<media::TimeUnit> mCurrentPosition;
  Canonical<bool> mIsAudioDataAudible;

  /*
   * Mirrors.
   */
  Mirror<MediaDecoder::PlayState> mPlayState;
  Mirror<double> mVolume;
  Mirror<bool> mPreservesPitch;
  Mirror<bool> mLooping;
  Mirror<RefPtr<AudioDeviceInfo>> mSinkDevice;
  Mirror<RefPtr<VideoFrameContainer>> mSecondaryVideoContainer;
  Mirror<MediaDecoder::OutputCaptureState> mOutputCaptureState;
  Mirror<CopyableTArray<RefPtr<ProcessedMediaTrack>>> mOutputTracks;
  Mirror<PrincipalHandle> mOutputPrincipal;

  /*
   * State change callbacks.
   * Most of them are watch callbacks for the mirrors above.
   */
  virtual void PlayStateChanged();
  virtual void VolumeChanged() {}
  virtual void PreservesPitchChanged() {}
  virtual void LoopingChanged() {}
  virtual void UpdateSecondaryVideoContainer() {}
  virtual void UpdateOutputCaptureState() {}
  virtual void OutputTracksChanged() {}
  virtual void OutputPrincipalChanged() {}
  virtual void PlaybackRateChanged() {}

 private:
  /*
   * Events.
   */
  MediaEventProducer<nsTArray<uint8_t>, nsString> mOnEncrypted;
  MediaEventProducer<void> mOnWaitingForKey;
  MediaEventProducer<MediaResult> mOnDecodeWarning;
  MediaEventProducer<VideoInfo> mOnStoreDecoderBenchmark;
  TimedMetadataEventProducer mTimedMetadataEvent;
  MediaEventProducer<void> mOnMediaNotSeekable;
  MediaEventProducerExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                        MediaDecoderEventVisibility>
      mMetadataLoadedEvent;
  MediaEventProducerExc<UniquePtr<MediaInfo>, MediaDecoderEventVisibility>
      mFirstFrameLoadedEvent;
  MediaEventProducer<MediaPlaybackEvent> mOnPlaybackEvent;
  MediaEventProducer<MediaResult> mOnPlaybackErrorEvent;
  MediaEventProducer<DecoderDoctorEvent> mOnDecoderDoctorEvent;
  MediaEventProducer<NextFrameStatus> mOnNextFrameStatus;
  MediaEventProducerExc<RefPtr<VideoFrameContainer>>
      mOnSecondaryVideoContainerInstalled;
  MediaEventProducer<VideoInfo, AudioInfo> mOnTrackInfoUpdatedEvent;
};

}  // namespace mozilla

#endif
