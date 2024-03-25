/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 ** Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 *reserved.
 ** Copyright 2012 The Android Open Source Project
 **
 ** Licensed under the Apache License Version 2.0(the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing software
 ** distributed under the License is distributed on an "AS IS" BASIS
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <EGL/egl.h>
#include <gui/BufferItem.h>
#include <gui/BufferQueue.h>
#include <gui/Surface.h>
#include <hardware/hardware.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>
#include <utils/String8.h>
#include <vndk/hardware_buffer.h>

#include "GonkDisplayWorkThread.h"

#include "FramebufferSurface.h"

#ifndef NUM_FRAMEBUFFER_SURFACE_BUFFERS
#  define NUM_FRAMEBUFFER_SURFACE_BUFFERS (3)
#endif

#ifdef LOG_TAG
#  undef LOG_TAG
#  define LOG_TAG "FramebufferSurface"
#endif

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

/*
 * This implements the (main) framebuffer management. This class
 * was adapted from the version in SurfaceFlinger
 */
FramebufferSurface::FramebufferSurface(
    uint32_t width, uint32_t height, uint32_t format,
    const sp<IGraphicBufferConsumer>& consumer, DisplayUtils displayUtils,
    bool visibility)
    : DisplaySurface(consumer),
      mCurrentSlot(BufferQueue::INVALID_BUFFER_SLOT),
      mCurrentBuffer(),
      mHasPendingRelease(false),
      mPreviousBufferSlot(BufferQueue::INVALID_BUFFER_SLOT),
      mPreviousBuffer(),
      mPrevFBAcquireFence(Fence::NO_FENCE),
      mLastPresentFence(Fence::NO_FENCE),
      mDisplayUtils(displayUtils),
      mVisibility(visibility) {
  mName = "FramebufferSurface";

  mConsumer->setConsumerName(mName);
  mConsumer->setConsumerUsageBits(
#if ANDROID_EMULATOR
      GRALLOC_USAGE_HW_FB |
#endif
      GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER);
  mConsumer->setDefaultBufferFormat(format);
  mConsumer->setDefaultBufferSize(width, height);
  mConsumer->setMaxAcquiredBufferCount(NUM_FRAMEBUFFER_SURFACE_BUFFERS);
}

void FramebufferSurface::resizeBuffers(const uint32_t width,
                                       const uint32_t height) {
  mConsumer->setDefaultBufferSize(width, height);
}

status_t FramebufferSurface::beginFrame(bool /*mustRecompose*/) {
  return NO_ERROR;
}

status_t FramebufferSurface::prepareFrame(CompositionType /*compositionType*/) {
  return NO_ERROR;
}

status_t FramebufferSurface::advanceFrame() {
  // Once we remove FB HAL support, we can call nextBuffer() from here
  // instead of using onFrameAvailable(). No real benefit, except it'll be
  // more like VirtualDisplaySurface.
  return NO_ERROR;
}

status_t FramebufferSurface::nextBuffer(sp<GraphicBuffer>& outBuffer,
                                        sp<Fence>& outFence) {
  Mutex::Autolock lock(mMutex);

  BufferItem item;
  status_t err = acquireBufferLocked(&item, 0);
  if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
    outBuffer = mCurrentBuffer;
    return NO_ERROR;
  } else if (err != NO_ERROR) {
    ALOGE("error acquiring buffer: %s (%d)", strerror(-err), err);
    return err;
  }

  const auto slot = item.mSlot;
  const auto buffer = mSlots[item.mSlot].mGraphicBuffer;
  const auto acquireFence = item.mFence;
  presentLocked(slot, buffer, acquireFence);

  // If the BufferQueue has freed and reallocated a buffer in mCurrentSlot
  // then we may have acquired the slot we already own.  If we had released
  // our current buffer before we call acquireBuffer then that release call
  // would have returned STALE_BUFFER_SLOT, and we would have called
  // freeBufferLocked on that slot.  Because the buffer slot has already
  // been overwritten with the new buffer all we have to do is skip the
  // releaseBuffer call and we should be in the same state we'd be in if we
  // had released the old buffer first.
  if (mCurrentSlot != BufferQueue::INVALID_BUFFER_SLOT &&
      slot != mCurrentSlot) {
    mHasPendingRelease = true;
    mPreviousBufferSlot = mCurrentSlot;
    mPreviousBuffer = mCurrentBuffer;
  }
  mCurrentSlot = slot;
  outBuffer = mCurrentBuffer = buffer;
  outFence = acquireFence;

  return NO_ERROR;
}

