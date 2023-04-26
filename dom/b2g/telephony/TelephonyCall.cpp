/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCall.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/RttModifyRequestReceivedEvent.h"
#include "mozilla/dom/RttModifyResponseReceivedEvent.h"
#include "mozilla/dom/RttMessageReceivedEvent.h"
#include "mozilla/dom/telephony/TelephonyCallback.h"

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#ifdef MOZ_WIDGET_GONK
// #include "mozilla/dom/DOMVideoCallProvider.h"
#endif
#include "nsPrintfCString.h"

#include "Telephony.h"
#include "TelephonyCallGroup.h"

#ifdef CONVERT_STRING_TO_NULLABLE_ENUM
#  undef CONVERT_STRING_TO_NULLABLE_ENUM
#endif

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)          \
  {                                                                         \
    _enum.SetNull();                                                        \
                                                                            \
    uint32_t i = 0;                                                         \
    for (const EnumEntry* entry = _enumType##Values::strings; entry->value; \
         ++entry, ++i) {                                                    \
      if (_string.EqualsASCII(entry->value)) {                              \
        _enum.SetValue(static_cast<_enumType>(i));                          \
        break;                                                              \
      }                                                                     \
    }                                                                       \
  }

#ifdef TELEPHONY_CALL_STATE
#  undef TELEPHONY_CALL_STATE
#endif

#define TELEPHONY_CALL_STATE(_state) \
  (TelephonyCallStateValues::strings[static_cast<int32_t>(_state)].value)

#ifdef MOZ_WIDGET_GONK
#  include <android/log.h>
#  undef LOG
#  define LOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, "TelephonyCall", ##args)
#else
#  undef LOG
#  define LOG(args...)
#endif

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using mozilla::ErrorResult;

// static
TelephonyRttMode TelephonyCall::ConvertToTelephonyRttMode(uint16_t aRttMode) {
  switch (aRttMode) {
    case nsITelephonyService::RTT_MODE_OFF:
      return TelephonyRttMode::Off;
    case nsITelephonyService::RTT_MODE_FULL:
      return TelephonyRttMode::Full;
  }

  // NS_NOTREACHED("Unknown state!");
  return TelephonyRttMode::Off;
}

// static
TelephonyCallState TelephonyCall::ConvertToTelephonyCallState(
    uint16_t aCallState) {
  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_DIALING:
      return TelephonyCallState::Dialing;
    case nsITelephonyService::CALL_STATE_ALERTING:
      return TelephonyCallState::Alerting;
    case nsITelephonyService::CALL_STATE_CONNECTED:
      return TelephonyCallState::Connected;
    case nsITelephonyService::CALL_STATE_HELD:
      return TelephonyCallState::Held;
    case nsITelephonyService::CALL_STATE_DISCONNECTED:
      return TelephonyCallState::Disconnected;
    case nsITelephonyService::CALL_STATE_INCOMING:
      return TelephonyCallState::Incoming;
  }

  // NS_NOTREACHED("Unknown state!");
  return TelephonyCallState::Disconnected;
}
// static
TelephonyCallVoiceQuality TelephonyCall::ConvertToTelephonyCallVoiceQuality(
    uint16_t aQuality) {
  switch (aQuality) {
    case nsITelephonyService::CALL_VOICE_QUALITY_HD:
      return TelephonyCallVoiceQuality::HD;
    case nsITelephonyService::CALL_VOICE_QUALITY_NORMAL:
      return TelephonyCallVoiceQuality::Normal;
  }

  // NS_NOTREACHED("Unknown quality!");
  return TelephonyCallVoiceQuality::Normal;
}

// static
TelephonyVideoCallState TelephonyCall::ConvertToTelephonyVideoCallState(
    uint16_t aState) {
  switch (aState) {
    case nsITelephonyCallInfo::STATE_AUDIO_ONLY:
      return TelephonyVideoCallState::Voice;
    case nsITelephonyCallInfo::STATE_TX_ENABLED:
      return TelephonyVideoCallState::TxEnabled;
    case nsITelephonyCallInfo::STATE_RX_ENABLED:
      return TelephonyVideoCallState::RxEnabled;
    case nsITelephonyCallInfo::STATE_BIDIRECTIONAL:
      return TelephonyVideoCallState::Bidirectional;
    case nsITelephonyCallInfo::STATE_PAUSED:
      return TelephonyVideoCallState::Paused;
  }

  // NS_NOTREACHED("Unknown videocallstate!");
  return TelephonyVideoCallState::Voice;
}

