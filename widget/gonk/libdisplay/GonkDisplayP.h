/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GONKDISPLAYP_H
#define GONKDISPLAYP_H

#include <gui/BufferQueue.h>

#include "HwcHAL.h"
#include "DisplaySurface.h"
#include "GonkDisplay.h"
#include "hardware/hwcomposer.h"
#include "hardware/power.h"
#if ANDROID_VERSION >= 30
#  include <android/hardware/power/IPower.h>
#else
#  include <android/hardware/power/1.0/IPower.h>
#endif
#include "NativeFramebufferDevice.h"
#include "NativeGralloc.h"
#include "ui/Fence.h"
#include "utils/RefBase.h"

namespace mozilla {

using namespace android;
#if ANDROID_VERSION >= 30
using ::android::hardware::power::IPower;
#else
using ::android::hardware::power::V1_0::IPower;
#endif
class MOZ_EXPORT GonkDisplayP : public GonkDisplay {
 public:
  GonkDisplayP();
  ~GonkDisplayP();

  void SetEnabled(bool enabled) override;

  void SetExtEnabled(bool enabled) override;

  void SetDisplayVisibility(bool visibility) override;

  void OnEnabled(OnEnabledCallbackType callback) override;

  void* GetHWCDevice() override;

  bool IsExtFBDeviceEnabled() override;

  bool SwapBuffers(DisplayType aDisplayType) override;

  ANativeWindowBuffer* DequeueBuffer(DisplayType aDisplayType) override;

  bool QueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType) override;

  void UpdateDispSurface(EGLDisplay aDisplayType, EGLSurface sur) override;

  NativeData GetNativeData(DisplayType aDisplayType,
                           IGraphicBufferProducer* aSink = nullptr) override;

  void NotifyBootAnimationStopped() override;

  int TryLockScreen() override;

  void UnlockScreen() override;

  sp<ANativeWindow> GetSurface(DisplayType aDisplayType) override;

  sp<GraphicBuffer> GetFrameBuffer(DisplayType aDisplayType) override;

 private:
  bool Post(buffer_handle_t buf, int fence, DisplayType aDisplayType);

  void CreateFramebufferSurface(sp<ANativeWindow>& aNativeWindow,
                                sp<DisplaySurface>& aDisplaySurface,
                                uint32_t aWidth, uint32_t aHeight,
                                unsigned int format, DisplayUtils displayUtils,
                                bool enableDisplay);

  void CreateVirtualDisplaySurface(IGraphicBufferProducer* aSink,
                                   sp<ANativeWindow>& aNativeWindow,
                                   sp<DisplaySurface>& aDisplaySurface);

  int DoQueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType);

  HWC2::Error SetHwcPowerMode(bool enabled);

  std::unique_ptr<HWC2::Device> mHwc;
  framebuffer_device_t* mFBDevice = nullptr;
  NativeFramebufferDevice* mExtFBDevice = nullptr;
  HWC2::Layer* mlayer = nullptr;
  HWC2::Layer* mlayerBootAnim = nullptr;
  sp<DisplaySurface> mDispSurface;
  sp<ANativeWindow> mSTClient;
  sp<DisplaySurface> mExtDispSurface;
  sp<ANativeWindow> mExtSTClient;
  sp<DisplaySurface> mBootAnimDispSurface;
  sp<ANativeWindow> mBootAnimSTClient;
  sp<IPower> mPower;
  OnEnabledCallbackType mEnabledCallback = nullptr;
  bool mEnableHWCPower = false;
  // Initial value should sync with hal::GetScreenEnabled()
  bool mFBEnabled = true;
  // Initial value should sync with hal::GetExtScreenEnabled()
  bool mExtFBEnabled = true;
  android::Mutex mPrimaryScreenLock;
  HWC2::Display* mHwcDisplay = nullptr;
};

}  // namespace mozilla

#endif /* GONKDISPLAYP_H */
