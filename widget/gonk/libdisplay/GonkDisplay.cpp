/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if ANDROID_VERSION < 29
#  error "Only ANDROID_VERSION >= 29 devices are supported"
#endif

#include <cutils/properties.h>
#include <gui/Surface.h>
#include <gui/IProducerListener.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <suspend/autosuspend.h>
#include <ui/Fence.h>

#if ANDROID_VERSION >= 30
#  include <android/hardware/power/Mode.h>

#  include <binder/IServiceManager.h>
#endif

#include "FramebufferSurface.h"
#include "GonkDisplayP.h"
#include "mozilla/Preferences.h"

#ifdef LOG_TAG
#  undef LOG_TAG
#  define LOG_TAG "GonkDisplay"
#endif

#if ANDROID_VERSION >= 30
namespace power = android::hardware::power;
#endif

using android::BufferQueue;
using android::Fence;
using android::FloatRect;
using android::FramebufferSurface;
using android::IGraphicBufferConsumer;
using android::IProducerListener;
using android::Rect;
using android::Region;
using android::Surface;

#if ANDROID_VERSION >= 33
using android::to_string;
using DummyProducerListener = android::StubProducerListener;
#if ANDROID_VERSION > 33
using aidl::android::hardware::graphics::composer3::RefreshRateChangedDebugData;
#endif
#else
using android::DummyProducerListener;
#endif

/* global variables */
std::mutex hotplugMutex;
std::condition_variable hotplugCv;

class HWComposerCallback final : public HWC2::ComposerCallback {
 public:
  HWComposerCallback(HWC2::Device* aDevice) : mHwcDevice(aDevice) {}

#if ANDROID_VERSION >= 33
  void onComposerHalHotplug(HWC2::hal::HWDisplayId display,
                            HWC2::hal::Connection connection) override {
    OnHotplug(display, connection);
  }

  void onComposerHalRefresh(HWC2::hal::HWDisplayId display) override {
    OnRefresh(display);
  }

  void onComposerHalVsync(HWC2::hal::HWDisplayId display, int64_t timestamp,
                          std::optional<HWC2::hal::VsyncPeriodNanos>) override {
    OnVsync(display, timestamp);
  }

  void onComposerHalVsyncPeriodTimingChanged(
      HWC2::hal::HWDisplayId,
      const HWC2::hal::VsyncPeriodChangeTimeline&) override {}

  void onComposerHalSeamlessPossible(HWC2::hal::HWDisplayId) override {}

  void onComposerHalVsyncIdle(HWC2::hal::HWDisplayId) override {}

#if ANDROID_VERSION > 33
  void onRefreshRateChangedDebug(const RefreshRateChangedDebugData&) override {}
#endif
#else
  void onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
                       int64_t timestamp) override {
    OnVsync(display, timestamp);
  }

  void onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
                         HWC2::Connection connection) override {
    OnHotplug(display, connection);
  }

  void onRefreshReceived(int32_t sequenceId, hwc2_display_t display) override {
    OnRefresh(display);
  }
#endif

 private:
  void OnVsync(hwc2_display_t aDisplay, int64_t aTimestamp) {
    if (auto callback = mozilla::GetGonkDisplay()->getVsyncCallBack()) {
      callback(aDisplay, aTimestamp);
    }
  }

  void OnHotplug(hwc2_display_t aDisplay, HWC2::Connection aConnection) {
    ALOGI("HWComposerCallback::OnHotplug, %" PRIu64 " %d", aDisplay,
          (int)aConnection);
    {
      std::lock_guard<std::mutex> lock(hotplugMutex);
      mHwcDevice->onHotplug(aDisplay, aConnection);
    }
    hotplugCv.notify_all();
  }

  void OnRefresh(hwc2_display_t aDisplay) {
    ALOGI("HWComposerCallback::OnRefresh, %" PRIu64, aDisplay);
    if (auto callback = mozilla::GetGonkDisplay()->getInvalidateCallBack()) {
      callback();
    }
  }

  HWC2::Device* mHwcDevice = nullptr;
};