// static
TelephonyCallRadioTech TelephonyCall::ConvertToTelephonyCallRadioTech(
    uint32_t aRadioTech) {
  switch (aRadioTech) {
    case nsITelephonyCallInfo::RADIO_TECH_CS:
      return TelephonyCallRadioTech::Cs;
    case nsITelephonyCallInfo::RADIO_TECH_PS:
      return TelephonyCallRadioTech::Ps;
    case nsITelephonyCallInfo::RADIO_TECH_WIFI:
      return TelephonyCallRadioTech::Wifi;
  }

  // NS_NOTREACHED("Unknown videocallstate!");
  return TelephonyCallRadioTech::Cs;
}

// static
TelephonyVowifiQuality TelephonyCall::ConvertToTelephonyVowifiQuality(
    uint32_t aVowifiQuality) {
  switch (aVowifiQuality) {
    case nsITelephonyCallInfo::VOWIFI_QUALITY_NONE:
      return TelephonyVowifiQuality::None;
    case nsITelephonyCallInfo::VOWIFI_QUALITY_EXCELLENT:
      return TelephonyVowifiQuality::Excellent;
    case nsITelephonyCallInfo::VOWIFI_QUALITY_FAIR:
      return TelephonyVowifiQuality::Fair;
    case nsITelephonyCallInfo::VOWIFI_QUALITY_BAD:
      return TelephonyVowifiQuality::Bad;
  }

  // NS_NOTREACHED("Unknown vowifi quality!");
  return TelephonyVowifiQuality::None;
}

// static
TelephonyVerStatus TelephonyCall::ConvertToTelephonyVerStatus(
    uint32_t aVerStatus) {
  switch (aVerStatus) {
    case nsITelephonyCallInfo::VER_NONE:
      return TelephonyVerStatus::None;
    case nsITelephonyCallInfo::VER_FAIL:
      return TelephonyVerStatus::Fail;
    case nsITelephonyCallInfo::VER_PASS:
      return TelephonyVerStatus::Pass;
  }

  return TelephonyVerStatus::None;
}

// static
already_AddRefed<TelephonyCall> TelephonyCall::Create(
    Telephony* aTelephony, TelephonyCallId* aId, uint32_t aServiceId,
    uint32_t aCallIndex, TelephonyCallState aState,
    TelephonyCallVoiceQuality aVoiceQuality, bool aEmergency, bool aConference,
    bool aSwitchable, bool aMergeable, bool aConferenceParent, bool aMarkable,
    TelephonyRttMode aRttMode, uint32_t aCapabilities,
    TelephonyVideoCallState aVideoCallState, TelephonyCallRadioTech aRadioTech,
    TelephonyVowifiQuality aVowifiQuality, TelephonyVerStatus aVerStatus) {
  NS_ASSERTION(aTelephony, "Null aTelephony pointer!");
  NS_ASSERTION(aId, "Null aId pointer!");
  NS_ASSERTION(aCallIndex >= 1, "Invalid call index!");

  RefPtr<TelephonyCall> call = new TelephonyCall(aTelephony->GetOwner());

  call->mTelephony = aTelephony;
  call->mId = aId;
  call->mServiceId = aServiceId;
  call->mCallIndex = aCallIndex;
  call->mVoiceQuality = aVoiceQuality;
  call->mEmergency = aEmergency;
  call->mGroup = aConference ? aTelephony->ConferenceGroup() : nullptr;
  call->mSwitchable = aSwitchable;
  call->mMergeable = aMergeable;
  call->mError = nullptr;
  call->mIsConferenceParent = aConferenceParent;
  call->mMarkable = aMarkable;
  call->mVideoCallState = aVideoCallState;
  call->mRttMode = aRttMode;
  call->mSupportRtt =
      aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_RTT;
  call->mCapabilities =
      new TelephonyCallCapabilities(aTelephony->GetOwner(), aCapabilities);
  call->mRadioTech = aRadioTech;
  call->mVowifiQuality = aVowifiQuality;
  call->mVerStatus = aVerStatus;

  call->ChangeStateInternal(aState, false);
  return call.forget();
}

