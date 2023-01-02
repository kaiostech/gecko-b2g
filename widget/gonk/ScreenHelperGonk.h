/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef ScreenHelperGonk_h___
#define ScreenHelperGonk_h___

#include "base/task.h"  // NewRunnableFunction
#include "cutils/properties.h"
#include "hardware/hwcomposer.h"

#include "libdisplay/GonkDisplay.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/Hal.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"
#include "nsProxyRelease.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/ScreenManager.h"

#include <android/native_window.h>

class nsWindow;

namespace android {
class DisplaySurface;
class IGraphicBufferProducer;
};  // namespace android

namespace mozilla {
namespace gl {
class GLContext;
}
namespace layers {
class CompositorVsyncScheduler;
class CompositorBridgeParent;
}  // namespace layers
}  // namespace mozilla

enum class NotifyDisplayChangedEvent : int8_t { Observable, Suppressed };

class nsScreenGonk : public nsIScreen {
  typedef mozilla::GonkDisplay GonkDisplay;
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;
  typedef mozilla::layers::CompositorBridgeParent CompositorBridgeParent;
  typedef mozilla::gfx::DrawTarget DrawTarget;

  ~nsScreenGonk();

 public:
  NS_DECL_ISUPPORTS

  // nsIScreen interface

  // These simply forward to the device-pixel versions;
  // implementations where desktop pixels may not correspond
  // to per-screen device pixels must override.
  NS_IMETHOD GetRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                               int32_t* outWidth, int32_t* outHeight) override;
  NS_IMETHOD GetAvailRectDisplayPix(int32_t* outLeft, int32_t* outTop,
                                    int32_t* outWidth,
                                    int32_t* outHeight) override;

  NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) override;

  NS_IMETHOD GetDefaultCSSScaleFactor(double* aScaleFactor) override;

  NS_IMETHOD GetDpi(float* aDPI) override;

  nsScreenGonk(uint32_t aId, DisplayType aDisplayType,
               const GonkDisplay::NativeData& aNativeData,
               NotifyDisplayChangedEvent aEventVisibility);

  NS_IMETHOD GetId(uint32_t* aId);
  NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth,
                     int32_t* aHeight) override;
  NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth,
                          int32_t* aHeight) override;
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override;
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override;
  NS_IMETHOD GetRotation(uint32_t* aRotation) override;
  NS_IMETHOD SetRotation(uint32_t aRotation) override;
  NS_IMETHOD GetRefreshRate(int32_t* aRefreshRate);
  NS_IMETHOD GetIsPseudoDisplay(bool* aIsPseudoDisplay) {
    *aIsPseudoDisplay = false;
    return NS_OK;
  }
  NS_IMETHOD GetColorGamut(mozilla::dom::ScreenColorGamut * aColorGamut);

  uint32_t GetId();
  NotifyDisplayChangedEvent GetEventVisibility();
  LayoutDeviceIntRect GetRect();
  float GetDpi();
  int32_t GetSurfaceFormat();
  ANativeWindow* GetNativeWindow();
  LayoutDeviceIntRect GetNaturalBounds();
  uint32_t EffectiveScreenRotation();
  bool IsPrimaryScreen();

  already_AddRefed<DrawTarget> StartRemoteDrawing();
  void EndRemoteDrawing();

  nsresult MakeSnapshot(ANativeWindowBuffer* aBuffer);
  void SetCompositorBridgeParent(
      CompositorBridgeParent* aCompositorBridgeParent);

  android::DisplaySurface* GetDisplaySurface();
  int GetPrevDispAcquireFd();
  bool IsComposer2DSupported();
  bool IsVsyncSupported();
  DisplayType GetDisplayType();

  void RegisterWindow(nsWindow* aWindow);
  void UnregisterWindow(nsWindow* aWindow);
  void BringToTop(nsWindow* aWindow);

  const nsTArray<nsWindow*>& GetTopWindows() const { return mTopWindows; }

  // Non-primary screen only
  bool EnableMirroring();
  bool DisableMirroring();
  bool IsMirroring() { return mIsMirroring; }

  // Primary screen only
  bool SetMirroringScreen(nsScreenGonk* aScreen);
  bool ClearMirroringScreen(nsScreenGonk* aScreen);

  // Called only on compositor thread
  void SetEGLInfo(hwc_display_t aDisplay, hwc_surface_t aSurface,
                  mozilla::gl::GLContext* aGLContext);
  hwc_display_t GetEGLDisplay();
  hwc_surface_t GetEGLSurface();
  already_AddRefed<mozilla::gl::GLContext> GetGLContext();
  void UpdateMirroringWidget(
      already_AddRefed<nsWindow>& aWindow);  // Primary screen only
  nsWindow* GetMirroringWidget();            // Primary screen only

 protected:
  ANativeWindowBuffer* DequeueBuffer();
  bool QueueBuffer(ANativeWindowBuffer* buf);

  uint32_t mId;
  NotifyDisplayChangedEvent mEventVisibility;
  int32_t mColorDepth;
  android::sp<ANativeWindow> mNativeWindow;
  float mDpi;
  int32_t mSurfaceFormat;
  LayoutDeviceIntRect
      mNaturalBounds;  // Screen bounds w/o rotation taken into account.
  LayoutDeviceIntRect
      mVirtualBounds;  // Screen bounds w/ rotation taken into account.
  uint32_t mScreenRotation;
  uint32_t mPhysicalScreenRotation;
  int32_t mRefreshRate;
  nsTArray<nsWindow*> mTopWindows;
  android::sp<android::DisplaySurface> mDisplaySurface;
  bool mComposer2DSupported;
  bool mVsyncSupported;
  bool mIsMirroring;                      // Non-primary screen only
  RefPtr<nsScreenGonk> mMirroringScreen;  // Primary screen only
  mozilla::Atomic<CompositorBridgeParent*> mCompositorBridgeParent;

  // Accessed and updated only on compositor thread
  DisplayType mDisplayType;

  hwc_display_t mEGLDisplay;
  hwc_surface_t mEGLSurface;
  RefPtr<mozilla::gl::GLContext> mGLContext;
  RefPtr<nsWindow> mMirroringWidget;  // Primary screen only

  // If we're using a BasicCompositor, these fields are temporarily
  // set during frame composition.  They wrap the hardware
  // framebuffer.
  RefPtr<DrawTarget> mFramebufferTarget;
  ANativeWindowBuffer* mFramebuffer;
  /**
   * Points to a mapped gralloc buffer between calls to lock and unlock.
   * Should be null outside of the lock-unlock pair.
   */
  uint8_t* mMappedBuffer;
  // If we're using a BasicCompositor, this is our window back
  // buffer.  The gralloc framebuffer driver expects us to draw the
  // entire framebuffer on every frame, but gecko expects the
  // windowing system to be tracking buffer updates for invalidated
  // regions.  We get stuck holding that bag.
  //
  // Only accessed on the compositor thread, except during
  // destruction.
  RefPtr<DrawTarget> mBackBuffer;
};