class GonkDisplayConfig {
 public:
#if ANDROID_VERSION >= 33
  static std::unique_ptr<GonkDisplayConfig> LoadActiveConfig(
      HWC2::Device* aHwcDevice) {
    using Config = android::Hwc2::Config;
    using Error = android::Hwc2::Error;
    using IComposerClient = android::Hwc2::IComposerClient;

    auto* composer = aHwcDevice->getComposer();
    auto displayId = aHwcDevice->getDefaultDisplayId();
    Config config;
    if (composer->getActiveConfig(displayId, &config) != Error::NONE) {
      ALOGE("getActiveConfig failed");
      return nullptr;
    }
    auto data = std::make_unique<GonkDisplayConfig>();
    (void)composer->getDisplayAttribute(
        displayId, config, IComposerClient::Attribute::WIDTH, &data->width);
    (void)composer->getDisplayAttribute(
        displayId, config, IComposerClient::Attribute::HEIGHT, &data->height);
    (void)composer->getDisplayAttribute(
        displayId, config, IComposerClient::Attribute::DPI_X, &data->dpiX);
    (void)composer->getDisplayAttribute(
        displayId, config, IComposerClient::Attribute::DPI_Y, &data->dpiY);
    (void)composer->getDisplayAttribute(
        displayId, config, IComposerClient::Attribute::VSYNC_PERIOD,
        &data->vsyncPeriod);
    return data;
  }
#else
  static std::unique_ptr<GonkDisplayConfig> LoadActiveConfig(
      HWC2::Device* aHwcDevice) {
    auto* hwcDisplay =
        aHwcDevice->getDisplayById(aHwcDevice->getDefaultDisplayId());
    std::shared_ptr<const HWC2::Display::Config> config;
    if (hwcDisplay->getActiveConfig(&config) != HWC2::Error::None) {
      ALOGE("getActiveConfig failed");
      return nullptr;
    }

    auto data = std::make_unique<GonkDisplayConfig>();
    data->width = config->getWidth();
    data->height = config->getHeight();
    data->dpiX = config->getDpiX();
    data->dpiY = config->getDpiY();
    data->vsyncPeriod = config->getVsyncPeriod();
    return data;
  }
#endif

  int32_t getWidth() { return width; }
  int32_t getHeight() { return height; }
  int32_t getDpiX() { return dpiX; }
  int32_t getDpiY() { return dpiY; }
  int32_t getVsyncPeriod() { return vsyncPeriod; }

 private:
  int32_t width = -1;
  int32_t height = -1;
  int32_t dpiX = -1;
  int32_t dpiY = -1;
  int32_t vsyncPeriod = -1;
};

namespace mozilla {

void HookSetVsyncAlwaysEnabled(bool aAlways);

GonkDisplayP::GonkDisplayP() {
  char serviceName[PROPERTY_VALUE_MAX] = {};
  property_get("debug.sf.hwc_service_name", serviceName, "default");
  ALOGI("Using HWComposer service: '%s'", serviceName);
  mHwcDevice = std::make_unique<HWC2::Device>(std::string(serviceName));
  mHwcCallback = std::make_unique<HWComposerCallback>(mHwcDevice.get());
  mHwcDevice->registerCallback(mHwcCallback.get(), 0);

  std::unique_lock<std::mutex> lock(hotplugMutex);
  HWC2::Display* hwcDisplay;
  while (!(hwcDisplay =
               mHwcDevice->getDisplayById(mHwcDevice->getDefaultDisplayId()))) {
    /* Wait at most 5s for hotplug events */
    hotplugCv.wait_for(lock, std::chrono::seconds(5));
  }
  hotplugMutex.unlock();
  assert(hwcDisplay);

  (void)hwcDisplay->setPowerMode(HWC2::PowerMode::On);
  mHwcDisplay = hwcDisplay;

  auto config = GonkDisplayConfig::LoadActiveConfig(mHwcDevice.get());

  char lcd_density_str[PROPERTY_VALUE_MAX] = "0";
  property_get("ro.sf.lcd_density", lcd_density_str, "0");
  int lcd_density = atoi(lcd_density_str);

  mEnableHWCPower = property_get_bool("persist.hwc.powermode", false);

  ALOGI("width: %d, height: %d, dpi: %d, lcd: %d, vsync: %d",
        config->getWidth(), config->getHeight(), config->getDpiX(), lcd_density,
        config->getVsyncPeriod());

  DisplayNativeData& dispData =
      mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY];
  if (config->getWidth() > 0) {
    dispData.mWidth = config->getWidth();
    dispData.mHeight = config->getHeight();
    dispData.mXdpi = (lcd_density > 0) ? lcd_density : config->getDpiX();
    dispData.mVsyncPeriod = config->getVsyncPeriod();
    /* The emulator actually reports RGBA_8888, but EGL doesn't return
     * any matching configuration. We force RGBX here to fix it. */
    /*TODO: need to discuss with vendor to check this format issue.*/
    dispData.mSurfaceformat = HAL_PIXEL_FORMAT_RGBA_8888;
  }
  mLayer = CreateLayer(config->getWidth(), config->getHeight());

