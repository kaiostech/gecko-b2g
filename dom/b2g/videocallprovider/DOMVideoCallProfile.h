/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domvideocallprofile_h__
#define mozilla_dom_domvideocallprofile_h__

#include "mozilla/dom/VideoCallProviderBinding.h"
#include "nsIVideoCallProvider.h"

namespace mozilla {
namespace dom {

class DOMVideoCallProfile final : public nsIVideoCallProfile {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIDEOCALLPROFILE

  DOMVideoCallProfile(VideoCallQuality aQuality, VideoCallState aState);

  void Update(nsIVideoCallProfile* aProfile);

 private:
  // Don't try to use the default constructor.
  DOMVideoCallProfile() {}

  ~DOMVideoCallProfile() {}

 private:
  VideoCallQuality mQuality;
  VideoCallState mState;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_domvideocallprofile_h__
