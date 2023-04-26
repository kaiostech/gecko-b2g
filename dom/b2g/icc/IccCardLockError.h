/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccCardLockError_h
#define mozilla_dom_IccCardLockError_h

//#include "mozilla/dom/DOMException.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class GlobalObject;

class IccCardLockError final : public nsISupports, public nsWrapperCache {
  nsCOMPtr<nsPIDOMWindowInner> mWindow;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IccCardLockError)

  IccCardLockError(nsPIDOMWindowInner* aWindow, const nsAString& aName,
                   int16_t aRetryCount);

  static already_AddRefed<IccCardLockError> Constructor(
      const GlobalObject& aGlobal, const nsAString& aName, int16_t aRetryCount);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  // WebIDL interface
  void GetErrorName(nsString& aRetval) const { aRetval = mName; }

  int16_t RetryCount() const { return mRetryCount; }

 private:
  virtual ~IccCardLockError();
  //  ~IccCardLockError() {}

 private:
  int16_t mRetryCount;
  nsString mName;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_IccCardLockError_h
