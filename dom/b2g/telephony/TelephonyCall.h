/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TelephonyCallBinding.h"
#include "mozilla/dom/TelephonyCallId.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "mozilla/dom/TelephonyCallCapabilities.h"

#include "nsITelephonyService.h"
#include "nsITelephonyCallInfo.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

#ifdef MOZ_WIDGET_GONK
// class DOMVideoCallProvider;
#endif

class TelephonyCall final : public DOMEventTargetHelper {
  RefPtr<Telephony> mTelephony;
  RefPtr<TelephonyCallGroup> mGroup;

  RefPtr<TelephonyCallId> mId;
  RefPtr<TelephonyCallId> mSecondId;

  uint32_t mServiceId;
  TelephonyCallState mState;
  bool mEmergency;
  RefPtr<DOMException> mError;
  Nullable<TelephonyCallDisconnectedReason> mDisconnectedReason;

  bool mSwitchable;
  bool mMergeable;
  bool mMarkable;

  bool mSupportRtt;
  TelephonyRttMode mRttMode;
  TelephonyVerStatus mVerStatus;

  uint32_t mCallIndex;
  bool mLive;

  TelephonyCallVoiceQuality mVoiceQuality;
  TelephonyVideoCallState mVideoCallState;
  RefPtr<TelephonyCallCapabilities> mCapabilities;
  TelephonyCallRadioTech mRadioTech;
  TelephonyVowifiQuality mVowifiQuality;
#ifdef MOZ_WIDGET_GONK
//  mutable RefPtr<DOMVideoCallProvider> mVideoCallProvider;
#endif

  bool mIsConferenceParent;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  // NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall, DOMEventTargetHelper)
  friend class Telephony;

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  already_AddRefed<TelephonyCallId> Id() const;

  already_AddRefed<TelephonyCallId> GetSecondId() const;

  TelephonyCallState State() const { return mState; }

  bool Emergency() const { return mEmergency; }

  bool Switchable() const { return mSwitchable; }

  bool Mergeable() const { return mMergeable; }

  bool Markable() const { return mMarkable; }

  TelephonyCallVoiceQuality VoiceQuality() const { return mVoiceQuality; }

  TelephonyVideoCallState VideoCallState() const { return mVideoCallState; }

  already_AddRefed<TelephonyCallCapabilities> Capabilities() const;

  TelephonyCallRadioTech RadioTech() const { return mRadioTech; }

  TelephonyRttMode RttMode() const { return mRttMode; }

  TelephonyVowifiQuality VowifiQuality() const { return mVowifiQuality; }

  bool SupportRtt() const { return mSupportRtt; }

  TelephonyVerStatus VerStatus() const { return mVerStatus; }

  bool IsActive() const {
    return mState == TelephonyCallState::Dialing ||
           mState == TelephonyCallState::Alerting ||
           mState == TelephonyCallState::Connected;
  }

  already_AddRefed<DOMException> GetError() const;

  Nullable<TelephonyCallDisconnectedReason> GetDisconnectedReason() const {
    return mDisconnectedReason;
  }

  already_AddRefed<TelephonyCallGroup> GetGroup() const;

