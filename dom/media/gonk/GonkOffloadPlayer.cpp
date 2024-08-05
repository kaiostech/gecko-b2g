/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkOffloadPlayer.h"

#include <media/stagefright/InterfaceUtils.h>
#include "GonkBufferQueueProducer.h"
#include "GonkMediaHTTPService.h"
#include "GrallocImages.h"
#include "MediaResourceDataSource.h"
#include "VideoFrameContainer.h"

namespace mozilla {

using namespace android;
using namespace mozilla::dom;
using namespace mozilla::media;
using namespace mozilla::layers;

#define LOG(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (fmt, ##__VA_ARGS__))
#define LOGV(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (fmt, ##__VA_ARGS__))

class AndroidMediaPlayerListener : public android::MediaPlayerListener {
 public:
  explicit AndroidMediaPlayerListener(GonkOffloadPlayer* aOwner)
      : mOwner(aOwner) {}

  virtual void notify(int msg, int ext1, int ext2,
                      const android::Parcel* obj) override {
    LOGV("AndroidMediaPlayerListener::notify, %s, %d, %d",
         MediaEventTypeToStr(msg), ext1, ext2);
    nsresult rv = mOwner->OwnerThread()->Dispatch(NS_NewRunnableFunction(
        "AndroidMediaPlayerListener::DispatchNotify",
        [owner = RefPtr<GonkOffloadPlayer>(mOwner), msg, ext1, ext2]() {
          owner->Notify(msg, ext1, ext2);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 private:
  static const char* MediaEventTypeToStr(int aEventType) {
    switch (aEventType) {
      case MEDIA_NOP:
        return "MEDIA_NOP";
      case MEDIA_PREPARED:
        return "MEDIA_PREPARED";
      case MEDIA_PLAYBACK_COMPLETE:
        return "MEDIA_PLAYBACK_COMPLETE";
      case MEDIA_BUFFERING_UPDATE:
        return "MEDIA_BUFFERING_UPDATE";
      case MEDIA_SEEK_COMPLETE:
        return "MEDIA_SEEK_COMPLETE";
      case MEDIA_SET_VIDEO_SIZE:
        return "MEDIA_SET_VIDEO_SIZE";
      case MEDIA_STARTED:
        return "MEDIA_STARTED";
      case MEDIA_PAUSED:
        return "MEDIA_PAUSED";
      case MEDIA_STOPPED:
        return "MEDIA_STOPPED";
      case MEDIA_SKIPPED:
        return "MEDIA_SKIPPED";
      case MEDIA_TIMED_TEXT:
        return "MEDIA_TIMED_TEXT";
      case MEDIA_ERROR:
        return "MEDIA_ERROR";
      case MEDIA_INFO:
        return "MEDIA_INFO";
      case MEDIA_SUBTITLE_DATA:
        return "MEDIA_SUBTITLE_DATA";
      case MEDIA_META_DATA:
        return "MEDIA_META_DATA";
      case MEDIA_DRM_INFO:
        return "MEDIA_DRM_INFO";
      case MEDIA_TIME_DISCONTINUITY:
        return "MEDIA_TIME_DISCONTINUITY";
      case MEDIA_AUDIO_ROUTING_CHANGED:
        return "MEDIA_AUDIO_ROUTING_CHANGED";
      default:
        return "unknown media event";
    }
  }

  GonkOffloadPlayer* mOwner;
};

static audio_stream_type_t ConvertToAudioStreamType(AudioChannel aChannel) {
  switch (aChannel) {
    default:
    case AudioChannel::Normal:
    case AudioChannel::Content:
      return AUDIO_STREAM_MUSIC;
    case AudioChannel::Notification:
      return AUDIO_STREAM_NOTIFICATION;
    case AudioChannel::Alarm:
      return AUDIO_STREAM_ALARM;
    case AudioChannel::Telephony:
      return AUDIO_STREAM_VOICE_CALL;
    case AudioChannel::Ringer:
      return AUDIO_STREAM_RING;
    case AudioChannel::Publicnotification:
      return AUDIO_STREAM_ENFORCED_AUDIBLE;
    case AudioChannel::System:
      return AUDIO_STREAM_RING;
  }
}

GonkOffloadPlayer::GonkOffloadPlayer(MediaFormatReaderInit& aInit, nsIURI* aURI)
    : MediaOffloadPlayer(aInit),
      mURI(aURI),
      mSentFirstFrameLoadedEvent(false),
      mPackedDisplaySize(0) {}

void GonkOffloadPlayer::OnNewFrame() {
  RefPtr<TextureClient> buffer = mNativeWindow->getCurrentBuffer();
  if (!buffer) {
    return;
  }

  RefPtr<GrallocImage> image(new GrallocImage());
  image->AdoptData(buffer, buffer->GetSize());
  int64_t packedDisplaySize = /* Atomic */ mPackedDisplaySize;
  gfx::IntSize displaySize(packedDisplaySize >> 32,
                           packedDisplaySize & 0xffffffff);
  mVideoFrameContainer->SetCurrentFrame(displaySize, image, TimeStamp::Now());

  if (!mSentFirstFrameLoadedEvent.exchange(true) /* Atomic */) {
    nsresult rv = OwnerThread()->Dispatch(NS_NewRunnableFunction(
        "GonkOffloadPlayer::SendFirstFrameLoadedEvent",
        [this, self = RefPtr<GonkOffloadPlayer>(this)]() {
          NotifyFirstFrameLoaded(MakeUnique<MediaInfo>(mInfo));
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

void GonkOffloadPlayer::InitInternal() {
  MOZ_ASSERT(OnTaskQueue());

  mMediaPlayer = new MediaPlayer();
  mMediaPlayerListener = new AndroidMediaPlayerListener(this);
  mMediaPlayer->setListener(mMediaPlayerListener);

  if (mURI && (mURI->SchemeIs("http") || mURI->SchemeIs("https"))) {
    LOG("GonkOffloadPlayer::InitInternal, set URI source %s",
        mURI->GetSpecOrDefault().get());
    nsCString uri(mURI->GetSpecOrDefault());
    sp<IMediaHTTPService> httpService =
        new GonkMediaHTTPService(mResource, uri.get(), mContainerType.get());
    mMediaPlayer->setDataSource(httpService, uri.get(), nullptr);
  } else {
    LOG("GonkOffloadPlayer::InitInternal, set DataSource %s",
        mURI ? mURI->GetSpecOrDefault().get() : "");
    sp<DataSource> source = new MediaResourceDataSource(mResource);
    mMediaPlayer->setDataSource(CreateIDataSourceFromDataSource(source));
  }

  if (mVideoFrameContainer) {
    sp<IGraphicBufferProducer> producer;
    sp<IGonkGraphicBufferConsumer> consumer;
    GonkBufferQueue::createBufferQueue(&producer, &consumer);
    mNativeWindow = new GonkNativeWindow(consumer, GRALLOC_USAGE_HW_TEXTURE);
    mNativeWindow->setNewFrameCallback(this);
    static_cast<GonkBufferQueueProducer*>(producer.get())
        ->setSynchronousMode(false);
    mMediaPlayer->setVideoSurfaceTexture(producer);
  }

  VolumeChanged();
  LoopingChanged();
  PlaybackSettingsChanged();
  mMediaPlayer->setAudioStreamType(ConvertToAudioStreamType(mAudioChannel));
  mMediaPlayer->prepareAsync();
}

void GonkOffloadPlayer::ShutdownInternal() { ResetInternal(); }

void GonkOffloadPlayer::ResetInternal() {
  if (mNativeWindow) {
    mNativeWindow->setNewFrameCallback(nullptr);
    mNativeWindow = nullptr;
  }
  if (mMediaPlayer) {
    mMediaPlayer->setListener(0);
    mMediaPlayer->disconnect();
    mMediaPlayer = nullptr;
  }
  mMediaPlayerListener = nullptr;
  mPrepared = false;

  // Don't reset mSentFirstFrameLoadedEvent because next time when we are
  // re-intializing, we are leaving dormant state, and we don't want to sent
  // first frame loaded event again.
}

void GonkOffloadPlayer::SeekInternal(const SeekTarget& aTarget, bool aVisible) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mMediaPlayer);

  mVisibleSeek = aVisible;
  int msec = aTarget.GetTime().ToMicroseconds() / 1000;
  MediaPlayerSeekMode mode;
  switch (aTarget.GetType()) {
    default:
    case SeekTarget::PrevSyncPoint:
      mode = MediaPlayerSeekMode::SEEK_PREVIOUS_SYNC;
      break;
    case SeekTarget::Accurate:
    case SeekTarget::NextFrame:
      mode = MediaPlayerSeekMode::SEEK_CLOSEST;
      break;
  }

  if (mMediaPlayer->seekTo(msec, mode) != OK) {
    NotifySeeked(false);
    return;
  }

  if (mVisibleSeek) {
    NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
    NotifyPlaybackEvent(MediaPlaybackEvent::SeekStarted);
  }
}

bool GonkOffloadPlayer::UpdateCurrentPosition() {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaPlayer) {
    int msec;
    mMediaPlayer->getCurrentPosition(&msec);
    mCurrentPosition = TimeUnit::FromMicroseconds(msec * 1000ll);
    return mMediaPlayer->isPlaying();
  }
  return false;
}

void GonkOffloadPlayer::Notify(int msg, int ext1, int ext2) {
  MOZ_ASSERT(OnTaskQueue());

  switch (msg) {
    case MEDIA_PREPARED:
      NotifyPrepared();
      NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
      break;
    case MEDIA_PLAYBACK_COMPLETE:
      // Ensure readyState is updated before firing the 'ended' event.
      NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);
      NotifyPlaybackEvent(MediaPlaybackEvent::PlaybackEnded);
      break;
    case MEDIA_BUFFERING_UPDATE: {
      TimeUnit duration = mDuration.Ref().value();
      TimeUnit current = mCurrentPosition.Ref();
      TimeUnit bufferd = std::max(duration * ext1 / 100, current);
      mBuffered = TimeIntervals(TimeInterval(current, bufferd));
    } break;
    case MEDIA_SEEK_COMPLETE:
      if (mVisibleSeek) {
        UpdateCurrentPosition();
        NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
      }
      NotifySeeked(true);
      break;
    case MEDIA_SET_VIDEO_SIZE:
      mInfo.mVideo.mDisplay = gfx::IntSize(ext1, ext2);
      mPackedDisplaySize = (((int64_t)ext1) << 32) | ext2;
      break;
    case MEDIA_STARTED:
      UpdateCurrentPositionPeriodically();
      break;
    case MEDIA_ERROR:
      NotifyPlaybackError(MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
      break;
    case MEDIA_INFO:
      NotifyMediaInfo(ext1, ext2);
      break;
    default:
      break;
  }
}

void GonkOffloadPlayer::NotifyMediaInfo(int ext1, int ext2) {
  MOZ_ASSERT(OnTaskQueue());

  switch (ext1) {
    case MEDIA_INFO_BUFFERING_START:
      NotifyNextFrameStatus(
          MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING);
      break;
    case MEDIA_INFO_BUFFERING_END:
      NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
      break;
    case MEDIA_INFO_NOT_SEEKABLE:
      mInfo.mMediaSeekable = false;
      NotifyMediaNotSeekable();
      break;
  }
}

void GonkOffloadPlayer::NotifyPrepared() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mMediaPlayer);

  // Get duration.
  int durationMs = 0;
  if (mMediaPlayer->getDuration(&durationMs) == OK) {
    mDuration = Some(TimeUnit::FromMicroseconds(durationMs * 1000ll));
    mInfo.mMetadataDuration = mDuration;
  }

  // Get track info.
  Parcel request, reply;
  request.writeInterfaceToken(IMediaPlayer::descriptor);
  request.writeInt32(INVOKE_ID_GET_TRACK_INFO);
  if (mMediaPlayer->invoke(request, &reply) == OK) {
    int32_t trackCount;
    reply.readInt32(&trackCount);
    for (int32_t i = 0; i < trackCount; ++i) {
      int32_t dummy, trackType;
      reply.readInt32(&dummy);
      reply.readInt32(&trackType);
      String8 mime(reply.readString16());
      String8 lang(reply.readString16());

      if (trackType == MEDIA_TRACK_TYPE_AUDIO) {
        mInfo.EnableAudio();
        mInfo.mAudio.mMimeType = mime.c_str();
      } else if (trackType == MEDIA_TRACK_TYPE_VIDEO) {
        mInfo.EnableVideo();
        mInfo.mVideo.mMimeType = mime.c_str();
      } else if (trackType == MEDIA_TRACK_TYPE_SUBTITLE) {
        int32_t isAuto, isDefault, isForced;
        reply.readInt32(&isAuto);
        reply.readInt32(&isDefault);
        reply.readInt32(&isForced);
      }
    }
  }

  mPrepared = true;
  NotifyMetadataLoaded(MakeUnique<MediaInfo>(mInfo),
                       MakeUnique<MetadataTags>());

  // There may be a pending seek job when we are initializing. Fire it now.
  FirePendingSeekIfExists();

  // NuPlayer won't render any frame before started, so call seekTo() to force
  // it to output the first frame. If we just fired a pending seek, no need to
  // seek again.
  if (!Seeking() && VideoEnabled()) {
    HandleSeek(SeekTarget(TimeUnit::Zero(), SeekTarget::Accurate), false);
  }

  if (!mSentFirstFrameLoadedEvent && !VideoEnabled()) {
    mSentFirstFrameLoadedEvent = true;
    NotifyFirstFrameLoaded(MakeUnique<MediaInfo>(mInfo));
  }
}

void GonkOffloadPlayer::PlayStateChanged() {
  MOZ_ASSERT(OnTaskQueue());

  if (mMediaPlayer) {
    if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
      mMediaPlayer->start();
      // NuPlayer won't notify MEDIA_STARTED when its offload audio mode is
      // enabled, which might be a bug, so we also need to trigger position
      // update timer here after start() is called.
      UpdateCurrentPositionPeriodically();
    } else if (mPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
      mMediaPlayer->pause();
    }
  }
  UpdateAudibleState();
  MediaOffloadPlayer::PlayStateChanged();
}

void GonkOffloadPlayer::VolumeChanged() {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaPlayer) {
    mMediaPlayer->setVolume((float)mVolume, (float)mVolume);
  }
}

void GonkOffloadPlayer::LoopingChanged() {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaPlayer) {
    mMediaPlayer->setLooping(mLooping);
  }
}

void GonkOffloadPlayer::PlaybackSettingsChanged() {
  MOZ_ASSERT(OnTaskQueue());
  AudioPlaybackRate rate;
  if (mMediaPlayer && mMediaPlayer->getPlaybackSettings(&rate) == OK) {
    rate.mSpeed = (float)mPlaybackRate;
    rate.mPitch = mPreservesPitch ? 1.0f : (float)mPlaybackRate;
    mMediaPlayer->setPlaybackSettings(rate);
  }
}

void GonkOffloadPlayer::UpdateAudibleState() {
  MOZ_ASSERT(OnTaskQueue());
  mIsAudioDataAudible =
      mInfo.HasAudio() && mMediaPlayer && mMediaPlayer->isPlaying();
}

}  // namespace mozilla
