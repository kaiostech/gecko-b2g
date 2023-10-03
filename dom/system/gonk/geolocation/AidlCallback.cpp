/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AidlCallback.h"
#include "GonkEvents.h"
#include "prtime.h"  // for PR_Now()

#undef LOG
#undef ERR
#undef DBG
#define LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GonkGPS_GEO_AIDL", ##args)
#define ERR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, "GonkGPS_GEO_AIDL", ##args)
// TODO: Don't prrint DBG message when 'gDebug_isLoggingEnabled' is false
#define DBG(args...)                                                 \
  do {                                                               \
    if (true) {                                                      \
      __android_log_print(ANDROID_LOG_DEBUG, "GonkGPS_GEO", ##args); \
    }                                                                \
  } while (0)

// Gnss Callback

Status AidlCallback::gnssSetCapabilitiesCb(int32_t capabilities) {
  LOG("%s", __FUNCTION__);
  NS_DispatchToMainThread(new AidlUpdateCapabilitiesEvent(capabilities));
  return Status::ok();
}

Status AidlCallback::gnssStatusCb(IGnssCallback::GnssStatusValue status) {
  const char* statusString[] = {"NONE", "SESSION_BEGIN", "SESSION_END",
                                "ENGINE_ON", "ENGINE_OFF"};
  LOG("%s status=%s", __FUNCTION__, statusString[(int)status]);

  return Status::ok();
}

Status AidlCallback::gnssSvStatusCb(
    const std::vector<IGnssCallback::GnssSvInfo>& svInfoList) {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlCallback::gnssLocationCb(const GnssLocation& location) {
  LOG("%s", __FUNCTION__);

  // TODO(fabrice)
  // if (gDebug_isGPSLocationIgnored) {
  //   LOG("gnssLocationCb is ignored due to the developer setting");
  //   return Status::ok();
  // }

  const float kImpossibleAccuracy_m = 0.001;
  if (location.horizontalAccuracyMeters < kImpossibleAccuracy_m) {
    ERR("%s: horizontalAccuracyMeters < kImpossibleAccuracy_m", __FUNCTION__);
    return Status::ok();
  }

  RefPtr<nsGeoPosition> somewhere = new nsGeoPosition(
      location.latitudeDegrees, location.longitudeDegrees,
      location.altitudeMeters, location.horizontalAccuracyMeters,
      location.verticalAccuracyMeters, location.bearingDegrees,
      location.speedMetersPerSec, PR_Now() / PR_USEC_PER_MSEC);
  // Note above: Can't use location->timestamp as the time from the satellite is
  // a minimum of 16 secs old (see http://leapsecond.com/java/gpsclock.htm). All
  // code from this point on expects the gps location to be timestamped with the
  // current time, most notably: the geolocation service which respects
  // maximumAge set in the DOM JS.

  DBG("geo: GPS got a fix (%f, %f). accuracy: %f", location.latitudeDegrees,
      location.longitudeDegrees, location.horizontalAccuracyMeters);

  nsCOMPtr<nsIRunnable> runnable = new UpdateLocationEvent(somewhere);
  NS_DispatchToMainThread(runnable);

  return Status::ok();
}

Status AidlCallback::gnssNmeaCb(int64_t timestamp, const ::std::string& nmea) {
  // LOG("%s: %s", __FUNCTION__, nmea.c_str());
  return Status::ok();
}

Status AidlCallback::gnssAcquireWakelockCb() {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlCallback::gnssReleaseWakelockCb() {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlCallback::gnssSetSystemInfoCb(
    const IGnssCallback::GnssSystemInfo& info) {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlCallback::gnssRequestTimeCb() {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlCallback::gnssRequestLocationCb(bool independentFromGnss,
                                           bool isUserEmergency) {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

// Visibility Control Callback

Status AidlVisibilityControlCallback::nfwNotifyCb(
    const GNSS::visibility_control::IGnssVisibilityControlCallback::
        NfwNotification& notification) {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}

Status AidlVisibilityControlCallback::isInEmergencySession(bool* aidl_return) {
  LOG("%s", __FUNCTION__);
  *aidl_return = false;
  return Status::ok();
}

// AGnss callback

Status AidlAGnssCallback::agnssStatusCb(
    GNSS::IAGnssCallback::AGnssType type,
    GNSS::IAGnssCallback::AGnssStatusValue status) {
  LOG("%s", __FUNCTION__);
  return Status::ok();
}
