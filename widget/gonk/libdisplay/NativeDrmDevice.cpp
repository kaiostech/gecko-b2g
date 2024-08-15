/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright (C) 2015 Acadine Technologies. All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2010-2014 The Linux Foundation. All rights reserved.
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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "NativeGralloc.h"
#include "cutils/properties.h"
#include "utils/Log.h"

#include "NativeDrmDevice.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cstdint>

#include <ui/Rect.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

#ifdef LOG_TAG
#  undef LOG_TAG
#  define LOG_TAG "NativeDrmDevice"
#endif

#define PANEL_COLOR_FORMAT DRM_FORMAT_RGB565
#define RGB_RED 0xFF0000
#define RGB_GREEN 0x008000
#define RGB_BLUE 0x0000FF
#define RGB_YELLOW 0xFFFF00

#define RGB565_RED 0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE 0x001F
#define RGB565_YELLOW 0xFFE0

#define drm_munmap(addr, length) munmap(addr, length)

#ifdef BUILD_ARM_NEON
#  include "rgb8888_to_rgb565_neon.h"
#endif

#define DEFAULT_XDPI 75.0

// ----------------------------------------------------------------------------
namespace mozilla {
// ----------------------------------------------------------------------------

using android::GraphicBufferMapper;
using android::Rect;

inline void Transform8888To565_Software(uint8_t* outbuf, const uint8_t* inbuf,
                                        int PixelNum) {
  uint32_t bytes = PixelNum * 4;
  uint16_t* out = (uint16_t*)outbuf;
  uint8_t* in = (uint8_t*)inbuf;
  for (uint32_t i = 0; i < bytes; i += 4) {
    *out++ =
        ((in[i] & 0xF8) << 8) | ((in[i + 1] & 0xFC) << 3) | ((in[i + 2]) >> 3);
  }
}

inline void Transform8888To565(uint8_t* outbuf, const uint8_t* inbuf,
                               int PixelNum) {
#ifdef BUILD_ARM_NEON
  Transform8888To565_NEON(outbuf, inbuf, PixelNum);
#else
  Transform8888To565_Software(outbuf, inbuf, PixelNum);
#endif
}

inline void* NativeDrmDevice::drm_mmap(void* addr, size_t length, int prot,
                                       int flags, int fd, loff_t offset) {
  /* offset must be aligned to 4096 (not necessarily the page size) */
  if (offset & 4095) {
    errno = EINVAL;
    return MAP_FAILED;
  }

  return mmap64(addr, length, prot, flags, fd, offset);
}

int NativeDrmDevice::drm_create_fb(struct drm_device* bo, uint32_t fourcc) {
  uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
  int ret = 0;
  /* create a dumb-buffer, the pixel format is XRGB888 */
  bo->create.width = bo->width;
  bo->create.height = bo->height;

  if (fourcc == DRM_FORMAT_XRGB8888)
    bo->create.bpp = 32;
  else if (fourcc == DRM_FORMAT_RGB565)
    bo->create.bpp = 16;
  else {
    ALOGD("not support formate\n");
    return -1;
  }

  /* handle, pitch, size will be returned */
  drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &bo->create);

  /* bind the dumb-buffer to an FB object */
  bo->pitch = bo->create.pitch;
  bo->size = bo->create.size;
  bo->handle = bo->create.handle;
  handles[0] = bo->handle;
  pitches[0] = bo->pitch;
  offsets[0] = 0;

  if (drmModeAddFB2(fd, bo->width, bo->height, fourcc, handles, pitches,
                    offsets, &bo->fb_id, 0)) {
    ALOGE("failed to add fb (%ux%u): %s\n", bo->width, bo->height,
          strerror(errno));
    return -1;
  }

  ALOGD("pitch = %d ,size = %d, handle = %d, width %d, height %d \n", bo->pitch,
        bo->size, bo->handle, bo->width, bo->height);
  // pitch = 256 ,size = 40960, handle = 1, width 128, height 160

  /* map the dumb-buffer to userspace */
  bo->map.handle = bo->create.handle;
  ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &bo->map);
  if (ret) return ret;

  bo->vaddr = drm_mmap(0, bo->create.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                       fd, bo->map.offset);
  if (bo->vaddr == MAP_FAILED) return -EINVAL;

  /* initialize the dumb-buffer with white-color */
  memset(bo->vaddr, 0x0, bo->size);

  return 0;
}

void NativeDrmDevice::drm_destroy_fb(struct drm_device* bo) {
  struct drm_mode_destroy_dumb destroy = {};
  if (bo->fb_id) drmModeRmFB(fd, bo->fb_id);
  if (bo->vaddr) munmap(bo->vaddr, bo->size);
  if (bo->handle) {
    destroy.handle = bo->handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
  }
}

