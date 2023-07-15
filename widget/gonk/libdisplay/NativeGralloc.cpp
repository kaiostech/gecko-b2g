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

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <GrallocUsageConversion.h>
#include <hardware/fb.h>
#include <hardware/gralloc.h>
#include <hardware/gralloc1.h>
#include <hardware/hardware.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "NativeGralloc.h"

namespace mozilla {

/* static variables */
static int version = -1;
static hw_module_t* gralloc_hardware_module = NULL;
static framebuffer_device_t* framebuffer_device = NULL;
static gralloc_module_t* gralloc0_module;
static alloc_device_t* gralloc0_alloc;

static gralloc1_device_t* gralloc1_device = NULL;
static int gralloc1_release_implies_delete = 0;
static GRALLOC1_PFN_CREATE_DESCRIPTOR gralloc1_create_descriptor = NULL;
static GRALLOC1_PFN_DESTROY_DESCRIPTOR gralloc1_destroy_descriptor = NULL;
static GRALLOC1_PFN_SET_CONSUMER_USAGE gralloc1_set_consumer_usage = NULL;
static GRALLOC1_PFN_SET_DIMENSIONS gralloc1_set_dimensions = NULL;
static GRALLOC1_PFN_SET_FORMAT gralloc1_set_format = NULL;
static GRALLOC1_PFN_SET_PRODUCER_USAGE gralloc1_set_producer_usage = NULL;
static GRALLOC1_PFN_GET_BACKING_STORE gralloc1_get_backing_store = NULL;
static GRALLOC1_PFN_GET_CONSUMER_USAGE gralloc1_get_consumer_usage = NULL;
static GRALLOC1_PFN_GET_DIMENSIONS gralloc1_get_dimensions = NULL;
static GRALLOC1_PFN_GET_FORMAT gralloc1_get_format = NULL;
static GRALLOC1_PFN_GET_PRODUCER_USAGE gralloc1_get_producer_usage = NULL;
static GRALLOC1_PFN_GET_STRIDE gralloc1_get_stride = NULL;
static GRALLOC1_PFN_ALLOCATE gralloc1_allocate = NULL;
static GRALLOC1_PFN_RETAIN gralloc1_retain = NULL;
static GRALLOC1_PFN_RELEASE gralloc1_release = NULL;
static GRALLOC1_PFN_GET_NUM_FLEX_PLANES gralloc1_get_num_flex_planes = NULL;
static GRALLOC1_PFN_LOCK gralloc1_lock = NULL;
static GRALLOC1_PFN_LOCK_FLEX gralloc1_lock_flex = NULL;
static GRALLOC1_PFN_UNLOCK gralloc1_unlock = NULL;
static GRALLOC1_PFN_SET_LAYER_COUNT gralloc1_set_layer_count = NULL;
static GRALLOC1_PFN_GET_LAYER_COUNT gralloc1_get_layer_count = NULL;

static void gralloc1_init(void);

void native_gralloc_deinitialize(void);

void native_gralloc_initialize(int framebuffer) {
  if (version == -1) {
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const struct hw_module_t**)&gralloc_hardware_module) ==
        0) {
      if ((gralloc1_open(gralloc_hardware_module, &gralloc1_device) == 0) &&
          (gralloc1_device != NULL)) {
        // success
        ALOGI("%s,gralloc1_open success, l:%d", __func__, __LINE__);
        gralloc1_init();
        version = 1;
        atexit(native_gralloc_deinitialize);
      } else {
        if (framebuffer) {
	  int rv = framebuffer_open(gralloc_hardware_module, &framebuffer_device);
          if (rv == 0) {
            if ((gralloc_open(gralloc_hardware_module, &gralloc0_alloc) == 0) &&
                gralloc0_alloc != NULL) {
              // success
              version = 0;
              ALOGI("gralloc_open success.l:%d", __LINE__);
              gralloc0_module =
                  reinterpret_cast<gralloc_module_t*>(gralloc_hardware_module);
              atexit(native_gralloc_deinitialize);
            } else {
              ALOGI("failed to open the gralloc 0 module");
              assert(NULL);
            }
          } else {
            ALOGI("failed to open the framebuffer module ret=%d (%s)", rv, strerror(-rv));
            assert(NULL);
          }
        } else if ((gralloc_open(gralloc_hardware_module, &gralloc0_alloc) ==
                    0) &&
                   gralloc0_alloc != NULL) {
          // success
          version = 0;
          gralloc0_module =
              reinterpret_cast<gralloc_module_t*>(gralloc_hardware_module);
          ALOGI("gralloc_open . l:%d", __LINE__);
          atexit(native_gralloc_deinitialize);
        } else {
          // fail
          framebuffer_device = NULL;
          gralloc1_device = NULL;
          version = -2;
          ALOGI(
              "failed to open gralloc module with both version 0 and 1"
              " methods. l:%d",
              __LINE__);
          native_gralloc_deinitialize();
          assert(NULL);
        }
      }
    } else {
      ALOGI("failed to find/load gralloc module. l:%d", __LINE__);
      assert(NULL);
    }
  } else {
    // shouldn't reach here.
    assert(NULL);
  }
}

