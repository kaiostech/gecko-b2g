/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/MobileConnectionParent.h"

//#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/mobileconnection/MobileConnectionIPCSerializer.h"
#include "mozilla/dom/MobileConnectionBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIVariant.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

MobileConnectionParent::MobileConnectionParent(uint32_t aClientId)
    : mLive(true) {
  nsCOMPtr<nsIMobileConnectionService> service =
      do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ASSERTION(service, "This shouldn't fail!");

  nsresult rv =
      service->GetItemByServiceId(aClientId, getter_AddRefs(mMobileConnection));
  if (NS_SUCCEEDED(rv) && mMobileConnection) {
    mMobileConnection->RegisterListener(this);
  }
}

void MobileConnectionParent::ActorDestroy(ActorDestroyReason why) {
  mLive = false;
  if (mMobileConnection) {
    mMobileConnection->UnregisterListener(this);
    mMobileConnection = nullptr;
  }
}

mozilla::ipc::IPCResult
MobileConnectionParent::RecvPMobileConnectionRequestConstructor(
    PMobileConnectionRequestParent* aActor,
    const MobileConnectionRequest& aRequest) {
  MobileConnectionRequestParent* actor =
      static_cast<MobileConnectionRequestParent*>(aActor);

  switch (aRequest.type()) {
    case MobileConnectionRequest::TGetNetworksRequest:
      return actor->DoRequest(aRequest.get_GetNetworksRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSelectNetworkRequest:
      return actor->DoRequest(aRequest.get_SelectNetworkRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSelectNetworkAutoRequest:
      return actor->DoRequest(aRequest.get_SelectNetworkAutoRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetPreferredNetworkTypeRequest:
      return actor->DoRequest(aRequest.get_SetPreferredNetworkTypeRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetPreferredNetworkTypeRequest:
      return actor->DoRequest(aRequest.get_GetPreferredNetworkTypeRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetRoamingPreferenceRequest:
      return actor->DoRequest(aRequest.get_SetRoamingPreferenceRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetRoamingPreferenceRequest:
      return actor->DoRequest(aRequest.get_GetRoamingPreferenceRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetVoicePrivacyModeRequest:
      return actor->DoRequest(aRequest.get_SetVoicePrivacyModeRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetVoicePrivacyModeRequest:
      return actor->DoRequest(aRequest.get_GetVoicePrivacyModeRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetCallForwardingRequest:
      return actor->DoRequest(aRequest.get_SetCallForwardingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetCallForwardingRequest:
      return actor->DoRequest(aRequest.get_GetCallForwardingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetCallBarringRequest:
      return actor->DoRequest(aRequest.get_SetCallBarringRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetCallBarringRequest:
      return actor->DoRequest(aRequest.get_GetCallBarringRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TChangeCallBarringPasswordRequest:
      return actor->DoRequest(aRequest.get_ChangeCallBarringPasswordRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetCallWaitingRequest:
      return actor->DoRequest(aRequest.get_SetCallWaitingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetCallWaitingRequest:
      return actor->DoRequest(aRequest.get_GetCallWaitingRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetCallingLineIdRestrictionRequest:
      return actor->DoRequest(aRequest.get_SetCallingLineIdRestrictionRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetCallingLineIdRestrictionRequest:
      return actor->DoRequest(aRequest.get_GetCallingLineIdRestrictionRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TExitEmergencyCbModeRequest:
      return actor->DoRequest(aRequest.get_ExitEmergencyCbModeRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TSetRadioEnabledRequest:
      return actor->DoRequest(aRequest.get_SetRadioEnabledRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TGetDeviceIdentitiesRequest:
      return actor->DoRequest(aRequest.get_GetDeviceIdentitiesRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionRequest::TStopNetworkScanRequest:
      return actor->DoRequest(aRequest.get_StopNetworkScanRequest())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Received invalid request type!");
  }

  return IPC_OK();
}

PMobileConnectionRequestParent*
MobileConnectionParent::AllocPMobileConnectionRequestParent(
    const MobileConnectionRequest& request) {
  // if (!AssertAppProcessPermission(Manager(), "mobileconnection")) {
  //  return nullptr;
  //}

  MobileConnectionRequestParent* actor =
      new MobileConnectionRequestParent(mMobileConnection);
  // Add an extra ref for IPDL. Will be released in
  // MobileConnectionParent::DeallocPMobileConnectionRequestParent().
  actor->AddRef();
  return actor;
}

bool MobileConnectionParent::DeallocPMobileConnectionRequestParent(
    PMobileConnectionRequestParent* aActor) {
  // MobileConnectionRequestParent is refcounted, must not be freed manually.
  static_cast<MobileConnectionRequestParent*>(aActor)->Release();
  return true;
}

mozilla::ipc::IPCResult MobileConnectionParent::RecvInit(
    nsMobileConnectionInfo* aVoice, nsMobileConnectionInfo* aData,
    nsString* aLastKnownNetwork, nsString* aLastKnownHomeNetwork,
    int32_t* aNetworkSelectionMode, int32_t* aRadioState,
    nsTArray<int32_t>* aSupportedNetworkTypes, bool* aEmergencyCbMode,
    nsMobileSignalStrength* aSignalStrength,
    nsMobileDeviceIdentities* aDeviceIdentities) {
  NS_ENSURE_TRUE(mMobileConnection, IPC_FAIL_NO_REASON(this));

  NS_ENSURE_SUCCESS(mMobileConnection->GetVoice(aVoice),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetData(aData),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetLastKnownNetwork(*aLastKnownNetwork),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(
      mMobileConnection->GetLastKnownHomeNetwork(*aLastKnownHomeNetwork),
      IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(
      mMobileConnection->GetNetworkSelectionMode(aNetworkSelectionMode),
      IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetRadioState(aRadioState),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetIsInEmergencyCbMode(aEmergencyCbMode),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetSignalStrength(aSignalStrength),
                    IPC_FAIL_NO_REASON(this));
  NS_ENSURE_SUCCESS(mMobileConnection->GetDeviceIdentities(aDeviceIdentities),
                    IPC_FAIL_NO_REASON(this));

  int32_t* types = nullptr;
  uint32_t length = 0;

  nsresult rv = mMobileConnection->GetSupportedNetworkTypes(&types, &length);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  for (uint32_t i = 0; i < length; ++i) {
    aSupportedNetworkTypes->AppendElement(types[i]);
  }

  free(types);

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionParent::RecvGetSupportedNetworkTypes(
    nsTArray<int32_t>* aSupportedNetworkTypes) {
  NS_ENSURE_TRUE(mMobileConnection, IPC_FAIL_NO_REASON(this));

  int32_t* types = nullptr;
  uint32_t length = 0;

  nsresult rv = mMobileConnection->GetSupportedNetworkTypes(&types, &length);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  for (uint32_t i = 0; i < length; ++i) {
    aSupportedNetworkTypes->AppendElement(types[i]);
  }

  free(types);

  return IPC_OK();
}

// nsIMobileConnectionListener

NS_IMPL_ISUPPORTS(MobileConnectionParent, nsIMobileConnectionListener)

NS_IMETHODIMP
MobileConnectionParent::NotifyVoiceChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIMobileConnectionInfo> info;
  rv = mMobileConnection->GetVoice(getter_AddRefs(info));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendNotifyVoiceInfoChanged(info.forget().take()) ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyDataChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIMobileConnectionInfo> info;
  rv = mMobileConnection->GetData(getter_AddRefs(info));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendNotifyDataInfoChanged(info.forget().take()) ? NS_OK
                                                         : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyDataError(const nsAString& aMessage) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyDataError(nsAutoString(aMessage)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyCFStateChanged(uint16_t aAction, uint16_t aReason,
                                             const nsAString& aNumber,
                                             uint16_t aTimeSeconds,
                                             uint16_t aServiceClass) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyCFStateChanged(aAction, aReason, nsAutoString(aNumber),
                                  aTimeSeconds, aServiceClass)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyEmergencyCbModeChanged(bool aActive,
                                                     uint32_t aTimeoutMs) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyEmergencyCbModeChanged(aActive, aTimeoutMs)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyOtaStatusChanged(const nsAString& aStatus) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyOtaStatusChanged(nsAutoString(aStatus)) ? NS_OK
                                                           : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyRadioStateChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  int32_t radioState;
  rv = mMobileConnection->GetRadioState(&radioState);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyRadioStateChanged(radioState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyClirModeChanged(uint32_t aMode) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyClirModeChanged(aMode) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyLastKnownNetworkChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString network;
  rv = mMobileConnection->GetLastKnownNetwork(network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyLastKnownHomeNetworkChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString network;
  rv = mMobileConnection->GetLastKnownHomeNetwork(network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastHomeNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyNetworkSelectionModeChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  int32_t mode;
  rv = mMobileConnection->GetNetworkSelectionMode(&mode);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyNetworkSelectionModeChanged(mode) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyDeviceIdentitiesChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);
  nsresult rv;
  nsCOMPtr<nsIMobileDeviceIdentities> deviceIdentities;
  rv = mMobileConnection->GetDeviceIdentities(getter_AddRefs(deviceIdentities));

  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyDeviceIdentitiesChanged(deviceIdentities.forget().take())
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifySignalStrengthChanged() {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIMobileSignalStrength> singalStrength;
  rv = mMobileConnection->GetSignalStrength(getter_AddRefs(singalStrength));

  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifySignalStrengthChanged(singalStrength.forget().take())
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyModemRestart(const nsAString& aReason) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyModemRestart(nsAutoString(aReason)) ? NS_OK
                                                       : NS_ERROR_FAILURE;
}

/******************************************************************************
 * PMobileConnectionRequestParent
 ******************************************************************************/

void MobileConnectionRequestParent::ActorDestroy(ActorDestroyReason why) {
  mLive = false;
  mMobileConnection = nullptr;
}

bool MobileConnectionRequestParent::DoRequest(
    const GetNetworksRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetNetworks(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SelectNetworkRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileNetworkInfo> network = dont_AddRef(aRequest.network());
  return NS_SUCCEEDED(mMobileConnection->SelectNetwork(network, this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SelectNetworkAutoRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SelectNetworkAutomatically(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetPreferredNetworkTypeRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(
      mMobileConnection->SetPreferredNetworkType(aRequest.type(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetPreferredNetworkTypeRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetPreferredNetworkType(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetRoamingPreferenceRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(
      mMobileConnection->SetRoamingPreference(aRequest.mode(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetRoamingPreferenceRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetRoamingPreference(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetVoicePrivacyModeRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(
      mMobileConnection->SetVoicePrivacyMode(aRequest.enabled(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetVoicePrivacyModeRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetVoicePrivacyMode(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetCallForwardingRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallForwarding(
      aRequest.action(), aRequest.reason(), aRequest.number(),
      aRequest.timeSeconds(), aRequest.serviceClass(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetCallForwardingRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallForwarding(
      aRequest.reason(), aRequest.serviceClass(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetCallBarringRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallBarring(
      aRequest.program(), aRequest.enabled(), aRequest.password(),
      aRequest.serviceClass(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetCallBarringRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallBarring(
      aRequest.program(), aRequest.password(), aRequest.serviceClass(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const ChangeCallBarringPasswordRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->ChangeCallBarringPassword(
      aRequest.pin(), aRequest.newPin(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetCallWaitingRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallWaiting(
      aRequest.enabled(), aRequest.serviceClass(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetCallWaitingRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallWaiting(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetCallingLineIdRestrictionRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(
      mMobileConnection->SetCallingLineIdRestriction(aRequest.mode(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetCallingLineIdRestrictionRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallingLineIdRestriction(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const ExitEmergencyCbModeRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->ExitEmergencyCbMode(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const SetRadioEnabledRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(
      mMobileConnection->SetRadioEnabled(aRequest.enabled(), this));
}

bool MobileConnectionRequestParent::DoRequest(
    const GetDeviceIdentitiesRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetIdentities(this));
}

bool MobileConnectionRequestParent::DoRequest(
    const StopNetworkScanRequest& aRequest) {
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->StopNetworkScan(this));
}

nsresult MobileConnectionRequestParent::SendReply(
    const MobileConnectionReply& aReply) {
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

// nsIMobileConnectionListener

NS_IMPL_ISUPPORTS(MobileConnectionRequestParent, nsIMobileConnectionCallback);

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySuccess() {
  return SendReply(MobileConnectionReplySuccess());
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySuccessWithBoolean(bool aResult) {
  return SendReply(MobileConnectionReplySuccessBoolean(aResult));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetNetworksSuccess(
    uint32_t aCount, nsIMobileNetworkInfo** aNetworks) {
  nsTArray<nsIMobileNetworkInfo*> networks;
  for (uint32_t i = 0; i < aCount; i++) {
    nsCOMPtr<nsIMobileNetworkInfo> network = aNetworks[i];
    // We release the ref after serializing process is finished in
    // MobileConnectionIPCSerializer.
    networks.AppendElement(network.forget().take());
  }
  return SendReply(MobileConnectionReplySuccessNetworks(networks));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallForwardingSuccess(
    uint32_t aCount, nsIMobileCallForwardingOptions** aResults) {
  nsTArray<nsIMobileCallForwardingOptions*> results;
  for (uint32_t i = 0; i < aCount; i++) {
    results.AppendElement(aResults[i]);
  }

  return SendReply(MobileConnectionReplySuccessCallForwarding(results));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallBarringSuccess(
    uint16_t aProgram, bool aEnabled, uint16_t aServiceClass) {
  return SendReply(MobileConnectionReplySuccessCallBarring(aProgram, aEnabled,
                                                           aServiceClass));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallWaitingSuccess(
    uint16_t aServiceClass) {
  return SendReply(MobileConnectionReplySuccessCallWaiting(aServiceClass));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetClirStatusSuccess(uint16_t aN,
                                                          uint16_t aM) {
  return SendReply(MobileConnectionReplySuccessClirStatus(aN, aM));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetPreferredNetworkTypeSuccess(
    int32_t aType) {
  return SendReply(MobileConnectionReplySuccessPreferredNetworkType(aType));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetRoamingPreferenceSuccess(
    int32_t aMode) {
  return SendReply(MobileConnectionReplySuccessRoamingPreference(aMode));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetDeviceIdentitiesRequestSuccess(
    nsIMobileDeviceIdentities* aResult) {
  return SendReply(MobileConnectionReplySuccessDeviceIdentities(aResult));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyError(const nsAString& aName) {
  nsAutoString error(aName);
  return SendReply(MobileConnectionReplyError(error));
}
