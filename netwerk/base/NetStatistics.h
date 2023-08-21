/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetStatistics_h__
#define NetStatistics_h__

#include "mozilla/Assertions.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsINetworkInterface.h"
#include "nsINetworkManager.h"
#include "nsINetworkStatsServiceProxy.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace net {

// The following members are used for network per-app metering.
const static uint64_t NETWORK_STATS_THRESHOLD = 65536;
const static char NETWORK_STATS_NO_SERVICE_TYPE[] = "";

inline nsresult GetActiveNetworkInfo(
    nsMainThreadPtrHandle<nsINetworkInfo>& aActiveNetworkInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsINetworkManager> networkManager =
      do_GetService("@mozilla.org/network/manager;1", &rv);

  if (NS_FAILED(rv) || !networkManager) {
    aActiveNetworkInfo = nullptr;
    return rv;
  }

  nsCOMPtr<nsINetworkInfo> networkInfo;
  networkManager->GetActiveNetworkInfo(getter_AddRefs(networkInfo));
  aActiveNetworkInfo = new nsMainThreadPtrHolder<nsINetworkInfo>(
      "mActiveNetworkInfo", networkInfo);

  return NS_OK;
}

class SaveNetworkStatsEvent : public Runnable {
 public:
  SaveNetworkStatsEvent(
      nsAutoCString& aOrigin, nsAutoCString& aURL,
      nsMainThreadPtrHandle<nsINetworkInfo>& aActiveNetworkInfo,
      uint64_t aCountRecv, uint64_t aCountSent, bool aIsAccumulative,
      bool aIsApp, nsAutoCString& aManifestURL)
      : mozilla::Runnable("SaveNetworkStatsEventTask"),
        mOrigin(aOrigin),
        mURL(aURL),
        mActiveNetworkInfo(aActiveNetworkInfo),
        mCountRecv(aCountRecv),
        mCountSent(aCountSent),
        mIsApp(aIsApp),
        mIsAccumulative(aIsAccumulative),
        mManifestURL(aManifestURL) {
    MOZ_ASSERT(!mOrigin.IsEmpty());
    MOZ_ASSERT(mActiveNetworkInfo);
  }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv;
    nsCOMPtr<nsINetworkStatsServiceProxy> mNetworkStatsServiceProxy =
        do_GetService("@mozilla.org/networkstatsServiceProxy;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // save the network stats through NetworkStatsServiceProxy
    mNetworkStatsServiceProxy->SaveAppStats(
        mOrigin, mURL, mActiveNetworkInfo, mCountRecv, mCountSent,
        mIsAccumulative, mIsApp, mManifestURL, nullptr);

    return NS_OK;
  }

 private:
  nsAutoCString mOrigin;
  nsAutoCString mURL;
  nsMainThreadPtrHandle<nsINetworkInfo> mActiveNetworkInfo;
  uint64_t mCountRecv;
  uint64_t mCountSent;
  bool mIsApp;
  bool mIsAccumulative;
  nsAutoCString mManifestURL;
};

}  // namespace net
}  // namespace mozilla

#endif  // !NetStatistics_h__
