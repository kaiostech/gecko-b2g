/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileConnection.h"

#include "DOMMobileConnectionDeviceIds.h"
#include "MobileConnectionCallback.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/CFStateChangeEvent.h"
#include "mozilla/dom/DataErrorEvent.h"
#include "mozilla/dom/ImsRegHandler.h"
#include "mozilla/dom/ClirModeEvent.h"
#include "mozilla/dom/EmergencyCbModeEvent.h"
#include "mozilla/dom/OtaStatusEvent.h"
#include "mozilla/dom/ModemRestartEvent.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
// #include "nsIDOMDOMRequest.h"
#include "nsIIccInfo.h"
#include "nsIPermissionManager.h"
#include "nsIVariant.h"
// #include "nsJSON.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"

#define MOBILECONN_ERROR_INVALID_PARAMETER u"InvalidParameter"_ns
#define MOBILECONN_ERROR_INVALID_PASSWORD u"InvalidPassword"_ns

#ifdef CONVERT_STRING_TO_NULLABLE_ENUM
#  undef CONVERT_STRING_TO_NULLABLE_ENUM
#endif
#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)          \
  {                                                                         \
    uint32_t i = 0;                                                         \
    for (const EnumEntry* entry = _enumType##Values::strings; entry->value; \
         ++entry, ++i) {                                                    \
      if (_string.EqualsASCII(entry->value)) {                              \
        _enum.SetValue(static_cast<_enumType>(i));                          \
      }                                                                     \
    }                                                                       \
  }

using mozilla::ErrorResult;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

class MobileConnection::Listener final : public nsIMobileConnectionListener,
                                         public nsIIccListener {
  MobileConnection* mMobileConnection;

 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIMOBILECONNECTIONLISTENER(mMobileConnection)
  NS_FORWARD_SAFE_NSIICCLISTENER(mMobileConnection)

  explicit Listener(MobileConnection* aMobileConnection)
      : mMobileConnection(aMobileConnection) {
    MOZ_ASSERT(mMobileConnection);
  }

  void Disconnect() {
    MOZ_ASSERT(mMobileConnection);
    mMobileConnection = nullptr;
  }

 private:
  ~Listener() { MOZ_ASSERT(!mMobileConnection); }
};

NS_IMPL_ISUPPORTS(MobileConnection::Listener, nsIMobileConnectionListener,
                  nsIIccListener)

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  DOMEventTargetHelper)
  // Don't traverse mListener because it doesn't keep any reference to
  // MobileConnection but a raw pointer instead. Neither does mMobileConnection
  // because it's an xpcom service owned object and is only released at shutting
  // down.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoice)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIccHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImsHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSignalStrength)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVoice)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mData)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIccHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mImsHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSignalStrength)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileConnection)
  // MobileConnection does not expose nsIMobileConnectionListener. mListener is
  // the exposed nsIMobileConnectionListener and forwards the calls it receives
  // to us.
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, DOMEventTargetHelper)

MobileConnection::MobileConnection(nsPIDOMWindowInner* aWindow,
                                   uint32_t aClientId)
    : DOMEventTargetHelper(aWindow), mClientId(aClientId) {
  nsCOMPtr<nsIMobileConnectionService> service =
      do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);

  // Per WebAPI design, mIccId should be null instead of an empty string when no
  // SIM card is inserted. Set null as default value.
  mIccId.SetIsVoid(true);

  // Not being able to acquire the service isn't fatal since we check
  // for it explicitly below.
  if (!service) {
    NS_WARNING("Could not acquire nsIMobileConnectionService!");
    return;
  }

  nsresult rv =
      service->GetItemByServiceId(mClientId, getter_AddRefs(mMobileConnection));

  if (NS_FAILED(rv) || !mMobileConnection) {
    NS_WARNING("Could not acquire nsIMobileConnection!");
    return;
  }

  mListener = new Listener(this);
  mVoice = new MobileConnectionInfo(GetOwner());
  mData = new MobileConnectionInfo(GetOwner());
  mSignalStrength = new DOMMobileSignalStrength(GetOwner());

  if (CheckPermission("mobileconnection")) {
    DebugOnly<nsresult> rv = mMobileConnection->RegisterListener(mListener);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed registering mobile connection messages with service");
    UpdateVoice();
    UpdateData();

    nsCOMPtr<nsIIccService> iccService = do_GetService(ICC_SERVICE_CONTRACTID);

    if (iccService) {
      iccService->GetIccByServiceId(mClientId, getter_AddRefs(mIccHandler));
    }

    if (!mIccHandler) {
      NS_WARNING("Could not acquire nsIMobileConnection or nsIIcc!");
      return;
    }

    rv = mIccHandler->RegisterListener(mListener);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed registering icc messages with service");
    UpdateIccId();
  }
}

