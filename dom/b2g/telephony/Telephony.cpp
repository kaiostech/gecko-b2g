/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telephony.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/CallEvent.h"
// #include "mozilla/dom/MobileConnectionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TelephonyBinding.h"
#include "mozilla/dom/RingbackToneEvent.h"
#include "mozilla/dom/SuppServiceNotificationEvent.h"
#include "mozilla/dom/TtyModeReceivedEvent.h"
#include "mozilla/dom/TelephonyCoverageLosingEvent.h"
// #include "mozilla/unused.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "CallsList.h"
#include "TelephonyCall.h"
#include "TelephonyCallGroup.h"
#include "TelephonyCallId.h"
#include "TelephonyDialCallback.h"

// Service instantiation
#include "ipc/TelephonyIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#  include "nsIGonkTelephonyService.h"
#endif
#include "nsXULAppAPI.h"  // For XRE_GetProcessType()

// === SIMULATOR START ===
#ifdef MOZ_WIDGET_GONK
// #include "SystemProperty.h"
// #include "mozilla/dom/DOMVideoCallProvider.h"

#  define FEED_TEST_DATA_TO_PRODUCER
#  ifdef FEED_TEST_DATA_TO_PRODUCER
#    include <cutils/properties.h>
// #include "mozilla/dom/FakeVideoCallProvider.h"
#  endif

#  include <android/log.h>
#  undef LOG
#  define LOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, "Telephony", ##args)
#else
#  undef LOG
#  define LOG(args...)

#endif
// SIMULATOR END ===

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
#ifdef MOZ_WIDGET_GONK
// using namespace mozilla::system;
#endif
using mozilla::ErrorResult;

/**
 * The MT type of notification.
 */
#define SSN_MT 1
/**
 * The number of code1.
 */
#define SSN_CODE1_LENGTH 9

class Telephony::Listener : public nsITelephonyListener {
  Telephony* mTelephony;

  virtual ~Listener() {}

 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSITELEPHONYLISTENER(mTelephony)

  explicit Listener(Telephony* aTelephony) : mTelephony(aTelephony) {
    MOZ_ASSERT(mTelephony);
  }

  void Disconnect() {
    MOZ_ASSERT(mTelephony);
    mTelephony = nullptr;
  }
};

Telephony::Telephony(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal),
      mIsAudioStartPlaying(false),
      mHaveDispatchedInterruptBeginEvent(false),
      mAudioChannelSuspended(true) {
  MOZ_ASSERT(aGlobal);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(aGlobal, rv);
  MOZ_ASSERT(!rv.Failed());

  mReadyPromise = promise;
}

Telephony::~Telephony() { Shutdown(); }

void Telephony::Shutdown() {
  if (mListener) {
    mListener->Disconnect();

    if (mService) {
      mService->UnregisterListener(mListener);
      mService = nullptr;
    }

    mListener = nullptr;
  }
}

JSObject* Telephony::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Telephony_Binding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<Telephony> Telephony::Create(nsIGlobalObject* aGlobal,
                                              ErrorResult& aRv) {
  NS_ASSERTION(aGlobal, "Null aGlobal!");

  nsCOMPtr<nsITelephonyService> ril =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  if (!ril) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Telephony> telephony = new Telephony(aGlobal);

  telephony->mService = ril;
  telephony->mListener = new Listener(telephony);
  telephony->mCallsList = new CallsList(telephony);
  telephony->mGroup = TelephonyCallGroup::Create(telephony);

  nsresult rv = ril->EnumerateCalls(telephony->mListener);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return telephony.forget();
}

// static
bool Telephony::IsValidNumber(const nsAString& aNumber) {
  return !aNumber.IsEmpty();
}

// static
uint32_t Telephony::GetNumServices() {
  return mozilla::Preferences::GetInt("ril.numRadioInterfaces", 1);
}

// static
bool Telephony::IsValidServiceId(uint32_t aServiceId) {
  return aServiceId < GetNumServices();
}