namespace mozilla {
namespace widget {

class ScreenHelperGonk final : public ScreenManager::Helper {
 public:
  ScreenHelperGonk();
  ~ScreenHelperGonk();

  static ScreenHelperGonk* GetSingleton();

  // Generic
  already_AddRefed<Screen> MakeScreen(
      uint32_t id, NotifyDisplayChangedEvent aEventVisibility =
                       NotifyDisplayChangedEvent::Observable);
  void Refresh();

  void AddScreen(uint32_t aScreenId, DisplayType aDisplayType,
                 LayoutDeviceIntRect aRect = LayoutDeviceIntRect(),
                 float aDensity = 1.0f,
                 NotifyDisplayChangedEvent aEventVisibility =
                     NotifyDisplayChangedEvent::Observable);
  void RemoveScreen(uint32_t aId);
  already_AddRefed<Screen> ScreenForId(uint32_t aScreenId);
  already_AddRefed<nsScreenGonk> ScreenGonkForId(uint32_t aScreenId);
  uint32_t GetNumberOfScreens();

  // nsScreenManagerGonk
  static uint32_t GetIdFromType(DisplayType aDisplayType);

  static already_AddRefed<nsScreenGonk> GetPrimaryScreen();
  static already_AddRefed<nsScreenGonk> GetScreenGonk(uint32_t i);

  void DisplayEnabled(bool aEnabled);

  bool IsScreenConnected(uint32_t aId);

  void SetCompositorVsyncScheduler(
      mozilla::layers::CompositorVsyncScheduler* aObserver);

 private:
  nsTHashMap<nsUint32HashKey, RefPtr<Screen>> mScreens;

  nsTHashMap<nsUint32HashKey, RefPtr<nsScreenGonk>> mScreenGonks;

  // nsScreenManagerGonk
  void VsyncControl(bool aEnabled);

  bool mInitialized;
  RefPtr<nsIRunnable> mScreenOnEvent;
  RefPtr<nsIRunnable> mScreenOffEvent;

  bool mDisplayEnabled;
  RefPtr<mozilla::layers::CompositorVsyncScheduler> mCompositorVsyncScheduler;
};

}  // namespace widget
}  // namespace mozilla

#endif /* ScreenHelperGonk_h___ */
