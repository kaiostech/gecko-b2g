/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOffloadPlayer.h"

#include "AudioOffloadPlayer.h"
#include "GonkOffloadPlayer.h"
#include "MediaTrackGraph.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

using namespace mozilla::media;
using namespace mozilla::layers;

#define LOG(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (fmt, ##__VA_ARGS__))
#define LOGV(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (fmt, ##__VA_ARGS__))

static bool CheckOffloadAllowlist(const MediaMIMEType& aMimeType) {
  nsAutoCString allowlist;
  Preferences::GetCString("media.offloadplayer.mime.allowlist", allowlist);
  if (allowlist.IsEmpty()) {
    return true;
  }

  for (const nsACString& mime : allowlist.Split(',')) {
    if (mime == aMimeType.AsString()) {
      return true;
    }
    if (mime == "application/*"_ns && aMimeType.HasApplicationMajorType()) {
      return true;
    }
    if (mime == "audio/*"_ns && aMimeType.HasAudioMajorType()) {
      return true;
    }
    if (mime == "video/*"_ns && aMimeType.HasVideoMajorType()) {
      return true;
    }
  }
  return false;
}

static bool CanOffloadMedia(nsIURI* aURI, const MediaMIMEType& aMimeType,
                            bool aIsVideo, bool aIsTransportSeekable) {
  if (!aIsTransportSeekable) {
    return false;
  }

  if (!CheckOffloadAllowlist(aMimeType)) {
    return false;
  }

  if (!aIsVideo && !StaticPrefs::media_offloadplayer_audio_enabled()) {
    return false;
  }

  if (aIsVideo && !StaticPrefs::media_offloadplayer_video_enabled()) {
    return false;
  }

  if ((aURI->SchemeIs("http") || aURI->SchemeIs("https")) &&
      StaticPrefs::media_offloadplayer_http_enabled()) {
    return true;
  }

  if (aURI->SchemeIs("file") || aURI->SchemeIs("blob")) {
    return true;
  }

  return false;
}

/* static */
RefPtr<MediaOffloadPlayer> MediaOffloadPlayer::Create(
    MediaDecoder* aDecoder, MediaFormatReaderInit& aInit, nsIURI* aURI) {
  const auto& containerType = aDecoder->ContainerType();
  if (!CanOffloadMedia(aURI, containerType.Type(),
                       /* aIsVideo = */ aInit.mVideoFrameContainer,
                       aDecoder->IsTransportSeekable())) {
    return nullptr;
  }

  RefPtr<MediaOffloadPlayer> player;
  if (containerType.Type().AsString() == "audio/mpeg"_ns) {
    player = new AudioOffloadPlayer(aInit, containerType);
  } else {
    player = new GonkOffloadPlayer(aInit, aURI);
  }

  if (player) {
    // Release unused already_AddRefed if a player is created.
    RefPtr<layers::KnowsCompositor> knowsCompositor = aInit.mKnowsCompositor;
    RefPtr<GMPCrashHelper> crashHelper = aInit.mCrashHelper;
  }
  return player;
}

