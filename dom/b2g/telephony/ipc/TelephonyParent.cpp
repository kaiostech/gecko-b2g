/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyParent.h"
#include "nsServiceManagerUtils.h"
#ifdef MOZ_WIDGET_GONK
// #include "mozilla/dom/videocallprovider/VideoCallProviderParent.h"
#endif

USING_TELEPHONY_NAMESPACE
USING_VIDEOCALLPROVIDER_NAMESPACE

/*******************************************************************************
 * TelephonyParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyParent, nsITelephonyListener)

TelephonyParent::TelephonyParent()
    : mActorDestroyed(false), mRegistered(false) {}

void TelephonyParent::ActorDestroy(ActorDestroyReason why) {
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mActorDestroyed is set to true. Return
  // an error here to avoid sending a message to the dead process.
  mActorDestroyed = true;

  // Try to unregister listener if we're still registered.
  RecvUnregisterListener();
}

mozilla::ipc::IPCResult TelephonyParent::RecvPTelephonyRequestConstructor(
    PTelephonyRequestParent* aActor, const IPCTelephonyRequest& aRequest) {
  TelephonyRequestParent* actor = static_cast<TelephonyRequestParent*>(aActor);
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);

  if (!service) {
    return NS_SUCCEEDED(
               actor->GetCallback()->NotifyError(u"InvalidStateError"_ns))
               ? IPC_OK()
               : IPC_FAIL_NO_REASON(this);
  }

  switch (aRequest.type()) {
    case IPCTelephonyRequest::TEnumerateCallsRequest: {
      service->EnumerateCalls(actor);
      return IPC_OK();
    }

    case IPCTelephonyRequest::TDialRequest: {
      const DialRequest& request = aRequest.get_DialRequest();
      service->Dial(request.clientId(), request.number(), request.isEmergency(),
                    request.type(), request.rttMode(),
                    actor->GetDialCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSendUSSDRequest: {
      const SendUSSDRequest& request = aRequest.get_SendUSSDRequest();
      service->SendUSSD(request.clientId(), request.ussd(),
                        actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TCancelUSSDRequest: {
      const CancelUSSDRequest& request = aRequest.get_CancelUSSDRequest();
      service->CancelUSSD(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TConferenceCallRequest: {
      const ConferenceCallRequest& request =
          aRequest.get_ConferenceCallRequest();
      service->ConferenceCall(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSeparateCallRequest: {
      const SeparateCallRequest& request = aRequest.get_SeparateCallRequest();
      service->SeparateCall(request.clientId(), request.callIndex(),
                            actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::THangUpConferenceRequest: {
      const HangUpConferenceRequest& request =
          aRequest.get_HangUpConferenceRequest();
      service->HangUpConference(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::THoldConferenceRequest: {
      const HoldConferenceRequest& request =
          aRequest.get_HoldConferenceRequest();
      service->HoldConference(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TResumeConferenceRequest: {
      const ResumeConferenceRequest& request =
          aRequest.get_ResumeConferenceRequest();
      service->ResumeConference(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TAnswerCallRequest: {
      const AnswerCallRequest& request = aRequest.get_AnswerCallRequest();
      service->AnswerCall(request.clientId(), request.callIndex(),
                          request.type(), request.rttMode(),
                          actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::THangUpAllCallsRequest: {
      const HangUpAllCallsRequest& request =
          aRequest.get_HangUpAllCallsRequest();
      service->HangUpAllCalls(request.clientId(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::THangUpCallRequest: {
      const HangUpCallRequest& request = aRequest.get_HangUpCallRequest();
      service->HangUpCall(request.clientId(), request.callIndex(),
                          request.reason(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TRejectCallRequest: {
      const RejectCallRequest& request = aRequest.get_RejectCallRequest();
      service->RejectCall(request.clientId(), request.callIndex(),
                          request.reason(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::THoldCallRequest: {
      const HoldCallRequest& request = aRequest.get_HoldCallRequest();
      service->HoldCall(request.clientId(), request.callIndex(),
                        actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TResumeCallRequest: {
      const ResumeCallRequest& request = aRequest.get_ResumeCallRequest();
      service->ResumeCall(request.clientId(), request.callIndex(),
                          actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSendTonesRequest: {
      const SendTonesRequest& request = aRequest.get_SendTonesRequest();
      service->SendTones(request.clientId(), request.dtmfChars(),
                         request.pauseDuration(), request.toneDuration(),
                         actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSendRttModifyRequest: {
      const SendRttModifyRequest& request = aRequest.get_SendRttModifyRequest();
      service->SendRttModify(request.clientId(), request.callIndex(),
                             request.rttMode(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSendRttModifyResponseRequest: {
      const SendRttModifyResponseRequest& request =
          aRequest.get_SendRttModifyResponseRequest();
      service->SendRttModifyResponse(request.clientId(), request.callIndex(),
                                     request.status(), actor->GetCallback());
      return IPC_OK();
    }

    case IPCTelephonyRequest::TSendRttMessageRequest: {
      const SendRttMessageRequest& request =
          aRequest.get_SendRttMessageRequest();
      service->SendRttMessage(request.clientId(), request.callIndex(),
                              request.message(), actor->GetCallback());
      return IPC_OK();
    }

    default:
      MOZ_CRASH("Unknown type!");
  }

  return IPC_FAIL_NO_REASON(this);
}

PTelephonyRequestParent* TelephonyParent::AllocPTelephonyRequestParent(
    const IPCTelephonyRequest& aRequest) {
  TelephonyRequestParent* actor = new TelephonyRequestParent();
  // Add an extra ref for IPDL. Will be released in
  // TelephonyParent::DeallocPTelephonyRequestParent().
  NS_ADDREF(actor);

  return actor;
}

bool TelephonyParent::DeallocPTelephonyRequestParent(
    PTelephonyRequestParent* aActor) {
  // TelephonyRequestParent is refcounted, must not be freed manually.
  static_cast<TelephonyRequestParent*>(aActor)->Release();
  return true;
}
/*
#ifdef MOZ_WIDGET_GONK
PVideoCallProviderParent*
TelephonyParent::AllocPVideoCallProviderParent(const uint32_t& aClientId,
                                               const uint32_t& aCallindex)
{
  LOG("AllocPVideoCallProviderParent");
  RefPtr<VideoCallProviderParent> parent = new
VideoCallProviderParent(aClientId, aCallindex); parent->AddRef();

  return parent;
}

bool
TelephonyParent::DeallocPVideoCallProviderParent(PVideoCallProviderParent*
aActor)
{
  LOG("DeallocPVideoCallProviderParent");
  static_cast<VideoCallProviderParent*>(aActor)->Release();
  return true;
}
#endif
*/
mozilla::ipc::IPCResult TelephonyParent::Recv__delete__() {
  return IPC_OK();  // Unregister listener in TelephonyParent::ActorDestroy().
}

