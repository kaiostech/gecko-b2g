/* Copyright 2012 Mozilla Foundation and Mozilla contributors
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

#include "android/log.h"
#include "GLContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/Hal.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/Services.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/widget/ScreenManager.h"
#include "libdisplay/BootAnimation.h"
#include "libdisplay/DisplaySurface.h"
#include "libdisplay/GonkDisplay.h"
#include "nsAppShell.h"
#include "nsIDisplayInfo.h"
#include "nsIObserverService.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsUserIdleService.h"
#include "nsWindow.h"
#include "ScreenHelperGonk.h"
#include "VsyncSource.h"
// TODO: FIXME: #include "HwcComposer2D.h"
//#include "pixelflinger/format.h"

#define LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "nsScreenGonk", ##args)
#define LOGW(args...) \
  __android_log_print(ANDROID_LOG_WARN, "nsScreenGonk", ##args)
#define LOGE(args...) \
  __android_log_print(ANDROID_LOG_ERROR, "nsScreenGonk", ##args)
#define SLOGI(...)                                                         \
  ((void)__android_log_buf_print(LOG_ID_SYSTEM, ANDROID_LOG_INFO, "Gecko", \
                                 __VA_ARGS__))

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::widget;

#include "NativeGrallocT.h"

class ScreenOnOffEvent : public mozilla::Runnable {
 public:
  explicit ScreenOnOffEvent(bool on)
      : mozilla::Runnable("ScreenOnOffEvent"), mIsOn(on) {}

  NS_IMETHOD Run() {
    // Notify observers that the screen state has just changed.
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(nullptr, "screen-state-changed",
                                       mIsOn ? u"on" : u"off");
    }

    RefPtr<nsScreenGonk> screen = ScreenHelperGonk::GetPrimaryScreen();
    const nsTArray<nsWindow*>& windows = screen->GetTopWindows();

    for (uint32_t i = 0; i < windows.Length(); i++) {
      nsWindow* win = windows[i];

      if (nsIWidgetListener* listener = win->GetWidgetListener()) {
        listener->SizeModeChanged(mIsOn ? nsSizeMode_Fullscreen
                                        : nsSizeMode_Minimized);
      }
    }

    return NS_OK;
  }

 private:
  bool mIsOn;
};

static void displayEnabledCallback(bool enabled) {
  ScreenHelperGonk* screenHelper = ScreenHelperGonk::GetSingleton();
  screenHelper->DisplayEnabled(enabled);
}

static uint32_t SurfaceFormatToColorDepth(int32_t aSurfaceFormat) {
  switch (aSurfaceFormat) {
    case HAL_PIXEL_FORMAT_RGB_565:
      return 16;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return 32;
  }
  return 24;  // GGL_PIXEL_FORMAT_RGBX_8888
}

// nsScreenGonk.cpp
NS_IMPL_ISUPPORTS(nsScreenGonk, nsIScreen)

NS_IMETHODIMP
nsScreenGonk::GetRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                                int32_t* outWidth, int32_t* outHeight) {
  return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsScreenGonk::GetAvailRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                                     int32_t* outWidth, int32_t* outHeight) {
  return GetAvailRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsScreenGonk::GetContentsScaleFactor(double* aContentsScaleFactor) {
  *aContentsScaleFactor = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetDefaultCSSScaleFactor(double* aScaleFactor) {
  *aScaleFactor = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetDpi(float* aDPI) {
  *aDPI = 96;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetColorGamut(mozilla::dom::ScreenColorGamut * aColorGamut) {
  *aColorGamut = dom::ScreenColorGamut::Srgb;
  return NS_OK;
}

nsScreenGonk::nsScreenGonk(uint32_t aId, DisplayType aDisplayType,
                           const GonkDisplay::NativeData& aNativeData,
                           NotifyDisplayChangedEvent aEventVisibility)
    : mId(aId),
      mEventVisibility(aEventVisibility),
      mNativeWindow(aNativeData.mNativeWindow),
      mDpi(aNativeData.mXdpi),
      mScreenRotation(ROTATION_0),
      mPhysicalScreenRotation(ROTATION_0),
      mRefreshRate(60),
      mDisplaySurface(aNativeData.mDisplaySurface),
      mComposer2DSupported(aNativeData.mComposer2DSupported),
      mVsyncSupported(aNativeData.mVsyncSupported),
      mIsMirroring(false),
      mDisplayType(aDisplayType),
      mEGLDisplay(EGL_NO_DISPLAY),
      mEGLSurface(EGL_NO_SURFACE),
      mGLContext(nullptr),
      mFramebuffer(nullptr),
      mMappedBuffer(nullptr) {
  if (mDpi == 0) {
    NS_WARNING("aNativeData.mXdpi should not be 0!! Defaulting to 96.0");
    mDpi = 96.0;
  }

  // If the property persist.b2g.dpi is set, use that for the dpi value.
  char propValue[PROPERTY_VALUE_MAX];
  property_get("persist.b2g.dpi", propValue, "");
  if (strlen(propValue)) {
    auto val = strtof(propValue, nullptr);
    if (val > 0) {
      // val is 0 in case of conversion errors and can also be a valid strtof()
      // but that's not a usable dpi so we don't have to check errno.
      mDpi = val;
    }
  }

  if (mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_WIDTH,
                           &mVirtualBounds.width) ||
      mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_HEIGHT,
                           &mVirtualBounds.height) ||
      mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_FORMAT,
                           &mSurfaceFormat)) {
    MOZ_CRASH("Failed to get native window size, aborting...");
  }

  mNaturalBounds = mVirtualBounds;

  if (IsPrimaryScreen()) {
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.sf.hwrotation", propValue, "0");
    mPhysicalScreenRotation = atoi(propValue) / 90;
  }

  mColorDepth = SurfaceFormatToColorDepth(mSurfaceFormat);
}

static void ReleaseGLContextSync(mozilla::gl::GLContext* aGLContext) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  aGLContext->Release();
}

nsScreenGonk::~nsScreenGonk() {
  // Release GLContext on compositor thread
  if (mGLContext) {
    CompositorThread()->Dispatch(
        NewRunnableFunction("nsScreenGonk::~nsScreenGonk",
                            &ReleaseGLContextSync, mGLContext.forget().take()));
    mGLContext = nullptr;
  }
}

bool nsScreenGonk::IsPrimaryScreen() {
  return mDisplayType == DisplayType::DISPLAY_PRIMARY;
}

NS_IMETHODIMP
nsScreenGonk::GetId(uint32_t* outId) {
  *outId = mId;
  return NS_OK;
}

uint32_t nsScreenGonk::GetId() { return mId; }

NotifyDisplayChangedEvent nsScreenGonk::GetEventVisibility() {
  return mEventVisibility;
}

NS_IMETHODIMP
nsScreenGonk::GetRect(int32_t* outLeft, int32_t* outTop, int32_t* outWidth,
                      int32_t* outHeight) {
  *outLeft = mVirtualBounds.x;
  *outTop = mVirtualBounds.y;

  *outWidth = mVirtualBounds.width;
  *outHeight = mVirtualBounds.height;

  return NS_OK;
}

LayoutDeviceIntRect nsScreenGonk::GetRect() { return mVirtualBounds; }

NS_IMETHODIMP
nsScreenGonk::GetAvailRect(int32_t* outLeft, int32_t* outTop, int32_t* outWidth,
                           int32_t* outHeight) {
  return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsScreenGonk::GetPixelDepth(int32_t* aPixelDepth) {
  // XXX: this should actually return 32 when we're using 24-bit
  // color, because we use RGBX.
  *aPixelDepth = mColorDepth;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetColorDepth(int32_t* aColorDepth) {
  *aColorDepth = mColorDepth;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetRotation(uint32_t* aRotation) {
  *aRotation = mScreenRotation;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetRefreshRate(int32_t* aRefreshRate) {
  *aRefreshRate = mRefreshRate;
  return NS_OK;
}

float nsScreenGonk::GetDpi() { return mDpi; }

int32_t nsScreenGonk::GetSurfaceFormat() { return mSurfaceFormat; }

ANativeWindow* nsScreenGonk::GetNativeWindow() { return mNativeWindow.get(); }

NS_IMETHODIMP
nsScreenGonk::SetRotation(uint32_t aRotation) {
  if (!(aRotation <= ROTATION_270)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (mScreenRotation == aRotation) {
    return NS_OK;
  }

  mScreenRotation = aRotation;
  uint32_t rotation = EffectiveScreenRotation();
  if (rotation == ROTATION_90 || rotation == ROTATION_270) {
    mVirtualBounds =
        LayoutDeviceIntRect(0, 0, mNaturalBounds.height, mNaturalBounds.width);
  } else {
    mVirtualBounds = mNaturalBounds;
  }

  nsAppShell::NotifyScreenRotation();

  for (unsigned int i = 0; i < mTopWindows.Length(); i++) {
    mTopWindows[i]->Resize(mVirtualBounds.width, mVirtualBounds.height, true);
  }
  return NS_OK;
}

LayoutDeviceIntRect nsScreenGonk::GetNaturalBounds() { return mNaturalBounds; }

uint32_t nsScreenGonk::EffectiveScreenRotation() {
  return (mScreenRotation + mPhysicalScreenRotation) % (360 / 90);
}

// NB: This isn't gonk-specific, but gonk is the only widget backend
// that does this calculation itself, currently.
static hal::ScreenOrientation ComputeOrientation(
    uint32_t aRotation, const LayoutDeviceIntSize& aScreenSize) {
  bool naturallyPortrait = (aScreenSize.height > aScreenSize.width);
  switch (aRotation) {
    case ROTATION_0:
      return (naturallyPortrait ? hal::ScreenOrientation::PortraitPrimary
                                : hal::ScreenOrientation::LandscapePrimary);
    case ROTATION_90:
      // Arbitrarily choosing 90deg to be primary "unnatural"
      // rotation.
      return (naturallyPortrait ? hal::ScreenOrientation::LandscapePrimary
                                : hal::ScreenOrientation::PortraitPrimary);
    case ROTATION_180:
      return (naturallyPortrait ? hal::ScreenOrientation::PortraitSecondary
                                : hal::ScreenOrientation::LandscapeSecondary);
    case ROTATION_270:
      return (naturallyPortrait ? hal::ScreenOrientation::LandscapeSecondary
                                : hal::ScreenOrientation::PortraitSecondary);
    default:
      MOZ_CRASH("Gonk screen must always have a known rotation");
  }
  return hal::ScreenOrientation::PortraitPrimary;
}

static uint16_t RotationToAngle(uint32_t aRotation) {
  uint16_t angle = 90 * aRotation;
  MOZ_ASSERT(angle == 0 || angle == 90 || angle == 180 || angle == 270);
  return angle;
}

void nsScreenGonk::RegisterWindow(nsWindow* aWindow) {
  mTopWindows.AppendElement(aWindow);
}

void nsScreenGonk::UnregisterWindow(nsWindow* aWindow) {
  mTopWindows.RemoveElement(aWindow);
}

void nsScreenGonk::BringToTop(nsWindow* aWindow) {
  mTopWindows.RemoveElement(aWindow);
  mTopWindows.InsertElementAt(0, aWindow);
}

static gralloc_module_t const* gralloc_module() {
  hw_module_t const* module;
  if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
    return nullptr;
  }
  return reinterpret_cast<gralloc_module_t const*>(module);
}

static SurfaceFormat HalFormatToSurfaceFormat(int aHalFormat) {
  switch (aHalFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
      // Needs RB swap
      return SurfaceFormat::B8G8R8A8;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      // Needs RB swap
      return SurfaceFormat::B8G8R8X8;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return SurfaceFormat::B8G8R8A8;
    case HAL_PIXEL_FORMAT_RGB_565:
      return SurfaceFormat::R5G6B5_UINT16;
    default:
      MOZ_CRASH("Unhandled HAL pixel format");
      return SurfaceFormat::UNKNOWN;  // not reached
  }
}

static bool NeedsRBSwap(int aHalFormat) {
  switch (aHalFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return true;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return true;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return false;
    case HAL_PIXEL_FORMAT_RGB_565:
      return false;
    default:
      MOZ_CRASH("Unhandled HAL pixel format");
      return false;  // not reached
  }
}

already_AddRefed<DrawTarget> nsScreenGonk::StartRemoteDrawing() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(!mFramebuffer);
  MOZ_ASSERT(!mMappedBuffer);

  mFramebuffer = DequeueBuffer();
  int width = mFramebuffer->width, height = mFramebuffer->height;
  if (NativeGralloc::getInstance().lock(
          mFramebuffer->handle,
          GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN |
              GRALLOC_USAGE_HW_FB,
          0, 0, width, height, reinterpret_cast<void**>(&mMappedBuffer))) {
    EndRemoteDrawing();
    return nullptr;
  }
  SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
  mFramebufferTarget = Factory::CreateDrawTargetForData(
      BackendType::SKIA, mMappedBuffer, IntSize(width, height),
      mFramebuffer->stride * gfx::BytesPerPixel(format), format);
  if (!mFramebufferTarget) {
    MOZ_CRASH("nsWindow::StartRemoteDrawing failed in CreateDrawTargetForData");
  }
  if (!mBackBuffer || mBackBuffer->GetSize() != mFramebufferTarget->GetSize() ||
      mBackBuffer->GetFormat() != mFramebufferTarget->GetFormat()) {
    mBackBuffer = mFramebufferTarget->CreateSimilarDrawTarget(
        mFramebufferTarget->GetSize(), mFramebufferTarget->GetFormat());
  }
  RefPtr<DrawTarget> buffer(mBackBuffer);
  return buffer.forget();
}

void nsScreenGonk::EndRemoteDrawing() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (mFramebufferTarget && mFramebuffer) {
    IntSize size = mFramebufferTarget->GetSize();
    Rect rect(0, 0, size.width, size.height);
    RefPtr<SourceSurface> source = mBackBuffer->Snapshot();
    mFramebufferTarget->DrawSurface(source, rect, rect);

    // Convert from BGR to RGB
    // XXX this is a temporary solution. It consumes extra cpu cycles,
    // it should not be used on product device.
    if (NeedsRBSwap(GetSurfaceFormat())) {
      LOGE("Very slow composition path, it should not be used on product!!!");
      SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
      gfxUtils::ConvertBGRAtoRGBA(mMappedBuffer,
                                  mFramebuffer->stride * mFramebuffer->height *
                                      gfx::BytesPerPixel(format));
    }
  }
  if (mMappedBuffer) {
    MOZ_ASSERT(mFramebuffer);
    NativeGralloc::getInstance().unlock(mFramebuffer->handle);
    mMappedBuffer = nullptr;
  }
  if (mFramebuffer) {
    QueueBuffer(mFramebuffer);
  }
  mFramebuffer = nullptr;
  mFramebufferTarget = nullptr;
}

ANativeWindowBuffer* nsScreenGonk::DequeueBuffer() {
  ANativeWindowBuffer* buf = nullptr;
  int fenceFd = -1;
  mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf, &fenceFd);
  android::sp<android::Fence> fence(new android::Fence(fenceFd));
  return buf;
}

bool nsScreenGonk::QueueBuffer(ANativeWindowBuffer* buf) {
  int ret = mNativeWindow->queueBuffer(mNativeWindow.get(), buf, -1);
  return ret == 0;
}

nsresult nsScreenGonk::MakeSnapshot(ANativeWindowBuffer* aBuffer) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aBuffer);

  layers::CompositorBridgeParent* compositorParent = mCompositorBridgeParent;
  if (!compositorParent) {
    return NS_ERROR_FAILURE;
  }

  int width = aBuffer->width, height = aBuffer->height;
  uint8_t* mappedBuffer = nullptr;
  if (NativeGralloc::getInstance().lock(
          aBuffer->handle,
          GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0,
          width, height, reinterpret_cast<void**>(&mappedBuffer))) {
    return NS_ERROR_FAILURE;
  }

  SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
  RefPtr<DrawTarget> mTarget = Factory::CreateDrawTargetForData(
      BackendType::CAIRO, mappedBuffer, IntSize(width, height),
      aBuffer->stride * gfx::BytesPerPixel(format), format);
  if (!mTarget) {
    return NS_ERROR_FAILURE;
  }

  // Convert from BGR to RGB
  // XXX this is a temporary solution. It consumes extra cpu cycles,
  if (NeedsRBSwap(GetSurfaceFormat())) {
    LOGE("Slow path of making Snapshot!!!");
    SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
    gfxUtils::ConvertBGRAtoRGBA(
        mappedBuffer,
        aBuffer->stride * aBuffer->height * gfx::BytesPerPixel(format));
    mappedBuffer = nullptr;
  }
  NativeGralloc::getInstance().unlock(aBuffer->handle);

  return NS_OK;
}

void nsScreenGonk::SetCompositorBridgeParent(
    layers::CompositorBridgeParent* aCompositorBridgeParent) {
  MOZ_ASSERT(NS_IsMainThread());
  mCompositorBridgeParent = aCompositorBridgeParent;
}

android::DisplaySurface* nsScreenGonk::GetDisplaySurface() {
  return mDisplaySurface.get();
}

int nsScreenGonk::GetPrevDispAcquireFd() {
  if (!mDisplaySurface.get()) {
    return -1;
  }
  return mDisplaySurface->GetPrevDispAcquireFd();
}

bool nsScreenGonk::IsComposer2DSupported() { return mComposer2DSupported; }

bool nsScreenGonk::IsVsyncSupported() { return mVsyncSupported; }

DisplayType nsScreenGonk::GetDisplayType() { return mDisplayType; }

void nsScreenGonk::SetEGLInfo(hwc_display_t aDisplay, hwc_surface_t aSurface,
                              gl::GLContext* aGLContext) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mEGLDisplay = aDisplay;
  mEGLSurface = aSurface;
  mGLContext = aGLContext;
}

hwc_display_t nsScreenGonk::GetEGLDisplay() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mEGLDisplay;
}

hwc_surface_t nsScreenGonk::GetEGLSurface() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mEGLSurface;
}

already_AddRefed<mozilla::gl::GLContext> nsScreenGonk::GetGLContext() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<mozilla::gl::GLContext> glContext = mGLContext;
  return glContext.forget();
}

static void UpdateMirroringWidgetSync(
    nsMainThreadPtrHandle<nsScreenGonk>&& aScreen, nsWindow* aWindow) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  already_AddRefed<nsWindow> window(aWindow);
  aScreen->UpdateMirroringWidget(window);
}

bool nsScreenGonk::EnableMirroring() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsPrimaryScreen());

  RefPtr<nsScreenGonk> primaryScreen = ScreenHelperGonk::GetPrimaryScreen();
  NS_ENSURE_TRUE(primaryScreen, false);

  bool ret = primaryScreen->SetMirroringScreen(this);
  NS_ENSURE_TRUE(ret, false);

  // Create a widget for mirroring
  InitData initData;
  initData.mScreenId = mId;
  RefPtr<nsWindow> window = new nsWindow();
  window->Create(nullptr, nullptr, mNaturalBounds, &initData);
  MOZ_ASSERT(static_cast<nsWindow*>(window)->GetScreen() == this);

  // Update mMirroringWidget on compositor thread
  nsMainThreadPtrHandle<nsScreenGonk> primary =
      nsMainThreadPtrHandle<nsScreenGonk>(
          new nsMainThreadPtrHolder<nsScreenGonk>(
              "nsScreenGonk::EnableMirroring", primaryScreen, false));
  CompositorThread()->Dispatch(NewRunnableFunction(
      "nsScreenGonk::EnableMirroring", &UpdateMirroringWidgetSync, primary,
      window.forget().take()));

  mIsMirroring = true;
  return true;
}

bool nsScreenGonk::DisableMirroring() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsPrimaryScreen());

  mIsMirroring = false;
  RefPtr<nsScreenGonk> primaryScreen = ScreenHelperGonk::GetPrimaryScreen();
  NS_ENSURE_TRUE(primaryScreen, false);

  bool ret = primaryScreen->ClearMirroringScreen(this);
  NS_ENSURE_TRUE(ret, false);

  // Update mMirroringWidget on compositor thread
  nsMainThreadPtrHandle<nsScreenGonk> primary =
      nsMainThreadPtrHandle<nsScreenGonk>(
          new nsMainThreadPtrHolder<nsScreenGonk>(
              "nsScreenGonk::DisableMirroring", primaryScreen, false));
  CompositorThread()->Dispatch(
      NewRunnableFunction("nsScreenGonk::DisableMirroring",
                          &UpdateMirroringWidgetSync, primary, nullptr));
  return true;
}

bool nsScreenGonk::SetMirroringScreen(nsScreenGonk* aScreen) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsPrimaryScreen());

  if (mMirroringScreen) {
    return false;
  }
  mMirroringScreen = aScreen;
  return true;
}

bool nsScreenGonk::ClearMirroringScreen(nsScreenGonk* aScreen) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsPrimaryScreen());

  if (mMirroringScreen != aScreen) {
    return false;
  }
  mMirroringScreen = nullptr;
  return true;
}

void nsScreenGonk::UpdateMirroringWidget(already_AddRefed<nsWindow>& aWindow) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(IsPrimaryScreen());

  if (mMirroringWidget) {
    nsCOMPtr<nsIWidget> widget = mMirroringWidget.forget();
    NS_ReleaseOnMainThread("nsScreenGonk::UpdateMirroringWidget",
                           widget.forget());
  }
  mMirroringWidget = aWindow;
}

nsWindow* nsScreenGonk::GetMirroringWidget() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(IsPrimaryScreen());

  return mMirroringWidget;
}

// A concrete class as a subject for 'display-changed' observer event.
class DisplayInfo : public nsIDisplayInfo {
 public:
  NS_DECL_ISUPPORTS

  DisplayInfo(uint32_t aId, bool aConnected)
      : mId(aId), mConnected(aConnected) {}

  NS_IMETHODIMP GetId(int32_t* aId) {
    *aId = mId;
    return NS_OK;
  }

  NS_IMETHODIMP GetConnected(bool* aConnected) {
    *aConnected = mConnected;
    return NS_OK;
  }

 private:
  virtual ~DisplayInfo() {}

  uint32_t mId;
  bool mConnected;
};

NS_IMPL_ISUPPORTS(DisplayInfo, nsIDisplayInfo, nsISupports)

class NotifyTask : public mozilla::Runnable {
 public:
  NotifyTask(uint32_t aId, bool aConnected)
      : mozilla::Runnable("NotifyTask"),
        mDisplayInfo(new DisplayInfo(aId, aConnected)) {}

  NS_IMETHOD Run() {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(mDisplayInfo, "display-changed", nullptr);
    }

    return NS_OK;
  }

 private:
  RefPtr<DisplayInfo> mDisplayInfo;
};

void NotifyDisplayChange(uint32_t aId, bool aConnected) {
  NS_DispatchToMainThread(new NotifyTask(aId, aConnected));
}

namespace mozilla {
namespace widget {

static ScreenHelperGonk* gHelper = nullptr;

ScreenHelperGonk::ScreenHelperGonk()
    : mInitialized(false), mDisplayEnabled(hal::GetScreenEnabled()) {
  char propValue[PROPERTY_VALUE_MAX];
  property_get("ro.build.type", propValue, NULL);
  if (strcmp(propValue, "user") != 0) {
    SLOGI("============ B2G device information ============");

    property_get("ro.build.kaios_uid", propValue, NULL);
    SLOGI("Build UID         = %s", propValue);

    property_get("ro.build.description", propValue, NULL);
    SLOGI("Build Description = %s", propValue);

    property_get("ro.bootloader", propValue, NULL);
    SLOGI("Bootloader        = %s", propValue);

    nsCString osVersion(
        MOZ_STRINGIFY(MOZ_B2G_OS_NAME) " " MOZ_STRINGIFY(MOZ_B2G_VERSION));
    SLOGI("OS Version        = %s", osVersion.get());

    SLOGI("==================================================");
  }

  LOGE("Setting the global pointer");
  // Generic
  MOZ_ASSERT(!gHelper);
  gHelper = this;

  LOGE("Building the events");
  mScreenOnEvent = new ScreenOnOffEvent(true);
  mScreenOffEvent = new ScreenOnOffEvent(false);
  LOGE("Getting the gonk display");
  GetGonkDisplay()->OnEnabled(displayEnabledCallback);

  LOGE("Refreshing screens");
  Refresh();

  LOGE("Notifying screen initialization");
  nsAppShell::NotifyScreenInitialized();
  mInitialized = true;
  LOGE("Done");
}

ScreenHelperGonk::~ScreenHelperGonk() { gHelper = nullptr; }

/* static */ ScreenHelperGonk* ScreenHelperGonk::GetSingleton() {
  return gHelper;
}

