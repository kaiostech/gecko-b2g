/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SubsidyLockIPCService.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::subsidylock;

NS_IMPL_ISUPPORTS(SubsidyLockIPCService, nsISubsidyLockService)

SubsidyLockIPCService::SubsidyLockIPCService() {
  int32_t numRil = Preferences::GetInt("ril.numRadioInterfaces", 1);
  mItems.SetLength(numRil);
}

SubsidyLockIPCService::~SubsidyLockIPCService() {
  uint32_t count = mItems.Length();
  for (uint32_t i = 0; i < count; i++) {
    if (mItems[i]) {
      mItems[i]->Shutdown();
    }
  }
}

// nsISubsidyLockService

NS_IMETHODIMP
SubsidyLockIPCService::GetNumItems(uint32_t* aNumItems) {
  *aNumItems = mItems.Length();
  return NS_OK;
}

NS_IMETHODIMP
SubsidyLockIPCService::GetItemByServiceId(uint32_t aServiceId,
                                          nsISubsidyLock** aItem) {
  NS_ENSURE_TRUE(aServiceId < mItems.Length(), NS_ERROR_INVALID_ARG);

  if (!mItems[aServiceId]) {
    RefPtr<SubsidyLockChild> child = new SubsidyLockChild(aServiceId);

    // |SendPSubsidyLockConstructor| adds another reference to the child
    // actor and removes in |DeallocPSubsidyLockChild|.
    ContentChild::GetSingleton()->SendPSubsidyLockConstructor(child,
                                                              aServiceId);

    mItems[aServiceId] = child;
  }

  RefPtr<nsISubsidyLock> item(mItems[aServiceId]);
  item.forget(aItem);
  return NS_OK;
}
