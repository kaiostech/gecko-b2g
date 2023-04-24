/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <media/stagefright/MediaCodecList.h>

#include "GLContext.h"
#include "GLContextProvider.h"
#include "nsTArray.h"
#include "GfxInfo.h"
#include "prenv.h"

using namespace mozilla::widget;

class GLStrings {
  nsCString mVendor;
  nsCString mRenderer;
  nsCString mVersion;
  nsTArray<nsCString> mExtensions;
  bool mReady;

 public:
  GLStrings() : mReady(false) {}

  const nsCString& Vendor() {
    EnsureInitialized();
    return mVendor;
  }

  // This spoofed value wins, even if the environment variable
  // MOZ_GFX_SPOOF_GL_VENDOR was set.
  void SpoofVendor(const nsCString& s) { mVendor = s; }

  const nsCString& Renderer() {
    EnsureInitialized();
    return mRenderer;
  }

  // This spoofed value wins, even if the environment variable
  // MOZ_GFX_SPOOF_GL_RENDERER was set.
  void SpoofRenderer(const nsCString& s) { mRenderer = s; }

  const nsCString& Version() {
    EnsureInitialized();
    return mVersion;
  }

  // This spoofed value wins, even if the environment variable
  // MOZ_GFX_SPOOF_GL_VERSION was set.
  void SpoofVersion(const nsCString& s) { mVersion = s; }

  const nsTArray<nsCString>& Extensions() {
    EnsureInitialized();
    return mExtensions;
  }

  void EnsureInitialized() {
    if (mReady) {
      return;
    }

    RefPtr<mozilla::gl::GLContext> gl;
    nsCString discardFailureId;
    gl = mozilla::gl::GLContextProvider::CreateHeadless(
        {mozilla::gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE},
        &discardFailureId);

    if (!gl) {
      // Setting mReady to true here means that we won't retry. Everything will
      // remain blocklisted forever. Ideally, we would like to update that once
      // any GLContext is successfully created, like the compositor's GLContext.
      mReady = true;
      return;
    }

    gl->MakeCurrent();

    if (mVendor.IsEmpty()) {
      const char* spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
      if (spoofedVendor) {
        mVendor.Assign(spoofedVendor);
      } else {
        mVendor.Assign((const char*)gl->fGetString(LOCAL_GL_VENDOR));
      }
    }

    if (mRenderer.IsEmpty()) {
      const char* spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
      if (spoofedRenderer) {
        mRenderer.Assign(spoofedRenderer);
      } else {
        mRenderer.Assign((const char*)gl->fGetString(LOCAL_GL_RENDERER));
      }
    }

    if (mVersion.IsEmpty()) {
      const char* spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
      if (spoofedVersion) {
        mVersion.Assign(spoofedVersion);
      } else {
        mVersion.Assign((const char*)gl->fGetString(LOCAL_GL_VERSION));
      }
    }

    if (mExtensions.IsEmpty()) {
      nsCString rawExtensions;
      rawExtensions.Assign((const char*)gl->fGetString(LOCAL_GL_EXTENSIONS));
      rawExtensions.Trim(" ");

      for (auto extension : rawExtensions.Split(' ')) {
        mExtensions.AppendElement(extension);
      }
    }

    mReady = true;
  }
};

GfxInfo::GfxInfo() {}

GfxInfo::~GfxInfo() {}

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after
 * gfxPlatform initialization has occurred because they depend on it for
 * information. (See bug 591561) */
nsresult GfxInfo::GetD2DEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

nsresult GfxInfo::GetDWriteEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetTestType(nsAString& aTestType) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
GfxInfo::GetDrmRenderDevice(nsACString& aDrmRenderDevice) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString& aDwriteVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString& aCleartypeParams) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString& aAdapterDescription) {
  aAdapterDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString& aAdapterDescription) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM(uint32_t* aAdapterRAM) {
  *aAdapterRAM = 0;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(uint32_t* aAdapterRAM) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString& aAdapterDriver) {
  aAdapterDriver.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString& aAdapterDriver) {
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVendor; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) {
  aAdapterDriverVendor.Truncate();
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVendor2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString& aAdapterDriverVersion) {
  aAdapterDriverVersion.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString& aAdapterDriverVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString& aAdapterDriverDate) {
  aAdapterDriverDate.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString& aAdapterDriverDate) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString& aAdapterVendorID) {
  aAdapterVendorID.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString& aAdapterVendorID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString& aAdapterDeviceID) {
  aAdapterDeviceID.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString& aAdapterDeviceID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active) { return NS_ERROR_FAILURE; }

