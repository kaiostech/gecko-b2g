/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileDeviceIdentities.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(MobileDeviceIdentities, nsIMobileDeviceIdentities)

MobileDeviceIdentities::MobileDeviceIdentities(const nsAString& aImei,
                                               const nsAString& aImeisv,
                                               const nsAString& aEsn,
                                               const nsAString& aMeid)
    : mImei(aImei), mImeisv(aImeisv), mEsn(aEsn), mMeid(aMeid) {
  // The parent object is nullptr when MobileDeviceIdentities is created by this
  // way, and it won't be exposed to web content.
}

void MobileDeviceIdentities::Update(nsIMobileDeviceIdentities* aIdentities) {
  if (!aIdentities) {
    return;
  }

  aIdentities->GetImei(mImei);
  aIdentities->GetImeisv(mImeisv);
  aIdentities->GetEsn(mEsn);
  aIdentities->GetMeid(mMeid);
}

// nsIMobileConnectionDeviceIds
NS_IMETHODIMP
MobileDeviceIdentities::GetImei(nsAString& aImei) {
  aImei = mImei;
  return NS_OK;
}

NS_IMETHODIMP
MobileDeviceIdentities::GetImeisv(nsAString& aImeisv) {
  aImeisv = mImeisv;
  return NS_OK;
}

NS_IMETHODIMP
MobileDeviceIdentities::GetEsn(nsAString& aEsn) {
  aEsn = mEsn;
  return NS_OK;
}

NS_IMETHODIMP
MobileDeviceIdentities::GetMeid(nsAString& aMeid) {
  aMeid = mMeid;
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
