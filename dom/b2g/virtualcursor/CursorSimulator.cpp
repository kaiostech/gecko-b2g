/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"

#include "nsIDOMEventListener.h"

#include "nsIContent.h"
#include "nsIFrame.h"

#include "nsFocusManager.h"
#include "nsLayoutUtils.h"
#include "VirtualCursorService.h"
#include "mozilla/dom/EditableUtils.h"

using namespace mozilla::dom;

extern mozilla::LazyLogModule gVirtualCursorLog;

namespace mozilla {
namespace dom {

#define DECL_CURSOR_MOVE_BEGIN
#define DECL_CURSOR_MOVE_END
#define DECL_CURSOR_MOVE_PHASE(X, OFFSET, TIME) \
  "dom.virtualcursor.move.pressed_time_" #X,

static const char* sTimeBoundPref[] = {CURSOR_MOVE_PHASES};

#define DECL_CURSOR_MOVE_PHASE(X, OFFSET, TIME) TIME,
static int sDefTimeBound[] = {CURSOR_MOVE_PHASES};

#define DECL_CURSOR_MOVE_PHASE(X, OFFSET, TIME) \
  "dom.virtualcursor.move.step_offset_" #X,

static const char* sStepOffsetPref[] = {CURSOR_MOVE_PHASES};

#define DECL_CURSOR_MOVE_PHASE(X, OFFSET, TIME) OFFSET,
static double sDefStepOffset[] = {CURSOR_MOVE_PHASES};

#define K_VIRTUALBOUNDARY_FACTOR 3.0
#define K_VIRTUALBOUNDARY_PIXEL 20
#define K_MOVE_STEP_OFFSET_DEFAULT 5.0
// If moving the cursor to (-1, -1), there is still a chance to see the
// cursor at that position, since the size of cursor image is 26px, moves
// the cursor to (-30, -30) to avoid seeing it.
#define K_INITIAL_POSITION -30

NS_IMPL_ISUPPORTS(CursorSimulator, nsIDOMEventListener, nsITimerCallback,
                  nsINamed)

CursorSimulator::CursorSimulator(nsPIDOMWindowOuter* aWindow,
                                 nsIVirtualCursor* aDelegate)
    : mOuterWindow(aWindow),
      mDelegate(aDelegate),
      mDevCursorPos(LayoutDeviceIntPoint(K_INITIAL_POSITION, K_INITIAL_POSITION)),
      mDirection(CursorDirection::NONE),
      mKeyPressedTime(0),
      mIsKeyPressed(false),
      mLastKeyCode(0),
      mCursorClickOnKeyUp(false),
      mShortClickMoveOffset(0.0f),
      mFullScreenElement(nullptr),
      mEnabled(false) {
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  for (int i = DECL_CURSOR_MOVE_FIRST; i <= DECL_CURSOR_MOVE_LAST; ++i) {
    mMoveStepOffset[i] =
        Preferences::GetFloat(sStepOffsetPref[i], sDefStepOffset[i]);
    mPressTimeBound[i] =
        Preferences::GetUint(sTimeBoundPref[i], sDefTimeBound[i]);
  }
  mShortClickMoveOffset = Preferences::GetFloat(
      "dom.virtualcursor.move.short_click_offset", K_MOVE_STEP_OFFSET_DEFAULT);

  mEventTarget = mOuterWindow->GetChromeEventHandler();
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator construct %p\n", this));
}

CursorSimulator::~CursorSimulator() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug, ("CursorSimulator destruct\n"));
}

NS_IMETHODIMP
CursorSimulator::GetName(nsACString& aName) {
  aName.AssignLiteral("CursorSimulator");
  return NS_OK;
}

void CursorSimulator::UpdatePos() {
  double width, height;
  if (StaticPrefs::dom_meta_viewport_enabled()) {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    PresShell* presShell = doc->GetPresShell();
    width = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().width);
    height = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().height);
  } else {
    mOuterWindow->GetInnerWidth(&width);
    mOuterWindow->GetInnerHeight(&height);
  }

  CenterizeCursorIfNecessary();

  LayoutDeviceIntPoint windowDevSize;
  CSSPoint cssSize(width, height);
  CSSToDevPixel(cssSize, windowDevSize);

  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator UpdatePos Pos= %d %d window size %d %d screen size "
           "%d %d active %d",
           mDevCursorPos.x, mDevCursorPos.y, windowDevSize.x, windowDevSize.y,
           mScreenWidth, mScreenHeight, IsActive()));
  LayoutDeviceIntPoint point = mDevCursorPos + mChromeOffset;
  if (!IsActive() || !mEnabled || point.x < 0 || point.y < 0) {
    return;
  }
  UpdatePos(point);
}

bool CursorSimulator::IsActive() {
  Document* doc = mOuterWindow->GetDoc();
  return doc && IsInActiveTab(doc);
}