TelephonyCall::TelephonyCall(nsPIDOMWindowInner* aOwner)
    : DOMEventTargetHelper(aOwner), mLive(false) {}

TelephonyCall::~TelephonyCall() {
  LOG("~TelephonyCall");
  /*#ifdef MOZ_WIDGET_GONK
    if (mVideoCallProvider) {
      mVideoCallProvider->Shutdown();
      mVideoCallProvider = nullptr;
    }
  #endif
  */
}

JSObject* TelephonyCall::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return TelephonyCall_Binding::Wrap(aCx, this, aGivenProto);
}

void TelephonyCall::ChangeStateInternal(TelephonyCallState aState,
                                        bool aFireEvents) {
  RefPtr<TelephonyCall> kungFuDeathGrip(this);

  // Update current state
  mState = aState;

  if (mIsConferenceParent) {
    RefPtr<TelephonyCallGroup> group = mTelephony->ConferenceGroup();
    RefPtr<TelephonyCall> conferenceParentCall =
        group->GetConferenceParentCall();
    if (aState == TelephonyCallState::Disconnected) {
      if (conferenceParentCall) {
        group->SetConferenceParentCall(nullptr);
      }
    } else {
      if (conferenceParentCall != this) {
        group->SetConferenceParentCall(this);
      }
    }
    return;
  }

  // Handle disconnected calls
  if (mState == TelephonyCallState::Disconnected) {
    NS_ASSERTION(mLive, "Should be live!");
    mLive = false;
    if (mGroup) {
      mGroup->RemoveCall(this);
    } else {
      mTelephony->RemoveCall(this);
    }
  } else if (!mLive) {  // Handle newly added calls
    mLive = true;
    if (mGroup) {
      mGroup->AddCall(this);
    } else {
      mTelephony->AddCall(this);
    }
  }

  // Dispatch call state changed and call state event
  if (aFireEvents) {
    NotifyStateChanged();
  }
}

nsresult TelephonyCall::NotifyStateChanged() {
  // Since |mState| can be changed after statechange handler called back here,
  // we must save current state. Maybe we should figure out something smarter.
  TelephonyCallState prevState = mState;

  nsresult res = DispatchCallEvent(u"statechange"_ns, this);
  if (NS_FAILED(res)) {
    NS_WARNING("Failed to dispatch specific event!");
  }

  // Check whether |mState| remains the same after the statechange handler.
  if (mState != prevState) {
    NS_WARNING("Call State has changed by statechange handler!");
    return res;
  }

  res = DispatchCallEvent(NS_ConvertASCIItoUTF16(TELEPHONY_CALL_STATE(mState)),
                          this);
  if (NS_FAILED(res)) {
    NS_WARNING("Failed to dispatch a specific event!");
  }

  return res;
}

nsresult TelephonyCall::DispatchCallEvent(const nsAString& aType,
                                          TelephonyCall* aCall) {
  MOZ_ASSERT(aCall);

  CallEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCall = aCall;

  RefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);

  return DispatchTrustedEvent(event);
}

