/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothParent.h"

#include "mozilla/Unused.h"
#include "BluetoothRequestParent.h"
#include "BluetoothService.h"

using mozilla::ipc::IPCResult;

using mozilla::Unused;
USING_BLUETOOTH_NAMESPACE

/*******************************************************************************
 * BluetoothParent
 ******************************************************************************/

BluetoothParent::BluetoothParent() : mShutdownState(Running) {
  MOZ_COUNT_CTOR(BluetoothParent);
}

BluetoothParent::~BluetoothParent() {
  MOZ_COUNT_DTOR(BluetoothParent);
  MOZ_ASSERT(!mService);
  MOZ_ASSERT(mShutdownState == Dead);
}

void BluetoothParent::BeginShutdown() {
  // Only do something here if we haven't yet begun the shutdown sequence.
  if (mShutdownState == Running) {
    Unused << SendBeginShutdown();
    mShutdownState = SentBeginShutdown;
  }
}

bool BluetoothParent::InitWithService(BluetoothService* aService) {
  MOZ_ASSERT(aService);
  MOZ_ASSERT(!mService);

  if (!SendEnabled(aService->IsEnabled())) {
    return false;
  }

  mService = aService;
  return true;
}

void BluetoothParent::UnregisterAllSignalHandlers() {
  MOZ_ASSERT(mService);
  mService->UnregisterAllSignalHandlers(this);
}

void BluetoothParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (mService) {
    UnregisterAllSignalHandlers();
#ifdef DEBUG
    mService = nullptr;
#endif
  }

#ifdef DEBUG
  mShutdownState = Dead;
#endif
}

IPCResult BluetoothParent::RecvRegisterSignalHandler(const nsString& aNode) {
  MOZ_ASSERT(mService);
  mService->RegisterBluetoothSignalHandler(aNode, this);
  return IPC_OK();
}

IPCResult BluetoothParent::RecvUnregisterSignalHandler(const nsString& aNode) {
  MOZ_ASSERT(mService);
  mService->UnregisterBluetoothSignalHandler(aNode, this);
  return IPC_OK();
}

IPCResult BluetoothParent::RecvStopNotifying() {
  MOZ_ASSERT(mService);

  if (mShutdownState != Running && mShutdownState != SentBeginShutdown) {
    MOZ_ASSERT(false, "Bad state!");
    return IPC_FAIL(this, "Bad state!");
  }

  mShutdownState = ReceivedStopNotifying;

  UnregisterAllSignalHandlers();

  if (SendNotificationsStopped()) {
    mShutdownState = SentNotificationsStopped;
    return IPC_OK();
  }

  return IPC_FAIL(this, "Bad state!");
}

