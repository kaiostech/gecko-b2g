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

#ifndef NATIVEGRALLOCT_H
#define NATIVEGRALLOCT_H

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

#ifdef MOZ_USE_GRALLOC3
#	include <android/hardware/graphics/mapper/3.0/IMapper.h>
#	include <utils/StrongPointer.h>
#endif
#	include <android/hardware/graphics/mapper/4.0/IMapper.h>
#	include <utils/StrongPointer.h>

namespace mozilla {

class NativeGralloc
{
public:
  inline ~NativeGralloc(){};
	static NativeGralloc &getInstance();

	int import(buffer_handle_t handle, buffer_handle_t *imported_handle);
	int release(buffer_handle_t handle);

	int lock(buffer_handle_t handle, int usage, int left, int top, int width, int height, void **vaddr);
	int unlock(buffer_handle_t handle);

private:
	NativeGralloc();
	uint8_t gralloc_version;
	const gralloc_module_t *gralloc0_module;
#ifdef MOZ_USE_GRALLOC1
	gralloc1_device_t *gralloc1_device = nullptr;
	GRALLOC1_PFN_LOCK gralloc1_lock = nullptr;
	GRALLOC1_PFN_UNLOCK gralloc1_unlock = nullptr;
#endif
#ifdef MOZ_USE_GRALLOC3
	android::sp<android::hardware::graphics::mapper::V3_0::IMapper> gralloc3_mapper;
#endif
	android::sp<android::hardware::graphics::mapper::V4_0::IMapper> gralloc4_mapper;
};

}

#endif  // NATIVEGRALLOCT_H
