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

#ifndef GONK_MEDIA_BUFFER_H_

#define GONK_MEDIA_BUFFER_H_

#include "CameraCommon.h"

#include <atomic>
#include <list>

#include <pthread.h>

#include <binder/MemoryDealer.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <media/stagefright/MediaBufferBase.h>

namespace android {

struct ABuffer;
class GonkMediaBuffer;
class MediaBufferObserver;
class MetaDataBase;

class GonkMediaBuffer : public MediaBufferBase {
 public:
  // allocations larger than or equal to this will use shared memory.
  static const size_t kSharedMemThreshold = 64 * 1024;

  // The underlying data remains the responsibility of the caller!
  GonkMediaBuffer(void* data, size_t size);

  explicit GonkMediaBuffer(size_t size);

  explicit GonkMediaBuffer(const sp<ABuffer>& buffer);
#ifndef NO_IMEMORY
  GonkMediaBuffer(const sp<IMemory>& mem)
      : GonkMediaBuffer((uint8_t*)IMEMORY_POINTER(mem) + sizeof(SharedControl),
                        mem->size()) {
    // delegate and override mMemory
    mMemory = mem;
  }
#endif

  // If MediaBufferGroup is set, decrement the local reference count;
  // if the local reference count drops to 0, return the buffer to the
  // associated MediaBufferGroup.
  //
  // If no MediaBufferGroup is set, the local reference count must be zero
  // when called, whereupon the GonkMediaBuffer is deleted.
  virtual void release();

  // Increments the local reference count.
  // Use only when MediaBufferGroup is set.
  virtual void add_ref();

  virtual void* data() const;
  virtual size_t size() const;

  virtual size_t range_offset() const;
  virtual size_t range_length() const;

  virtual void set_range(size_t offset, size_t length);

  MetaDataBase& meta_data();

  // Clears meta data and resets the range to the full extent.
  virtual void reset();

  virtual void setObserver(MediaBufferObserver* group);

  // sum of localRefcount() and remoteRefcount()
  // Result should be treated as approximate unless the result precludes
  // concurrent accesses.
  virtual int refcount() const { return localRefcount() + remoteRefcount(); }

  // Result should be treated as approximate unless the result precludes
  // concurrent accesses.
  virtual int localRefcount() const {
    return mRefCount.load(std::memory_order_relaxed);
  }

  virtual int remoteRefcount() const {
#ifndef NO_IMEMORY
    if (mMemory.get() == nullptr || IMEMORY_POINTER(mMemory) == nullptr) {
      return 0;
    }
    int32_t remoteRefcount =
        reinterpret_cast<SharedControl*>(IMEMORY_POINTER(mMemory))
            ->getRemoteRefcount();
    // Sanity check so that remoteRefCount() is non-negative.
    return remoteRefcount >= 0 ? remoteRefcount
                               : 0;  // do not allow corrupted data.
#else
    return 0;
#endif
  }

  // returns old value
  int addRemoteRefcount(int32_t value) {
#ifndef NO_IMEMORY
    if (mMemory.get() == nullptr || IMEMORY_POINTER(mMemory) == nullptr) {
      return 0;
    }
    return reinterpret_cast<SharedControl*>(IMEMORY_POINTER(mMemory))
        ->addRemoteRefcount(value);
#else
    (void)value;
    return 0;
#endif
  }

  bool isDeadObject() const { return isDeadObject(mMemory); }

  static bool isDeadObject(const sp<IMemory>& memory) {
#ifndef NO_IMEMORY
    if (memory.get() == nullptr || IMEMORY_POINTER(memory) == nullptr) {
      return false;
    }
    return reinterpret_cast<SharedControl*>(IMEMORY_POINTER(memory))
        ->isDeadObject();
#else
    (void)memory;
    return false;
#endif
  }

  // Sticky on enabling of shared memory MediaBuffers. By default we don't use
  // shared memory for MediaBuffers, but we enable this for those processes
  // that export MediaBuffers.
  static void useSharedMemory() {
    std::atomic_store_explicit(&mUseSharedMemory, (int_least32_t)1,
                               std::memory_order_seq_cst);
  }

 protected:
  // true if MediaBuffer is observed (part of a MediaBufferGroup).
  inline bool isObserved() const { return mObserver != nullptr; }

  virtual ~GonkMediaBuffer();

  sp<IMemory> mMemory;

 private:
  friend class MediaBufferGroup;
  friend class OMXDecoder;
  friend class BnMediaSource;
  friend class BpMediaSource;

  // For use by OMXDecoder, reference count must be 1, drop reference
  // count to 0 without signalling the observer.
  void claim();

  MediaBufferObserver* mObserver;
  std::atomic<int> mRefCount;

  void* mData;
  size_t mSize, mRangeOffset, mRangeLength;
  sp<ABuffer> mBuffer;

  bool mOwnsData;

  MetaDataBase* mMetaData;

  static std::atomic_int_least32_t mUseSharedMemory;

  GonkMediaBuffer(const GonkMediaBuffer&);
  GonkMediaBuffer& operator=(const GonkMediaBuffer&);

  // SharedControl block at the start of IMemory.
  struct SharedControl {
    enum {
      FLAG_DEAD_OBJECT = (1 << 0),
    };

    // returns old value
    inline int32_t addRemoteRefcount(int32_t value) {
      return std::atomic_fetch_add_explicit(
          &mRemoteRefcount, (int_least32_t)value, std::memory_order_seq_cst);
    }

    inline int32_t getRemoteRefcount() const {
      return std::atomic_load_explicit(&mRemoteRefcount,
                                       std::memory_order_seq_cst);
    }

    inline void setRemoteRefcount(int32_t value) {
      std::atomic_store_explicit(&mRemoteRefcount, (int_least32_t)value,
                                 std::memory_order_seq_cst);
    }

    inline bool isDeadObject() const {
      return (std::atomic_load_explicit(&mFlags, std::memory_order_seq_cst) &
              FLAG_DEAD_OBJECT) != 0;
    }

    inline void setDeadObject() {
      (void)std::atomic_fetch_or_explicit(
          &mFlags, (int_least32_t)FLAG_DEAD_OBJECT, std::memory_order_seq_cst);
    }

    inline void clear() {
      std::atomic_store_explicit(&mFlags, (int_least32_t)0,
                                 std::memory_order_seq_cst);
      std::atomic_store_explicit(&mRemoteRefcount, (int_least32_t)0,
                                 std::memory_order_seq_cst);
    }

   private:
    // Caution: atomic_int_fast32_t is 64 bits on LP64.
    std::atomic_int_least32_t mFlags;
    std::atomic_int_least32_t mRemoteRefcount;
    int32_t unused[6] __attribute__((__unused__));  // additional buffer space
  };

  inline SharedControl* getSharedControl() const {
#ifndef NO_IMEMORY
    return reinterpret_cast<SharedControl*>(IMEMORY_POINTER(mMemory));
#else
    return nullptr;
#endif
  }
};

}  // namespace android

#endif  // MEDIA_BUFFER_H_
