/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreviewMediaTrack.h"
#include "CameraCommon.h"
#include "MediaTrackListener.h"
#include "VideoFrameContainer.h"

/**
 * Maximum number of outstanding invalidates before we start to drop frames;
 * if we hit this threshold, it is an indicator that the main thread is
 * either very busy or the device is busy elsewhere (e.g. encoding or
 * persisting video data).
 */
#define MAX_INVALIDATE_PENDING 4

using namespace mozilla::layers;
using namespace mozilla::dom;

namespace mozilla {

void FakeMediaTrackGraph::DispatchToMainThreadStableState(
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> task = aRunnable;
  NS_DispatchToMainThread(task);
}

void FakeMediaTrackGraph::OpenAudioInput(DeviceInputTrack* aTrack) {}

void FakeMediaTrackGraph::CloseAudioInput(DeviceInputTrack* aTrack) {}

Watchable<mozilla::GraphTime>& FakeMediaTrackGraph::CurrentTime() {
  return mCurrentTime;
}

bool FakeMediaTrackGraph::OnGraphThreadOrNotRunning() const { return false; }

bool FakeMediaTrackGraph::OnGraphThread() const { return false; }

bool FakeMediaTrackGraph::Destroyed() const { return false; }

CameraPreviewMediaTrack::CameraPreviewMediaTrack()
    : ProcessedMediaTrack(FakeMediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
                          MediaSegment::VIDEO, new VideoSegment()),
      mMutex("mozilla::camera::CameraPreviewMediaTrack"),
      mInvalidatePending(0),
      mDiscardedFrames(0),
      mRateLimit(false),
      mTrackCreated(false) {
  mFakeMediaTrackGraph = new FakeMediaTrackGraph();
}

void CameraPreviewMediaTrack::AddVideoOutput(VideoFrameContainer* aContainer) {
  MutexAutoLock lock(mMutex);
  RefPtr<VideoFrameContainer> container = aContainer;
  AddVideoOutputImpl(container.forget());
}

void CameraPreviewMediaTrack::RemoveVideoOutput(
    VideoFrameContainer* aContainer) {
  MutexAutoLock lock(mMutex);
  RemoveVideoOutputImpl(aContainer);
}

void CameraPreviewMediaTrack::AddListener(MediaTrackListener* aListener) {
  MutexAutoLock lock(mMutex);

  MediaTrackListener* listener = *mListeners.AppendElement() = aListener;
  // TODO: Need to check the usage of these callbacks
  // listener->NotifyBlockingChanged(mFakeMediaTrackGraph,
  // MediaStreamListener::UNBLOCKED);
  // listener->NotifyHasCurrentData(mFakeMediaTrackGraph);
}

RefPtr<GenericPromise> CameraPreviewMediaTrack::RemoveListener(
    MediaTrackListener* aListener) {
  MutexAutoLock lock(mMutex);

  RefPtr<MediaTrackListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyRemoved(mFakeMediaTrackGraph);

  // TODO: check if that's correct.
  RefPtr<GenericPromise> p = GenericPromise::CreateAndResolve(true, __func__);

  return p;
}

void CameraPreviewMediaTrack::OnPreviewStateChange(bool aActive) {
  if (aActive) {
    MutexAutoLock lock(mMutex);
    if (!mTrackCreated) {
      mTrackCreated = true;
      VideoSegment tmpSegment;
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaTrackListener* l = mListeners[j];
        // TODO: Need to check the usage of these callbacks
        l->NotifyQueuedChanges(mFakeMediaTrackGraph, 0, tmpSegment);
        // l->NotifyFinishedTrackCreation(mFakeMediaTrackGraph);
      }
    }
  }
}

void CameraPreviewMediaTrack::Destroy() {
  MutexAutoLock lock(mMutex);
  mMainThreadDestroyed = true;
  DestroyImpl();
}

void CameraPreviewMediaTrack::Invalidate() {
  MutexAutoLock lock(mMutex);
  --mInvalidatePending;
  for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
       i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->Invalidate();
  }
}

void CameraPreviewMediaTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                           uint32_t aFlags) {
  return;
}

void CameraPreviewMediaTrack::RateLimit(bool aLimit) { mRateLimit = aLimit; }

void CameraPreviewMediaTrack::SetCurrentFrame(
    const gfx::IntSize& aIntrinsicSize, Image* aImage) {
  {
    MutexAutoLock lock(mMutex);

    if (mInvalidatePending > 0) {
      if (mRateLimit || mInvalidatePending > MAX_INVALIDATE_PENDING) {
        ++mDiscardedFrames;
        DOM_CAMERA_LOGW("Discard preview frame %d, %d invalidation(s) pending",
                        mDiscardedFrames, mInvalidatePending);
        return;
      }

      DOM_CAMERA_LOGI("Update preview frame, %d invalidation(s) pending",
                      mInvalidatePending);
    }
    mDiscardedFrames = 0;

    TimeStamp now = TimeStamp::Now();
    for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
         i < mVideoOutputs.Length(); ++i) {
      VideoFrameContainer* output = mVideoOutputs[i];
      output->SetCurrentFrame(aIntrinsicSize, aImage, now);
    }

    ++mInvalidatePending;
  }

  NS_DispatchToMainThread(
      NewRunnableMethod("CameraPreviewMediaTrack::SetCurrentFrame", this,
                        &CameraPreviewMediaTrack::Invalidate));
}

void CameraPreviewMediaTrack::ClearCurrentFrame() {
  MutexAutoLock lock(mMutex);

  for (nsTArray<RefPtr<VideoFrameContainer> >::size_type i = 0;
       i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->ClearCurrentFrame();
    NS_DispatchToMainThread(
        NewRunnableMethod("CameraPreviewMediaTrack::ClearCurrentFrame", output,
                          &VideoFrameContainer::Invalidate));
  }
}

uint32_t CameraPreviewMediaTrack::NumberOfChannels() const { return 1; };

}  // namespace mozilla
