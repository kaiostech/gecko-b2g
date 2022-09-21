/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreferenceDelegateService_h
#define PreferenceDelegateService_h

#include "nsIGeckoBridge.h"
#include "nsCOMPtr.h"

namespace mozilla {

class PreferenceDelegateService final : public nsIPreferenceDelegate {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFERENCEDELEGATE

  static already_AddRefed<PreferenceDelegateService>
  ConstructPreferenceDelegate();

 private:
  PreferenceDelegateService();
  ~PreferenceDelegateService();

  void NotifyObservers(const nsAString& aKey);
};

}  // namespace mozilla

#endif  // PreferenceDelegateService_h
