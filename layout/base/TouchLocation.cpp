/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TouchLocation.h"

#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_touch.h"
#include "mozilla/dom/TouchEvent.h"
#include "nsDOMTokenList.h"

#include <vector>

#if defined(MOZ_WIDGET_GONK)
#  include "cutils/properties.h"
#endif

namespace mozilla {

static bool IsTouchLocationEnabled() {
  return StaticPrefs::touch_show_location();
}

class TouchLocationCollection {
 public:
  void Append(TouchLocation* aItem) { mList.push_back(aItem); }

  void Remove(TouchLocation* aItem) {
    auto position = std::find(mList.begin(), mList.end(), aItem);
    if (position != mList.end()) {
      mList.erase(position);
    }
  }

  void HideAll(TouchLocation* aExceptItem = nullptr) {
    for (auto& item : mList) {
      if (item == aExceptItem) {
        continue;
      }
      item->SetEnable(false);
    }
  }

  std::vector<TouchLocation*> mList;
};

static TouchLocationCollection gTouchLocationCollection;

const uint32_t TouchLocation::sSupportFingers = 10;

TouchLocation::TouchLocation(PresShell* aPresShell) : mPresShell(aPresShell) {
  if (mPresShell) {
    InjectElement(mPresShell->GetDocument());
  }

  gTouchLocationCollection.Append(this);
}

TouchLocation::~TouchLocation() {
  gTouchLocationCollection.Remove(this);

  if (mPresShell) {
    RemoveElement(mPresShell->GetDocument());
  }
}

void TouchLocation::InjectElement(dom::Document* aDocument) {
  // Content structure of TouchLocation
  // <div class="moz-touchlocation">  <- TouchElement()
  //   <div class="cross start">        <- StartElement(0)
  //   <div class="cross start">        <- StartElement(1)
  //   ...
  //   <div class="cross start">        <- StartElement(sSupportFingers - 1)
  //   <div class="cross">              <- CrossElement(0)
  //   <div class="cross">              <- CrossElement(1)
  //   ...
  //   <div class="cross">              <- CrossElement(sSupportFingers - 1)

  ErrorResult rv;
  nsCOMPtr<dom::Element> element = aDocument->CreateHTMLElement(nsGkAtoms::div);
  element->ClassList()->Add(u"moz-touchlocation"_ns, rv);
  element->ClassList()->Add(u"none"_ns, rv);

  // Create for start element
  for (uint32_t i = 0; i < sSupportFingers; ++i) {
    nsCOMPtr<dom::Element> cross = aDocument->CreateHTMLElement(nsGkAtoms::div);
    cross->ClassList()->Add(u"cross"_ns, rv);
    cross->ClassList()->Add(u"start"_ns, rv);
    cross->ClassList()->Add(u"none"_ns, rv);
    element->AppendChildTo(cross, false, IgnoreErrors());
  }

  for (uint32_t i = 0; i < sSupportFingers; ++i) {
    nsCOMPtr<dom::Element> cross = aDocument->CreateHTMLElement(nsGkAtoms::div);
    cross->ClassList()->Add(u"cross"_ns, rv);
    cross->ClassList()->Add(u"none"_ns, rv);
    element->AppendChildTo(cross, false, IgnoreErrors());
  }

  mTouchElementHolder = aDocument->InsertAnonymousContent(*element, /* aForce = */ false, rv);
}

void TouchLocation::RemoveElement(dom::Document* aDocument) {
  aDocument->RemoveAnonymousContent(*mTouchElementHolder);
}

void TouchLocation::HandleTouchList(EventMessage aMessage,
                                    const TouchArray& aTouchList) {
  // We leave the latest status on the screen for purpose.
  if (!aTouchList.Length()) {
    return;
  }

  SetEnable(IsTouchLocationEnabled());

  if (!mEnabled) {
    // do nothing;
    return;
  }

  MOZ_ASSERT(aTouchList.Length() <= sSupportFingers,
             "We only support 10 fingers.");

  uint32_t liveMask = 0;

  for (auto& touch : aTouchList) {
    uint32_t id = touch->Identifier();

    if (id >= sSupportFingers) {
      // ASSERT(id < sSupportFingers);
      continue;
    }

    liveMask |= (1 << id);

    if (!touch->mChanged) {
      continue;
    }

    // device pixel = dp * lcd density / 160.
    int lcd_density = 160;
#if defined(MOZ_WIDGET_GONK)
    char lcd_density_str[PROPERTY_VALUE_MAX] = "0";
    property_get("ro.sf.lcd_density", lcd_density_str, "0");
    lcd_density = atoi(lcd_density_str);
#endif
    uint32_t x = touch->mRefPoint.x * 160 / lcd_density;
    uint32_t y = touch->mRefPoint.y * 160 / lcd_density;
    if (aMessage == eTouchStart) {
      SetStartElementStyle(id, x, y);
    }
    SetCrossElementStyle(id, x, y);
  }

  // hide others invsible
  for (uint32_t i = 0; i < sSupportFingers; i++) {
    if (!(liveMask & (1 << i))) {
      HideStartElement(i);
      HideCrossElement(i);
    }
  }
}

void TouchLocation::SetEnable(bool aEnabled) {
  if (mEnabled == aEnabled) {
    return;
  }

  ErrorResult rv;
  TouchElement().ClassList()->Toggle(u"none"_ns, dom::Optional<bool>(!aEnabled),
                                     rv);
  mEnabled = aEnabled;

  if (mEnabled) {
    // Hide the other ToucLocations to make sure there is
    // only one TouchLocation presetned at the same time.
    gTouchLocationCollection.HideAll(this);
  }
}

void TouchLocation::HideStartElement(TouchLocation::FingerStartId aId) {
  MOZ_ASSERT(aId < sSupportFingers, "We must have valid Id.");
  SetChildEnableHelper(aId, nullptr, false);
}

void TouchLocation::HideCrossElement(TouchLocation::FingerCrossId aId) {
  MOZ_ASSERT(aId < sSupportFingers, "We must have valid Id.");
  SetChildEnableHelper(aId, nullptr, false);
}

void TouchLocation::SetStartElementStyle(TouchLocation::FingerStartId aId,
                                         nscoord aX, nscoord aY) {
  MOZ_ASSERT(aId < sSupportFingers, "We must have valid Id.");
  dom::Element* start = StartElement(aId);

  nsAutoString styleStr;
  // Set the offset of the start.
  styleStr.AppendPrintf("--cross-x: %dpx; --cross-y: %dpx;", aX, aY);

  ErrorResult rv;
  start->SetAttribute(u"style"_ns, styleStr, rv);

  SetChildEnableHelper(aId, start, true);
}

void TouchLocation::SetCrossElementStyle(TouchLocation::FingerCrossId aId,
                                         nscoord aX, nscoord aY) {
  MOZ_ASSERT(aId < sSupportFingers, "We must have valid Id.");
  dom::Element* cross = CrossElement(aId);

  nsAutoString styleStr;
  // Set the offset of the cross.
  styleStr.AppendPrintf("--cross-x: %dpx; --cross-y: %dpx;", aX, aY);
  // Set the information string of the cross.
  styleStr.AppendPrintf("--cross-x-n: %d; --cross-y-n: %d;", aX, aY);

  ErrorResult rv;
  cross->SetAttribute(u"style"_ns, styleStr, rv);

  SetChildEnableHelper(aId, cross, true);
}

void TouchLocation::SetChildEnableHelper(TouchLocation::ChildId aId,
                                         dom::Element* aElement,
                                         bool aEnabled) {
  MOZ_ASSERT(aId.mId < sSupportFingers, "We must have valid Id.");

  const uint32_t idx = aId.mId;
  const uint32_t mask = 1 << idx;
  const bool enable = !!(mChildEnabled & mask);

  if (enable == aEnabled) {
    return;
  }

  // aCross may be null, call CrossElement to retrivet elements.
  dom::Element* element = aElement ? aElement : _NthChildElement(aId);

  MOZ_ASSERT(element);

  if (element) {
    ErrorResult rv;
    if (aEnabled) {
      element->ClassList()->Remove(u"none"_ns, rv);
      mChildEnabled |= mask;
    } else {
      element->ClassList()->Add(u"none"_ns, rv);
      mChildEnabled &= ~mask;
    }
  }
}

}  // namespace mozilla
