/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CursorSimulator_h
#define mozilla_dom_CursorSimulator_h

#include "nsIDOMEventListener.h"
#include "nsISupportsImpl.h"
#include "nsRefreshDriver.h"
#include "nsIScrollableFrame.h"
#include "VirtualCursorProxy.h"

class nsIContent;
class nsPIDOMWindowOuter;

namespace mozilla {
class WidgetKeyboardEvent;

namespace dom {
class Element;
}

namespace dom {

#define DECL_CURSOR_MOVE_PHASE(X, OFFSET, TIME) eCursorMovePhase##X,
#define DECL_CURSOR_MOVE_BEGIN eCursorMovePhaseBegin = -1,
#define DECL_CURSOR_MOVE_END eCursorMovePhaseEnd
#define DECL_CURSOR_MOVE_FIRST eCursorMovePhaseBegin + 1
#define DECL_CURSOR_MOVE_LAST eCursorMovePhaseEnd - 1

#define CURSOR_MOVE_PHASES              \
  DECL_CURSOR_MOVE_BEGIN                \
  DECL_CURSOR_MOVE_PHASE(1, 7.0, 300)   \
  DECL_CURSOR_MOVE_PHASE(2, 12.0, 1050) \
  DECL_CURSOR_MOVE_PHASE(3, 18.0, 1800) \
  DECL_CURSOR_MOVE_END

enum ECursorMovePhases { CURSOR_MOVE_PHASES };

class nsIVirtualCursor;
class CursorSimulator final : public nsIDOMEventListener,
                              public nsITimerCallback,
                              public nsINamed {
  enum class CursorDirection { UP, DOWN, LEFT, RIGHT, NONE };

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  CursorSimulator(nsPIDOMWindowOuter* aWindow, nsIVirtualCursor* aDelegate);
  void Enable();
  void Disable();
  bool isEnabled() { return mEnabled; }
  void UpdateScreenSize(int32_t aWidth, int32_t aHeight);
  void UpdateChromeOffset(const LayoutDeviceIntPoint& aChromeOffset);
  void UpdatePos();
  void CenterizeCursorIfNecessary();
  void Destroy() { Disable(); }

  enum class SelectionMode {
    None,
    Active,
    Start,
    Stop,
  };
  void SetSelectionMode(SelectionMode aMode);
  bool GetSelectionActive();

 private:
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::Event Event;

  virtual ~CursorSimulator();

  void AddEventListeners();
  void RemoveEventListeners();
  nsresult HandleReturnKey(WidgetKeyboardEvent* aKeyEvent);
  nsresult HandleContextMenuKey(WidgetKeyboardEvent* aKeyEvent);
  nsresult HandleNavigationKey(WidgetKeyboardEvent* aKeyEvent);
  void DevToCSSPixel(LayoutDeviceIntPoint& aDev, CSSPoint& aCSS);
  void CSSToDevPixel(CSSPoint& aCSS, LayoutDeviceIntPoint& aDev);
  void CheckFullScreenElement();

  bool IsCursorOnIMEEnabledElement();

  nsresult EnsureFocusOnVideo();

  // Policy
  void MovePolicy(CSSPoint& aOffset);
  void ApplyOffsetWithZoomLevel(float& aOffset);
  void AdjustMoveOffset(CSSIntSize& aWindowSize,
                        LayoutDeviceIntPoint& aCursorPos, CSSPoint& aOffset);

  bool ScrollPolicy(CSSIntSize& aWindowSize, CSSPoint& aOffset);
  nsIScrollableFrame* FindScrollableFrame(nsIFrame* aFrame);

  CSSPoint CalculateScrollOffset(const CSSRect& aVirtualBoundary,
                                 int32_t aScrollableX, int32_t aScrollableY,
                                 CSSPoint& aOffset);

  void CheckScrollable(int32_t& aScrollableX, int32_t& aScrollableY,
                       nsIScrollableFrame* aScrollFrame, CSSPoint& aOffset);

  bool PerformScroll(nsIScrollableFrame* aScrollFrame, nsIFrame* aRootFrame,
                     bool aCrossDoc, CSSIntSize& aWindowSize,
                     CSSPoint& aOffset);

  bool IsNavigationKey(uint32_t aKeycode);
  bool SetCursorDirection(uint32_t aKeycode);
  bool IsFocusedOnEditableElement();

  bool IsActive();
  void UpdatePos(const LayoutDeviceIntPoint& aPos);
  void CursorClick();
  void CursorDown();
  void CursorUp();
  void CursorMove();
  void CursorOut(bool aCheckActive = false);
  void ShowContextMenu();

  nsCOMPtr<nsPIDOMWindowOuter> mOuterWindow;
  nsCOMPtr<dom::EventTarget> mEventTarget;
  RefPtr<nsIVirtualCursor> mDelegate;

  LayoutDeviceIntPoint mDevCursorPos;
  LayoutDeviceIntPoint mChromeOffset;
  int32_t mScreenWidth;
  int32_t mScreenHeight;

  CursorDirection mDirection;

  // for MovePolicy().
  uint32_t mKeyPressedTime;
  bool mIsKeyPressed;
  nsCOMPtr<nsITimer> mTimer;
  uint32_t mPressTimeBound[DECL_CURSOR_MOVE_END];
  float mMoveStepOffset[DECL_CURSOR_MOVE_END];
  uint32_t mLastKeyCode;
  bool mCursorClickOnKeyUp;
  float mShortClickMoveOffset;
  // FullScreenElement is used in scroll as rootFrame
  Element* mFullScreenElement;
  bool mEnabled;
  SelectionMode mSelectionMode = SelectionMode::None;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CursorSimulator_h