void CursorSimulator::Enable() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::Enable current setting=%d", mEnabled));
  if (mEnabled) {
    return;
  }
  mEnabled = true;
  AddEventListeners();
  CenterizeCursorIfNecessary();
  CheckFullScreenElement();
  if (mFullScreenElement &&
      mFullScreenElement->IsHTMLElement(nsGkAtoms::video)) {
    CursorOut();
  } else {
    UpdatePos();
  }
};

void CursorSimulator::Disable() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::Disable current setting=%d", mEnabled));
  if (!mEnabled) {
    return;
  }
  mEnabled = false;
  SetSelectionMode(SelectionMode::None);
  RemoveEventListeners();
  mTimer->Cancel();
  CursorOut();
};

void CursorSimulator::SetSelectionMode(SelectionMode aMode) {
  RefPtr<nsGlobalWindowOuter> win = nsGlobalWindowOuter::Cast(mOuterWindow);

  if (aMode == SelectionMode::None) {
    // In case the cursor is already disable, we still want to set the
    // style of cursor back to auto.
    win->SetCursorOuter("auto"_ns, IgnoreErrors());
    mSelectionMode = SelectionMode::None;
    return;
  }

  if (!mEnabled) {
    return;
  }
  if (IsCursorOnIMEEnabledElement()) {
    return;
  }

  mSelectionMode = aMode;

  switch (mSelectionMode) {
    case SelectionMode::None:
      win->SetCursorOuter("auto"_ns, IgnoreErrors());
      break;
    case SelectionMode::Active:
      win->SetCursorOuter("crosshair"_ns, IgnoreErrors());
      break;
    case SelectionMode::Start:
      win->SetCursorOuter("none"_ns, IgnoreErrors());
      CursorDown();
      CursorMove();
      break;
    case SelectionMode::Stop:
      CursorUp();
      win->SetCursorOuter("crosshair"_ns, IgnoreErrors());
      break;
  };
}

bool CursorSimulator::GetSelectionActive() {
  return mSelectionMode > SelectionMode::None;
}

void CursorSimulator::UpdateScreenSize(int32_t aWidth, int32_t aHeight) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator UpdateScreenSize %d %d", aWidth, aHeight));
  mScreenWidth = aWidth;
  mScreenHeight = aHeight;
}

void CursorSimulator::UpdateChromeOffset(
    const LayoutDeviceIntPoint& aChromeOffset) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator UpdateChromeOffset %d %d", aChromeOffset.x,
           aChromeOffset.y));
  mChromeOffset = aChromeOffset;
}

void CursorSimulator::AddEventListeners() {
  mEventTarget->AddSystemEventListener(
      NS_LITERAL_STRING_FROM_CSTRING("fullscreenchange"), this, true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING_FROM_CSTRING("keydown"),
                                 this, true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING_FROM_CSTRING("keyup"), this,
                                 true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING_FROM_CSTRING("keypress"),
                                 this, true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING_FROM_CSTRING("blur"), this,
                                 true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING_FROM_CSTRING("focus"), this,
                                 true);
}

void CursorSimulator::RemoveEventListeners() {
  mEventTarget->RemoveSystemEventListener(
      NS_LITERAL_STRING_FROM_CSTRING("fullscreenchange"), this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING_FROM_CSTRING("keydown"),
                                    this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING_FROM_CSTRING("keyup"),
                                    this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING_FROM_CSTRING("keypress"),
                                    this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING_FROM_CSTRING("blur"),
                                    this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING_FROM_CSTRING("focus"),
                                    this, true);
}

bool CursorSimulator::IsFocusedOnEditableElement() {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) {
    return false;
  }
  Element* focusedElement = focusManager->GetFocusedElement();
  if (!focusedElement) {
    return false;
  }
  return EditableUtils::isContentEditable(focusedElement) &&
         EditableUtils::isFocusableElement(focusedElement);
}

