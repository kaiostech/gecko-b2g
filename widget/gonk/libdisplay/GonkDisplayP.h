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
#if ANDROID_VERSION >= 30
#  include <android/hardware/power/IPower.h>
#else
#  include <android/hardware/power/1.0/IPower.h>
#endif

#include "HwcHAL.h"
#include "DisplaySurface.h"
#include "GonkDisplay.h"
#include "NativeFramebufferDevice.h"
#include "NativeGralloc.h"

class HWComposerCallback;

namespace mozilla {

using android::DisplaySurface;
using android::DisplayUtils;
using android::GraphicBuffer;
using android::IGraphicBufferProducer;
using android::sp;

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

  std::shared_ptr<HWC2::Layer> CreateLayer(int32_t aWidth, int32_t aHeight);

  int DoQueueBuffer(ANativeWindowBuffer* buf, DisplayType aDisplayType);

  HWC2::Error SetHwcPowerMode(bool enabled);

  std::unique_ptr<HWC2::Device> mHwcDevice;
  std::unique_ptr<HWComposerCallback> mHwcCallback;
  framebuffer_device_t* mFBDevice = nullptr;
  NativeFramebufferDevice* mExtFBDevice = nullptr;
  std::shared_ptr<HWC2::Layer> mLayer;
  std::shared_ptr<HWC2::Layer> mBootAnimLayer;
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
