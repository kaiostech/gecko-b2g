/*
 * Copyright (C) 2012-2015 Mozilla Foundation
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

#include "FakeVideoCallProvider.h"

#include <binder/IPCThreadState.h>
#include <sys/system_properties.h>
#include <binder/IServiceManager.h>
#include <camera/CameraParameters.h>

#ifdef FAKE_VIDEOCALL_PROVIDER_LOG
#  undef FAKE_VIDEOCALL_PROVIDER_LOG
#endif
#define FAKE_VIDEOCALL_PROVIDER_LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "FakeVideoCallProvider", ##args)

using namespace android;

#define RESULT_PREVIEW_WIDTH 176
#define RESULT_PREVIEW_HEIGHT 144
#define RESULT_DISPLAY_WIDTH 176
#define RESULT_DISPLAY_HEIGHT 144

NS_IMPL_ISUPPORTS(VideoCallCameraCapabilities, nsIVideoCallCameraCapabilities)

/* readonly attribute unsigned short height; */
nsresult VideoCallCameraCapabilities::GetHeight(uint16_t* aHeight) {
  *aHeight = RESULT_PREVIEW_HEIGHT;
  return NS_OK;
}

/* readonly attribute unsigned short width; */
nsresult VideoCallCameraCapabilities::GetWidth(uint16_t* aWidth) {
  *aWidth = RESULT_PREVIEW_WIDTH;
  return NS_OK;
}

/* readonly attribute boolean zoomSupported; */
nsresult VideoCallCameraCapabilities::GetZoomSupported(bool* aZoomSupported) {
  *aZoomSupported = false;
  return NS_OK;
}

