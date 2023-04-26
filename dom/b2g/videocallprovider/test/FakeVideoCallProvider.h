/*
 * Copyright (C) 2012-2014 Mozilla Foundation
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

#ifndef DOM_FAKE_VIDEO_CALL_PROVIDER_H
#define DOM_FAKE_VIDEO_CALL_PROVIDER_H

#include "nsIVideoCallProvider.h"
#include "nsIVideoCallCallback.h"
#include <binder/IMemory.h>
#include <utils/threads.h>
#include <camera/Camera.h>
#include <media/stagefright/foundation/AHandler.h>
#include <utils/StrongPointer.h>
#include <gui/Surface.h>
#include <ui/GraphicBufferMapper.h>
#include <media/stagefright/foundation/AMessage.h>

using namespace android;

class FakeYUVFeeder;

class VideoCallCameraCapabilities : public nsIVideoCallCameraCapabilities {
 public:
  NS_DECL_ISUPPORTS

  VideoCallCameraCapabilities(){};

  /* readonly attribute unsigned short height; */
  NS_IMETHOD GetHeight(uint16_t* aHeight);

  /* readonly attribute unsigned short width; */
  NS_IMETHOD GetWidth(uint16_t* aWidth);

  /* readonly attribute boolean zoomSupported; */
  NS_IMETHOD GetZoomSupported(bool* aZoomSupported);

  /* readonly attribute float maxZoom; */
  NS_IMETHOD GetMaxZoom(float* aMaxZoom);

 private:
  virtual ~VideoCallCameraCapabilities(){};
};

class FakeVideoCallProvider : public nsIVideoCallProvider {
  friend FakeYUVFeeder;

 public:
  NS_DECL_ISUPPORTS

  FakeVideoCallProvider();

  // nsIVideoCallProvider interface
  /* void setCamera (in short cameraId); */
  NS_IMETHOD SetCamera(int16_t cameraId);

  /* void setPreviewSurface (in AGraphicBuffProducer producer, in unsigned short
   * width, in unsigned short height); */
  NS_IMETHOD SetPreviewSurface(
      android::sp<android::IGraphicBufferProducer>& aProducer,
      uint16_t aPreferWidth, uint16_t aPreferHeight);

  /* void setDisplaySurface (in AGraphicBuffProducer producer, in unsigned short
   * width, in unsigned short height); */
  NS_IMETHOD SetDisplaySurface(
      android::sp<android::IGraphicBufferProducer>& aProducer,
      uint16_t aPreferWidth, uint16_t aPreferHeight);

  /* void setDeviceOrientation (in unsigned short rotation); */
  NS_IMETHOD SetDeviceOrientation(uint16_t rotation);

  /* void setZoom (in float value); */
  NS_IMETHOD SetZoom(float value);

  /* void sendSessionModifyRequest (in nsIVideoCallProfile fromProfile, in
   * nsIVideoCallProfile toProfile); */
  NS_IMETHOD SendSessionModifyRequest(nsIVideoCallProfile* fromProfile,
                                      nsIVideoCallProfile* toProfile);

  /* void sendSessionModifyResponse (in nsIVideoCallProfile responseProfile); */
  NS_IMETHOD SendSessionModifyResponse(nsIVideoCallProfile* responseProfile);

  /* void requestCameraCapabilities (); */
  NS_IMETHOD RequestCameraCapabilities(void);

  /* void registerCallback (in nsIVideoCallCallback callback); */
  NS_IMETHOD RegisterCallback(nsIVideoCallCallback* callback);

  /* void unregisterCallback (in nsIVideoCallCallback callback); */
  NS_IMETHOD UnregisterCallback(nsIVideoCallCallback* callback);

 private:
  FakeVideoCallProvider(const FakeVideoCallProvider&) = delete;
  FakeVideoCallProvider& operator=(const FakeVideoCallProvider&) = delete;
  virtual ~FakeVideoCallProvider();

  nsIVideoCallCallback* mVideoCallCallback;

  // Preview surface
  void StopPreview();
  void SetCameraParameters();

  android::sp<android::Camera> mCamera;
  uint16_t mPreferPreviewWidth;
  uint16_t mPreferPreviewHeight;
  uint16_t mResultPreviewWidth;
  uint16_t mResultPreviewHeight;
  bool mPreviewStarted;
  nsIVideoCallCameraCapabilities* mVideoCallCameraCapabilities;

  // Display surface
  nsresult
  StartFakeImage();  // Test function to feed image data to the created surface.
  void StopDisplay();
  sp<ANativeWindow> mTestANativeWindow;
  unsigned char* mTestImage;
  unsigned char* mTestImage2;
  int mTestImageIndex;
  bool mIsTestRunning;
  sp<IGraphicBufferProducer> mFakeImageProducer;
  uint16_t mPreferDisplayWidth;
  uint16_t mPreferDisplayHeight;
  uint16_t mResultDisplayWidth;
  uint16_t mResultDisplayHeight;
  sp<ALooper> mFakeYUVFeederLooper;
  sp<FakeYUVFeeder> mFakeYUVFeeder;
  bool mDisplayStarted;
};

class FakeYUVFeeder : public AHandler {
 public:
  enum { kWhatSendFakeImage };

  FakeYUVFeeder(FakeVideoCallProvider* aFakeVideoCallProvider);
  virtual ~FakeYUVFeeder();
  virtual void onMessageReceived(const sp<AMessage>& msg);
  void SendFakeImage(FakeVideoCallProvider* aFakeVideoCallProvider);

 private:
  RefPtr<FakeVideoCallProvider> mFakeVideoCallProvider;
};

#endif  // DOM_FAKE_VIDEO_CALL_PROVIDER_H
