/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_GONK_VIDEO_SOURCE_H_
#define MEDIAENGINE_GONK_VIDEO_SOURCE_H_

#include "MediaEngineCameraVideoSource.h"
#include "mozilla/Hal.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "common_video/include/i420_buffer_pool.h"

namespace mozilla {

class CameraControlWrapper;

class MediaEngineGonkVideoSource : public MediaEngineCameraVideoSource,
                                   public hal::ScreenConfigurationObserver {
 public:
  explicit MediaEngineGonkVideoSource(const MediaDevice* aMediaDevice);

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, uint64_t aWindowID,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate() override;
  nsresult Start() override;
  nsresult Stop() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const char** aOutBadConstraint) override;

  void SetTrack(const RefPtr<MediaTrack>& aTrack,
                const PrincipalHandle& aPrincipal) override;

  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth,
                         uint32_t aHeight);

  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration) override;

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override;

 private:
  ~MediaEngineGonkVideoSource();
  // Initialize the needed Video engine interfaces.
  void Init();

  size_t NumCapabilities() const override;
  webrtc::CaptureCapability GetCapability(size_t aIndex) const override;

  void UpdateScreenConfiguration(const hal::ScreenConfiguration& aConfig);

  already_AddRefed<layers::Image> RotateImage(layers::Image* aImage,
                                              uint32_t aWidth,
                                              uint32_t aHeight);

  nsCString mDeviceName;

  // mMutex protects certain members on 3 threads:
  // MediaManager, Camera and MediaTrackGraph.
  Mutex mMutex;

  // Current state of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  MediaEngineSourceState mState = kReleased;

  // The source track that we feed video data to.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  RefPtr<SourceMediaTrack> mTrack;

  // The PrincipalHandle that gets attached to the frames we feed to mTrack.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;

  // Set in Start() and Deallocate() on the owning thread.
  // Accessed in DeliverFrame() on the camera thread, guaranteed to happen
  // after Start() and before the end of Stop().
  RefPtr<layers::ImageContainer> mImageContainer;

  // A buffer pool used to manage the temporary buffer used when rotating
  // incoming images. Camera thread only.
  webrtc::I420BufferPool mRotationBufferPool;

  // The intrinsic size of the latest captured image, so we can feed black
  // images of the same size while stopped.
  // Set under mMutex on the Cameras thread. Accessed under one of the two.
  gfx::IntSize mImageSize = gfx::IntSize(0, 0);

  // The capability currently chosen by constraints of the user of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  webrtc::CaptureCapability mCapability;

  int mRotation = 0;

  mutable nsTArray<webrtc::CaptureCapability> mHardcodedCapabilities;

  RefPtr<layers::TextureClientRecycleAllocator> mTextureClientAllocator;

  RefPtr<CameraControlWrapper> mWrapper;
};

}  // namespace mozilla

#endif  // MEDIAENGINE_GONK_VIDEO_SOURCE_H_