  ALOGI("created native window\n");
  native_gralloc_initialize(0);

#if ANDROID_VERSION >= 30
  mPower = android::waitForVintfService<IPower>();
#else
  mPower = IPower::getService();
#endif

  if (mPower == nullptr) {
    ALOGE("Can't find IPower service...");
  }

  DisplayUtils displayUtils;
  displayUtils.type = DisplayUtils::MAIN;
  displayUtils.utils.hwcDisplay = mHwcDisplay;
  // disable mDispSurface by default to avoid updating frame during boot
  // animation is being played.
  CreateFramebufferSurface(mSTClient, mDispSurface, config->getWidth(),
                           config->getHeight(), dispData.mSurfaceformat,
                           displayUtils, false);

  mBootAnimLayer = CreateLayer(config->getWidth(), config->getHeight());

  CreateFramebufferSurface(mBootAnimSTClient, mBootAnimDispSurface,
                           config->getWidth(), config->getHeight(),
                           dispData.mSurfaceformat, displayUtils, true);

  {
    // mBootAnimSTClient is used by CPU directly via GonkDisplayP's
    // dequeueBuffer() / queueBuffer(). We connect it here for use
    // later or it will be failed to queue buffers.
    Surface* surface = static_cast<Surface*>(mBootAnimSTClient.get());
    static sp<IProducerListener> listener = new DummyProducerListener();
    surface->connect(NATIVE_WINDOW_API_CPU, listener);
  }

  uint32_t usage =
      GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;

  // Set this prop to turn on native framebuffer support for fb1,
  // such as external screen of flip phone.
  // ro.h5.display.fb1_backlightdev=__full_backlight_device_path__
  //
  // ex: Octans will set this prop in device/t2m/octans/octans.mk
  DisplayNativeData& extDispData =
      mDispNativeData[(uint32_t)DisplayType::DISPLAY_EXTERNAL];
  mExtFBDevice = NativeFramebufferDevice::Create();

  if (mExtFBDevice) {
    if (mExtFBDevice->Open()) {
      extDispData.mWidth = mExtFBDevice->mWidth;
      extDispData.mHeight = mExtFBDevice->mHeight;
      extDispData.mSurfaceformat = mExtFBDevice->mSurfaceformat;
      extDispData.mXdpi = mExtFBDevice->mXdpi;

      mExtFBDevice->EnableScreen(true);

      displayUtils.type = DisplayUtils::EXTERNAL;
      displayUtils.utils.extFBDevice = mExtFBDevice;
      CreateFramebufferSurface(mExtSTClient, mExtDispSurface,
                               extDispData.mWidth, extDispData.mHeight,
                               extDispData.mSurfaceformat, displayUtils, true);
      mExtSTClient->perform(mExtSTClient.get(), NATIVE_WINDOW_SET_BUFFER_COUNT,
                            2);
      mExtSTClient->perform(mExtSTClient.get(), NATIVE_WINDOW_SET_USAGE, usage);

      {
        // mExtSTClient is used by CPU directly via GonkDisplayP's
        // dequeueBuffer() / queueBuffer(). We connect it here for use
        // later or it will be failed to queue buffers.
        Surface* surface = static_cast<Surface*>(mExtSTClient.get());
        static sp<IProducerListener> listener = new DummyProducerListener();
        surface->connect(NATIVE_WINDOW_API_CPU, listener);
      }
    } else {
      delete mExtFBDevice;
      mExtFBDevice = nullptr;
    }
  }

  if (!mExtFBDevice) {
    // Set mExtFBEnabled to false if no support externl screen.
    mExtFBEnabled = false;
  }
}

GonkDisplayP::~GonkDisplayP() {}

