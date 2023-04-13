/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioOffloadPlayer.h"

#include <binder/IPCThreadState.h>
#include <media/AudioParameter.h>
#include "AudioOutput.h"
#include "DecoderTraits.h"
#include "MP3Demuxer.h"
#include "ReaderProxy.h"

namespace mozilla {

using namespace android;
using namespace mozilla::dom;
using namespace mozilla::media;

#define SAMPLE_QUEUE_UPPER_THRESHOLD_US 10000000
#define SAMPLE_QUEUE_LOWER_THRESHOLD_US 1000000

#define LOG(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (fmt, ##__VA_ARGS__))
#define LOGV(fmt, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (fmt, ##__VA_ARGS__))

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

AudioOffloadPlayer::AudioOffloadPlayer(MediaFormatReaderInit& aInit,
                                       const MediaContainerType& aType)
    : MediaOffloadPlayer(aInit),
      mAudioWatchManager(this, OwnerThread()),
      mReaderBuffered(OwnerThread(), TimeIntervals(),
                      "AudioOffloadPlayer::mReaderBuffered"),
      mMutex("AudioOffloadPlayer::mMutex") {
  if (aType.Type().AsString() == "audio/mpeg"_ns) {
    mDemuxer = new MP3Demuxer(mResource);
  } else {
    mIsOffloaded = false;
  }

  mReader = DecoderTraits::CreateReader(aType, aInit);
  mReaderProxy = new ReaderProxy(OwnerThread(), mReader);

  mSamplePopListener = mSampleQueue.PopFrontEvent().Connect(
      OwnerThread(), this, &AudioOffloadPlayer::OnSamplePopped);
}

void AudioOffloadPlayer::InitInternal() {
  MOZ_ASSERT(OnTaskQueue());

  RefPtr<AudioOffloadPlayer> self = this;
  if (mIsOffloaded) {
    MOZ_ASSERT(mDemuxer);
    LOG("AudioOffloadPlayer::InitInternal, initializing demuxer");
    mDemuxer->Init()
        ->Then(OwnerThread(), __func__, this,
               &AudioOffloadPlayer::OnDemuxerInitDone,
               &AudioOffloadPlayer::OnDemuxerInitFailed)
        ->Track(mDemuxerInitRequest);
  } else {
    LOG("AudioOffloadPlayer::InitInternal, initializing MediaFormatReader");
    nsresult rv = mReaderProxy->Init();
    if (NS_FAILED(rv)) {
      LOG("AudioOffloadPlayer::InitInternal, error: 0x%x", rv);
      NotifyPlaybackError(MediaResult(rv, __func__));
      return;
    }

    mReaderBuffered.Connect(mReaderProxy->CanonicalBuffered());
    mAudioWatchManager.Watch(mReaderBuffered,
                             &AudioOffloadPlayer::ReaderBufferedUpdated);

    mReaderProxy->SetCanonicalDuration(&mDuration);
    mReaderProxy->ReadMetadata()
        ->Then(OwnerThread(), __func__, this,
               &AudioOffloadPlayer::OnReaderMetadataRead,
               &AudioOffloadPlayer::OnReaderMetadataNotRead)
        ->Track(mMetadataRequest);
  }
}

void AudioOffloadPlayer::OnDemuxerInitDone(const MediaResult& aResult) {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::OnDemuxerInitDone");
  mDemuxerInitRequest.Complete();

  mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
  MOZ_ASSERT(mTrackDemuxer);
  auto trackInfo = mTrackDemuxer->GetInfo();
  auto* audioInfo = trackInfo->GetAsAudioInfo();
  MOZ_ASSERT(audioInfo);

  mInfo.mAudio = *audioInfo;
  mInfo.mMetadataDuration = Some(audioInfo->mDuration);
  mInfo.mMediaSeekable = mDemuxer->IsSeekable();
  mInfo.mMediaSeekableOnlyInBufferedRanges =
      mDemuxer->IsSeekableOnlyInBufferedRanges();
  mDuration = mInfo.mMetadataDuration;
  NotifyMetadataLoaded(MakeUnique<MediaInfo>(mInfo),
                       MakeUnique<MetadataTags>());

  OpenAudioSink();
}

void AudioOffloadPlayer::OnDemuxerInitFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::OnDemuxerInitFailed, error: %s",
      aError.ErrorName().get());
  mDemuxerInitRequest.Complete();
  NotifyPlaybackError(aError);
}

