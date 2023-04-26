/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileNetworkInfo_h
#define mozilla_dom_MobileNetworkInfo_h

#include "nsIMobileNetworkInfo.h"

namespace mozilla {
namespace dom {

class MobileNetworkInfo final : public nsIMobileNetworkInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILENETWORKINFO

  MobileNetworkInfo(const nsAString& aShortName, const nsAString& aLongName,
                    const nsAString& aMcc, const nsAString& aMnc,
                    const nsAString& aState);

  explicit MobileNetworkInfo();

  void Update(nsIMobileNetworkInfo* aInfo);

 private:
  ~MobileNetworkInfo() {}

 private:
  nsString mShortName;
  nsString mLongName;
  nsString mMcc;
  nsString mMnc;
  nsString mState;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileNetworkInfo_h
