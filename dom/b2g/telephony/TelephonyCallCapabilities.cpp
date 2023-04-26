/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TelephonyCallCapabilities.h"
#include "nsITelephonyCallInfo.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TelephonyCallCapabilities, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TelephonyCallCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TelephonyCallCapabilities)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TelephonyCallCapabilities)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TelephonyCallCapabilities::TelephonyCallCapabilities(
    nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow),
      mVtLocalRx(false),
      mVtLocalTx(false),
      mVtRemoteRx(false),
      mVtRemoteTx(false),
      mSupportRtt(false) {}

TelephonyCallCapabilities::TelephonyCallCapabilities(
    nsPIDOMWindowInner* aWindow, uint32_t aCapabilities)
    : mWindow(aWindow) {
  Update(aCapabilities);
}

TelephonyCallCapabilities::~TelephonyCallCapabilities() {}

void TelephonyCallCapabilities::Update(uint32_t aCapabilities) {
  mVtLocalRx =
      aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_LOCAL_RX;
  mVtLocalTx =
      aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_LOCAL_TX;
  mVtRemoteRx =
      aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_REMOTE_RX;
  mVtRemoteTx =
      aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_REMOTE_TX;
  mSupportRtt = aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_RTT;
}

bool TelephonyCallCapabilities::Equals(
    RefPtr<TelephonyCallCapabilities>& aCompare) {
  if (!aCompare) {
    return false;
  }

  bool noChanged = mVtLocalRx == aCompare->VtLocalRx();
  noChanged &= mVtLocalTx == aCompare->VtLocalTx();
  noChanged &= mVtRemoteTx == aCompare->VtRemoteTx();
  noChanged &= mVtRemoteRx == aCompare->VtRemoteRx();
  noChanged &= mSupportRtt == aCompare->SupportRtt();
  return noChanged;
}

bool TelephonyCallCapabilities::Equals(uint32_t aCapabilities) {
  bool noChanged = true;

  noChanged &=
      mVtLocalRx ==
      (aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_LOCAL_RX);
  noChanged &=
      mVtLocalTx ==
      (aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_LOCAL_TX);
  noChanged &=
      mVtRemoteRx ==
      (aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_REMOTE_RX);
  noChanged &=
      mVtRemoteTx ==
      (aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_VT_REMOTE_TX);
  noChanged &= mSupportRtt ==
               (aCapabilities & nsITelephonyCallInfo::CAPABILITY_SUPPORTS_RTT);

  return noChanged;
}

JSObject* TelephonyCallCapabilities::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TelephonyCallCapabilities_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL

}  // namespace dom
}  // namespace mozilla