nsresult CursorSimulator::HandleEvent(Event* aEvent) {
  if (!aEvent) {
    return NS_OK;
  }
  // Handle Full Screen Event first
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("fullscreenchange")) {
    CheckFullScreenElement();
    if (mFullScreenElement &&
        mFullScreenElement->IsHTMLElement(nsGkAtoms::video)) {
      CursorOut();
    }
    if (!mFullScreenElement) {
      UpdatePos();
    }
    return NS_OK;
  }

  if (eventType.EqualsLiteral("blur")) {
    nsCOMPtr<nsPIDOMWindowOuter> outer =
        do_QueryInterface(aEvent->GetOriginalTarget());

    if (outer == mOuterWindow) {
      MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
              ("CursorSimulator window blur"));
      CursorOut(false);
    }
    return NS_OK;
  }

  if (eventType.EqualsLiteral("focus")) {
    bool focusedOnEditable = IsFocusedOnEditableElement();
    MOZ_LOG(
        gVirtualCursorLog, LogLevel::Debug,
        ("CursorSimulator element focus, is focused on an editable element=%d",
         focusedOnEditable));
    if (focusedOnEditable ||
        (mFullScreenElement &&
         mFullScreenElement->IsHTMLElement(nsGkAtoms::video))) {
      CursorOut();
    } else {
      UpdatePos();
    }
    return NS_OK;
  }

  WidgetKeyboardEvent* keyEvent = aEvent->WidgetEventPtr()->AsKeyboardEvent();

  if (!keyEvent || IsFocusedOnEditableElement()) {
    return NS_OK;
  }

  if (mFullScreenElement &&
      mFullScreenElement->IsHTMLElement(nsGkAtoms::video)) {
    // Handle CSK and Navigation only.
    if (keyEvent->mKeyCode == KeyboardEvent_Binding::DOM_VK_RETURN ||
        IsNavigationKey(keyEvent->mKeyCode)) {
      return EnsureFocusOnVideo();
    }
    return NS_OK;
  }

  if (keyEvent->mKeyCode == KeyboardEvent_Binding::DOM_VK_RETURN) {
    aEvent->PreventDefault();
    aEvent->StopPropagation();
    return HandleReturnKey(keyEvent);
  }

  if (keyEvent->mKeyNameIndex == KEY_NAME_INDEX_SoftRight) {
    return HandleContextMenuKey(keyEvent);
  }

  if (IsNavigationKey(keyEvent->mKeyCode)) {
    aEvent->PreventDefault();
    aEvent->StopPropagation();
    return HandleNavigationKey(keyEvent);
  }

  return NS_OK;
}

nsresult CursorSimulator::HandleReturnKey(WidgetKeyboardEvent* aKeyEvent) {
  if (mDevCursorPos.x < 0) {
    mDevCursorPos.x = 0;
  }

  if (mDevCursorPos.y < 0) {
    mDevCursorPos.y = 0;
  }

  UpdatePos();

  if (GetSelectionActive()) {
    return NS_OK;
  }

  // Do cursor click on keyup for IME enabled elements like forms.
  // If the click is on keydown, the keyup event might be catched
  // in the gap between Spatial Navigation service and IME service,
  // which leads to unexpected default action like form submit.
  if (aKeyEvent->mMessage == eKeyUp) {
    if (mCursorClickOnKeyUp) {
      mCursorClickOnKeyUp = false;
      CursorClick();
    } else {
      CursorUp();
    }
  } else if (aKeyEvent->mMessage == eKeyDown && !aKeyEvent->mIsRepeat) {
    if (IsCursorOnIMEEnabledElement()) {
      mCursorClickOnKeyUp = true;
    } else {
      CursorDown();
    }
  }
  return NS_OK;
}

nsresult CursorSimulator::HandleContextMenuKey(WidgetKeyboardEvent* aKeyEvent) {
  if (aKeyEvent->mMessage == eKeyDown) {
    ShowContextMenu();
  }
  return NS_OK;
}

nsresult CursorSimulator::HandleNavigationKey(WidgetKeyboardEvent* aKeyEvent) {
  if (aKeyEvent->mMessage == eKeyPress ||
      (mLastKeyCode != aKeyEvent->mKeyCode && mLastKeyCode != 0) ||
      (aKeyEvent->mMessage == eKeyDown && aKeyEvent->mIsRepeat)) {
    return NS_OK;
  }
  mLastKeyCode = aKeyEvent->mKeyCode;
  switch (mLastKeyCode) {
    case KeyboardEvent_Binding::DOM_VK_UP:
      mDirection = CursorDirection::UP;
      break;
    case KeyboardEvent_Binding::DOM_VK_DOWN:
      mDirection = CursorDirection::DOWN;
      break;
    case KeyboardEvent_Binding::DOM_VK_LEFT:
      mDirection = CursorDirection::LEFT;
      break;
    case KeyboardEvent_Binding::DOM_VK_RIGHT:
      mDirection = CursorDirection::RIGHT;
      break;
    default:
      MOZ_ASSERT(false, "Received non-direction keycode");
      return NS_OK;
  }

  CSSPoint cursorPos;
  DevToCSSPixel(mDevCursorPos, cursorPos);
  CSSPoint offset(0, 0);
  // Update window size before caculating offset.
  double width, height;
  if (StaticPrefs::dom_meta_viewport_enabled()) {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    PresShell* presShell = doc->GetPresShell();
    width = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().width);
    height = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().height);
  } else {
    mOuterWindow->GetInnerWidth(&width);
    mOuterWindow->GetInnerHeight(&height);
  }

  CSSIntSize windowSize;
  windowSize.width = width;
  windowSize.height = height;
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::HandleNavigationKey start %p", this));
  mIsKeyPressed = (aKeyEvent->mMessage == eKeyDown);
  MovePolicy(offset);
  ScrollPolicy(windowSize, offset);
  AdjustMoveOffset(windowSize, mDevCursorPos, offset);
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::HandleNavigationKey end"));

  cursorPos += offset;
  CSSToDevPixel(cursorPos, mDevCursorPos);
  UpdatePos();

  if (aKeyEvent->mMessage == eKeyUp) {
    mLastKeyCode = 0;
    mDirection = CursorDirection::NONE;
  }
  return NS_OK;
}