const nsTArray<GfxDriverInfo>& GfxInfo::GetGfxDriverInfo() {
  return *sDriverInfo;
}

uint32_t GfxInfo::OperatingSystemVersion() { return 0; }

NS_IMETHODIMP
GfxInfo::GetWindowProtocol(nsAString& aWindowProtocol) {
  return NS_ERROR_FAILURE;
}

nsresult GfxInfo::GetFeatureStatusImpl(
    int32_t aFeature, int32_t* aStatus, nsAString& aSuggestedDriverVersion,
    const nsTArray<GfxDriverInfo>& aDriverInfo, nsACString& aFailureId,
    OperatingSystem* aOS) {
  NS_ENSURE_ARG_POINTER(aStatus);
  OperatingSystem os = OperatingSystem::Android;
  if (aOS) *aOS = os;

  if (aFeature == nsIGfxInfo::FEATURE_WEBRENDER) {
    *aStatus = nsIGfxInfo::FEATURE_ALLOW_ALWAYS;
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_ENCODE) {
    *aStatus = WebRtcHwVp8EncodeSupported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_ENCODE";
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_DECODE) {
    *aStatus = WebRtcHwVp8DecodeSupported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_DECODE";
  } else if (aFeature == FEATURE_WEBRTC_HW_ACCELERATION_H264) {
    *aStatus = WebRtcHwH264Supported();
    aFailureId = "FEATURE_FAILURE_WEBRTC_H264";
  } else if (aFeature == FEATURE_WEBRENDER_SHADER_CACHE) {
    // Program binaries are known to be buggy on Adreno 3xx. While we haven't
    // encountered any correctness or stability issues with them, loading them
    // fails more often than not, so is a waste of time. Better to just not
    // even attempt to cache them. See bug 1615574.
    auto glStrings = new GLStrings();
    const bool isAdreno3xx =
        glStrings->Renderer().Find("Adreno (TM) 3") >= 0;
    if (isAdreno3xx) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
      aFailureId = "FEATURE_FAILURE_ADRENO_3XX";
    } else {
      *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
    }
  } else if (aFeature == FEATURE_GL_SWIZZLE) {
    // Swizzling appears to be buggy on PowerVR Rogue devices with webrender.
    // See bug 1704783.
    auto glStrings = new GLStrings();
    const bool isPowerVRRogue =
        glStrings->Renderer().Find("PowerVR Rogue") >= 0;
    if (isPowerVRRogue) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
      aFailureId = "FEATURE_FAILURE_POWERVR_ROGUE";
    } else {
      *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
    }
  } else if (aFeature == FEATURE_WEBGL_OPENGL) {
    *aStatus = nsIGfxInfo::FEATURE_STATUS_OK;
  } else {
    return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus,
                                             aSuggestedDriverVersion,
                                             aDriverInfo, aFailureId, &os);
  }

  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetEmbeddedInFirefoxReality(bool* aEmbeddedInFirefoxReality) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetHasBattery(bool* aHasBattery) {
  // All Gonk devices should have a battery!
  *aHasBattery = true;
  return NS_OK;
}

static bool HwCodecSupported(const char* aMimeType, bool aIsEncoder) {
  uint32_t flags = android::MediaCodecList::kHardwareCodecsOnly;
  android::Vector<android::AString> matchingCodecs;
  android::MediaCodecList::findMatchingCodecs(aMimeType, aIsEncoder, flags,
                                              &matchingCodecs);
  return !matchingCodecs.isEmpty();
}

int32_t GfxInfo::WebRtcHwVp8EncodeSupported() {
  bool supported = HwCodecSupported("video/x-vnd.on2.vp8", true);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

int32_t GfxInfo::WebRtcHwVp8DecodeSupported() {
  bool supported = HwCodecSupported("video/x-vnd.on2.vp8", false);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

int32_t GfxInfo::WebRtcHwH264Supported() {
  bool supported = HwCodecSupported("video/avc", true) &&
                   HwCodecSupported("video/avc", false);
  return supported ? nsIGfxInfo::FEATURE_STATUS_OK
                   : nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug

NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString&) { return NS_OK; }

NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t) { return NS_OK; }

#endif