void AudioOffloadPlayer::OnReaderMetadataRead(MetadataHolder&& aMetadata) {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::OnReaderMetadataRead");
  mMetadataRequest.Complete();

  mInfo = *aMetadata.mInfo;

  if (mInfo.mMetadataDuration.isSome()) {
    mDuration = mInfo.mMetadataDuration;
  } else if (mInfo.mUnadjustedMetadataEndTime.isSome()) {
    const TimeUnit unadjusted = mInfo.mUnadjustedMetadataEndTime.ref();
    const TimeUnit adjustment = mInfo.mStartTime;
    mInfo.mMetadataDuration.emplace(unadjusted - adjustment);
    mDuration = mInfo.mMetadataDuration;
  }

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mDuration.Ref().isNothing()) {
    mDuration = Some(TimeUnit::FromInfinity());
  }

  NotifyMetadataLoaded(MakeUnique<MediaInfo>(mInfo),
                       MakeUnique<MetadataTags>());

  OpenAudioSink();
}

void AudioOffloadPlayer::OnReaderMetadataNotRead(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::OnReaderMetadataNotRead, error: %s",
      aError.ErrorName().get());
  mMetadataRequest.Complete();
  NotifyPlaybackError(aError);
}

void AudioOffloadPlayer::OpenAudioSink() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::OpenAudioSink");
  auto streamType = ConvertToAudioStreamType(mAudioChannel);
  auto sampleRate = mInfo.mAudio.mRate;
  auto channels = mInfo.mAudio.mChannels;
  auto channelMask = mInfo.mAudio.mChannelMap;

  mAudioSessionId = static_cast<audio_session_t>(
      AudioSystem::newAudioUniqueId(AUDIO_UNIQUE_ID_USE_SESSION));
#if ANDROID_VERSION >= 30
  AudioSystem::acquireAudioSessionId(mAudioSessionId, -1, 0);
#else
  AudioSystem::acquireAudioSessionId(mAudioSessionId, -1);
