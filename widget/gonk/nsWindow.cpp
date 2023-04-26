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

#include "nsWindow.h"

#include "mozilla/DebugOnly.h"

#include <fcntl.h>

#include "android/log.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Services.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "GLCursorImageManager.h"
#include "nsLayoutUtils.h"
#include "nsAppShell.h"
#include "nsTArray.h"
#include "nsIWidgetListener.h"
#include "CompositorSession.h"
#include "ScreenHelperGonk.h"
#include "libdisplay/GonkDisplay.h"
#include "mozilla/TextEvents.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorBridgeParent.h"
// #include "mozilla/layers/LayerManagerComposite.h"
// #include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/TouchEvents.h"
#include "HwcComposer2D.h"
#include "nsImageLoadingContent.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "ThemeChangeKind.h"

#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Gonk", ##args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ##args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ##args)

#define IS_TOPLEVEL() \
  (mWindowType == WindowType::TopLevel || mWindowType == WindowType::Dialog)
#define OFFSCREEN_CURSOR_POSITION LayoutDeviceIntPoint(-1, -1)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;

static nsWindow* gFocusedWindow = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

nsWindow::nsWindow()
    : mSizeMode(nsSizeMode_Normal), mGLCursorImageManager(nullptr) {
  // This is a hack to force initialization of the compositor
  // resources, if we're going to use omtc.
  //
  // NB: GetPlatform() will create the gfxPlatform, which wants
  // to know the color depth, which asks our native window.
  // This has to happen after other init has finished.
  gfxPlatform::GetPlatform();
  if (!ShouldUseOffMainThreadCompositing()) {
    MOZ_CRASH("How can we render apps, then?");
  }
}

nsWindow::~nsWindow() {
  if (mScreen->IsPrimaryScreen()) {
    mComposer2D->SetCompositorBridgeParent(nullptr);
  }
}

void nsWindow::DoDraw(void) {
  if (!hal::GetScreenEnabled()) {
    gDrawRequest = true;
    return;
  }

  ScreenHelperGonk* screenHelper = ScreenHelperGonk::GetSingleton();
  uint32_t screenNums = screenHelper->GetNumberOfScreens();

  while (screenNums--) {
    RefPtr<nsScreenGonk> screen = screenHelper->ScreenGonkForId(screenNums);
    MOZ_ASSERT(screen);
    if (!screen) {
      continue;
    }

    const nsTArray<nsWindow*>& windows =
        static_cast<nsScreenGonk*>(screen.get())->GetTopWindows();
    if (windows.IsEmpty()) {
      continue;
    }

    /* Add external screen when the external fb is available. The AddScreen
       should be called after shell.js is loaded to receive the
       display-changed event. */
    if (!screenHelper->IsScreenConnected(
            (uint32_t)DisplayType::DISPLAY_EXTERNAL) &&
        screenNums == 0 && GetGonkDisplay()->IsExtFBDeviceEnabled()) {
      screenHelper->AddScreen(
          ScreenHelperGonk::GetIdFromType(DisplayType::DISPLAY_EXTERNAL),
          DisplayType::DISPLAY_EXTERNAL);
    }

    RefPtr<nsWindow> targetWindow = (nsWindow*)windows[0];
    while (targetWindow->GetLastChild()) {
      targetWindow = (nsWindow*)targetWindow->GetLastChild();
    }

    nsIWidgetListener* listener = targetWindow->GetWidgetListener();
    if (listener) {
      listener->WillPaintWindow(targetWindow);
    }

    listener = targetWindow->GetWidgetListener();
    if (listener) {
      listener->DidPaintWindow();
    }
  }
}

void nsWindow::ConfigureAPZControllerThread() {
  APZThreadUtils::SetControllerThread(CompositorThread());
}

void nsWindow::SetMouseCursorPosition(const ScreenIntPoint& aScreenIntPoint) {
#if 0
  // The only implementation of nsIWidget::SetMouseCursorPosition in nsWindow
  // is for remote control.
  if (gFocusedWindow) {
    // NB: this is a racy use of gFocusedWindow.  We assume that
    // our one and only top widget is already in a stable state by
    // the time we start receiving mousemove events.
    gFocusedWindow->SetScreenIntPoint(aScreenIntPoint);
    KickOffComposition();
  }
#endif
}