void MobileConnection::Shutdown() {
  if (mListener) {
    if (mMobileConnection) {
      mMobileConnection->UnregisterListener(mListener);
    }

    if (mIccHandler) {
      mIccHandler->UnregisterListener(mListener);
    }

    mListener->Disconnect();
    mListener = nullptr;
  }

  if (mImsHandler) {
    mImsHandler->Shutdown();
    mImsHandler = nullptr;
  }
}

MobileConnection::~MobileConnection() { Shutdown(); }

void MobileConnection::DisconnectFromOwner() {
  DOMEventTargetHelper::DisconnectFromOwner();
  // Event listeners can't be handled anymore, so we can shutdown
  // the MobileConnection.
  Shutdown();
}

JSObject* MobileConnection::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return MobileConnection_Binding::Wrap(aCx, this, aGivenProto);
}

bool MobileConnection::CheckPermission(const char* aType) const {
  return true;
  /* TODO
  nsCOMPtr<nsIPermissionManager> permMgr =
    mozilla::services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(GetOwner(), aType, &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
  */
}

void MobileConnection::UpdateVoice() {
  if (!mMobileConnection) {
    return;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info;
  mMobileConnection->GetVoice(getter_AddRefs(info));
  mVoice->Update(info);
  mVoice->UpdateDOMNetworkInfo(info);
}

void MobileConnection::UpdateData() {
  if (!mMobileConnection) {
    return;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info;
  mMobileConnection->GetData(getter_AddRefs(info));
  mData->Update(info);
  mData->UpdateDOMNetworkInfo(info);
}

bool MobileConnection::UpdateIccId() {
  nsAutoString iccId;
  nsCOMPtr<nsIIccInfo> iccInfo;
  if (mIccHandler &&
      NS_SUCCEEDED(mIccHandler->GetIccInfo(getter_AddRefs(iccInfo))) &&
      iccInfo) {
    iccInfo->GetIccid(iccId);
  } else {
    iccId.SetIsVoid(true);
  }

  if (!mIccId.Equals(iccId)) {
    mIccId = iccId;
    return true;
  }

  return false;
}

void MobileConnection::UpdateSignalStrength() {
  nsCOMPtr<nsIMobileSignalStrength> ss;
  mMobileConnection->GetSignalStrength(getter_AddRefs(ss));
  mSignalStrength->Update(ss);
}

nsresult MobileConnection::NotifyError(DOMRequest* aRequest,
                                       const nsAString& aMessage) {
  nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireErrorAsync(aRequest, aMessage);
}

bool MobileConnection::IsValidPassword(const nsAString& aPassword) {
  // Check valid PIN for supplementary services. See TS.22.004 clause 5.2.
  if (aPassword.IsEmpty() || aPassword.Length() != 4) {
    return false;
  }

  nsresult rv;
  int32_t password = nsString(aPassword).ToInteger(&rv);
  return NS_SUCCEEDED(rv) && password >= 0 && password <= 9999;
}

bool MobileConnection::IsValidCallForwardingService(int32_t aServiceClass) {
  return aServiceClass >= nsIMobileConnection::ICC_SERVICE_CLASS_VOICE &&
         aServiceClass <= 0xFF;
}

bool MobileConnection::IsValidCallForwardingReason(int32_t aReason) {
  return aReason >= nsIMobileConnection::CALL_FORWARD_REASON_UNCONDITIONAL &&
         aReason <= nsIMobileConnection::
                        CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING;
}

bool MobileConnection::IsValidCallForwardingAction(int32_t aAction) {
  return aAction >= nsIMobileConnection::CALL_FORWARD_ACTION_DISABLE &&
         aAction <= nsIMobileConnection::CALL_FORWARD_ACTION_ERASURE &&
         // Set operation doesn't allow "query" action.
         aAction != nsIMobileConnection::CALL_FORWARD_ACTION_QUERY_STATUS;
}

bool MobileConnection::IsValidCallBarringProgram(int32_t aProgram) {
  return aProgram >= nsIMobileConnection::CALL_BARRING_PROGRAM_ALL_OUTGOING &&
         aProgram <= nsIMobileConnection::CALL_BARRING_PROGRAM_INCOMING_SERVICE;
}

bool MobileConnection::IsValidCallBarringOptions(
    const CallBarringOptions& aOptions, bool isSetting) {
  if (aOptions.mServiceClass.IsNull() || aOptions.mProgram.IsNull() ||
      !IsValidCallBarringProgram(aOptions.mProgram.Value())) {
    return false;
  }

  // For setting callbarring options, |enabled| and |password| are required.
  if (isSetting && (aOptions.mEnabled.IsNull() || aOptions.mPassword.IsVoid() ||
                    aOptions.mPassword.IsEmpty())) {
    return false;
  }

  return true;
}

bool MobileConnection::IsValidCallForwardingOptions(
    const CallForwardingOptions& aOptions) {
  if (aOptions.mReason.IsNull() || aOptions.mAction.IsNull() ||
      aOptions.mServiceClass.IsNull() ||
      !IsValidCallForwardingReason(aOptions.mReason.Value()) ||
      !IsValidCallForwardingAction(aOptions.mAction.Value()) ||
      !IsValidCallForwardingService(aOptions.mServiceClass.Value())) {
    return false;
  }

  return true;
}

// WebIDL interface

void MobileConnection::GetLastKnownNetwork(nsString& aRetVal) const {
  aRetVal.SetIsVoid(true);

  if (!mMobileConnection) {
    return;
  }

  mMobileConnection->GetLastKnownNetwork(aRetVal);
}

void MobileConnection::GetLastKnownHomeNetwork(nsString& aRetVal) const {
  aRetVal.SetIsVoid(true);

  if (!mMobileConnection) {
    return;
  }

  mMobileConnection->GetLastKnownHomeNetwork(aRetVal);
}

// All fields below require the "mobileconnection" permission.

MobileConnectionInfo* MobileConnection::Voice() const { return mVoice; }

MobileConnectionInfo* MobileConnection::Data() const { return mData; }

already_AddRefed<DOMMobileConnectionDeviceIds>
MobileConnection::GetDeviceIdentities() {
  if (!mMobileConnection) {
    return nullptr;
  }

  nsAutoString imei;
  nsAutoString imeisv;
  nsAutoString esn;
  nsAutoString meid;

  nsCOMPtr<nsIMobileDeviceIdentities> identities;
  mMobileConnection->GetDeviceIdentities(getter_AddRefs(identities));

  identities->GetImei(imei);
  identities->GetImeisv(imeisv);
  identities->GetEsn(esn);
  identities->GetMeid(meid);

  RefPtr<DOMMobileConnectionDeviceIds> domIdentities =
      new DOMMobileConnectionDeviceIds(GetOwner(), imei, imeisv, esn, meid);

  return domIdentities.forget();
}

already_AddRefed<DOMMobileSignalStrength> MobileConnection::SignalStrength()
    const {
  if (!mMobileConnection) {
    return nullptr;
  }
  RefPtr<DOMMobileSignalStrength> signalStrength = mSignalStrength;
  return signalStrength.forget();
}

void MobileConnection::GetIccId(nsString& aRetVal) const { aRetVal = mIccId; }

bool MobileConnection::GetIsInEmergencyCbMode(ErrorResult& aRv) const {
  bool result = false;

  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  nsresult rv = mMobileConnection->GetIsInEmergencyCbMode(&result);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  return result;
}

Nullable<MobileNetworkSelectionMode> MobileConnection::GetNetworkSelectionMode()
    const {
  Nullable<MobileNetworkSelectionMode> retVal =
      Nullable<MobileNetworkSelectionMode>();

  if (!mMobileConnection) {
    return retVal;
  }

  int32_t mode = nsIMobileConnection::NETWORK_SELECTION_MODE_UNKNOWN;
  if (NS_SUCCEEDED(mMobileConnection->GetNetworkSelectionMode(&mode)) &&
      mode != nsIMobileConnection::NETWORK_SELECTION_MODE_UNKNOWN) {
    MOZ_ASSERT(mode <
               static_cast<int32_t>(MobileNetworkSelectionMode::EndGuard_));
    retVal.SetValue(static_cast<MobileNetworkSelectionMode>(mode));
  }

  return retVal;
}

Nullable<MobileRadioState> MobileConnection::GetRadioState() const {
  Nullable<MobileRadioState> retVal = Nullable<MobileRadioState>();

  if (!mMobileConnection) {
    return retVal;
  }

  int32_t state = nsIMobileConnection::MOBILE_RADIO_STATE_UNKNOWN;
  if (NS_SUCCEEDED(mMobileConnection->GetRadioState(&state)) &&
      state != nsIMobileConnection::MOBILE_RADIO_STATE_UNKNOWN) {
    MOZ_ASSERT(state < static_cast<int32_t>(MobileRadioState::EndGuard_));
    retVal.SetValue(static_cast<MobileRadioState>(state));
  }

  return retVal;
}

already_AddRefed<Promise> MobileConnection::GetSupportedNetworkTypes(
    ErrorResult& aRv) {
  nsTArray<MobileNetworkType> result;

  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (!promise) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  int32_t* types = nullptr;
  uint32_t length = 0;

  aRv = mMobileConnection->GetSupportedNetworkTypes(&types, &length);
  MOZ_ASSERT(!aRv.Failed());

  for (uint32_t i = 0; i < length; ++i) {
    int32_t type = types[i];

    MOZ_ASSERT(type < static_cast<int32_t>(MobileNetworkType::EndGuard_));
    result.AppendElement(static_cast<MobileNetworkType>(type));
  }

  promise->MaybeResolve(result);
  free(types);

  return promise.forget();
}

already_AddRefed<ImsRegHandler> MobileConnection::GetImsHandler() const {
  if (!mMobileConnection) {
    return nullptr;
  }

  if (mImsHandler) {
    RefPtr<ImsRegHandler> handler = mImsHandler;
    return handler.forget();
  }

  nsCOMPtr<nsIImsRegService> imsService =
      do_GetService(IMS_REG_SERVICE_CONTRACTID);
  if (!imsService) {
    NS_WARNING("Could not acquire nsIImsRegService!");
    return nullptr;
  }

  nsCOMPtr<nsIImsRegHandler> internalHandler;
  imsService->GetHandlerByServiceId(mClientId, getter_AddRefs(internalHandler));
  // ImsRegHandler is optional, always check even if it was retreived
  // successfully.
  if (!internalHandler) {
    NS_WARNING("Could not acquire nsIImsRegHandler!");
    return nullptr;
  }

  mImsHandler = new ImsRegHandler(GetOwner(), internalHandler);
  RefPtr<ImsRegHandler> handler = mImsHandler;

  return handler.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetNetworks(ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetNetworks(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SelectNetwork(
    DOMMobileNetworkInfo& aNetwork, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  RefPtr<MobileNetworkInfo> networkInfo;
  networkInfo = aNetwork.GetNetwork();

  nsresult rv = mMobileConnection->SelectNetwork(networkInfo, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SelectNetworkAutomatically(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SelectNetworkAutomatically(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetPreferredNetworkType(
    MobilePreferredNetworkType& aType, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  int32_t type = static_cast<int32_t>(aType);

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv =
      mMobileConnection->SetPreferredNetworkType(type, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetPreferredNetworkType(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetPreferredNetworkType(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetRoamingPreference(
    MobileRoamingMode& aMode, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  int32_t mode = static_cast<int32_t>(aMode);

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SetRoamingPreference(mode, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetRoamingPreference(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetRoamingPreference(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetVoicePrivacyMode(
    bool aEnabled, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv =
      mMobileConnection->SetVoicePrivacyMode(aEnabled, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetVoicePrivacyMode(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetVoicePrivacyMode(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetCallForwardingOption(
    uint16_t aReason, uint16_t aServiceClass, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());

  if (!IsValidCallForwardingReason(aReason) ||
      !IsValidCallForwardingService(aServiceClass)) {
    nsresult rv = NotifyError(request, MOBILECONN_ERROR_INVALID_PARAMETER);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    return request.forget();
  }

  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetCallForwarding(aReason, aServiceClass,
                                                     requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetCallForwardingOption(
    const CallForwardingOptions& aOptions, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());

  if (!IsValidCallForwardingOptions(aOptions)) {
    nsresult rv = NotifyError(request, MOBILECONN_ERROR_INVALID_PARAMETER);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    return request.forget();
  }

  // Fill in optional attributes.
  uint16_t timeSeconds = 0;
  if (!aOptions.mTimeSeconds.IsNull()) {
    timeSeconds = aOptions.mTimeSeconds.Value();
  }
  uint16_t serviceClass = nsIMobileConnection::ICC_SERVICE_CLASS_NONE;
  if (!aOptions.mServiceClass.IsNull()) {
    serviceClass = aOptions.mServiceClass.Value();
  }
  nsAutoString number;
  if (aOptions.mNumber.IsVoid() || aOptions.mNumber.IsEmpty()) {
    number.SetIsVoid(true);
  } else {
    number = aOptions.mNumber;
  }

  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SetCallForwarding(
      aOptions.mAction.Value(), aOptions.mReason.Value(), number, timeSeconds,
      serviceClass, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetCallBarringOption(
    const CallBarringOptions& aOptions, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());

  if (!IsValidCallBarringOptions(aOptions, false)) {
    nsresult rv = NotifyError(request, MOBILECONN_ERROR_INVALID_PARAMETER);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    return request.forget();
  }

  // Fill in optional attributes.
  nsAutoString password;
  if (aOptions.mPassword.IsVoid() || aOptions.mPassword.IsEmpty()) {
    password.SetIsVoid(true);
  } else {
    password = aOptions.mPassword;
  }

  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetCallBarring(
      aOptions.mProgram.Value(), password, aOptions.mServiceClass.Value(),
      requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetCallBarringOption(
    const CallBarringOptions& aOptions, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());

  if (!IsValidCallBarringOptions(aOptions, true)) {
    nsresult rv = NotifyError(request, MOBILECONN_ERROR_INVALID_PARAMETER);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    return request.forget();
  }

  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SetCallBarring(
      aOptions.mProgram.Value(), aOptions.mEnabled.Value(), aOptions.mPassword,
      aOptions.mServiceClass.Value(), requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::ChangeCallBarringPassword(
    const CallBarringOptions& aOptions, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());

  if (aOptions.mPin.IsVoid() || aOptions.mPin.IsEmpty() ||
      aOptions.mNewPin.IsVoid() || aOptions.mNewPin.IsEmpty() ||
      !IsValidPassword(aOptions.mPin) || !IsValidPassword(aOptions.mNewPin)) {
    nsresult rv = NotifyError(request, MOBILECONN_ERROR_INVALID_PASSWORD);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    return request.forget();
  }

  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->ChangeCallBarringPassword(
      aOptions.mPin, aOptions.mNewPin, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetCallWaitingOption(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetCallWaiting(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetCallWaitingOption(
    bool aEnabled, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SetCallWaiting(
      aEnabled, nsIMobileConnection::ICC_SERVICE_CLASS_VOICE, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::GetCallingLineIdRestriction(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->GetCallingLineIdRestriction(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetCallingLineIdRestriction(
    uint16_t aMode, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv =
      mMobileConnection->SetCallingLineIdRestriction(aMode, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::ExitEmergencyCbMode(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->ExitEmergencyCbMode(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::SetRadioEnabled(
    bool aEnabled, ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->SetRadioEnabled(aEnabled, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> MobileConnection::StopNetworkScan(
    ErrorResult& aRv) {
  if (!mMobileConnection) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  RefPtr<MobileConnectionCallback> requestCallback =
      new MobileConnectionCallback(GetOwner(), request);

  nsresult rv = mMobileConnection->StopNetworkScan(requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

// nsIMobileConnectionListener

NS_IMETHODIMP
MobileConnection::NotifyVoiceChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  UpdateVoice();

  return DispatchTrustedEvent(u"voicechange"_ns);
}

NS_IMETHODIMP
MobileConnection::NotifyDataChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  UpdateData();

  return DispatchTrustedEvent(u"datachange"_ns);
}

NS_IMETHODIMP
MobileConnection::NotifyDataError(const nsAString& aMessage) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  DataErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMessage = aMessage;

  RefPtr<DataErrorEvent> event =
      DataErrorEvent::Constructor(this, u"dataerror"_ns, init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyCFStateChanged(unsigned short aAction,
                                       unsigned short aReason,
                                       const nsAString& aNumber,
                                       unsigned short aSeconds,
                                       unsigned short aServiceClass) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  CFStateChangeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mAction = aAction;
  init.mReason = aReason;
  init.mNumber = aNumber;
  init.mTimeSeconds = aSeconds;
  init.mServiceClass = aServiceClass;

  RefPtr<CFStateChangeEvent> event =
      CFStateChangeEvent::Constructor(this, u"cfstatechange"_ns, init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyEmergencyCbModeChanged(bool aActive,
                                               uint32_t aTimeoutMs) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  EmergencyCbModeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mActive = aActive;
  init.mTimeoutMs = aTimeoutMs;

  RefPtr<EmergencyCbModeEvent> event = EmergencyCbModeEvent::Constructor(
      this, u"emergencycbmodechange"_ns, init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyOtaStatusChanged(const nsAString& aStatus) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }
  OtaStatusEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mStatus = aStatus;

  RefPtr<OtaStatusEvent> event =
      OtaStatusEvent::Constructor(this, u"otastatuschange"_ns, init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyRadioStateChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(u"radiostatechange"_ns);
}

NS_IMETHODIMP
MobileConnection::NotifyClirModeChanged(uint32_t aMode) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  ClirModeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMode = aMode;

  RefPtr<ClirModeEvent> event =
      ClirModeEvent::Constructor(this, u"clirmodechange"_ns, init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyLastKnownNetworkChanged() { return NS_OK; }

NS_IMETHODIMP
MobileConnection::NotifyLastKnownHomeNetworkChanged() { return NS_OK; }

NS_IMETHODIMP
MobileConnection::NotifyNetworkSelectionModeChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(u"networkselectionmodechange"_ns);
}

NS_IMETHODIMP
MobileConnection::NotifyDeviceIdentitiesChanged() {
  // To be supported when bug 1222870 is required in m-c.
  return NS_OK;
}

NS_IMETHODIMP
MobileConnection::NotifySignalStrengthChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  UpdateSignalStrength();

  return DispatchTrustedEvent(u"signalstrengthchange"_ns);
}

NS_IMETHODIMP
MobileConnection::NotifyModemRestart(const nsAString& aReason) {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  ModemRestartEventInit init;
  init.mReason = aReason;

  RefPtr<ModemRestartEvent> event =
      ModemRestartEvent::Constructor(this, u"modemrestart"_ns, init);

  return DispatchTrustedEvent(event);
}

// nsIIccListener

NS_IMETHODIMP
MobileConnection::NotifyStkCommand(nsIStkProactiveCmd* aStkProactiveCmd) {
  return NS_OK;
}

NS_IMETHODIMP
MobileConnection::NotifyStkSessionEnd() { return NS_OK; }

NS_IMETHODIMP
MobileConnection::NotifyCardStateChanged() { return NS_OK; }

NS_IMETHODIMP
MobileConnection::NotifyIccInfoChanged() {
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!UpdateIccId()) {
    return NS_OK;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, u"iccchange"_ns, CanBubble::eNo);

  return asyncDispatcher->PostDOMEvent();
}

NS_IMETHODIMP
MobileConnection::NotifyIsimInfoChanged() { return NS_OK; }
