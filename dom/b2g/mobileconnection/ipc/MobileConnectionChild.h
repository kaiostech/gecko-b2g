/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionChild_h
#define mozilla_dom_mobileconnection_MobileConnectionChild_h

#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/mobileconnection/PMobileConnectionChild.h"
#include "mozilla/dom/mobileconnection/PMobileConnectionRequestChild.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIMobileConnectionService.h"
#include "nsIVariant.h"
#include "mozilla/dom/MobileSignalStrength.h"

class nsIMobileConnectionCallback;

namespace mozilla {
namespace dom {
namespace mobileconnection {

/**
 * Child actor of PMobileConnection. The object is created by
 * MobileConnectionIPCService and destroyed after MobileConnectionIPCService is
 * shutdown. For multi-sim device, more than one instance will
 * be created and each instance represents a sim slot.
 */
class MobileConnectionChild final : public PMobileConnectionChild,
                                    public nsIMobileConnection {
  friend class PMobileConnectionChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTION

  explicit MobileConnectionChild(uint32_t aServiceId);

  void Init();

  void Shutdown();

 private:
  MobileConnectionChild() = delete;

  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~MobileConnectionChild() {}

 protected:
  bool SendRequest(const MobileConnectionRequest& aRequest,
                   nsIMobileConnectionCallback* aCallback);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  PMobileConnectionRequestChild* AllocPMobileConnectionRequestChild(
      const MobileConnectionRequest& request);

  bool DeallocPMobileConnectionRequestChild(
      PMobileConnectionRequestChild* aActor);

  mozilla::ipc::IPCResult RecvNotifyVoiceInfoChanged(
      nsIMobileConnectionInfo* const& aInfo);

  mozilla::ipc::IPCResult RecvNotifyDataInfoChanged(
      nsIMobileConnectionInfo* const& aInfo);

  mozilla::ipc::IPCResult RecvNotifyDataError(const nsString& aMessage);

  mozilla::ipc::IPCResult RecvNotifyCFStateChanged(
      const uint16_t& aAction, const uint16_t& aReason, const nsString& aNumber,
      const uint16_t& aTimeSeconds, const uint16_t& aServiceClass);

  mozilla::ipc::IPCResult RecvNotifyEmergencyCbModeChanged(
      const bool& aActive, const uint32_t& aTimeoutMs);

  mozilla::ipc::IPCResult RecvNotifyOtaStatusChanged(const nsString& aStatus);

  mozilla::ipc::IPCResult RecvNotifyRadioStateChanged(
      const int32_t& aRadioState);

  mozilla::ipc::IPCResult RecvNotifyClirModeChanged(const uint32_t& aMode);

  mozilla::ipc::IPCResult RecvNotifyLastNetworkChanged(
      const nsString& aNetwork);

  mozilla::ipc::IPCResult RecvNotifyLastHomeNetworkChanged(
      const nsString& aNetwork);

  mozilla::ipc::IPCResult RecvNotifyNetworkSelectionModeChanged(
      const int32_t& aMode);

  mozilla::ipc::IPCResult RecvNotifySignalStrengthChanged(
      nsIMobileSignalStrength* const& aSignalStrength);

  mozilla::ipc::IPCResult RecvNotifyModemRestart(const nsString& aReason);

  mozilla::ipc::IPCResult RecvNotifyDeviceIdentitiesChanged(
      nsIMobileDeviceIdentities* const& aDeviceIdentities);

 private:
  uint32_t mServiceId;
  bool mLive;
  nsCOMArray<nsIMobileConnectionListener> mListeners;
  RefPtr<MobileConnectionInfo> mVoice;
  RefPtr<MobileConnectionInfo> mData;
  int32_t mRadioState;
  nsString mLastNetwork;
  nsString mLastHomeNetwork;
  int32_t mNetworkSelectionMode;
  nsTArray<int32_t> mSupportedNetworkTypes;
  bool mEmergencyCbMode;
  RefPtr<MobileSignalStrength> mSingalStrength;
  RefPtr<MobileDeviceIdentities> mDeviceIdentities;
};

/******************************************************************************
 * PMobileConnectionRequestChild
 ******************************************************************************/

/**
 * Child actor of PMobileConnectionRequest. The object is created when an
 * asynchronous request is made and destroyed after receiving the response sent
 * by parent actor.
 */
class MobileConnectionRequestChild : public PMobileConnectionRequestChild {
  friend class PMobileConnectionRequestChild;

 public:
  explicit MobileConnectionRequestChild(
      nsIMobileConnectionCallback* aRequestCallback)
      : mRequestCallback(aRequestCallback) {
    MOZ_ASSERT(mRequestCallback);
  }

  bool DoReply(const MobileConnectionReplySuccess& aReply);

  bool DoReply(const MobileConnectionReplySuccessBoolean& aReply);

  bool DoReply(const MobileConnectionReplySuccessDeviceIdentities& aReply);

  bool DoReply(const MobileConnectionReplySuccessNetworks& aReply);

  bool DoReply(const MobileConnectionReplySuccessCallForwarding& aReply);

  bool DoReply(const MobileConnectionReplySuccessCallBarring& aReply);

  bool DoReply(const MobileConnectionReplySuccessCallWaiting& aReply);

  bool DoReply(const MobileConnectionReplySuccessClirStatus& aReply);

  bool DoReply(const MobileConnectionReplySuccessPreferredNetworkType& aReply);

  bool DoReply(const MobileConnectionReplySuccessRoamingPreference& aMode);

  bool DoReply(const MobileConnectionReplyError& aReply);

 protected:
  virtual ~MobileConnectionRequestChild() {}

  virtual void ActorDestroy(ActorDestroyReason why) override;

  mozilla::ipc::IPCResult Recv__delete__(const MobileConnectionReply& aReply);

 private:
  nsCOMPtr<nsIMobileConnectionCallback> mRequestCallback;
};

}  // namespace mobileconnection
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobileconnection_MobileConnectionChild_h