uint32_t Telephony::GetServiceId(const Optional<uint32_t>& aServiceId,
                                 bool aGetIfActiveCall) {
  if (aServiceId.WasPassed()) {
    return aServiceId.Value();
  } else if (aGetIfActiveCall) {
    if (mGroup->IsActive()) {
      return mGroup->GetServiceId();
    }

    for (uint32_t i = 0; i < mCalls.Length(); i++) {
      if (mCalls[i]->IsActive()) {
        return mCalls[i]->mServiceId;
      }
    }
  }

  uint32_t serviceId = 0;
  mService->GetDefaultServiceId(&serviceId);
  return serviceId;
}

already_AddRefed<Promise> Telephony::DialInternal(uint32_t aServiceId,
                                                  const nsAString& aNumber,
                                                  bool aEmergency, bool aRtt,
                                                  ErrorResult& aRv,
                                                  uint16_t aType) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!IsValidNumber(aNumber) || !IsValidServiceId(aServiceId)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyDialCallback> callback =
      new TelephonyDialCallback(GetOwner(), this, promise);

  uint16_t rttMode = aRtt ? nsITelephonyService::RTT_MODE_FULL
                          : nsITelephonyService::RTT_MODE_OFF;
  nsresult rv = mService->Dial(
      aServiceId, aNumber, aEmergency, aType,
      /* aRtt to RttMode, now we only support RTT_MODE_FULL when enabled */
      rttMode, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<TelephonyCallId> Telephony::CreateCallId(
    nsITelephonyCallInfo* aInfo) {
  nsAutoString number;
  nsAutoString name;
  uint16_t numberPresentation;
  uint16_t namePresentation;

  aInfo->GetNumber(number);
  aInfo->GetName(name);
  aInfo->GetNumberPresentation(&numberPresentation);
  aInfo->GetNamePresentation(&namePresentation);

  return CreateCallId(number, numberPresentation, name, namePresentation);
}

already_AddRefed<TelephonyCallId> Telephony::CreateCallId(
    const nsAString& aNumber, uint16_t aNumberPresentation,
    const nsAString& aName, uint16_t aNamePresentation) {
  RefPtr<TelephonyCallId> id = new TelephonyCallId(
      GetOwner(), aNumber, aNumberPresentation, aName, aNamePresentation);

  return id.forget();
}

already_AddRefed<TelephonyCall> Telephony::CreateCall(
    TelephonyCallId* aId, uint32_t aServiceId, uint32_t aCallIndex,
    TelephonyCallState aState, TelephonyCallVoiceQuality aVoiceQuality,
    TelephonyVideoCallState aVideoCallState, uint32_t aCapabilities,
    TelephonyCallRadioTech aRadioTech, bool aEmergency, bool aConference,
    bool aSwitchable, bool aMergeable, bool aConferenceParent, bool aMarkable,
    TelephonyRttMode aRttMode, TelephonyVowifiQuality aVowifiCallQuality,
    TelephonyVerStatus aVerStatus) {
  // We don't have to create an already ended call.
  if (aState == TelephonyCallState::Disconnected) {
    return nullptr;
  }

  RefPtr<TelephonyCall> call = TelephonyCall::Create(
      this, aId, aServiceId, aCallIndex, aState, aVoiceQuality, aEmergency,
      aConference, aSwitchable, aMergeable, aConferenceParent, aMarkable,
      aRttMode, aCapabilities, aVideoCallState, aRadioTech, aVowifiCallQuality,
      aVerStatus);

  NS_ASSERTION(call, "This should never fail!");
  NS_ASSERTION(
      aConference ? mGroup->CallsArray().Contains(call) : mCalls.Contains(call),
      "Should have auto-added new call!");

  return call.forget();
}

nsresult Telephony::NotifyEvent(const nsAString& aType) {
  return DispatchCallEvent(aType, nullptr);
}

nsresult Telephony::NotifyCallsChanged(TelephonyCall* aCall) {
  return DispatchCallEvent(u"callschanged"_ns, aCall);
}

already_AddRefed<TelephonyCall> Telephony::GetCall(uint32_t aServiceId,
                                                   uint32_t aCallIndex) {
  RefPtr<TelephonyCall> call;

  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    RefPtr<TelephonyCall>& tempCall = mCalls[i];
    if (tempCall->ServiceId() == aServiceId &&
        tempCall->CallIndex() == aCallIndex) {
      call = tempCall;
      return call.forget();
    }
  }

  return nullptr;
}

