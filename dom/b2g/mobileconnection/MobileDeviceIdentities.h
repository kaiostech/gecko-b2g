/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileDeviceIdentities_h
#define mozilla_dom_MobileDeviceIdentities_h

#include "nsIMobileDeviceIdentities.h"

namespace mozilla {
namespace dom {

class MobileDeviceIdentities final : public nsIMobileDeviceIdentities {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEDEVICEIDENTITIES

  MobileDeviceIdentities() {}
  MobileDeviceIdentities(const nsAString& aImei, const nsAString& aImeisv,
                         const nsAString& aEsn, const nsAString& aMeid);

  void Update(nsIMobileDeviceIdentities* aIdentities);

 private:
  ~MobileDeviceIdentities() {}

 private:
  nsString mImei;
  nsString mImeisv;
  nsString mEsn;
  nsString mMeid;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileDeviceIdentities_h