void GonkDisplayP::CreateFramebufferSurface(sp<ANativeWindow>& nativeWindow,
                                            sp<DisplaySurface>& displaySurface,
                                            uint32_t width, uint32_t height,
                                            unsigned int format,
                                            DisplayUtils displayUtils,
                                            bool visibility) {
  sp<IGraphicBufferProducer> producer;
  sp<IGraphicBufferConsumer> consumer;
  BufferQueue::createBufferQueue(&producer, &consumer);

  displaySurface = new FramebufferSurface(width, height, format, consumer,
                                          displayUtils, visibility);
  nativeWindow = new Surface(producer, true);
}

void GonkDisplayP::CreateVirtualDisplaySurface(
    IGraphicBufferProducer* sink, sp<ANativeWindow>& nativeWindow,
    sp<DisplaySurface>& displaySurface) {
  /* TODO: implement VirtualDisplay*/
  (void)sink;
  (void)nativeWindow;
  (void)displaySurface;

  /* FIXME: bug 4036, fix the build error in libdisplay
  #if ANDROID_VERSION >= 19
      sp<VirtualDisplaySurface> virtualDisplay;
      virtualDisplay = new VirtualDisplaySurface(-1, aSink, producer, consumer,
  String8("VirtualDisplaySurface")); aDisplaySurface = virtualDisplay;
      aNativeWindow = new Surface(virtualDisplay);
  #endif*/
}

std::shared_ptr<HWC2::Layer> GonkDisplayP::CreateLayer(int32_t aWidth,
                                                       int32_t aHeight) {
#if ANDROID_VERSION >= 33
  auto expected = mHwcDisplay->createLayer();
  if (!expected.has_value()) {
    return nullptr;
  }
  auto layer = expected.value();
#else
  HWC2::Layer* layerPtr = nullptr;
  (void)mHwcDisplay->createLayer(&layerPtr);
  if (!layerPtr) {
    return nullptr;
  }
  // See OutputLayer::initialize(). The shared_ptr deleter should call
  // Display::destroyLayer().
  auto layer = std::shared_ptr<HWC2::Layer>(
      layerPtr, [display = mHwcDisplay](auto* aLayer) {
        (void)display->destroyLayer(aLayer);
      });
#endif
  auto compositionType = HWC2::Composition::Client;
  auto blendMode = HWC2::BlendMode::None;
  auto rect = Rect{0, 0, aWidth, aHeight};
  auto fRect = FloatRect(0.0f, 0.0f, aWidth, aHeight);

  (void)layer->setCompositionType(compositionType);
  (void)layer->setBlendMode(blendMode);
  (void)layer->setSourceCrop(fRect);
  (void)layer->setDisplayFrame(rect);
  (void)layer->setVisibleRegion(Region(rect));
  return layer;
}

void GonkDisplayP::SetEnabled(bool enabled) {
  android::Mutex::Autolock lock(mPrimaryScreenLock);
  if (enabled) {
    if (!mExtFBEnabled) {
      autosuspend_disable();
      if (mPower) {
#if ANDROID_VERSION >= 30
        ALOGI("Power change: display enabled=%d", enabled);
        mPower->setMode(power::Mode::INTERACTIVE, true);
        mPower->setMode(power::Mode::DISPLAY_INACTIVE, false);
#else
        mPower->setInteractive(true);
#endif
      }

      if (mEnableHWCPower) {
        SetHwcPowerMode(enabled);
      } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
      }
    }
    mFBEnabled = enabled;

    // enable vsync
    if (mEnabledCallback && !mExtFBEnabled) {
      mEnabledCallback(enabled);
    }
    if (Preferences::GetBool("gfx.vsync.force-enable", false)) {
      HookSetVsyncAlwaysEnabled(true);
    }
  } else {
    if (mEnabledCallback && !mExtFBEnabled) {
      mEnabledCallback(enabled);
    }

    if (!mExtFBEnabled) {
      if (mEnableHWCPower) {
        SetHwcPowerMode(enabled);
      } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
      }

      autosuspend_enable();
      if (mPower) {
#if ANDROID_VERSION >= 30
        ALOGI("Power change: display enabled=%d", enabled);
        mPower->setMode(power::Mode::INTERACTIVE, false);
        mPower->setMode(power::Mode::DISPLAY_INACTIVE, true);
#else
        mPower->setInteractive(false);
#endif
      }
    }
    mFBEnabled = enabled;
  }
}

