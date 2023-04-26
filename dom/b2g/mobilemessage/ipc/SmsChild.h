/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsChild_h
#define mozilla_dom_mobilemessage_SmsChild_h

#include "mozilla/dom/mobilemessage/PSmsChild.h"
#include "mozilla/dom/mobilemessage/PSmsRequestChild.h"
#include "mozilla/dom/mobilemessage/PMobileMessageCursorChild.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageCursorCallback.h"

using mozilla::ipc::IPCResult;

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsChild : public PSmsChild {
  friend class PSmsChild;

 public:
  SmsChild() { MOZ_COUNT_CTOR(SmsChild); }

 protected:
  virtual ~SmsChild() { MOZ_COUNT_DTOR(SmsChild); }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  IPCResult RecvNotifyReceivedMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyRetrievingMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifySendingMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifySentMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyFailedMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyDeliverySuccessMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyDeliveryErrorMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyReceivedSilentMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyReadSuccessMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyReadErrorMessage(const MobileMessageData& aMessage);

  IPCResult RecvNotifyDeletedMessageInfo(
      const DeletedMessageInfoData& aDeletedInfo);

  PSmsRequestChild* AllocPSmsRequestChild(const IPCSmsRequest& aRequest);

  bool DeallocPSmsRequestChild(PSmsRequestChild* aActor);

  PMobileMessageCursorChild* AllocPMobileMessageCursorChild(
      const IPCMobileMessageCursor& aCursor);

  bool DeallocPMobileMessageCursorChild(PMobileMessageCursorChild* aActor);
};

class SmsRequestChild : public PSmsRequestChild {
  friend class SmsChild;
  friend class PSmsRequestChild;

  nsCOMPtr<nsIMobileMessageCallback> mReplyRequest;

 public:
  explicit SmsRequestChild(nsIMobileMessageCallback* aReplyRequest);

 protected:
  ~SmsRequestChild() { MOZ_COUNT_DTOR(SmsRequestChild); }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  IPCResult Recv__delete__(const MessageReply& aReply);
};

class MobileMessageCursorChild : public PMobileMessageCursorChild,
                                 public nsICursorContinueCallback {
  friend class SmsChild;
  friend class PMobileMessageCursorChild;

  nsCOMPtr<nsIMobileMessageCursorCallback> mCursorCallback;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICURSORCONTINUECALLBACK

  explicit MobileMessageCursorChild(nsIMobileMessageCursorCallback* aCallback);

 protected:
  ~MobileMessageCursorChild() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  IPCResult RecvNotifyResult(const MobileMessageCursorData& aData);

  IPCResult Recv__delete__(const int32_t& aError);

 private:
  void DoNotifyResult(const nsTArray<MobileMessageData>& aData);

  void DoNotifyResult(const nsTArray<ThreadData>& aData);
};

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobilemessage_SmsChild_h
