/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright (C) 2015 Acadine Technologies. All rights reserved.
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

#ifndef NATIVEFRAMEBUFFERDEVICE_H
#define NATIVEFRAMEBUFFERDEVICE_H

#include <hardware/gralloc.h>
#include <linux/fb.h>
#include <system/window.h>
#include <utils/Mutex.h>
#if ANDROID_VERSION >= 34
#  include "NativeDrmDevice.h"
#endif

// ----------------------------------------------------------------------------
namespace mozilla {
// ----------------------------------------------------------------------------

using android::sp;

class NativeFramebufferDevice {
 public:
  ~NativeFramebufferDevice();

  static NativeFramebufferDevice* Create();

  bool Open();

#if ANDROID_VERSION >= 34
  bool Post(const sp<GraphicBuffer>& buffer);
#endif

  bool Post(buffer_handle_t buf);

  bool EnableScreen(int enabled);

  bool IsValid();

  // Only be valid after open sucessfully
  uint32_t mWidth;
  uint32_t mHeight;
  int32_t mSurfaceformat;
  float mXdpi;

 private:
  NativeFramebufferDevice(int aExtFbFd);
  bool Close();
  void DrawSolidColorFrame();

  bool mIsEnabled;
  int mFd;
  void* mMappedAddr;
  uint32_t mMemLength;
  struct fb_var_screeninfo mVInfo;
  struct fb_fix_screeninfo mFInfo;
  gralloc_module_t* mGrmodule;
  int32_t mFBSurfaceformat;

  // Locks against both mFd and mIsEnable.
  mutable android::Mutex mMutex;
#if ANDROID_VERSION >= 34
  static NativeDrmDevice* mDrmDev;
#endif
};

// ----------------------------------------------------------------------------
}  // namespace mozilla
// ----------------------------------------------------------------------------

#endif /* NATIVEFRAMEBUFFERDEVICE_H */
