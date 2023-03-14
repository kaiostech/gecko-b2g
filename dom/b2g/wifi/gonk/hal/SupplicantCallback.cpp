/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "SupplicantCallback"

#include "WifiCommon.h"
#include "SupplicantCallback.h"

#define EVENT_SUPPLICANT_STATE_CHANGED u"SUPPLICANT_STATE_CHANGED"_ns
#define EVENT_SUPPLICANT_NETWORK_CONNECTED u"SUPPLICANT_NETWORK_CONNECTED"_ns
#define EVENT_SUPPLICANT_NETWORK_DISCONNECTED \
  u"SUPPLICANT_NETWORK_DISCONNECTED"_ns
#define EVENT_SUPPLICANT_AUTH_FAILURE u"SUPPLICANT_AUTH_FAILURE"_ns
#define EVENT_SUPPLICANT_ASSOC_REJECT u"SUPPLICANT_ASSOC_REJECT"_ns
#define EVENT_SUPPLICANT_TARGET_BSSID u"SUPPLICANT_TARGET_BSSID"_ns
#define EVENT_SUPPLICANT_ASSOCIATED_BSSID u"SUPPLICANT_ASSOCIATED_BSSID"_ns
#define EVENT_WPS_CONNECTION_SUCCESS u"WPS_CONNECTION_SUCCESS"_ns
#define EVENT_WPS_CONNECTION_FAIL u"WPS_CONNECTION_FAIL"_ns
#define EVENT_WPS_CONNECTION_TIMEOUT u"WPS_CONNECTION_TIMEOUT"_ns
#define EVENT_WPS_CONNECTION_PBC_OVERLAP u"WPS_CONNECTION_PBC_OVERLAP"_ns

using namespace mozilla::dom::wifi;

static mozilla::Mutex sLock("supplicant-callback");

/**
 * SupplicantStaIfaceCallback implementation
 */
SupplicantStaIfaceCallback::SupplicantStaIfaceCallback(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<PasspointEventCallback>& aPasspointCallback,
    const android::sp<SupplicantStaManager> aSupplicantManager)
    : mStateBeforeDisconnect(ISupplicantStaIfaceCallback::State::INACTIVE),
      mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mPasspointCallback(aPasspointCallback),
      mSupplicantManager(aSupplicantManager) {}

