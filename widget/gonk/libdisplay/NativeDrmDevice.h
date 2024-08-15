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

#ifndef NATIVEDRMDEVICE_H
#define NATIVEDRMDEVICE_H

#include <hardware/gralloc.h>
#include <linux/fb.h>
#include <system/window.h>
#include <utils/Mutex.h>

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <ui/GraphicBuffer.h>

// ----------------------------------------------------------------------------
namespace mozilla {
// ----------------------------------------------------------------------------

using android::GraphicBuffer;
using android::sp;

class NativeDrmDevice {
 public:
  ~NativeDrmDevice();

  static NativeDrmDevice* Create();

  bool Open();
  bool Post(const sp<GraphicBuffer>& buffer);
  bool Post(buffer_handle_t buf);

  bool EnableScreen(int enabled);

  bool IsValid();

  // Only be valid after open sucessfully
  uint32_t mWidth;
  uint32_t mHeight;
  int32_t mSurfaceformat;
  float mXdpi;

  struct drm_device {
    uint32_t width;   // display width in pixel
    uint32_t height;  // display height in pixel
    uint32_t pitch;   // bytes per line
    uint32_t handle;  // return handle by drm_mode_create_dumb
    uint32_t size;    // total byte of the display buffer
    void* vaddr;      // mmap start addr
    uint32_t fb_id;   // framebuffer id created
    struct drm_mode_create_dumb create;  // dumb created
    struct drm_mode_map_dumb map;        // memmap struct
  };
  struct property_crtc {
    uint32_t blob_id;
    uint32_t property_crtc_id;
    uint32_t property_mode_id;
    uint32_t property_active;

    /*plan prop*/
    uint32_t property_fb_id;
    uint32_t property_crtc_w, property_crtc_h;
    uint32_t property_crtc_x, property_crtc_y;
    uint32_t property_src_w, property_src_h;
    uint32_t property_src_x, property_src_y;
  };

 public:
 private:
  inline void* drm_mmap(void* addr, size_t length, int prot, int flags, int fd,
                        loff_t offset);
  int drm_create_fb(struct drm_device* bo, uint32_t fourcc);
  void drm_destroy_fb(struct drm_device* bo);
  uint32_t get_property(int fd, drmModeObjectProperties* props);
  uint32_t get_property_id(int fd, drmModeObjectProperties* props,
                           const char* name);
  int drm_init(int drmFd);
  int drm_exit();
  NativeDrmDevice(int aExtDrmFd);

 private:
  drmModeConnector* conn;
  drmModeRes* res;
  drmModePlaneRes* plane_res;

  int fd;
  uint32_t conn_id;
  uint32_t crtc_id;
  uint32_t plane_id;
  struct drm_device buf;
  struct property_crtc pc;

 public:
  bool Close();
  void DrawSolidColorFrame();

 private:
  bool mIsEnabled;
  void* mMappedAddr;
  uint32_t mMemLength;
  int32_t mFBSurfaceformat;

  // Locks against both mFd and mIsEnable.
  mutable android::Mutex mMutex;
};

// ----------------------------------------------------------------------------
}  // namespace mozilla
// ----------------------------------------------------------------------------

#endif /* NATIVEDRMDEVICE_H */
