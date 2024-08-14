/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
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

#ifndef SupplicantCallback_H
#define SupplicantCallback_H

#include <android/hardware/wifi/supplicant/BnSupplicantStaIfaceCallback.h>

class SupplicantStaIfaceCallback
    : virtual public ::android::hardware::wifi::supplicant::
          BnSupplicantStaIfaceCallback {
 public:
  explicit SupplicantStaIfaceCallback(
      const std::string& aInterfaceName,
      const android::sp<WifiEventCallback>& aCallback,
      const android::sp<PasspointEventCallback>& aPasspointCallback,
      const android::sp<SupplicantStaManager> aSupplicantManager);

  // ISupplicantStaIfaceCallback
  ::android::binder::Status onAnqpQueryDone(
      const ::std::vector<uint8_t>& /*bssid*/,
      const ::android::hardware::wifi::supplicant::AnqpData& /*data*/,
      const ::android::hardware::wifi::supplicant::Hs20AnqpData& /*hs20Data*/)
      override;
  ::android::binder::Status onAssociationRejected(
      const ::android::hardware::wifi::supplicant::
          AssociationRejectionData& /*assocRejectData*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onAuthenticationTimeout(
      const ::std::vector<uint8_t>& /*bssid*/) override;
  ::android::binder::Status onAuxiliarySupplicantEvent(
      ::android::hardware::wifi::supplicant::
          AuxiliarySupplicantEventCode /*eventCode*/,
      const ::std::vector<uint8_t>& /*bssid*/,
      const ::android::String16& /*reasonString*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onBssTmHandlingDone(
      const ::android::hardware::wifi::supplicant::BssTmData& /*tmData*/)
      override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onBssidChanged(
      ::android::hardware::wifi::supplicant::BssidChangeReason /*reason*/,
      const ::std::vector<uint8_t>& /*bssid*/) override;
  ::android::binder::Status onDisconnected(
      const ::std::vector<uint8_t>& /*bssid*/, bool /*locallyGenerated*/,
      ::android::hardware::wifi::supplicant::StaIfaceReasonCode /*reasonCode*/)
      override;
  ::android::binder::Status onDppFailure(
      ::android::hardware::wifi::supplicant::DppFailureCode /*code*/,
      const ::android::String16& /*ssid*/,
      const ::android::String16& /*channelList*/,
      const ::std::vector<char16_t>& /*bandList*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onDppProgress(
      ::android::hardware::wifi::supplicant::DppProgressCode /*code*/)
      override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onDppSuccess(
      ::android::hardware::wifi::supplicant::DppEventType /*event*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onDppSuccessConfigReceived(
      const ::std::vector<uint8_t>& /*ssid*/,
      const ::android::String16& /*password*/,
      const ::std::vector<uint8_t>& /*psk*/,
      ::android::hardware::wifi::supplicant::DppAkm /*securityAkm*/,
      const ::android::hardware::wifi::supplicant::
          DppConnectionKeys& /*dppConnectionKeys*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onDppSuccessConfigSent() override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onEapFailure(
      const ::std::vector<uint8_t>& /*bssid*/, int32_t /*errorCode*/) override;
  ::android::binder::Status onExtRadioWorkStart(int32_t /*id*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onExtRadioWorkTimeout(int32_t /*id*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onHs20DeauthImminentNotice(
      const ::std::vector<uint8_t>& /*bssid*/, int32_t /*reasonCode*/,
      int32_t /*reAuthDelayInSec*/,
      const ::android::String16& /*url*/) override;
  ::android::binder::Status onHs20IconQueryDone(
      const ::std::vector<uint8_t>& /*bssid*/,
      const ::android::String16& /*fileName*/,
      const ::std::vector<uint8_t>& /*data*/) override;
  ::android::binder::Status onHs20SubscriptionRemediation(
      const ::std::vector<uint8_t>& /*bssid*/,
      ::android::hardware::wifi::supplicant::OsuMethod /*osuMethod*/,
      const ::android::String16& /*url*/) override;
  ::android::binder::Status
  onHs20TermsAndConditionsAcceptanceRequestedNotification(
      const ::std::vector<uint8_t>& /*bssid*/,
      const ::android::String16& /*url*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onNetworkAdded(int32_t /*id*/) override;
  ::android::binder::Status onNetworkNotFound(
      const ::std::vector<uint8_t>& /*ssid*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onNetworkRemoved(int32_t /*id*/) override;
  ::android::binder::Status onPmkCacheAdded(
      int64_t /*expirationTimeInSec*/,
      const ::std::vector<uint8_t>& /*serializedEntry*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onStateChanged(
      ::android::hardware::wifi::supplicant::StaIfaceCallbackState /*newState*/,
      const ::std::vector<uint8_t>& /*bssid*/, int32_t /*id*/,
      const ::std::vector<uint8_t>& /*ssid*/, bool /*filsHlpSent*/) override;

  ::android::binder::Status onWpsEventFail(
      const ::std::vector<uint8_t>& /*bssid*/,
      ::android::hardware::wifi::supplicant::WpsConfigError /*configError*/,
      ::android::hardware::wifi::supplicant::WpsErrorIndication /*errorInd*/)
      override;
  ::android::binder::Status onWpsEventPbcOverlap() override;
  ::android::binder::Status onWpsEventSuccess() override;
  ::android::binder::Status onQosPolicyReset() override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
  ::android::binder::Status onQosPolicyRequest(
      int32_t /*qosPolicyRequestId*/,
      const ::std::vector<::android::hardware::wifi::supplicant::
                              QosPolicyData>& /*qosPolicyData*/) override {
    return ::android::binder::Status::fromStatusT(::android::OK);
  }
#if ANDROID_VERSION >= 34
  ::android::binder::Status onMloLinksInfoChanged(
      ::android::hardware::wifi::supplicant::ISupplicantStaIfaceCallback::MloLinkInfoChangeReason reason) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }
  ::android::binder::Status onDppConfigReceived(
      const ::android::hardware::wifi::supplicant::DppConfigurationData& configData) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }
  ::android::binder::Status onDppConnectionStatusResultSent(::android::hardware::wifi::supplicant::DppStatusErrorCode code) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }

  ::android::binder::Status onBssFrequencyChanged(int32_t frequencyMhz) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }

  ::android::binder::Status onSupplicantStateChanged(
    const ::android::hardware::wifi::supplicant::SupplicantStateChangeData& stateChangeData) override;

  ::android::binder::Status onQosPolicyResponseForScs(
    const ::std::vector<::android::hardware::wifi::supplicant::QosPolicyScsResponseStatus>& qosPolicyScsResponseStatus) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }

  ::android::binder::Status onPmkSaCacheAdded(
    const ::android::hardware::wifi::supplicant::PmkSaCacheData& pmkSaData) override {
    return ::android::binder::Status::fromStatusT(
        ::android::OK);
  }
