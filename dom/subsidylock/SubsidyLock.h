/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SubsidyLock_h
#define mozilla_dom_SubsidyLock_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/SubsidyLockBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISubsidyLockService.h"

namespace mozilla {
namespace dom {

class DOMRequest;

class SubsidyLock final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SubsidyLock)

  SubsidyLock(nsPIDOMWindowInner* aWindow, uint32_t aClientId);

  void Shutdown();

  nsPIDOMWindowInner* GetParentObject() const;

  // WrapperCache

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interfaces

  already_AddRefed<DOMRequest> GetSubsidyLockStatus(ErrorResult& aRv);

  already_AddRefed<DOMRequest> UnlockSubsidyLock(const UnlockOptions& aOptions,
                                                 ErrorResult& aRv);

 private:
  ~SubsidyLock();
  uint32_t mClientId;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsISubsidyLock> mSubsidyLock;

  bool CheckPermission(const char* aType) const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SubsidyLock_h