TextEventDispatcherListener* nsWindow::GetNativeTextEventDispatcherListener() {
  if (IS_TOPLEVEL() && IsVisible()) {
    return mEditableSupport;
  } else {
    return nullptr;
  }
}

/*static*/ nsEventStatus nsWindow::DispatchKeyInput(
    WidgetKeyboardEvent& aEvent) {
  if (!gFocusedWindow) {
    return nsEventStatus_eIgnore;
  }

  gFocusedWindow->UserActivity();

  nsEventStatus status;
  aEvent.mWidget = gFocusedWindow;
  gFocusedWindow->DispatchEvent(&aEvent, status);
  return status;
}

/*static*/ void nsWindow::DispatchTouchInput(MultiTouchInput& aInput) {
  APZThreadUtils::AssertOnControllerThread();

  if (!gFocusedWindow) {
    return;
  }

  gFocusedWindow->DispatchTouchInputViaAPZ(aInput);
}

/*static*/ void nsWindow::SetMouseDevice(bool aMouse) {
#if 0
  if (gFocusedWindow) {
    gFocusedWindow->SetDrawMouse(aMouse);
    ScreenIntPoint point(0, 0);
    nsWindow::NotifyHoverMove(point);
  }
#endif
}

/*static*/ void nsWindow::NotifyHoverMove(const ScreenIntPoint& point) {
#if 0
  if (gFocusedWindow) {
    // NB: this is a racy use of gFocusedWindow.  We assume that
    // our one and only top widget is already in a stable state by
    // the time we start receiving hover-move events.
    gFocusedWindow->SetScreenIntPoint(point);
    KickOffComposition();
  }
#endif
}

class DispatchTouchInputOnMainThread : public mozilla::Runnable {
 public:
  DispatchTouchInputOnMainThread(const MultiTouchInput& aInput,
                                 mozilla::layers::APZEventResult aApzResponse)
      : mozilla::Runnable("DispatchTouchInputOnMainThread"),
        mInput(aInput),
        mApzResponse(aApzResponse) {}

  NS_IMETHOD Run() {
    if (gFocusedWindow) {
      gFocusedWindow->DispatchTouchEventForAPZ(mInput, mApzResponse);
    }
    return NS_OK;
  }

 private:
  MultiTouchInput mInput;
  mozilla::layers::APZEventResult mApzResponse;
};

/*static*/ void nsWindow::KickOffComposition() {
  MOZ_ASSERT(NS_IsMainThread());

  // gFocusedWindow should only be accessed in main thread.
  if (!gFocusedWindow) {
    return;
  }

  if (auto compositorBridge =
          gFocusedWindow->mCompositorSession->GetInProcessBridge()) {
    if (!CompositorThreadHolder::IsInCompositorThread()) {
      CompositorThread()->Dispatch(NewRunnableFunction(
          "nsWindow::KickOffCompositionImpl", &nsWindow::KickOffCompositionImpl,
          compositorBridge));
      return;
    }

    KickOffCompositionImpl(compositorBridge);
  }
}

/*static*/ void nsWindow::KickOffCompositionImpl(
    CompositorBridgeParent* aCompositorBridge) {
  // Removed in https://bugzilla.mozilla.org/show_bug.cgi?id=1785021
  // aCompositorBridge->Invalidate();
  aCompositorBridge->ScheduleComposition(wr::RenderReasons::FLUSH);
}

void nsWindow::DispatchTouchInputViaAPZ(MultiTouchInput& aInput) {
  APZThreadUtils::AssertOnControllerThread();

  if (!mAPZC) {
    // In general mAPZC should not be null, but during initial setup
    // it might be, so we handle that case by ignoring touch input there.
    return;
  }

  // First send it through the APZ code
  mozilla::layers::APZEventResult result =
      mAPZC->InputBridge()->ReceiveInputEvent(aInput);
  // If the APZ says to drop it, then we drop it
  if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  // Can't use NS_NewRunnableMethod because it only takes up to one arg and
  // we need more. Also we can't pass in |this| to the task because nsWindow
  // refcounting is not threadsafe. Instead we just use the gFocusedWindow
  // static ptr inside the task.
  NS_DispatchToMainThread(new DispatchTouchInputOnMainThread(aInput, result));
}