#endif
  mAudioSink = new AudioOutput(
      mAudioSessionId, IPCThreadState::self()->getCallingUid(), streamType);

  status_t err;
  if (mIsOffloaded) {
    MOZ_ASSERT(mTrackDemuxer);
    auto* trackDemuxer = static_cast<MP3TrackDemuxer*>(mTrackDemuxer.get());
    auto audioFormat = AUDIO_FORMAT_MP3;
    auto duration = mInfo.mAudio.mDuration.ToSeconds();
    auto bitrate = duration ? trackDemuxer->StreamLength() * 8 / duration : 0;

    audio_offload_info_t offloadInfo = AUDIO_INFO_INITIALIZER;
    offloadInfo.sample_rate = sampleRate;
    offloadInfo.channel_mask = channelMask;
    offloadInfo.format = audioFormat;
    offloadInfo.stream_type = streamType;
    offloadInfo.bit_rate = bitrate;
    offloadInfo.duration_us = mInfo.mAudio.mDuration.ToMicroseconds();
    offloadInfo.has_video = false;
    offloadInfo.is_streaming = false;
    offloadInfo.bit_width = mInfo.mAudio.mBitDepth;
    // FIXME: hardcode offload_buffer_size to 0 cause Qualcomm libavenhancement
    // is disabled and AVUtils::get()->getAudioMaxInputBufferSize() always
    // returns 0. If libavenhancement is enabled again and we want to query the
    // buffer size from it, we need to fill necessary metadata into an AMessage
    // object before calling that API.
    offloadInfo.offload_buffer_size = 0;

    LOG("AudioOffloadPlayer::OpenAudioSink, offload info: sample_rate=%u, "
        "channel_mask=0x%x, format=0x%x, stream_type=%d, bit_rate=%u, "
        "duration_us=%lld, has_video=%d, is_streaming=%d, bit_width=%u, "
        "offload_buffer_size=%u",
        offloadInfo.sample_rate, offloadInfo.channel_mask, offloadInfo.format,
        offloadInfo.stream_type, offloadInfo.bit_rate, offloadInfo.duration_us,
        offloadInfo.has_video, offloadInfo.is_streaming, offloadInfo.bit_width,
        offloadInfo.offload_buffer_size);

    err = mAudioSink->Open(sampleRate, channels, channelMask, audioFormat,
                           &AudioOffloadPlayer::AudioSinkCallback, this,
                           AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD, &offloadInfo);
    if (err == OK) {
      SendMetaDataToHal(offloadInfo);
    }
  } else {
#ifdef MOZ_SAMPLE_TYPE_S16
    MOZ_ASSERT(AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16);
    auto audioFormat = AUDIO_FORMAT_PCM_16_BIT;
#else
    MOZ_ASSERT(AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_FLOAT32);
    auto audioFormat = AUDIO_FORMAT_PCM_FLOAT;
#endif

    err = mAudioSink->Open(sampleRate, channels, channelMask, audioFormat,
                           &AudioOffloadPlayer::AudioSinkCallback, this,
                           AUDIO_OUTPUT_FLAG_NONE, nullptr);
  }

  if (err == OK) {
    LOG("AudioOffloadPlayer::OpenAudioSink, successful");
    mInitDone = true;
    VolumeChanged();
    PlaybackSettingsChanged();
    if (!FirePendingSeekIfExists()) {
      MaybeStartDemuxing();
      MaybeStartDecoding();
    }
  } else {
    if (mIsOffloaded) {
      LOG("AudioOffloadPlayer::OpenAudioSink, offloaded sink error, switching "
          "to non offloaded sink");
      mIsOffloaded = false;
      ResetInternal();
      InitInternal();
    } else {
      LOG("AudioOffloadPlayer::OpenAudioSink, non offloaded sink error");
      NotifyPlaybackError(
          MediaResult(NS_ERROR_DOM_MEDIA_MEDIASINK_ERR, __func__));
    }
  }
}

void AudioOffloadPlayer::SendMetaDataToHal(audio_offload_info_t& aOffloadInfo) {
  // If the playback is offloaded to h/w we pass the HAL some metadata
  // information. We don't want to do this for PCM because it will be going
  // through the AudioFlinger mixer before reaching the hardware
  AudioParameter param;

  param.addInt(String8(AUDIO_OFFLOAD_CODEC_SAMPLE_RATE),
               aOffloadInfo.sample_rate);
  param.addInt(String8(AUDIO_OFFLOAD_CODEC_NUM_CHANNEL),
               aOffloadInfo.channel_mask);
  param.addInt(String8(AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE),
               aOffloadInfo.bit_rate);

  auto* trackDemuxer = static_cast<MP3TrackDemuxer*>(mTrackDemuxer.get());
  auto& vbrInfo = trackDemuxer->VBRInfo();

  if (vbrInfo.Type() != FrameParser::VBRHeader::NONE) {
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES),
                 vbrInfo.EncoderDelay());
    param.addInt(String8(AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES),
                 vbrInfo.EncoderPadding());
  }

  mAudioSink->SetParameters(param.toString());
}

void AudioOffloadPlayer::ShutdownInternal() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::ShutdownInternal");

  ResetInternal();
  if (mReaderProxy) {
    mReaderProxy->Shutdown();
    mReaderProxy = nullptr;
  }
  mReader = nullptr;
  mDemuxer = nullptr;
  mSamplePopListener.Disconnect();
  mAudioWatchManager.Shutdown();
}

void AudioOffloadPlayer::ResetInternal() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::ResetInternal");

  mInitDone = false;
  mIsPlaying = false;
  Flush();

  if (mAudioSink) {
    mAudioSink->Stop();
    mAudioSink->Flush();
    mAudioSink->Close();
    mAudioSink = nullptr;
    AudioSystem::releaseAudioSessionId(mAudioSessionId, -1);
  }
  if (mTrackDemuxer) {
    mTrackDemuxer->Reset();
    mTrackDemuxer = nullptr;
  }
  if (mReaderProxy) {
    mReaderProxy->ReleaseResources();
  }
  mDemuxerInitRequest.DisconnectIfExists();
  mDemuxRequest.DisconnectIfExists();
  mDemuxerSeekRequest.DisconnectIfExists();
  mMetadataRequest.DisconnectIfExists();
  mAudioDataRequest.DisconnectIfExists();
  mReaderSeekRequest.DisconnectIfExists();
  mReaderBuffered.DisconnectIfConnected();
  mAudioWatchManager.Unwatch(mReaderBuffered,
                             &AudioOffloadPlayer::ReaderBufferedUpdated);
}