void native_gralloc_deinitialize(void) {
  ALOGI("%s,deinitialize l:%d", __func__, __LINE__);
  if (framebuffer_device) framebuffer_close(framebuffer_device);
  framebuffer_device = NULL;

  if (gralloc0_alloc) gralloc_close(gralloc0_alloc);
  gralloc0_alloc = NULL;

  if (gralloc1_device) gralloc1_close(gralloc1_device);
  gralloc1_device = NULL;

  if (gralloc_hardware_module) dlclose(gralloc_hardware_module->dso);
  gralloc_hardware_module = NULL;
}

static void gralloc1_init(void) {
  uint32_t count = 0;
  gralloc1_device->getCapabilities(gralloc1_device, &count, NULL);

  if (count >= 1) {
    uint32_t i;
    int32_t* gralloc1_capabilities = (int32_t*)malloc(sizeof(int32_t) * count);

    gralloc1_device->getCapabilities(gralloc1_device, &count,
                                     gralloc1_capabilities);

    // currently the only one that affects us/interests us is release imply
    // delete.
    for (i = 0; i < count; i++) {
      if (gralloc1_capabilities[i] ==
          GRALLOC1_CAPABILITY_RELEASE_IMPLY_DELETE) {
        gralloc1_release_implies_delete = 1;
      }
    }

    free(gralloc1_capabilities);
  }

  gralloc1_create_descriptor =
      (GRALLOC1_PFN_CREATE_DESCRIPTOR)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR);
  gralloc1_destroy_descriptor =
      (GRALLOC1_PFN_DESTROY_DESCRIPTOR)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR);
  gralloc1_set_consumer_usage =
      (GRALLOC1_PFN_SET_CONSUMER_USAGE)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE);
  gralloc1_set_dimensions =
      (GRALLOC1_PFN_SET_DIMENSIONS)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_SET_DIMENSIONS);
  gralloc1_set_format = (GRALLOC1_PFN_SET_FORMAT)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_SET_FORMAT);
  gralloc1_set_producer_usage =
      (GRALLOC1_PFN_SET_PRODUCER_USAGE)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE);
  gralloc1_get_backing_store =
      (GRALLOC1_PFN_GET_BACKING_STORE)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_BACKING_STORE);
  gralloc1_get_consumer_usage =
      (GRALLOC1_PFN_GET_CONSUMER_USAGE)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_CONSUMER_USAGE);
  gralloc1_get_dimensions =
      (GRALLOC1_PFN_GET_DIMENSIONS)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_DIMENSIONS);
  gralloc1_get_format = (GRALLOC1_PFN_GET_FORMAT)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_GET_FORMAT);
  gralloc1_get_producer_usage =
      (GRALLOC1_PFN_GET_PRODUCER_USAGE)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_PRODUCER_USAGE);
  gralloc1_get_stride = (GRALLOC1_PFN_GET_STRIDE)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_GET_STRIDE);
  gralloc1_allocate = (GRALLOC1_PFN_ALLOCATE)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_ALLOCATE);
  gralloc1_retain = (GRALLOC1_PFN_RETAIN)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_RETAIN);
  gralloc1_release = (GRALLOC1_PFN_RELEASE)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_RELEASE);
  gralloc1_get_num_flex_planes =
      (GRALLOC1_PFN_GET_NUM_FLEX_PLANES)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES);
  gralloc1_lock = (GRALLOC1_PFN_LOCK)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_LOCK);
  gralloc1_lock_flex = (GRALLOC1_PFN_LOCK_FLEX)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_LOCK_FLEX);
  gralloc1_unlock = (GRALLOC1_PFN_UNLOCK)gralloc1_device->getFunction(
      gralloc1_device, GRALLOC1_FUNCTION_UNLOCK);
  gralloc1_set_layer_count =
      (GRALLOC1_PFN_SET_LAYER_COUNT)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_SET_LAYER_COUNT);
  gralloc1_get_layer_count =
      (GRALLOC1_PFN_GET_LAYER_COUNT)gralloc1_device->getFunction(
          gralloc1_device, GRALLOC1_FUNCTION_GET_LAYER_COUNT);
}

