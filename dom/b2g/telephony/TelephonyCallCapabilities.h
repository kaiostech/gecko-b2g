/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephonycallcapabilities_h__
#define mozilla_dom_telephonycallcapabilities_h__

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/TelephonyCallCapabilitiesBinding.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class TelephonyCallCapabilities final : public nsISupports,
                                        public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TelephonyCallCapabilities)

  explicit TelephonyCallCapabilities(nsPIDOMWindowInner* aWindow);

  TelephonyCallCapabilities(nsPIDOMWindowInner* aWindow,
                            uint32_t aCapabilities);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void Update(uint32_t aCapabilities);

  // WebIDL interface

  bool VtLocalRx() const { return mVtLocalRx; }

  bool VtLocalTx() const { return mVtLocalTx; }

  bool VtLocalBidirectional() const { return mVtLocalRx && mVtLocalTx; }

  bool VtRemoteRx() const { return mVtRemoteRx; }

  bool VtRemoteTx() const { return mVtRemoteTx; }

  bool VtRemoteBidirectional() const { return mVtRemoteRx && mVtRemoteTx; }

  bool SupportRtt() const { return mSupportRtt; }

  bool Equals(RefPtr<TelephonyCallCapabilities>& aCompare);

  bool Equals(uint32_t aCapabilities);

 private:
  ~TelephonyCallCapabilities();

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mVtLocalRx;
  bool mVtLocalTx;
  bool mVtRemoteRx;
  bool mVtRemoteTx;
  bool mSupportRtt;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_telephonycallcapabilities_h__