void AudioOffloadPlayer::SeekInternal(const SeekTarget& aTarget,
                                      bool aVisible) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mDemuxerSeekRequest.Exists());
  MOZ_ASSERT(!mReaderSeekRequest.Exists());
  LOG("AudioOffloadPlayer::SeekInternal");

  if (aVisible) {
    NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
    NotifyPlaybackEvent(MediaPlaybackEvent::SeekStarted);
  }

  Flush();

  RefPtr<AudioOffloadPlayer> self = this;
  if (mIsOffloaded) {
    mTrackDemuxer->Seek(aTarget.GetTime())
        ->Then(
            OwnerThread(), __func__,
            [this, self](const TimeUnit& aTime) {
              mDemuxerSeekRequest.Complete();
              mCurrentPosition = aTime;
              MaybeStartDemuxing();
              NotifySeeked(true);
            },
            [this, self](const MediaResult& aError) {
              mDemuxerSeekRequest.Complete();
              MaybeStartDemuxing();
              NotifySeeked(false);
            })
        ->Track(mDemuxerSeekRequest);
  } else {
    mReaderProxy->Seek(aTarget)
        ->Then(
            OwnerThread(), __func__,
            [this, self](const TimeUnit& aTime) {
              mReaderSeekRequest.Complete();
              mCurrentPosition = aTime;
              MaybeStartDecoding();
              NotifySeeked(true);
            },
            [this, self](const SeekRejectValue& aReject) {
              mReaderSeekRequest.Complete();
              MaybeStartDecoding();
              NotifySeeked(false);
            })
        ->Track(mReaderSeekRequest);
  }
}

void AudioOffloadPlayer::Flush() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("AudioOffloadPlayer::Flush");

  if (mAudioSink) {
    mAudioSink->Pause();
    mAudioSink->Flush();
  }

  if (mReaderProxy) {
    mReaderProxy->ResetDecode(TrackInfo::kAudioTrack);
  }
  {
    MutexAutoLock lock(mMutex);
    mSampleQueue.Reset();
    mSampleOffset = 0;
  }
  mInputEOS = false;
  mNotifiedEOS = false;
  mDemuxRequest.DisconnectIfExists();
  mAudioDataRequest.DisconnectIfExists();
  mStartPosition = TimeUnit::Invalid();

  if (mAudioSink && mIsPlaying) {
    mAudioSink->Start();
  }
}

void AudioOffloadPlayer::MaybeStartDemuxing() {
  MOZ_ASSERT(OnTaskQueue());

  if (!mInitDone || !mIsOffloaded) {
    return;
  }
  if (mDemuxRequest.Exists() || mDemuxerSeekRequest.Exists()) {
    return;
  }
  if (mSampleQueue.IsFinished() ||
      mSampleQueue.Duration() > SAMPLE_QUEUE_LOWER_THRESHOLD_US) {
    return;
  }

  LOG("AudioOffloadPlayer::MaybeStartDemuxing, start demuxing");
  DemuxSamples();
}

void AudioOffloadPlayer::DemuxSamples() {
  MOZ_ASSERT(OnTaskQueue());

  mTrackDemuxer->GetSamples(10)
      ->Then(OwnerThread(), __func__, this,
             &AudioOffloadPlayer::OnDemuxCompleted,
             &AudioOffloadPlayer::OnDemuxFailed)
      ->Track(mDemuxRequest);
}

