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

#ifndef GONK_CAMERA_SOURCE_H_

#define GONK_CAMERA_SOURCE_H_

#include <camera/CameraParameters.h>
#include <gui/BufferItemConsumer.h>
#include <media/hardware/MetadataBufferType.h>
#include <media/stagefright/MediaSource.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/String16.h>

#include "GonkCameraHwMgr.h"
#include "GonkMediaBuffer.h"

namespace mozilla {
class ICameraControl;
}

namespace android {

class IMemory;

class GonkCameraSource : public MediaSource, public MediaBufferObserver {
 public:
  static GonkCameraSource* Create(const sp<GonkCameraHardware>& aCameraHw,
                                  Size videoSize, int32_t frameRate,
                                  bool storeMetaDataInVideoBuffers = false);

  static GonkCameraSource* Create(mozilla::ICameraControl* aControl,
                                  Size videoSize, int32_t frameRate);

  virtual ~GonkCameraSource();

  virtual status_t start(MetaData* params = NULL);
  virtual status_t stop() { return reset(); }
  virtual status_t read(MediaBufferBase** buffer,
                        const ReadOptions* options = NULL);
  virtual status_t setStopTimeUs(int64_t stopTimeUs);

  /**
   * Check whether a GonkCameraSource object is properly initialized.
   * Must call this method before stop().
   * @return OK if initialization has successfully completed.
   */
  virtual status_t initCheck() const;

  /**
   * Returns the MetaData associated with the GonkCameraSource,
   * including:
   * kKeyColorFormat: YUV color format of the video frames
   * kKeyWidth, kKeyHeight: dimension (in pixels) of the video frames
   * kKeySampleRate: frame rate in frames per second
   * kKeyMIMEType: always fixed to be MEDIA_MIMETYPE_VIDEO_RAW
   */
  virtual sp<MetaData> getFormat();

  /**
   * Tell whether this camera source stores meta data or real YUV
   * frame data in video buffers.
   *
   * @return a valid type if meta data is stored in the video
   *      buffers; kMetadataBufferTypeInvalid if real YUV data is stored in
   *      the video buffers.
   */
  MetadataBufferType metaDataStoredInVideoBuffers() const;

  virtual void signalBufferReturned(MediaBufferBase* buffer);

  /**
   * It sends recording frames to listener directly in the same thread.
   * Because recording frame is critical resource and it should not be
   * propagated to other thread as much as possible or there could be frame
   * rate jitter due to camera HAL waiting for resource.
   */
  class DirectBufferListener : public RefBase {
   public:
    DirectBufferListener(){};

    virtual status_t BufferAvailable(MediaBufferBase* aBuffer) = 0;

   protected:
    virtual ~DirectBufferListener() {}
  };

  status_t AddDirectBufferListener(DirectBufferListener* aListener);

 protected:
  /**
   * The class for listening to BufferQueue's onFrameAvailable. This is used to
   * receive video buffers in VIDEO_BUFFER_MODE_BUFFER_QUEUE mode. When a frame
   * is available, CameraSource::processBufferQueueFrame() will be called.
   */
  class BufferQueueListener
      : public Thread,
        public BufferItemConsumer::FrameAvailableListener {
   public:
    BufferQueueListener(const sp<BufferItemConsumer>& consumer,
                        const sp<GonkCameraSource>& cameraSource);
    virtual void onFrameAvailable(const BufferItem& item);
    virtual bool threadLoop();

   private:
    static const nsecs_t kFrameAvailableTimeout = 50000000;  // 50ms

    sp<BufferItemConsumer> mConsumer;
    sp<GonkCameraSource> mCameraSource;

    Mutex mLock;
    Condition mFrameAvailableSignal;
    bool mFrameAvailable;
  };

  enum CameraFlags {
    FLAGS_SET_CAMERA = 1L << 0,
    FLAGS_HOT_CAMERA = 1L << 1,
  };

  int32_t mCameraFlags;
  Size mVideoSize;
  int32_t mNumInputBuffers;
  int32_t mVideoFrameRate;
  int32_t mColorFormat;
  int32_t mEncoderFormat;
  int32_t mEncoderDataSpace;
  status_t mInitCheck;

  sp<MetaData> mMeta;

  int64_t mStartTimeUs;
  int32_t mNumFramesReceived;
  int64_t mLastFrameTimestampUs;
  bool mStarted;
  bool mEos;
  int32_t mNumFramesEncoded;

  // Time between capture of two frames.
  int64_t mTimeBetweenFrameCaptureUs;

