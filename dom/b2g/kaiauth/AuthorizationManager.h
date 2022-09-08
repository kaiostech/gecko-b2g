/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthorizationManager_h
#define mozilla_dom_AuthorizationManager_h

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/AuthorizationManagerBinding.h"
#include "nsIAuthorizationManager.h"

namespace mozilla {
namespace dom {

class AuthorizationManager final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AuthorizationManager)

  explicit AuthorizationManager(nsIGlobalObject* aGlobal);

  already_AddRefed<Promise> GetRestrictedToken(
      const KaiServiceType& aServiceType);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 protected:
  ~AuthorizationManager();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  nsCOMPtr<nsIAuthorizationManager> mAuthProxy;
};

}  // namespace dom
}  // namespace mozilla

#endif