void nsWindow::DispatchTouchEventForAPZ(
    const MultiTouchInput& aInput,
    mozilla::layers::APZEventResult aApzResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  UserActivity();

  // Convert it to an event we can send to Gecko
  WidgetTouchEvent event = aInput.ToWidgetEvent(this);

  // Dispatch the event into the gecko root process for "normal" flow.
  // The event might get sent to a child process,
  // but if it doesn't we need to notify the APZ of various things.
  // All of that happens in ProcessUntransformedAPZEvent
  ProcessUntransformedAPZEvent(&event, aApzResponse);
}

class DispatchTouchInputOnControllerThread : public mozilla::Runnable {
 public:
  explicit DispatchTouchInputOnControllerThread(const MultiTouchInput& aInput)
      : mozilla::Runnable("DispatchTouchInputOnController"), mInput(aInput) {}

  NS_IMETHOD Run() override {
    if (gFocusedWindow) {
      gFocusedWindow->DispatchTouchInputViaAPZ(mInput);
    }

    return NS_OK;
  }

 private:
  MultiTouchInput mInput;
};

nsresult nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  if (aPointerState == TOUCH_HOVER) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mSynthesizedTouchInput) {
    mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
  }

  ScreenIntPoint pointerScreenPoint = ViewAs<ScreenPixel>(
      aPoint, PixelCastJustification::LayoutDeviceIsScreenForBounds);

  // We can't dispatch mSynthesizedTouchInput directly because (a) dispatching
  // it might inadvertently modify it and (b) in the case of touchend or
  // touchcancel events mSynthesizedTouchInput will hold the touches that are
  // still down whereas the input dispatched needs to hold the removed
  // touch(es). We use |inputToDispatch| for this purpose.
  MultiTouchInput inputToDispatch;
  inputToDispatch.mInputType = MULTITOUCH_INPUT;

  int32_t index = mSynthesizedTouchInput->IndexOfTouch((int32_t)aPointerId);
  if (aPointerState == TOUCH_CONTACT) {
    if (index >= 0) {
      // found an existing touch point, update it
      SingleTouchData& point = mSynthesizedTouchInput->mTouches[index];
      point.mScreenPoint = pointerScreenPoint;
      point.mRotationAngle = (float)aPointerOrientation;
      point.mForce = (float)aPointerPressure;
      inputToDispatch.mType = MultiTouchInput::MULTITOUCH_MOVE;
    } else {
      // new touch point, add it
      mSynthesizedTouchInput->mTouches.AppendElement(SingleTouchData(
          (int32_t)aPointerId, pointerScreenPoint, ScreenSize(0, 0),
          (float)aPointerOrientation, (float)aPointerPressure));
      inputToDispatch.mType = MultiTouchInput::MULTITOUCH_START;
    }
    inputToDispatch.mTouches = mSynthesizedTouchInput->mTouches;
  } else {
    MOZ_ASSERT(aPointerState == TOUCH_REMOVE || aPointerState == TOUCH_CANCEL);
    // a touch point is being lifted, so remove it from the stored list
    if (index >= 0) {
      mSynthesizedTouchInput->mTouches.RemoveElementAt(index);
    }
    inputToDispatch.mType =
        (aPointerState == TOUCH_REMOVE ? MultiTouchInput::MULTITOUCH_END
                                       : MultiTouchInput::MULTITOUCH_CANCEL);
    inputToDispatch.mTouches.AppendElement(SingleTouchData(
        (int32_t)aPointerId, pointerScreenPoint, ScreenSize(0, 0),
        (float)aPointerOrientation, (float)aPointerPressure));
  }

  // Can't use NewRunnableMethod here because that will pass a const-ref
  // argument to DispatchTouchInputViaAPZ whereas that function takes a
  // non-const ref. At this callsite we don't care about the mutations that
  // the function performs so this is fine. Also we can't pass |this| to the
  // task because nsWindow refcounting is not threadsafe. Instead we just use
  // the gFocusedWindow static ptr instead the task.
  APZThreadUtils::RunOnControllerThread(
      new DispatchTouchInputOnControllerThread(inputToDispatch));

  return NS_OK;
}

static const char* sThemePrefList[] = {
    "ui.useAccessibilityTheme",
    "ui.systemUsesDarkTheme",
    "ui.prefersReducedMotion",
    "ui.prefersTextSizeId",
};

