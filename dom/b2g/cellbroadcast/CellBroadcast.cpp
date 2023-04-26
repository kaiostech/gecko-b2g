/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CellBroadcast.h"
#include "mozilla/dom/CellBroadcastMessage.h"
#include "mozilla/dom/CellBroadcastBinding.h"
#include "mozilla/dom/CellBroadcastEvent.h"
#include "nsServiceManagerUtils.h"

// Service instantiation
#include "ipc/CellBroadcastChild.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#  include "nsIGonkCellBroadcastService.h"
#endif
#include "nsXULAppAPI.h"  // For XRE_GetProcessType()

using namespace mozilla::dom;
using mozilla::ErrorResult;

/**
 * CellBroadcast::Listener Implementation.
 */

class CellBroadcast::Listener final : public nsICellBroadcastListener {
 private:
  CellBroadcast* mCellBroadcast;

 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSICELLBROADCASTLISTENER(mCellBroadcast)

  explicit Listener(CellBroadcast* aCellBroadcast)
      : mCellBroadcast(aCellBroadcast) {
    MOZ_ASSERT(mCellBroadcast);
  }

  void Disconnect() {
    MOZ_ASSERT(mCellBroadcast);
    mCellBroadcast = nullptr;
  }

 private:
  ~Listener() { MOZ_ASSERT(!mCellBroadcast); }
};

NS_IMPL_ISUPPORTS(CellBroadcast::Listener, nsICellBroadcastListener)

/**
 * CellBroadcast Implementation.
 */

// static
already_AddRefed<CellBroadcast> CellBroadcast::Create(nsIGlobalObject* aGlobal,
                                                      ErrorResult& aRv) {
  nsCOMPtr<nsICellBroadcastService> service =
      do_GetService(CELLBROADCAST_SERVICE_CONTRACTID);
  if (!service) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<CellBroadcast> cb = new CellBroadcast(aGlobal, service);
  return cb.forget();
}

CellBroadcast::CellBroadcast(nsIGlobalObject* aGlobal,
                             nsICellBroadcastService* aService)
    : DOMEventTargetHelper(aGlobal) {
  mListener = new Listener(this);
  nsresult rv = aService->RegisterListener(mListener);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed registering Cell Broadcast callback");
  }
}

CellBroadcast::~CellBroadcast() {
  MOZ_ASSERT(mListener);

  mListener->Disconnect();
  nsCOMPtr<nsICellBroadcastService> service =
      do_GetService(CELLBROADCAST_SERVICE_CONTRACTID);
  if (service) {
    service->UnregisterListener(mListener);
  }

  mListener = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(CellBroadcast)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CellBroadcast,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CellBroadcast,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CellBroadcast)
  // CellBroadcast does not expose nsICellBroadcastListener.  mListener is the
  // exposed nsICellBroadcastListener and forwards the calls it receives to us.
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(CellBroadcast, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(CellBroadcast, DOMEventTargetHelper)

JSObject* CellBroadcast::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return CellBroadcast_Binding::Wrap(aCx, this, aGivenProto);
}

// Forwarded nsICellBroadcastListener methods

NS_IMETHODIMP
CellBroadcast::NotifyMessageReceived(
    uint32_t aServiceId, uint32_t aGsmGeographicalScope, uint16_t aMessageCode,
    uint16_t aMessageId, const nsAString& aLanguage, const nsAString& aBody,
    uint32_t aMessageClass, DOMTimeStamp aTimestamp,
    uint32_t aCdmaServiceCategory, bool aHasEtwsInfo, uint32_t aEtwsWarningType,
    bool aEtwsEmergencyUserAlert, bool aEtwsPopup) {
  CellBroadcastEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mMessage = new CellBroadcastMessage(
      GetOwner(), aServiceId, aGsmGeographicalScope, aMessageCode, aMessageId,
      aLanguage, aBody, aMessageClass, aTimestamp, aCdmaServiceCategory,
      aHasEtwsInfo, aEtwsWarningType, aEtwsEmergencyUserAlert, aEtwsPopup);

  RefPtr<CellBroadcastEvent> event =
      CellBroadcastEvent::Constructor(this, u"received"_ns, init);
  return DispatchTrustedEvent(event);
}

already_AddRefed<nsICellBroadcastService> NS_CreateCellBroadcastService() {
  nsCOMPtr<nsICellBroadcastService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::cellbroadcast::CellBroadcastChild();
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
  } else {
    service = do_GetService(GONK_CELLBROADCAST_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}
