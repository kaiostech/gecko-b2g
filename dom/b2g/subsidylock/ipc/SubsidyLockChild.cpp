/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/subsidylock/SubsidyLockChild.h"

using namespace mozilla::dom;
using namespace mozilla::dom::subsidylock;

NS_IMPL_ISUPPORTS(SubsidyLockChild, nsISubsidyLock)

SubsidyLockChild::SubsidyLockChild(uint32_t aServiceId)
    : mServiceId(aServiceId), mLive(true) {}

void SubsidyLockChild::Shutdown() {
  if (mLive) {
    mLive = false;
    Send__delete__(this);
  }
}

// nsISubsidyLock

NS_IMETHODIMP
SubsidyLockChild::GetServiceId(uint32_t* aServiceId) {
  *aServiceId = mServiceId;
  return NS_OK;
}

NS_IMETHODIMP
SubsidyLockChild::GetSubsidyLockStatus(nsISubsidyLockCallback* aCallback) {
  return SendRequest(GetSubsidyLockStatusRequest(), aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SubsidyLockChild::UnlockSubsidyLock(uint32_t aLockType,
                                    const nsAString& aPassword,
                                    nsISubsidyLockCallback* aCallback) {
  return SendRequest(
             UnlockSubsidyLockRequest(aLockType, nsAutoString(aPassword)),
             aCallback)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

bool SubsidyLockChild::SendRequest(const SubsidyLockRequest& aRequest,
                                   nsISubsidyLockCallback* aCallback) {
  NS_ENSURE_TRUE(mLive, false);

  SubsidyLockRequestChild* actor = new SubsidyLockRequestChild(aCallback);
  SendPSubsidyLockRequestConstructor(actor, aRequest);

  return true;
}

void SubsidyLockChild::ActorDestroy(ActorDestroyReason why) { mLive = false; }

PSubsidyLockRequestChild* SubsidyLockChild::AllocPSubsidyLockRequestChild(
    const SubsidyLockRequest& request) {
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool SubsidyLockChild::DeallocPSubsidyLockRequestChild(
    PSubsidyLockRequestChild* aActor) {
  delete aActor;
  return true;
}

/******************************************************************************
 * SubsidyLockRequestChild
 ******************************************************************************/

void SubsidyLockRequestChild::ActorDestroy(ActorDestroyReason why) {
  mRequestCallback = nullptr;
}

bool SubsidyLockRequestChild::DoReply(
    const SubsidyLockGetStatusSuccess& aReply) {
  uint32_t len = aReply.types().Length();
  uint32_t types[len];
  for (uint32_t i = 0; i < len; i++) {
    types[i] = aReply.types()[i];
  }

  return NS_SUCCEEDED(
      mRequestCallback->NotifyGetSubsidyLockStatusSuccess(len, types));
}

bool SubsidyLockRequestChild::DoReply(const SubsidyLockReplySuccess& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifySuccess());
}

bool SubsidyLockRequestChild::DoReply(const SubsidyLockReplyError& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifyError(aReply.message()));
}

bool SubsidyLockRequestChild::DoReply(const SubsidyLockUnlockError& aReply) {
  return NS_SUCCEEDED(mRequestCallback->NotifyUnlockSubsidyLockError(
      aReply.message(), aReply.remainingRetry()));
}

mozilla::ipc::IPCResult SubsidyLockRequestChild::Recv__delete__(
    const SubsidyLockReply& aReply) {
  MOZ_ASSERT(mRequestCallback);

  switch (aReply.type()) {
    case SubsidyLockReply::TSubsidyLockGetStatusSuccess:
      return DoReply(aReply.get_SubsidyLockGetStatusSuccess())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case SubsidyLockReply::TSubsidyLockReplySuccess:
      return DoReply(aReply.get_SubsidyLockReplySuccess())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case SubsidyLockReply::TSubsidyLockReplyError:
      return DoReply(aReply.get_SubsidyLockReplyError())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    case SubsidyLockReply::TSubsidyLockUnlockError:
      return DoReply(aReply.get_SubsidyLockUnlockError())
                 ? IPC_OK()
                 : IPC_FAIL_NO_REASON(this);
    default:
      MOZ_CRASH("Received invalid response type!");
  }

  return IPC_OK();
}
