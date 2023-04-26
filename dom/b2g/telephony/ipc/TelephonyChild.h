/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyChild_h
#define mozilla_dom_telephony_TelephonyChild_h

#include "mozilla/dom/telephony/PTelephonyChild.h"
#include "mozilla/dom/telephony/PTelephonyRequestChild.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "nsITelephonyCallInfo.h"
#include "nsITelephonyService.h"

BEGIN_TELEPHONY_NAMESPACE

class TelephonyIPCService;

class TelephonyChild : public PTelephonyChild {
  friend class PTelephonyChild;

 public:
  explicit TelephonyChild(TelephonyIPCService* aService);

 protected:
  virtual ~TelephonyChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  PTelephonyRequestChild* AllocPTelephonyRequestChild(
      const IPCTelephonyRequest& aRequest);

  bool DeallocPTelephonyRequestChild(PTelephonyRequestChild* aActor);
  /*
  #ifdef MOZ_WIDGET_GONK
    PVideoCallProviderChild*
    AllocPVideoCallProviderChild(const uint32_t& clientId,
                                 const uint32_t& callIndex);

    bool
    DeallocPVideoCallProviderChild(PVideoCallProviderChild* aActor);
  #endif
  */
  mozilla::ipc::IPCResult RecvNotifyCallStateChanged(
      nsTArray<nsITelephonyCallInfo*>&& aAllInfo);

  mozilla::ipc::IPCResult RecvNotifyCdmaCallWaiting(
      const uint32_t& aClientId, const IPCCdmaWaitingCallData& aData);

  mozilla::ipc::IPCResult RecvNotifyConferenceError(const nsString& aName,
                                                    const nsString& aMessage);

  mozilla::ipc::IPCResult RecvNotifyRingbackTone(const bool& PlayRingbackTone);

  mozilla::ipc::IPCResult RecvNotifyTtyModeReceived(const uint16_t& aMode);

  mozilla::ipc::IPCResult RecvNotifyTelephonyCoverageLosing(
      const uint16_t& aType);

  mozilla::ipc::IPCResult RecvNotifySupplementaryService(
      const uint32_t& aClientId, const int32_t& aNotificationType,
      const int32_t& aCode, const int32_t& aIndex, const int32_t& aType,
      const nsString& aNumber);

  mozilla::ipc::IPCResult RecvNotifyRttModifyRequestReceived(
      const uint32_t& aClientId, const int32_t& aCallIndex,
      const uint16_t& aRttMode);

  mozilla::ipc::IPCResult RecvNotifyRttModifyResponseReceived(
      const uint32_t& aClientId, const int32_t& aCallIndex,
      const uint16_t& aStatus);

  mozilla::ipc::IPCResult RecvNotifyRttMessageReceived(
      const uint32_t& aClientId, const int32_t& aCallIndex,
      const nsString& aMessage);

 private:
  RefPtr<TelephonyIPCService> mService;
};

class TelephonyRequestChild : public PTelephonyRequestChild {
  friend class PTelephonyRequestChild;

 public:
  TelephonyRequestChild(nsITelephonyListener* aListener,
                        nsITelephonyCallback* aCallback);

 protected:
  virtual ~TelephonyRequestChild() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(const IPCTelephonyResponse& aResponse);

  mozilla::ipc::IPCResult RecvNotifyEnumerateCallState(
      nsITelephonyCallInfo* const& aInfo);

  mozilla::ipc::IPCResult RecvNotifyDialMMI(const nsString& aServiceCode);

 private:
  bool DoResponse(const SuccessResponse& aResponse);

  bool DoResponse(const ErrorResponse& aResponse);

  bool DoResponse(const DialResponseCallSuccess& aResponse);

  bool DoResponse(const DialResponseMMISuccess& aResponse);

  bool DoResponse(const DialResponseMMIError& aResponse);

  nsCOMPtr<nsITelephonyListener> mListener;
  nsCOMPtr<nsITelephonyCallback> mCallback;
};

END_TELEPHONY_NAMESPACE

#endif  // mozilla_dom_telephony_TelephonyChild_h