#define INIT_MIRROR(name, val) \
  name(mTaskQueue, val, "MediaOffloadPlayer::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mTaskQueue, val, "MediaOffloadPlayer::" #name " (Canonical)")

MediaOffloadPlayer::MediaOffloadPlayer(MediaFormatReaderInit& aInit)
    : mTaskQueue(TaskQueue::Create(GetMediaThreadPool(MediaThreadType::MDSM),
                                   "MediaOffloadPlayer::mTaskQueue",
                                   /* aSupportsTailDispatch = */ true)),
      mWatchManager(this, mTaskQueue),
      mCurrentPositionTimer(mTaskQueue, true /*aFuzzy*/),
      mDormantTimer(mTaskQueue, true /*aFuzzy*/),
      mVideoFrameContainer(aInit.mVideoFrameContainer),
      mResource(aInit.mResource),
      INIT_CANONICAL(mBuffered, TimeIntervals()),
      INIT_CANONICAL(mDuration, NullableTimeUnit()),
      INIT_CANONICAL(mCurrentPosition, TimeUnit::Zero()),
      INIT_CANONICAL(mIsAudioDataAudible, false),
      INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
      INIT_MIRROR(mVolume, 1.0),
      INIT_MIRROR(mPreservesPitch, true),
      INIT_MIRROR(mLooping, false),
      INIT_MIRROR(mSinkDevice, nullptr),
      INIT_MIRROR(mSecondaryVideoContainer, nullptr),
      INIT_MIRROR(mOutputCaptureState, MediaDecoder::OutputCaptureState::None),
      INIT_MIRROR(mOutputTracks, nsTArray<RefPtr<ProcessedMediaTrack>>()),
      INIT_MIRROR(mOutputPrincipal, PRINCIPAL_HANDLE_NONE) {
  LOG("MediaOffloadPlayer::MediaOffloadPlayer");
  DDLINKCHILD("source", mResource.get());
}

#undef INIT_MIRROR
#undef INIT_CANONICAL

MediaOffloadPlayer::~MediaOffloadPlayer() {
  LOG("MediaOffloadPlayer::~MediaOffloadPlayer");
}

void MediaOffloadPlayer::InitializationTask(MediaDecoder* aDecoder) {
  AUTO_PROFILER_LABEL("MediaOffloadPlayer::InitializationTask", MEDIA_PLAYBACK);
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::InitializationTask");

  // Connect mirrors.
  aDecoder->CanonicalPlayState().ConnectMirror(&mPlayState);
  aDecoder->CanonicalVolume().ConnectMirror(&mVolume);
  aDecoder->CanonicalPreservesPitch().ConnectMirror(&mPreservesPitch);
  aDecoder->CanonicalLooping().ConnectMirror(&mLooping);
  aDecoder->CanonicalSinkDevice().ConnectMirror(&mSinkDevice);
  aDecoder->CanonicalSecondaryVideoContainer().ConnectMirror(
      &mSecondaryVideoContainer);
  aDecoder->CanonicalOutputCaptureState().ConnectMirror(&mOutputCaptureState);
  aDecoder->CanonicalOutputTracks().ConnectMirror(&mOutputTracks);
  aDecoder->CanonicalOutputPrincipal().ConnectMirror(&mOutputPrincipal);

  // Initialize watchers.
  mWatchManager.Watch(mPlayState, &MediaOffloadPlayer::PlayStateChanged);
  mWatchManager.Watch(mVolume, &MediaOffloadPlayer::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch,
                      &MediaOffloadPlayer::PreservesPitchChanged);
  mWatchManager.Watch(mLooping, &MediaOffloadPlayer::LoopingChanged);
  mWatchManager.Watch(mSecondaryVideoContainer,
                      &MediaOffloadPlayer::UpdateSecondaryVideoContainer);
  mWatchManager.Watch(mOutputCaptureState,
                      &MediaOffloadPlayer::UpdateOutputCaptureState);
  mWatchManager.Watch(mOutputTracks, &MediaOffloadPlayer::OutputTracksChanged);
  mWatchManager.Watch(mOutputPrincipal,
                      &MediaOffloadPlayer::OutputPrincipalChanged);

  mTransportSeekable = aDecoder->IsTransportSeekable();
  mAudioChannel = aDecoder->GetAudioChannel();
  mContainerType = aDecoder->ContainerType().Type().AsString();

  InitInternal();
}

nsresult MediaOffloadPlayer::Init(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  OwnerThread()->DispatchStateChange(NewRunnableMethod<RefPtr<MediaDecoder>>(
      "MediaOffloadPlayer::InitializationTask", this,
      &MediaOffloadPlayer::InitializationTask, aDecoder));
  return NS_OK;
}

RefPtr<ShutdownPromise> MediaOffloadPlayer::Shutdown() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::Shutdown");

  mCurrentPositionTimer.Reset();
  mDormantTimer.Reset();
  ShutdownInternal();

  // Disconnect canonicals and mirrors before shutting down our task queue.
  mPlayState.DisconnectIfConnected();
  mVolume.DisconnectIfConnected();
  mPreservesPitch.DisconnectIfConnected();
  mLooping.DisconnectIfConnected();
  mSinkDevice.DisconnectIfConnected();
  mSecondaryVideoContainer.DisconnectIfConnected();
  mOutputCaptureState.DisconnectIfConnected();
  mOutputTracks.DisconnectIfConnected();
  mOutputPrincipal.DisconnectIfConnected();

  mBuffered.DisconnectAll();
  mDuration.DisconnectAll();
  mCurrentPosition.DisconnectAll();
  mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager to stop further notifications.
  mWatchManager.Shutdown();
  return OwnerThread()->BeginShutdown();
}

RefPtr<ShutdownPromise> MediaOffloadPlayer::BeginShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaOffloadPlayer::Shutdown);
}

