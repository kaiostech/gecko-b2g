/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodService_h
#define mozilla_dom_InputMethodService_h

#include "mozilla/ClearOnShutdown.h"

class nsIEditableSupportListener;
class nsIEditableSupport;
class nsIInputContext;
namespace mozilla {

namespace dom {

class ContentParent;

class InputMethodService final : public nsIEditableSupport {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORT

  static already_AddRefed<InputMethodService> GetInstance();

  void HandleFocus(nsIEditableSupport* aEditableSupport,
                   nsIInputContext* aPropBag);
  void HandleBlur(nsIEditableSupport* aEditableSupport,
                  nsIInputContext* aPropBag);

  void RegisterEditableSupport(nsIEditableSupport* aEditableSupport) {
    MOZ_ASSERT(!mEditableSupport);
    mEditableSupport = aEditableSupport;
  }

  void UnregisterEditableSupport(nsIEditableSupport* aEditableSupport) {
    MOZ_ASSERT(mEditableSupport == aEditableSupport);
    mEditableSupport = nullptr;
  }

  nsIEditableSupport* GetRegisteredEditableSupport() {
    return mEditableSupport;
  }
  void HandleTextChanged(const nsAString& aText);
  void HandleSelectionChanged(uint32_t aStartOffset, uint32_t aEndOffset);

 private:
  InputMethodService() = default;
  ~InputMethodService() = default;
  nsCOMPtr<nsIEditableSupport> mEditableSupport;
};

}  // namespace dom
}  // namespace mozilla
#endif  // mozilla_dom_InputMethodService_h