already_AddRefed<TelephonyCall> Telephony::GetCallFromEverywhere(
    uint32_t aServiceId, uint32_t aCallIndex) {
  RefPtr<TelephonyCall> call = GetCall(aServiceId, aCallIndex);
  if (call) {
    return call.forget();
  }

  call = mGroup->GetCall(aServiceId, aCallIndex);
  if (call) {
    return call.forget();
  }

  return nullptr;
}

nsresult Telephony::HandleCallInfo(nsITelephonyCallInfo* aInfo) {
  uint32_t serviceId;
  uint32_t callIndex;
  uint16_t callState;
  uint16_t callQuality;
  uint16_t videoCallState;
  uint32_t capabilities;
  uint32_t callRadioTech;
  uint32_t callVowifiCallQuality;
  uint16_t rttMode;
  uint32_t callVerStatus;

  bool isEmergency;
  bool isConference;
  bool isSwitchable;
  bool isMergeable;
  bool isConferenceParent;
  bool isMarkable;

  aInfo->GetClientId(&serviceId);
  aInfo->GetCallIndex(&callIndex);
  aInfo->GetCallState(&callState);
  aInfo->GetVoiceQuality(&callQuality);
  aInfo->GetVideoCallState(&videoCallState);
  aInfo->GetCapabilities(&capabilities);
  aInfo->GetRadioTech(&callRadioTech);
  aInfo->GetVowifiCallQuality(&callVowifiCallQuality);
  aInfo->GetRttMode(&rttMode);
  aInfo->GetVerStatus(&callVerStatus);

  aInfo->GetIsEmergency(&isEmergency);
  aInfo->GetIsConference(&isConference);
  aInfo->GetIsSwitchable(&isSwitchable);
  aInfo->GetIsMergeable(&isMergeable);
  aInfo->GetIsConferenceParent(&isConferenceParent);
  aInfo->GetIsMarkable(&isMarkable);

  TelephonyCallState state =
      TelephonyCall::ConvertToTelephonyCallState(callState);
  TelephonyCallVoiceQuality quality =
      TelephonyCall::ConvertToTelephonyCallVoiceQuality(callQuality);
  TelephonyVideoCallState videoState =
      TelephonyCall::ConvertToTelephonyVideoCallState(videoCallState);
  TelephonyCallRadioTech radioTech =
      TelephonyCall::ConvertToTelephonyCallRadioTech(callRadioTech);

  TelephonyRttMode rtt = TelephonyCall::ConvertToTelephonyRttMode(rttMode);
  TelephonyVowifiQuality vowifiCallQuality =
      TelephonyCall::ConvertToTelephonyVowifiQuality(callVowifiCallQuality);
  TelephonyVerStatus verStatus =
      TelephonyCall::ConvertToTelephonyVerStatus(callVerStatus);

  RefPtr<TelephonyCall> call = GetCallFromEverywhere(serviceId, callIndex);
  // Handle a newly created call.
  if (!call) {
    RefPtr<TelephonyCallId> id = CreateCallId(aInfo);
    call = CreateCall(id, serviceId, callIndex, state, quality, videoState,
                      capabilities, radioTech, isEmergency, isConference,
                      isSwitchable, isMergeable, isConferenceParent, isMarkable,
                      rtt, vowifiCallQuality, verStatus);
    // The newly created call is an incoming call.
    if (call && state == TelephonyCallState::Incoming) {
      nsresult rv = DispatchCallEvent(u"incoming"_ns, call);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  // Update an existing call
  // TODO We should update call detail inside a call instance, instead of
  // telephony.
  // TODO We should update attribute only if it changed.
  call->UpdateEmergency(isEmergency);
  call->UpdateSwitchable(isSwitchable);
  call->UpdateMergeable(isMergeable);
  bool changed = isConferenceParent != call->IsConferenceParent();
  call->UpdateIsConferenceParent(isConferenceParent);

  changed |= quality != call->VoiceQuality();
  call->UpdateVoiceQuality(quality);

  changed |= videoState != call->VideoCallState();
  call->UpdateVideoCallState(videoState);

  RefPtr<TelephonyCallCapabilities> prevCapabilities = call->Capabilities();
  changed |= !prevCapabilities->Equals(capabilities);
  call->UpdateCapabilities(capabilities);

  call->mSupportRtt =
      capabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_RTT;

  changed |= radioTech != call->RadioTech();
  call->UpdateRadioTech(radioTech);

  changed |= vowifiCallQuality != call->VowifiQuality();
  call->UpdateVowifiQuality(vowifiCallQuality);

  changed |= isMarkable != call->Markable();
  call->UpdateMarkable(isMarkable);

  changed |= rtt != call->RttMode();
  call->UpdateRttMode(rtt);

  changed |= verStatus != call->VerStatus();
  call->UpdateVerStatus(verStatus);

  nsAutoString number;
  nsAutoString name;
  uint16_t numberPresentation;
  uint16_t namePresentation;
  aInfo->GetNumber(number);
  aInfo->GetName(name);
  aInfo->GetNumberPresentation(&numberPresentation);
  aInfo->GetNamePresentation(&namePresentation);
  RefPtr<TelephonyCallId> id = call->Id();
  id->UpdateNumber(number);
  id->UpdateName(name);
  id->UpdateNumberPresentation(numberPresentation);
  id->UpdateNamePresentation(namePresentation);

  nsAutoString disconnectedReason;
  aInfo->GetDisconnectedReason(disconnectedReason);

  changed |= call->State() != state;

  // State changed.
  if (changed) {
    if (state == TelephonyCallState::Disconnected) {
      call->UpdateDisconnectedReason(disconnectedReason);
      call->ChangeState(TelephonyCallState::Disconnected);
      return NS_OK;
    }

    // We don't fire the statechange event on a call in conference here.
    // Instead, the event will be fired later in
    // TelephonyCallGroup::ChangeState(). Thus the sequence of firing the
    // statechange events is guaranteed: first on TelephonyCallGroup then on
    // individual TelephonyCall objects.
    bool fireEvent = !isConference;
    call->ChangeStateInternal(state, fireEvent);
  }

  // Group changed.
  RefPtr<TelephonyCallGroup> group = call->GetGroup();

  if (!group && isConference) {
    // Add to conference.
    NS_ASSERTION(mCalls.Contains(call), "Should in mCalls");
    mGroup->AddCall(call);
    RemoveCall(call);
  } else if (group && !isConference) {
    // Remove from conference.
    NS_ASSERTION(mGroup->CallsArray().Contains(call), "Should in mGroup");
    mGroup->RemoveCall(call);
    AddCall(call);
  }

  return NS_OK;
}

already_AddRefed<Promise> Telephony::HangUpAllCalls(
    const Optional<uint32_t>& aServiceId, ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  nsresult rv = mService->HangUpAllCalls(serviceId, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<Promise> Telephony::GetEccList(
    const Optional<uint32_t>& aServiceId, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!promise) {
    return nullptr;
  }

#ifdef MOZ_WIDGET_GONK
#  define KEY_MAX_LENGTH 32
#  define VALUE_MAX_LENGTH 92
  char propKey[KEY_MAX_LENGTH];
  char propValue[VALUE_MAX_LENGTH];

  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);
  switch (serviceId) {
    case 1:
      strcpy(propKey, "ril.ecclist1");
      break;
    case 0:
    default:
      strcpy(propKey, "ril.ecclist");
      break;
  }

  if (property_get(propKey, propValue, "") == 0) {
    strcpy(propKey, "ro.ril.ecclist");
    property_get(propKey, propValue, "");
  }

  promise->MaybeResolve(NS_ConvertASCIItoUTF16(propValue));
#else
  promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
#endif

  return promise.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Telephony)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Telephony,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadyPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Telephony, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadyPromise)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Telephony)
  // Telephony does not expose nsITelephonyListener.  mListener is the exposed
  // nsITelephonyListener and forwards the calls it receives to us.
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Telephony, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Telephony, DOMEventTargetHelper)