void CursorSimulator::DevToCSSPixel(LayoutDeviceIntPoint& aDev,
                                    CSSPoint& aCSS) {
  RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);
  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE_VOID(presContext);

  float resolution = presShell->GetResolution();
  aCSS.x = presContext->DevPixelsToFloatCSSPixels(aDev.x) / resolution;
  aCSS.y = presContext->DevPixelsToFloatCSSPixels(aDev.y) / resolution;
}

void CursorSimulator::CSSToDevPixel(CSSPoint& aCSS,
                                    LayoutDeviceIntPoint& aDev) {
  RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);
  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE_VOID(presContext);

  float resolution = presShell->GetResolution();
  aDev.x = NS_lround(presContext->CSSPixelsToDevPixels(aCSS.x * resolution));
  aDev.y = NS_lround(presContext->CSSPixelsToDevPixels(aCSS.y * resolution));
}

void CursorSimulator::CheckFullScreenElement() {
  RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
  MOZ_ASSERT(doc);
  mFullScreenElement = doc->GetFullscreenElement();
}

void CursorSimulator::CenterizeCursorIfNecessary() {
  double width, height;
  if (StaticPrefs::dom_meta_viewport_enabled()) {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    PresShell* presShell = doc->GetPresShell();
    width = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().width);
    height = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().height);
  } else {
    mOuterWindow->GetInnerWidth(&width);
    mOuterWindow->GetInnerHeight(&height);
  }

  CSSIntSize windowSize;
  windowSize.width = width;
  windowSize.height = height;

  LayoutDeviceIntPoint devSize;
  CSSPoint cssSize(windowSize.width, windowSize.height);
  CSSToDevPixel(cssSize, devSize);

  if (devSize.x <= 0 || devSize.y <= 0) {
    return;
  }

  if (mDevCursorPos.x >= 0 && mDevCursorPos.x < devSize.x &&
      mDevCursorPos.y >= 0 && mDevCursorPos.y < devSize.y) {
    return;
  }
  mDevCursorPos.x = devSize.x / 2;
  mDevCursorPos.y = devSize.y / 2;
}

void CursorSimulator::ApplyOffsetWithZoomLevel(float& aOffset) {
  RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);
  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);
  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE_VOID(presContext);

  aOffset /=
      (presShell->GetCumulativeResolution() * presContext->GetFullZoom());
}

