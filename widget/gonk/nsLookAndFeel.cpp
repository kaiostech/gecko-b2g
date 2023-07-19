/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsLookAndFeel.h"
#include "nsStyleConsts.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "mozilla/gfx/2D.h"
#include "cutils/properties.h"
#include "nsITheme.h"

using namespace mozilla;

static const char16_t UNICODE_BULLET = 0x2022;

nsLookAndFeel::nsLookAndFeel() = default;

nsLookAndFeel::~nsLookAndFeel() {}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme,
                                       nscolor& aColor) {
  nsresult rv = NS_OK;

#define BASE_ACTIVE_COLOR NS_RGB(0xaa, 0xaa, 0xaa)
#define BASE_NORMAL_COLOR NS_RGB(0xff, 0xff, 0xff)
#define BASE_SELECTED_COLOR NS_RGB(0xaa, 0xaa, 0xaa)
#define BG_ACTIVE_COLOR NS_RGB(0xff, 0xff, 0xff)
#define BG_INSENSITIVE_COLOR NS_RGB(0xaa, 0xaa, 0xaa)
#define BG_NORMAL_COLOR NS_RGB(0xff, 0xff, 0xff)
#define BG_PRELIGHT_COLOR NS_RGB(0xee, 0xee, 0xee)
#define BG_SELECTED_COLOR NS_RGB(0x99, 0x99, 0x99)
#define DARK_NORMAL_COLOR NS_RGB(0x88, 0x88, 0x88)
#define FG_INSENSITIVE_COLOR NS_RGB(0x44, 0x44, 0x44)
#define FG_NORMAL_COLOR NS_RGB(0x00, 0x00, 0x00)
#define FG_PRELIGHT_COLOR NS_RGB(0x77, 0x77, 0x77)
#define FG_SELECTED_COLOR NS_RGB(0xaa, 0xaa, 0xaa)
#define LIGHT_NORMAL_COLOR NS_RGB(0xaa, 0xaa, 0xaa)
#define TEXT_ACTIVE_COLOR NS_RGB(0x99, 0x99, 0x99)
#define TEXT_NORMAL_COLOR NS_RGB(0x00, 0x00, 0x00)
#define TEXT_SELECTED_COLOR NS_RGB(0x00, 0x00, 0x00)

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // (except here at least TextSelectBackground and TextSelectForeground)
      // The CSS2 colors below are used.
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
      // still used
      aColor = BASE_SELECTED_COLOR;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      // still used
      aColor = TEXT_SELECTED_COLOR;
      break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:
      // active window border
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Activecaption:
      // active window caption background
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Appworkspace:
      // MDI background color
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Background:
      // desktop background
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Captiontext:
      // text in active window caption, size box, and scrollbar arrow box (!)
      aColor = FG_NORMAL_COLOR;
      break;
    case ColorID::Graytext:
      // disabled text in windows, menus, etc.
      aColor = FG_INSENSITIVE_COLOR;
      break;
    case ColorID::Highlight:
      // background of selected item
      aColor = BASE_SELECTED_COLOR;
      break;
    case ColorID::Highlighttext:
      // text of selected item
      aColor = TEXT_SELECTED_COLOR;
      break;
    case ColorID::Inactiveborder:
      // inactive window border
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Inactivecaption:
      // inactive window caption
      aColor = BG_INSENSITIVE_COLOR;
      break;
    case ColorID::Inactivecaptiontext:
      // text in inactive window caption
      aColor = FG_INSENSITIVE_COLOR;
      break;
    case ColorID::Infobackground:
      // tooltip background color
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Infotext:
      // tooltip text color
      aColor = TEXT_NORMAL_COLOR;
      break;
    case ColorID::Menu:
      // menu background
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::Menutext:
      // menu text
      aColor = TEXT_NORMAL_COLOR;
      break;
    case ColorID::Scrollbar:
      // scrollbar gray area
      aColor = BG_ACTIVE_COLOR;
      break;

    case ColorID::Threedface:
    case ColorID::Buttonface:
      // 3-D face color
      aColor = BG_NORMAL_COLOR;
      break;

    case ColorID::Buttontext:
      // text on push buttons
      aColor = TEXT_NORMAL_COLOR;
      break;

    case ColorID::Buttonhighlight:
      // 3-D highlighted edge color
    case ColorID::Threedhighlight:
      // 3-D highlighted outer edge color
      aColor = LIGHT_NORMAL_COLOR;
      break;

    case ColorID::Threedlightshadow:
      // 3-D highlighted inner edge color
      aColor = BG_NORMAL_COLOR;
      break;

    case ColorID::Buttonshadow:
      // 3-D shadow edge color
    case ColorID::Threedshadow:
      // 3-D shadow inner edge color
      aColor = DARK_NORMAL_COLOR;
      break;

    case ColorID::Threeddarkshadow:
      // 3-D shadow outer edge color
      aColor = NS_RGB(0, 0, 0);
      break;

    case ColorID::Window:
    case ColorID::Windowframe:
      aColor = BG_NORMAL_COLOR;
      break;

    case ColorID::Windowtext:
      aColor = FG_NORMAL_COLOR;
      break;

    case ColorID::MozEventreerow:
    case ColorID::Field:
      aColor = BASE_NORMAL_COLOR;
      break;
    case ColorID::Fieldtext:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case ColorID::MozDialog:
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::MozDialogtext:
      aColor = FG_NORMAL_COLOR;
      break;
    case ColorID::MozDragtargetzone:
      aColor = BG_SELECTED_COLOR;
      break;
    case ColorID::MozButtonhoverface:
      aColor = BG_PRELIGHT_COLOR;
      break;
    case ColorID::MozButtonhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    case ColorID::MozCellhighlight:
    case ColorID::Selecteditem:
      aColor = BASE_ACTIVE_COLOR;
      break;
    case ColorID::MozCellhighlighttext:
    case ColorID::Selecteditemtext:
      aColor = TEXT_ACTIVE_COLOR;
      break;
    case ColorID::MozMenuhover:
      aColor = BG_PRELIGHT_COLOR;
      break;
    case ColorID::MozMenuhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    case ColorID::MozOddtreerow:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::MozNativehyperlinktext:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::MozComboboxtext:
      aColor = TEXT_NORMAL_COLOR;
      break;
    case ColorID::MozCombobox:
      aColor = BG_NORMAL_COLOR;
      break;
    case ColorID::MozMenubarhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    default:
      /* default color is BLACK */
      aColor = 0;
      rv = NS_ERROR_FAILURE;
      break;
  }

  return rv;
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult rv = NS_OK;

  switch (aID) {
    case IntID::CaretBlinkTime:
      aResult = 500;
      break;

    case IntID::CaretWidth:
      aResult = 1;
      break;

    case IntID::ShowCaretDuringSelection:
      aResult = 0;
      break;

    case IntID::SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;

    case IntID::SubmenuDelay:
      aResult = 200;
      break;

    case IntID::TooltipDelay:
      aResult = 500;
      break;

    case IntID::MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;

    case IntID::ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;

    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
      aResult = int32_t(StyleTextDecorationStyle::Solid);
      break;

    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = int32_t(StyleTextDecorationStyle::None);
      break;

    case IntID::SpellCheckerUnderlineStyle:
      aResult = int32_t(StyleTextDecorationStyle::Wavy);
      break;

    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;

// TODO: implement physical home button??
#if 0
        case IntID::PhysicalHomeButton: {
            char propValue[PROPERTY_VALUE_MAX];
            property_get("ro.moz.has_home_button", propValue, "1");
            aResult = atoi(propValue);
            break;
        }
#endif

    case IntID::ContextMenuOffsetVertical:
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 2;
      break;

    case IntID::UseOverlayScrollbars:
      aResult = 1;
      break;

    default:
      aResult = 0;
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult res = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

/*virtual*/
bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                                  gfxFontStyle& aFontStyle) {
  aFontName.AssignLiteral("\"Fira Sans\"");
  aFontStyle.style = mozilla::FontSlantStyle::NORMAL;
  aFontStyle.weight = mozilla::FontWeight::NORMAL;
  aFontStyle.stretch = mozilla::FontStretch::NORMAL;
  aFontStyle.size = 9.0 * 96.0f / 72.0f;
  aFontStyle.systemFont = true;
  return true;
}

/*virtual*/
bool nsLookAndFeel::GetEchoPasswordImpl() { return true; }

/*virtual*/
uint32_t nsLookAndFeel::GetPasswordMaskDelayImpl() {
  // Same value on Android framework
  return 1500;
}

/* virtual */
char16_t nsLookAndFeel::GetPasswordCharacterImpl() { return UNICODE_BULLET; }

// No native theme on Gonk.
already_AddRefed<nsITheme> do_GetNativeTheme() { return nullptr; }
