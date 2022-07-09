/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageError.h"
#include "mozilla/dom/MobileMessageErrorBinding.h"
#include "MmsMessage.h"
#include "SmsMessage.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileMessageError, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileMessageError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileMessageError)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileMessageError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileMessageError::MobileMessageError(nsPIDOMWindowInner* aWindow,
                                       const nsAString& aName, SmsMessage* aSms)
    : mWindow(aWindow), mMessage(aName), mSms(aSms), mMms(nullptr) {}

MobileMessageError::MobileMessageError(nsPIDOMWindowInner* aWindow,
                                       const nsAString& aName, MmsMessage* aMms)
    : mWindow(aWindow), mMessage(aName), mSms(nullptr), mMms(aMms) {}

void MobileMessageError::GetData(OwningSmsMessageOrMmsMessage& aRetVal) const {
  if (mSms) {
    aRetVal.SetAsSmsMessage() = mSms;
    return;
  }

  if (mMms) {
    aRetVal.SetAsMmsMessage() = mMms;
    return;
  }

  MOZ_CRASH("Bad object with invalid mSms and mMms.");
}

JSObject* MobileMessageError::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return MobileMessageError_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* MobileMessageError::GetParentObject() const {
  return mWindow;
}

MobileMessageError::~MobileMessageError(){};

}  // namespace dom
}  // namespace mozilla
