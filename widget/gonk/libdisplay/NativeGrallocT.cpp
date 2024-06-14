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
#include <hardware/gralloc.h>
#include <hardware/gralloc1.h>
#include <hardware/hardware.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "NativeGrallocT.h"

namespace mozilla {

#ifdef MOZ_USE_GRALLOC1
#	include <sync/sync.h>
#endif
#ifdef MOZ_USE_GRALLOC3
using V3Error = android::hardware::graphics::mapper::V3_0::Error;
using V3Mapper = android::hardware::graphics::mapper::V3_0::IMapper;
using android::hardware::hidl_handle;
#endif
using V4Error = android::hardware::graphics::mapper::V4_0::Error;
using V4Mapper = android::hardware::graphics::mapper::V4_0::IMapper;
using android::hardware::hidl_handle;

NativeGralloc &NativeGralloc::getInstance()
{
	static NativeGralloc sInstance;
	return sInstance;
}

NativeGralloc::NativeGralloc()
{
	gralloc4_mapper = V4Mapper::getService();
	if(gralloc4_mapper != nullptr)
	{
    ALOGI("%s using Gralloc4.", __FUNCTION__);
		return;
	} else {
    ALOGI("%s no Gralloc4 availble.", __FUNCTION__);
        }

#ifdef MOZ_USE_GRALLOC3
	gralloc3_mapper = V3Mapper::getService();
	if(gralloc3_mapper != nullptr)
	{
    ALOGI("%s using Gralloc3.", __FUNCTION__);
		return;
	}
#endif

	const hw_module_t *module = nullptr;
	hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);

	gralloc_version = (module->module_api_version >> 8) & 0xff;
	switch(gralloc_version)
	{
	case 0:
    ALOGI("%s using Gralloc0.", __FUNCTION__);
		gralloc0_module = reinterpret_cast<const gralloc_module_t *>(module);
		break;
	case 1:
#ifdef MOZ_USE_GRALLOC1
    ALOGI("%s using Gralloc1.", __FUNCTION__);
		gralloc1_open(module, &gralloc1_device);
		gralloc1_lock = (GRALLOC1_PFN_LOCK)gralloc1_device->getFunction(gralloc1_device, GRALLOC1_FUNCTION_LOCK);
		gralloc1_unlock = (GRALLOC1_PFN_UNLOCK)gralloc1_device->getFunction(gralloc1_device, GRALLOC1_FUNCTION_UNLOCK);
		break;
#endif
	default:
		ALOGE("unknown gralloc major version (%d)", gralloc_version);
		break;
	}
}

int NativeGralloc::import(buffer_handle_t handle, buffer_handle_t *imported_handle)
{
	if(gralloc4_mapper != nullptr)
	{
		V4Error error;
    ALOGI("use gralloc4 module to import");
		auto ret = gralloc4_mapper->importBuffer(handle,
		                                           [&](const auto &tmp_err, const auto &tmp_buf) {
			                                           error = tmp_err;
			                                           if(error == V4Error::NONE)
			                                           {
				                                           *imported_handle = static_cast<buffer_handle_t>(tmp_buf);
			                                           }
		                                           });
		return ret.isOk() && error == V4Error::NONE ? 0 : -1;
	}

#ifdef MOZ_USE_GRALLOC3
	if(gralloc3_mapper != nullptr)
	{
		V3Error error;
    ALOGI("use gralloc3 module to import");
		auto ret = gralloc3_mapper->importBuffer(handle,
		                                           [&](const auto &tmp_err, const auto &tmp_buf) {
			                                           error = tmp_err;
			                                           if(error == V3Error::NONE)
			                                           {
				                                           *imported_handle = static_cast<buffer_handle_t>(tmp_buf);
			                                           }
		                                           });
		return ret.isOk() && error == V3Error::NONE ? 0 : -1;
	}
#endif

	*imported_handle = handle;
	return 0;
}

int NativeGralloc::release(buffer_handle_t handle)
{
	if(gralloc4_mapper != nullptr)
	{
    ALOGI("use gralloc4 module to release");
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);
		return gralloc4_mapper->freeBuffer(native_handle).isOk() ? 0 : 1;
	}

