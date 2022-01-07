/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineCameraVideoSource.h"

#include "MediaEnginePrefs.h"
#include "MediaManager.h"
#include "mozilla/dom/MediaTrackSettingsBinding.h"
#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla {

extern LazyLogModule gMediaManagerLog;
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))

using dom::MediaSourceEnum;
using dom::MediaTrackConstraintSet;
using dom::MediaTrackSettings;
using dom::VideoFacingModeEnum;

/* static */
camera::CaptureEngine MediaEngineCameraVideoSource::CaptureEngine(
    MediaSourceEnum aMediaSource) {
  switch (aMediaSource) {
    case MediaSourceEnum::Browser:
      return camera::BrowserEngine;
    case MediaSourceEnum::Camera:
      return camera::CameraEngine;
    case MediaSourceEnum::Screen:
      return camera::ScreenEngine;
    case MediaSourceEnum::Window:
      return camera::WinEngine;
    default:
      MOZ_CRASH();
  }
}

/* static */
Maybe<VideoFacingModeEnum> MediaEngineCameraVideoSource::GetFacingMode(
    const nsString& aDeviceName) {
  // Set facing mode based on device name.
#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
  if (aDeviceName.EqualsLiteral("back")) {
    return Some(VideoFacingModeEnum::Environment);
  }
  if (aDeviceName.EqualsLiteral("front")) {
    return Some(VideoFacingModeEnum::User);
  }
#endif  // MOZ_B2G_CAMERA
#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
  // Names are generated. Example: "Camera 0, Facing back, Orientation 90"
  //
  // See media/webrtc/trunk/webrtc/modules/video_capture/android/java/src/org/
  // webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java

  if (aDeviceName.Find(u"Facing back"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::Environment);
  }
  if (aDeviceName.Find(u"Facing front"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::User);
  }
#endif  // ANDROID
#ifdef XP_MACOSX
  // Kludge to test user-facing cameras on OSX.
  if (aDeviceName.Find(u"Face"_ns) != -1) {
    return Some(VideoFacingModeEnum::User);
  }
#endif
#ifdef XP_WIN
  // The cameras' name of Surface book are "Microsoft Camera Front" and
  // "Microsoft Camera Rear" respectively.

  if (aDeviceName.Find(u"Front"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::User);
  }
  if (aDeviceName.Find(u"Rear"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::Environment);
  }
#endif  // WINDOWS

  return Nothing();
}

MediaEngineCameraVideoSource::MediaEngineCameraVideoSource(
    const MediaDevice* aMediaDevice)
    : mSettingsUpdatedByFrame(MakeAndAddRef<media::Refcountable<AtomicBool>>()),
      mSettings(MakeAndAddRef<media::Refcountable<MediaTrackSettings>>()),
      mMediaDevice(aMediaDevice),
      mCapEngine(CaptureEngine(aMediaDevice->mMediaSource)),
      mFirstFramePromise(mFirstFramePromiseHolder.Ensure(__func__)) {
  mSettings->mWidth.Construct(0);
  mSettings->mHeight.Construct(0);
  mSettings->mFrameRate.Construct(0);
  if (mCapEngine == camera::CameraEngine) {
    // Only cameras can have a facing mode.
    Maybe<VideoFacingModeEnum> facingMode =
        GetFacingMode(mMediaDevice->mRawName);
    if (facingMode.isSome()) {
      NS_ConvertASCIItoUTF16 facingString(
          dom::VideoFacingModeEnumValues::GetString(*facingMode));
      mSettings->mFacingMode.Construct(facingString);
      mFacingMode.emplace(facingString);
    }
  }
}

MediaEngineCameraVideoSource::~MediaEngineCameraVideoSource() {
  mFirstFramePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
}

uint32_t MediaEngineCameraVideoSource::GetDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints,
    const DistanceCalculation aCalculate) const {
  if (aCalculate == kFeasibility) {
    return GetFeasibilityDistance(aCandidate, aConstraints);
  }
  return GetFitnessDistance(aCandidate, aConstraints);
}

uint32_t MediaEngineCameraVideoSource::GetFitnessDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints) const {
  AssertIsOnOwningThread();

  // Treat width|height|frameRate == 0 on capability as "can do any".
  // This allows for orthogonal capabilities that are not in discrete steps.

  typedef MediaConstraintsHelper H;
  uint64_t distance =
      uint64_t(H::FitnessDistance(mFacingMode, aConstraints.mFacingMode)) +
      uint64_t(aCandidate.width ? H::FitnessDistance(int32_t(aCandidate.width),
                                                     aConstraints.mWidth)
                                : 0) +
      uint64_t(aCandidate.height
                   ? H::FitnessDistance(int32_t(aCandidate.height),
                                        aConstraints.mHeight)
                   : 0) +
      uint64_t(aCandidate.maxFPS ? H::FitnessDistance(double(aCandidate.maxFPS),
                                                      aConstraints.mFrameRate)
                                 : 0);
  return uint32_t(std::min(distance, uint64_t(UINT32_MAX)));
}