void MediaOffloadPlayer::UpdateCurrentPositionPeriodically() {
  bool scheduleNextUpdate = UpdateCurrentPosition();
  if (!scheduleNextUpdate) {
    return;
  }

  TimeStamp target = TimeStamp::Now() + TimeDuration::FromMilliseconds(250);
  mCurrentPositionTimer.Ensure(
      target,
      [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
        mCurrentPositionTimer.CompleteRequest();
        UpdateCurrentPositionPeriodically();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

RefPtr<GenericPromise> MediaOffloadPlayer::RequestDebugInfo(
    dom::MediaDecoderStateMachineDebugInfo& aInfo) {
  return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

void MediaOffloadPlayer::NotifySeeked(bool aSuccess) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mCurrentSeek.Exists());

  LOG("MediaOffloadPlayer::NotifySeeked, %s seek to %.03lf sec %s",
      mCurrentSeek.mVisible ? "user" : "internal",
      mCurrentSeek.mTarget->GetTime().ToSeconds(),
      aSuccess ? "succeeded" : "failed");

  if (aSuccess) {
    mCurrentSeek.mPromise.ResolveIfExists(true, __func__);
  } else {
    mCurrentSeek.mPromise.RejectIfExists(true, __func__);
  }

  // If there is a pending seek job, dispatch it to another runnable.
  if (mPendingSeek.Exists()) {
    mCurrentSeek = std::move(mPendingSeek);
    mPendingSeek = SeekObject();
    nsresult rv = OwnerThread()->Dispatch(NS_NewRunnableFunction(
        "MediaOffloadPlayer::SeekPendingJob",
        [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
          SeekInternal(mCurrentSeek.mTarget.ref(), mCurrentSeek.mVisible);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  } else {
    mCurrentSeek = SeekObject();
    // We may just exit from dormant state, so manually call PlayStateChanged()
    // to enter playing/paused state correctly.
    PlayStateChanged();
  }
}

RefPtr<MediaDecoder::SeekPromise> MediaOffloadPlayer::HandleSeek(
    const SeekTarget& aTarget, bool aVisible) {
  MOZ_ASSERT(OnTaskQueue());

  LOG("MediaOffloadPlayer::HandleSeek, %s seek to %.03lf sec, type %d",
      aVisible ? "user" : "internal", aTarget.GetTime().ToSeconds(),
      aTarget.GetType());

  ExitDormant();

  // If we are currently seeking or we need to defer seeking, save the job in
  // mPendingSeek. If there is already a pending seek job, reject it and replace
  // it by the new one.
  if (mCurrentSeek.Exists() || NeedToDeferSeek()) {
    LOG("MediaOffloadPlayer::HandleSeek, cannot seek now, replacing previous "
        "pending target %.03lf sec",
        mPendingSeek.Exists() ? mPendingSeek.mTarget->GetTime().ToSeconds()
                              : UnspecifiedNaN<double>());
    mPendingSeek.RejectIfExists(__func__);
    mPendingSeek.mTarget.emplace(aTarget);
    mPendingSeek.mVisible = aVisible;
    return mPendingSeek.mPromise.Ensure(__func__);
  }

  MOZ_ASSERT(!mPendingSeek.Exists());
  mPendingSeek = SeekObject();
  mCurrentSeek.mTarget.emplace(aTarget);
  mCurrentSeek.mVisible = aVisible;
  RefPtr<MediaDecoder::SeekPromise> p = mCurrentSeek.mPromise.Ensure(__func__);
  SeekInternal(aTarget, aVisible);
  return p;
}

RefPtr<MediaDecoder::SeekPromise> MediaOffloadPlayer::InvokeSeek(
    const SeekTarget& aTarget) {
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaOffloadPlayer::HandleSeek, aTarget, true);
}

bool MediaOffloadPlayer::FirePendingSeekIfExists() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mCurrentSeek.Exists());

  if (mPendingSeek.Exists()) {
    LOG("MediaOffloadPlayer::FirePendingSeekIfExists, %s seek, %.03lf sec",
        mPendingSeek.mVisible ? "user" : "internal",
        mPendingSeek.mTarget->GetTime().ToSeconds());
    mCurrentSeek = std::move(mPendingSeek);
    mPendingSeek = SeekObject();
    SeekInternal(mCurrentSeek.mTarget.ref(), mCurrentSeek.mVisible);
    return true;
  }
  return false;
}

void MediaOffloadPlayer::DispatchSetPlaybackRate(double aPlaybackRate) {
  MOZ_ASSERT(NS_IsMainThread());
  OwnerThread()->DispatchStateChange(NS_NewRunnableFunction(
      "MediaOffloadPlayer::SetPlaybackRate",
      [this, self = RefPtr<MediaOffloadPlayer>(this), aPlaybackRate]() {
        mPlaybackRate = aPlaybackRate;
        PlaybackRateChanged();
      }));
}

RefPtr<GenericPromise> MediaOffloadPlayer::InvokeSetSink(
    RefPtr<AudioDeviceInfo> aSink) {
  return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

RefPtr<SetCDMPromise> MediaOffloadPlayer::SetCDMProxy(CDMProxy* aProxy) {
  return SetCDMPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

bool MediaOffloadPlayer::IsCDMProxySupported(CDMProxy* aProxy) { return false; }

void MediaOffloadPlayer::UpdateCompositor(
    already_AddRefed<layers::KnowsCompositor> aCompositor) {
  RefPtr<layers::KnowsCompositor> compositor = aCompositor;
}

RefPtr<GenericPromise> MediaOffloadPlayer::RequestDebugInfo(
    dom::MediaFormatReaderDebugInfo& aInfo) {
  return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
}

void MediaOffloadPlayer::StartDormantTimer() {
  if (mInDormant) {
    return;
  }

  if (!mTransportSeekable || !mInfo.mMediaSeekable) {
    return;
  }

  auto timeout = StaticPrefs::media_dormant_on_pause_timeout_ms();
  if (timeout < 0) {
    return;
  } else if (timeout == 0) {
    EnterDormant();
    return;
  }

  LOG("MediaOffloadPlayer::StartDormantTimer, start in %d ms", timeout);
  TimeStamp target = TimeStamp::Now() + TimeDuration::FromMilliseconds(timeout);
  mDormantTimer.Ensure(
      target,
      [this, self = RefPtr<MediaOffloadPlayer>(this)]() {
        mDormantTimer.CompleteRequest();
        EnterDormant();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

void MediaOffloadPlayer::EnterDormant() {
  LOG("MediaOffloadPlayer::EnterDormant, resetting internal player");
  mInDormant = true;
  UpdateCurrentPosition();
  ResetInternal();
}

void MediaOffloadPlayer::ExitDormant() {
  mDormantTimer.Reset();
  if (mInDormant) {
    LOG("MediaOffloadPlayer::ExitDormant, initializing internal player");
    mInDormant = false;
    // If there is no pending seek job, set it to current position. It will be
    // fired after init done.
    if (!mPendingSeek.Exists()) {
      HandleSeek(SeekTarget(mCurrentPosition, SeekTarget::Accurate), false);
    }
    InitInternal();
  }
}

void MediaOffloadPlayer::PlayStateChanged() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaOffloadPlayer::PlayStateChanged, %d", mPlayState.Ref());
  if (mPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
    StartDormantTimer();
  } else if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
    ExitDormant();
  }
}

void MediaOffloadPlayer::NotifyMediaNotSeekable() {
  LOG("MediaOffloadPlayer::NotifyMediaNotSeekable");
  mOnMediaNotSeekable.Notify();
}

void MediaOffloadPlayer::NotifyMetadataLoaded(UniquePtr<MediaInfo> aInfo,
                                              UniquePtr<MetadataTags> aTags) {
  LOG("MediaOffloadPlayer::NotifyMetadataLoaded, duration %.03lf sec, "
      "audio: %s, video: %s",
      aInfo->mMetadataDuration ? aInfo->mMetadataDuration->ToSeconds()
                               : UnspecifiedNaN<double>(),
      aInfo->HasAudio() ? aInfo->mAudio.mMimeType.get() : "none",
      aInfo->HasVideo() ? aInfo->mVideo.mMimeType.get() : "none");
  mMetadataLoadedEvent.Notify(std::move(aInfo), std::move(aTags),
                              MediaDecoderEventVisibility::Observable);
}

void MediaOffloadPlayer::NotifyFirstFrameLoaded(UniquePtr<MediaInfo> aInfo) {
  LOG("MediaOffloadPlayer::NotifyFirstFrameLoaded");
  mFirstFrameLoadedEvent.Notify(std::move(aInfo),
                                MediaDecoderEventVisibility::Observable);
}

void MediaOffloadPlayer::NotifyPlaybackEvent(MediaPlaybackEvent aEvent) {
  LOG("MediaOffloadPlayer::NotifyPlaybackEvent, type %d", aEvent.mType);
  mOnPlaybackEvent.Notify(aEvent);
}

void MediaOffloadPlayer::NotifyPlaybackError(MediaResult aError) {
  LOG("MediaOffloadPlayer::NotifyPlaybackError, %s",
      aError.Description().get());
  mOnPlaybackErrorEvent.Notify(aError);
}

void MediaOffloadPlayer::NotifyNextFrameStatus(NextFrameStatus aStatus) {
  LOG("MediaOffloadPlayer::NotifyNextFrameStatus, status %d", aStatus);
  mOnNextFrameStatus.Notify(aStatus);
}

}  // namespace mozilla