int GonkDisplayP::TryLockScreen() {
  int ret = mPrimaryScreenLock.tryLock();
  return ret;
}

void GonkDisplayP::UnlockScreen() { mPrimaryScreenLock.unlock(); }

void GonkDisplayP::SetExtEnabled(bool enabled) {
  android::Mutex::Autolock lock(mPrimaryScreenLock);
  if (!mExtFBDevice) {
    return;
  }

  if (enabled) {
    if (!mFBEnabled) {
      autosuspend_disable();
      if (mPower) {
#if ANDROID_VERSION >= 30
        mPower->setMode(power::Mode::INTERACTIVE, true);
        mPower->setMode(power::Mode::DISPLAY_INACTIVE, false);
#else
        mPower->setInteractive(true);
#endif
      }

      SetHwcPowerMode(enabled);
    }
    mExtFBDevice->EnableScreen(enabled);
    mExtFBEnabled = enabled;

    if (mEnabledCallback && !mFBEnabled) {
      mEnabledCallback(enabled);
    }
  } else {
    if (mEnabledCallback && !mFBEnabled) {
      mEnabledCallback(enabled);
    }

    mExtFBDevice->EnableScreen(enabled);
    mExtFBEnabled = enabled;
    if (!mFBEnabled) {
      SetHwcPowerMode(enabled);

      autosuspend_enable();
      if (mPower) {
#if ANDROID_VERSION >= 30
        mPower->setMode(power::Mode::INTERACTIVE, false);
        mPower->setMode(power::Mode::DISPLAY_INACTIVE, true);
#else
        mPower->setInteractive(false);
#endif
      }
    }
  }
}

HWC2::Error GonkDisplayP::SetHwcPowerMode(bool enabled) {
  HWC2::PowerMode mode = (enabled ? HWC2::PowerMode::On : HWC2::PowerMode::Off);
  HWC2::Display* hwcDisplay =
      mHwcDevice->getDisplayById(mHwcDevice->getDefaultDisplayId());

  auto error = hwcDisplay->setPowerMode(mode);
  if (error != HWC2::Error::None) {
    ALOGE(
        "SetHwcPowerMode: Unable to set power mode %s for "
        "display %d: %s (%d)",
        to_string(mode).c_str(), HWC_DISPLAY_PRIMARY, to_string(error).c_str(),
        static_cast<int32_t>(error));
  }

  return error;
}

void GonkDisplayP::SetDisplayVisibility(bool visibility) {
  android::Mutex::Autolock lock(mPrimaryScreenLock);
  mDispSurface->setVisibility(visibility);
}

void GonkDisplayP::OnEnabled(OnEnabledCallbackType callback) {
  mEnabledCallback = callback;
}

void* GonkDisplayP::GetHWCDevice() { return mHwcDevice.get(); }

bool GonkDisplayP::IsExtFBDeviceEnabled() { return !!mExtFBDevice; }

bool GonkDisplayP::SwapBuffers(DisplayType displayType) {
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    // Should be called when composition rendering is complete for a frame.
    // Only HWC v1.0 needs this call.
    // HWC > v1.0 case, do not call compositionComplete().
    // mFBDevice is present only when HWC is v1.0.

    return Post(mDispSurface->lastHandle, mDispSurface->GetPrevDispAcquireFd(),
                DisplayType::DISPLAY_PRIMARY);
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      return Post(mExtDispSurface->lastHandle,
                  mExtDispSurface->GetPrevDispAcquireFd(),
                  DisplayType::DISPLAY_EXTERNAL);
    }

    return false;
  }

  return false;
}

bool GonkDisplayP::Post(buffer_handle_t buffer, int fence,
                        DisplayType displayType) {
  sp<Fence> fenceObj = new Fence(fence);
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    // UpdateDispSurface(0, EGL_NO_SURFACE);
    return true;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    // Only support fb1 for certain device, use hwc to control
    // external screen in general case.
    // update buffer on onFrameAvailable.
    return true;
  }

  return false;
}