already_AddRefed<Promise> TelephonyCall::CreatePromise(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

void TelephonyCall::UpdateIsConferenceParent(bool aIsParent) {
  if (aIsParent == mIsConferenceParent) {
    return;
  }

  if (!aIsParent) {
    RefPtr<TelephonyCallGroup> group = mTelephony->ConferenceGroup();
    RefPtr<TelephonyCall> conferenceParentCall =
        group->GetConferenceParentCall();
    if (conferenceParentCall == this) {
      group->SetConferenceParentCall(nullptr);
    }
  }

  mIsConferenceParent = aIsParent;
}

void TelephonyCall::NotifyError(const nsAString& aError) {
  // Set the error string
  NS_ASSERTION(!mError, "Already have an error?");

  // TODO: fix DOMError or Exception impl
  // mError = new DOMException(NS_ERROR_DOM_INVALID_STATE_ERR, "TelephonyCall",
  // aError,
  //                     DOMException_Binding::INVALID_STATE_ERR);

  nsresult rv = DispatchCallEvent(u"error"_ns, this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch error event!");
  }
}

void TelephonyCall::UpdateDisconnectedReason(
    const nsAString& aDisconnectedReason) {
  NS_ASSERTION(Substring(aDisconnectedReason, aDisconnectedReason.Length() - 5)
                   .EqualsLiteral("Error"),
               "Disconnected reason should end with 'Error'");

  if (!mDisconnectedReason.IsNull()) {
    return;
  }

  // There is no 'Error' suffix in the corresponding enum. We should skip
  // that part for comparison.
  CONVERT_STRING_TO_NULLABLE_ENUM(aDisconnectedReason,
                                  TelephonyCallDisconnectedReason,
                                  mDisconnectedReason);

  if (!aDisconnectedReason.EqualsLiteral("NormalCallClearingError")) {
    NotifyError(aDisconnectedReason);
  }
}

void TelephonyCall::ChangeGroup(TelephonyCallGroup* aGroup) {
  mGroup = aGroup;

  nsresult rv = DispatchCallEvent(u"groupchange"_ns, this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch error event!");
  }
}

void TelephonyCall::NotifyRttModifyRequest(uint16_t aRttMode) {
  RttModifyRequestReceivedEventInit init;
  init.mMode = ConvertToTelephonyRttMode(aRttMode);
  RefPtr<RttModifyRequestReceivedEvent> event =
      RttModifyRequestReceivedEvent::Constructor(this, u"rttmodifyrequest"_ns,
                                                 init);
  nsresult rv = DispatchTrustedEvent(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch rttmodifyrequest event!");
  }
}

void TelephonyCall::NotifyRttModifyResponse(uint16_t aStatus) {
  RttModifyResponseReceivedEventInit init;
  init.mStatus = (unsigned short)aStatus;
  RefPtr<RttModifyResponseReceivedEvent> event =
      RttModifyResponseReceivedEvent::Constructor(this, u"rttmodifyresponse"_ns,
                                                  init);
  nsresult rv = DispatchTrustedEvent(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch rttmodifyresponse event!");
  }
}

void TelephonyCall::NotifyRttMessage(const nsAString& aMessage) {
  RttMessageReceivedEventInit init;
  init.mMessage = aMessage;
  RefPtr<RttMessageReceivedEvent> event =
      RttMessageReceivedEvent::Constructor(this, u"rttmessage"_ns, init);
  nsresult rv = DispatchTrustedEvent(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch rttmessage event!");
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TelephonyCall, DOMEventTargetHelper,
                                   mTelephony, mError, mGroup, mId, mSecondId,
                                   mCapabilities
#ifdef MOZ_WIDGET_GONK
//                                   ,mVideoCallProvider
#endif
);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TelephonyCall)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TelephonyCall, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TelephonyCall, DOMEventTargetHelper)

// TelephonyCall WebIDL

already_AddRefed<TelephonyCallId> TelephonyCall::Id() const {
  RefPtr<TelephonyCallId> id = mId;
  return id.forget();
}

already_AddRefed<TelephonyCallId> TelephonyCall::GetSecondId() const {
  RefPtr<TelephonyCallId> id = mSecondId;
  return id.forget();
}

already_AddRefed<DOMException> TelephonyCall::GetError() const {
  RefPtr<DOMException> error = mError;
  return error.forget();
}

already_AddRefed<TelephonyCallCapabilities> TelephonyCall::Capabilities()
    const {
  RefPtr<TelephonyCallCapabilities> capabilities = mCapabilities;
  return capabilities.forget();
}

already_AddRefed<TelephonyCallGroup> TelephonyCall::GetGroup() const {
  RefPtr<TelephonyCallGroup> group = mGroup;
  return group.forget();
}

