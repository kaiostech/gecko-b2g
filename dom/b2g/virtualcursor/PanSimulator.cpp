/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PanSimulator.h"

#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StaticPrefs_apz.h"

#define K_MOVE_STEP_DEFAULT_OFFSET 7.0

extern mozilla::LazyLogModule gVirtualCursorLog;

NS_IMPL_ISUPPORTS(PanSimulator, nsIDOMEventListener, nsITimerCallback, nsINamed)

PanSimulator::PanSimulator(nsPIDOMWindowOuter* aWindow,
                           nsIDOMWindowUtils* aWinUtils)
    : mWindow(aWindow),
      mWinUtils(aWinUtils),
      mIsPanning(false),
      mRepeatTimes(0),
      mKeyCode(0),
      mPoint(0, 0) {
  MOZ_ASSERT(aWinUtils != nullptr);
  mTimer = do_CreateInstance("@mozilla.org/timer;1");

  mTouchInterval =
      Preferences::GetUint("dom.virtualcursor.pan_simulator.interval", 33);

  mPointOffset =
      Preferences::GetFloat("dom.virtualcursor.pan_simulator.step_offset",
                            K_MOVE_STEP_DEFAULT_OFFSET);

  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug, ("PanSimulator construct"));
}

void PanSimulator::Start(nsIntPoint aPoint) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("PanSimulator::Start current setting=%d", mIsPanning));
  if (mIsPanning) {
    return;
  }
  mIsPanning = true;
  mPoint = aPoint;
  nsCOMPtr<EventTarget> target = do_QueryInterface(mWindow);
  NS_ENSURE_TRUE_VOID(target);
  if (target) {
    target->AddEventListener(u"keydown"_ns, this, /* useCapture = */ true);
    target->AddEventListener(u"keyup"_ns, this, /* useCapture = */ true);
    target->AddEventListener(u"keypress"_ns, this, /* useCapture = */ true);
  }
}

void PanSimulator::Finish() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("PanSimulator::Finish current setting=%d", mIsPanning));
  if (!mIsPanning) {
    return;
  }
  mIsPanning = false;
  mTimer->Cancel();
  nsCOMPtr<EventTarget> target = do_QueryInterface(mWindow);
  NS_ENSURE_TRUE_VOID(target);
  if (target) {
    target->RemoveEventListener(u"keydown"_ns, this, /* useCapture = */ true);
    target->RemoveEventListener(u"keyup"_ns, this, /* useCapture = */ true);
    target->RemoveEventListener(u"keypress"_ns, this, /* useCapture = */ true);
  }
}

void PanSimulator::FireTouchEvent(int aType, nsIntPoint aPoint) {
  MOZ_ASSERT(aType >= eTouchStart && aType < eTouchMax);

  uint32_t touchState = 4;
  switch (aType) {
    case eTouchStart:
    case eTouchMove:
      // 2 -> TouchPointerState::TOUCH_CONTACT in nsIWidget.h
      touchState = 2;
      break;
    case eTouchEnd:
      // 4 -> TouchPointerState::TOUCH_REMOVE in nsIWidget.h
      touchState = 4;
      break;
    default:
      touchState = 4;
      break;
  }
  mWinUtils->SendNativeTouchPoint(0, touchState, aPoint.x, aPoint.y, 0, 0,
                                  nullptr);
}

nsIntPoint PanSimulator::CalculatePointPosition() {
  nsIntPoint p(mPoint);
  p.x += NS_lround(
      mRepeatTimes *
      ((mKeyCode == KeyboardEvent_Binding::DOM_VK_UP ||
        mKeyCode == KeyboardEvent_Binding::DOM_VK_DOWN)
           ? 0
           : (mKeyCode == KeyboardEvent_Binding::DOM_VK_LEFT ? mPointOffset
                                                             : -mPointOffset)));
  p.y += NS_lround(
      mRepeatTimes *
      ((mKeyCode == KeyboardEvent_Binding::DOM_VK_LEFT ||
        mKeyCode == KeyboardEvent_Binding::DOM_VK_RIGHT)
           ? 0
           : (mKeyCode == KeyboardEvent_Binding::DOM_VK_UP ? mPointOffset
                                                           : -mPointOffset)));

  return p;
}

NS_IMETHODIMP PanSimulator::Notify(nsITimer* aTimer) {
  nsIntPoint p = CalculatePointPosition();
  FireTouchEvent(eTouchMove, p);

  mRepeatTimes++;
  Unused << aTimer->InitWithCallback(this, mTouchInterval,
                                     nsITimer::TYPE_ONE_SHOT);
  return NS_OK;
}

NS_IMETHODIMP PanSimulator::HandleEvent(Event* aEvent) {
  MOZ_ASSERT(aEvent != nullptr);
  WidgetKeyboardEvent* keyEvent = aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_OK;
  }

  if (!mIsPanning) {
    return NS_OK;
  }

  if (keyEvent->mKeyCode != KeyboardEvent_Binding::DOM_VK_UP &&
      keyEvent->mKeyCode != KeyboardEvent_Binding::DOM_VK_DOWN &&
      keyEvent->mKeyCode != KeyboardEvent_Binding::DOM_VK_LEFT &&
      keyEvent->mKeyCode != KeyboardEvent_Binding::DOM_VK_RIGHT) {
    return NS_OK;
  }

  aEvent->PreventDefault();
  aEvent->StopPropagation();

  if (keyEvent->mMessage == eKeyPress) {
    return NS_OK;
  }

  if (keyEvent->mMessage == eKeyDown) {
    if (keyEvent->mIsRepeat || mKeyCode != 0) {
      return NS_OK;
    }
    FireTouchEvent(eTouchStart, mPoint);

    mKeyCode = keyEvent->mKeyCode;
    mRepeatTimes = 1;
    Unused << mTimer->InitWithCallback(this, mTouchInterval,
                                       nsITimer::TYPE_ONE_SHOT);
  } else if (keyEvent->mMessage == eKeyUp) {
    if (mKeyCode != keyEvent->mKeyCode) {
      return NS_OK;
    }
    mTimer->Cancel();
    nsIntPoint p = CalculatePointPosition();
    // If points of eTouchMove in this round are not far enough to touch start,
    // the eTouchEnd here will trigger a click event by APZC so actively
    // compensate the eTouchMove to threshold.
    float dpi = 1.0f;
    mWinUtils->GetDisplayDPI(&dpi);
    ScreenCoord threshold = StaticPrefs::apz_touch_start_tolerance() * dpi;
    nsIntPoint distance = p - mPoint;
    while ((float)distance.Length() <= (threshold) + mPointOffset) {
      FireTouchEvent(eTouchMove, p);
      mRepeatTimes++;
      p = CalculatePointPosition();
      distance = p - mPoint;
    }
    FireTouchEvent(eTouchEnd, p);
    mKeyCode = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
PanSimulator::GetName(nsACString& aName) {
  aName.AssignLiteral("PanSimulator");
  return NS_OK;
}
