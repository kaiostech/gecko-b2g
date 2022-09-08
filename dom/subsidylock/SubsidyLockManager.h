/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_subsidylock_SubsidyLockManager_h__
#define mozilla_dom_subsidylock_SubsidyLockManager_h__

#include "mozilla/dom/SubsidyLock.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class SubsidyLockManager final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SubsidyLockManager)

  explicit SubsidyLockManager(nsPIDOMWindowInner* aWindow);

  nsPIDOMWindowInner* GetParentObject() const;

  // WrapperCache

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  //  WebIDL

  SubsidyLock* Item(uint32_t aIndex);

  uint32_t Length();

  SubsidyLock* IndexedGetter(uint32_t aIndex, bool& aFound);

 private:
  ~SubsidyLockManager();

  bool mLengthInitialized;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsTArray<RefPtr<SubsidyLock>> mSubsidyLocks;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_subsidylock_SubsidyLockManager_h__
