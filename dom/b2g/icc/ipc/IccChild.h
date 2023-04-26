/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccChild_h
#define mozilla_dom_icc_IccChild_h

#include "mozilla/dom/icc/PIccChild.h"
#include "mozilla/dom/icc/PIccRequestChild.h"
#include "nsCOMArray.h"
#include "nsIIccService.h"

namespace mozilla {
namespace dom {

class IccInfo;

namespace icc {

class IccChild final : public PIccChild, public nsIIcc {
  friend class PIccChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICC

  explicit IccChild();

  void Init();

  void Shutdown();

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;

  PIccRequestChild* AllocPIccRequestChild(const IccRequest& aRequest);

  bool DeallocPIccRequestChild(PIccRequestChild* aActor);

  mozilla::ipc::IPCResult RecvNotifyCardStateChanged(
      const uint32_t& aCardState, const uint32_t& aPin2CardState);

  mozilla::ipc::IPCResult RecvNotifyIccInfoChanged(
      const OptionalIccInfoData& aInfoData);

  mozilla::ipc::IPCResult RecvNotifyStkCommand(
      const nsString& aStkProactiveCmd);

  mozilla::ipc::IPCResult RecvNotifyStkSessionEnd();

 private:
  ~IccChild();

  void UpdateIccInfo(const OptionalIccInfoData& aInfoData);

  bool SendRequest(const IccRequest& aRequest, nsIIccCallback* aRequestReply);

  bool SendChannelRequest(const IccRequest& aRequest,
                          nsIIccChannelCallback* aChannelRequestReply);

  nsCOMArray<nsIIccListener> mListeners;
  RefPtr<IccInfo> mIccInfo;
  uint32_t mCardState;
  uint32_t mPin2CardState;
  bool mIsAlive;
};

class IccRequestChild final : public PIccRequestChild {
  friend class PIccRequestChild;

 public:
  explicit IccRequestChild(nsIIccCallback* aRequestReply);
  explicit IccRequestChild(nsIIccChannelCallback* aChannelRequestReply);

 protected:
  mozilla::ipc::IPCResult Recv__delete__(const IccReply& aReply);

 private:
  ~IccRequestChild() { MOZ_COUNT_DTOR(IccRequestChild); }

  nsCOMPtr<nsIIccCallback> mRequestReply;
  nsCOMPtr<nsIIccChannelCallback> mChannelRequestReply;
};

}  // namespace icc
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_icc_IccChild_h
