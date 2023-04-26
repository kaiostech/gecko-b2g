/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/icc/IccIPCUtils.h"
#include "mozilla/dom/icc/IccParent.h"
#include "nsIIccService.h"
#include "nsIStkCmdFactory.h"
#include "nsIStkProactiveCmd.h"

namespace mozilla {
namespace dom {
namespace icc {

/**
 * PIccParent Implementation.
 */

IccParent::IccParent(uint32_t aServiceId) {
  nsCOMPtr<nsIIccService> service = do_GetService(ICC_SERVICE_CONTRACTID);

  NS_ASSERTION(service, "Failed to get IccService!");

  service->GetIccByServiceId(aServiceId, getter_AddRefs(mIcc));

  NS_ASSERTION(mIcc, "Failed to get Icc with specified serviceId.");

  mIcc->RegisterListener(this);
}

void IccParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (mIcc) {
    mIcc->UnregisterListener(this);
    mIcc = nullptr;
  }
}

mozilla::ipc::IPCResult IccParent::RecvInit(OptionalIccInfoData* aInfoData,
                                            uint32_t* aCardState,
                                            uint32_t* aPin2CardState) {
  NS_ENSURE_TRUE(mIcc, IPC_FAIL_NO_REASON(this));

  nsresult rv = mIcc->GetCardState(aCardState);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));
  rv = mIcc->GetPin2CardState(aPin2CardState);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIIccInfo> iccInfo;
  rv = mIcc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  if (iccInfo) {
    IccInfoData data;
    IccIPCUtils::GetIccInfoDataFromIccInfo(iccInfo, data);
    *aInfoData = OptionalIccInfoData(data);

    return IPC_OK();
  }

  *aInfoData = OptionalIccInfoData(void_t());

  return IPC_OK();
}