nsresult CursorSimulator::Notify(nsITimer* aTimer) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator timer is fired"));
  mKeyPressedTime += 33;

  // Timer is only scheduled during key pressed.
  CSSPoint offset(0, 0);
  // Update window size before caculating offset.
  double width, height;
  if (StaticPrefs::dom_meta_viewport_enabled()) {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    if (!doc) {
      return NS_OK;
    }
    PresShell* presShell = doc->GetPresShell();
    if (!presShell) {
      return NS_OK;
    }
    width = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().width);
    height = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().height);
  } else {
    mOuterWindow->GetInnerWidth(&width);
    mOuterWindow->GetInnerHeight(&height);
  }

  CSSIntSize windowSize;
  windowSize.width = width;
  windowSize.height = height;
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::HandleNavigationKey (trigger by timer) start"));
  MovePolicy(offset);
  ScrollPolicy(windowSize, offset);
  AdjustMoveOffset(windowSize, mDevCursorPos, offset);
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::HandleNavigationKey end"));

  CSSPoint cursorPos;
  DevToCSSPixel(mDevCursorPos, cursorPos);
  cursorPos += offset;
  CSSToDevPixel(cursorPos, mDevCursorPos);
  UpdatePos();

  if (mDirection != CursorDirection::NONE) {
    Unused << aTimer->InitWithCallback(this, 33, nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

bool CursorSimulator::IsCursorOnIMEEnabledElement() {
  nsIFrame* rootFrame;
  if (mFullScreenElement) {
    nsIFrame* primaryFrame = mFullScreenElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(primaryFrame, false);
    rootFrame = nsLayoutUtils::GetDisplayRootFrame(primaryFrame);
  } else {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    NS_ENSURE_TRUE(doc, false);
    PresShell* presShell = doc->GetPresShell();
    NS_ENSURE_TRUE(presShell, false);
    rootFrame = presShell->GetRootFrame();
  }
  NS_ENSURE_TRUE(rootFrame, false);

  CSSPoint cursorPos;
  DevToCSSPixel(mDevCursorPos, cursorPos);
  nsPoint position(nsPresContext::CSSPixelsToAppUnits(cursorPos.x),
                   nsPresContext::CSSPixelsToAppUnits(cursorPos.y));
  nsTArray<nsIFrame*> frames;
  nsresult rv = nsLayoutUtils::GetFramesForArea(
      RelativeTo{rootFrame}, nsRect(position, nsSize(1, 1)), frames);

  if (!NS_FAILED(rv)) {
    for (uint32_t i = 0; i < frames.Length(); i++) {
      nsIFrame* frame = frames[i];
      nsIContent* content = frame->GetContent();
      if (content && content->GetDesiredIMEState().IsEditable()) {
        return true;
      }
    }
  }

  return false;
}

nsresult CursorSimulator::EnsureFocusOnVideo() {
  MOZ_ASSERT(mOuterWindow->GetExtantDoc());
  MOZ_ASSERT(mFullScreenElement);

  // ensure that the element is actually focused
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) {
    return NS_OK;
  }
  nsIContent* focusedElement = focusManager->GetFocusedElement();
  if (SameCOMIdentity(ToSupports(mFullScreenElement), focusedElement)) {
    return NS_OK;
  }
  focusManager->SetFocus(mFullScreenElement, nsIFocusManager::FLAG_NOSCROLL |
                                                 nsIFocusManager::FLAG_BYMOUSE);

  return NS_OK;
}

void CursorSimulator::MovePolicy(CSSPoint& aOffset) {
  if (!mKeyPressedTime && mIsKeyPressed) {
    // To fire a timer for judging long or short pressing.
    Unused << mTimer->InitWithCallback(this, 300, nsITimer::TYPE_ONE_SHOT);
    return;
  }

  // In the current implementation we can only handle one pressed key in a time
  // and ignore other pressed keys later before first key is up.
  if (!mIsKeyPressed) {
    if (mKeyPressedTime > 300) {
      mKeyPressedTime = 0;
      mTimer->Cancel();
      MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
              ("CursorSimulator::MovePolicy: Key released after long press"));
      return;
    }

    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::MovePolicy: Key released in short press stage"));
    mKeyPressedTime = 0;
    mTimer->Cancel();
  }

  // during pressing state
  float offset = 0;
  bool longPressed = true;
  // Base on pressed time to set different offset. The longer key is pressed,
  // the larger offset is returned.

  if (!mIsKeyPressed &&
      mKeyPressedTime <= mPressTimeBound[DECL_CURSOR_MOVE_FIRST]) {
    longPressed = false;
    offset = mShortClickMoveOffset;
  } else {
    for (int i = DECL_CURSOR_MOVE_FIRST; i <= DECL_CURSOR_MOVE_LAST; ++i) {
      if (mKeyPressedTime <= mPressTimeBound[i]) {
        break;
      }
      offset = mMoveStepOffset[i];
    }
  }

  if (longPressed) {
    ApplyOffsetWithZoomLevel(offset);
  }
  switch (mDirection) {
    case CursorDirection::UP:
      aOffset.y = -offset;
      break;
    case CursorDirection::DOWN:
      aOffset.y = offset;
      break;
    case CursorDirection::LEFT:
      aOffset.x = -offset;
      break;
    case CursorDirection::RIGHT:
      aOffset.x = offset;
      break;
    default:
      break;
  }
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::MovePolicy: Offset(%.2f,%.2f) is calculated",
           aOffset.x, aOffset.y));
}

