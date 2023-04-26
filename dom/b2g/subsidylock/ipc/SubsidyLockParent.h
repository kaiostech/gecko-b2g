/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_subsidylock_SubsidyLockParent_h
#define mozilla_dom_subsidylock_SubsidyLockParent_h

#include "mozilla/dom/subsidylock/PSubsidyLockParent.h"
#include "mozilla/dom/subsidylock/PSubsidyLockRequestParent.h"
#include "nsISubsidyLockService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace subsidylock {

/**
 * Parent actor of PSubsidyLock. This object is created/destroyed along
 * with child actor.
 */
class SubsidyLockParent : public PSubsidyLockParent {
  NS_INLINE_DECL_REFCOUNTING(SubsidyLockParent)
  friend PSubsidyLockParent;

 public:
  explicit SubsidyLockParent(uint32_t aClientId);

 protected:
  virtual ~SubsidyLockParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvPSubsidyLockRequestConstructor(
      PSubsidyLockRequestParent* actor,
      const SubsidyLockRequest& aRequest) override;

  PSubsidyLockRequestParent* AllocPSubsidyLockRequestParent(
      const SubsidyLockRequest& aRequest);

  bool DeallocPSubsidyLockRequestParent(PSubsidyLockRequestParent* aActor);

 private:
  nsCOMPtr<nsISubsidyLock> mSubsidyLock;
  bool mLive;
};

/******************************************************************************
 * PSubsidyLockRequestParent
 ******************************************************************************/

/**
 * Parent actor of PSubsidyLockRequestParent. The object is created along
 * with child actor and destroyed after the callback function of
 * nsIXXXXXXXXXXXCallback is called. Child actor might be destroyed before
 * any callback is triggered. So we use mLive to maintain the status of child
 * actor in order to present sending data to a dead one.
 */
class SubsidyLockRequestParent : public PSubsidyLockRequestParent,
                                 public nsISubsidyLockCallback {
  friend PSubsidyLockRequestParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUBSIDYLOCKCALLBACK

  explicit SubsidyLockRequestParent(nsISubsidyLock* aSubsidyLock)
      : mSubsidyLock(aSubsidyLock), mLive(true) {}

  bool DoRequest(const GetSubsidyLockStatusRequest& aRequest);

  bool DoRequest(const UnlockSubsidyLockRequest& aRequest);

 protected:
  virtual ~SubsidyLockRequestParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult SendReply(const SubsidyLockReply& aReply);

 private:
  nsCOMPtr<nsISubsidyLock> mSubsidyLock;
  bool mLive;
};

}  // namespace subsidylock
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_subsidylock_SubsidyLockParent_h
