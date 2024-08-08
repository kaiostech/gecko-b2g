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

/**
 * SupplicantStaIfaceCallback implementation
 */
SupplicantStaIfaceCallback::SupplicantStaIfaceCallback(
    const std::string& aInterfaceName,
    const android::sp<WifiEventCallback>& aCallback,
    const android::sp<SupplicantStaManager> aSupplicantManager)
    : mStateBeforeDisconnect(::android::hardware::wifi::supplicant::
                                 StaIfaceCallbackState::INACTIVE),
      mInterfaceName(aInterfaceName),
      mCallback(aCallback),
      mSupplicantManager(aSupplicantManager) {}

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
