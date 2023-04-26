/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMVideoCallProfile.h"

namespace mozilla {
namespace dom {

// nsIVideoCallProfile
NS_IMPL_ISUPPORTS(DOMVideoCallProfile, nsIVideoCallProfile)

DOMVideoCallProfile::DOMVideoCallProfile(VideoCallQuality aQuality,
                                         VideoCallState aState)
    : mQuality(aQuality), mState(aState) {}

void DOMVideoCallProfile::Update(nsIVideoCallProfile* aProfile) {
  uint16_t quality;
  uint16_t state;
  aProfile->GetQuality(&quality);
  aProfile->GetState(&state);

  mQuality = static_cast<VideoCallQuality>(quality);
  mState = static_cast<VideoCallState>(state);
}

NS_IMETHODIMP
DOMVideoCallProfile::GetQuality(uint16_t* aQuality) {
  *aQuality = static_cast<uint16_t>(mQuality);
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallProfile::GetState(uint16_t* aState) {
  *aState = static_cast<uint16_t>(mState);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