mozilla::ipc::IPCResult TelephonyParent::RecvRegisterListener() {
  NS_ENSURE_TRUE(!mRegistered, IPC_OK());

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  mRegistered = NS_SUCCEEDED(service->RegisterListener(this));
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvUnregisterListener() {
  NS_ENSURE_TRUE(mRegistered, IPC_OK());

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  mRegistered = !NS_SUCCEEDED(service->UnregisterListener(this));
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvStartTone(
    const uint32_t& aClientId, const nsString& aTone) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->StartTone(aClientId, aTone);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvStopTone(
    const uint32_t& aClientId) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->StopTone(aClientId);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvGetHacMode(bool* aEnabled) {
  *aEnabled = false;

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->GetHacMode(aEnabled);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvSetHacMode(const bool& aEnabled) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->SetHacMode(aEnabled);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvGetMicrophoneMuted(bool* aMuted) {
  *aMuted = false;

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->GetMicrophoneMuted(aMuted);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvSetMicrophoneMuted(
    const bool& aMuted) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->SetMicrophoneMuted(aMuted);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvGetSpeakerEnabled(bool* aEnabled) {
  *aEnabled = false;

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->GetSpeakerEnabled(aEnabled);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvSetSpeakerEnabled(
    const bool& aEnabled) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->SetSpeakerEnabled(aEnabled);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvGetTtyMode(uint16_t* aMode) {
  *aMode = nsITelephonyService::TTY_MODE_OFF;

  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->GetTtyMode(aMode);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyParent::RecvSetTtyMode(const uint16_t& aMode) {
  nsCOMPtr<nsITelephonyService> service =
      do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, IPC_OK());

  service->SetTtyMode(aMode);
  return IPC_OK();
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyParent::CallStateChanged(uint32_t aLength,
                                  nsITelephonyCallInfo** aAllInfo) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  nsTArray<nsITelephonyCallInfo*> allInfo;
  for (uint32_t i = 0; i < aLength; i++) {
    allInfo.AppendElement(aAllInfo[i]);
  }

  return SendNotifyCallStateChanged(allInfo) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallStateComplete() {
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallState(nsITelephonyCallInfo* aInfo) {
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::NotifyCdmaCallWaiting(uint32_t aClientId,
                                       const nsAString& aNumber,
                                       uint16_t aNumberPresentation,
                                       const nsAString& aName,
                                       uint16_t aNamePresentation) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCdmaWaitingCallData data(nsString(aNumber), aNumberPresentation,
                              nsString(aName), aNamePresentation);
  return SendNotifyCdmaCallWaiting(aClientId, data) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyConferenceError(const nsAString& aName,
                                       const nsAString& aMessage) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyConferenceError(nsString(aName), nsString(aMessage))
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyRingbackTone(bool aPlayRingbackTone) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyRingbackTone(aPlayRingbackTone) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyTtyModeReceived(uint16_t mode) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyTtyModeReceived(mode) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyTelephonyCoverageLosing(uint16_t aType) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyTelephonyCoverageLosing(aType) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::SupplementaryServiceNotification(uint32_t aClientId,
                                                  int32_t aNotificationType,
                                                  int32_t aCode, int32_t aIndex,
                                                  int32_t aType,
                                                  const nsAString& aNumber) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifySupplementaryService(aClientId, aNotificationType, aCode,
                                        aIndex, aType, nsString(aNumber))
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyRttModifyRequestReceived(uint32_t aClientId,
                                                int32_t aCallIndex,
                                                uint16_t aRttMode) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);
  return SendNotifyRttModifyRequestReceived(aClientId, aCallIndex, aRttMode)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyRttModifyResponseReceived(uint32_t aClientId,
                                                 int32_t aCallIndex,
                                                 uint16_t aStatus) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);
  return SendNotifyRttModifyResponseReceived(aClientId, aCallIndex, aStatus)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyRttMessageReceived(uint32_t aClientId,
                                          int32_t aCallIndex,
                                          const nsAString& aMessage) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);
  return SendNotifyRttMessageReceived(aClientId, aCallIndex, nsString(aMessage))
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifySrvccState(uint32_t aClientId, int32_t aState) {
  return NS_OK;
}

/*******************************************************************************
 * TelephonyRequestParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyRequestParent, nsITelephonyListener)

TelephonyRequestParent::TelephonyRequestParent()
    : mActorDestroyed(false),
      mCallback(new Callback(this)),
      mDialCallback(new DialCallback(this)) {}

void TelephonyRequestParent::ActorDestroy(ActorDestroyReason why) {
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mActorDestroyed is set to true. Return
  // an error here to avoid sending a message to the dead process.
  mActorDestroyed = true;
}

nsresult TelephonyRequestParent::SendResponse(
    const IPCTelephonyResponse& aResponse) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, aResponse) ? NS_OK : NS_ERROR_FAILURE;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyRequestParent::CallStateChanged(uint32_t aLength,
                                         nsITelephonyCallInfo** aAllInfo) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallStateComplete() {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, EnumerateCallsResponse()) ? NS_OK
                                                        : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallState(nsITelephonyCallInfo* aInfo) {
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyEnumerateCallState(aInfo) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyCdmaCallWaiting(uint32_t aClientId,
                                              const nsAString& aNumber,
                                              uint16_t aNumberPresentation,
                                              const nsAString& aName,
                                              uint16_t aNamePresentation) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyConferenceError(const nsAString& aName,
                                              const nsAString& aMessage) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyRingbackTone(bool aPlayRingbackTone) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyTtyModeReceived(uint16_t aMode) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyTelephonyCoverageLosing(uint16_t aType) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::SupplementaryServiceNotification(
    uint32_t aClientId, int32_t aNotificationType, int32_t aCode,
    int32_t aIndex, int32_t aType, const nsAString& aNumber) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyRttModifyRequestReceived(uint32_t aClientId,
                                                       int32_t aCallIndex,
                                                       uint16_t aRttMode) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyRttModifyResponseReceived(uint32_t aClientId,
                                                        int32_t aCallIndex,
                                                        uint16_t aStatus) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyRttMessageReceived(uint32_t aClientId,
                                                 int32_t aCallIndex,
                                                 const nsAString& aMessage) {
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifySrvccState(uint32_t aClientId, int32_t aState) {
  MOZ_CRASH("Not a TelephonyParent!");
}

/*******************************************************************************
 * TelephonyRequestParent::Callback
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyRequestParent::Callback, nsITelephonyCallback)

nsresult TelephonyRequestParent::Callback::SendResponse(
    const IPCTelephonyResponse& aResponse) {
  if (mParent &&
      !mParent->mActorDestroyed) {  // ASYNC operation feedback from ril message
    return mParent->SendResponse(aResponse);
  } else {
    LOG("TelephonyRequestParent::Callback::SendResponse return with child "
        "process already exit");
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
TelephonyRequestParent::Callback::NotifySuccess() {
  return SendResponse(SuccessResponse());
}

NS_IMETHODIMP
TelephonyRequestParent::Callback::NotifyError(const nsAString& aError) {
  return SendResponse(ErrorResponse(nsAutoString(aError)));
}

/*******************************************************************************
 * TelephonyRequestParent::DialCallback
 ******************************************************************************/

NS_IMPL_ISUPPORTS_INHERITED(TelephonyRequestParent::DialCallback,
                            TelephonyRequestParent::Callback,
                            nsITelephonyDialCallback)

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMI(
    const nsAString& aServiceCode) {
  if (mParent && !mParent->mActorDestroyed) {
    return mParent->SendNotifyDialMMI(nsAutoString(aServiceCode))
               ? NS_OK
               : NS_ERROR_FAILURE;
  } else {
    // ASYNC operation feedback from ril message
    LOG("DialCallback::NotifyDialMMI return with child process already exit");
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialCallSuccess(
    uint32_t aClientId, uint32_t aCallIndex, const nsAString& aNumber,
    bool aIsEmergency, uint16_t aRttMode, uint16_t aVoiceQuality,
    uint16_t aVideoCallState, uint32_t aCapabilities, uint16_t aRadioTech) {
  return SendResponse(DialResponseCallSuccess(
      aClientId, aCallIndex, nsAutoString(aNumber), aIsEmergency, aRttMode,
      aVoiceQuality, aVideoCallState, aCapabilities, aRadioTech));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccess(
    const nsAString& aStatusMessage) {
  return SendResponse(DialResponseMMISuccess(
      nsAutoString(aStatusMessage), AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccessWithInteger(
    const nsAString& aStatusMessage, uint16_t aAdditionalInformation) {
  return SendResponse(
      DialResponseMMISuccess(nsAutoString(aStatusMessage),
                             AdditionalInformation(aAdditionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccessWithStrings(
    const nsAString& aStatusMessage, uint32_t aCount,
    const char16_t** aAdditionalInformation) {
  nsTArray<nsString> additionalInformation;
  nsString* infos = additionalInformation.AppendElements(aCount);
  for (uint32_t i = 0; i < aCount; i++) {
    infos[i].Rebind(aAdditionalInformation[i],
                    nsCharTraits<char16_t>::length(aAdditionalInformation[i]));
  }

  return SendResponse(
      DialResponseMMISuccess(nsAutoString(aStatusMessage),
                             AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::
    NotifyDialMMISuccessWithCallForwardingOptions(
        const nsAString& aStatusMessage, uint32_t aCount,
        nsIMobileCallForwardingOptions** aAdditionalInformation) {
  nsTArray<nsIMobileCallForwardingOptions*> additionalInformation;
  for (uint32_t i = 0; i < aCount; i++) {
    additionalInformation.AppendElement(aAdditionalInformation[i]);
  }

  return SendResponse(
      DialResponseMMISuccess(nsAutoString(aStatusMessage),
                             AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMIError(
    const nsAString& aError) {
  return SendResponse(DialResponseMMIError(
      nsAutoString(aError), AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMIErrorWithInfo(
    const nsAString& aError, uint16_t aInfo) {
  return SendResponse(
      DialResponseMMIError(nsAutoString(aError), AdditionalInformation(aInfo)));
}
