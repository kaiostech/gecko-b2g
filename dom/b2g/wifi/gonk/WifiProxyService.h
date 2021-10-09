/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiProxyService_h
#define WifiProxyService_h

#include "nsIWifiService.h"
#include "nsCOMPtr.h"
#include "nsThread.h"
#include "mozilla/dom/WifiOptionsBinding.h"
#include "nsTArray.h"
#include "nsWifiResult.h"
#include "nsWifiEvent.h"
#include "WifiEventCallback.h"

using namespace mozilla::dom::wifi;
namespace mozilla {

class WifiProxyService final : public nsIWifiProxyService,
                               public WifiEventCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWIFIPROXYSERVICE

  static already_AddRefed<WifiProxyService> FactoryCreate();

  nsIWifiEventListener* GetListener() { return mListener; }

  void Notify(nsWifiEvent* aEvent, const nsACString& aInterface) override;

 private:
  WifiProxyService();
  ~WifiProxyService();

  nsCOMPtr<nsIThread> mControlThread;
  nsCOMPtr<nsIWifiEventListener> mListener;
};

}  // namespace mozilla

#endif  // WifiProxyService_h