  already_AddRefed<Promise> Answer(uint16_t aType, const Optional<bool>& aIsRtt,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> HangUp(const Optional<bool>& aUnwanted,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Hold(ErrorResult& aRv);

  already_AddRefed<Promise> Resume(ErrorResult& aRv);

  already_AddRefed<Promise> SendRttModifyRequest(bool aEnable,
                                                 ErrorResult& aRv);

  already_AddRefed<Promise> SendRttModifyResponse(bool status,
                                                  ErrorResult& aRv);

  already_AddRefed<Promise> SendRttMessage(const nsAString& message,
                                           ErrorResult& aRv);

#ifdef MOZ_WIDGET_GONK
//  already_AddRefed<DOMVideoCallProvider>
//  GetVideoCallProvider(ErrorResult& aRv);
#endif

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(dialing)
  IMPL_EVENT_HANDLER(alerting)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(disconnected)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(groupchange)
  IMPL_EVENT_HANDLER(rttmodifyrequest)
  IMPL_EVENT_HANDLER(rttmodifyresponse)
  IMPL_EVENT_HANDLER(rttmessage)

  static TelephonyRttMode ConvertToTelephonyRttMode(uint16_t aRttMode);

  static TelephonyCallState ConvertToTelephonyCallState(uint16_t aCallState);

  static TelephonyCallVoiceQuality ConvertToTelephonyCallVoiceQuality(
      uint16_t aQuality);

  static TelephonyVideoCallState ConvertToTelephonyVideoCallState(
      uint16_t aState);

  static TelephonyCallRadioTech ConvertToTelephonyCallRadioTech(
      uint32_t aRadioTech);

  static TelephonyVowifiQuality ConvertToTelephonyVowifiQuality(
      uint32_t aVowifiQuality);

  static TelephonyVerStatus ConvertToTelephonyVerStatus(uint32_t aVerStatus);

  static already_AddRefed<TelephonyCall> Create(
      Telephony* aTelephony, TelephonyCallId* aId, uint32_t aServiceId,
      uint32_t aCallIndex, TelephonyCallState aState,
      TelephonyCallVoiceQuality aVoiceQuality, bool aEmergency = false,
      bool aConference = false, bool aSwitchable = true, bool aMergeable = true,
      bool aConferenceParent = false, bool aMarkable = false,
      TelephonyRttMode aRttMode = TelephonyRttMode::Off,
      uint32_t aCapabilities = 0,
      TelephonyVideoCallState aVideoCallState = TelephonyVideoCallState::Voice,
      TelephonyCallRadioTech aRadioTech = TelephonyCallRadioTech::Cs,
      TelephonyVowifiQuality aVowifiQuality = TelephonyVowifiQuality::None,
      TelephonyVerStatus aVerStatus = TelephonyVerStatus::None);

  void ChangeState(TelephonyCallState aState) {
    ChangeStateInternal(aState, true);
  }

  nsresult NotifyStateChanged();

  uint32_t ServiceId() const { return mServiceId; }

  uint32_t CallIndex() const { return mCallIndex; }

  bool IsConferenceParent() { return mIsConferenceParent; }

  void UpdateEmergency(bool aEmergency) { mEmergency = aEmergency; }

  void UpdateSwitchable(bool aSwitchable) { mSwitchable = aSwitchable; }

  void UpdateMergeable(bool aMergeable) { mMergeable = aMergeable; }

  void UpdateMarkable(bool aMarkable) { mMarkable = aMarkable; }

  void UpdateSecondId(TelephonyCallId* aId) { mSecondId = aId; }

  void UpdateIsConferenceParent(bool aIsParent);

  void UpdateVoiceQuality(TelephonyCallVoiceQuality aVoiceQuality) {
    mVoiceQuality = aVoiceQuality;
  }

  void UpdateVideoCallState(TelephonyVideoCallState aState) {
    mVideoCallState = aState;
  }

  void UpdateCapabilities(uint32_t aCapabilities) {
    mCapabilities->Update(aCapabilities);
    mSupportRtt = aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_RTT;
  }

  void UpdateRadioTech(TelephonyCallRadioTech aRadioTech) {
    mRadioTech = aRadioTech;
  }

  void UpdateVowifiQuality(TelephonyVowifiQuality aVowifiQuality) {
    mVowifiQuality = aVowifiQuality;
  }

  void UpdateRttMode(TelephonyRttMode aRttMode) { mRttMode = aRttMode; }

  void UpdateVerStatus(TelephonyVerStatus aVerStatus) {
    mVerStatus = aVerStatus;
  }

  void NotifyError(const nsAString& aError);

  void UpdateDisconnectedReason(const nsAString& aDisconnectedReason);

  void ChangeGroup(TelephonyCallGroup* aGroup);

  void NotifyRttModifyRequest(uint16_t aRttMode);

  void NotifyRttModifyResponse(uint16_t aStatus);

  void NotifyRttMessage(const nsAString& aMessage);

 private:
  explicit TelephonyCall(nsPIDOMWindowInner* aOwner);

  ~TelephonyCall();

  nsresult Hold(nsITelephonyCallback* aCallback);

  nsresult Resume(nsITelephonyCallback* aCallback);

  void ChangeStateInternal(TelephonyCallState aState, bool aFireEvents);

  nsresult DispatchCallEvent(const nsAString& aType, TelephonyCall* aCall);

  already_AddRefed<Promise> CreatePromise(ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_telephony_telephonycall_h__