void AudioOffloadPlayer::OnDemuxCompleted(
    RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
  MOZ_ASSERT(OnTaskQueue());
  mDemuxRequest.Complete();

  for (auto& sample : aSamples->GetSamples()) {
    if (!mSentFirstFrameLoadedEvent) {
      mSentFirstFrameLoadedEvent = true;
      NotifyFirstFrameLoaded(MakeUnique<MediaInfo>(mInfo));
    }
    // First sample after init/flush.
    if (!mStartPosition.IsValid()) {
      NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
      mStartPosition = sample->mTime;
    }
    mSampleQueue.Push(sample.get());
  }

  LOGV("AudioOffloadPlayer::OnDemuxCompleted, have %.1f seconds in queue",
       static_cast<double>(mSampleQueue.Duration()) / 1000000);

  // Pause demuxing if we have enough data.
  if (mSampleQueue.Duration() >= SAMPLE_QUEUE_UPPER_THRESHOLD_US) {
    LOG("AudioOffloadPlayer::OnDemuxCompleted, pause demuxing");
    return;
  }

  DemuxSamples();
}

void AudioOffloadPlayer::OnDemuxFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  mDemuxRequest.Complete();

  if (aError.Code() == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
    LOG("AudioOffloadPlayer::OnDemuxFailed, finish demuxing due to EOS");
    mInputEOS = true;
    mSampleQueue.Finish();
  } else {
    LOG("AudioOffloadPlayer::OnDemuxFailed, abort demuxing due to error: %s",
        aError.ErrorName().get());
    NotifyPlaybackError(aError);
    mSampleQueue.Finish();
  }
}

void AudioOffloadPlayer::MaybeStartDecoding() {
  MOZ_ASSERT(OnTaskQueue());

  if (!mInitDone || mIsOffloaded) {
    return;
  }
  if (mAudioDataRequest.Exists() || mReaderSeekRequest.Exists()) {
    return;
  }
  if (mSampleQueue.IsFinished() ||
      mSampleQueue.Duration() > SAMPLE_QUEUE_LOWER_THRESHOLD_US) {
    return;
  }

  LOG("AudioOffloadPlayer::MaybeStartDecoding, start decoding");
  DecodeAudio();
}

void AudioOffloadPlayer::DecodeAudio() {
  mReaderProxy->RequestAudioData()
      ->Then(OwnerThread(), __func__, this, &AudioOffloadPlayer::OnAudioDecoded,
             &AudioOffloadPlayer::OnAudioNotDecoded)
      ->Track(mAudioDataRequest);
}

void AudioOffloadPlayer::OnAudioDecoded(RefPtr<AudioData> aAudio) {
  MOZ_ASSERT(OnTaskQueue());
  mAudioDataRequest.Complete();

  if (!mSentFirstFrameLoadedEvent) {
    mSentFirstFrameLoadedEvent = true;
    NotifyFirstFrameLoaded(MakeUnique<MediaInfo>(mInfo));
  }
  // First sample after init/flush.
  if (!mStartPosition.IsValid()) {
    NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
    mStartPosition = aAudio->mTime;
  }

  mSampleQueue.Push(aAudio.get());

  LOGV("AudioOffloadPlayer::OnAudioDecoded, have %.1f seconds in queue",
       static_cast<double>(mSampleQueue.Duration()) / 1000000);

  // Pause decoding if we have enough data.
  if (mSampleQueue.Duration() >= SAMPLE_QUEUE_UPPER_THRESHOLD_US) {
    LOG("AudioOffloadPlayer::OnAudioDecoded, pause decoding");
    return;
  }

  DecodeAudio();
}

void AudioOffloadPlayer::OnAudioNotDecoded(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  mAudioDataRequest.Complete();

  switch (aError.Code()) {
    case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
      LOG("AudioOffloadPlayer::OnAudioNotDecoded, finish decoding due to EOS");
      mSampleQueue.Finish();
      mInputEOS = true;
      break;
    case NS_ERROR_DOM_MEDIA_CANCELED:
      LOG("AudioOffloadPlayer::OnAudioNotDecoded, cancel decoding");
      break;
    case NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA:
      LOG("AudioOffloadPlayer::OnAudioNotDecoded, waiting for data");
      // We currently don't support buffering state. Keep calling DecodeAudio.
      DecodeAudio();
      break;
    default:
      LOG("AudioOffloadPlayer::OnAudioNotDecoded, abort decoding due to error: "
          "%s",
          aError.ErrorName().get());
      mSampleQueue.Finish();
      NotifyPlaybackError(aError);
      break;
  }
}

