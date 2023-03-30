/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
#define DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H

#include "MediaTrackGraph.h"
#include "mozilla/Mutex.h"
#include "VideoFrameContainer.h"

namespace mozilla {

class FakeMediaTrackGraph : public MediaTrackGraph {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FakeMediaTrackGraph)
 public:
  FakeMediaTrackGraph()
      : MediaTrackGraph(16000),
        mCurrentTime((long long)0, "FakeMediaTrackGraph:CurrentTime") {}

  void DispatchToMainThreadStableState(already_AddRefed<nsIRunnable> aRunnable);
  virtual void OpenAudioInput(DeviceInputTrack* aTrack) override;
  virtual void CloseAudioInput(DeviceInputTrack* aTrack) override;
  virtual Watchable<GraphTime>& CurrentTime() override;

  virtual bool OnGraphThreadOrNotRunning() const override;
  virtual bool OnGraphThread() const override;
  virtual bool Destroyed() const override;

 protected:
  Watchable<mozilla::GraphTime> mCurrentTime;
  ~FakeMediaTrackGraph() {}
};

/**
 * This is a stream for camera preview.
 *
 * XXX It is a temporary fix of SourceMediaStream.
 * A camera preview requests no delay and no buffering stream,
 * but the SourceMediaStream does not support it.
 */
class CameraPreviewMediaStream : public ProcessedMediaTrack {
  typedef mozilla::layers::Image Image;

 public:
  CameraPreviewMediaStream();

  void AddVideoOutput(VideoFrameContainer* aContainer) override;
  void RemoveVideoOutput(VideoFrameContainer* aContainer) override;
  void AddListener(MediaTrackListener* aListener) override;
  RefPtr<GenericPromise> RemoveListener(MediaTrackListener* aListener) override;
  void Destroy() override;
  void OnPreviewStateChange(bool aActive);

  void Invalidate();

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  uint32_t NumberOfChannels() const override;

  // Call these on any thread.
  void SetCurrentFrame(const gfx::IntSize& aIntrinsicSize, Image* aImage);
  void ClearCurrentFrame();
  void RateLimit(bool aLimit);

 protected:
  // mMutex protects all the class' fields.
  // This class is not registered to MediaTrackGraph.
  // It needs to protect all the fields.
  Mutex mMutex;
  int32_t mInvalidatePending;
  uint32_t mDiscardedFrames;
  bool mRateLimit;
  bool mTrackCreated;
  RefPtr<FakeMediaTrackGraph> mFakeMediaTrackGraph;

  // TODO:from mediastream
  nsTArray<RefPtr<MediaTrackListener> > mListeners;
};

}  // namespace mozilla

#endif  // DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