static void ThemePrefChanged(const char* aPref, void* aModule) {
  auto window = static_cast<nsWindow*>(aModule);
  window->NotifyThemeChanged(ThemeChangeKind::MediaQueriesOnly);
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget* aParent, void* aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 InitData* aInitData) {
  BaseCreate(aParent, aInitData);

  uint32_t screenId =
      aParent ? ((nsWindow*)aParent)->mScreen->GetId() : aInitData->mScreenId;
  ScreenHelperGonk* screenHelper = ScreenHelperGonk::GetSingleton();
  mScreen = screenHelper->ScreenGonkForId(screenId);

  mBounds = aRect;

  mParent = (nsWindow*)aParent;
  mVisible = false;

  if (!aParent) {
    mBounds = mScreen->GetRect();
  }

  mComposer2D = HwcComposer2D::GetInstance();

  if (!IS_TOPLEVEL()) {
    return NS_OK;
  }

  mScreen->RegisterWindow(this);

  Resize(0, 0, mBounds.width, mBounds.height, false);

  for (auto& pref : sThemePrefList) {
    Preferences::RegisterCallback(ThemePrefChanged, nsCString(pref), this);
  }

  return NS_OK;
}

void nsWindow::Destroy(void) {
  for (auto& pref : sThemePrefList) {
    Preferences::UnregisterCallback(ThemePrefChanged, nsCString(pref), this);
  }

  mOnDestroyCalled = true;
  mScreen->UnregisterWindow(this);
  if (this == gFocusedWindow) {
    gFocusedWindow = nullptr;
  }
  nsBaseWidget::OnDestroy();
}

void nsWindow::Show(bool aState) {
  if (mWindowType == WindowType::Invisible) {
    return;
  }

  if (mVisible == aState) {
    return;
  }

  mVisible = aState;
  if (!IS_TOPLEVEL()) {
    return;  // mParent ? mParent->Show(aState) : NS_OK;
  }

  if (aState) {
    BringToTop();
  } else {
    const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
    for (unsigned int i = 0; i < windows.Length(); i++) {
      nsWindow* win = windows[i];
      if (!win->mVisible) {
        continue;
      }
      win->BringToTop();
      break;
    }
  }
}

bool nsWindow::IsVisible() const { return mVisible; }

void nsWindow::ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) {}

void nsWindow::Move(double aX, double aY) {}

void nsWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  Resize(0, 0, aWidth, aHeight, aRepaint);
}

void nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) {
  mBounds = LayoutDeviceIntRect(NSToIntRound(aX), NSToIntRound(aY),
                                NSToIntRound(aWidth), NSToIntRound(aHeight));
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }

  if (aRepaint) {
    Invalidate(mBounds);
  }
}

void nsWindow::Enable(bool aState) {}

bool nsWindow::IsEnabled() const { return true; }

void nsWindow::SetFocus(Raise aRaise, mozilla::dom::CallerType aCallerType) {
  if (aRaise == Raise::Yes) {
    BringToTop();
  }

  if (!IS_TOPLEVEL() && mScreen->IsPrimaryScreen()) {
    // We should only set focused window on non-toplevel primary window.
    gFocusedWindow = this;
  }
}

void nsWindow::Invalidate(const LayoutDeviceIntRect& aRect) {
  nsWindow* top = mParent;
  while (top && top->mParent) {
    top = top->mParent;
  }
  const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
  if (top != windows[0] && this != windows[0]) {
    return;
  }

  gDrawRequest = true;
  mozilla::NotifyEvent();
}

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset() {
  LayoutDeviceIntPoint p(0, 0);
  nsWindow* w = this;

  while (w && w->mParent) {
    p.x += w->mBounds.x;
    p.y += w->mBounds.y;

    w = w->mParent;
  }

  return p;
}

void* nsWindow::GetNativeData(uint32_t aDataType) {
  switch (aDataType) {
    case NS_NATIVE_WINDOW:
      // Called before primary display's EGLSurface creation.
      return mScreen->GetNativeWindow();
    case NS_NATIVE_OPENGL_CONTEXT:
      return mScreen->GetGLContext().take();
    case NS_RAW_NATIVE_IME_CONTEXT: {
      void* pseudoIMEContext = GetPseudoIMEContext();
      if (pseudoIMEContext) {
        return pseudoIMEContext;
      }
      // There is only one IME context on Gonk.
      return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
    }
  }

  return nullptr;
}

void nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal) {
  switch (aDataType) {
    case NS_NATIVE_OPENGL_CONTEXT:
      GLContext* context = reinterpret_cast<GLContext*>(aVal);
      if (!context) {
        mScreen->SetEGLInfo(EGL_NO_DISPLAY, EGL_NO_SURFACE, nullptr);
        return;
      }
      mScreen->SetEGLInfo(GLContextEGL::Cast(context)->mEgl->mDisplay,
                          GLContextEGL::Cast(context)->GetEGLSurface(),
                          context);
      return;
  }
}

void nsWindow::EnsureGLCursorImageManager() {
  if (mGLCursorImageManager) {
    return;
  }

  mGLCursorImageManager = mozilla::MakeUnique<GLCursorImageManager>();
}

void nsWindow::SetCursor(const Cursor& aCursor) {
  nsBaseWidget::SetCursor(aCursor);
  if (mGLCursorImageManager) {
    // Prepare GLCursor if it doesn't exist
    mGLCursorImageManager->PrepareCursorImage(aCursor.mDefaultCursor, this);
    mGLCursorImageManager->HasSetCursor();
    KickOffComposition();
  }
}

static void StopRenderWithHwc(bool aStop) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  HwcComposer2D::GetInstance()->StopRenderWithHwc(aStop);
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus) {
  if (StaticPrefs::dom_virtualcursor_enabled()) {
    if (aEvent->mMessage == eMouseMove) {
      LayoutDeviceIntPoint position(
          std::clamp(int(aEvent->mRefPoint.x), 0, mBounds.width),
          std::clamp(int(aEvent->mRefPoint.y), 0, mBounds.height));

      EnsureGLCursorImageManager();
      mGLCursorImageManager->SetGLCursorPosition(position);
      KickOffComposition();

    } else if (aEvent->mMessage == eMouseExitFromWidget) {
      EnsureGLCursorImageManager();
      mGLCursorImageManager->SetGLCursorPosition(
          GLCursorImageManager::kOffscreenCursorPosition);
      KickOffComposition();
    }
  }

  if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction) {
  mInputContext = aContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext() { return mInputContext; }

void nsWindow::ReparentNativeWidget(nsIWidget* aNewParent) {}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen) {
  if (mWindowType != WindowType::TopLevel) {
    // Ignore fullscreen request for non-toplevel windows.
    NS_WARNING("MakeFullScreen() on a dialog or child widget?");
    return nsBaseWidget::MakeFullScreen(aFullScreen);
  }

  if (aFullScreen) {
    // Fullscreen is "sticky" for toplevel widgets on gonk: we
    // must paint the entire screen, and should only have one
    // toplevel widget, so it doesn't make sense to ever "exit"
    // fullscreen.  If we do, we can leave parts of the screen
    // unpainted.
    nsIntRect virtualBounds;
    mScreen->GetRect(&virtualBounds.x, &virtualBounds.y, &virtualBounds.width,
                     &virtualBounds.height);
    Resize(virtualBounds.x, virtualBounds.y, virtualBounds.width,
           virtualBounds.height,
           /*repaint*/ true);
  }

  if (nsIWidgetListener* listener = GetWidgetListener()) {
    listener->SizeModeChanged(nsSizeMode_Fullscreen);
  }
  return NS_OK;
}

// void nsWindow::DrawWindowOverlay(LayerManagerComposite* aManager,
//                                  LayoutDeviceIntRect aRect) {
//   if (aManager && mGLCursorImageManager) {
//     CompositorOGL* compositor =
//         static_cast<CompositorOGL*>(aManager->GetCompositor());
//     if (compositor) {
//       if (mGLCursorImageManager->ShouldDrawGLCursor() &&
//           mGLCursorImageManager->IsCursorImageReady(mCursor.mDefaultCursor))
//           {
//         GLCursorImageManager::GLCursorImage cursorImage =
//             mGLCursorImageManager->GetGLCursorImage(mCursor.mDefaultCursor);
//         LayoutDeviceIntPoint position =
//             mGLCursorImageManager->GetGLCursorPosition();
//         compositor->DrawGLCursor(aRect, position, cursorImage.mSurface,
//                                  cursorImage.mImgSize, cursorImage.mHotspot);
//       }
//     }
//   }
// }

already_AddRefed<DrawTarget> nsWindow::StartRemoteDrawing() {
  RefPtr<DrawTarget> buffer = mScreen->StartRemoteDrawing();
  return buffer.forget();
}

