/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileMessageError_h
#define mozilla_dom_MobileMessageError_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class MmsMessage;
class OwningSmsMessageOrMmsMessage;
class SmsMessage;

class MobileMessageError final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MobileMessageError)

  MobileMessageError(nsPIDOMWindowInner* aWindow, const nsAString& aName,
                     SmsMessage* aSms);

  MobileMessageError(nsPIDOMWindowInner* aWindow, const nsAString& aName,
                     MmsMessage* aMms);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const;

  void GetData(OwningSmsMessageOrMmsMessage& aRetVal) const;

 private:
  ~MobileMessageError();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mMessage;
  RefPtr<SmsMessage> mSms;
  RefPtr<MmsMessage> mMms;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileMessageError_h
