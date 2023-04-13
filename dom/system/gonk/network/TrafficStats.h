/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef moziila_TrafficStats_H
#define moziila_TrafficStats_H

#include "nsITrafficStats.h"
#include "nsString.h"
#include "bpf/BpfUtils.h"
#include "netdbpf/BpfNetworkStats.h"

#if ANDROID_VERSION >= 30
using ::Stats;
#else
using android::bpf::Stats;
#endif

namespace mozilla {

class TrafficStats final : public nsITrafficStats {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRAFFICSTATS

  TrafficStats();

 protected:
  virtual ~TrafficStats();
};

class StatsInfo : public nsIStatsInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTATSINFO

  StatsInfo(const nsCString& aInterface, const int64_t aRxBytes,
            const int64_t aRxPackets, const int64_t aTxBytes,
            const int64_t aTxPackets);

 protected:
  virtual ~StatsInfo() = default;

 private:
  const nsCString mName;
  const int64_t mRxBytes;
  const int64_t mRxPackets;
  const int64_t mTxBytes;
  const int64_t mTxPackets;
};

}  // namespace mozilla
#endif  // moziila_TrafficStats_H
