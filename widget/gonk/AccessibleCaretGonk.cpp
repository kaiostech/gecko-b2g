/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"

#include "AccessibleCaretGonk.h"

#include "AccessibleCaretLogger.h"
#include "nsCaret.h"

#include "nsLayoutUtils.h"

namespace mozilla {

#undef AC_LOG
#define AC_LOG(message, ...) \
  AC_LOG_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

#undef AC_LOGV
#define AC_LOGV(message, ...) \
  AC_LOGV_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

AccessibleCaretGonk::~AccessibleCaretGonk() {}

void AccessibleCaretGonk::SetAppearance(Appearance aAppearance) {
  if (mForceAppearance != Appearance::None && aAppearance != Appearance::None &&
      aAppearance != Appearance::NormalNotShown &&
      mForceAppearance != aAppearance) {
    return AccessibleCaret::SetAppearance(mForceAppearance);
  }
  return AccessibleCaret::SetAppearance(aAppearance);
}

AccessibleCaret::PositionChangedResult AccessibleCaretGonk::SetPosition(
    nsIFrame* aFrame, int32_t aOffset) {
  if (!CustomContentContainerFrame()) {
    return PositionChangedResult::NotChanged;
  }

  nsRect imaginaryCaretRectInFrameBeforeClamp =
      nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  nsRect imaginaryCaretRectInFrame = nsLayoutUtils::ClampRectToScrollFrames(
      aFrame, imaginaryCaretRectInFrameBeforeClamp);

  if (imaginaryCaretRectInFrame.IsEmpty()) {
    // Don't bother to set the caret position since it's invisible.
    ClearCachedData();
    return PositionChangedResult::Invisible;
  }

  if (imaginaryCaretRectInFrameBeforeClamp.YMost() >
      imaginaryCaretRectInFrame.YMost()) {
    // Hide caret if the bottom of selected line is invisible.
    ClearCachedData();
    return PositionChangedResult::Invisible;
  }

  // SetCaretElementStyle() requires the input rect relative to the custom
  // content container frame.
  nsRect imaginaryCaretRectInContainerFrame = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, CustomContentContainerFrame(),
                               imaginaryCaretRectInContainerFrame);
  const float zoomLevel = GetZoomLevel();

  // Always update cached mImaginaryCaretRect (relative to the root frame)
  // because it can change when the caret is scrolled.
  mImaginaryCaretRect = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), mImaginaryCaretRect);

  mImaginaryCaretRectInContainerFrame = imaginaryCaretRectInContainerFrame;
  mImaginaryCaretReferenceFrame = aFrame;
  mZoomLevel = zoomLevel;

  const nscoord auWidth = nsPresContext::CSSPixelsToAppUnits(
      StaticPrefs::layout_accessiblecaret_width());
  const nscoord auHeight = nsPresContext::CSSPixelsToAppUnits(
      StaticPrefs::layout_accessiblecaret_height());

  nsRect boundingBox = RootFrame()->GetRectRelativeToSelf();
  float res = mPresShell->GetResolution();
  boundingBox.width /= res;
  boundingBox.height /= res;

  nsRect marginRect = imaginaryCaretRectInFrameBeforeClamp;
  marginRect.Inflate(nsMargin(0, auWidth / res, auHeight / res, auWidth / res));
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), marginRect);

  // Move the caret up if bottom overflow.
  if (marginRect.YMost() > boundingBox.YMost()) {
    imaginaryCaretRectInContainerFrame.height =
        imaginaryCaretRectInFrameBeforeClamp.height - auHeight / zoomLevel;
  }

  // Move the caret to other side if left or right overflow.
  if (marginRect.X() < boundingBox.X()) {
    SetForceAppearance(Appearance::Right);
  } else if (marginRect.XMost() > boundingBox.XMost()) {
    SetForceAppearance(Appearance::Left);
  } else {
    SetForceAppearance(Appearance::None);
  }

  SetCaretElementStyle(imaginaryCaretRectInContainerFrame, mZoomLevel);
  SetSelectionBarElementStyle(imaginaryCaretRectInContainerFrame, mZoomLevel);

  return PositionChangedResult::Position;
}

void AccessibleCaretGonk::SetCaretElementStyle(const nsRect& aRect,
                                               float aZoomLevel) {
  nsPoint position = CaretElementPosition(aRect);
  nsAutoString styleStr;
  // We can't use AppendPrintf here, because it does locale-specific
  // formatting of floating-point values.
  // We can't use AppendPrintf here, because it does locale-specific
  // formatting of floating-point values.
  styleStr.AppendLiteral("left: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(position.x));
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(position.y));
  styleStr.AppendLiteral("px; width: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_width() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; margin-left: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_margin_left() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; transition-duration: ");
  styleStr.AppendFloat(
      StaticPrefs::layout_accessiblecaret_transition_duration());
  styleStr.AppendLiteral("ms");

  styleStr.AppendLiteral("; --caret-width: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_width() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; --caret-height: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_height() /
                       aZoomLevel);
  styleStr.AppendLiteral("px");

  styleStr.AppendLiteral("; --caret-bar-width: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_sbar_width() /
                       aZoomLevel);
  styleStr.AppendLiteral("px");

  if (RefPtr<ComputedStyle> style =
          mImaginaryCaretReferenceFrame->ComputeSelectionStyle(
              nsISelectionController::SELECTION_ON)) {
    nscolor selectionBackgroundColor =
        style->GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);
    if (selectionBackgroundColor != NS_SAME_AS_FOREGROUND_COLOR) {
      // Set --caret-color to be the same as :selection { background: ... }
      styleStr.AppendPrintf("; --caret-color: rgba(%d, %d, %d, 1)",
                            NS_GET_R(selectionBackgroundColor),
                            NS_GET_G(selectionBackgroundColor),
                            NS_GET_B(selectionBackgroundColor));
    }
  }

  CaretElement().SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());

  // Set style string for children.
  SetTextOverlayElementStyle(aRect, aZoomLevel);
  SetCaretImageElementStyle(aRect, aZoomLevel);
}

void AccessibleCaretGonk::SetForceAppearance(Appearance aForceAppearance) {
  if (mForceAppearance == aForceAppearance) {
    return;
  }
  AC_LOG("%s: Force %s", __FUNCTION__, ToString(aForceAppearance).c_str());
  mForceAppearance = aForceAppearance;
}

}  // namespace mozilla