Return<void> SupplicantStaIfaceCallback::onNetworkAdded(uint32_t id) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onNetworkAdded()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onNetworkRemoved(uint32_t id) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onNetworkRemoved()");
  mStateBeforeDisconnect = ISupplicantStaIfaceCallback::State::INACTIVE;
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onStateChanged(
    ISupplicantStaIfaceCallback::State newState,
    const android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
    const android::hardware::hidl_vec<uint8_t>& ssid) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onStateChanged()");

  std::string bssidStr = ConvertMacToString(bssid);
  std::string ssidStr(ssid.begin(), ssid.end());

  if (newState != ISupplicantStaIfaceCallback::State::DISCONNECTED) {
    mStateBeforeDisconnect = newState;
  }
  if (newState == ISupplicantStaIfaceCallback::State::COMPLETED) {
    NotifyConnected(ssidStr, bssidStr);
  }

  NotifyStateChanged((uint32_t)newState, ssidStr, bssidStr);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onAnqpQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onAnqpQueryDone()");

  std::string bssidStr = ConvertMacToString(bssid);
  NotifyAnqpQueryDone(bssidStr, data, hs20Data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onHs20IconQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ::android::hardware::hidl_string& fileName,
    const ::android::hardware::hidl_vec<uint8_t>& data) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onHs20IconQueryDone()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyIconResponse(iface, bssidStr);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onHs20SubscriptionRemediation(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::OsuMethod osuMethod,
    const ::android::hardware::hidl_string& url) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaIfaceCallback.onHs20SubscriptionRemediation()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyWnmFrameReceived(iface, bssidStr);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onHs20DeauthImminentNotice(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    uint32_t reasonCode, uint32_t reAuthDelayInSec,
    const ::android::hardware::hidl_string& url) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG,
            "ISupplicantStaIfaceCallback.onHs20DeauthImminentNotice()");

  nsString bssidStr(NS_ConvertUTF8toUTF16(ConvertMacToString(bssid).c_str()));
  nsCString iface(mInterfaceName);
  if (mPasspointCallback) {
    mPasspointCallback->NotifyWnmFrameReceived(iface, bssidStr);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onDisconnected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    bool locallyGenerated, ISupplicantStaIfaceCallback::ReasonCode reasonCode) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onDisconnected()");

  if (mSupplicantManager) {
    NetworkConfiguration curConfig =
        mSupplicantManager->GetCurrentConfiguration();
    if (curConfig.IsValidNetwork()) {
      if (mStateBeforeDisconnect ==
              ISupplicantStaIfaceCallback::State::FOURWAY_HANDSHAKE &&
          curConfig.IsPskNetwork() &&
          (!locallyGenerated ||
           reasonCode !=
               ISupplicantStaIfaceCallback::ReasonCode::IE_IN_4WAY_DIFFERS)) {
        NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_WRONG_KEY,
                                    nsIWifiEvent::ERROR_CODE_NONE);
      } else if (mStateBeforeDisconnect ==
                     ISupplicantStaIfaceCallback::State::ASSOCIATED &&
                 curConfig.IsEapNetwork()) {
        NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_EAP_FAILURE,
                                    nsIWifiEvent::ERROR_CODE_NONE);
      }
    }
  }

  std::string bssidStr = ConvertMacToString(bssid);
  NotifyDisconnected(bssidStr, locallyGenerated, (uint32_t)reasonCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onAssociationRejected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::StatusCode statusCode, bool timedOut) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onAssociationRejected()");

  bool wrongKey = false;
  if (mSupplicantManager) {
    if (mSupplicantManager->IsCurrentSaeNetwork() &&
        statusCode ==
            ISupplicantStaIfaceCallback::StatusCode::UNSPECIFIED_FAILURE) {
      // In SAE networks, the password authentication is not related to the
      // 4-way handshake. In this case, we will send an authentication failure
      // event up if status code 1 (unspecified failure).
      WIFI_LOGD(LOG_TAG, "Incorrect password for SAE");
      wrongKey = true;
    } else if (mSupplicantManager->IsCurrentWepNetwork() &&
               statusCode ==
                   ISupplicantStaIfaceCallback::StatusCode::CHALLENGE_FAIL) {
      WIFI_LOGD(LOG_TAG, "Incorrect password for WEP");
      wrongKey = true;
    }
  }

  if (wrongKey) {
    NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_WRONG_KEY,
                                nsIWifiEvent::ERROR_CODE_NONE);
  } else {
    std::string bssidStr = ConvertMacToString(bssid);
    NotifyAssociationReject(bssidStr, (uint32_t)statusCode, timedOut);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onAuthenticationTimeout(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onAuthenticationTimeout()");
  NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_TIMEOUT,
                              nsIWifiEvent::ERROR_CODE_NONE);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onEapFailure() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onEapFailure()");
  NotifyAuthenticationFailure(nsIWifiEvent::AUTH_FAILURE_EAP_FAILURE,
                              nsIWifiEvent::ERROR_CODE_NONE);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onBssidChanged(
    ISupplicantStaIfaceCallback::BssidChangeReason reason,
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onBssidChanged()");

  std::string bssidStr = ConvertMacToString(bssid);
  if (reason == ISupplicantStaIfaceCallback::BssidChangeReason::ASSOC_START) {
    NotifyTargetBssid(bssidStr);
  } else if (reason ==
             ISupplicantStaIfaceCallback::BssidChangeReason::ASSOC_COMPLETE) {
    NotifyAssociatedBssid(bssidStr);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onWpsEventSuccess() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventSuccess()");
  NotifyWpsSuccess();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onWpsEventFail(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::WpsConfigError configError,
    ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventFail()");

  if (configError == ISupplicantStaIfaceCallback::WpsConfigError::MSG_TIMEOUT &&
      errorInd == ISupplicantStaIfaceCallback::WpsErrorIndication::NO_ERROR) {
    NotifyWpsTimeout();
  } else {
    std::string bssidStr = ConvertMacToString(bssid);
    NotifyWpsFailure(bssidStr, (uint16_t)configError, (uint16_t)errorInd);
  }
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onWpsEventPbcOverlap() {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onWpsEventPbcOverlap()");
  NotifyWpsOverlap();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onExtRadioWorkStart(uint32_t id) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onExtRadioWorkStart()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallback::onExtRadioWorkTimeout(uint32_t id) {
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallback.onExtRadioWorkTimeout()");
  return android::hardware::Void();
}

/**
 * Helper functions to notify event callback for
 * ISupplicantStaIfaceCallbackV1_0.
 */
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

void SupplicantStaIfaceCallback::NotifyConnected(const std::string& aSsid,
                                                 const std::string& aBssid) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event =
      new nsWifiEvent(EVENT_SUPPLICANT_NETWORK_CONNECTED);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());

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

void SupplicantStaIfaceCallback::NotifyAssociationReject(
    const std::string& aBssid, uint32_t aStatusCode, bool aTimeout) {
  nsCString iface(mInterfaceName);
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_SUPPLICANT_ASSOC_REJECT);
  event->mBssid = NS_ConvertUTF8toUTF16(aBssid.c_str());
  event->mStatusCode = aStatusCode;
  event->mTimeout = aTimeout;

  INVOKE_CALLBACK(mCallback, event, iface);
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

void SupplicantStaIfaceCallback::NotifyAnqpQueryDone(
    const std::string& aBssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) {
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
                         data.venueName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPRoamingConsortium,
                         data.roamingConsortium);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPIPAddrAvailability,
                         data.ipAddrTypeAvailability);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPNAIRealm,
                         data.naiRealm);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQP3GPPNetwork,
                         data.anqp3gppCellularNetwork);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::ANQPDomainName,
                         data.domainName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSFriendlyName,
                         hs20Data.operatorFriendlyName);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSWANMetrics,
                         hs20Data.wanMetrics);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSConnCapability,
                         hs20Data.connectionCapability);
    ASSIGN_ANQP_IF_EXIST(anqpData, AnqpElementType::HSOSUProviders,
                         hs20Data.osuProvidersList);