#endif

 private:
  void NotifyStateChanged(uint32_t aState, const std::string& aSsid,
                          const std::string& aBssid);
  void NotifyConnected(const std::string& aSsid, const std::string& aBssid);
  void NotifyDisconnected(const std::string& aBssid, bool aLocallyGenerated,
                          uint32_t aReason);
  void NotifyAuthenticationFailure(uint32_t aReason, int32_t aErrorCode);
  void NotifyAnqpQueryDone(
      const std::string& aBssid,
      const ::android::hardware::wifi::supplicant::AnqpData& aData,
      const ::android::hardware::wifi::supplicant::Hs20AnqpData& aHs20Data);
  void NotifyTargetBssid(const std::string& aBssid);
  void NotifyAssociatedBssid(const std::string& aBssid);
  void NotifyWpsSuccess();
  void NotifyWpsFailure(const std::string& aBssid, uint16_t aConfigError,
                        uint16_t aErrorIndication);
  void NotifyWpsTimeout();
  void NotifyWpsOverlap();

  ::android::hardware::wifi::supplicant::StaIfaceCallbackState
      mStateBeforeDisconnect;
  std::string mInterfaceName;
  android::sp<WifiEventCallback> mCallback;
  android::sp<SupplicantStaManager> mSupplicantManager;
  android::sp<PasspointEventCallback> mPasspointCallback;
};

#endif  // SupplicantCallback_H
