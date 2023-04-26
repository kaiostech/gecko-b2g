/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMVideoCallCameraCapabilities.h"

using namespace mozilla::dom;

// nsIVideoCallCameraCapabilities
NS_IMPL_ISUPPORTS(DOMVideoCallCameraCapabilities,
                  nsIVideoCallCameraCapabilities)

DOMVideoCallCameraCapabilities::DOMVideoCallCameraCapabilities(
    uint16_t aWidth, uint16_t aHeight, bool aZoomSupported, float aMaxZoom)
    : mWidth(aWidth),
      mHeight(aHeight),
      mZoomSupported(aZoomSupported),
      mMaxZoom(aMaxZoom) {}

void DOMVideoCallCameraCapabilities::Update(
    nsIVideoCallCameraCapabilities* aCapabilities) {
  aCapabilities->GetWidth(&mWidth);
  aCapabilities->GetHeight(&mHeight);
  aCapabilities->GetZoomSupported(&mZoomSupported);
  aCapabilities->GetMaxZoom(&mMaxZoom);
}

// nsIVideoCallCameraCapabilities

NS_IMETHODIMP
DOMVideoCallCameraCapabilities::GetWidth(uint16_t* aWidth) {
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallCameraCapabilities::GetHeight(uint16_t* aHeight) {
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallCameraCapabilities::GetZoomSupported(bool* aZoomSupported) {
  *aZoomSupported = mZoomSupported;
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallCameraCapabilities::GetMaxZoom(float* aMaxZoom) {
  *aMaxZoom = mMaxZoom;
  return NS_OK;
}