#undef ASSIGN_ANQP_IF_EXIST

    mPasspointCallback->NotifyAnqpResponse(iface, bssid, anqpData);
  } else {
    WIFI_LOGE(LOG_TAG, "mPasspointCallback is null");
  }
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

/**
 * SupplicantStaIfaceCallbackV1_1 implementation
 */
SupplicantStaIfaceCallbackV1_1::SupplicantStaIfaceCallbackV1_1(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<SupplicantStaIfaceCallback>& aSupplicantCallback)
    : mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mSupplicantCallback(aSupplicantCallback) {}

Return<void> SupplicantStaIfaceCallbackV1_1::onNetworkAdded(uint32_t id) {
  mSupplicantCallback->onNetworkAdded(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onNetworkRemoved(uint32_t id) {
  mSupplicantCallback->onNetworkRemoved(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onStateChanged(
    ISupplicantStaIfaceCallback::State newState,
    const android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
    const android::hardware::hidl_vec<uint8_t>& ssid) {
  mSupplicantCallback->onStateChanged(newState, bssid, id, ssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onAnqpQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) {
  mSupplicantCallback->onAnqpQueryDone(bssid, data, hs20Data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onHs20IconQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ::android::hardware::hidl_string& fileName,
    const ::android::hardware::hidl_vec<uint8_t>& data) {
  mSupplicantCallback->onHs20IconQueryDone(bssid, fileName, data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onHs20SubscriptionRemediation(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::OsuMethod osuMethod,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallback->onHs20SubscriptionRemediation(bssid, osuMethod, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onHs20DeauthImminentNotice(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    uint32_t reasonCode, uint32_t reAuthDelayInSec,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallback->onHs20DeauthImminentNotice(bssid, reasonCode,
                                                  reAuthDelayInSec, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onDisconnected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    bool locallyGenerated, ISupplicantStaIfaceCallback::ReasonCode reasonCode) {
  mSupplicantCallback->onDisconnected(bssid, locallyGenerated, reasonCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onAssociationRejected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::StatusCode statusCode, bool timedOut) {
  mSupplicantCallback->onAssociationRejected(bssid, statusCode, timedOut);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onAuthenticationTimeout(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallback->onAuthenticationTimeout(bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onEapFailure() {
  mSupplicantCallback->onEapFailure();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onBssidChanged(
    ISupplicantStaIfaceCallback::BssidChangeReason reason,
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallback->onBssidChanged(reason, bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onWpsEventSuccess() {
  mSupplicantCallback->onWpsEventSuccess();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onWpsEventFail(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::WpsConfigError configError,
    ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) {
  mSupplicantCallback->onWpsEventFail(bssid, configError, errorInd);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onWpsEventPbcOverlap() {
  mSupplicantCallback->onWpsEventPbcOverlap();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onExtRadioWorkStart(uint32_t id) {
  mSupplicantCallback->onExtRadioWorkStart(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onExtRadioWorkTimeout(
    uint32_t id) {
  mSupplicantCallback->onExtRadioWorkTimeout(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_1::onEapFailure_1_1(
    ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) {
  MutexAutoLock lock(sLock);
  WIFI_LOGD(LOG_TAG, "ISupplicantStaIfaceCallbackV1_1.onEapFailure_1_1()");
  return android::hardware::Void();
}

/**
 * SupplicantStaIfaceCallbackV1_2 implementation
 */
SupplicantStaIfaceCallbackV1_2::SupplicantStaIfaceCallbackV1_2(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<SupplicantStaIfaceCallbackV1_1>& aSupplicantCallback)
    : mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mSupplicantCallbackV1_1(aSupplicantCallback) {}

Return<void> SupplicantStaIfaceCallbackV1_2::onNetworkAdded(uint32_t id) {
  mSupplicantCallbackV1_1->onNetworkAdded(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onNetworkRemoved(uint32_t id) {
  mSupplicantCallbackV1_1->onNetworkRemoved(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onStateChanged(
    ISupplicantStaIfaceCallback::State newState,
    const android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
    const android::hardware::hidl_vec<uint8_t>& ssid) {
  mSupplicantCallbackV1_1->onStateChanged(newState, bssid, id, ssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onAnqpQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) {
  mSupplicantCallbackV1_1->onAnqpQueryDone(bssid, data, hs20Data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onHs20IconQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ::android::hardware::hidl_string& fileName,
    const ::android::hardware::hidl_vec<uint8_t>& data) {
  mSupplicantCallbackV1_1->onHs20IconQueryDone(bssid, fileName, data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onHs20SubscriptionRemediation(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::OsuMethod osuMethod,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallbackV1_1->onHs20SubscriptionRemediation(bssid, osuMethod, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onHs20DeauthImminentNotice(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    uint32_t reasonCode, uint32_t reAuthDelayInSec,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallbackV1_1->onHs20DeauthImminentNotice(bssid, reasonCode,
                                                      reAuthDelayInSec, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onDisconnected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    bool locallyGenerated, ISupplicantStaIfaceCallback::ReasonCode reasonCode) {
  mSupplicantCallbackV1_1->onDisconnected(bssid, locallyGenerated, reasonCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onAssociationRejected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::StatusCode statusCode, bool timedOut) {
  mSupplicantCallbackV1_1->onAssociationRejected(bssid, statusCode, timedOut);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onAuthenticationTimeout(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallbackV1_1->onAuthenticationTimeout(bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onEapFailure() {
  mSupplicantCallbackV1_1->onEapFailure();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onBssidChanged(
    ISupplicantStaIfaceCallback::BssidChangeReason reason,
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallbackV1_1->onBssidChanged(reason, bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onWpsEventSuccess() {
  mSupplicantCallbackV1_1->onWpsEventSuccess();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onWpsEventFail(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::WpsConfigError configError,
    ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) {
  mSupplicantCallbackV1_1->onWpsEventFail(bssid, configError, errorInd);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onWpsEventPbcOverlap() {
  mSupplicantCallbackV1_1->onWpsEventPbcOverlap();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onExtRadioWorkStart(uint32_t id) {
  mSupplicantCallbackV1_1->onExtRadioWorkStart(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onExtRadioWorkTimeout(
    uint32_t id) {
  mSupplicantCallbackV1_1->onExtRadioWorkTimeout(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onEapFailure_1_1(
    ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) {
  mSupplicantCallbackV1_1->onEapFailure_1_1(errorCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onDppSuccessConfigReceived(
    const ::android::hardware::hidl_vec<uint8_t>& ssid,
    const ::android::hardware::hidl_string& password,
    const ::android::hardware::hidl_array<uint8_t, 32>& psk,
    ::android::hardware::wifi::supplicant::V1_2::DppAkm securityAkm) {
  WIFI_LOGD(LOG_TAG,
            "SupplicantStaIfaceCallbackV1_2.onDppSuccessConfigReceived()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onDppSuccessConfigSent() {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_2.onDppSuccessConfigSent()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onDppProgress(
    ::android::hardware::wifi::supplicant::V1_2::DppProgressCode code) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_2.onDppProgress()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_2::onDppFailure(
    ::android::hardware::wifi::supplicant::V1_2::DppFailureCode code) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_2.onDppFailure()");
  return android::hardware::Void();
}

/**
 * SupplicantStaIfaceCallbackV1_3 implementation
 */
SupplicantStaIfaceCallbackV1_3::SupplicantStaIfaceCallbackV1_3(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<SupplicantStaIfaceCallbackV1_2>& aSupplicantCallback)
    : mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mSupplicantCallbackV1_2(aSupplicantCallback) {}

Return<void> SupplicantStaIfaceCallbackV1_3::onNetworkAdded(uint32_t id) {
  mSupplicantCallbackV1_2->onNetworkAdded(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onNetworkRemoved(uint32_t id) {
  mSupplicantCallbackV1_2->onNetworkRemoved(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onStateChanged(
    ISupplicantStaIfaceCallback::State newState,
    const android::hardware::hidl_array<uint8_t, 6>& bssid, uint32_t id,
    const android::hardware::hidl_vec<uint8_t>& ssid) {
  mSupplicantCallbackV1_2->onStateChanged(newState, bssid, id, ssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onAnqpQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ISupplicantStaIfaceCallback::AnqpData& data,
    const ISupplicantStaIfaceCallback::Hs20AnqpData& hs20Data) {
  mSupplicantCallbackV1_2->onAnqpQueryDone(bssid, data, hs20Data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onHs20IconQueryDone(
    const android::hardware::hidl_array<uint8_t, 6>& bssid,
    const ::android::hardware::hidl_string& fileName,
    const ::android::hardware::hidl_vec<uint8_t>& data) {
  mSupplicantCallbackV1_2->onHs20IconQueryDone(bssid, fileName, data);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onHs20SubscriptionRemediation(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::OsuMethod osuMethod,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallbackV1_2->onHs20SubscriptionRemediation(bssid, osuMethod, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onHs20DeauthImminentNotice(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    uint32_t reasonCode, uint32_t reAuthDelayInSec,
    const ::android::hardware::hidl_string& url) {
  mSupplicantCallbackV1_2->onHs20DeauthImminentNotice(bssid, reasonCode,
                                                      reAuthDelayInSec, url);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDisconnected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    bool locallyGenerated, ISupplicantStaIfaceCallback::ReasonCode reasonCode) {
  mSupplicantCallbackV1_2->onDisconnected(bssid, locallyGenerated, reasonCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onAssociationRejected(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::StatusCode statusCode, bool timedOut) {
  mSupplicantCallbackV1_2->onAssociationRejected(bssid, statusCode, timedOut);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onAuthenticationTimeout(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallbackV1_2->onAuthenticationTimeout(bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onEapFailure() {
  mSupplicantCallbackV1_2->onEapFailure();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onBssidChanged(
    ISupplicantStaIfaceCallback::BssidChangeReason reason,
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid) {
  mSupplicantCallbackV1_2->onBssidChanged(reason, bssid);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onWpsEventSuccess() {
  mSupplicantCallbackV1_2->onWpsEventSuccess();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onWpsEventFail(
    const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
    ISupplicantStaIfaceCallback::WpsConfigError configError,
    ISupplicantStaIfaceCallback::WpsErrorIndication errorInd) {
  mSupplicantCallbackV1_2->onWpsEventFail(bssid, configError, errorInd);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onWpsEventPbcOverlap() {
  mSupplicantCallbackV1_2->onWpsEventPbcOverlap();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onExtRadioWorkStart(uint32_t id) {
  mSupplicantCallbackV1_2->onExtRadioWorkStart(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onExtRadioWorkTimeout(
    uint32_t id) {
  mSupplicantCallbackV1_2->onExtRadioWorkTimeout(id);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onEapFailure_1_1(
    ISupplicantStaIfaceCallbackV1_1::EapErrorCode errorCode) {
  mSupplicantCallbackV1_2->onEapFailure_1_1(errorCode);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppSuccessConfigReceived(
    const ::android::hardware::hidl_vec<uint8_t>& ssid,
    const ::android::hardware::hidl_string& password,
    const ::android::hardware::hidl_array<uint8_t, 32>& psk,
    ::android::hardware::wifi::supplicant::V1_2::DppAkm securityAkm) {
  mSupplicantCallbackV1_2->onDppSuccessConfigReceived(ssid, password, psk, securityAkm);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppSuccessConfigSent() {
  mSupplicantCallbackV1_2->onDppSuccessConfigSent();
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppProgress(
    ::android::hardware::wifi::supplicant::V1_2::DppProgressCode code) {
  mSupplicantCallbackV1_2->onDppProgress(code);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppFailure(
    ::android::hardware::wifi::supplicant::V1_2::DppFailureCode code) {
  mSupplicantCallbackV1_2->onDppFailure(code);
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onPmkCacheAdded(
      int64_t expirationTimeInSec,
      const ::android::hardware::hidl_vec<uint8_t>& serializedEntry) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onPmkCacheAdded()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppSuccess(
      ::android::hardware::wifi::supplicant::V1_3::DppSuccessCode code) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onDppSuccess()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppProgress_1_3(
      ::android::hardware::wifi::supplicant::V1_3::DppProgressCode code) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onDppProgress_1_3()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onDppFailure_1_3(
      ::android::hardware::wifi::supplicant::V1_3::DppFailureCode code,
      const ::android::hardware::hidl_string& ssid,
      const ::android::hardware::hidl_string& channelList,
      const ::android::hardware::hidl_vec<uint16_t>& bandList) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onDppFailure_1_3()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onBssTmHandlingDone(
      const ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaIfaceCallback::BssTmData& tmData) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onBssTmHandlingDone()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onEapFailure_1_3(uint32_t errorCode) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onEapFailure_1_3()");
  return android::hardware::Void();
}

Return<void> SupplicantStaIfaceCallbackV1_3::onStateChanged_1_3(
      ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaIfaceCallback::State newState,
      const ::android::hardware::hidl_array<uint8_t, 6>& bssid,
      uint32_t id,
      const ::android::hardware::hidl_vec<uint8_t>& ssid,
      bool filsHlpSent) {
  WIFI_LOGD(LOG_TAG, "SupplicantStaIfaceCallbackV1_3.onStateChanged_1_3()");
  mSupplicantCallbackV1_2->onStateChanged(newState, bssid, id, ssid);
  return android::hardware::Void();
}
