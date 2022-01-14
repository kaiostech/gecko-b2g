/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HwcHAL.h"
#include "libdisplay/GonkDisplay.h"
#include "mozilla/Assertions.h"
#include "nsIScreen.h"
#include <dlfcn.h>

namespace mozilla {

HwcHAL::HwcHAL() : HwcHALBase() {
  // Some HALs don't want to open hwc twice.
  // If GetDisplay already load hwc module, we don't need to load again
  mHwc = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();

  if (!mHwc) {
    printf_stderr("HwcHAL Error: Cannot load hwcomposer");
    return;
  }
}

HwcHAL::~HwcHAL() { mHwc = nullptr; }

bool HwcHAL::Query(QueryType aType) { return false; }

int HwcHAL::Set(HwcList* aList, uint32_t aDisp) { return -1; }

int HwcHAL::ResetHwc() { return Set(nullptr, HWC_DISPLAY_PRIMARY); }

int HwcHAL::Prepare(HwcList* aList, uint32_t aDisp, hwc_rect_t aDispRect,
                    buffer_handle_t aHandle, int aFenceFd) {
  return -1;
}

bool HwcHAL::SupportTransparency() const { return true; }

uint32_t HwcHAL::GetGeometryChangedFlag(bool aGeometryChanged) const {
  return aGeometryChanged ? HWC_GEOMETRY_CHANGED : 0;
}

void HwcHAL::SetCrop(HwcLayer& aLayer, const hwc_rect_t& aSrcCrop) const {
  if (GetAPIVersion() >= HwcAPIVersion(1, 3)) {
    aLayer.sourceCropf.left = aSrcCrop.left;
    aLayer.sourceCropf.top = aSrcCrop.top;
    aLayer.sourceCropf.right = aSrcCrop.right;
    aLayer.sourceCropf.bottom = aSrcCrop.bottom;
  } else {
    aLayer.sourceCrop = aSrcCrop;
  }
}

bool HwcHAL::EnableVsync(bool aEnable) {
  if (!mHwc) {
    printf_stderr("Failed to get hwc\n");
    return false;
  }
  HWC2::Display* hwcDisplay = hwc2_getDisplayById(mHwc, HWC_DISPLAY_PRIMARY);
  auto error = hwc2_setVsyncEnabled(
      hwcDisplay, aEnable ? HWC2::Vsync::Enable : HWC2::Vsync::Disable);
  if (error != HWC2::Error::None) {
    printf_stderr("setVsyncEnabled: Failed to set vsync to %d on %d/%" PRIu64
                  ": %s (%d)",
                  aEnable, HWC_DISPLAY_PRIMARY, hwcDisplay->getId(),
                  to_string(error).c_str(), static_cast<int32_t>(error));
    return false;
  }
  return true;
}

bool HwcHAL::RegisterHwcEventCallback(const HwcHALProcs_t& aProcs) {
  if (!mHwc) {
    printf_stderr("Failed to get hwc\n");
    return false;
  }
  EnableVsync(false);

  // Register Vsync and Invalidate Callback only
  GetGonkDisplay()->registerVsyncCallBack(aProcs.vsync);
  GetGonkDisplay()->registerInvalidateCallBack(aProcs.invalidate);

  return true;
}

uint32_t HwcHAL::GetAPIVersion() const { return HWC_DEVICE_API_VERSION_2_0; }

// Create HwcHAL
UniquePtr<HwcHALBase> HwcHALBase::CreateHwcHAL() {
  return MakeUnique<HwcHAL>();
}

}  // namespace mozilla

extern "C" MOZ_EXPORT __attribute__((weak)) HWC2::Display* hwc2_getDisplayById(
    HWC2::Device* p, hwc2_display_t id) {
  return p->getDisplayById(id) ?
         p->getDisplayById(id) : p->getDisplayById(p->getDefaultDisplayId()) ;
}

extern "C" MOZ_EXPORT __attribute__((weak)) void hwc2_registerCallback(
    HWC2::Device* p, HWC2::ComposerCallback* callback, int32_t sequenceId) {
  return p->registerCallback(callback, sequenceId);
}

extern "C" MOZ_EXPORT __attribute__((weak)) HWC2::Error hwc2_setVsyncEnabled(
    HWC2::Display* p, HWC2::Vsync enabled) {
  return p->setVsyncEnabled(enabled);
}

extern "C" MOZ_EXPORT __attribute__((weak)) void hwc2_onHotplug(
    HWC2::Device* p, hwc2_display_t displayId, HWC2::Connection connection) {
  return p->onHotplug(displayId, connection);
}
