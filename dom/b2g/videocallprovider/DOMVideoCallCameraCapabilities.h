/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domvideocallcameracapabilities_h__
#define mozilla_dom_domvideocallcameracapabilities_h__

#include "nsIVideoCallProvider.h"
#include "nsIVideoCallCallback.h"

namespace mozilla {
namespace dom {

class DOMVideoCallCameraCapabilities final
    : public nsIVideoCallCameraCapabilities {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIDEOCALLCAMERACAPABILITIES

  DOMVideoCallCameraCapabilities(uint16_t aWidth, uint16_t aHeight,
                                 bool aZoomSupported, float aMaxZoom);

  void Update(nsIVideoCallCameraCapabilities* aCapabilities);

 private:
  DOMVideoCallCameraCapabilities() {}

  ~DOMVideoCallCameraCapabilities() {}

 private:
  uint16_t mWidth;
  uint16_t mHeight;
  bool mZoomSupported;
  float mMaxZoom;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_domvideocallcameracapabilities_h__