uint32_t NativeDrmDevice::get_property(int fd, drmModeObjectProperties* props) {
  drmModePropertyPtr property;
  uint32_t i, id = 0;

  for (i = 0; i < props->count_props; i++) {
    property = drmModeGetProperty(fd, props->props[i]);
    ALOGD("\"%s\"\t\t---", property->name);
    ALOGD("id = %d , value=%ld\n", props->props[i], props->prop_values[i]);
    drmModeFreeProperty(property);
  }
  return 0;
}

uint32_t NativeDrmDevice::get_property_id(int fd,
                                          drmModeObjectProperties* props,
                                          const char* name) {
  drmModePropertyPtr property;
  uint32_t i, id = 0;

  /* find property according to the name */
  for (i = 0; i < props->count_props; i++) {
    property = drmModeGetProperty(fd, props->props[i]);
    if (!strcmp(property->name, name)) id = property->prop_id;
    drmModeFreeProperty(property);
    if (id) break;
  }

  return id;
}

int NativeDrmDevice::drm_init(int drmFd) {
  int i;
  int ret = -1;
  uint64_t cap = 0;

  drmModeObjectProperties* props;
  drmModeAtomicReq* req;
  fd = drmFd;

  ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ALOGE("no atomic modesetting support: %s\n", strerror(errno));
    return -1;
  }

  ret = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap);
  if (ret || cap == 0) {
    ALOGE("driver doesn't support the dumb buffer API\n");
    return -1;
  }

  drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  res = drmModeGetResources(fd);
  crtc_id = res->crtcs[0];
  conn_id = res->connectors[0];

  plane_res = drmModeGetPlaneResources(fd);

  plane_id = plane_res->planes[0];
  ALOGV("planes = %d, crtc_id %d, conn_id %d\n", plane_id, crtc_id, conn_id);

  conn = drmModeGetConnector(fd, conn_id);
  buf.width = conn->modes[0].hdisplay;
  buf.height = conn->modes[0].vdisplay;
  ALOGV("create fb, buf.width = %u, buf.width = %u\n", buf.width,
        buf.height);  // create fb, buf.width = 128, buf.width = 160
  ret = drm_create_fb(&buf, PANEL_COLOR_FORMAT);
  if (ret) {
    ALOGE("create fb failed\n");
    return ret;
  }
  /* get connector properties */
  props = drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
  ALOGV("/-----conn_Property-----/\n");
  get_property(fd, props);
  ALOGV("\n");
  pc.property_crtc_id = get_property_id(fd, props, "CRTC_ID");
  drmModeFreeObjectProperties(props);

  /* get crtc properties */
  props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
  ALOGV("/-----CRTC_Property-----/\n");
  get_property(fd, props);
  ALOGV("\n");
  pc.property_active = get_property_id(fd, props, "ACTIVE");
  pc.property_mode_id = get_property_id(fd, props, "MODE_ID");
  drmModeFreeObjectProperties(props);

  props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
  ALOGV("/-----PLANE_Property-----/\n");
  get_property(fd, props);
  ALOGV("\n");
  pc.property_fb_id = get_property_id(fd, props, "FB_ID");
  pc.property_crtc_x = get_property_id(fd, props, "CRTC_X");
  pc.property_crtc_y = get_property_id(fd, props, "CRTC_Y");
  pc.property_crtc_w = get_property_id(fd, props, "CRTC_W");
  pc.property_crtc_h = get_property_id(fd, props, "CRTC_H");
  pc.property_src_x = get_property_id(fd, props, "SRC_X");
  pc.property_src_y = get_property_id(fd, props, "SRC_Y");
  pc.property_src_w = get_property_id(fd, props, "SRC_W");
  pc.property_src_h = get_property_id(fd, props, "SRC_H");
  drmModeFreeObjectProperties(props);

  /* create blob to store current mode, and retun the blob id */
  drmModeCreatePropertyBlob(fd, &conn->modes[0], sizeof(conn->modes[0]),
                            &pc.blob_id);

  /* start modeseting */
  req = drmModeAtomicAlloc();
  drmModeAtomicAddProperty(req, crtc_id, pc.property_active, 1);
  drmModeAtomicAddProperty(req, crtc_id, pc.property_mode_id, pc.blob_id);
  drmModeAtomicAddProperty(req, conn_id, pc.property_crtc_id, crtc_id);

  drmModeAtomicAddProperty(req, plane_id, pc.property_fb_id, buf.fb_id);
  drmModeAtomicAddProperty(req, plane_id, pc.property_crtc_id, crtc_id);
  drmModeAtomicAddProperty(req, plane_id, pc.property_src_x, 0);
  drmModeAtomicAddProperty(req, plane_id, pc.property_src_y, 0);
  drmModeAtomicAddProperty(req, plane_id, pc.property_src_w, buf.width << 16);
  drmModeAtomicAddProperty(req, plane_id, pc.property_src_h, buf.height << 16);
  drmModeAtomicAddProperty(req, plane_id, pc.property_crtc_x, 0);
  drmModeAtomicAddProperty(req, plane_id, pc.property_crtc_y, 0);
  drmModeAtomicAddProperty(req, plane_id, pc.property_crtc_w, buf.width);
  drmModeAtomicAddProperty(req, plane_id, pc.property_crtc_h, buf.height);

  ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
  if (ret) {
    ALOGE("Atomic Commit failed\n");
    drmModeAtomicFree(req);
    close(fd);
    return -1;
  }
  drmModeAtomicFree(req);
  return 0;
}