// Overrides ConsumerBase::onFrameAvailable(), does not call base class impl.
void FramebufferSurface::onFrameAvailable(const BufferItem& item) {
  (void)item;
  carthage::GonkDisplayWorkThread::Get()->Post([=] {
    sp<GraphicBuffer> buf;
    sp<Fence> acquireFence;
    status_t err = nextBuffer(buf, acquireFence);
    if (err != NO_ERROR) {
      ALOGE("error latching nnext FramebufferSurface buffer: %s (%d)",
            strerror(-err), err);
      return;
    }

    if (acquireFence.get() && acquireFence->isValid()) {
      mPrevFBAcquireFence = new Fence(acquireFence->dup());
    } else {
      mPrevFBAcquireFence = Fence::NO_FENCE;
    }

    lastHandle = buf->handle;
  });
}

void FramebufferSurface::presentLocked(const int bufferSlot,
                                       const sp<GraphicBuffer>& buffer,
                                       const sp<Fence>& acquireFence) {
  uint32_t numTypes = 0;
  uint32_t numRequests = 0;
  auto error = ::HWC2::Error::None;
  ui::Dataspace dataspace = ui::Dataspace::UNKNOWN;

  struct timeval tv1, tv2, delta;
  gettimeofday(&tv1, nullptr);
  if (mVisibility) {
    if (mDisplayUtils.type == DisplayUtils::EXTERNAL) {
      if (acquireFence.get() && acquireFence->isValid()) {
        sp<Fence> fenceObj = new Fence(acquireFence->dup());
        fenceObj->waitForever("FramebufferSurface::Post");
      }
      mDisplayUtils.utils.extFBDevice->Post(buffer->handle);
    } else {
#if ANDROID_VERSION >= 33
      nsecs_t expectedPresentTime = systemTime();  // FIXME
      error = mDisplayUtils.utils.hwcDisplay->validate(expectedPresentTime,
                                                       &numTypes, &numRequests);
#else
      error = mDisplayUtils.utils.hwcDisplay->validate(&numTypes, &numRequests);
#endif
      if (error != ::HWC2::Error::None && error != ::HWC2::Error::HasChanges) {
        ALOGE("prepare: validate failed : %s (%d)", to_string(error).c_str(),
              static_cast<int32_t>(error));
        goto FrameCommitted;
      }

      if (numTypes || numRequests) {
        ALOGE("prepare: validate required changes : %s (%d)",
              to_string(error).c_str(), static_cast<int32_t>(error));
        goto FrameCommitted;
      }

      error = mDisplayUtils.utils.hwcDisplay->acceptChanges();
      if (error != ::HWC2::Error::None) {
        ALOGE("prepare: acceptChanges failed: %s", to_string(error).c_str());
        goto FrameCommitted;
      }

      (void)mDisplayUtils.utils.hwcDisplay->setClientTarget(
          bufferSlot, buffer, acquireFence, dataspace);

      error = mDisplayUtils.utils.hwcDisplay->present(&mLastPresentFence);
      if (error != ::HWC2::Error::None) {
        ALOGE("present: failed : %s (%d)", to_string(error).c_str(),
              static_cast<int32_t>(error));
      }
    }
  }

FrameCommitted:
  gettimeofday(&tv2, nullptr);
  timersub(&tv2, &tv1, &delta);
  long usec = delta.tv_sec * 1000000 + delta.tv_usec;
  if (usec > 1000000) {  // show warning if delta is more than 1s.
    ALOGW("Frame delay is %ld us on %s", usec,
          (mDisplayUtils.type == DisplayUtils::EXTERNAL) ? "Ext Screen"
                                                         : "Primary Screen");
  }
  onFrameCommitted();
}

void FramebufferSurface::freeBufferLocked(int slotIndex) {
  ConsumerBase::freeBufferLocked(slotIndex);
  if (slotIndex == mCurrentSlot) {
    mCurrentSlot = BufferQueue::INVALID_BUFFER_SLOT;
  }
}

status_t FramebufferSurface::setReleaseFenceFd(int fenceFd) {
  status_t err = NO_ERROR;
  if (fenceFd >= 0) {
    sp<Fence> fence(new Fence(fenceFd));
    if (mCurrentSlot != BufferQueue::INVALID_BUFFER_SLOT) {
      status_t err = addReleaseFence(mCurrentSlot, mCurrentBuffer, fence);
      ALOGE_IF(err, "setReleaseFenceFd: failed to add the fence: %s (%d)",
               strerror(-err), err);
    }
  }
  return err;
}

int FramebufferSurface::GetPrevDispAcquireFd() {
  if (mPrevFBAcquireFence.get() && mPrevFBAcquireFence->isValid()) {
    return mPrevFBAcquireFence->dup();
  }
  return -1;
}

void FramebufferSurface::onFrameCommitted() {
  if (mHasPendingRelease) {
    addReleaseFenceLocked(mPreviousBufferSlot, mPreviousBuffer,
                          mLastPresentFence);
    releaseBufferLocked(mPreviousBufferSlot, mPreviousBuffer);
    mHasPendingRelease = false;
  }
}

// ----------------------------------------------------------------------------
};  // namespace android
// ----------------------------------------------------------------------------
