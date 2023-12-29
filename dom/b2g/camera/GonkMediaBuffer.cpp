/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "GonkMediaBuffer"
#include <utils/Log.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include "GonkMediaBuffer.h"

namespace android {

/* static */
std::atomic_int_least32_t GonkMediaBuffer::mUseSharedMemory(0);

GonkMediaBuffer::GonkMediaBuffer(void* data, size_t size)
    : mObserver(NULL),
      mRefCount(0),
      mData(data),
      mSize(size),
      mRangeOffset(0),
      mRangeLength(size),
      mOwnsData(false),
      mMetaData(new MetaDataBase) {}

GonkMediaBuffer::GonkMediaBuffer(size_t size)
    : mObserver(NULL),
      mRefCount(0),
      mData(NULL),
      mSize(size),
      mRangeOffset(0),
      mRangeLength(size),
      mOwnsData(true),
      mMetaData(new MetaDataBase) {
#ifndef NO_IMEMORY
  if (size < kSharedMemThreshold ||
      std::atomic_load_explicit(&mUseSharedMemory, std::memory_order_seq_cst) ==
          0) {
#endif
    mData = malloc(size);
#ifndef NO_IMEMORY
  } else {
    ALOGV("creating memoryDealer");
    size_t newSize = 0;
    if (!__builtin_add_overflow(size, sizeof(SharedControl), &newSize)) {
      sp<MemoryDealer> memoryDealer =
          new MemoryDealer(newSize, "GonkMediaBuffer");
      mMemory = memoryDealer->allocate(newSize);
    }
    if (mMemory == NULL) {
      ALOGW("Failed to allocate shared memory, trying regular allocation!");
      mData = malloc(size);
      if (mData == NULL) {
        ALOGE("Out of memory");
      }
    } else {
      getSharedControl()->clear();
      mData = (uint8_t*)IMEMORY_POINTER(mMemory) + sizeof(SharedControl);
      ALOGV("Allocated shared mem buffer of size %zu @ %p", size, mData);
    }
  }
#endif
}

GonkMediaBuffer::GonkMediaBuffer(const sp<ABuffer>& buffer)
    : mObserver(NULL),
      mRefCount(0),
      mData(buffer->data()),
      mSize(buffer->size()),
      mRangeOffset(0),
      mRangeLength(mSize),
      mBuffer(buffer),
      mOwnsData(false),
      mMetaData(new MetaDataBase) {}

void GonkMediaBuffer::release() {
  if (mObserver == NULL) {
    // Legacy contract for GonkMediaBuffer without a MediaBufferGroup.
    CHECK_EQ(mRefCount, 0);
    delete this;
    return;
  }

  int prevCount = mRefCount.fetch_sub(1);
  if (prevCount == 1) {
    if (mObserver == NULL) {
      delete this;
      return;
    }

    mObserver->signalBufferReturned(this);
  }
  CHECK(prevCount > 0);
}

void GonkMediaBuffer::claim() {
  CHECK(mObserver != NULL);
  CHECK_EQ(mRefCount.load(std::memory_order_relaxed), 1);

  mRefCount.store(0, std::memory_order_relaxed);
}

void GonkMediaBuffer::add_ref() { (void)mRefCount.fetch_add(1); }

void* GonkMediaBuffer::data() const { return mData; }

size_t GonkMediaBuffer::size() const { return mSize; }

size_t GonkMediaBuffer::range_offset() const { return mRangeOffset; }

size_t GonkMediaBuffer::range_length() const { return mRangeLength; }

void GonkMediaBuffer::set_range(size_t offset, size_t length) {
  if (offset + length > mSize) {
    ALOGE("offset = %zu, length = %zu, mSize = %zu", offset, length, mSize);
  }
  CHECK(offset + length <= mSize);

  mRangeOffset = offset;
  mRangeLength = length;
}

MetaDataBase& GonkMediaBuffer::meta_data() { return *mMetaData; }

void GonkMediaBuffer::reset() {
  mMetaData->clear();
  set_range(0, mSize);
}

GonkMediaBuffer::~GonkMediaBuffer() {
  CHECK(mObserver == NULL);

  if (mOwnsData && mData != NULL && mMemory == NULL) {
    free(mData);
    mData = NULL;
  }

  if (mMemory.get() != nullptr) {
    getSharedControl()->setDeadObject();
  }
  delete mMetaData;
}

void GonkMediaBuffer::setObserver(MediaBufferObserver* observer) {
  CHECK(observer == NULL || mObserver == NULL);
  mObserver = observer;
}

}  // namespace android