void AudioOffloadPlayer::OnSamplePopped(const RefPtr<MediaData>& aSample) {
  MOZ_ASSERT(OnTaskQueue());

  if (mSampleQueue.AtEndOfStream()) {
    // If we have sent all samples into audio sink, stop it to let it drain its
    // internal buffered data.
    LOG("AudioOffloadPlayer::OnSamplePopped, no more samples, stop sink");
    mAudioSink->Stop();
    // As there is currently no EVENT_STREAM_END callback notification for
    // non offloaded audio sinks, we need to post the EOS here.
    if (!mIsOffloaded && mInputEOS && !mNotifiedEOS) {
      LOG("AudioOffloadPlayer::OnSamplePopped, notifying EOS");
      mNotifiedEOS = true;
      NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);
      NotifyPlaybackEvent(MediaPlaybackEvent::PlaybackEnded);
    }
    return;
  }

  MaybeStartDemuxing();
  MaybeStartDecoding();
}

/* static */
size_t AudioOffloadPlayer::AudioSinkCallback(GonkAudioSink* aAudioSink,
                                             void* aData, size_t aSize,
                                             void* aCookie,
                                             GonkAudioSink::cb_event_t aEvent) {
  RefPtr<AudioOffloadPlayer> self = static_cast<AudioOffloadPlayer*>(aCookie);

  switch (aEvent) {
    case GonkAudioSink::CB_EVENT_FILL_BUFFER:
      LOGV("AudioOffloadPlayer::AudioSinkCallback, CB_EVENT_FILL_BUFFER");
      return self->FillAudioBuffer(aData, aSize);

    case GonkAudioSink::CB_EVENT_STREAM_END:
      LOG("AudioOffloadPlayer::AudioSinkCallback, CB_EVENT_STREAM_END");
      self->NotifyEOSCallback();
      break;

    case GonkAudioSink::CB_EVENT_TEAR_DOWN:
      LOG("AudioOffloadPlayer::AudioSinkCallback, CB_EVENT_TEAR_DOWN");
      self->NotifyAudioTearDown();
      break;

    default:
      LOG("AudioOffloadPlayer::AudioSinkCallback, unknown event %d", aEvent);
      break;
  }
  return 0;
}

static Span<const uint8_t> ToByteSpan(MediaData* aSample) {
  switch (aSample->mType) {
    case MediaData::Type::RAW_DATA:
      // Calling |aSample->As<MediaRawData>()| causes compilation error under
      // debug build, because upstream didn't define |sType| in |MediaRawData|.
      // Hence use |static_cast| here.
      return *static_cast<MediaRawData*>(aSample);
    case MediaData::Type::AUDIO_DATA:
      return AsBytes(aSample->As<AudioData>()->Data());
    default:
      MOZ_CRASH("not reached");
      return Span<const uint8_t>();
  }
}

static size_t CopySpan(Span<uint8_t>& aDst, const Span<const uint8_t>& aSrc) {
  size_t copy = std::min(aDst.Length(), aSrc.Length());
  std::copy_n(aSrc.cbegin(), copy, aDst.begin());
  return copy;
}

size_t AudioOffloadPlayer::FillAudioBuffer(void* aData, size_t aSize) {
  MutexAutoLock lock(mMutex);

  auto dst = Span<uint8_t>(static_cast<uint8_t*>(aData), aSize);
  while (dst.Length()) {
    auto sample = mSampleQueue.PeekFront();
    if (!sample) {
      break;
    }

    auto src = ToByteSpan(sample);
    auto copied = CopySpan(dst, src.From(mSampleOffset));
    dst = dst.From(copied);

    mSampleOffset += copied;
    if (mSampleOffset == src.Length()) {
      sample = mSampleQueue.PopFront();
      mSampleOffset = 0;
    }
  }
  LOGV("AudioOffloadPlayer::FillAudioBuffer, done, have %.1f seconds in queue",
       static_cast<double>(mSampleQueue.Duration()) / 1000000);
  return aSize - dst.Length();
}