already_AddRefed<Promise> TelephonyCall::Answer(uint16_t aType,
                                                const Optional<bool>& aIsRtt,
                                                ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mState != TelephonyCallState::Incoming) {
    NS_WARNING(nsPrintfCString("Answer on non-incoming call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState))
                   .get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  uint16_t rttMode = nsITelephonyService::RTT_MODE_OFF;
  if (aIsRtt.WasPassed()) {
    if (aIsRtt.Value()) {
      /* IsRtt to RttMode, now we only support RTT_MODE_FULL when enabled */
      rttMode = nsITelephonyService::RTT_MODE_FULL;
    }
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->AnswerCall(mServiceId, mCallIndex, aType,
                                          rttMode, callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise> TelephonyCall::HangUp(const Optional<bool>& aUnwanted,
                                                ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mState == TelephonyCallState::Disconnected) {
    NS_WARNING(nsPrintfCString("HangUp on a disconnected call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState))
                   .get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  uint16_t reason = nsITelephonyService::CALL_FAIL_NONE;
  if (aUnwanted.WasPassed()) {
    if (aUnwanted.Value()) {
      reason = nsITelephonyService::CALL_FAIL_USER_MARKED_UNWANTED;
      ;
    }
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mState == TelephonyCallState::Incoming
            ? mTelephony->Service()->RejectCall(mServiceId, mCallIndex, reason,
                                                callback)
            : mTelephony->Service()->HangUpCall(mServiceId, mCallIndex, reason,
                                                callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise> TelephonyCall::Hold(ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = Hold(callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

nsresult TelephonyCall::Hold(nsITelephonyCallback* aCallback) {
  if (mState != TelephonyCallState::Connected) {
    NS_WARNING(nsPrintfCString("Hold non-connected call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState))
                   .get());
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mGroup) {
    NS_WARNING("Hold a call in conference is rejected!");
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (!mSwitchable) {
    NS_WARNING("Hold a non-switchable call is rejected!");
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv =
      mTelephony->Service()->HoldCall(mServiceId, mCallIndex, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  if (mSecondId) {
    // No state transition when we switch two numbers within one TelephonyCall
    // object. Otherwise, the state here will be inconsistent with the backend
    // RIL and will never be right.
    return NS_OK;
  }

  return NS_OK;
}

already_AddRefed<Promise> TelephonyCall::Resume(ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = Resume(callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

nsresult TelephonyCall::Resume(nsITelephonyCallback* aCallback) {
  if (mState != TelephonyCallState::Held) {
    NS_WARNING(nsPrintfCString("Resume non-held call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState))
                   .get());
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mGroup) {
    NS_WARNING("Resume a call in conference is rejected!");
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (!mSwitchable) {
    NS_WARNING("Resume a non-switchable call is rejected!");
    aCallback->NotifyError(u"InvalidStateError"_ns);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv =
      mTelephony->Service()->ResumeCall(mServiceId, mCallIndex, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<Promise> TelephonyCall::SendRttModifyRequest(
    bool aEnable, ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }
  // We only support RTT_MODE_FULL
  uint16_t rttMode = aEnable ? nsITelephonyService::RTT_MODE_FULL
                             : nsITelephonyService::RTT_MODE_OFF;
  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->SendRttModify(mServiceId, mCallIndex, rttMode,
                                             callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise> TelephonyCall::SendRttModifyResponse(
    bool aStatus, ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->SendRttModifyResponse(mServiceId, mCallIndex,
                                                     aStatus, callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise> TelephonyCall::SendRttMessage(
    const nsAString& aMessage, ErrorResult& aRv) {
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->SendRttMessage(mServiceId, mCallIndex, aMessage,
                                              callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}
/*
#ifdef MOZ_WIDGET_GONK
already_AddRefed<DOMVideoCallProvider>
TelephonyCall::GetVideoCallProvider(ErrorResult& aRv)
{
  LOG("GetVideoCallProvider");
  if (mVideoCallProvider) {
    LOG("return cached provider");
    RefPtr<DOMVideoCallProvider> provider = mVideoCallProvider;
      return provider.forget();
  } else {
    LOG("return new provider");
    nsCOMPtr<nsIVideoCallProvider> handler;
    mTelephony->Service()->GetVideoCallProvider(mServiceId, mCallIndex,
getter_AddRefs(handler));
    // if fail to acquire nsIVideoCallProvider, return nullptr
    if (!handler) {
      LOG("no handler");
      return nullptr;
    }

    mVideoCallProvider = new DOMVideoCallProvider(GetOwner(), handler);
    RefPtr<DOMVideoCallProvider> provider = mVideoCallProvider;

    return provider.forget();
  }
}
#endif */