/* static */ already_AddRefed<nsScreenGonk>
ScreenHelperGonk::GetPrimaryScreen() {
  return GetScreenGonk(GetIdFromType(DisplayType::DISPLAY_PRIMARY));
}

/* static */
already_AddRefed<nsScreenGonk> ScreenHelperGonk::GetScreenGonk(
    uint32_t aScreenId) {
  ScreenHelperGonk* screenHelper = ScreenHelperGonk::GetSingleton();
  RefPtr<nsScreenGonk> screen = screenHelper->mScreenGonks.Get(aScreenId);
  return screen.forget();
}

already_AddRefed<Screen> ScreenHelperGonk::MakeScreen(
    uint32_t id, NotifyDisplayChangedEvent aEventVisibility) {
  MOZ_ASSERT(XRE_IsParentProcess());
  DisplayType displayType = (DisplayType)id;
  NS_ENSURE_TRUE(!IsScreenConnected(id), nullptr);
  GonkDisplay::NativeData nativeData =

      GetGonkDisplay()->GetNativeData(displayType, nullptr);
  RefPtr<nsScreenGonk> screengonk =
      new nsScreenGonk(id, displayType, nativeData, aEventVisibility);
  mScreenGonks.InsertOrUpdate(id, screengonk);

  if (aEventVisibility == NotifyDisplayChangedEvent::Observable) {
    NotifyDisplayChange(id, true);
  }

// TODO: FIXME
#if 0
  // By default, non primary screen does mirroring.
  if (aDisplayType != DisplayType::DISPLAY_PRIMARY) {
      screen->EnableMirroring();
  }

  VsyncSource::VsyncType vsyncType = (screen->IsVsyncSupported()) ?
    VsyncSource::VsyncType::HARDWARE_VYSNC :
    VsyncSource::VsyncType::SORTWARE_VSYNC;

  gfxPlatform::GetPlatform()->GetHardwareVsync()->AddDisplay(id, vsyncType);
#endif

  LayoutDeviceIntRect bounds = screengonk->GetNaturalBounds();
  int32_t depth;

  screengonk->GetColorDepth(&depth);

  int32_t refreshRate;
  screengonk->GetRefreshRate(&refreshRate);

  float density = 160.0f;  // FIXME: This is the default density
  float dpi = screengonk->GetDpi();

  RefPtr<Screen> screen = new Screen(bounds, bounds, depth, depth, refreshRate,
                                     DesktopToLayoutDeviceScale(density),
                                     CSSToLayoutDeviceScale(1.0f), dpi,
                                     Screen::IsPseudoDisplay::No);
  return screen.forget();
}

