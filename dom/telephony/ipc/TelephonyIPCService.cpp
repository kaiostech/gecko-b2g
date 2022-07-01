/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/telephony/TelephonyChild.h"
#include "mozilla/Preferences.h"

#include "nsITelephonyCallInfo.h"

USING_TELEPHONY_NAMESPACE
using namespace mozilla::dom;
USING_VIDEOCALLPROVIDER_NAMESPACE

#ifdef MOZ_WIDGET_GONK
#  include <android/log.h>
#  undef LOG
#  define LOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, "TelephonyIPCService", ##args)
#else
#  undef LOG
#  define LOG(args...)
#endif

namespace {

const char* kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
#define kPrefDefaultServiceId "dom.telephony.defaultServiceId"
const char* kObservedPrefs[] = {kPrefDefaultServiceId, nullptr};

const uint32_t CLIENT_ID_FAKE = 1000;
const uint32_t CALL_INDEX_FAKE = 1000;

uint32_t getDefaultServiceId() {
  int32_t id = mozilla::Preferences::GetInt(kPrefDefaultServiceId, 0);
  int32_t numRil = mozilla::Preferences::GetInt(kPrefRilNumRadioInterfaces, 1);

  if (id >= numRil || id < 0) {
    id = 0;
  }

  return id;
}

}  // namespace

NS_IMPL_ISUPPORTS(TelephonyIPCService, nsITelephonyService,
                  nsITelephonyListener, nsIObserver)

TelephonyIPCService::TelephonyIPCService()
#ifdef MOZ_WIDGET_GONK
//  : mLoopbackProvider(nullptr)
#endif
{
  LOG("constructor");
  // Deallocated in ContentChild::DeallocPTelephonyChild().
  mPTelephonyChild = new TelephonyChild(this);
  ContentChild::GetSingleton()->SendPTelephonyConstructor(mPTelephonyChild);

  Preferences::AddStrongObservers(this, kObservedPrefs);
  mDefaultServiceId = getDefaultServiceId();
}

TelephonyIPCService::~TelephonyIPCService() {
  LOG("deconstructor");
#ifdef MOZ_WIDGET_GONK
//  CleanupVideocallProviders();
#endif

  if (mPTelephonyChild) {
    mPTelephonyChild->Send__delete__(mPTelephonyChild);
    mPTelephonyChild = nullptr;
  }
}
/*
#ifdef MOZ_WIDGET_GONK
void
TelephonyIPCService::CleanupVideocallProviders() {
  CleanupLoopbackProvider();
}

void
TelephonyIPCService::CleanupLoopbackProvider() {
  if (mLoopbackProvider) {
    mLoopbackProvider->Shutdown();
    mLoopbackProvider = nullptr;
  }
}
#endif
*/
void TelephonyIPCService::NoteActorDestroyed() {
  LOG("NoteActorDestroyed");
  MOZ_ASSERT(mPTelephonyChild);

  mPTelephonyChild = nullptr;

#ifdef MOZ_WIDGET_GONK
//  MOZ_ASSERT(mLoopbackProvider);
//  mLoopbackProvider = nullptr;
#endif
}

/*
 * Implementation of nsIObserver.
 */