mozilla::ipc::IPCResult IccParent::RecvStkResponse(const nsString& aCmd,
                                                   const nsString& aResponse) {
  NS_ENSURE_TRUE(mIcc, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
      do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cmdFactory, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIStkProactiveCmd> cmd;
  cmdFactory->InflateCommand(aCmd, getter_AddRefs(cmd));
  NS_ENSURE_TRUE(cmd, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIStkTerminalResponse> response;
  cmdFactory->InflateResponse(aResponse, getter_AddRefs(response));
  NS_ENSURE_TRUE(response, IPC_FAIL_NO_REASON(this));

  nsresult rv = mIcc->SendStkResponse(cmd, response);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  return IPC_OK();
}

mozilla::ipc::IPCResult IccParent::RecvStkMenuSelection(
    const uint16_t& aItemIdentifier, const bool& aHelpRequested) {
  NS_ENSURE_TRUE(mIcc, IPC_FAIL_NO_REASON(this));

  nsresult rv = mIcc->SendStkMenuSelection(aItemIdentifier, aHelpRequested);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  return IPC_OK();
}

mozilla::ipc::IPCResult IccParent::RecvStkTimerExpiration(
    const uint16_t& aTimerId, const uint32_t& aTimerValue) {
  NS_ENSURE_TRUE(mIcc, IPC_FAIL_NO_REASON(this));

  nsresult rv = mIcc->SendStkTimerExpiration(aTimerId, aTimerValue);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  return IPC_OK();
}

mozilla::ipc::IPCResult IccParent::RecvStkEventDownload(
    const nsString& aEvent) {
  NS_ENSURE_TRUE(mIcc, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
      do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cmdFactory, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIStkDownloadEvent> event;
  cmdFactory->InflateDownloadEvent(aEvent, getter_AddRefs(event));
  NS_ENSURE_TRUE(event, IPC_FAIL_NO_REASON(this));

  nsresult rv = mIcc->SendStkEventDownload(event);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  return IPC_OK();
}

PIccRequestParent* IccParent::AllocPIccRequestParent(
    const IccRequest& aRequest) {
  NS_ASSERTION(mIcc, "AllocPIccRequestParent after actor was destroyed!");

  IccRequestParent* actor = new IccRequestParent(mIcc);
  // Add an extra ref for IPDL. Will be released in
  // IccParent::DeallocPIccRequestParent().
  actor->AddRef();
  return actor;
}

bool IccParent::DeallocPIccRequestParent(PIccRequestParent* aActor) {
  // IccRequestParent is refcounted, must not be freed manually.
  static_cast<IccRequestParent*>(aActor)->Release();
  return true;
}

mozilla::ipc::IPCResult IccParent::RecvPIccRequestConstructor(
    PIccRequestParent* aActor, const IccRequest& aRequest) {
  NS_ASSERTION(mIcc, "RecvPIccRequestConstructor after actor was destroyed!");

  IccRequestParent* actor = static_cast<IccRequestParent*>(aActor);

  switch (aRequest.type()) {
    case IccRequest::TGetCardLockEnabledRequest:
      return actor->DoRequest(aRequest.get_GetCardLockEnabledRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TUnlockCardLockRequest:
      return actor->DoRequest(aRequest.get_UnlockCardLockRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TSetCardLockEnabledRequest:
      return actor->DoRequest(aRequest.get_SetCardLockEnabledRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TChangeCardLockPasswordRequest:
      return actor->DoRequest(aRequest.get_ChangeCardLockPasswordRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TMatchMvnoRequest:
      return actor->DoRequest(aRequest.get_MatchMvnoRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TGetServiceStateEnabledRequest:
      return actor->DoRequest(aRequest.get_GetServiceStateEnabledRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TReadContactsRequest:
      return actor->DoRequest(aRequest.get_ReadContactsRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TUpdateContactRequest:
      return actor->DoRequest(aRequest.get_UpdateContactRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TGetIccAuthenticationRequest:
      return actor->DoRequest(aRequest.get_GetIccAuthenticationRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TIccOpenChannelRequest:
      return actor->DoRequest(aRequest.get_IccOpenChannelRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TIccCloseChannelRequest:
      return actor->DoRequest(aRequest.get_IccCloseChannelRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case IccRequest::TIccExchangeAPDURequest:
      return actor->DoRequest(aRequest.get_IccExchangeAPDURequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Received invalid request type!");
  }

  return IPC_OK();
}

/**
 * nsIIccListener Implementation.
 */

NS_IMPL_ISUPPORTS(IccParent, nsIIccListener)

NS_IMETHODIMP
IccParent::NotifyStkCommand(nsIStkProactiveCmd* aStkProactiveCmd) {
  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
      do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cmdFactory, NS_ERROR_UNEXPECTED);

  nsAutoString cmd;
  nsresult rv = cmdFactory->DeflateCommand(aStkProactiveCmd, cmd);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyStkCommand(cmd) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccParent::NotifyStkSessionEnd() {
  return SendNotifyStkSessionEnd() ? NS_OK : NS_ERROR_FAILURE;
  ;
}

NS_IMETHODIMP
IccParent::NotifyCardStateChanged() {
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  uint32_t cardState;
  uint32_t pin2CardState;
  nsresult rv = mIcc->GetCardState(&cardState);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mIcc->GetPin2CardState(&pin2CardState);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyCardStateChanged(cardState, pin2CardState)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccParent::NotifyIccInfoChanged() {
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIIccInfo> iccInfo;
  nsresult rv = mIcc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!iccInfo) {
    return SendNotifyIccInfoChanged(OptionalIccInfoData(void_t()))
               ? NS_OK
               : NS_ERROR_FAILURE;
  }

  IccInfoData data;
  IccIPCUtils::GetIccInfoDataFromIccInfo(iccInfo, data);

  return SendNotifyIccInfoChanged(OptionalIccInfoData(data)) ? NS_OK
                                                             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccParent::NotifyIsimInfoChanged() { return NS_OK; }
/**
 * PIccRequestParent Implementation.
 */

IccRequestParent::IccRequestParent(nsIIcc* aIcc)
    : mIcc(aIcc), mChannelCallback(new ChannelCallback(*this)) {}

void IccRequestParent::ActorDestroy(ActorDestroyReason aWhy) { mIcc = nullptr; }

bool IccRequestParent::DoRequest(const GetCardLockEnabledRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->GetCardLockEnabled(aRequest.lockType(), this));
}

bool IccRequestParent::DoRequest(const UnlockCardLockRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->UnlockCardLock(
      aRequest.lockType(), aRequest.password(), aRequest.newPin(), this));
}

bool IccRequestParent::DoRequest(const SetCardLockEnabledRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->SetCardLockEnabled(
      aRequest.lockType(), aRequest.password(), aRequest.enabled(), this));
}

bool IccRequestParent::DoRequest(
    const ChangeCardLockPasswordRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->ChangeCardLockPassword(
      aRequest.lockType(), aRequest.password(), aRequest.newPassword(), this));
}

bool IccRequestParent::DoRequest(const MatchMvnoRequest& aRequest) {
  return NS_SUCCEEDED(
      mIcc->MatchMvno(aRequest.mvnoType(), aRequest.mvnoData(), this));
}

bool IccRequestParent::DoRequest(
    const GetServiceStateEnabledRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->GetServiceStateEnabled(aRequest.service(), this));
}

bool IccRequestParent::DoRequest(const ReadContactsRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->ReadContacts(aRequest.contactType(), this));
}

bool IccRequestParent::DoRequest(const UpdateContactRequest& aRequest) {
  nsCOMPtr<nsIIccContact> contact;
  nsresult rv = nsIccContact::Create(
      aRequest.contact().id(), aRequest.contact().names(),
      aRequest.contact().numbers(), aRequest.contact().emails(),
      getter_AddRefs(contact));
  NS_ENSURE_SUCCESS(rv, false);

  return NS_SUCCEEDED(mIcc->UpdateContact(aRequest.contactType(), contact,
                                          aRequest.pin2(), this));
}

bool IccRequestParent::DoRequest(const GetIccAuthenticationRequest& aRequest) {
  return NS_SUCCEEDED(mIcc->GetIccAuthentication(
      aRequest.appType(), aRequest.authType(), aRequest.data(), this));
}

bool IccRequestParent::DoRequest(const IccOpenChannelRequest& aRequest) {
  return NS_SUCCEEDED(
      mIcc->IccOpenChannel(aRequest.aid(), this->GetChannelCallback()));
}

bool IccRequestParent::DoRequest(const IccCloseChannelRequest& aRequest) {
  return NS_SUCCEEDED(
      mIcc->IccCloseChannel(aRequest.channel(), this->GetChannelCallback()));
}

bool IccRequestParent::DoRequest(const IccExchangeAPDURequest& aRequest) {
  return NS_SUCCEEDED(
      mIcc->IccExchangeAPDU(aRequest.channel(), aRequest.cla(), aRequest.ins(),
                            aRequest.p1(), aRequest.p2(), aRequest.p3(),
                            aRequest.data(), this->GetChannelCallback()));
}

nsresult IccRequestParent::SendReply(const IccReply& aReply) {
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

/**
 * nsIIccCallback Implementation.
 */

NS_IMPL_ISUPPORTS(IccRequestParent, nsIIccCallback)

NS_IMETHODIMP
IccRequestParent::NotifySuccess() { return SendReply(IccReplySuccess()); }

NS_IMETHODIMP
IccRequestParent::NotifySuccessWithBoolean(bool aResult) {
  return SendReply(IccReplySuccessWithBoolean(aResult));
}

NS_IMETHODIMP
IccRequestParent::NotifyError(const nsAString& aErrorMsg) {
  return SendReply(IccReplyError(nsAutoString(aErrorMsg)));
}

NS_IMETHODIMP
IccRequestParent::NotifyCardLockError(const nsAString& aErrorMsg,
                                      int32_t aRetryCount) {
  return SendReply(IccReplyCardLockError(aRetryCount, nsAutoString(aErrorMsg)));
}

NS_IMETHODIMP
IccRequestParent::NotifyRetrievedIccContacts(nsIIccContact** aContacts,
                                             unsigned int aCount) {
  nsTArray<IccContactData> contacts;

  for (uint32_t i = 0; i < aCount; i++) {
    MOZ_ASSERT(aContacts[i]);

    IccContactData contactData;

    IccIPCUtils::GetIccContactDataFromIccContact(aContacts[i], contactData);
    contacts.AppendElement(contactData);
  }

  return SendReply(IccReplyReadContacts(contacts));
}

NS_IMETHODIMP
IccRequestParent::NotifyUpdatedIccContact(nsIIccContact* aContact) {
  MOZ_ASSERT(aContact);

  IccContactData contactData;

  IccIPCUtils::GetIccContactDataFromIccContact(aContact, contactData);

  return SendReply(IccReplyUpdateContact(contactData));
}

NS_IMETHODIMP
IccRequestParent::NotifyAuthResponse(const nsAString& aData) {
  return SendReply(IccReplyAuthResponse(nsAutoString(aData)));
}

/**
 * nsIIccChannelCallback Implementation.
 */

NS_IMPL_ISUPPORTS(IccRequestParent::ChannelCallback, nsIIccChannelCallback)

nsresult IccRequestParent::ChannelCallback::SendReply(const IccReply& aReply) {
  return mParent.SendReply(aReply);
}

NS_IMETHODIMP
IccRequestParent::ChannelCallback::NotifyOpenChannelSuccess(int32_t aChannel) {
  return SendReply(IccReplyOpenChannel(aChannel));
}

NS_IMETHODIMP
IccRequestParent::ChannelCallback::NotifyCloseChannelSuccess() {
  return SendReply(IccReplyCloseChannel());
}

NS_IMETHODIMP
IccRequestParent::ChannelCallback::NotifyExchangeAPDUResponse(
    uint8_t aSw1, uint8_t aSw2, const nsAString& aData) {
  return SendReply(IccReplyExchangeAPDU(aSw1, aSw2, nsAutoString(aData)));
}

NS_IMETHODIMP
IccRequestParent::ChannelCallback::NotifyError(const nsAString& aError) {
  return SendReply(IccReplyChannelError(nsAutoString(aError)));
}

}  // namespace icc
}  // namespace dom
}  // namespace mozilla
