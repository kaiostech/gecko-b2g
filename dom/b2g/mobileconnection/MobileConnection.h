/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileConnection_h
#define mozilla_dom_MobileConnection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMMobileSignalStrength.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MobileSignalStrength.h"
#include "mozilla/dom/MobileConnectionBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIIccService.h"
#include "nsIMobileConnectionService.h"
// #include "nsWeakPtr.h"

namespace mozilla {
namespace dom {

class ImsRegHandler;
class DOMMobileConnectionDeviceIds;
class MobileDeviceIdentities;

class MobileConnection final : public DOMEventTargetHelper,
                               private nsIMobileConnectionListener,
                               private nsIIccListener {
  /**
   * Class MobileConnection doesn't actually expose
   * nsIMobileConnectionListener. Instead, it owns an
   * nsIMobileConnectionListener derived instance mListener and passes it to
   * nsIMobileConnectionService. The onreceived events are first delivered to
   * mListener and then forwarded to its owner, MobileConnection. See also bug
   * 775997 comment #51.
   */
  class Listener;
  nsCOMPtr<nsIGlobalObject> mOwner;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMOBILECONNECTIONLISTENER
  NS_DECL_NSIICCLISTENER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MobileConnection,
                                           DOMEventTargetHelper)

  MobileConnection(nsPIDOMWindowInner* aWindow, uint32_t aClientId);

  void Shutdown();

  virtual void DisconnectFromOwner() override;

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  void GetLastKnownNetwork(nsString& aRetVal) const;

  void GetLastKnownHomeNetwork(nsString& aRetVal) const;

  MobileConnectionInfo* Voice() const;

  MobileConnectionInfo* Data() const;

  already_AddRefed<DOMMobileConnectionDeviceIds> GetDeviceIdentities();

  void GetIccId(nsString& aRetVal) const;

  bool GetIsInEmergencyCbMode(ErrorResult& aRv) const;

  Nullable<MobileNetworkSelectionMode> GetNetworkSelectionMode() const;

  Nullable<MobileRadioState> GetRadioState() const;

  already_AddRefed<DOMMobileSignalStrength> SignalStrength() const;

  void GetSupportedNetworkTypes(nsTArray<MobileNetworkType>& aTypes) const;

  already_AddRefed<Promise> GetSupportedNetworkTypes(ErrorResult& aRv);

  already_AddRefed<ImsRegHandler> GetImsHandler() const;

  already_AddRefed<DOMRequest> GetNetworks(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SelectNetwork(DOMMobileNetworkInfo& aNetwork,
                                             ErrorResult& aRv);

  already_AddRefed<DOMRequest> SelectNetworkAutomatically(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetPreferredNetworkType(
      MobilePreferredNetworkType& aType, ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetPreferredNetworkType(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetRoamingPreference(MobileRoamingMode& aMode,
                                                    ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetRoamingPreference(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetVoicePrivacyMode(bool aEnabled,
                                                   ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetVoicePrivacyMode(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetCallForwardingOption(
      const CallForwardingOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetCallForwardingOption(uint16_t aReason,
                                                       uint16_t aServiceClass,
                                                       ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetCallBarringOption(
      const CallBarringOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetCallBarringOption(
      const CallBarringOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> ChangeCallBarringPassword(
      const CallBarringOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetCallWaitingOption(bool aEnabled,
                                                    ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetCallWaitingOption(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetCallingLineIdRestriction(uint16_t aMode,
                                                           ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetCallingLineIdRestriction(ErrorResult& aRv);

  already_AddRefed<DOMRequest> ExitEmergencyCbMode(ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetRadioEnabled(bool aEnabled, ErrorResult& aRv);

  already_AddRefed<DOMRequest> StopNetworkScan(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(voicechange)
  IMPL_EVENT_HANDLER(datachange)
  IMPL_EVENT_HANDLER(dataerror)
  IMPL_EVENT_HANDLER(cfstatechange)
  IMPL_EVENT_HANDLER(emergencycbmodechange)
  IMPL_EVENT_HANDLER(otastatuschange)
  IMPL_EVENT_HANDLER(iccchange)
  IMPL_EVENT_HANDLER(radiostatechange)
  IMPL_EVENT_HANDLER(clirmodechange)
  IMPL_EVENT_HANDLER(signalstrengthchange)
  IMPL_EVENT_HANDLER(modemrestart)
  IMPL_EVENT_HANDLER(networkselectionmodechange)

 private:
  ~MobileConnection();

 private:
  uint32_t mClientId;
  nsString mIccId;
  nsCOMPtr<nsIMobileConnection> mMobileConnection;
  nsCOMPtr<nsIIcc> mIccHandler;
  RefPtr<Listener> mListener;
  RefPtr<MobileConnectionInfo> mVoice;
  RefPtr<MobileConnectionInfo> mData;
  RefPtr<DOMMobileSignalStrength> mSignalStrength;
  // mutable for lazy initialization in GetImsRegHandler() const.
  mutable RefPtr<ImsRegHandler> mImsHandler;

  bool CheckPermission(const char* aType) const;

  void UpdateVoice();

  void UpdateData();

  bool UpdateIccId();

  void UpdateSignalStrength();

  nsresult NotifyError(DOMRequest* aRequest, const nsAString& aMessage);

  bool IsValidPassword(const nsAString& aPassword);

  bool IsValidCallBarringOptions(const CallBarringOptions& aOptions,
                                 bool isSetting);

  bool IsValidCallForwardingOptions(const CallForwardingOptions& aOptions);

  bool IsValidCallForwardingService(int32_t aServiceClass);

  bool IsValidCallForwardingReason(int32_t aReason);

  bool IsValidCallForwardingAction(int32_t aAction);

  bool IsValidCallBarringProgram(int32_t aProgram);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileConnection_h
