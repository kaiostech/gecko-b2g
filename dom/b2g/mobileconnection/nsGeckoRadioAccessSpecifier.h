/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsGeckoRadioAccessSpecifier_h
#define mozilla_dom_nsGeckoRadioAccessSpecifier_h

#include "nsIGeckoRadioAccessSpecifier.h"
namespace mozilla {
namespace dom {

class GeckoRadioAccessSpecifier final : public nsIGeckoRadioAccessSpecifier {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGECKORADIOACCESSSPECIFIER

  GeckoRadioAccessSpecifier() {}
  GeckoRadioAccessSpecifier(int32_t aRadioAccessNetwork, const nsAString& aBands, const nsAString& aChannels);


 private:
  ~GeckoRadioAccessSpecifier() { }

 private:
  int32_t mRadioAccessNetwork;
  nsString mBands;
  nsString mChannels;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_nsGeckoRadioAccessSpecifier_h
