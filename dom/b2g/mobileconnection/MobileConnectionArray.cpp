/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileConnectionArray.h"
#include "mozilla/dom/MobileConnectionArrayBinding.h"
#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"

// Service instantiation
#include "fallback/ImsRegService.h"
#include "ipc/ImsRegIPCService.h"
#include "ipc/MobileConnectionIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#  include "nsIGonkMobileConnectionService.h"
#  include "nsIGonkImsRegService.h"
#endif
#include "nsXULAppAPI.h"  // For XRE_GetProcessType()

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileConnectionArray, mWindow,
                                      mMobileConnections)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileConnectionArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileConnectionArray)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileConnectionArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileConnectionArray::MobileConnectionArray(nsPIDOMWindowInner* aWindow)
    : mLengthInitialized(false), mWindow(aWindow) {}

MobileConnectionArray::~MobileConnectionArray() {
  uint32_t len = mMobileConnections.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (!mMobileConnections[i]) {
      continue;
    }

    mMobileConnections[i]->Shutdown();
    mMobileConnections[i] = nullptr;
  }
}

nsPIDOMWindowInner* MobileConnectionArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject* MobileConnectionArray::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return MobileConnectionArray_Binding::Wrap(aCx, this, aGivenProto);
}

MobileConnection* MobileConnectionArray::Item(uint32_t aIndex) {
  bool unused;
  return IndexedGetter(aIndex, unused);
}

uint32_t MobileConnectionArray::Length() {
  if (!mLengthInitialized) {
    mLengthInitialized = true;

    nsCOMPtr<nsIMobileConnectionService> service =
        do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(service, 0);

    uint32_t length = 0;
    nsresult rv = service->GetNumItems(&length);
    NS_ENSURE_SUCCESS(rv, 0);

    mMobileConnections.SetLength(length);
  }

  return mMobileConnections.Length();
}

MobileConnection* MobileConnectionArray::IndexedGetter(uint32_t aIndex,
                                                       bool& aFound) {
  aFound = aIndex < Length();
  if (!aFound) {
    return nullptr;
  }

  if (!mMobileConnections[aIndex]) {
    mMobileConnections[aIndex] = new MobileConnection(mWindow, aIndex);
  }

  return mMobileConnections[aIndex];
}

already_AddRefed<nsIMobileConnectionService>
NS_CreateMobileConnectionService() {
  nsCOMPtr<nsIMobileConnectionService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::mobileconnection::MobileConnectionIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    service = do_GetService(GONK_MOBILECONNECTION_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}

already_AddRefed<nsIImsRegService> NS_CreateImsRegService() {
  nsCOMPtr<nsIImsRegService> service;

  if (XRE_IsContentProcess()) {
    service = mozilla::dom::mobileconnection::ImsRegIPCService::GetSingleton();
  } else {
    // Note: Gonk implementation is provided by OEM Vendor by replacing the
    //       XPCOM component of nsIImsRegService with bundle in parent process.
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    service = do_GetService(GONK_IMSREGSERVICE_CONTRACTID);
#endif
  }

  // The JS side doesn't deal well with nullptr instances of nsIImsRegService,
  // so provide a dummy implementation instead.
  // This works in both parent and child processes.
  if (!service) {
    service = new mobileconnection::FallbackImsRegService();
  }

  return service.forget();
}
