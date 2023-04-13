/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SupplicantCallback_H
#define SupplicantCallback_H

#include "WifiCommon.h"

#if ANDROID_VERSION == 30
#include <android/hardware/wifi/supplicant/1.3/ISupplicantStaIfaceCallback.h>
#else
#include <android/hardware/wifi/supplicant/1.2/ISupplicantStaIfaceCallback.h>
#endif


using ISupplicantStaIfaceCallbackV1_0 =
    ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaIfaceCallback;
using ISupplicantStaIfaceCallbackV1_1 =
    ::android::hardware::wifi::supplicant::V1_1::ISupplicantStaIfaceCallback;
using ISupplicantStaIfaceCallbackV1_2 =
    ::android::hardware::wifi::supplicant::V1_2::ISupplicantStaIfaceCallback;
#if ANDROID_VERSION == 30
using ISupplicantStaIfaceCallbackV1_3 =
    ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaIfaceCallback;
#endif

BEGIN_WIFI_NAMESPACE

/**
 * Class to handle callback from supplicant station mode interface.
 * Implement supplicant hal version 1.0.
 */
class SupplicantStaIfaceCallback : public ISupplicantStaIfaceCallbackV1_0 {
 public:
  explicit SupplicantStaIfaceCallback(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<PasspointEventCallback>& aPasspointCallback,
      const android::sp<SupplicantStaManager> aSupplicantManager);

