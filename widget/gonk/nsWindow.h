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

#ifndef nsWindow_h
#define nsWindow_h

#include "InputData.h"
#include "mozilla/UniquePtr.h"
#include "nsBaseWidget.h"
#include "nsRegion.h"
#include "nsIUserIdleServiceInternal.h"
#include "Units.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "GeckoEditableSupport.h"
class ANativeWindowBuffer;

namespace mozilla {
namespace dom {
class AnonymousContent;
}
namespace gfx {
class SourceSurface;
}

namespace layers {
class Composer2D;
}

class HwcComposer2D;
}  // namespace mozilla

namespace widget {
struct InputContext;
struct InputContextAction;
}  // namespace widget

class nsScreenGonk;
class GLCursorImageManager;

class nsWindow final : public nsBaseWidget {
 public:
  using nsBaseWidget::GetWindowRenderer;

  nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  static void DoDraw(void);
  static nsEventStatus DispatchKeyInput(mozilla::WidgetKeyboardEvent& aEvent);
  static void DispatchTouchInput(mozilla::MultiTouchInput& aInput);
  static void SetMouseDevice(bool aMouse);
  static void NotifyHoverMove(const ScreenIntPoint& point);
  static void KickOffComposition();
  static void KickOffCompositionImpl(CompositorBridgeParent* aCompositorBridge);

  using nsBaseWidget::Create;  // for Create signature not overridden here
  NS_IMETHOD Create(nsIWidget* aParent, void* aNativeParent,
                    const LayoutDeviceIntRect& aRect,
                    InitData* aInitData) override;
  void Destroy(void) override;

  void Show(bool aState) override;
  virtual bool IsVisible() const override;
  void ConstrainPosition(DesktopIntPoint&) override;
  void Move(double aX, double aY) override;
  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  virtual nsSizeMode SizeMode() override { return mSizeMode; }
  virtual void SetSizeMode(nsSizeMode aMode) override { mSizeMode = aMode; };
  void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual void* GetNativeData(uint32_t aDataType) override;
  NS_IMETHOD SetTitle(const nsAString& aTitle) override { return NS_OK; }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  void DispatchTouchInputViaAPZ(mozilla::MultiTouchInput& aInput);
  void DispatchTouchEventForAPZ(const mozilla::MultiTouchInput& aInput,
                                mozilla::layers::APZEventResult aApzResponse);
  NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus& aStatus) override;
  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;

  void CaptureRollupEvents(bool aDoCapture) override {}
  void ReparentNativeWidget(nsIWidget* aNewParent) override;

  NS_IMETHOD MakeFullScreen(bool aFullScreen) override;

  virtual already_AddRefed<mozilla::gfx::DrawTarget> StartRemoteDrawing()
      override;
  virtual void EndRemoteDrawing() override;

  void SetCursor(const Cursor&) override;

  void UpdateCursorSourceMap(Cursor aCursor);
  already_AddRefed<mozilla::gfx::SourceSurface> RestyleCursorElement(
      Cursor aCursor);

  virtual float GetDPI() override;
  virtual bool IsVsyncSupported();
  virtual double GetDefaultScaleInternal() override;
  virtual WindowRenderer* GetWindowRenderer() override;
  virtual void DestroyCompositor() override;

  virtual CompositorBridgeParent* NewCompositorBridgeParent(int aSurfaceWidth,
                                                            int aSurfaceHeight);

  NS_IMETHOD_(void)
  SetInputContext(const InputContext& aContext,
                  const InputContextAction& aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;

  virtual uint32_t GetGLFrameBufferFormat() override;

  virtual LayoutDeviceIntRect GetNaturalBounds() override;
  virtual bool NeedsPaint() override;

  mozilla::layers::Composer2D* GetComposer2D();

  void ConfigureAPZControllerThread() override;

  nsScreenGonk* GetScreen();

  // Call this function after remote control use nsContentUtils::SendMouseEvent
  virtual void SetMouseCursorPosition(const ScreenIntPoint& aScreenIntPoint);

  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  void SetNativeTextEventDispatcherListener(
      mozilla::widget::GeckoEditableSupport* aListener) {
    MOZ_ASSERT(!mEditableSupport);
    mEditableSupport = aListener;
  }

 protected:
  nsWindow* mParent;
  nsSizeMode mSizeMode;
  bool mVisible;
  InputContext mInputContext;
  nsCOMPtr<nsIUserIdleServiceInternal> mIdleService;

  virtual ~nsWindow();

  void BringToTop();

  // Call this function when the users activity is the direct cause of an
  // event (like a keypress or mouse click).
  void UserActivity();

  //   void DrawWindowOverlay(LayerManagerComposite* aManager,
  //                          LayoutDeviceIntRect aRect);
  /* Draw virtual cursor */
  mozilla::gfx::DataSourceSurface* GetGLCursorInfo(LayoutDeviceIntPoint& aCursorPos,
    nsIntSize& aImgSize, LayoutDeviceIntPoint& aHotspot);


 private:
  void EnsureGLCursorImageManager();

  // This is used by SynthesizeNativeTouchPoint to maintain state between
  // multiple synthesized points
  mozilla::UniquePtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;

  RefPtr<nsScreenGonk> mScreen;

  RefPtr<mozilla::HwcComposer2D> mComposer2D;

  // 1. This member variable would be accessed by main and compositor thread.
  // 2. Currently there is a lock in GLCursorImageManager to protect it's data
  //    between two threads already.
  // 3. For variable itself, currently there is no protection because compo-
  //    sitor thread will call GLCursorImageManager::ShouldDrawGLCursor() to
  //    check whether it is ready for drawing. Which is protected as mentioned
  //    above. On the other hand the timing it becomes nullptr is inside
  //    destructor of nsWindow and the one access it on compositor thread owns
  //    strong reference of nsWindow so it is impossible to be happened.
  mozilla::UniquePtr<GLCursorImageManager> mGLCursorImageManager;

  virtual bool IsBelongedToPrimaryScreen();
  RefPtr<mozilla::widget::GeckoEditableSupport> mEditableSupport;
};

#endif /* nsWindow_h */