/* readonly attribute float maxZoom; */
nsresult VideoCallCameraCapabilities::GetMaxZoom(float* aMaxZoom) {
  *aMaxZoom = 0;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(FakeVideoCallProvider, nsIVideoCallProvider)

FakeVideoCallProvider::FakeVideoCallProvider()
    : mVideoCallCallback(NULL),
      mPreferPreviewWidth(0),
      mPreferPreviewHeight(0),
      mResultPreviewWidth(RESULT_PREVIEW_WIDTH),
      mResultPreviewHeight(RESULT_PREVIEW_HEIGHT),
      mPreviewStarted(false),
      mVideoCallCameraCapabilities(new VideoCallCameraCapabilities()),
      mTestImage(NULL),
      mTestImage2(NULL),
      mTestImageIndex(0),
      mIsTestRunning(false),
      mPreferDisplayWidth(0),
      mPreferDisplayHeight(0),
      mResultDisplayWidth(RESULT_DISPLAY_WIDTH),
      mResultDisplayHeight(RESULT_DISPLAY_HEIGHT),
      mDisplayStarted(false) {
  FAKE_VIDEOCALL_PROVIDER_LOG("FakeVideoCallProvider()");
}

/* void setCamera (in short cameraId); */
nsresult FakeVideoCallProvider::SetCamera(int16_t cameraId) {
  // Not implement yet.
  return NS_OK;
}

/* void setPreviewSurface (in AGraphicBuffProducer producer, in unsigned short
 * width, in unsigned short height); */
nsresult FakeVideoCallProvider::SetPreviewSurface(
    android::sp<android::IGraphicBufferProducer>& aProducer,
    uint16_t aPreferWidth, uint16_t aPreferHeight) {
  if (aProducer != NULL) {
    FAKE_VIDEOCALL_PROVIDER_LOG(
        "SetPreviewSurface aPreferWidth:%d, aPreferHeight:%d, "
        "mResultPreviewWidth:%d, mResultPreviewHeight:%d",
        aPreferWidth, aPreferHeight, mResultPreviewWidth, mResultPreviewHeight);

    if (mCamera == NULL) {
      const uint32_t CAMERASERVICE_POLL_DELAY = 1000000;

      sp<IServiceManager> sm = defaultServiceManager();
      sp<IBinder> binder;
      do {
        binder = sm->getService(String16("media.camera"));
        if (binder != 0) {
          break;
        }
        FAKE_VIDEOCALL_PROVIDER_LOG("CameraService not published, waiting...");
        usleep(CAMERASERVICE_POLL_DELAY);
      } while (true);

#if ANDROID_VERSION >= 23
      const uint32_t DEFAULT_USER_ID = 0;
      sp<ICameraService> gCameraService =
          interface_cast<ICameraService>(binder);
      int32_t args[1];
      args[0] = DEFAULT_USER_ID;
      int32_t event = 1;
      size_t length = 1;

      gCameraService->notifySystemEvent(event, args, length);
#endif
      mCamera =
          Camera::connect(0, /* clientPackageName */ String16("gonk.camera"),
                          Camera::USE_CALLING_UID);
    }

    if (mCamera != NULL) {
      mCamera->setPreviewTarget(aProducer);
      mPreferPreviewWidth = aPreferWidth;
      mPreferPreviewHeight = aPreferHeight;
      if (mVideoCallCallback) {
        FAKE_VIDEOCALL_PROVIDER_LOG("OnChangeCameraCapabilities");
        mVideoCallCallback->OnChangeCameraCapabilities(
            mVideoCallCameraCapabilities);
      }
    } else {
      FAKE_VIDEOCALL_PROVIDER_LOG("SetPreviewSurface without valid camera");
    }

    // Start preview
    if ((mCamera != NULL) && !mPreviewStarted) {
      SetCameraParameters();
      FAKE_VIDEOCALL_PROVIDER_LOG("startPreview");
      mCamera->startPreview();
      mPreviewStarted = true;
    }
  } else {
    StopPreview();
  }
  return NS_OK;
}

/* void setDisplaySurface (in AGraphicBuffProducer producer, in unsigned short
 * width, in unsigned short height); */
nsresult FakeVideoCallProvider::SetDisplaySurface(
    android::sp<android::IGraphicBufferProducer>& aProducer,
    uint16_t aPreferWidth, uint16_t aPreferHeight) {
  if (aProducer != NULL) {
    FAKE_VIDEOCALL_PROVIDER_LOG(
        "SetDisplaySurface aPreferWidth:%d, aPreferHeight:%d, "
        "mResultDisplayWidth:%d, mResultDisplayHeight:%d",
        aPreferWidth, aPreferHeight, mResultDisplayWidth, mResultDisplayHeight);
    // Do nothing, since we are TestDataSource"Camera", can only open 1 camera a
    // time.
    mFakeImageProducer = aProducer;
    if (mVideoCallCallback) {
      mVideoCallCallback->OnChangePeerDimensions(mResultDisplayWidth,
                                                 mResultDisplayHeight);
    }

    // Start sending fake images.
    if ((mFakeImageProducer != NULL) && !mDisplayStarted) {
      if (mFakeYUVFeederLooper == NULL) {
        mFakeYUVFeederLooper = new ALooper;
        mFakeYUVFeeder = new FakeYUVFeeder(this);
        mFakeYUVFeederLooper->registerHandler(mFakeYUVFeeder);
        mFakeYUVFeederLooper->setName("FakeImageLooper");
        mFakeYUVFeederLooper->start(false,  // runOnCallingThread
                                    false,  // canCallJava
                                    ANDROID_PRIORITY_AUDIO);
      }
      StartFakeImage();
      mDisplayStarted = true;
    }
  } else {
    StopDisplay();
  }

  return NS_OK;
}

/* void setDeviceOrientation (in unsigned short rotation); */
nsresult FakeVideoCallProvider::SetDeviceOrientation(uint16_t rotation) {
  // Not implement yet.
  return NS_OK;
}

/* void setZoom (in float value); */
nsresult FakeVideoCallProvider::SetZoom(float value) {
  // Not implement yet.
  return NS_OK;
}

/* void sendSessionModifyRequest (in nsIVideoCallProfile fromProfile, in
 * nsIVideoCallProfile toProfile); */
nsresult FakeVideoCallProvider::SendSessionModifyRequest(
    nsIVideoCallProfile* fromProfile, nsIVideoCallProfile* toProfile) {
  // Not implement yet.
  return NS_OK;
}

/* void sendSessionModifyResponse (in nsIVideoCallProfile responseProfile); */
nsresult FakeVideoCallProvider::SendSessionModifyResponse(
    nsIVideoCallProfile* responseProfile) {
  // Not implement yet.
  return NS_OK;
}

/* void requestCameraCapabilities (); */
nsresult FakeVideoCallProvider::RequestCameraCapabilities(void) {
  // Not implement yet.
  return NS_OK;
}

/* void registerCallback (in nsIVideoCallCallback callback); */
nsresult FakeVideoCallProvider::RegisterCallback(
    nsIVideoCallCallback* callback) {
  mVideoCallCallback = callback;
  return NS_OK;
}

/* void unregisterCallback (in nsIVideoCallCallback callback); */
nsresult FakeVideoCallProvider::UnregisterCallback(
    nsIVideoCallCallback* callback) {
  mVideoCallCallback = NULL;
  return NS_OK;
}

void FakeVideoCallProvider::SetCameraParameters() {
  FAKE_VIDEOCALL_PROVIDER_LOG("SetCameraParameters");
  if (mCamera != NULL) {
    CameraParameters params;
    // Initialize our camera configuration database.
    const String8 s1 = mCamera->getParameters();
    params.unflatten(s1);
    // Set preferred preview frame format.
    params.setPreviewFormat("yuv420sp");
    FAKE_VIDEOCALL_PROVIDER_LOG(
        "PushParameters: ResultWidth:%d ResultHeight:%d", mResultPreviewWidth,
        mResultPreviewHeight);
    params.setPreviewSize(mResultPreviewWidth, mResultPreviewHeight);
    // Set parameter back to camera.
    String8 s2 = params.flatten();
    FAKE_VIDEOCALL_PROVIDER_LOG("PushParameters:%s Line:%d", s2.string(),
                                __LINE__);
    mCamera->setParameters(s2);
  } else {
    FAKE_VIDEOCALL_PROVIDER_LOG("SetCameraParameters without valid camera");
  }
}

void FakeVideoCallProvider::StopPreview() {
  FAKE_VIDEOCALL_PROVIDER_LOG("StopPreview");
  // Stop Preview
  if (mCamera != NULL) {
    mCamera->stopPreview();
    mPreviewStarted = false;
    mCamera->disconnect();
    mCamera.clear();
  }
}

void FakeVideoCallProvider::StopDisplay() {
  FAKE_VIDEOCALL_PROVIDER_LOG("StopDisplay");
  // Stop Display
  mIsTestRunning = false;
  mDisplayStarted = false;

  if (mTestImage) {
    delete[] mTestImage;
    mTestImage = NULL;
  }

  if (mTestImage2) {
    delete[] mTestImage2;
    mTestImage2 = NULL;
  }
}

FakeVideoCallProvider::~FakeVideoCallProvider() {
  FAKE_VIDEOCALL_PROVIDER_LOG("~FakeVideoCallProvider");
  if (mCamera != NULL) {
    mCamera->disconnect();
    mCamera.clear();
  }

  if (mTestImage) {
    delete[] mTestImage;
    mTestImage = NULL;
  }

  if (mTestImage2) {
    delete[] mTestImage2;
    mTestImage2 = NULL;
  }
}

bool readYUVDataFromFile(const char* path, unsigned char* pYUVData, int size) {
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    return false;
  }
  fread(pYUVData, size, 1, fp);
  fclose(fp);
  return true;
}

