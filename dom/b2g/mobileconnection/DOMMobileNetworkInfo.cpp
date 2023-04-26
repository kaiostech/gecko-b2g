/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMMobileNetworkInfo.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMMobileNetworkInfo, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMMobileNetworkInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMMobileNetworkInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMobileNetworkInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMMobileNetworkInfo::DOMMobileNetworkInfo(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {}

DOMMobileNetworkInfo::DOMMobileNetworkInfo(nsPIDOMWindowInner* aWindow,
                                           const nsAString& aShortName,
                                           const nsAString& aLongName,
                                           const nsAString& aMcc,
                                           const nsAString& aMnc,
                                           const nsAString& aState)
    : mWindow(aWindow),
      mShortName(aShortName),
      mLongName(aLongName),
      mMcc(aMcc),
      mMnc(aMnc),
      mState(aState) {}

void DOMMobileNetworkInfo::Update(nsIMobileNetworkInfo* aInfo) {
  if (!aInfo) {
    return;
  }

  aInfo->GetShortName(mShortName);
  aInfo->GetLongName(mLongName);
  aInfo->GetMcc(mMcc);
  aInfo->GetMnc(mMnc);
  aInfo->GetState(mState);
}

JSObject* DOMMobileNetworkInfo::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return DOMMobileNetworkInfo_Binding::Wrap(aCx, this, aGivenProto);
}