NS_IMPL_ISUPPORTS(Telephony::Listener, nsITelephonyListener)

// Telephony WebIDL

already_AddRefed<Promise> Telephony::Dial(const nsAString& aNumber,
                                          const uint16_t& aType, bool aRtt,
                                          const Optional<uint32_t>& aServiceId,
                                          ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId);
  RefPtr<Promise> promise =
      DialInternal(serviceId, aNumber, false, aRtt, aRv, aType);
  return promise.forget();
}

already_AddRefed<Promise> Telephony::DialEmergency(
    const nsAString& aNumber, bool aRtt, const Optional<uint32_t>& aServiceId,
    ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId);
  RefPtr<Promise> promise = DialInternal(serviceId, aNumber, true, aRtt, aRv);
  return promise.forget();
}

already_AddRefed<Promise> Telephony::SendTones(
    const nsAString& aDTMFChars, uint32_t aPauseDuration,
    uint32_t aToneDuration, const Optional<uint32_t>& aServiceId,
    ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aDTMFChars.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  if (!IsValidServiceId(serviceId)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  aRv = mService->SendTones(serviceId, aDTMFChars, aPauseDuration,
                            aToneDuration, callback);
  return promise.forget();
}

void Telephony::StartTone(const nsAString& aDTMFChar,
                          const Optional<uint32_t>& aServiceId,
                          ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);

  if (aDTMFChar.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    return;
  }

  if (aDTMFChar.Length() > 1 || !IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mService->StartTone(serviceId, aDTMFChar);
}

void Telephony::StopTone(const Optional<uint32_t>& aServiceId,
                         ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);

  if (!IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mService->StopTone(serviceId);
}

void Telephony::OwnAudioChannel(ErrorResult& aRv) {
  if (mAudioChannelAgent) {
    return;
  }
  mAudioChannelAgent = new AudioChannelAgent();
  MOZ_ASSERT(mAudioChannelAgent);
  aRv = mAudioChannelAgent->Init(
      GetOwner(), static_cast<int32_t>(AudioChannel::Telephony), this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  aRv = HandleAudioChannelState();
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

already_AddRefed<Promise> Telephony::SendUSSD(
    const nsAString& aUssd, const Optional<uint32_t>& aServiceId,
    ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  nsresult rv = mService->SendUSSD(serviceId, aUssd, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise> Telephony::CancelUSSD(
    const Optional<uint32_t>& aServiceId, ErrorResult& aRv) {
  uint32_t serviceId = GetServiceId(aServiceId, true /* aGetIfActiveCall */);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);

  nsresult rv = mService->CancelUSSD(serviceId, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}

nsresult Telephony::HandleAudioChannelState() {
  if (!mAudioChannelAgent) {
    return NS_OK;
  }

  Nullable<OwningTelephonyCallOrTelephonyCallGroup> activeCall;
  GetActive(activeCall);
  nsresult rv;
  // Only stop the agent when there's no call.
  RefPtr<TelephonyCall> conferenceParentCall =
      mGroup->GetConferenceParentCall();
  if ((!mCalls.Length() && !mGroup->CallsArray().Length() &&
       !conferenceParentCall) &&
      mIsAudioStartPlaying) {
    mIsAudioStartPlaying = false;
    rv = mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (!activeCall.IsNull() && !mIsAudioStartPlaying) {
    mIsAudioStartPlaying = true;
    rv = mAudioChannelAgent->NotifyStartedPlaying(
        AudioChannelService::AudibleState::eAudible);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // In B2G, the system app manages audio playback policy. If there is a new
    // sound want to be playback, it must wait for the permission from the
    // system app. It means that the sound would be suspended first, and then be
    // resumed. For telephony, the behaviors are hold() first, then resume().
    // However, the telephony service can't handle all these requests within a
    // short period. The telephony service would reject our resume request,
    // because the modem have not changed the call state yet. It causes that the
    // telephony can't be resumed. Therefore, we don't call PullInitialUpdate()
    // so we won't be suspended at the beginning.
  }
  return NS_OK;
}
/*
#ifdef MOZ_WIDGET_GONK
already_AddRefed<DOMVideoCallProvider>
Telephony::GetLoopbackProvider() const
{
  LOG("GetLoopbackProvider");

  if (mLoopbackProvider) {
    LOG("return cached provider");
    RefPtr<DOMVideoCallProvider> provider = mLoopbackProvider;
      return provider.forget();
  } else {
    LOG("return new provider");
    nsCOMPtr<nsIVideoCallProvider> handler;
#ifdef FEED_TEST_DATA_TO_PRODUCER
    char prop[128];
    if ((property_get("vt.loopback", prop, NULL) != 0) &&
        (strcmp(prop, "1") == 0)) {
      handler = new FakeVideoCallProvider();
    } else {
#endif
      mService->GetVideoCallProvider(1000, 1000, getter_AddRefs(handler));
#ifdef FEED_TEST_DATA_TO_PRODUCER
    }
#endif
    // if fail to acquire nsIVideoCallProvider, return nullptr
    mLoopbackProvider = new DOMVideoCallProvider(GetOwner(), handler);
    RefPtr<DOMVideoCallProvider> provider = mLoopbackProvider;

    return provider.forget();
  }
}
#endif
*/
bool Telephony::GetHacMode(ErrorResult& aRv) const {
  bool enabled = false;
  aRv = mService->GetHacMode(&enabled);

  return enabled;
}

void Telephony::SetHacMode(bool aEnabled, ErrorResult& aRv) {
  aRv = mService->SetHacMode(aEnabled);
}

bool Telephony::GetMuted(ErrorResult& aRv) const {
  bool muted = false;
  aRv = mService->GetMicrophoneMuted(&muted);

  return muted;
}

void Telephony::SetMuted(bool aMuted, ErrorResult& aRv) {
  aRv = mService->SetMicrophoneMuted(aMuted);
}

bool Telephony::GetSpeakerEnabled(ErrorResult& aRv) const {
  bool enabled = false;
  aRv = mService->GetSpeakerEnabled(&enabled);

  return enabled;
}

void Telephony::SetSpeakerEnabled(bool aEnabled, ErrorResult& aRv) {
  aRv = mService->SetSpeakerEnabled(aEnabled);
}

TtyMode Telephony::GetTtyMode(ErrorResult& aRv) const {
  uint16_t mode;
  aRv = mService->GetTtyMode(&mode);

  return static_cast<TtyMode>(mode);
}

void Telephony::SetTtyMode(TtyMode aMode, ErrorResult& aRv) {
  aRv = mService->SetTtyMode(static_cast<uint16_t>(aMode));
}

void Telephony::GetActive(
    Nullable<OwningTelephonyCallOrTelephonyCallGroup>& aValue) {
  if (mGroup->IsActive()) {
    aValue.SetValue().SetAsTelephonyCallGroup() = mGroup;
    return;
  }

  // Search for the active call.
  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    if (mCalls[i]->IsActive()) {
      aValue.SetValue().SetAsTelephonyCall() = mCalls[i];
      return;
    }
  }

  // Nothing active found.
  aValue.SetNull();
}

already_AddRefed<CallsList> Telephony::Calls() const {
  RefPtr<CallsList> list = mCallsList;
  return list.forget();
}

already_AddRefed<TelephonyCallGroup> Telephony::ConferenceGroup() const {
  RefPtr<TelephonyCallGroup> group = mGroup;
  return group.forget();
}

already_AddRefed<Promise> Telephony::GetReady(ErrorResult& aRv) const {
  if (!mReadyPromise) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = mReadyPromise;
  return promise.forget();
}

// nsIAudioChannelAgentCallback

NS_IMETHODIMP
Telephony::WindowVolumeChanged(float aVolume, bool aMuted) {
  // Do nothing, it's useless for the telephony object.
  return NS_OK;
}

NS_IMETHODIMP
Telephony::WindowAudioCaptureChanged(bool aCapture) {
  // Do nothing, it's useless for the telephony object.
  return NS_OK;
}

NS_IMETHODIMP
Telephony::WindowSuspendChanged(uint32_t aSuspend) {
  // It's impossible to put all the calls on-hold in the multi-call case.
  if (mCalls.Length() > 1 ||
      (mCalls.Length() == 1 && mGroup->CallsArray().Length())) {
    return NS_ERROR_FAILURE;
  }

  // These events will be triggered when the telephony is interrupted by other
  // audio channel.
  bool suspended = aSuspend != nsISuspendedTypes::NONE_SUSPENDED;
  if (mAudioChannelSuspended != suspended) {
    mAudioChannelSuspended = suspended;
    // We should not dispatch "mozinterruptend" when the system app initializes
    // the telephony audio from muted to unmuted at the first time. The event
    // "mozinterruptend" must be dispatched after the "mozinterruptbegin".
    if (!mHaveDispatchedInterruptBeginEvent && mAudioChannelSuspended) {
      DispatchTrustedEvent(u"mozinterruptbegin"_ns);
      mHaveDispatchedInterruptBeginEvent = mAudioChannelSuspended;
    } else if (mHaveDispatchedInterruptBeginEvent && !mAudioChannelSuspended) {
      DispatchTrustedEvent(u"mozinterruptend"_ns);
      mHaveDispatchedInterruptBeginEvent = mAudioChannelSuspended;
    }
  }
  return NS_OK;
}

// nsITelephonyListener

NS_IMETHODIMP
Telephony::CallStateChanged(uint32_t aLength, nsITelephonyCallInfo** aAllInfo) {
  // Update call state
  nsresult rv;
  for (uint32_t i = 0; i < aLength; ++i) {
    rv = HandleCallInfo(aAllInfo[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Update conference state
  mGroup->ChangeState();

  rv = HandleAudioChannelState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(nsITelephonyCallInfo* aInfo) {
  return HandleCallInfo(aInfo);
}

NS_IMETHODIMP
Telephony::EnumerateCallStateComplete() {
  // Set conference state.
  mGroup->ChangeState();

  HandleAudioChannelState();
  if (mReadyPromise) {
    mReadyPromise->MaybeResolve(JS::UndefinedHandleValue);
  }

  if (NS_FAILED(mService->RegisterListener(mListener))) {
    NS_WARNING("Failed to register listener!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::SupplementaryServiceNotification(uint32_t aServiceId,
                                            int32_t aNotificationType,
                                            int32_t aCode, int32_t aIndex,
                                            int32_t aType,
                                            const nsAString& aNumber) {
  DispatchSsnEvent(u"suppservicenotification"_ns, aNotificationType, aCode,
                   aIndex, aIndex, aNumber);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyCdmaCallWaiting(uint32_t aServiceId, const nsAString& aNumber,
                                 uint16_t aNumberPresentation,
                                 const nsAString& aName,
                                 uint16_t aNamePresentation) {
  MOZ_ASSERT(mCalls.Length() == 1);

  RefPtr<TelephonyCall> callToNotify = mCalls[0];
  MOZ_ASSERT(callToNotify && callToNotify->ServiceId() == aServiceId);

  RefPtr<TelephonyCallId> id = new TelephonyCallId(
      GetOwner(), aNumber, aNumberPresentation, aName, aNamePresentation);
  callToNotify->UpdateSecondId(id);
  DispatchCallEvent(u"callschanged"_ns, callToNotify);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyConferenceError(const nsAString& aName,
                                 const nsAString& aMessage) {
  mGroup->NotifyError(aName, aMessage);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyRingbackTone(bool aPlayRingbackTone) {
  DispatchRingbackToneEvent(u"ringbacktone"_ns, aPlayRingbackTone);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyTtyModeReceived(uint16_t aMode) {
  DispatchTtyModeReceived(u"ttymodereceived"_ns, aMode);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyTelephonyCoverageLosing(uint16_t aType) {
  DispatchTelephonyCoverageLosingEvent(u"telephonycoveragelosing"_ns, aType);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyRttModifyRequestReceived(uint32_t aClientId,
                                          int32_t aCallIndex,
                                          uint16_t aRttMode) {
  RefPtr<TelephonyCall> aCall;
  if (!mCalls.IsEmpty()) {
    aCall = GetCall(aClientId, aCallIndex);
    if (aCall) {
      aCall->NotifyRttModifyRequest(aRttMode);
    } else {
      NS_WARNING("Failed to notify RttModifyRequest: wrong CallIndex!");
    }
  } else {
    NS_WARNING("Failed to notify RttModifyRequest: no call exist!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyRttModifyResponseReceived(uint32_t aClientId,
                                           int32_t aCallIndex,
                                           uint16_t aStatus) {
  RefPtr<TelephonyCall> aCall;
  if (!mCalls.IsEmpty()) {
    aCall = GetCall(aClientId, aCallIndex);
    if (aCall) {
      aCall->NotifyRttModifyResponse(aStatus);
    } else {
      NS_WARNING("Failed to notify RttModifyResponse: wrong CallIndex!");
    }
  } else {
    NS_WARNING("Failed to notify RttModifyResponse: no call exist!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyRttMessageReceived(uint32_t aClientId, int32_t aCallIndex,
                                    const nsAString& aMessage) {
  RefPtr<TelephonyCall> aCall;
  if (!mCalls.IsEmpty()) {
    aCall = GetCall(aClientId, aCallIndex);
    if (aCall) {
      aCall->NotifyRttMessage(aMessage);
    } else {
      NS_WARNING("Failed to notify RttMessage: wrong CallIndex!");
    }
  } else {
    NS_WARNING("Failed to notify RttMessage: no call exist!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifySrvccState(uint32_t aClientId, int32_t aState) {
  return NS_OK;
}

nsresult Telephony::DispatchCallEvent(const nsAString& aType,
                                      TelephonyCall* aCall) {
  // If it is an incoming event, the call should not be null.
  MOZ_ASSERT(!aType.EqualsLiteral("incoming") || aCall);

  CallEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCall = aCall;

  RefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);

  return DispatchTrustedEvent(event);
}

nsresult Telephony::DispatchRingbackToneEvent(const nsAString& aType,
                                              bool playRingbackTone) {
  RingbackToneEventInit init;
  init.mPlayRingbackTone = playRingbackTone;
  RefPtr<RingbackToneEvent> event =
      RingbackToneEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult Telephony::DispatchSsnEvent(const nsAString& aType,
                                     int32_t notificationType, int32_t code,
                                     int32_t index, int32_t type,
                                     const nsAString& number) {
  SuppServiceNotificationEventInit init;
  init.mNotificationType = static_cast<NotifyType>(notificationType);
  if (notificationType == SSN_MT) code = code + SSN_CODE1_LENGTH;
  init.mCode = static_cast<CodeType>(code);
  init.mIndex = index;
  init.mType = type;
  init.mNumber = number;
  RefPtr<SuppServiceNotificationEvent> event =
      SuppServiceNotificationEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult Telephony::DispatchTelephonyCoverageLosingEvent(const nsAString& aType,
                                                         uint16_t type) {
  TelephonyCoverageLosingEventInit init;
  init.mType = static_cast<TelephonyBearer>(type);
  RefPtr<TelephonyCoverageLosingEvent> event =
      TelephonyCoverageLosingEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult Telephony::DispatchTtyModeReceived(const nsString& aType,
                                            uint16_t aMode) {
  TtyModeReceivedEventInit init;
  init.mMode = static_cast<TtyMode>(aMode);
  RefPtr<TtyModeReceivedEvent> event =
      TtyModeReceivedEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

already_AddRefed<nsITelephonyService> NS_CreateTelephonyService() {
  nsCOMPtr<nsITelephonyService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::telephony::TelephonyIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    service = do_GetService(GONK_TELEPHONY_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}
