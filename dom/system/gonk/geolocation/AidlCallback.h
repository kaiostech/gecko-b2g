/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkGPSGeolocationAidlCallback_h
#define GonkGPSGeolocationAidlCallback_h

#include <android/hardware/gnss/BnGnssCallback.h>
#include <android/hardware/gnss/BnAGnssCallback.h>
#include <android/hardware/gnss/visibility_control/BnGnssVisibilityControlCallback.h>

using android::binder::Status;
using android::hardware::gnss::GnssLocation;

#define GNSS android::hardware::gnss

// See b2g-sysroot/include/android/hardware/gnss/IGnssCallback.h

class AidlCallback : virtual public GNSS::BnGnssCallback {
 public:
  Status gnssSetCapabilitiesCb(int32_t capabilities) override;
  Status gnssStatusCb(IGnssCallback::GnssStatusValue status) override;
  Status gnssSvStatusCb(
      const std::vector<IGnssCallback::GnssSvInfo>& svInfoList) override;
  Status gnssLocationCb(const GnssLocation& location) override;
  Status gnssNmeaCb(int64_t timestamp, const ::std::string& nmea) override;
  Status gnssAcquireWakelockCb() override;
  Status gnssReleaseWakelockCb() override;
  Status gnssSetSystemInfoCb(
      const IGnssCallback::GnssSystemInfo& info) override;
  Status gnssRequestTimeCb() override;
  Status gnssRequestLocationCb(bool independentFromGnss,
                               bool isUserEmergency) override;
};

class AidlVisibilityControlCallback
    : virtual public GNSS::visibility_control::BnGnssVisibilityControlCallback {
 public:
  Status nfwNotifyCb(
      const GNSS::visibility_control::IGnssVisibilityControlCallback::
          NfwNotification& notification) override;
  Status isInEmergencySession(bool* _aidl_return) override;
};

class AidlAGnssCallback : virtual public GNSS::BnAGnssCallback {
 public:
  Status agnssStatusCb(GNSS::IAGnssCallback::AGnssType type,
                       GNSS::IAGnssCallback::AGnssStatusValue status) override;
};

#endif  // GonkGPSGeolocationAidlCallback_h
