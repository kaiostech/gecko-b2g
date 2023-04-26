/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccParent_h
#define mozilla_dom_icc_IccParent_h

#include "mozilla/dom/icc/PIccParent.h"
#include "mozilla/dom/icc/PIccRequestParent.h"
#include "nsIIccService.h"

namespace mozilla {
namespace dom {
namespace icc {

class IccParent final : public PIccParent, public nsIIccListener {
  friend class PIccParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCLISTENER

  explicit IccParent(uint32_t aServiceId);

 protected:
  virtual ~IccParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvInit(OptionalIccInfoData* aInfoData,
                                   uint32_t* aCardState,
                                   uint32_t* aPin2CardState);

  mozilla::ipc::IPCResult RecvStkResponse(const nsString& aCmd,
                                          const nsString& aResponse);

  mozilla::ipc::IPCResult RecvStkMenuSelection(const uint16_t& aItemIdentifier,
                                               const bool& aHelpRequested);

  mozilla::ipc::IPCResult RecvStkTimerExpiration(const uint16_t& aTimerId,
                                                 const uint32_t& aTimerValue);

  mozilla::ipc::IPCResult RecvStkEventDownload(const nsString& aEvent);

  PIccRequestParent* AllocPIccRequestParent(const IccRequest& aRequest);

  bool DeallocPIccRequestParent(PIccRequestParent* aActor);

  mozilla::ipc::IPCResult RecvPIccRequestConstructor(
      PIccRequestParent* aActor, const IccRequest& aRequest) override;

 private:
  IccParent();
  nsCOMPtr<nsIIcc> mIcc;
};

class IccRequestParent final : public PIccRequestParent, public nsIIccCallback {
  friend class IccParent;
  friend class IccRequestParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCCALLBACK

  explicit IccRequestParent(nsIIcc* icc);

  class ChannelCallback : public nsIIccChannelCallback {
    friend class IccRequestParent;

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIICCCHANNELCALLBACK

   protected:
    explicit ChannelCallback(IccRequestParent& aParent) : mParent(aParent) {}

    virtual ~ChannelCallback() {}

   private:
    nsresult SendReply(const IccReply& aReply);

    IccRequestParent& mParent;
  };

 protected:
  void ActorDestroy(ActorDestroyReason why) override;

  nsresult SendReply(const IccReply& aReply);

  ChannelCallback* GetChannelCallback() { return mChannelCallback; }

 private:
  virtual ~IccRequestParent() {}

  bool DoRequest(const GetCardLockEnabledRequest& aRequest);

  bool DoRequest(const UnlockCardLockRequest& aRequest);

  bool DoRequest(const SetCardLockEnabledRequest& aRequest);

  bool DoRequest(const ChangeCardLockPasswordRequest& aRequest);

  bool DoRequest(const MatchMvnoRequest& aRequest);

  bool DoRequest(const GetServiceStateEnabledRequest& aRequest);

  bool DoRequest(const ReadContactsRequest& aRequest);

  bool DoRequest(const UpdateContactRequest& aRequest);

  bool DoRequest(const GetIccAuthenticationRequest& aRequest);

  bool DoRequest(const IccOpenChannelRequest& aRequest);

  bool DoRequest(const IccExchangeAPDURequest& aRequest);

  bool DoRequest(const IccCloseChannelRequest& aRequest);

  nsCOMPtr<nsIIcc> mIcc;
  RefPtr<ChannelCallback> mChannelCallback;
};

}  // namespace icc
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_icc_IccParent_h
