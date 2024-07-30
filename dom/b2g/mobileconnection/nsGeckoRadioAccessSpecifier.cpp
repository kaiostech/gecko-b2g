/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeckoRadioAccessSpecifier.h"
namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(GeckoRadioAccessSpecifier, nsIGeckoRadioAccessSpecifier)

GeckoRadioAccessSpecifier::GeckoRadioAccessSpecifier(int32_t aRadioAccessNetwork,
                                                         const nsAString& aBands,
                                                         const nsAString& aChannels)
    : mRadioAccessNetwork(aRadioAccessNetwork), mBands(aBands), mChannels(aChannels) {
  // The parent object is nullptr when GeckoRadioAccessSpecifier is created by this
  // way, and it won't be exposed to web content.
}

// nsIGeckoRadioAccessSpecifier
NS_IMETHODIMP
GeckoRadioAccessSpecifier::GetRadioAccessNetwork(int32_t* aRadioAccessNetwork) {
  *aRadioAccessNetwork = mRadioAccessNetwork;
  return NS_OK;
}

NS_IMETHODIMP
GeckoRadioAccessSpecifier::GetBands(nsAString& aBands) {
  aBands = mBands;
  return NS_OK;
}

NS_IMETHODIMP
GeckoRadioAccessSpecifier::GetChannels(nsAString& aChannels) {
  aChannels = mChannels;
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
