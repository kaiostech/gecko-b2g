/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlashlightManager_h
#define mozilla_dom_FlashlightManager_h

#include "mozilla/dom/Promise.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "nsCycleCollectionParticipant.h"

class nsIScriptContext;

namespace mozilla {
namespace hal {
class FlashlightInformation;
}  // namespace hal
namespace dom {

typedef Observer<hal::FlashlightInformation> FlashlightObserver;

class FlashlightManager final : public DOMEventTargetHelper,
                                public FlashlightObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FlashlightManager,
                                           DOMEventTargetHelper)

  explicit FlashlightManager(nsPIDOMWindowInner* aWindow);

  void Init();
  void Shutdown();

  already_AddRefed<Promise> GetPromise(ErrorResult& aRv);

  void Notify(const hal::FlashlightInformation& aFlashlightInfo) override;

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool FlashlightEnabled() const;
  void SetFlashlightEnabled(bool aEnabled);

  IMPL_EVENT_HANDLER(flashlightchange)

 private:
  ~FlashlightManager();

  bool mFlashlightEnabled;
  nsTArray<RefPtr<Promise>> mPendingFlashlightPromises;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FlashlightManager_h