NS_IMETHODIMP
TelephonyIPCService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral(kPrefDefaultServiceId)) {
      mDefaultServiceId = getDefaultServiceId();
    }
    return NS_OK;
  }

  MOZ_ASSERT(false, "TelephonyIPCService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/*
 * Implementation of nsITelephonyService.
 */

NS_IMETHODIMP
TelephonyIPCService::GetDefaultServiceId(uint32_t* aServiceId) {
  *aServiceId = mDefaultServiceId;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::RegisterListener(nsITelephonyListener* aListener) {
  MOZ_ASSERT(!mListeners.Contains(aListener));

  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // nsTArray doesn't fail.
  mListeners.AppendElement(aListener);

  if (mListeners.Length() == 1) {
    mPTelephonyChild->SendRegisterListener();
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::UnregisterListener(nsITelephonyListener* aListener) {
  MOZ_ASSERT(mListeners.Contains(aListener));

  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // We always have the element here, so it can't fail.
  mListeners.RemoveElement(aListener);

  if (!mListeners.Length()) {
    mPTelephonyChild->SendUnregisterListener();
  }
  return NS_OK;
}

nsresult TelephonyIPCService::SendRequest(nsITelephonyListener* aListener,
                                          nsITelephonyCallback* aCallback,
                                          const IPCTelephonyRequest& aRequest) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // Life time of newly allocated TelephonyRequestChild instance is managed by
  // IPDL itself.
  TelephonyRequestChild* actor =
      new TelephonyRequestChild(aListener, aCallback);
  mPTelephonyChild->SendPTelephonyRequestConstructor(actor, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCalls(nsITelephonyListener* aListener) {
  return SendRequest(aListener, nullptr, EnumerateCallsRequest());
}

NS_IMETHODIMP
TelephonyIPCService::Dial(uint32_t aClientId, const nsAString& aNumber,
                          bool aIsEmergency, uint16_t aType, uint16_t aRttMode,
                          nsITelephonyDialCallback* aCallback) {
  return SendRequest(
      nullptr, aCallback,
      DialRequest(aClientId, aType, nsString(aNumber), aIsEmergency, aRttMode));
}

NS_IMETHODIMP
TelephonyIPCService::AnswerCall(uint32_t aClientId, uint32_t aCallIndex,
                                uint16_t aType, uint16_t aRttMode,
                                nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     AnswerCallRequest(aClientId, aCallIndex, aType, aRttMode));
}

NS_IMETHODIMP
TelephonyIPCService::HangUpAllCalls(uint32_t aClientId,
                                    nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, HangUpAllCallsRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::HangUpCall(uint32_t aClientId, uint32_t aCallIndex,
                                uint16_t aReason,
                                nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     HangUpCallRequest(aClientId, aCallIndex, aReason));
}

NS_IMETHODIMP
TelephonyIPCService::RejectCall(uint32_t aClientId, uint32_t aCallIndex,
                                uint16_t aReason,
                                nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     RejectCallRequest(aClientId, aCallIndex, aReason));
}

NS_IMETHODIMP
TelephonyIPCService::HoldCall(uint32_t aClientId, uint32_t aCallIndex,
                              nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     HoldCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::ResumeCall(uint32_t aClientId, uint32_t aCallIndex,
                                nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     ResumeCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::ConferenceCall(uint32_t aClientId,
                                    nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback, ConferenceCallRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::SeparateCall(uint32_t aClientId, uint32_t aCallIndex,
                                  nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback,
                     SeparateCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::HangUpConference(uint32_t aClientId,
                                      nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback, HangUpConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::HoldConference(uint32_t aClientId,
                                    nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback, HoldConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::ResumeConference(uint32_t aClientId,
                                      nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback, ResumeConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::SendTones(uint32_t aClientId, const nsAString& aDtmfChars,
                               uint32_t aPauseDuration, uint32_t aToneDuration,
                               nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback,
                     SendTonesRequest(aClientId, nsString(aDtmfChars),
                                      aPauseDuration, aToneDuration));
}

NS_IMETHODIMP
TelephonyIPCService::StartTone(uint32_t aClientId, const nsAString& aDtmfChar) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendStartTone(aClientId, nsString(aDtmfChar));
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::StopTone(uint32_t aClientId) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendStopTone(aClientId);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SendUSSD(uint32_t aClientId, const nsAString& aUssd,
                              nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback,
                     SendUSSDRequest(aClientId, nsString(aUssd)));
}

NS_IMETHODIMP
TelephonyIPCService::CancelUSSD(uint32_t aClientId,
                                nsITelephonyCallback* aCallback) {
  return SendRequest(nullptr, aCallback, CancelUSSDRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::SendRttModify(uint32_t aClientId, uint32_t aCallIndex,
                                   uint16_t aRttMode,
                                   nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback,
                     SendRttModifyRequest(aClientId, aCallIndex, aRttMode));
}

NS_IMETHODIMP
TelephonyIPCService::SendRttModifyResponse(uint32_t aClientId,
                                           uint32_t aCallIndex, bool aStatus,
                                           nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(
      nullptr, aCallback,
      SendRttModifyResponseRequest(aClientId, aCallIndex, aStatus));
}

NS_IMETHODIMP
TelephonyIPCService::SendRttMessage(uint32_t aClientId, uint32_t aCallIndex,
                                    const nsAString& aMessage,
                                    nsITelephonyCallback* aCallback) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(
      nullptr, aCallback,
      SendRttMessageRequest(aClientId, aCallIndex, nsString(aMessage)));
}
/*
#ifdef MOZ_WIDGET_GONK
NS_IMETHODIMP
TelephonyIPCService::GetVideoCallProvider(uint32_t aClientId, uint32_t
aCallIndex, nsIVideoCallProvider **aProvider)
{
  LOG("GetVideoCallProvider, clientId: %d, callIndex: %d", aClientId,
aCallIndex); if ( CLIENT_ID_FAKE == aClientId && CALL_INDEX_FAKE == aCallIndex)
{ return GetLoopbackProvider(aProvider); } else { RefPtr<VideoCallProviderChild>
child = new VideoCallProviderChild(aClientId, aCallIndex);

    mPTelephonyChild->SendPVideoCallProviderConstructor(child, CLIENT_ID_FAKE,
CALL_INDEX_FAKE); mLoopbackProvider = child;

    child.forget(aProvider);
  }

  return NS_OK;
}

nsresult
TelephonyIPCService::GetLoopbackProvider(nsIVideoCallProvider **aProvider)
{
  if (!mLoopbackProvider) {
    if (!mPTelephonyChild) {
      NS_WARNING("TelephonyService used after shutdown has begun!");
      return NS_ERROR_FAILURE;
    }

    LOG("new VideoCallProviderChild");
    RefPtr<VideoCallProviderChild> child =
        new VideoCallProviderChild(CLIENT_ID_FAKE, CALL_INDEX_FAKE);

    mPTelephonyChild->SendPVideoCallProviderConstructor(child, CLIENT_ID_FAKE,
CALL_INDEX_FAKE); mLoopbackProvider = child;

  }

  RefPtr<nsIVideoCallProvider> provider(mLoopbackProvider);
  provider.forget(aProvider);
  return NS_OK;
}

void
TelephonyIPCService::RemoveVideoCallProvider(nsITelephonyCallInfo *aInfo)
{
  if (!aInfo) {
    return;
  }

  uint16_t state;
  aInfo->GetCallState(&state);
  if (state == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    uint32_t clientId;
    uint32_t callIndex;
    aInfo->GetClientId(&clientId);
    aInfo->GetCallIndex(&callIndex);
    RemoveVideoCallProvider(clientId, callIndex);
  }
}

void
TelephonyIPCService::RemoveVideoCallProvider(uint32_t aClientId, uint32_t
aCallIndex)
{
}
#endif
*/

NS_IMETHODIMP
TelephonyIPCService::GetHacMode(bool* aEnabled) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetHacMode(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetHacMode(bool aEnabled) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetHacMode(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::GetMicrophoneMuted(bool* aMuted) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetMicrophoneMuted(aMuted);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetMicrophoneMuted(bool aMuted) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetMicrophoneMuted(aMuted);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::GetSpeakerEnabled(bool* aEnabled) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetSpeakerEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetSpeakerEnabled(bool aEnabled) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetSpeakerEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::GetTtyMode(uint16_t* aMode) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetTtyMode(aMode);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetTtyMode(uint16_t aMode) {
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetTtyMode(aMode);
  return NS_OK;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyIPCService::CallStateChanged(uint32_t aLength,
                                      nsITelephonyCallInfo** aAllInfo) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->CallStateChanged(aLength, aAllInfo);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCallStateComplete() {
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCallState(nsITelephonyCallInfo* aInfo) {
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyIPCService::NotifyCdmaCallWaiting(uint32_t aClientId,
                                           const nsAString& aNumber,
                                           uint16_t aNumberPresentation,
                                           const nsAString& aName,
                                           uint16_t aNamePresentation) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyCdmaCallWaiting(
        aClientId, aNumber, aNumberPresentation, aName, aNamePresentation);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyConferenceError(const nsAString& aName,
                                           const nsAString& aMessage) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyConferenceError(aName, aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyRingbackTone(bool aPlayRingbackTone) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyRingbackTone(aPlayRingbackTone);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyTtyModeReceived(uint16_t mode) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyTtyModeReceived(mode);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyTelephonyCoverageLosing(uint16_t aType) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyTelephonyCoverageLosing(aType);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SupplementaryServiceNotification(
    uint32_t aClientId, int32_t aNotificationType, int32_t aCode,
    int32_t aIndex, int32_t aType, const nsAString& aNumber) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->SupplementaryServiceNotification(
        aClientId, aNotificationType, aCode, aIndex, aType, aNumber);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyRttModifyRequestReceived(uint32_t aClientId,
                                                    int32_t aCallIndex,
                                                    uint16_t aRttMode) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyRttModifyRequestReceived(aClientId, aCallIndex,
                                                  aRttMode);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyRttModifyResponseReceived(uint32_t aClientId,
                                                     int32_t aCallIndex,
                                                     uint16_t aStatus) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyRttModifyResponseReceived(aClientId, aCallIndex,
                                                   aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyRttMessageReceived(uint32_t aClientId,
                                              int32_t aCallIndex,
                                              const nsAString& aMessage) {
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyRttMessageReceived(aClientId, aCallIndex, aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifySrvccState(uint32_t aClientId, int32_t aState) {
  // no action.
  return NS_OK;
}
