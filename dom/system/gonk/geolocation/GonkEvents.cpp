/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkEvents.h"
#include "GonkGPSGeolocationProvider.h"

// HIDL
NS_IMETHODIMP HidlUpdateCapabilitiesEvent::Run() {
  using android::hardware::gnss::V2_0::IGnssCallback;

  RefPtr<GonkGPSGeolocationProvider> provider =
      GonkGPSGeolocationProvider::GetSingleton();

  provider->mSupportsScheduling =
      mCapabilities & IGnssCallback::Capabilities::SCHEDULING;
  provider->mSupportsMSB = mCapabilities & IGnssCallback::Capabilities::MSB;
  provider->mSupportsMSA = mCapabilities & IGnssCallback::Capabilities::MSA;
  provider->mSupportsSingleShot =
      mCapabilities & IGnssCallback::Capabilities::SINGLE_SHOT;
  provider->mSupportsTimeInjection =
      mCapabilities & IGnssCallback::Capabilities::ON_DEMAND_TIME;

  return NS_OK;
}

#ifdef AIDL_GNSS
// AIDL
NS_IMETHODIMP AidlUpdateCapabilitiesEvent::Run() {
  using android::hardware::gnss::IGnssCallback;

  RefPtr<GonkGPSGeolocationProvider> provider =
      GonkGPSGeolocationProvider::GetSingleton();

  provider->mSupportsScheduling =
      mCapabilities & IGnssCallback::CAPABILITY_SCHEDULING;
  provider->mSupportsMSB = mCapabilities & IGnssCallback::CAPABILITY_MSB;
  provider->mSupportsMSA = mCapabilities & IGnssCallback::CAPABILITY_MSA;
  provider->mSupportsSingleShot =
      mCapabilities & IGnssCallback::CAPABILITY_SINGLE_SHOT;
  provider->mSupportsTimeInjection =
      mCapabilities & IGnssCallback::CAPABILITY_ON_DEMAND_TIME;

  return NS_OK;
}
#endif  // AIDL_GNSS

// Common to HIDL and AIDL
NS_IMETHODIMP UpdateLocationEvent::Run() {
  RefPtr<GonkGPSGeolocationProvider> provider =
      GonkGPSGeolocationProvider::GetSingleton();
  nsCOMPtr<nsIGeolocationUpdate> callback = provider->mLocationCallback;
  provider->mLastGPSPosition = mPosition;
  if (callback) {
    callback->Update(mPosition);
  }
  return NS_OK;
}