mozilla::ipc::IPCResult BluetoothParent::RecvPBluetoothRequestConstructor(
    PBluetoothRequestParent* aActor, const Request& aRequest) {
  BluetoothRequestParent* actor = static_cast<BluetoothRequestParent*>(aActor);

#ifdef DEBUG
  actor->mRequestType = aRequest.type();
#endif

  switch (aRequest.type()) {
    case Request::TGetAdaptersRequest:
      return actor->DoRequest(aRequest.get_GetAdaptersRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStartBluetoothRequest:
      return actor->DoRequest(aRequest.get_StartBluetoothRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStopBluetoothRequest:
      return actor->DoRequest(aRequest.get_StopBluetoothRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSetPropertyRequest:
      return actor->DoRequest(aRequest.get_SetPropertyRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStartDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StartDiscoveryRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStopDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StopDiscoveryRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStartLeScanRequest:
      return actor->DoRequest(aRequest.get_StartLeScanRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStopLeScanRequest:
      return actor->DoRequest(aRequest.get_StopLeScanRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStartAdvertisingRequest:
      return actor->DoRequest(aRequest.get_StartAdvertisingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStopAdvertisingRequest:
      return actor->DoRequest(aRequest.get_StopAdvertisingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TPairRequest:
      return actor->DoRequest(aRequest.get_PairRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TUnpairRequest:
      return actor->DoRequest(aRequest.get_UnpairRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TPairedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_PairedDevicePropertiesRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TConnectedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_ConnectedDevicePropertiesRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TFetchUuidsRequest:
      return actor->DoRequest(aRequest.get_FetchUuidsRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TPinReplyRequest:
      return actor->DoRequest(aRequest.get_PinReplyRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSspReplyRequest:
      return actor->DoRequest(aRequest.get_SspReplyRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TConnectRequest:
      return actor->DoRequest(aRequest.get_ConnectRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TDisconnectRequest:
      return actor->DoRequest(aRequest.get_DisconnectRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TAcceptConnectionRequest:
      return actor->DoRequest(aRequest.get_AcceptConnectionRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TRejectConnectionRequest:
      return actor->DoRequest(aRequest.get_RejectConnectionRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSendFileRequest:
      return actor->DoRequest(aRequest.get_SendFileRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TStopSendingFileRequest:
      return actor->DoRequest(aRequest.get_StopSendingFileRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TConfirmReceivingFileRequest:
      return actor->DoRequest(aRequest.get_ConfirmReceivingFileRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TDenyReceivingFileRequest:
      return actor->DoRequest(aRequest.get_DenyReceivingFileRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TConnectScoRequest:
      return actor->DoRequest(aRequest.get_ConnectScoRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TDisconnectScoRequest:
      return actor->DoRequest(aRequest.get_DisconnectScoRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TIsScoConnectedRequest:
      return actor->DoRequest(aRequest.get_IsScoConnectedRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSetObexPasswordRequest:
      return actor->DoRequest(aRequest.get_SetObexPasswordRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TRejectObexAuthRequest:
      return actor->DoRequest(aRequest.get_RejectObexAuthRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyTovCardPullingRequest:
      return actor->DoRequest(aRequest.get_ReplyTovCardPullingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToPhonebookPullingRequest:
      return actor->DoRequest(aRequest.get_ReplyToPhonebookPullingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyTovCardListingRequest:
      return actor->DoRequest(aRequest.get_ReplyTovCardListingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToFolderListingRequest:
      return actor->DoRequest(aRequest.get_ReplyToFolderListingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToMessagesListingRequest:
      return actor->DoRequest(aRequest.get_ReplyToMessagesListingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToGetMessageRequest:
      return actor->DoRequest(aRequest.get_ReplyToGetMessageRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToSetMessageStatusRequest:
      return actor->DoRequest(aRequest.get_ReplyToSetMessageStatusRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToSendMessageRequest:
      return actor->DoRequest(aRequest.get_ReplyToSendMessageRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TReplyToMessageUpdateRequest:
      return actor->DoRequest(aRequest.get_ReplyToMessageUpdateRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
#ifdef MOZ_B2G_RIL
    case Request::TAnswerWaitingCallRequest:
      return actor->DoRequest(aRequest.get_AnswerWaitingCallRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TIgnoreWaitingCallRequest:
      return actor->DoRequest(aRequest.get_IgnoreWaitingCallRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TToggleCallsRequest:
      return actor->DoRequest(aRequest.get_ToggleCallsRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
#endif
    case Request::TSendMetaDataRequest:
      return actor->DoRequest(aRequest.get_SendMetaDataRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSendPlayStatusRequest:
      return actor->DoRequest(aRequest.get_SendPlayStatusRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TSendMessageEventRequest:
      return actor->DoRequest(aRequest.get_SendMessageEventRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TConnectGattClientRequest:
      return actor->DoRequest(aRequest.get_ConnectGattClientRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TDisconnectGattClientRequest:
      return actor->DoRequest(aRequest.get_DisconnectGattClientRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TDiscoverGattServicesRequest:
      return actor->DoRequest(aRequest.get_DiscoverGattServicesRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientStartNotificationsRequest:
      return actor->DoRequest(
                 aRequest.get_GattClientStartNotificationsRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientStopNotificationsRequest:
      return actor->DoRequest(aRequest.get_GattClientStopNotificationsRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TUnregisterGattClientRequest:
      return actor->DoRequest(aRequest.get_UnregisterGattClientRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientReadRemoteRssiRequest:
      return actor->DoRequest(aRequest.get_GattClientReadRemoteRssiRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientReadCharacteristicValueRequest:
      return actor->DoRequest(
                 aRequest.get_GattClientReadCharacteristicValueRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientWriteCharacteristicValueRequest:
      return actor->DoRequest(
                 aRequest.get_GattClientWriteCharacteristicValueRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientReadDescriptorValueRequest:
      return actor->DoRequest(
                 aRequest.get_GattClientReadDescriptorValueRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattClientWriteDescriptorValueRequest:
      return actor->DoRequest(
                 aRequest.get_GattClientWriteDescriptorValueRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerRegisterRequest:
      return actor->DoRequest(aRequest.get_GattServerRegisterRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerConnectPeripheralRequest:
      return actor->DoRequest(aRequest.get_GattServerConnectPeripheralRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerDisconnectPeripheralRequest:
      return actor->DoRequest(
                 aRequest.get_GattServerDisconnectPeripheralRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TUnregisterGattServerRequest:
      return actor->DoRequest(aRequest.get_UnregisterGattServerRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerAddServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerAddServiceRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerRemoveServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerRemoveServiceRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerStopServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerStopServiceRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerSendResponseRequest:
      return actor->DoRequest(aRequest.get_GattServerSendResponseRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case Request::TGattServerSendIndicationRequest:
      return actor->DoRequest(aRequest.get_GattServerSendIndicationRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Unknown type!");
  }

  MOZ_CRASH("Should never get here!");
}

PBluetoothRequestParent* BluetoothParent::AllocPBluetoothRequestParent(
    const Request& aRequest) {
  MOZ_ASSERT(mService);
  return new BluetoothRequestParent(mService);
}

bool BluetoothParent::DeallocPBluetoothRequestParent(
    PBluetoothRequestParent* aActor) {
  delete aActor;
  return true;
}

void BluetoothParent::Notify(const BluetoothSignal& aSignal) {
  Unused << SendNotify(aSignal);
}
