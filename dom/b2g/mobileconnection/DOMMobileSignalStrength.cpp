/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMMobileSignalStrength.h"
#include "mozilla/dom/MobileSignalStrengthBinding.h"
#include "jsapi.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMMobileSignalStrength, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMMobileSignalStrength)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMMobileSignalStrength)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMobileSignalStrength)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMMobileSignalStrength::DOMMobileSignalStrength(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {}

void DOMMobileSignalStrength::Update(nsIMobileSignalStrength* aInfo) {
  if (!aInfo) {
    return;
  }

  aInfo->GetLevel(&mLevel);
  aInfo->GetGsmSignalStrength(&mGsmSignalStrength);
  aInfo->GetGsmBitErrorRate(&mGsmSignalBitErrorRate);
  aInfo->GetCdmaDbm(&mCdmaDbm);
  aInfo->GetCdmaEcio(&mCdmaEcio);
  aInfo->GetCdmaEvdoDbm(&mCdmaEvdoDbm);
  aInfo->GetCdmaEvdoEcio(&mCdmaEvdoEcio);
  aInfo->GetCdmaEvdoSNR(&mCdmaEvdoSNR);
  aInfo->GetLteSignalStrength(&mLteSignalStrength);
  aInfo->GetLteRsrp(&mLteRsrp);
  aInfo->GetLteRsrq(&mLteRsrq);
  aInfo->GetLteRssnr(&mLteRssnr);
  aInfo->GetLteCqi(&mLteCqi);
  aInfo->GetLteTimingAdvance(&mLteTimingAdvance);
  aInfo->GetTdscdmaRscp(&mTdscdmaRscp);
}

JSObject* DOMMobileSignalStrength::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return DOMMobileSignalStrength_Binding::Wrap(aCx, this, aGivenProto);
}