void nsWindow::EndRemoteDrawing() { mScreen->EndRemoteDrawing(); }

float nsWindow::GetDPI() { return mScreen->GetDpi(); }

bool nsWindow::IsVsyncSupported() { return mScreen->IsVsyncSupported(); }

double nsWindow::GetDefaultScaleInternal() {
  float dpi = GetDPI();
  // The mean pixel density for mdpi devices is 160dpi, 240dpi for hdpi,
  // and 320dpi for xhdpi, respectively.
  // We'll take the mid-value between these three numbers as the boundary.
  if (dpi < 200.0) {
    return 1.0;  // mdpi devices.
  }
  if (dpi < 280.0) {
    return 1.5;  // hdpi devices.
  }
  // xhdpi devices and beyond.
  return floor(dpi / 150.0 + 0.5);
}

WindowRenderer* nsWindow::GetWindowRenderer() {
  /*if (aAllowRetaining) {
    *aAllowRetaining = true;
  }*/
  if (mWindowRenderer) {
    return mWindowRenderer;
  }

  const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
  nsWindow* topWindow = windows[0];

  if (!topWindow) {
    LOGW(" -- no topwindow\n");
    return nullptr;
  }

  CreateCompositor();
  auto mCompositorBridgeParent = mCompositorSession->GetInProcessBridge();
  if (mCompositorBridgeParent) {
    mScreen->SetCompositorBridgeParent(mCompositorBridgeParent);
    if (mScreen->IsPrimaryScreen()) {
      mComposer2D->SetCompositorBridgeParent(mCompositorBridgeParent);
    }
  }
  MOZ_ASSERT(mWindowRenderer);
  return mWindowRenderer;
}

void nsWindow::DestroyCompositor() {
  auto mCompositorBridgeParent = mCompositorSession->GetInProcessBridge();
  if (mCompositorBridgeParent) {
    mScreen->SetCompositorBridgeParent(nullptr);
    if (mScreen->IsPrimaryScreen()) {
      // Unset CompositorBridgeParent
      mComposer2D->SetCompositorBridgeParent(nullptr);
    }
  }
  nsBaseWidget::DestroyCompositor();
}

CompositorBridgeParent* nsWindow::NewCompositorBridgeParent(
    int aSurfaceWidth, int aSurfaceHeight) {
  uint64_t innerWindowId = 0;
  if (Document* doc = GetDocument()) {
    innerWindowId = doc->InnerWindowID();
  }
  return new CompositorBridgeParent(
      nullptr, CSSToLayoutDeviceScale(), TimeDuration::FromMilliseconds(17),
      CompositorOptions(), false, gfx::IntSize(aSurfaceWidth, aSurfaceHeight),
      innerWindowId);
}

void nsWindow::BringToTop() {
  // Only bring primary screen to top to avoid shifting focus away from
  // primary window.
  if (!mScreen->IsPrimaryScreen()) {
    return;
  }

  const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
  if (!windows.IsEmpty()) {
    if (nsIWidgetListener* listener = windows[0]->GetWidgetListener()) {
      listener->WindowDeactivated();
    }
  }

  mScreen->BringToTop(this);

  if (mWidgetListener) {
    mWidgetListener->WindowActivated();
  }

  Invalidate(mBounds);
}

void nsWindow::UserActivity() {
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/useridleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

uint32_t nsWindow::GetGLFrameBufferFormat() {
  if (mWindowRenderer) {
    // We directly map the hardware fb on Gonk.  The hardware fb
    // has RGB format.
    return LOCAL_GL_RGB;
  }
  return LOCAL_GL_NONE;
}

LayoutDeviceIntRect nsWindow::GetNaturalBounds() {
  return mScreen->GetNaturalBounds();
}

nsScreenGonk* nsWindow::GetScreen() { return mScreen; }

bool nsWindow::NeedsPaint() {
  if (!mWindowRenderer) {
    return false;
  }
  return nsIWidget::NeedsPaint();
}

mozilla::layers::Composer2D* nsWindow::GetComposer2D() {
  if (mScreen->GetDisplayType() == DisplayType::DISPLAY_VIRTUAL) {
    return nullptr;
  }

  return mComposer2D;
}

bool nsWindow::IsBelongedToPrimaryScreen() {
  return mScreen->IsPrimaryScreen();
}

already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}