ANativeWindowBuffer* GonkDisplayP::DequeueBuffer(DisplayType displayType) {
  // Check for bootAnim or normal display flow.
  sp<ANativeWindow> nativeWindow;
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    nativeWindow = !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      nativeWindow = mExtSTClient;
    }
  }

  if (!nativeWindow.get()) {
    return nullptr;
  }

  ANativeWindowBuffer* buffer;
  int fenceFd = -1;
  nativeWindow->dequeueBuffer(nativeWindow.get(), &buffer, &fenceFd);
  sp<Fence> fence(new Fence(fenceFd));
  fence->waitForever("GonkDisplay::DequeueBuffer");
  return buffer;
}

bool GonkDisplayP::QueueBuffer(ANativeWindowBuffer* buffer,
                               DisplayType displayType) {
  bool success = false;
  int error = DoQueueBuffer(buffer, displayType);

  sp<DisplaySurface> displaySurface;
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    displaySurface =
        !mBootAnimSTClient.get() ? mDispSurface : mBootAnimDispSurface;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      displaySurface = mExtDispSurface;
    }
  }

  if (!displaySurface.get()) {
    return false;
  }

  success = Post(displaySurface->lastHandle,
                 displaySurface->GetPrevDispAcquireFd(), displayType);

  return error == 0 && success;
}

int GonkDisplayP::DoQueueBuffer(ANativeWindowBuffer* buffer,
                                DisplayType displayType) {
  int error = 0;
  sp<ANativeWindow> nativeWindow;
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    nativeWindow = !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      nativeWindow = mExtSTClient;
    }
  }

  if (!nativeWindow.get()) {
    return error;
  }

  error = nativeWindow->queueBuffer(nativeWindow.get(), buffer, -1);

  return error;
}

void GonkDisplayP::UpdateDispSurface(EGLDisplay dpy, EGLSurface sur) {
  // not used now.
}

void GonkDisplayP::NotifyBootAnimationStopped() {
  if (mBootAnimSTClient.get()) {
    ALOGI("[%s] NotifyBootAnimationStopped \n", __func__);
    mBootAnimLayer = nullptr;
    mBootAnimSTClient = nullptr;
    mBootAnimDispSurface = nullptr;
  }

  // enable mDispSurface for updating DISPLAY_PRIMARY.
  SetDisplayVisibility(true);

  if (mExtSTClient.get()) {
    Surface* surface = static_cast<Surface*>(mExtSTClient.get());
    surface->disconnect(NATIVE_WINDOW_API_CPU);
  }
}

GonkDisplay::NativeData GonkDisplayP::GetNativeData(
    DisplayType displayType, IGraphicBufferProducer* sink) {
  NativeData data;

  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    data.mNativeWindow = mSTClient;
    data.mDisplaySurface = mDispSurface;
    data.mXdpi = mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY].mXdpi;
    data.mComposer2DSupported = true;
    data.mVsyncSupported = true;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      data.mNativeWindow = mExtSTClient;
      data.mDisplaySurface = mExtDispSurface;
      data.mXdpi =
          mDispNativeData[(uint32_t)DisplayType::DISPLAY_EXTERNAL].mXdpi;
      data.mComposer2DSupported = false;
      data.mVsyncSupported = false;
    }
  } else if (displayType == DisplayType::DISPLAY_VIRTUAL) {
    data.mXdpi = mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY].mXdpi;
    CreateVirtualDisplaySurface(sink, data.mNativeWindow, data.mDisplaySurface);
  }

  return data;
}

sp<ANativeWindow> GonkDisplayP::GetSurface(DisplayType displayType) {
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    return mSTClient ? mSTClient : nullptr;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    return mExtSTClient ? mExtSTClient : nullptr;
  }

  return nullptr;
}

sp<GraphicBuffer> GonkDisplayP::GetFrameBuffer(DisplayType displayType) {
  if (displayType == DisplayType::DISPLAY_PRIMARY) {
    return mDispSurface ? mDispSurface->GetCurrentFrameBuffer() : nullptr;
  } else if (displayType == DisplayType::DISPLAY_EXTERNAL) {
    return mExtDispSurface ? mExtDispSurface->GetCurrentFrameBuffer() : nullptr;
  }

  return nullptr;
}

GonkDisplay* GetGonkDisplay() {
  static GonkDisplay* sGonkDisplay = nullptr;
  static android::Mutex sMutex;

  android::Mutex::Autolock lock(sMutex);
  if (!sGonkDisplay) {
    sGonkDisplay = new GonkDisplayP();
  }
  return sGonkDisplay;
}

}  // namespace mozilla
