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

#define LOG_TAG "SupplicantCallback"

#include "WifiCommon.h"
#include "SupplicantCallback.h"

#define EVENT_SUPPLICANT_AUTH_FAILURE u"SUPPLICANT_AUTH_FAILURE"_ns
#define EVENT_SUPPLICANT_STATE_CHANGED u"SUPPLICANT_STATE_CHANGED"_ns
#define EVENT_SUPPLICANT_NETWORK_CONNECTED u"SUPPLICANT_NETWORK_CONNECTED"_ns
#define EVENT_SUPPLICANT_NETWORK_DISCONNECTED \
  u"SUPPLICANT_NETWORK_DISCONNECTED"_ns
#define EVENT_SUPPLICANT_TARGET_BSSID u"SUPPLICANT_TARGET_BSSID"_ns
#define EVENT_SUPPLICANT_ASSOCIATED_BSSID u"SUPPLICANT_ASSOCIATED_BSSID"_ns
#define EVENT_WPS_CONNECTION_SUCCESS u"WPS_CONNECTION_SUCCESS"_ns
#define EVENT_WPS_CONNECTION_FAIL u"WPS_CONNECTION_FAIL"_ns
#define EVENT_WPS_CONNECTION_TIMEOUT u"WPS_CONNECTION_TIMEOUT"_ns
#define EVENT_WPS_CONNECTION_PBC_OVERLAP u"WPS_CONNECTION_PBC_OVERLAP"_ns

static mozilla::Mutex sLock("supplicant-callback");

/**
 * SupplicantStaIfaceCallback implementation
 */
SupplicantStaIfaceCallback::SupplicantStaIfaceCallback(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<PasspointEventCallback>& aPasspointCallback,
    const android::sp<SupplicantStaManager> aSupplicantManager)
    : mStateBeforeDisconnect(::android::hardware::wifi::supplicant::
                                 StaIfaceCallbackState::INACTIVE),
      mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mPasspointCallback(aPasspointCallback),
      mSupplicantManager(aSupplicantManager) {}

::android::binder::Status SupplicantStaIfaceCallback::onAnqpQueryDone(
    const ::std::vector<uint8_t>& bssid,
    const ::android::hardware::wifi::supplicant::AnqpData& data,
    const ::android::hardware::wifi::supplicant::Hs20AnqpData& hs20Data) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onAnqpQueryDone()");

  std::string bssidStr = ConvertMacToString(bssid);
  NotifyAnqpQueryDone(bssidStr, data, hs20Data);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onNetworkAdded(
    int32_t id) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onNetworkAdded()");
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onNetworkRemoved(
    int32_t id) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onNetworkRemoved()");
  mStateBeforeDisconnect =
      ::android::hardware::wifi::supplicant::StaIfaceCallbackState::INACTIVE;
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onHs20IconQueryDone(
    const ::std::vector<uint8_t>& bssid, const ::android::String16& fileName,
    const ::std::vector<uint8_t>& data) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onHs20IconQueryDone()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyIconResponse(iface, bssidStr);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status
SupplicantStaIfaceCallback::onHs20SubscriptionRemediation(
    const ::std::vector<uint8_t>& bssid,
    ::android::hardware::wifi::supplicant::OsuMethod osuMethod,
    const ::android::String16& url) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaIfaceCallback.onHs20SubscriptionRemediation()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyWnmFrameReceived(iface, bssidStr);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status