nsresult FakeVideoCallProvider::StartFakeImage() {
  if (mTestANativeWindow == NULL) {
    if (mFakeImageProducer != NULL) {
      mTestANativeWindow =
          new android::Surface(mFakeImageProducer, /*controlledByApp*/ true);

      native_window_set_buffer_count(mTestANativeWindow.get(), 8);
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // Prepare test data
  if (!mTestImage) {
    int size = 176 * 144 * 1.5;
    mTestImage = new unsigned char[size];

    const char* path =
        "/mnt/media_rw/sdcard/tulips_yuv420_prog_planar_qcif.yuv";
    bool getResult = readYUVDataFromFile(path, mTestImage, size);
    if (!getResult) {
      memset(mTestImage, 120, size);
    }
  }

  if (!mTestImage2) {
    int size = 176 * 144 * 1.5;
    mTestImage2 = new unsigned char[size];

    const char* path =
        "/mnt/media_rw/sdcard/tulips_yvu420_inter_planar_qcif.yuv";
    bool getResult = readYUVDataFromFile(path, mTestImage2,
                                         size);  // get yuv data from file;
    if (!getResult) {
      memset(mTestImage2, 60, size);
    }
  }

  mIsTestRunning = true;
#if ANDROID_VERSION >= 23
  sp<AMessage> msg =
      new AMessage(FakeYUVFeeder::kWhatSendFakeImage, mFakeYUVFeeder);
#elif ANDROID_VERSION == 19
  sp<AMessage> msg =
      new AMessage(FakeYUVFeeder::kWhatSendFakeImage, mFakeYUVFeeder->id());
#endif
  msg->post();

  return NS_OK;
}

FakeYUVFeeder::FakeYUVFeeder(FakeVideoCallProvider* aFakeVideoCallProvider)
    : mFakeVideoCallProvider(aFakeVideoCallProvider) {}

FakeYUVFeeder::~FakeYUVFeeder() {}

void FakeYUVFeeder::onMessageReceived(const sp<AMessage>& msg) {
  switch (msg->what()) {
    case kWhatSendFakeImage: {
      SendFakeImage(mFakeVideoCallProvider);
      break;
    }
    default:
      // Do nothing
      return;
  }
}

void FakeYUVFeeder::SendFakeImage(
    FakeVideoCallProvider* aFakeVideoCallProvider) {
  if (!aFakeVideoCallProvider->mIsTestRunning) {
    return;
  }

  int err;
  int cropWidth = 176;
  int cropHeight = 144;

  int halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
  int bufWidth = (cropWidth + 1) & ~1;
  int bufHeight = (cropHeight + 1) & ~1;

  native_window_set_usage(
      aFakeVideoCallProvider->mTestANativeWindow.get(),
      GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN |
          GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);

  native_window_set_scaling_mode(
      aFakeVideoCallProvider->mTestANativeWindow.get(),
      NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);

  native_window_set_buffers_geometry(
      aFakeVideoCallProvider->mTestANativeWindow.get(), bufWidth, bufHeight,
      halFormat);

  ANativeWindowBuffer* buf;

  if ((err = native_window_dequeue_buffer_and_wait(
           aFakeVideoCallProvider->mTestANativeWindow.get(), &buf)) != 0) {
    return;
  }

  GraphicBufferMapper& mapper = GraphicBufferMapper::get();

  android::Rect bounds(cropWidth, cropHeight);

  void* dst;
  mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);

  if (aFakeVideoCallProvider->mTestImageIndex == 0) {
    aFakeVideoCallProvider->mTestImageIndex = 1;
    memcpy(dst, aFakeVideoCallProvider->mTestImage,
           cropWidth * cropHeight * 1.5);
  } else {
    aFakeVideoCallProvider->mTestImageIndex = 0;
    memcpy(dst, aFakeVideoCallProvider->mTestImage2,
           cropWidth * cropHeight * 1.5);
  }
  mapper.unlock(buf->handle);

  err = aFakeVideoCallProvider->mTestANativeWindow->queueBuffer(
      aFakeVideoCallProvider->mTestANativeWindow.get(), buf, -1);

  buf = NULL;

  // Next round
  usleep(100000);
#if ANDROID_VERSION >= 23
  sp<AMessage> msg = new AMessage(kWhatSendFakeImage, this);
#elif ANDROID_VERSION == 19
  sp<AMessage> msg = new AMessage(kWhatSendFakeImage, id());
#endif
  msg->post();
}
