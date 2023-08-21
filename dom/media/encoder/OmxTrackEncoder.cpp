/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxTrackEncoder.h"
#include "OMXCodecWrapper.h"
#include "VideoUtils.h"
#include "ISOTrackMetadata.h"
#include "GeckoProfiler.h"

#ifdef MOZ_WIDGET_GONK
#  include <android/log.h>
#  define OMX_LOG(args...)                                              \
    do {                                                                \
      __android_log_print(ANDROID_LOG_INFO, "OmxTrackEncoder", ##args); \
    } while (0)
#else
#  define OMX_LOG(args, ...)
#endif

using namespace android;

namespace mozilla {

#define ENCODER_CONFIG_FRAME_RATE 30            // fps
#define GET_ENCODED_VIDEO_FRAME_TIMEOUT 100000  // microseconds

OmxAudioTrackEncoder::OmxAudioTrackEncoder(
    TrackRate aTrackRate, MediaQueue<EncodedFrame>& aEncodedDataQueue)
    : AudioTrackEncoder(aTrackRate, aEncodedDataQueue),
      mSamplingRate(aTrackRate) {}

OmxAudioTrackEncoder::~OmxAudioTrackEncoder() {
  if (mEncoder) {
    delete mEncoder;
  }
}

nsresult OmxAudioTrackEncoder::AppendEncodedFrames() {
  nsTArray<uint8_t> frameData;
  int outFlags = 0;
  int64_t outTimeUs = -1;

  nsresult rv = mEncoder->GetNextEncodedFrame(&frameData, &outTimeUs, &outFlags,
                                              3000);  // wait up to 3ms
  NS_ENSURE_SUCCESS(rv, rv);

  if (!frameData.IsEmpty() ||
      outFlags & OMXCodecWrapper::BUFFER_EOS) {  // Some hw codec may send out
                                                 // EOS with an empty frame
    bool isCSD = false;
    if (outFlags &
        OMXCodecWrapper::BUFFER_CODEC_CONFIG) {  // codec specific data
      isCSD = true;
    }
    auto audioData = MakeRefPtr<EncodedFrame::FrameData>();
    audioData->SwapElements(frameData);

    mEncodedDataQueue.Push(MakeAndAddRef<EncodedFrame>(
        media::TimeUnit::FromMicroseconds(outTimeUs),
        0,  // TODO what's the correct value?
        16000,
        isCSD ? EncodedFrame::AMR_AUDIO_CSD : EncodedFrame::AMR_AUDIO_FRAME,
        std::move(audioData)));

    // Tell MediaQueue that we've done ecoding
    if (outFlags & OMXCodecWrapper::BUFFER_EOS) {  // last frame
      mEncodedDataQueue.Finish();
    }
  }

  return NS_OK;
}

nsresult OmxAudioTrackEncoder::Encode(AudioSegment* aSegment) {
  MOZ_ASSERT(aSegment);
  MOZ_ASSERT(mInitialized || mCanceled);
  if (mCanceled || !mInitialized || IsEncodingComplete()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  if (aSegment->GetDuration() == 0) {
    // Notify EOS at least once, even if segment is empty.
    if (mEndOfStream) {
      bool EosSetInEncoder = false;
      rv = mEncoder->Encode(*aSegment, OMXCodecWrapper::BUFFER_EOS,
                            &EosSetInEncoder);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Nothing to encode but encoder could still have encoded data for earlier
    // input.
    return AppendEncodedFrames();
  }

  while (aSegment->GetDuration() > 0) {
    rv = mEncoder->Encode(*aSegment, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AppendEncodedFrames();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

already_AddRefed<TrackMetadataBase> OmxAMRAudioTrackEncoder::GetMetadata() {
  if (!mInitialized) {
    return nullptr;
  }

  RefPtr<AMRTrackMetadata> meta = new AMRTrackMetadata(mWB);
  return meta.forget();
}

nsresult OmxAMRAudioTrackEncoder::Init(int aChannels) {
  mChannels = aChannels;

  mEncoder = mWB ? OMXCodecWrapper::CreateAMRWBEncoder()
                 : OMXCodecWrapper::CreateAMRNBEncoder();
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  nsresult rv =
      mEncoder->Configure(mChannels, mSamplingRate, AMR_WB_SAMPLERATE);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  SetInitialized();

  return NS_OK;
}

}  // namespace mozilla
