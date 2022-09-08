/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMMobileConnectionDeviceIds_h
#define mozilla_dom_DOMMobileConnectionDeviceIds_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MobileDeviceIdentities.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class DOMMobileConnectionDeviceIds final : public nsISupports,
                                           public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMMobileConnectionDeviceIds)

  explicit DOMMobileConnectionDeviceIds(nsPIDOMWindowInner* aWindow);
  DOMMobileConnectionDeviceIds(nsPIDOMWindowInner* aWindow,
                               const nsAString& aImei, const nsAString& aImeisv,
                               const nsAString& aEsn, const nsAString& aMeid);
  void Update(nsIMobileDeviceIdentities* aIdentities);

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetImei(nsString& aRetVal) const;
  void GetImeisv(nsString& aRetVal) const;
  void GetEsn(nsString& aRetVal) const;
  void GetMeid(nsString& aRetVal) const;

 protected:
  ~DOMMobileConnectionDeviceIds();

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mImei;
  nsString mImeisv;
  nsString mEsn;
  nsString mMeid;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMMobileDeviceIdentities_h
