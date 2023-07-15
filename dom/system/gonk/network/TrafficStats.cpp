/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrafficStats.h"
#include <inttypes.h>

// FIXME
#if 1
using android::bpf::bpfGetIfaceStats;
using android::bpf::stats_line;
#endif 

namespace mozilla {

TrafficStats::TrafficStats() {}

TrafficStats::~TrafficStats() {}

//-----------------------------------------------------------------------------
// TrafficStats::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(TrafficStats, nsITrafficStats)

//-----------------------------------------------------------------------------
// TrafficStats::TrafficStats
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TrafficStats::GetStats(nsTArray<RefPtr<nsIStatsInfo>>& aStatsInfos) {
// FIXME: stats_line is not found
#if 1
  std::vector<stats_line> lines;
  if (parseBpfNetworkStatsDev(&lines) == 0) {
    for (auto line : lines) {
      nsCString name(line.iface);
      RefPtr<nsIStatsInfo> statsInfo = new StatsInfo(
          name, line.rxBytes, line.rxPackets, line.txBytes, line.txPackets);
      aStatsInfos.AppendElement(std::move(statsInfo));
    }
  }
#endif
  return NS_OK;
}

//-----------------------------------------------------------------------------
// StatsInfo::nsISupports
//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(StatsInfo, nsIStatsInfo)

//-----------------------------------------------------------------------------
// StatsInfo::StatsInfo
//-----------------------------------------------------------------------------
StatsInfo::StatsInfo(const nsCString& aInterface, const int64_t aRxBytes,
                     const int64_t aRxPackets, const int64_t aTxBytes,
                     const int64_t aTxPackets)
    : mName(aInterface),
      mRxBytes(aRxBytes),
      mRxPackets(aRxPackets),
      mTxBytes(aTxBytes),
      mTxPackets(aTxPackets) {}

NS_IMETHODIMP
StatsInfo::GetName(nsACString& aName) {
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
StatsInfo::GetRxBytes(int64_t* aRxBytes) {
  *aRxBytes = mRxBytes;
  return NS_OK;
}

NS_IMETHODIMP
StatsInfo::GetRxPackets(int64_t* aRxPackets) {
  *aRxPackets = mRxPackets;
  return NS_OK;
}

NS_IMETHODIMP
StatsInfo::GetTxBytes(int64_t* aTxBytes) {
  *aTxBytes = mTxBytes;
  return NS_OK;
}

NS_IMETHODIMP
StatsInfo::GetTxPackets(int64_t* aTxPackets) {
  *aTxPackets = mTxPackets;
  return NS_OK;
}

}  // namespace mozilla

already_AddRefed<nsITrafficStats> NS_CreateTrafficStats() {
  nsCOMPtr<nsITrafficStats> service;
  service = new mozilla::TrafficStats();

  return service.forget();
}
