/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImsDeviceConfiguration_h
#define mozilla_dom_ImsDeviceConfiguration_h

#include "nsWrapperCache.h"

struct JSContext;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class ImsDeviceConfiguration final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ImsDeviceConfiguration)

  ImsDeviceConfiguration(nsPIDOMWindowInner* aWindow,
                         const nsTArray<ImsBearer>& aBearers);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetSupportedBearers(nsTArray<ImsBearer>& aBearers) const;

 private:
  ImsDeviceConfiguration() = delete;
  ~ImsDeviceConfiguration();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsTArray<ImsBearer> mBearers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ImsDeviceConfiguration_h
