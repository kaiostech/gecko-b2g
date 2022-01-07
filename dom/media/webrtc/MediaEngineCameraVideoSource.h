/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_CAMERA_VIDEO_SOURCE_H_
#define MEDIAENGINE_CAMERA_VIDEO_SOURCE_H_

#include "MediaEngineSource.h"
#include "mozilla/media/CamerasTypes.h"
#include "modules/video_capture/video_capture_defines.h"

namespace webrtc {
using CaptureCapability = VideoCaptureCapability;
}

namespace mozilla {

class MediaEngineCameraVideoSource : public MediaEngineSource {
 public:
  // Fitness distance is defined in
  // https://w3c.github.io/mediacapture-main/getusermedia.html#dfn-selectsettings

  // The main difference of feasibility and fitness distance is that if the
  // constraint is required ('max', or 'exact'), and the settings dictionary's
  // value for the constraint does not satisfy the constraint, the fitness
  // distance is positive infinity. Given a continuous space of settings
  // dictionaries comprising all discrete combinations of dimension and
  // frame-rate related properties, the feasibility distance is still in keeping
  // with the constraints algorithm.
  enum DistanceCalculation { kFitness, kFeasibility };

 private:
  struct CapabilityCandidate {
    explicit CapabilityCandidate(webrtc::CaptureCapability aCapability,
                                 uint32_t aDistance = 0)
        : mCapability(aCapability), mDistance(aDistance) {}

    const webrtc::CaptureCapability mCapability;
    uint32_t mDistance;
  };

  class CapabilityComparator {
   public:
    bool Equals(const CapabilityCandidate& aCandidate,
                const webrtc::CaptureCapability& aCapability) const {
      return aCandidate.mCapability == aCapability;
    }
  };

  uint32_t GetDistance(const webrtc::CaptureCapability& aCandidate,
                       const NormalizedConstraintSet& aConstraints,
                       const DistanceCalculation aCalculate) const;

  uint32_t GetFitnessDistance(
      const webrtc::CaptureCapability& aCandidate,
      const NormalizedConstraintSet& aConstraints) const;

  uint32_t GetFeasibilityDistance(
      const webrtc::CaptureCapability& aCandidate,
      const NormalizedConstraintSet& aConstraints) const;

  /**
   * Returns the number of capabilities for the underlying device.
   *
   * Guaranteed to return at least one capability.
   */
  virtual size_t NumCapabilities() const = 0;

  /**
   * Returns the capability with index `aIndex` for our assigned device.
   *
   * It is an error to call this with `aIndex >= NumCapabilities()`.
   *
   * The lifetime of the returned capability is the same as for this source.
   */
  virtual webrtc::CaptureCapability GetCapability(size_t aIndex) const = 0;

  static void TrimLessFitCandidates(nsTArray<CapabilityCandidate>& aSet);

  static Maybe<dom::VideoFacingModeEnum> GetFacingMode(
      const nsString& aDeviceName);

  static void LogCapability(const char* aHeader,
                            const webrtc::CaptureCapability& aCapability,
                            uint32_t aDistance);

 public:
  MediaEngineCameraVideoSource(const MediaDevice* aMediaDevice);

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets)
      const override;

  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

  RefPtr<GenericNonExclusivePromise> GetFirstFramePromise() const override {
    return mFirstFramePromise;
  }

  static camera::CaptureEngine CaptureEngine(dom::MediaSourceEnum aMediaSource);

 protected:
  ~MediaEngineCameraVideoSource();

  bool ChooseCapability(const NormalizedConstraints& aConstraints,
                        const MediaEnginePrefs& aPrefs,
                        webrtc::CaptureCapability& aCapability,
                        const DistanceCalculation aCalculate);

  struct AtomicBool {
    Atomic<bool> mValue;
  };

  // True when resolution settings have been updated from a real frame's
  // resolution. Threadsafe.
  // TODO: This can be removed in bug 1453269.
  const RefPtr<media::Refcountable<AtomicBool>> mSettingsUpdatedByFrame;

  // The current settings of this source.
  // Note that these may be different from the settings of the underlying device
  // since we scale frames to avoid fingerprinting.
  // Members are main thread only.
  const RefPtr<media::Refcountable<dom::MediaTrackSettings>> mSettings;
  MozPromiseHolder<GenericNonExclusivePromise> mFirstFramePromiseHolder;

  /**
   * True if mCapabilities only contains hardcoded capabilities. This can happen
   * if the underlying device is not reporting any capabilities. These can be
   * affected by constraints, so they're evaluated in ChooseCapability() rather
   * than GetCapability().
   *
   * This is mutable so that the const methods NumCapabilities() and
   * GetCapability() can reset it. Owning thread only.
   */
  mutable bool mCapabilitiesAreHardcoded = false;
  const RefPtr<const MediaDevice> mMediaDevice;

 private:
  const camera::CaptureEngine mCapEngine;  // source of media (cam, screen etc)
  RefPtr<GenericNonExclusivePromise> mFirstFramePromise;
  Maybe<nsString> mFacingMode;
};

}  // namespace mozilla

#endif  // MEDIAENGINE_CAMERA_VIDEO_SOURCE_H_
