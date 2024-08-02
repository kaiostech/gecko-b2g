/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GonkGPSGeolocationProvider_h
#define GonkGPSGeolocationProvider_h

#include "GonkEvents.h"

#include "android/hardware/gnss/1.1/IGnss.h"
#include "android/hardware/gnss/2.0/IGnss.h"
#if defined(AIDL_GNSS)
#  include "android/hardware/gnss/IGnss.h"
#endif
#include "android/hardware/gnss/1.0/IGnss.h"
#include "android/hardware/gnss/2.0/IAGnss.h"
#include "android/hardware/gnss/2.0/IAGnssRil.h"
#include "android/hardware/gnss/visibility_control/1.0/IGnssVisibilityControl.h"

#include "nsCOMPtr.h"
#include "nsIDOMGeoPosition.h"
#include "nsIGeolocationProvider.h"
#include "nsISettings.h"
#ifdef MOZ_B2G_RIL
#  include "nsIObserver.h"
#  include "nsIRadioInterfaceLayer.h"
#  include "nsITelephonyService.h"
#endif

#define GONK_GPS_GEOLOCATION_PROVIDER_CID            \
  {                                                  \
    0x48525ec5, 0x5a7f, 0x490a, {                    \
      0x92, 0x77, 0xba, 0x66, 0xe0, 0xd2, 0x2c, 0x8b \
    }                                                \
  }

#define GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID \
  "@mozilla.org/gonk-gps-geolocation-provider;1"

class GonkGPSGeolocationProvider : public nsIGeolocationProvider,
#ifdef MOZ_B2G_RIL
                                   public nsIObserver,
                                   public nsITelephonyListener,
