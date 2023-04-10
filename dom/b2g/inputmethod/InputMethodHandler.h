/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodHandler_h
#define mozilla_dom_InputMethodHandler_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsIEditableSupport.h"

namespace mozilla {
namespace dom {

class Promise;
class InputMethodRequest;
class InputMethodServiceChild;

class InputMethodHandler final : public nsIEditableSupportListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORTLISTENER

  static already_AddRefed<InputMethodHandler> Create(
      Promise* aPromise, InputMethodServiceChild* aServiceChild);
  static already_AddRefed<InputMethodHandler> Create(
      InputMethodServiceChild* aServiceChild);

  nsresult SetComposition(uint64_t aId, const nsAString& aText, int32_t aOffset,
                          int32_t aLength);
  nsresult EndComposition(uint64_t aId, const nsAString& aText);
  nsresult Keydown(uint64_t aId, const nsAString& aKey);
  nsresult Keyup(uint64_t aId, const nsAString& aKey);
  nsresult SendKey(uint64_t aId, const nsAString& aKey);
  nsresult DeleteBackward(uint64_t aId);

  void RemoveFocus(uint64_t aId);
  nsresult GetSelectionRange(uint64_t aId);
  nsresult GetText(uint64_t aId, int32_t aOffset, int32_t aLength);
  void SetValue(uint64_t aId, const nsAString& aValue);
  void ClearAll(uint64_t aId);
  nsresult ReplaceSurroundingText(uint64_t aId, const nsAString& aText,
                                  int32_t aOffset, int32_t aLength);

 private:
  explicit InputMethodHandler(Promise* aPromise,
                              InputMethodServiceChild* aServiceChild);
  InputMethodHandler(InputMethodServiceChild* aServiceChild);
  ~InputMethodHandler() = default;

  void Initialize();
  void SendRequest(uint64_t aId, const InputMethodRequest& aRequest);

  RefPtr<Promise> mPromise;
  RefPtr<InputMethodServiceChild> mServiceChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethodHandler_h