void AudioOffloadPlayer::NotifyEOSCallback() {
  nsresult rv = OwnerThread()->Dispatch(NS_NewRunnableFunction(
      "AudioOffloadPlayer::NotifyEOSCallback",
      [this, self = RefPtr<AudioOffloadPlayer>(this)]() {
        if (mInputEOS && !mNotifiedEOS) {
          LOG("AudioOffloadPlayer::NotifyEOSCallback, notifying EOS");
          mNotifiedEOS = true;
          NotifyNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);
          NotifyPlaybackEvent(MediaPlaybackEvent::PlaybackEnded);
        }
      }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

void AudioOffloadPlayer::NotifyAudioTearDown() {
  nsresult rv = OwnerThread()->Dispatch(NS_NewRunnableFunction(
      "AudioOffloadPlayer::NotifyAudioTearDown",
      [this, self = RefPtr<AudioOffloadPlayer>(this)]() {
        LOG("AudioOffloadPlayer::NotifyAudioTearDown, restarting");
        ResetInternal();
        HandleSeek(SeekTarget(mCurrentPosition, SeekTarget::Accurate), false);
        InitInternal();
      }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

bool AudioOffloadPlayer::UpdateCurrentPosition() {
  MOZ_ASSERT(OnTaskQueue());

  auto sampleRate = mInfo.mAudio.mRate;
  if (!mInitDone || !sampleRate) {
    return false;
  }

  if (mStartPosition.IsValid()) {
    uint32_t playedSamples = 0;
    mAudioSink->GetPosition(&playedSamples);

    auto playedSeconds = static_cast<double>(playedSamples) / sampleRate;
    mCurrentPosition = mStartPosition + TimeUnit::FromSeconds(playedSeconds);
  }
  return mPlayState == MediaDecoder::PLAY_STATE_PLAYING;
}

void AudioOffloadPlayer::NotifyDataArrived() {
  MOZ_ASSERT(OnTaskQueue());

  if (mInitDone) {
    if (mIsOffloaded) {
      mDemuxer->NotifyDataArrived();
      mBuffered = mTrackDemuxer->GetBuffered();
      MaybeStartDemuxing();
    } else {
      // ReaderProxy doesn't have this method, so directly call it on mReader.
      nsresult rv = mReader->OwnerThread()->Dispatch(NewRunnableMethod(
          "MediaFormatReader::NotifyDataArrived", mReader.get(),
          &MediaFormatReader::NotifyDataArrived));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      MaybeStartDecoding();
    }
  }
}

void AudioOffloadPlayer::PlayStateChanged() {
  MOZ_ASSERT(OnTaskQueue());

  if (mInitDone) {
    if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING && !mIsPlaying) {
      LOG("AudioOffloadPlayer::PlayStateChanged, start sink");
      mIsPlaying = true;
      mAudioSink->Start();
      UpdateCurrentPositionPeriodically();
    } else if (mPlayState == MediaDecoder::PLAY_STATE_PAUSED && mIsPlaying) {
      LOG("AudioOffloadPlayer::PlayStateChanged, pause sink");
      mIsPlaying = false;
      mAudioSink->Pause();
    }
  }
  UpdateAudibleState();
  MediaOffloadPlayer::PlayStateChanged();
}

void AudioOffloadPlayer::VolumeChanged() {
  MOZ_ASSERT(OnTaskQueue());
  if (mInitDone) {
    mAudioSink->SetVolume(static_cast<float>(mVolume));
  }
}

void AudioOffloadPlayer::PlaybackSettingsChanged() {
  MOZ_ASSERT(OnTaskQueue());
  if (mInitDone) {
    auto rate = AUDIO_PLAYBACK_RATE_DEFAULT;
    rate.mSpeed = static_cast<float>(mPlaybackRate);
    rate.mPitch = mPreservesPitch ? 1.0f : (float)mPlaybackRate;
    mAudioSink->SetPlaybackRate(rate);
  }
}

void AudioOffloadPlayer::UpdateAudibleState() {
  MOZ_ASSERT(OnTaskQueue());
  mIsAudioDataAudible = mInitDone && mIsPlaying;
}

}  // namespace mozilla
