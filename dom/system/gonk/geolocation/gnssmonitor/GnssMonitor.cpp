/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#include "GnssMonitor.h"

#include <android/log.h>

#include "mozilla/ClearOnShutdown.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h" // for Runnable, NS_DispatchToMainThread
#include "nsXULAppAPI.h"   // for XRE_IsParentProcess

#define LOG_ERROR(msg, ...)                                                    \
  __android_log_print(ANDROID_LOG_ERROR, "Monitor_GEO", "%s: " msg,            \
                      __FUNCTION__, ##__VA_ARGS__)

namespace b2g {

mozilla::StaticRefPtr<GnssMonitor> GnssMonitor::sSingleton;

NS_IMPL_ISUPPORTS(GnssMonitor, nsIGnssMonitor)

already_AddRefed<GnssMonitor> GnssMonitor::GetSingleton() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new GnssMonitor();
    ClearOnShutdown(&sSingleton);
  }

  RefPtr<GnssMonitor> monitor = sSingleton.get();
  return monitor.forget();
}

NS_IMETHODIMP GnssMonitor::RegisterListener(nsIGnssListener *aListener) {
  NS_ENSURE_TRUE(!mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP GnssMonitor::UnregisterListener(nsIGnssListener *aListener) {
  NS_ENSURE_TRUE(mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.RemoveObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP GnssMonitor::GetStatus(nsIGnssMonitor::GnssStatusValue *aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP GnssMonitor::GetSvList(nsTArray<RefPtr<nsIGnssSvInfo>> &aSvList) {
  aSvList = mSvList.Clone();
  return NS_OK;
}

NS_IMETHODIMP GnssMonitor::GetNmeaData(nsIGnssNmea **aNmeaData) {
  NS_IF_ADDREF(*aNmeaData = mNmeaData);
  return NS_OK;
}

NS_IMETHODIMP
GnssMonitor::UpdateGnssStatus(nsIGnssMonitor::GnssStatusValue aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  mStatus = aStatus;
  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    if (NS_FAILED(mListeners[i]->OnGnssStatusUpdate(mStatus))) {
      LOG_ERROR("failed to notify listeners about the gnss status update");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GnssMonitor::UpdateSvInfo(const nsTArray<RefPtr<nsIGnssSvInfo>> &aSvList) {
  MOZ_ASSERT(NS_IsMainThread());

  mSvList = aSvList.Clone();
  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    if (NS_FAILED(mListeners[i]->OnSvInfoUpdate(mSvList))) {
      LOG_ERROR("failed to notify listeners about the sv info update");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP GnssMonitor::UpdateNmea(nsIGnssNmea *aNmeaData) {
  MOZ_ASSERT(NS_IsMainThread());

  mNmeaData = aNmeaData;
  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    if (NS_FAILED(mListeners[i]->OnNmeaUpdate(mNmeaData))) {
      LOG_ERROR("failed to notify listeners about the nmea update");
    }
  }
  return NS_OK;
}

} // namespace b2g