uint32_t MediaEngineCameraVideoSource::GetFeasibilityDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints) const {
  AssertIsOnOwningThread();

  // Treat width|height|frameRate == 0 on capability as "can do any".
  // This allows for orthogonal capabilities that are not in discrete steps.

  typedef MediaConstraintsHelper H;
  uint64_t distance =
      uint64_t(H::FitnessDistance(mFacingMode, aConstraints.mFacingMode)) +
      uint64_t(aCandidate.width
                   ? H::FeasibilityDistance(int32_t(aCandidate.width),
                                            aConstraints.mWidth)
                   : 0) +
      uint64_t(aCandidate.height
                   ? H::FeasibilityDistance(int32_t(aCandidate.height),
                                            aConstraints.mHeight)
                   : 0) +
      uint64_t(aCandidate.maxFPS
                   ? H::FeasibilityDistance(double(aCandidate.maxFPS),
                                            aConstraints.mFrameRate)
                   : 0);
  return uint32_t(std::min(distance, uint64_t(UINT32_MAX)));
}

// Find best capability by removing inferiors. May leave >1 of equal distance

/* static */
void MediaEngineCameraVideoSource::TrimLessFitCandidates(
    nsTArray<CapabilityCandidate>& aSet) {
  uint32_t best = UINT32_MAX;
  for (auto& candidate : aSet) {
    if (best > candidate.mDistance) {
      best = candidate.mDistance;
    }
  }
  for (size_t i = 0; i < aSet.Length();) {
    if (aSet[i].mDistance > best) {
      aSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
  MOZ_ASSERT(aSet.Length());
}

uint32_t MediaEngineCameraVideoSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets) const {
  AssertIsOnOwningThread();

  size_t num = NumCapabilities();
  nsTArray<CapabilityCandidate> candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(CapabilityCandidate(GetCapability(i)));
  }

  bool first = true;
  for (const NormalizedConstraintSet* ns : aConstraintSets) {
    for (size_t i = 0; i < candidateSet.Length();) {
      auto& candidate = candidateSet[i];
      uint32_t distance = GetFitnessDistance(candidate.mCapability, *ns);
      if (distance == UINT32_MAX) {
        candidateSet.RemoveElementAt(i);
      } else {
        ++i;
        if (first) {
          candidate.mDistance = distance;
        }
      }
    }
    first = false;
  }
  if (!candidateSet.Length()) {
    return UINT32_MAX;
  }
  TrimLessFitCandidates(candidateSet);
  return candidateSet[0].mDistance;
}

/* static */
void MediaEngineCameraVideoSource::LogCapability(
    const char* aHeader, const webrtc::CaptureCapability& aCapability,
    uint32_t aDistance) {
  static const char* const types[] = {
      "Unknown type", "I420",   "IYUV",     "RGB24", "ABGR", "ARGB",
      "ARGB4444",     "RGB565", "ARGB1555", "YUY2",  "YV12", "UYVY",
      "MJPEG",        "NV21",   "NV12",     "BGRA"};

  LOG("%s: %4u x %4u x %2u maxFps, %s. Distance = %" PRIu32, aHeader,
      aCapability.width, aCapability.height, aCapability.maxFPS,
      types[std::min(std::max(uint32_t(0), uint32_t(aCapability.videoType)),
                     uint32_t(sizeof(types) / sizeof(*types) - 1))],
      aDistance);
}