bool CursorSimulator::ScrollPolicy(CSSIntSize& aWindowSize, CSSPoint& aOffset) {
  if (mDirection == CursorDirection::NONE || (!aOffset.x && !aOffset.y)) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::ScrollPolicy: Direction::NONE or zero offset."));
    return false;
  }

  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::ScrollPolicy: aOffset (%.2f, %.2f)", aOffset.x,
           aOffset.y));

  // Find a frame from cursor point first
  // If in full screen mode, use full screen element as rootFrame
  nsIFrame* rootFrame;
  if (mFullScreenElement) {
    nsIFrame* primaryFrame = mFullScreenElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(primaryFrame, false);
    rootFrame = nsLayoutUtils::GetDisplayRootFrame(primaryFrame);
  } else {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    NS_ENSURE_TRUE(doc, false);
    PresShell* presShell = doc->GetPresShell();
    NS_ENSURE_TRUE(presShell, false);
    rootFrame = presShell->GetRootFrame();
  }
  NS_ENSURE_TRUE(rootFrame, false);

  CSSPoint cursorPos;
  DevToCSSPixel(mDevCursorPos, cursorPos);
  nsPoint position(nsPresContext::CSSPixelsToAppUnits(cursorPos.x),
                   nsPresContext::CSSPixelsToAppUnits(cursorPos.y));
  nsIFrame* frame =
      nsLayoutUtils::GetFrameForPoint(RelativeTo{rootFrame}, position);

  if (!frame) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::ScrollPolicy No frame at (%.2f, %.2f).",
             cursorPos.x, cursorPos.y));
    return false;
  }

  // Find scrollable frame
  nsIScrollableFrame* scrollableFrame = FindScrollableFrame(frame);

  if (!scrollableFrame) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::ScrollPolicy No scrollable frame at (%.2f,%.2f)",
             cursorPos.x, cursorPos.y));
    return false;
  }

  // Check if CrossDoc
  bool isCrossDoc = false;
  nsIContent* content = frame->GetContent();
  NS_ENSURE_TRUE(content, false);
  Document* doc = content->OwnerDoc();
  NS_ENSURE_TRUE(doc, false);
  nsPIDOMWindowOuter* window = doc->GetWindow();
  NS_ENSURE_TRUE(window, false);
  if (window != mOuterWindow) {
    isCrossDoc = true;
  }

  return PerformScroll(scrollableFrame, rootFrame, isCrossDoc, aWindowSize,
                       aOffset);
}

bool CursorSimulator::PerformScroll(nsIScrollableFrame* aScrollFrame,
                                    nsIFrame* aRootFrame, bool aCrossDoc,
                                    CSSIntSize& aWindowSize,
                                    CSSPoint& aOffset) {
  // Obtain scrollabe range along the direction of move offset
  int32_t scrollableX = 0;
  int32_t scrollableY = 0;
  CheckScrollable(scrollableX, scrollableY, aScrollFrame, aOffset);

  if (scrollableX == 0 && scrollableY == 0) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::ScrollPolicy: scroll to the end already."));
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = mOuterWindow->GetDocShell();
  NS_ENSURE_TRUE(docShell, false);
  RefPtr<nsPresContext> presContext = docShell->GetPresContext();

  NS_ENSURE_TRUE(presContext, false);
  nsIFrame* frame = aScrollFrame->GetScrolledFrame();
  NS_ENSURE_TRUE(frame, false);
  nsPoint frameOffset;
  if (aCrossDoc) {
    frameOffset = frame->GetOffsetToCrossDoc(aRootFrame);
  } else {
    frameOffset = frame->GetOffsetTo(aRootFrame);
  }
  nsRect frameRect = aScrollFrame->GetScrollPortRect();
  nsPoint scrollPt = aScrollFrame->GetScrollPosition();
  frameRect.x = frameOffset.x + scrollPt.x;
  frameRect.y = frameOffset.y + scrollPt.y;

  CSSRect winRect(0, 0, aWindowSize.width, aWindowSize.height);
  CSSRect viewedRect(presContext->AppUnitsToFloatCSSPixels(frameRect.x),
                     presContext->AppUnitsToFloatCSSPixels(frameRect.y),
                     presContext->AppUnitsToFloatCSSPixels(frameRect.width),
                     presContext->AppUnitsToFloatCSSPixels(frameRect.height));
  viewedRect = winRect.Intersect(viewedRect);
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::ScrollPolicy: viewedRect (%.2f, %.2f, %.2f, %.2f)",
           viewedRect.x, viewedRect.y, viewedRect.width, viewedRect.height));

  CSSRect virtualBoundary(viewedRect.x + K_VIRTUALBOUNDARY_PIXEL,
                          viewedRect.y + K_VIRTUALBOUNDARY_PIXEL,
                          viewedRect.width - (K_VIRTUALBOUNDARY_PIXEL * 2),
                          viewedRect.height - (K_VIRTUALBOUNDARY_PIXEL * 2));

  // Check virtualBoundary to see if scroll is needed and calculate offset.
  CSSPoint scrollOffset =
      CalculateScrollOffset(virtualBoundary, scrollableX, scrollableY, aOffset);

  if (scrollOffset.x == 0 && scrollOffset.y == 0) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("CursorSimulator::ScrollPolicy: No scroll required"));
    return false;
  }

  nsIntPoint destinationPt;
  destinationPt.x =
      NS_lround(presContext->CSSPixelsToDevPixels(scrollOffset.x));
  destinationPt.y =
      NS_lround(presContext->CSSPixelsToDevPixels(scrollOffset.y));

  aScrollFrame->ScrollBy(destinationPt, ScrollUnit::DEVICE_PIXELS,
                         ScrollMode::Smooth);
  return true;
}