#ifdef MOZ_USE_GRALLOC3
	if(gralloc3_mapper != nullptr)
	{
    ALOGI("use gralloc3 module to release");
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);
		return gralloc3_mapper->freeBuffer(native_handle).isOk() ? 0 : 1;
	}
#endif

	return 0;
}

int NativeGralloc::lock(buffer_handle_t handle, int usage, int left, int top, int width, int height, void **vaddr)
{
	if(gralloc4_mapper != nullptr)
	{
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

		V4Mapper::Rect rect;
		rect.left = left;
		rect.top = top;
		rect.width = width;
		rect.height = height;

		hidl_handle empty_fence_handle;

		V4Error error;
    ALOGI("use gralloc4 module to lock");
		auto ret = gralloc4_mapper->lock(native_handle, usage, rect, empty_fence_handle,
		                                   [&](const auto &tmp_err, const auto &tmp_vaddr) {
			                                   error = tmp_err;
			                                   if(tmp_err == V4Error::NONE)
			                                   {
				                                   *vaddr = tmp_vaddr;
			                                   }
		                                   });
		return ret.isOk() && error == V4Error::NONE ? 0 : -1;
	}

#ifdef MOZ_USE_GRALLOC3
	if(gralloc3_mapper != nullptr)
	{
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

		V3Mapper::Rect rect;
		rect.left = left;
		rect.top = top;
		rect.width = width;
		rect.height = height;

		hidl_handle empty_fence_handle;

		V3Error error;
    ALOGI("use gralloc3 module to lock");
		auto ret = gralloc3_mapper->lock(native_handle, usage, rect, empty_fence_handle,
		                                   [&](const auto &tmp_err,
		                                       const auto &tmp_vaddr,
		                                       const auto & /*bytes_per_pixel*/,
		                                       const auto & /*bytes_per_stride*/) {
			                                   error = tmp_err;
			                                   if(tmp_err == V3Error::NONE)
			                                   {
				                                   *vaddr = tmp_vaddr;
			                                   }
		                                   });
		return ret.isOk() && error == V3Error::NONE ? 0 : -1;
	}
#endif

	switch(gralloc_version)
	{
	case 0:
		{
      ALOGI("use gralloc0 module to lock");
			return gralloc0_module->lock(gralloc0_module, handle, usage, left, top, width, height, vaddr);
		}
	case 1:
#ifdef MOZ_USE_GRALLOC1
		{
			gralloc1_rect_t outRect{};
			outRect.left = left;
			outRect.top = top;
			outRect.width = width;
			outRect.height = height;
      ALOGI("use gralloc1 module to lock");
			return gralloc1_lock(gralloc1_device, handle, usage, usage, &outRect, vaddr, -1);
		}
#endif
	default:
		{
			ALOGE("no gralloc module to lock");
			return -1;
		}
	}
}

int NativeGralloc::unlock(buffer_handle_t handle)
{
	if(gralloc4_mapper != nullptr)
	{
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

		V4Error error;
    ALOGI("use gralloc4 module to unlock");
		auto ret = gralloc4_mapper->unlock(native_handle,
		                                     [&](const auto &tmp_err, const auto &) {
			                                     error = tmp_err;
		                                     });
		return ret.isOk() && error == V4Error::NONE ? 0 : -1;
	}

#ifdef MOZ_USE_GRALLOC3
	if(gralloc3_mapper != nullptr)
	{
		native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

		V3Error error;
    ALOGI("use gralloc3 module to unlock");
		auto ret = gralloc3_mapper->unlock(native_handle,
		                                     [&](const auto &tmp_err, const auto &) {
			                                     error = tmp_err;
		                                     });
		return ret.isOk() && error == V3Error::NONE ? 0 : -1;
	}
#endif

	switch(gralloc_version)
	{
	case 0:
		{
      ALOGI("use gralloc0 module to unlock");
			return gralloc0_module->unlock(gralloc0_module, handle);
		}
	case 1:
#ifdef MOZ_USE_GRALLOC1
		{
			int32_t fenceFd = -1;
      ALOGI("use gralloc1 module to unlock");
			int error = gralloc1_unlock(gralloc1_device, handle, &fenceFd);
			if(!error)
			{
				sync_wait(fenceFd, -1);
				close(fenceFd);
			}
			return error;
		}
#endif
	default:
		{
			ALOGE("no gralloc module to unlock");
			return -1;
		}
	}
}
} // mozilla
