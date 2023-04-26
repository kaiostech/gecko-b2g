/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/MobileConnectionChild.h"

#include "MobileConnectionCallback.h"
#include "mozilla/dom/MobileConnectionBinding.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

NS_IMPL_ISUPPORTS(MobileConnectionChild, nsIMobileConnection)

MobileConnectionChild::MobileConnectionChild(uint32_t aServiceId)
    : mServiceId(aServiceId),
      mLive(true),
      mRadioState(0),
      mNetworkSelectionMode(0),
      mEmergencyCbMode(false) {}

void MobileConnectionChild::Init() {
  nsIMobileConnectionInfo* rawVoice;
  nsIMobileConnectionInfo* rawData;
  nsIMobileSignalStrength* rawSignalStrength;
  nsIMobileDeviceIdentities* rawDeviceIdentities;

  SendInit(&rawVoice, &rawData, &mLastNetwork, &mLastHomeNetwork,
           &mNetworkSelectionMode, &mRadioState, &mSupportedNetworkTypes,
           &mEmergencyCbMode, &rawSignalStrength, &rawDeviceIdentities);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> voice = dont_AddRef(rawVoice);
  mVoice = new MobileConnectionInfo(nullptr);
  mVoice->Update(voice);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> data = dont_AddRef(rawData);
  mData = new MobileConnectionInfo(nullptr);
  mData->Update(data);

  nsCOMPtr<nsIMobileSignalStrength> signalStrength =
      dont_AddRef(rawSignalStrength);
  mSingalStrength = new MobileSignalStrength();
  mSingalStrength->Update(signalStrength);

  nsCOMPtr<nsIMobileDeviceIdentities> deviceIdentities =
      dont_AddRef(rawDeviceIdentities);
  mDeviceIdentities = new MobileDeviceIdentities();
  mDeviceIdentities->Update(deviceIdentities);
}

void MobileConnectionChild::Shutdown() {
  if (mLive) {
    mLive = false;
    Send__delete__(this);
  }

  mListeners.Clear();
  mVoice = nullptr;
  mData = nullptr;
}

// nsIMobileConnection