void ScreenHelperGonk::Refresh() {
  AutoTArray<RefPtr<Screen>, 1> screenList;
  uint32_t id = GetIdFromType(DisplayType::DISPLAY_PRIMARY);
  RefPtr<Screen> screen = mScreens.Get(id);
  if (!screen) {
    screen = MakeScreen(id);
    if (screen) {
      mScreens.InsertOrUpdate(id, screen);
    }
  }

  for (auto iter = mScreens.ConstIter(); !iter.Done(); iter.Next()) {
    screenList.AppendElement(iter.Data());
  }

  ScreenManager& manager = ScreenManager::GetSingleton();
  manager.Refresh(std::move(screenList));
}

void ScreenHelperGonk::AddScreen(uint32_t aScreenId, DisplayType aDisplayType,
                                 LayoutDeviceIntRect aRect, float aDensity,
                                 NotifyDisplayChangedEvent aEventVisibility) {
  MOZ_ASSERT(aScreenId > 0);
  MOZ_ASSERT(!mScreens.Get(aScreenId, nullptr));

  RefPtr<Screen> screen = MakeScreen(aScreenId, aEventVisibility);

  mScreens.InsertOrUpdate(aScreenId, screen);
  Refresh();
}

void ScreenHelperGonk::RemoveScreen(uint32_t aScreenId) {
  NotifyDisplayChangedEvent eventVisibility =
      NotifyDisplayChangedEvent::Observable;

  mScreenGonks.Remove(aScreenId);
  mScreens.Remove(aScreenId);

  if (eventVisibility == NotifyDisplayChangedEvent::Observable) {
    NotifyDisplayChange(aScreenId, false);
  }

  Refresh();
}

