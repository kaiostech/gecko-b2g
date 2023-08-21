/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OmxTrackEncoder_h_
#define OmxTrackEncoder_h_

#include "TrackEncoder.h"

namespace android {
class OMXVideoEncoder;
class OMXAudioEncoder;
}  // namespace android

/**
 * There are two major classes defined in file OmxTrackEncoder;
 * OmxVideoTrackEncoder and OmxAudioTrackEncoder, the video and audio track
 * encoder for media type AVC/H.264 and AAC. OMXCodecWrapper wraps and controls
 * an instance of MediaCodec, defined in libstagefright, runs on Android Jelly
 * Bean platform.
 */

namespace mozilla {

class OmxAudioTrackEncoder : public AudioTrackEncoder {
 public:
  OmxAudioTrackEncoder(TrackRate aRate,
                       MediaQueue<EncodedFrame>& aEncodedDataQueue);
  ~OmxAudioTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() override = 0;

  nsresult Encode(AudioSegment* aSegment) override;

 protected:
  nsresult Init(int aChannels) override = 0;

  // Append encoded frames to aContainer.
  nsresult AppendEncodedFrames();

  android::OMXAudioEncoder* mEncoder;
  TrackRate mSamplingRate;
};

class OmxAMRAudioTrackEncoder final : public OmxAudioTrackEncoder {
 public:
  OmxAMRAudioTrackEncoder(TrackRate aTrackRate,
                          MediaQueue<EncodedFrame>& aEncodedDataQueue)
      : OmxAudioTrackEncoder(aTrackRate, aEncodedDataQueue),
        // force mWB to be true by default
        mWB(true) {}

  enum {
    AMR_NB_SAMPLERATE = 8000,
    AMR_WB_SAMPLERATE = 16000,
  };
  already_AddRefed<TrackMetadataBase> GetMetadata() override;

 protected:
  nsresult Init(int aChannels) override;
  bool mWB;
};

}  // namespace mozilla
#endif
