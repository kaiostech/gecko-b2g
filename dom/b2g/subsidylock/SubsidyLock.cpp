/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SubsidyLock.h"

#include "SubsidyLockCallback.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsIPermissionManager.h"
#include "nsIVariant.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

using mozilla::ErrorResult;
using namespace mozilla::dom;
using namespace mozilla::dom::subsidylock;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SubsidyLock, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SubsidyLock)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SubsidyLock)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SubsidyLock)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SubsidyLock::SubsidyLock(nsPIDOMWindowInner* aWindow, uint32_t aClientId)
    : mClientId(aClientId), mWindow(aWindow) {
  nsCOMPtr<nsISubsidyLockService> service =
      do_GetService(NS_SUBSIDY_LOCK_SERVICE_CONTRACTID);

  if (!service) {
    NS_WARNING("Could not acquire nsISubsidyLockService!");
    return;
  }

  nsresult rv =
      service->GetItemByServiceId(mClientId, getter_AddRefs(mSubsidyLock));

  if (NS_FAILED(rv) || !mSubsidyLock) {
    NS_WARNING("Could not acquire nsISubsidyLock!");
    return;
  }
}

void SubsidyLock::Shutdown() {}

SubsidyLock::~SubsidyLock() { Shutdown(); }

nsPIDOMWindowInner* SubsidyLock::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

// WrapperCache

JSObject* SubsidyLock::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return SubsidyLock_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL interface

already_AddRefed<DOMRequest> SubsidyLock::GetSubsidyLockStatus(
    ErrorResult& aRv) {
  if (!mSubsidyLock) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(mWindow);
  RefPtr<SubsidyLockCallback> requestCallback =
      new SubsidyLockCallback(mWindow, request);

  nsresult rv = mSubsidyLock->GetSubsidyLockStatus(requestCallback);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest> SubsidyLock::UnlockSubsidyLock(
    const UnlockOptions& aOptions, ErrorResult& aRv) {
  if (!mSubsidyLock) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Type range check.
  uint32_t type = aOptions.mLockType;
  if (type < nsISubsidyLock::SUBSIDY_LOCK_SIM_NETWORK ||
      type > nsISubsidyLock::SUBSIDY_LOCK_SIM_SIM_PUK) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(mWindow);
  RefPtr<SubsidyLockCallback> requestCallback =
      new SubsidyLockCallback(mWindow, request);

  nsresult rv = mSubsidyLock->UnlockSubsidyLock(
      type, aOptions.mPassword.Value(), requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

}  // namespace dom
}  // namespace mozilla