int native_gralloc_release(buffer_handle_t handle, int was_allocated) {
  int ret = -ENOSYS;

  if GRALLOC1 (ret = gralloc1_release(gralloc1_device, handle);

               // this needs to happen if the last reference is gone, this
               // function is only called in such cases.
               if (!gralloc1_release_implies_delete) {
                 native_handle_close((native_handle_t*)handle);
                 native_handle_delete((native_handle_t*)handle);
               })
    else if GRALLOC0 (
        if (was_allocated) {
          ret = gralloc0_alloc->free(gralloc0_alloc, handle);
        } else {
          ret = gralloc0_module->unregisterBuffer(gralloc0_module, handle);

          // this needs to happen if the last reference is gone, this function
          // is only called in such cases.
          native_handle_close((native_handle_t*)handle);
          native_handle_delete((native_handle_t*)handle);
        }) else NO_GRALLOC

        return ret;
}

int native_gralloc_retain(buffer_handle_t handle) {
  int ret = -ENOSYS;

  if GRALLOC1 (ret = gralloc1_retain(gralloc1_device, handle);)
    else if GRALLOC0 (ret = gralloc0_module->registerBuffer(
                          gralloc0_module, handle);) else NO_GRALLOC

        return ret;
}

int native_gralloc_allocate(int width, int height, int format, int usage,
                            buffer_handle_t* handle_ptr, uint32_t* stride_ptr) {
  int ret = -ENOSYS;

  if GRALLOC1 (gralloc1_buffer_descriptor_t desc; uint64_t producer_usage;
               uint64_t consumer_usage;

               android_convertGralloc0To1Usage(usage, &producer_usage,
                                               &consumer_usage);

               // create temporary description (descriptor) of buffer to
               // allocate
               ret = gralloc1_create_descriptor(gralloc1_device, &desc);
               ret |=
               gralloc1_set_dimensions(gralloc1_device, desc, width, height);
               ret |= gralloc1_set_consumer_usage(gralloc1_device, desc,
                                                  consumer_usage);
               ret |= gralloc1_set_producer_usage(gralloc1_device, desc,
                                                  producer_usage);
               ret |= gralloc1_set_format(gralloc1_device, desc, format);

               // actual allocation
               ret |= gralloc1_allocate(gralloc1_device, 1, &desc, handle_ptr);

               // get stride and release temporary descriptor
               ret |=
               gralloc1_get_stride(gralloc1_device, *handle_ptr, stride_ptr);
               ret |= gralloc1_destroy_descriptor(gralloc1_device, desc);)
    else if GRALLOC0 (ret = gralloc0_alloc->alloc(
                          gralloc0_alloc, width, height, format, usage,
                          handle_ptr, (int*)stride_ptr);) else NO_GRALLOC

        return ret;
}

int native_gralloc_lock(buffer_handle_t handle, int usage, int l, int t, int w,
                        int h, void** vaddr) {
  int ret = -ENOSYS;

  if GRALLOC1 (uint64_t producer_usage; uint64_t consumer_usage;
               gralloc1_rect_t access_region;

               access_region.left = l; access_region.top = t;
               access_region.width = w; access_region.height = h;

               android_convertGralloc0To1Usage(usage, &producer_usage,
                                               &consumer_usage);

               ret = gralloc1_lock(gralloc1_device, handle, producer_usage,
                                   consumer_usage, &access_region, vaddr, -1);)
    else if GRALLOC0 (ret = gralloc0_module->lock(gralloc0_module, handle,
                                                  usage, l, t, w, h,
                                                  vaddr);) else NO_GRALLOC

        return ret;
}

int native_gralloc_unlock(buffer_handle_t handle) {
  int ret = -ENOSYS;

  if GRALLOC1 (int releaseFence = 0;
               ret = gralloc1_unlock(gralloc1_device, handle, &releaseFence);
               close(releaseFence);)
    else if GRALLOC0 (ret = gralloc0_module->unlock(gralloc0_module,
                                                    handle);) else NO_GRALLOC

        return ret;
}

}  // namespace mozilla
