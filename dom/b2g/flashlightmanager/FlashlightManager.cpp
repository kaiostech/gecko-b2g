/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlashlightManager.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/FlashlightManagerBinding.h"
//#include "nsIDOMClassInfo.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(FlashlightManager, DOMEventTargetHelper,
                                   mPendingFlashlightPromises)

NS_IMPL_ADDREF_INHERITED(FlashlightManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FlashlightManager, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlashlightManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

FlashlightManager::FlashlightManager(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow), mFlashlightEnabled(false) {}

FlashlightManager::~FlashlightManager() {}

void FlashlightManager::Init() {
  hal::RegisterFlashlightObserver(this);
}

void FlashlightManager::Shutdown() {
  hal::UnregisterFlashlightObserver(this);

  for (uint32_t i = 0; i < mPendingFlashlightPromises.Length(); ++i) {
    mPendingFlashlightPromises[i]->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  mPendingFlashlightPromises.Clear();
}

JSObject* FlashlightManager::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return FlashlightManager_Binding::Wrap(aCx, this, aGivenProto);
}

void FlashlightManager::Notify(
    const hal::FlashlightInformation& aFlashlightInfo) {
  if (!aFlashlightInfo.present()) {
    for (uint32_t i = 0; i < mPendingFlashlightPromises.Length(); ++i) {
      mPendingFlashlightPromises[i]->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    }
    mPendingFlashlightPromises.Clear();
  } else {
    bool hasChanged = aFlashlightInfo.enabled() != mFlashlightEnabled;
    mFlashlightEnabled = aFlashlightInfo.enabled();

    for (uint32_t i = 0; i < mPendingFlashlightPromises.Length(); ++i) {
      mPendingFlashlightPromises[i]->MaybeResolve(this);
    }
    mPendingFlashlightPromises.Clear();

    if (hasChanged) {
      DispatchTrustedEvent(u"flashlightchange"_ns);
    }
  }
}

already_AddRefed<Promise> FlashlightManager::GetPromise(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mPendingFlashlightPromises.AppendElement(promise);
  hal::RequestCurrentFlashlightState();

  return promise.forget();
}

void FlashlightManager::SetFlashlightEnabled(bool aEnabled) {
  hal::SetFlashlightEnabled(aEnabled);
}

bool FlashlightManager::FlashlightEnabled() const { return mFlashlightEnabled; }

}  // namespace dom
}  // namespace mozilla
