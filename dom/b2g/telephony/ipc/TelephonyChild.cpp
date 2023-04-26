/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyChild.h"

#include "mozilla/dom/telephony/TelephonyDialCallback.h"
#ifdef MOZ_WIDGET_GONK
//#include "mozilla/dom/videocallprovider/VideoCallProviderChild.h"
#endif
#include "mozilla/UniquePtr.h"
#include "TelephonyIPCService.h"

USING_TELEPHONY_NAMESPACE
USING_VIDEOCALLPROVIDER_NAMESPACE

/*******************************************************************************
 * TelephonyChild
 ******************************************************************************/

TelephonyChild::TelephonyChild(TelephonyIPCService* aService)
    : mService(aService) {
  MOZ_ASSERT(aService);
}

TelephonyChild::~TelephonyChild() {}

void TelephonyChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mService) {
    mService->NoteActorDestroyed();
    mService = nullptr;
  }
}

PTelephonyRequestChild* TelephonyChild::AllocPTelephonyRequestChild(
    const IPCTelephonyRequest& aRequest) {
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool TelephonyChild::DeallocPTelephonyRequestChild(
    PTelephonyRequestChild* aActor) {
  delete aActor;
  return true;
}
/*
#ifdef MOZ_WIDGET_GONK
PVideoCallProviderChild*
TelephonyChild::AllocPVideoCallProviderChild(const uint32_t& clientId, const
uint32_t& callIndex)
{
  NS_NOTREACHED("No one should be allocating PVideoCallProviderChild actors");
  return nullptr;
}

bool
TelephonyChild::DeallocPVideoCallProviderChild(PVideoCallProviderChild* aActor)
{
  // To release a child.
  delete aActor;
  return true;
}
#endif
*/
mozilla::ipc::IPCResult TelephonyChild::RecvNotifyCallStateChanged(
    nsTArray<nsITelephonyCallInfo*>&& aAllInfo) {
  uint32_t length = aAllInfo.Length();
  nsTArray<nsCOMPtr<nsITelephonyCallInfo>> results;
  for (uint32_t i = 0; i < length; ++i) {
    // Use dont_AddRef here because this instance has already been AddRef-ed in
    // TelephonyIPCSerializer.h
    nsCOMPtr<nsITelephonyCallInfo> info = dont_AddRef(aAllInfo[i]);
    results.AppendElement(info);
  }

  MOZ_ASSERT(mService);

  mService->CallStateChanged(
      length, const_cast<nsITelephonyCallInfo**>(aAllInfo.Elements()));

  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyCdmaCallWaiting(
    const uint32_t& aClientId, const IPCCdmaWaitingCallData& aData) {
  MOZ_ASSERT(mService);

  mService->NotifyCdmaCallWaiting(aClientId, aData.number(),
                                  aData.numberPresentation(), aData.name(),
                                  aData.namePresentation());
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyConferenceError(
    const nsString& aName, const nsString& aMessage) {
  MOZ_ASSERT(mService);

  mService->NotifyConferenceError(aName, aMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyRingbackTone(
    const bool& aPlayRingbackTone) {
  MOZ_ASSERT(mService);

  mService->NotifyRingbackTone(aPlayRingbackTone);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyTtyModeReceived(
    const uint16_t& aMode) {
  MOZ_ASSERT(mService);

  mService->NotifyTtyModeReceived(aMode);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyTelephonyCoverageLosing(
    const uint16_t& aType) {
  MOZ_ASSERT(mService);

  mService->NotifyTelephonyCoverageLosing(aType);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifySupplementaryService(
    const uint32_t& aClientId, const int32_t& aNotificationType,
    const int32_t& aCode, const int32_t& aIndex, const int32_t& aType,
    const nsString& aNumber) {
  MOZ_ASSERT(mService);

  mService->SupplementaryServiceNotification(aClientId, aNotificationType,
                                             aCode, aIndex, aType, aNumber);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyRttModifyRequestReceived(
    const uint32_t& aClientId, const int32_t& aCallIndex,
    const uint16_t& aRttMode) {
  MOZ_ASSERT(mService);

  mService->NotifyRttModifyRequestReceived(aClientId, aCallIndex, aRttMode);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyRttModifyResponseReceived(
    const uint32_t& aClientId, const int32_t& aCallIndex,
    const uint16_t& aStatus) {
  MOZ_ASSERT(mService);

  mService->NotifyRttModifyResponseReceived(aClientId, aCallIndex, aStatus);
  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyChild::RecvNotifyRttMessageReceived(
    const uint32_t& aClientId, const int32_t& aCallIndex,
    const nsString& aMessage) {
  MOZ_ASSERT(mService);

  mService->NotifyRttMessageReceived(aClientId, aCallIndex, aMessage);
  return IPC_OK();
}

/*******************************************************************************
 * TelephonyRequestChild
 ******************************************************************************/

TelephonyRequestChild::TelephonyRequestChild(nsITelephonyListener* aListener,
                                             nsITelephonyCallback* aCallback) {
  mListener = aListener;
  nsISupports* temp = aCallback;
  mCallback = do_QueryInterface(temp);
}

void TelephonyRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  mListener = nullptr;
  mCallback = nullptr;
}

mozilla::ipc::IPCResult TelephonyRequestChild::Recv__delete__(
    const IPCTelephonyResponse& aResponse) {
  switch (aResponse.type()) {
    case IPCTelephonyResponse::TEnumerateCallsResponse:
      mListener->EnumerateCallStateComplete();
      break;
    case IPCTelephonyResponse::TSuccessResponse:
      return DoResponse(aResponse.get_SuccessResponse())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IPCTelephonyResponse::TErrorResponse:
      return DoResponse(aResponse.get_ErrorResponse())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IPCTelephonyResponse::TDialResponseCallSuccess:
      return DoResponse(aResponse.get_DialResponseCallSuccess())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IPCTelephonyResponse::TDialResponseMMISuccess:
      return DoResponse(aResponse.get_DialResponseMMISuccess())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IPCTelephonyResponse::TDialResponseMMIError:
      return DoResponse(aResponse.get_DialResponseMMIError())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Unknown type!");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyRequestChild::RecvNotifyEnumerateCallState(
    nsITelephonyCallInfo* const& aInfo) {
  // Use dont_AddRef here because this instances has already been AddRef-ed in
  // TelephonyIPCSerializer.h
  nsCOMPtr<nsITelephonyCallInfo> info = dont_AddRef(aInfo);

  MOZ_ASSERT(mListener);

  mListener->EnumerateCallState(aInfo);

  return IPC_OK();
}

mozilla::ipc::IPCResult TelephonyRequestChild::RecvNotifyDialMMI(
    const nsString& aServiceCode) {
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);
  callback->NotifyDialMMI(aServiceCode);
  return IPC_OK();
}

bool TelephonyRequestChild::DoResponse(const SuccessResponse& aResponse) {
  MOZ_ASSERT(mCallback);
  mCallback->NotifySuccess();
  return true;
}

bool TelephonyRequestChild::DoResponse(const ErrorResponse& aResponse) {
  MOZ_ASSERT(mCallback);
  mCallback->NotifyError(aResponse.name());
  return true;
}

bool TelephonyRequestChild::DoResponse(
    const DialResponseCallSuccess& aResponse) {
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);
  callback->NotifyDialCallSuccess(
      aResponse.clientId(), aResponse.callIndex(), aResponse.number(),
      aResponse.isEmergency(), aResponse.rttMode(), aResponse.voiceQuality(),
      aResponse.videoCallState(), aResponse.capabilities(),
      aResponse.radioTech());
  return true;
}

bool TelephonyRequestChild::DoResponse(
    const DialResponseMMISuccess& aResponse) {
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);

  nsAutoString statusMessage(aResponse.statusMessage());
  AdditionalInformation info(aResponse.additionalInformation());

  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      callback->NotifyDialMMISuccess(statusMessage);
      break;
    case AdditionalInformation::Tuint16_t:
      callback->NotifyDialMMISuccessWithInteger(statusMessage,
                                                info.get_uint16_t());
      break;
    case AdditionalInformation::TArrayOfnsString: {
      uint32_t count = info.get_ArrayOfnsString().Length();
      const nsTArray<nsString>& additionalInformation =
          info.get_ArrayOfnsString();

      auto additionalInfoPtrs = MakeUnique<const char16_t*[]>(count);
      for (size_t i = 0; i < count; ++i) {
        additionalInfoPtrs[i] = additionalInformation[i].get();
      }

      callback->NotifyDialMMISuccessWithStrings(statusMessage, count,
                                                additionalInfoPtrs.get());
      break;
    }
    case AdditionalInformation::TArrayOfnsMobileCallForwardingOptions: {
      uint32_t count = info.get_ArrayOfnsMobileCallForwardingOptions().Length();

      nsTArray<nsCOMPtr<nsIMobileCallForwardingOptions>> results;
      for (uint32_t i = 0; i < count; i++) {
        // Use dont_AddRef here because these instances are already AddRef-ed in
        // MobileConnectionIPCSerializer.h
        nsCOMPtr<nsIMobileCallForwardingOptions> item =
            dont_AddRef(info.get_ArrayOfnsMobileCallForwardingOptions()[i]);
        results.AppendElement(item);
      }

      callback->NotifyDialMMISuccessWithCallForwardingOptions(
          statusMessage, count,
          const_cast<nsIMobileCallForwardingOptions**>(
              info.get_ArrayOfnsMobileCallForwardingOptions().Elements()));
      break;
    }
    default:
      MOZ_CRASH("Received invalid type!");
      break;
  }

  return true;
}

bool TelephonyRequestChild::DoResponse(const DialResponseMMIError& aResponse) {
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);

  nsAutoString name(aResponse.name());
  AdditionalInformation info(aResponse.additionalInformation());

  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      callback->NotifyDialMMIError(name);
      break;
    case AdditionalInformation::Tuint16_t:
      callback->NotifyDialMMIErrorWithInfo(name, info.get_uint16_t());
      break;
    default:
      MOZ_CRASH("Received invalid type!");
      break;
  }

  return true;
}