  GonkCameraSource(const sp<GonkCameraHardware>& aCameraHw, Size videoSize,
                   int32_t frameRate, bool storeMetaDataInVideoBuffers = false);

  virtual int startCameraRecording();
  virtual void stopCameraRecording();
  virtual void releaseRecordingFrame(const sp<IMemory>& frame);
  virtual void releaseRecordingFrameHandle(native_handle_t* handle);

  // Returns true if need to skip the current frame.
  // Called from dataCallbackTimestamp.
  virtual bool skipCurrentFrame(int64_t timestampUs) { return false; }

  friend struct GonkCameraSourceListener;
  // Callback called when still camera raw data is available.
  virtual void dataCallback(int32_t msgType, const sp<IMemory>& data) {}

  virtual void dataCallbackTimestamp(int64_t timestampUs, int32_t msgType,
                                     const sp<IMemory>& data);

  virtual void recordingFrameHandleCallbackTimestamp(int64_t timestampUs,
                                                     native_handle_t* handle);

 private:
  Mutex mLock;
  Condition mFrameAvailableCondition;
  Condition mFrameCompleteCondition;
  List<sp<IMemory>> mFramesReceived;
  List<sp<IMemory>> mFramesBeingEncoded;
  List<int64_t> mFrameTimes;
  bool mRateLimit;

  int64_t mFirstFrameTimeUs;
  int64_t mStopSystemTimeUs;
  int32_t mNumFramesDropped;
  int32_t mNumGlitches;
  int64_t mGlitchDurationThresholdUs;
  bool mCollectStats;

  // The mode video buffers are received from camera. One of
  // VIDEO_BUFFER_MODE_*.
  int32_t mVideoBufferMode;

  static const uint32_t kDefaultVideoBufferCount = 32;

  /**
   * The following variables are used in VIDEO_BUFFER_MODE_BUFFER_QUEUE mode.
   */
  static const size_t kConsumerBufferCount = 8;
  static const nsecs_t kMemoryBaseAvailableTimeoutNs = 200000000;  // 200ms
  // Consumer and producer of the buffer queue between this class and camera.
  sp<BufferItemConsumer> mVideoBufferConsumer;
  sp<IGraphicBufferProducer> mVideoBufferProducer;
  // Memory used to send the buffers to encoder, where sp<IMemory> stores
  // VideoNativeMetadata.
  sp<IMemoryHeap> mMemoryHeapBase;
  List<sp<IMemory>> mMemoryBases;
  // The condition that will be signaled when there is an entry available in
  // mMemoryBases.
  Condition mMemoryBaseAvailableCond;
  // A mapping from ANativeWindowBuffer sent to encoder to BufferItem received
  // from camera. This is protected by mLock.
  KeyedVector<ANativeWindowBuffer*, BufferItem> mReceivedBufferItemMap;
  sp<BufferQueueListener> mBufferQueueListener;

  sp<GonkCameraHardware> mCameraHw;
  sp<DirectBufferListener> mDirectBufferListener;

  void releaseQueuedFrames();
  void releaseOneRecordingFrame(const sp<IMemory>& frame);
  void createVideoBufferMemoryHeap(size_t size, uint32_t bufferCount);

  status_t init(Size videoSize, int32_t frameRate,
                bool storeMetaDataInVideoBuffers);
  // Initialize the buffer queue used in VIDEO_BUFFER_MODE_BUFFER_QUEUE mode.
  status_t initBufferQueue(uint32_t width, uint32_t height, uint32_t format,
                           android_dataspace dataSpace, uint32_t bufferCount);
  status_t isCameraColorFormatSupported(const CameraParameters& params);
  status_t configureCamera(CameraParameters* params, int32_t width,
                           int32_t height, int32_t frameRate);

  status_t checkVideoSize(const CameraParameters& params, int32_t width,
                          int32_t height);

  status_t checkFrameRate(const CameraParameters& params, int32_t frameRate);

  // Check if this frame should be skipped based on the frame's timestamp in
  // microsecond. mLock must be locked before calling this function.
  bool shouldSkipFrameLocked(int64_t timestampUs);

  // Process a buffer item received in BufferQueueListener.
  virtual void processBufferQueueFrame(BufferItem& buffer);

  void releaseCamera();
  status_t reset();

  GonkCameraSource(const GonkCameraSource&);
  GonkCameraSource& operator=(const GonkCameraSource&);
};

}  // namespace android

#endif  // GONK_CAMERA_SOURCE_H_