#endif
                                   public nsISettingsGetResponse,
                                   public nsISettingsObserver,
                                   public nsISidlDefaultResponse {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIOBSERVER
#endif
  NS_DECL_NSISETTINGSGETRESPONSE
  NS_DECL_NSISETTINGSOBSERVER
  NS_DECL_NSISIDLDEFAULTRESPONSE

  static already_AddRefed<GonkGPSGeolocationProvider> GetSingleton();

 private:
  friend class UpdateLocationEvent;
  friend class HidlUpdateCapabilitiesEvent;
  friend class AidlUpdateCapabilitiesEvent;
  friend struct GnssCallback;
  friend struct AGnssCallback_V2_0;

  /* Client should use GetSingleton() to get the provider instance. */
  GonkGPSGeolocationProvider();
  GonkGPSGeolocationProvider(const GonkGPSGeolocationProvider&);
  GonkGPSGeolocationProvider& operator=(const GonkGPSGeolocationProvider&);
  virtual ~GonkGPSGeolocationProvider();

  static GonkGPSGeolocationProvider* sSingleton;
#ifdef MOZ_B2G_RIL
  // APN setting key, could either be "ril.data.apn.sim1" or "ril.data.apn.sim2"
  static nsString sSettingRilDataApn;
  // APN setting key, could either be "ril.supl.apn.sim1" or "ril.supl.apn.sim2"
  static nsString sSettingRilSuplApn;
  // APN setting key, "ril.emergency.apn.sim1" or "..sim2"
  static nsString sSettingRilSuplEsApn;
  // APN setting key, "ril.supl.protocol.sim1" or "..sim2"
  static nsString sSettingRilSuplProtocol;
  // APN setting key, "ril.emergency.protocol.sim1" or "..sim2"
  static nsString sSettingRilSuplEsProtocol;
  // APN setting key, "ril.supl.roaming_protocol.sim1" or "..sim2"
  static nsString sSettingRilSuplRoamingProtocol;
  // APN setting key, "ril.emergency.roaming_protocol.sim1" or "..sim2"
  static nsString sSettingRilSuplEsRoamingProtocol;
  static nsCString sRilDataApn;
  static nsCString sRilSuplApn;
  static nsCString sRilSuplEsApn;
#endif

  void Init();
  void SetupGnssHal();
#if defined(AIDL_GNSS)
  void SetupAidlHal();
#endif
  void CleanupGnssHal();
  void StartGPS();
  void ShutdownGPS();

  void InjectLocation(double latitude, double longitude, float accuracy);
  // Set the GnssPositionMode to HAL, return true on success.
  bool SetPositionMode(bool enableHighAccuracy);

  NS_IMETHOD HandleSettings(nsISettingInfo* const info, bool isObserved);

#ifdef MOZ_B2G_RIL
  void UpdateApnObservers(uint32_t aServiceId);
  void UpdateRadioInterface();
  bool IsValidRilServiceId(uint32_t aServiceId);
  void SetupAGPS();
  int32_t GetDataConnectionState(bool isEmergencySupl = false);
  void AGpsDataConnectionOpen(bool isEmergencySupl = false);
  void HandleAGpsDataConnection(nsISupports* aNetworkInfo);
  void RequestDataConnection(bool isEmergencySupl = false);
  void ReleaseDataConnection(bool isEmergencySupl = false);
  static android::hardware::gnss::V2_0::IAGnss::ApnIpType GetApnIpType(
      nsAString& protocol);
  bool IsRoaming();
  void ListenTelephonyService(bool aStart);
  // Update network state to HAL either when the network state changed or when
  // the HAL want to know the state.
  // If it's triggered by network state changed, set aObserved = true.
  void UpdateNetworkState(nsISupports* aNetworkInfo, bool aObserved);
  NS_IMETHOD CallStateChanged(uint32_t length,
                              nsITelephonyCallInfo** allInfo) override;

  NS_IMETHOD EnumerateCallStateComplete(void) override { return NS_OK; }
  NS_IMETHOD EnumerateCallState(nsITelephonyCallInfo* info) override {
    return NS_OK;
  }
  NS_IMETHOD SupplementaryServiceNotification(
      uint32_t clientId, int32_t notification, int32_t code, int32_t index,
      int32_t type, const nsAString& number) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyCdmaCallWaiting(uint32_t clientId, const nsAString& number,
                                   uint16_t numberPresentation,
                                   const nsAString& name,
                                   uint16_t namePresentation) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyConferenceError(const nsAString& name,
                                   const nsAString& message) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyRingbackTone(bool playRingbackTone) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyTtyModeReceived(uint16_t mode) override { return NS_OK; }
  NS_IMETHOD NotifyTelephonyCoverageLosing(uint16_t type) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyRttModifyRequestReceived(uint32_t clientId,
                                            int32_t callIndex,
                                            uint16_t rttMode) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyRttModifyResponseReceived(uint32_t clientId,
                                             int32_t callIndex,
                                             uint16_t status) override {
    return NS_OK;
  }
  NS_IMETHOD NotifyRttMessageReceived(uint32_t clientId, int32_t callIndex,
                                      const nsAString& message) override {
    return NS_OK;
  }
  NS_IMETHOD NotifySrvccState(uint32_t clientId, int32_t state) override {
    return NS_OK;
  }
#endif  // MOZ_B2G_RIL

  // Whether IGNSS HAL is ready to handle location callbacks
  // Framework activate/deactivate HAL when Geolocation setting is toggled.
  // When Geolocation setting is on,
  //  - IGnss::setCallback_2_0()
  //  - IGnssVisibilityControl::enableNfwLocationAccess({"b2g_system"})
  // When Geolocation setting is off,
  //  - IGnss::cleanup()
  //  - IGnssVisibilityControl::enableNfwLocationAccess({})
  bool mGnssHalReady;

  bool mStarted;
  bool mSupportsScheduling;
  bool mSupportsSingleShot;
  bool mSupportsTimeInjection;
  bool mSupportsMSB;
  bool mSupportsMSA;

  nsCOMPtr<nsIGeolocationUpdate> mLocationCallback;
  nsCOMPtr<nsIDOMGeoPosition> mLastGPSPosition;
  nsCOMPtr<nsIGeolocationProvider> mNetworkLocationProvider;

#ifdef MOZ_B2G_RIL
  uint32_t mRilDataServiceId;
  // mNumberOfRilServices indicates how many SIM slots supported on device, and
  // RadioInterfaceLayer.js takes responsibility to set up the corresponding
  // preference value.
  uint32_t mNumberOfRilServices;
  nsCOMPtr<nsIRadioInterface> mRadioInterface;
  int32_t mSuplNetId;
  int32_t mSuplEsNetId;
  int32_t mActiveNetId;
  int32_t mActiveType;
  uint16_t mActiveCapabilities;
#endif
  bool mEnableHighAccuracy;

  struct GnssDeathRecipient
      : virtual public android::hardware::hidl_death_recipient {
    // hidl_death_recipient interface
    virtual void serviceDied(
        uint64_t cookie,
        const android::wp<android::hidl::base::V1_0::IBase>& who) override;
  };

  void InitGnssHal();

  android::sp<GnssDeathRecipient> mGnssHalDeathRecipient;
  android::sp<android::hardware::gnss::V1_0::IGnss> mGnssHal;
  android::sp<android::hardware::gnss::V1_1::IGnss> mGnssHal_V1_1;
  android::sp<android::hardware::gnss::V2_0::IGnss> mGnssHal_V2_0;
#if defined(AIDL_GNSS)
  android::sp<GNSS::IGnss> mAidlGnss;
  android::sp<GNSS::visibility_control::IGnssVisibilityControl>
      mAidlVisibilityControl;
  android::sp<GNSS::IAGnss> mAidlAgnss;
  android::sp<GNSS::IAGnssRil> mAidlAgnssRil;
#endif
  android::sp<android::hardware::gnss::V2_0::IAGnss> mAGnssHal_V2_0;
  android::sp<android::hardware::gnss::V2_0::IAGnssRil> mAGnssRilHal_V2_0;
  android::sp<
      android::hardware::gnss::visibility_control::V1_0::IGnssVisibilityControl>
      mGnssVisibilityControlHal;

  class NetworkLocationUpdate : public nsIGeolocationUpdate {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIGEOLOCATIONUPDATE

    NetworkLocationUpdate() {}

   private:
    virtual ~NetworkLocationUpdate() {}
  };
};

#endif /* GonkGPSGeolocationProvider_h */
