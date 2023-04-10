/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethod_h
#define mozilla_dom_InputMethod_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/InputMethodBinding.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;
class InputMethodServiceChild;

class InputMethod final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(InputMethod)

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  explicit InputMethod(nsIGlobalObject* aGlobal);

  // WebIDL methods
  already_AddRefed<Promise> SetComposition(const nsAString& aText,
                                           const Optional<int32_t>& aOffset,
                                           const Optional<int32_t>& aLength);
  already_AddRefed<Promise> EndComposition(const Optional<nsAString>& aText);

  already_AddRefed<Promise> SendKey(const nsAString& aKey);
  already_AddRefed<Promise> Keydown(const nsAString& aKey);
  already_AddRefed<Promise> Keyup(const nsAString& aKey);
  already_AddRefed<Promise> DeleteBackward();

  void RemoveFocus();
  already_AddRefed<Promise> GetSelectionRange();
  already_AddRefed<Promise> GetText(const Optional<int32_t>& aOffset,
                                    const Optional<int32_t>& aLength);
  void SetValue(const nsAString& aValue);
  void ClearAll();
  already_AddRefed<Promise> ReplaceSurroundingText(
      const nsAString& aText, const Optional<int32_t>& aOffset,
      const Optional<int32_t>& aLength);

 protected:
  ~InputMethod() = default;
  void Init();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<InputMethodServiceChild> mServiceChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethod_h