NS_IMETHODIMP
MobileConnectionChild::GetServiceId(uint32_t* aServiceId) {
  *aServiceId = mServiceId;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::RegisterListener(
    nsIMobileConnectionListener* aListener) {
  NS_ENSURE_TRUE(!mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::UnregisterListener(
    nsIMobileConnectionListener* aListener) {
  NS_ENSURE_TRUE(mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.RemoveObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetVoice(nsIMobileConnectionInfo** aVoice) {
  RefPtr<nsIMobileConnectionInfo> voice(mVoice);
  voice.forget(aVoice);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetData(nsIMobileConnectionInfo** aData) {
  RefPtr<nsIMobileConnectionInfo> data(mData);
  data.forget(aData);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetRadioState(int32_t* aRadioState) {
  *aRadioState = mRadioState;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetDeviceIdentities(
    nsIMobileDeviceIdentities** aIdentities) {
  RefPtr<nsIMobileDeviceIdentities> deviceIdentities(mDeviceIdentities);
  deviceIdentities.forget(aIdentities);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetSignalStrength(
    nsIMobileSignalStrength** aSignalStrength) {
  RefPtr<nsIMobileSignalStrength> signalStrength(mSingalStrength);
  signalStrength.forget(aSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetSupportedNetworkTypes(int32_t** aTypes,
                                                uint32_t* aLength) {
  NS_ENSURE_ARG(aTypes);
  NS_ENSURE_ARG(aLength);

  // Currently the support network type get from property in gecko layer.
  // Therefore, only get those value one time.
  // Need to change this design, if the support network type will change.
  if (mSupportedNetworkTypes.IsEmpty()) {
    SendGetSupportedNetworkTypes(&mSupportedNetworkTypes);
  }

  *aLength = mSupportedNetworkTypes.Length();
  *aTypes = static_cast<int32_t*>(moz_xmalloc((*aLength) * sizeof(int32_t)));
  NS_ENSURE_TRUE(*aTypes, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < *aLength; i++) {
    (*aTypes)[i] = mSupportedNetworkTypes[i];
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetLastKnownNetwork(nsAString& aNetwork) {
  aNetwork = mLastNetwork;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetLastKnownHomeNetwork(nsAString& aNetwork) {
  aNetwork = mLastHomeNetwork;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetIsInEmergencyCbMode(bool* aActive) {
  *aActive = mEmergencyCbMode;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetNetworkSelectionMode(int32_t* aMode) {
  *aMode = mNetworkSelectionMode;
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionChild::GetNetworks(nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetNetworksRequest(), aCallback) ? NS_OK
                                                      : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SelectNetwork(nsIMobileNetworkInfo* aNetwork,
                                     nsIMobileConnectionCallback* aCallback) {
  nsCOMPtr<nsIMobileNetworkInfo> network = aNetwork;
  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendRequest(SelectNetworkRequest(network.forget().take()), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SelectNetworkAutomatically(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SelectNetworkAutoRequest(), aCallback) ? NS_OK
                                                            : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetPreferredNetworkType(
    int32_t aType, nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetPreferredNetworkTypeRequest(aType), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetPreferredNetworkType(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetPreferredNetworkTypeRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetRoamingPreference(
    int32_t aMode, nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetRoamingPreferenceRequest(aMode), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetRoamingPreference(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetRoamingPreferenceRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetVoicePrivacyMode(
    bool aEnabled, nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetVoicePrivacyModeRequest(aEnabled), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetVoicePrivacyMode(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetVoicePrivacyModeRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetCallForwarding(
    uint16_t aAction, uint16_t aReason, const nsAString& aNumber,
    uint16_t aTimeSeconds, uint16_t aServiceClass,
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(
             SetCallForwardingRequest(aAction, aReason, nsString(aNumber),
                                      aTimeSeconds, aServiceClass),
             aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetCallForwarding(
    uint16_t aReason, uint16_t aServiceClass,
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetCallForwardingRequest(aReason, aServiceClass),
                     aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetCallBarring(uint16_t aProgram, bool aEnabled,
                                      const nsAString& aPassword,
                                      uint16_t aServiceClass,
                                      nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetCallBarringRequest(aProgram, aEnabled,
                                           nsString(aPassword), aServiceClass),
                     aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetCallBarring(uint16_t aProgram,
                                      const nsAString& aPassword,
                                      uint16_t aServiceClass,
                                      nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetCallBarringRequest(aProgram, nsString(aPassword),
                                           aServiceClass),
                     aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::ChangeCallBarringPassword(
    const nsAString& aPin, const nsAString& aNewPin,
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(ChangeCallBarringPasswordRequest(nsString(aPin),
                                                      nsString(aNewPin)),
                     aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetCallWaiting(bool aEnabled, uint16_t aServiceClass,
                                      nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetCallWaitingRequest(aEnabled, aServiceClass), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetCallWaiting(nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetCallWaitingRequest(), aCallback) ? NS_OK
                                                         : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetCallingLineIdRestriction(
    uint16_t aMode, nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetCallingLineIdRestrictionRequest(aMode), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetCallingLineIdRestriction(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetCallingLineIdRestrictionRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::ExitEmergencyCbMode(
    nsIMobileConnectionCallback* aCallback) {
  return SendRequest(ExitEmergencyCbModeRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::SetRadioEnabled(bool aEnabled,
                                       nsIMobileConnectionCallback* aCallback) {
  return SendRequest(SetRadioEnabledRequest(aEnabled), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::GetNeighboringCellIds(
    nsINeighboringCellIdsCallback* aCallback) {
  // This function is supported in chrome context only.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileConnectionChild::GetCellInfoList(nsICellInfoListCallback* aCallback) {
  // This function is supported in chrome context only.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileConnectionChild::GetIdentities(nsIMobileConnectionCallback* aCallback) {
  return SendRequest(GetDeviceIdentitiesRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionChild::StopNetworkScan(nsIMobileConnectionCallback* aCallback) {
  return SendRequest(StopNetworkScanRequest(), aCallback) ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

bool MobileConnectionChild::SendRequest(
    const MobileConnectionRequest& aRequest,
    nsIMobileConnectionCallback* aCallback) {
  NS_ENSURE_TRUE(mLive, false);

  // Deallocated in
  // MobileConnectionChild::DeallocPMobileConnectionRequestChild().
  MobileConnectionRequestChild* actor =
      new MobileConnectionRequestChild(aCallback);
  SendPMobileConnectionRequestConstructor(actor, aRequest);

  return true;
}

void MobileConnectionChild::ActorDestroy(ActorDestroyReason why) {
  mLive = false;
}

PMobileConnectionRequestChild*
MobileConnectionChild::AllocPMobileConnectionRequestChild(
    const MobileConnectionRequest& request) {
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool MobileConnectionChild::DeallocPMobileConnectionRequestChild(
    PMobileConnectionRequestChild* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyVoiceInfoChanged(
    nsIMobileConnectionInfo* const& aInfo) {
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> voice = dont_AddRef(aInfo);
  mVoice->Update(voice);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyVoiceChanged();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyDataInfoChanged(
    nsIMobileConnectionInfo* const& aInfo) {
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> data = dont_AddRef(aInfo);
  mData->Update(data);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyDataChanged();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyDataError(
    const nsString& aMessage) {
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyDataError(aMessage);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyCFStateChanged(
    const uint16_t& aAction, const uint16_t& aReason, const nsString& aNumber,
    const uint16_t& aTimeSeconds, const uint16_t& aServiceClass) {
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyCFStateChanged(aAction, aReason, aNumber, aTimeSeconds,
                                        aServiceClass);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyEmergencyCbModeChanged(
    const bool& aActive, const uint32_t& aTimeoutMs) {
  mEmergencyCbMode = aActive;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyEmergencyCbModeChanged(aActive, aTimeoutMs);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyOtaStatusChanged(
    const nsString& aStatus) {
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyOtaStatusChanged(aStatus);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyRadioStateChanged(
    const int32_t& aRadioState) {
  mRadioState = aRadioState;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyRadioStateChanged();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyClirModeChanged(
    const uint32_t& aMode) {
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyClirModeChanged(aMode);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyLastNetworkChanged(
    const nsString& aNetwork) {
  mLastNetwork.Assign(aNetwork);

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyLastHomeNetworkChanged(
    const nsString& aNetwork) {
  mLastHomeNetwork.Assign(aNetwork);

  return IPC_OK();
}

mozilla::ipc::IPCResult
MobileConnectionChild::RecvNotifyNetworkSelectionModeChanged(
    const int32_t& aMode) {
  mNetworkSelectionMode = aMode;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyNetworkSelectionModeChanged();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifySignalStrengthChanged(
    nsIMobileSignalStrength* const& aSignalStrength) {
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileSignalStrength> signalStrength =
      dont_AddRef(aSignalStrength);
  mSingalStrength->Update(signalStrength);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifySignalStrengthChanged();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult MobileConnectionChild::RecvNotifyModemRestart(
    const nsString& aReason) {
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyModemRestart(aReason);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
MobileConnectionChild::RecvNotifyDeviceIdentitiesChanged(
    nsIMobileDeviceIdentities* const& aDeviceIdentities) {
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileDeviceIdentities> deviceIdentities =
      dont_AddRef(aDeviceIdentities);
  mDeviceIdentities->Update(deviceIdentities);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyDeviceIdentitiesChanged();
  }

  return IPC_OK();
}

/******************************************************************************
 * MobileConnectionRequestChild
 ******************************************************************************/

void MobileConnectionRequestChild::ActorDestroy(ActorDestroyReason why) {
  mRequestCallback = nullptr;
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccess& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifySuccess());
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessBoolean& aReply) {
  return NS_SUCCEEDED(
      mRequestCallback->NotifySuccessWithBoolean(aReply.result()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessDeviceIdentities& aReply) {
  nsCOMPtr<nsIMobileDeviceIdentities> result = dont_AddRef(aReply.result());
  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetDeviceIdentitiesRequestSuccess(result));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessNetworks& aReply) {
  uint32_t count = aReply.results().Length();
  nsTArray<nsCOMPtr<nsIMobileNetworkInfo>> results;
  for (uint32_t i = 0; i < count; i++) {
    // Use dont_AddRef here because these instances are already AddRef-ed in
    // MobileConnectionIPCSerializer.h
    nsCOMPtr<nsIMobileNetworkInfo> item = dont_AddRef(aReply.results()[i]);
    results.AppendElement(item);
  }

  return NS_SUCCEEDED(mRequestCallback->NotifyGetNetworksSuccess(
      count, const_cast<nsIMobileNetworkInfo**>(aReply.results().Elements())));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessCallForwarding& aReply) {
  uint32_t count = aReply.results().Length();
  nsTArray<nsCOMPtr<nsIMobileCallForwardingOptions>> results;
  for (uint32_t i = 0; i < count; i++) {
    // Use dont_AddRef here because these instances are already AddRef-ed in
    // MobileConnectionIPCSerializer.h
    nsCOMPtr<nsIMobileCallForwardingOptions> item =
        dont_AddRef(aReply.results()[i]);
    results.AppendElement(item);
  }

  return NS_SUCCEEDED(mRequestCallback->NotifyGetCallForwardingSuccess(
      count, const_cast<nsIMobileCallForwardingOptions**>(
                 aReply.results().Elements())));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessCallBarring& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifyGetCallBarringSuccess(
      aReply.program(), aReply.enabled(), aReply.serviceClass()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessCallWaiting& aReply) {
  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetCallWaitingSuccess(aReply.serviceClass()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessClirStatus& aReply) {
  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetClirStatusSuccess(aReply.n(), aReply.m()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessPreferredNetworkType& aReply) {
  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetPreferredNetworkTypeSuccess(aReply.type()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplySuccessRoamingPreference& aReply) {
  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetRoamingPreferenceSuccess(aReply.mode()));
}

bool MobileConnectionRequestChild::DoReply(
    const MobileConnectionReplyError& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifyError(aReply.message()));
}

mozilla::ipc::IPCResult MobileConnectionRequestChild::Recv__delete__(
    const MobileConnectionReply& aReply) {
  MOZ_ASSERT(mRequestCallback);

  switch (aReply.type()) {
    case MobileConnectionReply::TMobileConnectionReplySuccess:
      return DoReply(aReply.get_MobileConnectionReplySuccess())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessBoolean:
      return DoReply(aReply.get_MobileConnectionReplySuccessBoolean())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessNetworks:
      return DoReply(aReply.get_MobileConnectionReplySuccessNetworks())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessCallForwarding:
      return DoReply(aReply.get_MobileConnectionReplySuccessCallForwarding())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessCallBarring:
      return DoReply(aReply.get_MobileConnectionReplySuccessCallBarring())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessCallWaiting:
      return DoReply(aReply.get_MobileConnectionReplySuccessCallWaiting())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessClirStatus:
      return DoReply(aReply.get_MobileConnectionReplySuccessClirStatus())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessDeviceIdentities:
      return DoReply(aReply.get_MobileConnectionReplySuccessDeviceIdentities())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::
        TMobileConnectionReplySuccessPreferredNetworkType:
      return DoReply(
                 aReply.get_MobileConnectionReplySuccessPreferredNetworkType())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplySuccessRoamingPreference:
      return DoReply(aReply.get_MobileConnectionReplySuccessRoamingPreference())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case MobileConnectionReply::TMobileConnectionReplyError:
      return DoReply(aReply.get_MobileConnectionReplyError())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Received invalid response type!");
  }

  return IPC_FAIL_NO_REASON(this);
}