bool MediaEngineCameraVideoSource::ChooseCapability(
    const NormalizedConstraints& aConstraints, const MediaEnginePrefs& aPrefs,
    webrtc::CaptureCapability& aCapability,
    const DistanceCalculation aCalculate) {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  if (MOZ_LOG_TEST(gMediaManagerLog, LogLevel::Debug)) {
    LOG("ChooseCapability: prefs: %dx%d @%dfps", aPrefs.GetWidth(),
        aPrefs.GetHeight(), aPrefs.mFPS);
    MediaConstraintsHelper::LogConstraints(aConstraints);
    if (!aConstraints.mAdvanced.empty()) {
      LOG("Advanced array[%zu]:", aConstraints.mAdvanced.size());
      for (auto& advanced : aConstraints.mAdvanced) {
        MediaConstraintsHelper::LogConstraints(advanced);
      }
    }
  }

  switch (mCapEngine) {
    case camera::ScreenEngine:
    case camera::WinEngine: {
      FlattenedConstraints c(aConstraints);
      // The actual resolution to constrain around is not easy to find ahead of
      // time (and may in fact change over time), so as a hack, we push ideal
      // and max constraints down to desktop_capture_impl.cc and finish the
      // algorithm there.
      // TODO: This can be removed in bug 1453269.
      aCapability.width =
          (std::min(0xffff, c.mWidth.mIdeal.valueOr(0)) & 0xffff) << 16 |
          (std::min(0xffff, c.mWidth.mMax) & 0xffff);
      aCapability.height =
          (std::min(0xffff, c.mHeight.mIdeal.valueOr(0)) & 0xffff) << 16 |
          (std::min(0xffff, c.mHeight.mMax) & 0xffff);
      aCapability.maxFPS =
          c.mFrameRate.Clamp(c.mFrameRate.mIdeal.valueOr(aPrefs.mFPS));
      return true;
    }
    default:
      break;
  }

  nsTArray<CapabilityCandidate> candidateSet;
  size_t num = NumCapabilities();
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(CapabilityCandidate(GetCapability(i)));
  }

  if (mCapabilitiesAreHardcoded && mCapEngine == camera::CameraEngine) {
    // We have a hardcoded capability, which means this camera didn't report
    // discrete capabilities. It might still allow a ranged capability, so we
    // add a couple of default candidates based on prefs and constraints.
    // The chosen candidate will be propagated to StartCapture() which will fail
    // for an invalid candidate.
    MOZ_DIAGNOSTIC_ASSERT(candidateSet.Length() == 1);
    candidateSet.Clear();

    FlattenedConstraints c(aConstraints);
    // Reuse the code across both the low-definition (`false`) pref and
    // the high-definition (`true`) pref.
    // If there are constraints we try to satisfy them but we default to prefs.
    // Note that since constraints are from content and can literally be
    // anything we put (rather generous) caps on them.
    for (bool isHd : {false, true}) {
      webrtc::CaptureCapability cap;
      int32_t prefWidth = aPrefs.GetWidth(isHd);
      int32_t prefHeight = aPrefs.GetHeight(isHd);

      cap.width = c.mWidth.Get(prefWidth);
      cap.width = std::max(0, std::min(cap.width, 7680));

      cap.height = c.mHeight.Get(prefHeight);
      cap.height = std::max(0, std::min(cap.height, 4320));

      cap.maxFPS = c.mFrameRate.Get(aPrefs.mFPS);
      cap.maxFPS = std::max(0, std::min(cap.maxFPS, 480));

      if (cap.width != prefWidth) {
        // Width was affected by constraints.
        // We'll adjust the height too so the aspect ratio is retained.
        cap.height = cap.width * prefHeight / prefWidth;
      } else if (cap.height != prefHeight) {
        // Height was affected by constraints but not width.
        // We'll adjust the width too so the aspect ratio is retained.
        cap.width = cap.height * prefWidth / prefHeight;
      }

      if (candidateSet.Contains(cap, CapabilityComparator())) {
        continue;
      }
      LogCapability("Hardcoded capability", cap, 0);
      candidateSet.AppendElement(cap);
    }
  }

  // First, filter capabilities by required constraints (min, max, exact).

  for (size_t i = 0; i < candidateSet.Length();) {
    auto& candidate = candidateSet[i];
    candidate.mDistance =
        GetDistance(candidate.mCapability, aConstraints, aCalculate);
    LogCapability("Capability", candidate.mCapability, candidate.mDistance);
    if (candidate.mDistance == UINT32_MAX) {
      candidateSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }

  if (candidateSet.IsEmpty()) {
    LOG("failed to find capability match from %zu choices",
        candidateSet.Length());
    return false;
  }

  // Filter further with all advanced constraints (that don't overconstrain).

  for (const auto& cs : aConstraints.mAdvanced) {
    nsTArray<CapabilityCandidate> rejects;
    for (size_t i = 0; i < candidateSet.Length();) {
      if (GetDistance(candidateSet[i].mCapability, cs, aCalculate) ==
          UINT32_MAX) {
        rejects.AppendElement(candidateSet[i]);
        candidateSet.RemoveElementAt(i);
      } else {
        ++i;
      }
    }
    if (!candidateSet.Length()) {
      candidateSet.AppendElements(std::move(rejects));
    }
  }
  MOZ_ASSERT(
      candidateSet.Length(),
      "advanced constraints filtering step can't reduce candidates to zero");

  // Remaining algorithm is up to the UA.

  TrimLessFitCandidates(candidateSet);

  // Any remaining multiples all have the same distance. A common case of this
  // occurs when no ideal is specified. Lean toward defaults.
  uint32_t sameDistance = candidateSet[0].mDistance;
  {
    MediaTrackConstraintSet prefs;
    prefs.mWidth.Construct().SetAsLong() = aPrefs.GetWidth();
    prefs.mHeight.Construct().SetAsLong() = aPrefs.GetHeight();
    prefs.mFrameRate.Construct().SetAsDouble() = aPrefs.mFPS;
    NormalizedConstraintSet normPrefs(prefs, false);

    for (auto& candidate : candidateSet) {
      candidate.mDistance =
          GetDistance(candidate.mCapability, normPrefs, aCalculate);
    }
    TrimLessFitCandidates(candidateSet);
  }

  aCapability = candidateSet[0].mCapability;

  LogCapability("Chosen capability", aCapability, sameDistance);
  return true;
}

void MediaEngineCameraVideoSource::GetSettings(
    MediaTrackSettings& aOutSettings) const {
  aOutSettings = *mSettings;
}

}  // namespace mozilla