SupplicantStaIfaceCallback::onHs20DeauthImminentNotice(
    const ::std::vector<uint8_t>& bssid, int32_t reasonCode,
    int32_t reAuthDelayInSec, const ::android::String16& url) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaIfaceCallback.onHs20DeauthImminentNotice()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyWnmFrameReceived(iface, bssidStr);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onStateChanged(
    ::android::hardware::wifi::supplicant::StaIfaceCallbackState newState,
    const ::std::vector<uint8_t>& bssid, int32_t id,
    const ::std::vector<uint8_t>& ssid, bool filsHlpSent) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onStateChanged()");

  std::string bssidStr = ConvertMacToString(bssid);
  std::string ssidStr(ssid.begin(), ssid.end());

  if (newState != ::android::hardware::wifi::supplicant::StaIfaceCallbackState::
                      DISCONNECTED) {
    mStateBeforeDisconnect = newState;
  }
  if (newState ==
      ::android::hardware::wifi::supplicant::StaIfaceCallbackState::COMPLETED) {
    NotifyConnected(ssidStr, bssidStr);
  }

  NotifyStateChanged((uint32_t)newState, ssidStr, bssidStr);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onDisconnected(
    const ::std::vector<uint8_t>& bssid, bool locallyGenerated,
    ::android::hardware::wifi::supplicant::StaIfaceReasonCode reasonCode) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onDisconnected()");
  if (mSupplicantManager) {
    NetworkConfiguration curConfig =
        mSupplicantManager->GetCurrentConfiguration();
    if (curConfig.IsValidNetwork()) {
      if (mStateBeforeDisconnect ==
              ::android::hardware::wifi::supplicant::StaIfaceCallbackState::
                  FOURWAY_HANDSHAKE &&
          curConfig.IsPskNetwork() &&
          (!locallyGenerated ||
           reasonCode != ::android::hardware::wifi::supplicant::
                             StaIfaceReasonCode::IE_IN_4WAY_DIFFERS)) {
        NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_WRONG_KEY,
                                    nsIWifiEvent::ERROR_CODE_NONE);
      } else if (mStateBeforeDisconnect ==
                     ::android::hardware::wifi::supplicant::
                         StaIfaceCallbackState::ASSOCIATED &&
                 curConfig.IsEapNetwork()) {
        NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_EAP_FAILURE,
                                    nsIWifiEvent::ERROR_CODE_NONE);
      }
    }
  }

  std::string bssidStr = ConvertMacToString(bssid);
  NotifyDisconnected(bssidStr, locallyGenerated, (uint32_t)reasonCode);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onAuthenticationTimeout(
    const ::std::vector<uint8_t>& bssid) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onAuthenticationTimeout()");
  NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_TIMEOUT,
                              nsIWifiEvent::ERROR_CODE_NONE);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onEapFailure(
    const ::std::vector<uint8_t>& bssid, int32_t errorCode) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onEapFailure()");
  NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_EAP_FAILURE,
                              nsIWifiEvent::ERROR_CODE_NONE);
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onBssidChanged(
    ::android::hardware::wifi::supplicant::BssidChangeReason reason,
    const ::std::vector<uint8_t>& bssid) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onBssidChanged()");

  std::string bssidStr = ConvertMacToString(bssid);
  if (reason ==
      ::android::hardware::wifi::supplicant::BssidChangeReason::ASSOC_START) {
    NotifyTargetBssid(bssidStr);
  } else if (reason == ::android::hardware::wifi::supplicant::
                           BssidChangeReason::ASSOC_COMPLETE) {
    NotifyAssociatedBssid(bssidStr);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onWpsEventSuccess() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventSuccess()");
  NotifyWpsSuccess();
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onWpsEventFail(
    const ::std::vector<uint8_t>& bssid,
    ::android::hardware::wifi::supplicant::WpsConfigError configError,
    ::android::hardware::wifi::supplicant::WpsErrorIndication errorInd) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventFail()");

  if (configError ==
          ::android::hardware::wifi::supplicant::WpsConfigError::MSG_TIMEOUT &&
      errorInd ==
          ::android::hardware::wifi::supplicant::WpsErrorIndication::NO_ERROR) {
    NotifyWpsTimeout();
  } else {
    std::string bssidStr = ConvertMacToString(bssid);
    NotifyWpsFailure(bssidStr, (uint16_t)configError, (uint16_t)errorInd);
  }
  return ::android::binder::Status::fromStatusT(::android::OK);
}

::android::binder::Status SupplicantStaIfaceCallback::onWpsEventPbcOverlap() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventPbcOverlap()");
  NotifyWpsOverlap();
  return ::android::binder::Status::fromStatusT(::android::OK);
}

#if ANDROID_VERSION >= 34
::android::binder::Status SupplicantStaIfaceCallback::onSupplicantStateChanged(
    const ::android::hardware::wifi::supplicant::SupplicantStateChangeData&
        stateChangeData) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onSupplicantStateChanged()");
  std::string bssidStr = ConvertMacToString(stateChangeData.bssid);
  std::string ssidStr(stateChangeData.ssid.begin(), stateChangeData.ssid.end());

  if (stateChangeData.newState != ::android::hardware::wifi::supplicant::
                                      StaIfaceCallbackState::DISCONNECTED) {
    mStateBeforeDisconnect = stateChangeData.newState;
  }
  if (stateChangeData.newState ==
      ::android::hardware::wifi::supplicant::StaIfaceCallbackState::COMPLETED) {
    NotifyConnected(ssidStr, bssidStr);
  }

  NotifyStateChanged((uint32_t)stateChangeData.newState, ssidStr, bssidStr);
  return ::android::binder::Status::fromStatusT(::android::OK);
}
#endif