bool CursorSimulator::IsNavigationKey(uint32_t aKeycode) {
  return (aKeycode == KeyboardEvent_Binding::DOM_VK_UP ||
          aKeycode == KeyboardEvent_Binding::DOM_VK_DOWN ||
          aKeycode == KeyboardEvent_Binding::DOM_VK_LEFT ||
          aKeycode == KeyboardEvent_Binding::DOM_VK_RIGHT);
}

void CursorSimulator::AdjustMoveOffset(CSSIntSize& aWindowSize,
                                       LayoutDeviceIntPoint& aCursorPos,
                                       CSSPoint& aOffset) {
  LayoutDeviceIntPoint devSize;

  CSSPoint cssSize(aWindowSize.width, aWindowSize.height);
  CSSToDevPixel(cssSize, devSize);
  LayoutDeviceIntPoint screen(mScreenWidth, mScreenHeight);

  LayoutDeviceIntPoint boundaryDevSize(std::min(devSize.x, screen.x) - 1,
                                       std::min(devSize.y, screen.y) - 1);
  CSSPoint boundaryCSSSize;
  DevToCSSPixel(boundaryDevSize, boundaryCSSSize);
  CSSPoint cursorPos;
  DevToCSSPixel(aCursorPos, cursorPos);
  if (cursorPos.y + aOffset.y < 0) {
    aOffset.y = -cursorPos.y;
  } else if (cursorPos.y + aOffset.y > boundaryCSSSize.y) {
    aOffset.y = boundaryCSSSize.y - cursorPos.y;
  }

  if (cursorPos.x + aOffset.x < 0) {
    aOffset.x = -cursorPos.x;
  } else if (cursorPos.x + aOffset.x > boundaryCSSSize.x) {
    aOffset.x = boundaryCSSSize.x - cursorPos.x;
  }
  MOZ_LOG(
      gVirtualCursorLog, LogLevel::Debug,
      ("CursorSimulator::AdjustMoveOffset(%.2f,%.2f)", aOffset.x, aOffset.y));
}

nsIScrollableFrame* CursorSimulator::FindScrollableFrame(nsIFrame* aFrame) {
  // Depends on direction to compute ScrollTarget
  CSSPoint directionPt(0, 0);
  EventStateManager::ComputeScrollTargetOptions options =
      EventStateManager::COMPUTE_SCROLLABLE_TARGET_ALONG_X_Y_AXIS;
  switch (mDirection) {
    case CursorDirection::UP:
      directionPt.y = -1;
      break;
    case CursorDirection::DOWN:
      directionPt.y = 1;
      break;
    case CursorDirection::LEFT:
      directionPt.x = -1;
      break;
    case CursorDirection::RIGHT:
      directionPt.x = 1;
      break;
    default:
      break;
  }

  nsCOMPtr<nsIDocShell> docShell = mOuterWindow->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  RefPtr<nsPresContext> presContext = docShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, nullptr);
  EventStateManager* esm = presContext->EventStateManager();
  NS_ENSURE_TRUE(esm, nullptr);

  WidgetWheelEvent event(true, eWheel, nullptr);
  event.mDeltaX = directionPt.x;
  event.mDeltaY = directionPt.y;
  nsIScrollableFrame* scrollableFrame =
      do_QueryFrame(esm->ComputeScrollTarget(aFrame, &event, options));

  return scrollableFrame;
}

void CursorSimulator::CheckScrollable(int32_t& aScrollableX,
                                      int32_t& aScrollableY,
                                      nsIScrollableFrame* aScrollFrame,
                                      CSSPoint& aOffset) {
  int32_t scrollRangeMaxX = nsPresContext::AppUnitsToIntCSSPixels(
      aScrollFrame->GetScrollRange().XMost());
  int32_t scrollRangeMaxY = nsPresContext::AppUnitsToIntCSSPixels(
      aScrollFrame->GetScrollRange().YMost());
  int32_t scrollX = aScrollFrame->GetScrollPositionCSSPixels().x;
  int32_t scrollY = aScrollFrame->GetScrollPositionCSSPixels().y;
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("CursorSimulator::CheckScrollable: scroll:(%d,%d) max:(%d,%d)",
           scrollX, scrollY, scrollRangeMaxX, scrollRangeMaxY));

  if (StaticPrefs::dom_meta_viewport_enabled()) {
    RefPtr<Document> doc = mOuterWindow->GetExtantDoc();
    PresShell* presShell = doc->GetPresShell();

    // scrollHeight
    int32_t scrollHeight = nsPresContext::AppUnitsToIntCSSPixels(
        aScrollFrame->GetScrollRange().Height() +
        aScrollFrame->GetScrollPortRect().Height());
    // scrollWidth
    int32_t scrollWidth = nsPresContext::AppUnitsToIntCSSPixels(
        aScrollFrame->GetScrollRange().Width() +
        aScrollFrame->GetScrollPortRect().Width());

    // visualViewport.pageTop
    int32_t pageTop = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportOffset().y);
    // visualViewport.pageLeft
    int32_t pageLeft = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportOffset().x);

    // visualViewport.height
    int32_t visualViewportHeight = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().height);
    // visualViewport.width
    int32_t visualViewportWidth = nsPresContext::AppUnitsToIntCSSPixels(
        presShell->GetVisualViewportSize().width);

    if (scrollHeight > visualViewportHeight &&
        scrollWidth > visualViewportHeight) {
      scrollRangeMaxY = scrollHeight - visualViewportHeight;
      scrollRangeMaxX = scrollWidth - visualViewportWidth;
      scrollY = pageTop;
      scrollX = pageLeft;
    }
  }

  if (aOffset.x > 0 && scrollRangeMaxX - scrollX > 0) {
    aScrollableX = scrollRangeMaxX - scrollX;
  } else if (aOffset.x < 0 && scrollX > 0) {
    aScrollableX = -scrollX;
  } else {
    aScrollableX = 0;
  }
  if (aOffset.y > 0 && scrollRangeMaxY - scrollY > 0) {
    aScrollableY = scrollRangeMaxY - scrollY;
  } else if (aOffset.y < 0 && scrollY > 0) {
    aScrollableY = -scrollY;
  } else {
    aScrollableY = 0;
  }
}