  /**
   * Used to indicate that a new network has been added.
   *
   * @param id Network ID allocated to the corresponding network.
   */
  Return<void> onNetworkAdded(uint32_t id) override;
  /**
   * Used to indicate that a network has been removed.
   *
   * @param id Network ID allocated to the corresponding network.
   */
  Return<void> onNetworkRemoved(uint32_t id) override;
  /**
   * Used to indicate a state change event on this particular iface. If this
   * event is triggered by a particular network, the |SupplicantNetworkId|,
   * |ssid|, |bssid| parameters must indicate the parameters of the network/AP
   * which cased this state transition.
   *
   * @param newState New State of the interface. This must be one of the |State|
   *        values above.
   * @param bssid BSSID of the corresponding AP which caused this state
   *        change event. This must be zero'ed if this event is not
   *        specific to a particular network.
   * @param id ID of the corresponding network which caused this
   *        state change event. This must be invalid (UINT32_MAX) if this
   *        event is not specific to a particular network.
   * @param ssid SSID of the corresponding network which caused this state
   *        change event. This must be empty if this event is not specific
   *        to a particular network.
   */
  Return<void> onStateChanged(
      ISupplicantStaIfaceCallback::State newState,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
      const ::android::hardware::hidl_vec<uint8_t>& ssid) override;
  /**
   * Used to indicate the result of ANQP (either for IEEE 802.11u Interworking
   * or Hotspot 2.0) query.
   *
   * @param bssid BSSID of the access point.
   * @param data ANQP data fetched from the access point.
   *        All the fields in this struct must be empty if the query failed.
   * @param hs20Data ANQP data fetched from the Hotspot 2.0 access point.
   *        All the fields in this struct must be empty if the query failed.
   */
  Return<void> onAnqpQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ISupplicantStaIfaceCallback::AnqpData& data,
      const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) override;
  /**
   * Used to indicate the result of Hotspot 2.0 Icon query.
   *
   * @param bssid BSSID of the access point.
   * @param fileName Name of the file that was requested.
   * @param data Icon data fetched from the access point.
   *        Must be empty if the query failed.
   */
  Return<void> onHs20IconQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ::android::hardware::hidl_string& fileName,
      const ::android::hardware::hidl_vec<uint8_t>& data) override;
  /**
   * Used to indicate a Hotspot 2.0 subscription remediation event.
   *
   * @param bssid BSSID of the access point.
   * @param osuMethod OSU method.
   * @param url URL of the server.
   */
  Return<void> onHs20SubscriptionRemediation(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::OsuMethod osuMethod,
      const ::android::hardware::hidl_string& url) override;
  /**
   * Used to indicate a Hotspot 2.0 imminent deauth notice.
   *
   * @param bssid BSSID of the access point.
   * @param reasonCode Code to indicate the deauth reason.
   *        Refer to section 3.2.1.2 of the Hotspot 2.0 spec.
   * @param reAuthDelayInSec Delay before reauthenticating.
   * @param url URL of the server.
   */
  Return<void> onHs20DeauthImminentNotice(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t reasonCode, uint32_t reAuthDelayInSec,
      const ::android::hardware::hidl_string& url) override;
  /**
   * Used to indicate the disconnection from the currently connected
   * network on this iface.
   *
   * @param bssid BSSID of the AP from which we disconnected.
   * @param locallyGenerated If the disconnect was triggered by
   *        wpa_supplicant.
   * @param reasonCode 802.11 code to indicate the disconnect reason
   *        from access point. Refer to section 8.4.1.7 of IEEE802.11 spec.
   */
  Return<void> onDisconnected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      bool locallyGenerated,
      ISupplicantStaIfaceCallback::ReasonCode reasonCode) override;
  /**
   * Used to indicate an association rejection recieved from the AP
   * to which the connection is being attempted.
   *
   * @param bssid BSSID of the corresponding AP which sent this
   *        reject.
   * @param statusCode 802.11 code to indicate the reject reason.
   *        Refer to section 8.4.1.9 of IEEE 802.11 spec.
   * @param timedOut Whether failure is due to timeout rather
   *        than explicit rejection response from the AP.
   */
  Return<void> onAssociationRejected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::StatusCode statusCode,
      bool timedOut) override;
  /**
   * Used to indicate the timeout of authentication to an AP.
   *
   * @param bssid BSSID of the corresponding AP.
   */
  Return<void> onAuthenticationTimeout(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  /**
   * Used to indicate an EAP authentication failure.
   */
  Return<void> onEapFailure() override;
  /**
   * Used to indicate the change of active bssid.
   * This is useful to figure out when the driver/firmware roams to a bssid
   * on its own.
   *
   * @param reason Reason why the bssid changed.
   * @param bssid BSSID of the corresponding AP.
   */
  Return<void> onBssidChanged(
      ISupplicantStaIfaceCallback::BssidChangeReason reason,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  /**
   * Used to indicate the success of a WPS connection attempt.
   */
  Return<void> onWpsEventSuccess() override;
  /**
   * Used to indicate the failure of a WPS connection attempt.
   *
   * @param bssid BSSID of the AP to which we initiated WPS
   *        connection.
   * @param configError Configuration error code.
   * @param errorInd Error indication code.
   */
  Return<void> onWpsEventFail(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::WpsConfigError configError,
      ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) override;
  /**
   * Used to indicate the overlap of a WPS PBC connection attempt.
   */
  Return<void> onWpsEventPbcOverlap() override;
  /**
   * Used to indicate that the external radio work can start now.
   *
   * @return id Identifier generated for the radio work request.
   */
  Return<void> onExtRadioWorkStart(uint32_t id) override;
  /**
   * Used to indicate that the external radio work request has timed out.
   *
   * @return id Identifier generated for the radio work request.
   */
  Return<void> onExtRadioWorkTimeout(uint32_t id) override;

 private:
  void NotifyStateChanged(uint32_t aState, const std::string& aSsid,
                          const std::string& aBssid);
  void NotifyConnected(const std::string& aSsid, const std::string& aBssid);
  void NotifyDisconnected(const std::string& aBssid, bool aLocallyGenerated,
                          uint32_t aReason);
  void NotifyAuthenticationFailure(uint32_t aReason, int32_t aErrorCode);
  void NotifyAssociationReject(const std::string& aBssid, uint32_t aStatusCode,
                               bool aTimeout);
  void NotifyTargetBssid(const std::string& aBssid);
  void NotifyAssociatedBssid(const std::string& aBssid);
  void NotifyAnqpQueryDone(
      const std::string& aBssid,
      const ISupplicantStaIfaceCallback::AnqpData& data,
      const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data);
  void NotifyWpsSuccess();
  void NotifyWpsFailure(const std::string& aBssid, uint16_t aConfigError,
                        uint16_t aErrorIndication);
  void NotifyWpsTimeout();
  void NotifyWpsOverlap();

  ISupplicantStaIfaceCallback::State mStateBeforeDisconnect;
  std::string mInterfaceName;
  android::sp<WifiEventCallback> mCallback;
  android::sp<PasspointEventCallback> mPasspointCallback;
  android::sp<SupplicantStaManager> mSupplicantManager;
};

/**
 * Class to handle callback from supplicant station mode interface.
 * Implement supplicant hal version 1.1.
 */
class SupplicantStaIfaceCallbackV1_1 : public ISupplicantStaIfaceCallbackV1_1 {
 public:
  explicit SupplicantStaIfaceCallbackV1_1(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<SupplicantStaIfaceCallback>& aSupplicantCallback);

  Return<void> onNetworkAdded(uint32_t id) override;
  Return<void> onNetworkRemoved(uint32_t id) override;
  Return<void> onStateChanged(
      ISupplicantStaIfaceCallback::State newState,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
      const ::android::hardware::hidl_vec<uint8_t>& ssid) override;
  Return<void> onAnqpQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ISupplicantStaIfaceCallback::AnqpData& data,
      const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) override;
  Return<void> onHs20IconQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ::android::hardware::hidl_string& fileName,
      const ::android::hardware::hidl_vec<uint8_t>& data) override;
  Return<void> onHs20SubscriptionRemediation(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::OsuMethod osuMethod,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onHs20DeauthImminentNotice(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t reasonCode, uint32_t reAuthDelayInSec,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onDisconnected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      bool locallyGenerated,
      ISupplicantStaIfaceCallback::ReasonCode reasonCode) override;
  Return<void> onAssociationRejected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::StatusCode statusCode,
      bool timedOut) override;
  Return<void> onAuthenticationTimeout(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onEapFailure() override;
  Return<void> onBssidChanged(
      ISupplicantStaIfaceCallback::BssidChangeReason reason,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onWpsEventSuccess() override;
  Return<void> onWpsEventFail(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::WpsConfigError configError,
      ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) override;
  Return<void> onWpsEventPbcOverlap() override;
  Return<void> onExtRadioWorkStart(uint32_t id) override;
  Return<void> onExtRadioWorkTimeout(uint32_t id) override;
  /**
   * Used to indicate an EAP authentication failure.
   */
  Return<void> onEapFailure_1_1(
      ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) override;

 private:
  std::string mInterfaceName;
  android::sp<WifiEventCallback> mCallback;
  android::sp<SupplicantStaIfaceCallback> mSupplicantCallback;
};

/**
 * Class to handle callback from supplicant station mode interface.
 * Implement supplicant hal version 1.2.
 */
class SupplicantStaIfaceCallbackV1_2 : public ISupplicantStaIfaceCallbackV1_2 {
 public:
  explicit SupplicantStaIfaceCallbackV1_2(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<SupplicantStaIfaceCallbackV1_1>& aSupplicantCallback);

  Return<void> onNetworkAdded(uint32_t id) override;
  Return<void> onNetworkRemoved(uint32_t id) override;
  Return<void> onStateChanged(
      ISupplicantStaIfaceCallback::State newState,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
      const ::android::hardware::hidl_vec<uint8_t>& ssid) override;
  Return<void> onAnqpQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ISupplicantStaIfaceCallback::AnqpData& data,
      const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) override;
  Return<void> onHs20IconQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ::android::hardware::hidl_string& fileName,
      const ::android::hardware::hidl_vec<uint8_t>& data) override;
  Return<void> onHs20SubscriptionRemediation(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::OsuMethod osuMethod,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onHs20DeauthImminentNotice(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t reasonCode, uint32_t reAuthDelayInSec,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onDisconnected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      bool locallyGenerated,
      ISupplicantStaIfaceCallback::ReasonCode reasonCode) override;
  Return<void> onAssociationRejected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::StatusCode statusCode,
      bool timedOut) override;
  Return<void> onAuthenticationTimeout(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onEapFailure() override;
  Return<void> onBssidChanged(
      ISupplicantStaIfaceCallback::BssidChangeReason reason,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onWpsEventSuccess() override;
  Return<void> onWpsEventFail(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::WpsConfigError configError,
      ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) override;
  Return<void> onWpsEventPbcOverlap() override;
  Return<void> onExtRadioWorkStart(uint32_t id) override;
  Return<void> onExtRadioWorkTimeout(uint32_t id) override;
  Return<void> onEapFailure_1_1(
      ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) override;
  /**
   * Indicates DPP configuration received success event (Enrolee mode).
   */
  Return<void> onDppSuccessConfigReceived(
      const ::android::hardware::hidl_vec<uint8_t>& ssid,
      const ::android::hardware::hidl_string& password,
      const ::android::hardware::hidl_array<uint8_t, 32>& psk,
      ::android::hardware::wifi::supplicant::V1_2::DppAkm securityAkm) override;
  /**
   * Indicates DPP configuration sent success event (Configurator mode).
   */
  Return<void> onDppSuccessConfigSent() override;
  /**
   * Indicates a DPP progress event.
   */
  Return<void> onDppProgress(
      ::android::hardware::wifi::supplicant::V1_2::DppProgressCode code)
      override;
  /**
   * Indicates a DPP failure event.
   */
  Return<void> onDppFailure(
      ::android::hardware::wifi::supplicant::V1_2::DppFailureCode code)
      override;

 private:
  std::string mInterfaceName;
  android::sp<WifiEventCallback> mCallback;
  android::sp<SupplicantStaIfaceCallbackV1_1> mSupplicantCallbackV1_1;
};

#if ANDROID_VERSION == 30
class SupplicantStaIfaceCallbackV1_3 : public ISupplicantStaIfaceCallbackV1_3 {
 public:
  explicit SupplicantStaIfaceCallbackV1_3(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<SupplicantStaIfaceCallbackV1_2>& aSupplicantCallback);

  Return<void> onNetworkAdded(uint32_t id) override;
  Return<void> onNetworkRemoved(uint32_t id) override;
  Return<void> onStateChanged(
    ISupplicantStaIfaceCallback::State newState,
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
    const ::android::hardware::hidl_vec<uint8_t>& ssid) override;
  Return<void> onAnqpQueryDone(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) override;
  Return<void> onHs20IconQueryDone(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      const ::android::hardware::hidl_string& fileName,
      const ::android::hardware::hidl_vec<uint8_t>& data) override;
  Return<void> onHs20SubscriptionRemediation(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::OsuMethod osuMethod,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onHs20DeauthImminentNotice(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t reasonCode, uint32_t reAuthDelayInSec,
      const ::android::hardware::hidl_string& url) override;
  Return<void> onDisconnected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      bool locallyGenerated,
      ISupplicantStaIfaceCallback::ReasonCode reasonCode) override;
  Return<void> onAssociationRejected(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::StatusCode statusCode,
      bool timedOut) override;
  Return<void> onAuthenticationTimeout(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onEapFailure() override;
  Return<void> onBssidChanged(
      ISupplicantStaIfaceCallback::BssidChangeReason reason,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid) override;
  Return<void> onWpsEventSuccess() override;
  Return<void> onWpsEventFail(
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      ISupplicantStaIfaceCallback::WpsConfigError configError,
      ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) override;
  Return<void> onWpsEventPbcOverlap() override;
  Return<void> onExtRadioWorkStart(uint32_t id) override;
  Return<void> onExtRadioWorkTimeout(uint32_t id) override;
  Return<void> onEapFailure_1_1(
      ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) override;
  /**
    * Indicates DPP configuration received success event (Enrolee mode).
    */
  Return<void> onDppSuccessConfigReceived(
      const ::android::hardware::hidl_vec<uint8_t>& ssid,
      const ::android::hardware::hidl_string& password,
      const ::android::hardware::hidl_array<uint8_t, 32>& psk,
      ::android::hardware::wifi::supplicant::V1_2::DppAkm securityAkm) override;
  /**
    * Indicates DPP configuration sent success event (Configurator mode).
    */
  Return<void> onDppSuccessConfigSent() override;
  /**
    * Indicates a DPP progress event.
    */
  Return<void> onDppProgress(
      ::android::hardware::wifi::supplicant::V1_2::DppProgressCode code)
      override;
  /**
   * Indicates a DPP failure event.
   */
  Return<void> onDppFailure(
      ::android::hardware::wifi::supplicant::V1_2::DppFailureCode code)
      override;

  Return<void> onPmkCacheAdded(
      int64_t expirationTimeInSec,
      const ::android::hardware::hidl_vec<uint8_t>& serializedEntry)
      override;

  Return<void> onDppSuccess(
      ::android::hardware::wifi::supplicant::V1_3::DppSuccessCode code)
      override;
  Return<void> onDppProgress_1_3(
      ::android::hardware::wifi::supplicant::V1_3::DppProgressCode code)
      override;
  Return<void> onDppFailure_1_3(
      ::android::hardware::wifi::supplicant::V1_3::DppFailureCode code,
      const ::android::hardware::hidl_string& ssid,
      const ::android::hardware::hidl_string& channelList,
      const ::android::hardware::hidl_vec<uint16_t>& bandList)
      override;

  Return<void> onBssTmHandlingDone(
      const ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaIfaceCallback::BssTmData& tmData)
      override;

  Return<void> onEapFailure_1_3(uint32_t errorCode)
      override;

  Return<void> onStateChanged_1_3(
      ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaIfaceCallback::State newState,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t id,
      const ::android::hardware::hidl_vec<uint8_t>& ssid,
      bool filsHlpSent)
      override;
 private:
  std::string mInterfaceName;
  android::sp<WifiEventCallback> mCallback;
  android::sp<SupplicantStaIfaceCallbackV1_2> mSupplicantCallbackV1_2;
};
#endif // ANDROID_VERSION == 30

END_WIFI_NAMESPACE

#endif  // SupplicantCallback_H
