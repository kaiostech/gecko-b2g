/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileConnectionDeviceIdsBinding.h"
#include "mozilla/dom/DOMMobileConnectionDeviceIds.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMMobileConnectionDeviceIds, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMMobileConnectionDeviceIds)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMMobileConnectionDeviceIds)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMobileConnectionDeviceIds)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMMobileConnectionDeviceIds::DOMMobileConnectionDeviceIds(
    nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {
  mImei.SetIsVoid(true);
  mImeisv.SetIsVoid(true);
  mEsn.SetIsVoid(true);
  mMeid.SetIsVoid(true);
}

DOMMobileConnectionDeviceIds::DOMMobileConnectionDeviceIds(
    nsPIDOMWindowInner* aWindow, const nsAString& aImei,
    const nsAString& aImeisv, const nsAString& aEsn, const nsAString& aMeid)
    : mWindow(aWindow),
      mImei(aImei),
      mImeisv(aImeisv),
      mEsn(aEsn),
      mMeid(aMeid) {}

DOMMobileConnectionDeviceIds::~DOMMobileConnectionDeviceIds() {}

void DOMMobileConnectionDeviceIds::Update(
    nsIMobileDeviceIdentities* aIdentities) {
  if (!aIdentities) {
    return;
  } else {
    aIdentities->GetImei(mImei);
    aIdentities->GetImeisv(mImeisv);
    aIdentities->GetEsn(mEsn);
    aIdentities->GetMeid(mMeid);
  }
}

// DOMMobileConnectionDeviceIds WebIDL interface
void DOMMobileConnectionDeviceIds::GetImei(nsString& aRetVal) const {
  aRetVal = mImei;
}

void DOMMobileConnectionDeviceIds::GetImeisv(nsString& aRetVal) const {
  aRetVal = mImeisv;
}

void DOMMobileConnectionDeviceIds::GetEsn(nsString& aRetVal) const {
  aRetVal = mEsn;
}

void DOMMobileConnectionDeviceIds::GetMeid(nsString& aRetVal) const {
  aRetVal = mMeid;
}

JSObject* DOMMobileConnectionDeviceIds::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return DOMMobileConnectionDeviceIds_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* DOMMobileConnectionDeviceIds::GetParentObject() const {
  return mWindow;
}

}  // namespace dom
}  // namespace mozilla