int NativeDrmDevice::drm_exit() {
  drm_destroy_fb(&buf);
  if (conn) drmModeFreeConnector(conn);
  if (plane_res) drmModeFreePlaneResources(plane_res);
  if (res) drmModeFreeResources(res);
  if (fd > 0) close(fd);
  return 0;
}

NativeDrmDevice::NativeDrmDevice(int drmFd)
    : mWidth(128),
      mHeight(160),
      mSurfaceformat(HAL_PIXEL_FORMAT_RGB_565),
      mXdpi(DEFAULT_XDPI),
      mIsEnabled(false),
      mMappedAddr(nullptr),
      mMemLength(0) {
  drm_init(drmFd);
}

NativeDrmDevice::~NativeDrmDevice() { Close(); }

NativeDrmDevice* NativeDrmDevice::Create() {
  char const* const device_template[] = {"/dev/dri/card1", 0};

  int i = 0;
  int drmFd = -1;

  while ((drmFd == -1) && device_template[i]) {
    drmFd = open(device_template[i], O_RDWR, 0);
    i++;
  }

  if (drmFd < 0) {
    i = 0;
    while (device_template[i]) {
      ALOGE("Failed to open external framebuffer device %s",
            device_template[i]);
      i++;
    }
    return nullptr;
  }

  return new NativeDrmDevice(drmFd);
}

bool NativeDrmDevice::Open() {
  mWidth = buf.width;
  mHeight = buf.height;

  int pWidth = (int)((mWidth * 25.4) / 160.0f + 0.5);
  float xdpi = (mWidth * 25.4) / mWidth;
  mXdpi = xdpi;
  mFBSurfaceformat = HAL_PIXEL_FORMAT_RGB_565;
  mSurfaceformat = mFBSurfaceformat;
  mIsEnabled = true;
  ALOGV(
      "in NativeDrmDevice Open method, mWidth = %d, mHight = %d, "
      "property_crtc_w = %d, property_crtc_h = %d, mXdpi = %f",
      mWidth, mHeight, pc.property_crtc_w, pc.property_crtc_h, mXdpi);

  return true;
}

bool NativeDrmDevice::Post(const sp<GraphicBuffer>& buffer) {
  void* vaddr = NULL;
  if (buffer->lock(GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN |
                       GRALLOC_USAGE_HW_FB,
                   &vaddr) != android::OK) {
    ALOGE("Failed to lock GraphicBuffer");
    Close();
    return false;
  }

  if (mFBSurfaceformat == HAL_PIXEL_FORMAT_RGB_565 &&
      mSurfaceformat == HAL_PIXEL_FORMAT_RGBA_8888) {
    Transform8888To565((uint8_t*)buf.vaddr, (uint8_t*)vaddr,
                       buf.width * buf.height);
  } else {
    memcpy((uint8_t*)buf.vaddr, vaddr, buf.pitch * buf.height);
  }

  drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0, 0, 0, buf.width,
                  buf.height, 0 << 16, 0 << 16, buf.width << 16,
                  buf.height << 16);

  buffer->unlock();
  return true;
}

bool NativeDrmDevice::Post(buffer_handle_t buff) { return true; }

bool NativeDrmDevice::Close() {
  android::Mutex::Autolock lock(mMutex);

  if (buf.vaddr) {
    munmap(buf.vaddr, buf.create.size);
    buf.vaddr = 0;
    buf.create.size = 0;
  }

  if (fd != -1) {
    close(fd);
    fd = -1;
  }

  return true;
}

bool NativeDrmDevice::EnableScreen(int enabled) {
  bool ret_val = false;
  drmModeAtomicReq* req = drmModeAtomicAlloc();
  if (!req) {
    return false;
  }
  int pro_val = enabled > 0 ? 1 : 0;
  drmModeAtomicAddProperty(req, crtc_id, pc.property_active,
                           pro_val);  // 1 enable, 0 disble
  int ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
  drmModeAtomicFree(req);

  return ret == 0;
}

void NativeDrmDevice::DrawSolidColorFrame() {
  if ((fd < 0) || !(buf.vaddr)) {
    return;
  }

  memset((uint8_t*)buf.vaddr, 0, buf.pitch * buf.height);
  drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0, 0, 0, buf.width,
                  buf.height, 0 << 16, 0 << 16, buf.width << 16,
                  buf.height << 16);
  return;
}

// ----------------------------------------------------------------------------
}  // namespace mozilla
// ----------------------------------------------------------------------------
