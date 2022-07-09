/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EditableUtils_h
#define mozilla_dom_EditableUtils_h

namespace mozilla {
namespace dom {

class Element;

class EditableUtils final {
 public:
  static bool isFocusableElement(dom::Element* aElement);
  static bool isContentEditable(dom::Element* aElement);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EditableUtils_h
