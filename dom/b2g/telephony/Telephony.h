/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephony_h__
#define mozilla_dom_telephony_telephony_h__

#include "AudioChannelService.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

#include "nsITelephonyCallInfo.h"
#include "nsITelephonyService.h"

// Need to include TelephonyCall.h because we have inline methods that
// assume they see the definition of TelephonyCall.
#include "TelephonyCall.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {
namespace telephony {

class TelephonyDialCallback;

}  // namespace telephony

class OwningTelephonyCallOrTelephonyCallGroup;
#ifdef MOZ_WIDGET_GONK
// class DOMVideoCallProvider;
#endif

enum class TtyMode : uint8_t;

class Telephony final : public DOMEventTargetHelper,
                        public nsIAudioChannelAgentCallback,
                        private nsITelephonyListener {
  /**
   * Class Telephony doesn't actually expose nsITelephonyListener.
   * Instead, it owns an nsITelephonyListener derived instance mListener
   * and passes it to nsITelephonyService. The onreceived events are first
   * delivered to mListener and then forwarded to its owner, Telephony. See
   * also bug 775997 comment #51.
   */
  class Listener;

  friend class telephony::TelephonyDialCallback;

  // The audio agent is needed to communicate with the audio channel service.
  RefPtr<AudioChannelAgent> mAudioChannelAgent;
  nsCOMPtr<nsITelephonyService> mService;
  RefPtr<Listener> mListener;

  nsTArray<RefPtr<TelephonyCall> > mCalls;
  RefPtr<CallsList> mCallsList;

  RefPtr<TelephonyCallGroup> mGroup;
#ifdef MOZ_WIDGET_GONK
  // mutable RefPtr<DOMVideoCallProvider> mLoopbackProvider;
#endif

  RefPtr<Promise> mReadyPromise;

  bool mIsAudioStartPlaying;
  bool mHaveDispatchedInterruptBeginEvent;
  bool mAudioChannelSuspended;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK
  NS_DECL_NSITELEPHONYLISTENER
  // NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Telephony, DOMEventTargetHelper)
  /*
    nsPIDOMWindowInner*
    GetParentObject() const
    {
      return GetOwner();
    }
  */
  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  already_AddRefed<Promise> Dial(const nsAString& aNumber,
                                 const uint16_t& aType, bool aRtt,
                                 const Optional<uint32_t>& aServiceId,
                                 ErrorResult& aRv);

  already_AddRefed<Promise> DialEmergency(const nsAString& aNumber, bool aRtt,
                                          const Optional<uint32_t>& aServiceId,
                                          ErrorResult& aRv);

  already_AddRefed<Promise> SendTones(const nsAString& aDTMFChars,
                                      uint32_t aPauseDuration,
                                      uint32_t aToneDuration,
                                      const Optional<uint32_t>& aServiceId,
                                      ErrorResult& aRv);

  void StartTone(const nsAString& aDTMFChar,
                 const Optional<uint32_t>& aServiceId, ErrorResult& aRv);

  void StopTone(const Optional<uint32_t>& aServiceId, ErrorResult& aRv);

  bool GetHacMode(ErrorResult& aRv) const;

  void SetHacMode(bool aEnabled, ErrorResult& aRv);

  // In the audio channel architecture, the system app needs to know the state
  // of every audio channel, including the telephony. Therefore, when a
  // telephony call is activated , the audio channel service would notify the
  // system app about that. And we need an agent to communicate with the audio
  // channel service. We would follow the call states to make a correct
  // notification.
  void OwnAudioChannel(ErrorResult& aRv);

  already_AddRefed<Promise> SendUSSD(const nsAString& aUssd,
                                     const Optional<uint32_t>& aServiceId,
                                     ErrorResult& aRv);

  already_AddRefed<Promise> CancelUSSD(const Optional<uint32_t>& aServiceId,
                                       ErrorResult& aRv);

  bool GetMuted(ErrorResult& aRv) const;

  void SetMuted(bool aMuted, ErrorResult& aRv);

  bool GetSpeakerEnabled(ErrorResult& aRv) const;

  void SetSpeakerEnabled(bool aEnabled, ErrorResult& aRv);

  TtyMode GetTtyMode(ErrorResult& aRv) const;

  void SetTtyMode(TtyMode aMode, ErrorResult& aRv);

  void GetActive(Nullable<OwningTelephonyCallOrTelephonyCallGroup>& aValue);

  already_AddRefed<CallsList> Calls() const;

  already_AddRefed<TelephonyCallGroup> ConferenceGroup() const;

  already_AddRefed<Promise> GetReady(ErrorResult& aRv) const;

  already_AddRefed<Promise> HangUpAllCalls(const Optional<uint32_t>& aServiceId,
                                           ErrorResult& aRv);

  already_AddRefed<Promise> GetEccList(const Optional<uint32_t>& aServiceId,
                                       ErrorResult& aRv);

#ifdef MOZ_WIDGET_GONK
  // already_AddRefed<DOMVideoCallProvider>
  // GetLoopbackProvider() const;
#endif

  IMPL_EVENT_HANDLER(incoming)
  IMPL_EVENT_HANDLER(callschanged)
  IMPL_EVENT_HANDLER(ringbacktone)
  IMPL_EVENT_HANDLER(suppservicenotification)
  IMPL_EVENT_HANDLER(ttymodereceived)
  IMPL_EVENT_HANDLER(telephonycoveragelosing)

  static already_AddRefed<Telephony> Create(nsIGlobalObject* aGlobal,
                                            ErrorResult& aRv);

  void AddCall(TelephonyCall* aCall) {
    NS_ASSERTION(!mCalls.Contains(aCall), "Already know about this one!");
    mCalls.AppendElement(aCall);
    NotifyCallsChanged(aCall);
  }

  void RemoveCall(TelephonyCall* aCall) {
    NS_ASSERTION(mCalls.Contains(aCall), "Didn't know about this one!");
    mCalls.RemoveElement(aCall);
    NotifyCallsChanged(aCall);
  }

  nsITelephonyService* Service() const { return mService; }

  const nsTArray<RefPtr<TelephonyCall> >& CallsArray() const { return mCalls; }

 private:
  explicit Telephony(nsIGlobalObject* aGlobal);
  ~Telephony();

  void Shutdown();

  static bool IsValidNumber(const nsAString& aNumber);

  static uint32_t GetNumServices();

  static bool IsValidServiceId(uint32_t aServiceId);

  uint32_t GetServiceId(const Optional<uint32_t>& aServiceId,
                        bool aGetIfActiveCall = false);

  already_AddRefed<Promise> DialInternal(
      uint32_t aServiceId, const nsAString& aNumber, bool aEmergency, bool aRtt,
      ErrorResult& aRv,
      uint16_t aType = nsITelephonyService::CALL_TYPE_VOICE_N_VIDEO);

  already_AddRefed<TelephonyCallId> CreateCallId(nsITelephonyCallInfo* aInfo);

  already_AddRefed<TelephonyCallId> CreateCallId(
      const nsAString& aNumber,
      uint16_t aNumberPresentation =
          nsITelephonyService::CALL_PRESENTATION_ALLOWED,
      const nsAString& aName = EmptyString(),
      uint16_t aNamePresentation =
          nsITelephonyService::CALL_PRESENTATION_ALLOWED);

  already_AddRefed<TelephonyCall> CreateCall(
      TelephonyCallId* aId, uint32_t aServiceId, uint32_t aCallIndex,
      TelephonyCallState aState, TelephonyCallVoiceQuality aVoiceQuality,
      TelephonyVideoCallState aVideoCallState = TelephonyVideoCallState::Voice,
      uint32_t aCapabilities = nsITelephonyCallInfo::CAPABILITY_SUPPORTS_NONE,
      TelephonyCallRadioTech aRadioTech = TelephonyCallRadioTech::Cs,
      bool aEmergency = false, bool aConference = false,
      bool aSwitchable = true, bool aMergeable = true,
      bool aConferenceParent = false, bool aMarkable = false,
      TelephonyRttMode aRtt = TelephonyRttMode::Off,
      TelephonyVowifiQuality aVowifiCallQuality = TelephonyVowifiQuality::None,
      TelephonyVerStatus aVerStatus = TelephonyVerStatus::None);

  nsresult NotifyEvent(const nsAString& aType);

  nsresult NotifyCallsChanged(TelephonyCall* aCall);

  nsresult DispatchCallEvent(const nsAString& aType, TelephonyCall* aCall);

  nsresult DispatchRingbackToneEvent(const nsAString& aType,
                                     bool playRingbackTone);

  nsresult DispatchSsnEvent(const nsAString& aType, int32_t notificationType,
                            int32_t code, int32_t index, int32_t type,
                            const nsAString& number);

  nsresult DispatchTtyModeReceived(const nsString& aType, uint16_t mode);

  nsresult DispatchTelephonyCoverageLosingEvent(const nsAString& aType,
                                                uint16_t type);

  nsresult DispatchRttMessageReceived(const nsAString& aType,
                                      uint32_t aClientId, uint32_t aCallIndex,
                                      const nsAString& aMessage);

  already_AddRefed<TelephonyCall> GetCall(uint32_t aServiceId,
                                          uint32_t aCallIndex);

  already_AddRefed<TelephonyCall> GetCallFromEverywhere(uint32_t aServiceId,
                                                        uint32_t aCallIndex);

  nsresult HandleCallInfo(nsITelephonyCallInfo* aInfo);

  // Check the call states to decide whether need to send the notificaiton.
  nsresult HandleAudioChannelState();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_telephony_telephony_h__
