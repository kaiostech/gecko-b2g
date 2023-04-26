/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsParent_h
#define mozilla_dom_mobilemessage_SmsParent_h

#include "mozilla/Observer.h"
#include "mozilla/dom/mobilemessage/PSmsParent.h"
#include "mozilla/dom/mobilemessage/PSmsRequestParent.h"
#include "mozilla/dom/mobilemessage/PMobileMessageCursorParent.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageCursorCallback.h"
#include "nsIObserver.h"

using mozilla::ipc::IPCResult;

namespace mozilla {
namespace dom {

class ContentParent;

namespace mobilemessage {

class SmsParent : public PSmsParent, public nsIObserver {
  friend class mozilla::dom::ContentParent;
  friend class PSmsParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 protected:
  IPCResult RecvAddSilentNumber(const nsString& aNumber);

  IPCResult RecvRemoveSilentNumber(const nsString& aNumber);

  SmsParent();
  virtual ~SmsParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual IPCResult RecvPSmsRequestConstructor(
      PSmsRequestParent* aActor, const IPCSmsRequest& aRequest) override;

  PSmsRequestParent* AllocPSmsRequestParent(const IPCSmsRequest& aRequest);

  virtual bool DeallocPSmsRequestParent(PSmsRequestParent* aActor);

  virtual IPCResult RecvPMobileMessageCursorConstructor(
      PMobileMessageCursorParent* aActor,
      const IPCMobileMessageCursor& aRequest) override;

  virtual PMobileMessageCursorParent* AllocPMobileMessageCursorParent(
      const IPCMobileMessageCursor& aCursor);

  virtual bool DeallocPMobileMessageCursorParent(
      PMobileMessageCursorParent* aActor);

 private:
  nsTArray<nsString> mSilentNumbers;
};

class SmsRequestParent : public PSmsRequestParent,
                         public nsIMobileMessageCallback {
  friend class SmsParent;

  bool mActorDestroyed;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECALLBACK

 protected:
  SmsRequestParent() : mActorDestroyed(false) {}

  virtual ~SmsRequestParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  bool DoRequest(const SendMessageRequest& aRequest);

  bool DoRequest(const RetrieveMessageRequest& aRequest);

  bool DoRequest(const GetMessageRequest& aRequest);

  bool DoRequest(const DeleteMessageRequest& aRequest);

  bool DoRequest(const MarkMessageReadRequest& aRequest);

  bool DoRequest(const GetSegmentInfoForTextRequest& aRequest);

  bool DoRequest(const GetSmscAddressRequest& aRequest);

  bool DoRequest(const SetSmscAddressRequest& aRequest);

  nsresult SendReply(const MessageReply& aReply);
};

class MobileMessageCursorParent : public PMobileMessageCursorParent,
                                  public nsIMobileMessageCursorCallback {
  friend class SmsParent;
  friend class PMobileMessageCursorParent;
  nsCOMPtr<nsICursorContinueCallback> mContinueCallback;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECURSORCALLBACK

 protected:
  MobileMessageCursorParent() {}

  virtual ~MobileMessageCursorParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  IPCResult RecvContinue();

  bool DoRequest(const CreateMessageCursorRequest& aRequest);

  bool DoRequest(const CreateThreadCursorRequest& aRequest);
};

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobilemessage_SmsParent_h