CSSPoint CursorSimulator::CalculateScrollOffset(const CSSRect& aVirtualBoundary,
                                                int32_t aScrollableX,
                                                int32_t aScrollableY,
                                                CSSPoint& aOffset) {
  CSSPoint scrollOffset = aOffset;
  aOffset.MoveTo(0, 0);

  CSSPoint cursorPos;
  DevToCSSPixel(mDevCursorPos, cursorPos);
  if (scrollOffset.x > 0) {
    if (aScrollableX > 0 &&
        cursorPos.x + scrollOffset.x > aVirtualBoundary.XMost()) {
      aOffset.x =
          std::min(aVirtualBoundary.XMost() - cursorPos.x, float(aScrollableX));
      scrollOffset.x -= aOffset.x;
    } else {
      aOffset.x = scrollOffset.x;
      scrollOffset.x = 0;
    }
  } else if (scrollOffset.x < 0) {
    if (aScrollableX < 0 &&
        cursorPos.x + scrollOffset.x < aVirtualBoundary.x + 1) {
      aOffset.x =
          std::max(aVirtualBoundary.x - cursorPos.x + 1, float(aScrollableX));
      scrollOffset.x -= aOffset.x;
    } else {
      aOffset.x = scrollOffset.x;
      scrollOffset.x = 0;
    }
  }

  if (scrollOffset.y > 0) {
    if (aScrollableY > 0 &&
        cursorPos.y + scrollOffset.y > aVirtualBoundary.YMost()) {
      aOffset.y =
          std::min(aVirtualBoundary.YMost() - cursorPos.y, float(aScrollableY));
      scrollOffset.y -= aOffset.y;
    } else {
      aOffset.y = scrollOffset.y;
      scrollOffset.y = 0;
    }
  } else if (scrollOffset.y < 0) {
    if (aScrollableY < 0 &&
        cursorPos.y + scrollOffset.y < aVirtualBoundary.y + 1) {
      aOffset.y =
          std::max(aVirtualBoundary.y - cursorPos.y + 1, float(aScrollableY));
      scrollOffset.y -= aOffset.y;
    } else {
      aOffset.y = scrollOffset.y;
      scrollOffset.y = 0;
    }
  }

  return scrollOffset;
}

void CursorSimulator::UpdatePos(const LayoutDeviceIntPoint& aPos) {
  if (IsActive()) {
    mDelegate->UpdatePos(aPos);
  }
}

void CursorSimulator::CursorClick() {
  if (IsActive()) {
    mDelegate->CursorClick();
  }
}

void CursorSimulator::CursorDown() {
  if (IsActive()) {
    mDelegate->CursorDown();
  }
}

void CursorSimulator::CursorUp() {
  if (IsActive()) {
    mDelegate->CursorUp();
  }
}

void CursorSimulator::CursorMove() {
  if (IsActive()) {
    mDelegate->CursorMove();
  }
}

void CursorSimulator::CursorOut(bool aCheckActive) {
  if (!aCheckActive || IsActive()) {
    // Move the cursor position to K_INITIAL_POSITION so that it wonldn't hover on something
    LayoutDeviceIntPoint point;
    point.x = K_INITIAL_POSITION;
    point.y = K_INITIAL_POSITION;
    mDelegate->UpdatePos(point);
    mDelegate->CursorOut();
  }
}

void CursorSimulator::ShowContextMenu() {
  if (IsActive()) {
    mDelegate->ShowContextMenu();
  }
}

}  // namespace dom
}  // namespace mozilla