/* static */ uint32_t ScreenHelperGonk::GetIdFromType(
    DisplayType aDisplayType) {
  // This is the only place where we make the assumption that
  // display type is equivalent to screen id.

  // Bug 1138287 will address the conversion from type to id.
  return (uint32_t)aDisplayType;
}

already_AddRefed<Screen> ScreenHelperGonk::ScreenForId(uint32_t aScreenId) {
  RefPtr<Screen> screen = mScreens.Get(aScreenId);
  return screen.forget();
}

already_AddRefed<nsScreenGonk> ScreenHelperGonk::ScreenGonkForId(
    uint32_t aScreenId) {
  RefPtr<nsScreenGonk> screen = mScreenGonks.Get(aScreenId);

  if (!screen && aScreenId == 0) {
    RefPtr<Screen> screen2 = MakeScreen(aScreenId);
    if (screen2) {
      mScreens.InsertOrUpdate(0, screen2);
    }
    screen = mScreenGonks.Get(aScreenId);
  }

  return screen.forget();
}

uint32_t ScreenHelperGonk::GetNumberOfScreens() { return mScreens.Count(); }

void ScreenHelperGonk::DisplayEnabled(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());

  /* Bug 1244044
   * This function could be called before |mCompositorVsyncScheduler| is set.
   * To avoid this issue, keep the value stored in |mDisplayEnabled|.
   */
  mDisplayEnabled = aEnabled;
  if (mCompositorVsyncScheduler) {
    mCompositorVsyncScheduler->SetDisplay(mDisplayEnabled);
  }

  VsyncControl(aEnabled);
  NS_DispatchToMainThread(aEnabled ? mScreenOnEvent : mScreenOffEvent);
}

bool ScreenHelperGonk::IsScreenConnected(uint32_t aId) {
  return mScreens.Get(aId) != nullptr;
}

void ScreenHelperGonk::VsyncControl(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<VsyncSource> vsyncSource =
      gfxPlatform::GetPlatform()->GetGonkVsyncSource();
  if (aEnabled) {
    vsyncSource->EnableVsync();
  } else {
    vsyncSource->DisableVsync();
  }
}

void ScreenHelperGonk::SetCompositorVsyncScheduler(
    mozilla::layers::CompositorVsyncScheduler* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aObserver);
  mCompositorVsyncScheduler = aObserver;
  mCompositorVsyncScheduler->SetDisplay(mDisplayEnabled);
}

}  // namespace widget
}  // namespace mozilla
