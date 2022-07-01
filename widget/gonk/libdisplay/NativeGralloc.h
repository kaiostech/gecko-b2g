/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NATIVEGRALLOC_H
#define NATIVEGRALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

// for usage definitions and so on
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <hardware/gralloc.h>
#include <hardware/gralloc1.h>
#include <mozilla/Types.h>

#ifdef LOG_TAG
#  undef LOG_TAG
#  define LOG_TAG "native-gralloc"
#endif

#define GRALLOC0(code) \
  (version == 0) { code }
#define GRALLOC1(code) \
  (version == 1) { code }

#define NO_GRALLOC                                                           \
  {                                                                          \
    ALOGE("%s:%d: called gralloc method without gralloc loaded\n", __func__, \
          __LINE__);                                                         \
    assert(NULL);                                                            \
  }
namespace mozilla {

void native_gralloc_deinitialize(void);

void native_gralloc_initialize(int framebuffer);

void native_gralloc_deinitialize(void);

int native_gralloc_release(buffer_handle_t handle, int was_allocated);

int native_gralloc_retain(buffer_handle_t handle);

int native_gralloc_allocate(int width, int height, int format, int usage,
                            buffer_handle_t* handle, uint32_t* stride);

extern "C" MOZ_EXPORT __attribute__((weak)) int native_gralloc_lock(
    buffer_handle_t handle, int usage, int l, int t, int w, int h,
    void** vaddr);

extern "C" MOZ_EXPORT __attribute__((weak)) int native_gralloc_unlock(
    buffer_handle_t handle);

int native_gralloc_fbdev_format(void);

int native_gralloc_fbdev_framebuffer_count(void);

int native_gralloc_fbdev_setSwapInterval(int interval);

int native_gralloc_fbdev_post(buffer_handle_t handle);

int native_gralloc_fbdev_width(void);

int native_gralloc_fbdev_height(void);

}  // namespace mozilla

#ifdef __cplusplus
};
#endif

#endif /* NATIVEGRALLOC_H */
