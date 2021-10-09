/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "PasspointHandler"

#include "PasspointHandler.h"

using namespace mozilla;
using namespace mozilla::dom::wifi;

/* passpoint event name */
#define EVENT_ANQP_QUERY_DONE u"ANQP_QUERY_DONE"_ns
#define EVENT_HS20_ICON_QUERY_DONE u"HS20_ICON_QUERY_DONE"_ns
#define EVENT_WNM_FRAME_RECEIVED u"WNM_FRAME_RECEIVED"_ns

static const uint32_t minAnqpEscapeTimeMs = 1000;
static const uint32_t maxAnqpTimeIncrement = 6;

StaticRefPtr<PasspointHandler> PasspointHandler::sPasspointHandler;
StaticMutex PasspointHandler::sMutex;

already_AddRefed<PasspointHandler> PasspointHandler::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(sMutex);

  if (!sPasspointHandler) {
    sPasspointHandler = new PasspointHandler();
    ClearOnShutdown(&sPasspointHandler);
  }

  RefPtr<PasspointHandler> handler = sPasspointHandler;
  return handler.forget();
}

PasspointHandler::PasspointHandler() {}

void PasspointHandler::Cleanup() {
  mAnqpRequestTime.Clear();
  mAnqpPendingRequest.Clear();
  UnregisterEventCallback();
}

void PasspointHandler::SetSupplicantManager(
    const android::sp<SupplicantStaManager>& aManager) {
  mSupplicantManager = aManager;

  if (mSupplicantManager) {
    mSupplicantManager->RegisterPasspointCallback(this);
  }
}

void PasspointHandler::RegisterEventCallback(
    const android::sp<WifiEventCallback>& aCallback) {
  mCallback = aCallback;
}

void PasspointHandler::UnregisterEventCallback() { mCallback = nullptr; }

Result_t PasspointHandler::RequestAnqp(const nsAString& aAnqpKey,
                                       const nsAString& aBssid,
                                       bool aRoamingConsortiumOIs,
                                       bool aSupportRelease2) {
  if (!ReadyToRequest(aBssid)) {
    // Just ignore this request.
    return nsIWifiResult::SUCCESS;
  }

  if (StartAnqpQuery(aBssid, aRoamingConsortiumOIs, aSupportRelease2) !=
      nsIWifiResult::SUCCESS) {
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  UpdateTimeStamp(aBssid);

  nsAutoString anqpKey(aAnqpKey);
  mAnqpPendingRequest.InsertOrUpdate(aBssid, anqpKey);
  return nsIWifiResult::SUCCESS;
}

Result_t PasspointHandler::StartAnqpQuery(const nsAString& aBssid,
                                          bool aRoamingConsortiumOIs,
                                          bool aSupportRelease2) {
  std::vector<uint32_t> infoElements;
  std::vector<uint32_t> hs20SubTypes;

  for (const auto& element : R1_ANQP_SET) {
    infoElements.push_back(element);
  }

  if (aRoamingConsortiumOIs) {
    infoElements.push_back((uint32_t)AnqpElementType::ANQPRoamingConsortium);
  }

  if (aSupportRelease2) {
    for (const auto& element : R2_ANQP_SET) {
      hs20SubTypes.push_back(element);
    }
  }

  std::array<uint8_t, 6> bssid;
  ConvertMacToByteArray(NS_ConvertUTF16toUTF8(aBssid).get(), bssid);
  return mSupplicantManager->SendAnqpRequest(bssid, infoElements, hs20SubTypes);
}

bool PasspointHandler::ReadyToRequest(const nsAString& aBssid) {
  // check mAnqpTimeStamp to see if this bssid has already sent ANQP
  // requested in certain time interval, which get twice increment of
  // minAnqpEscapeTimeMs.
  AnqpRequestTime* requestTime;
  mozilla::TimeStamp currentTime = TimeStamp::Now();
  if (!mAnqpRequestTime.Get(aBssid, &requestTime)) {
    mAnqpRequestTime.InsertOrUpdate(aBssid,
                                    MakeUnique<AnqpRequestTime>(currentTime));
    return true;
  }

  TimeDuration delta = currentTime - requestTime->mTimeStamp;
  if (delta.ToMilliseconds() <=
      minAnqpEscapeTimeMs * (1 << requestTime->mTimeIncrement)) {
    // Delta time is not long enough, return false to skip this request.
    return false;
  }
  return true;
}

bool PasspointHandler::UpdateTimeStamp(const nsAString& aBssid) {
  AnqpRequestTime* requestTime = mAnqpRequestTime.Get(aBssid);
  if (requestTime) {
    if (requestTime->mTimeIncrement < maxAnqpTimeIncrement) {
      requestTime->mTimeIncrement++;
    }
    // Update timestamp for current bssid.
    requestTime->mTimeStamp = TimeStamp::Now();
  }
  return true;
}

/**
 * Implement PasspointEventCallback
 */
void PasspointHandler::NotifyAnqpResponse(const nsACString& aIface,
                                          const nsAString& aBssid,
                                          AnqpResponseMap& aAnqpData) {
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_ANQP_QUERY_DONE);

  nsAutoString anqpNetworkKey;
  if (aBssid.IsEmpty() || !mAnqpPendingRequest.Get(aBssid, &anqpNetworkKey)) {
    // Not a valid request in the list, ignore this response.
    WIFI_LOGD(LOG_TAG, "Ignore the ANQP response");
    return;
  }

  RefPtr<nsAnqpResponse> anqpResponse = new nsAnqpResponse(aBssid);
  for (auto iter = aAnqpData.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t& key = iter.Key();

    if (ANQP_PARSER_TABLE.at(key)) {
      (ANQP_PARSER_TABLE.at(key))(iter.Data(), anqpResponse);
    }
  }

  event->mAnqpNetworkKey = anqpNetworkKey;
  event->updateAnqpResponse(anqpResponse);
  INVOKE_CALLBACK(mCallback, event, aIface);
}

void PasspointHandler::NotifyIconResponse(const nsACString& aIface,
                                          const nsAString& aBssid) {
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_HS20_ICON_QUERY_DONE);

  // TODO: parse icon data

  INVOKE_CALLBACK(mCallback, event, aIface);
}

void PasspointHandler::NotifyWnmFrameReceived(const nsACString& aIface,
                                              const nsAString& aBssid) {
  RefPtr<nsWifiEvent> event = new nsWifiEvent(EVENT_WNM_FRAME_RECEIVED);

  // TODO: parse wireless network management frame

  INVOKE_CALLBACK(mCallback, event, aIface);
}

NS_IMPL_ISUPPORTS0(PasspointHandler)