void SupplicantStaIfaceCallback::NotifyConnected(const std::string& aSsid,
                                                 const std::string& aBssid) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event =
      new nsWifiEvent(EVENT_SUPPLICANT_NETWORK_CONNECTED);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyStateChanged(uint32_t aState,
                                                    const std::string& aSsid,
                                                    const std::string& aBssid) {
  int32_t networkId = INVALID_NETWORK_ID;
  if (mSupplicantManager) {
    networkId = mSupplicantManager->GetCurrentNetworkId();
  }

  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SUPPLICANT_STATE_CHANGED);
  RefPtr<nsStateChanged> stateChanged = new nsStateChanged(
      aState, networkId, NS_ConvertUTF8toUTF16(aBssid.c_str()),
      NS_ConvertUTF8toUTF16(aSsid.c_str()));
  event->updateStateChanged(stateChanged);

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyDisconnected(const std::string& aBssid,
                                                    bool aLocallyGenerated,
                                                    uint32_t aReason) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event =
      new nsWifiEvent(EVENT_SUPPLICANT_NETWORK_DISCONNECTED);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());
  event->mLocallyGenerated = aLocallyGenerated;
  event->mReason = aReason;

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyAuthenticationFailure(
    uint32_t aReason, int32_t aErrorCode) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SUPPLICANT_AUTH_FAILURE);
  event->mReason = aReason;
  event->mErrorCode = aErrorCode;

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyAnqpQueryDone(
    const std::string& aBssid,
    const ::android::hardware::wifi::supplicant::AnqpData& aData,
    const ::android::hardware::wifi::supplicant::Hs20AnqpData& aHs20Data) {
  if (mPasspointCallback) {
    nsCString iface(mInterfaceName);
    nsString bssid(NS_ConvertUTF8toUTF16(aBssid.c_str()));

#define ASSIGN_ANQP_IF_EXIST(map, type, payload)   \
  do {                                             \
    if (payload.size() > 0) {                      \
      map.InsertOrUpdate((uint32_t)type, payload); \
    }                                              \
  } while (0)

    AnqpResponseMap anqpData;
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPVenueName,
                         aData.venueName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPRoamingConsortium,
                         aData.roamingConsortium);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPIPAddrAvailability,
                         aData.ipAddrTypeAvailability);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPNAIRealm,
                         aData.naiRealm);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQP3GPPNetwork,
                         aData.anqp3gppCellularNetwork);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPDomainName,
                         aData.domainName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSFriendlyName,
                         aHs20Data.operatorFriendlyName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSWANMetrics,
                         aHs20Data.wanMetrics);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSConnCapability,
                         aHs20Data.connectionCapability);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSOSUProviders,
                         aHs20Data.osuProvidersList);

#undef ASSIGN_ANQP_IF_EXIST

    mPasspointCallback->NotifyAnqpResponse(iface, bssid, anqpData);
  } else {
    WIFI_LOGE(LOG_TAG, "mPasspointCallback is null");
  }
}

void SupplicantStaIfaceCallback::NotifyTargetBssid(const std::string& aBssid) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SUPPLICANT_TARGET_BSSID);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyAssociatedBssid(
    const std::string& aBssid) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event =
      new nsWifiEvent(EVENT_SUPPLICANT_ASSOCIATED_BSSID);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyWpsSuccess() {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_WPS_CONNECTION_SUCCESS);

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyWpsFailure(const std::string& aBssid,
                                                  uint16_t aConfigError,
                                                  uint16_t aErrorIndication) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_WPS_CONNECTION_FAIL);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());
  event->mWpsConfigError = aConfigError;
  event->mWpsErrorIndication = aErrorIndication;

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyWpsTimeout() {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_WPS_CONNECTION_TIMEOUT);

  INVOKE_CALLBACK(mCallback, event, iface);
}

void SupplicantStaIfaceCallback::NotifyWpsOverlap() {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_WPS_CONNECTION_PBC_OVERLAP);

  INVOKE_CALLBACK(mCallback, event, iface);
}
