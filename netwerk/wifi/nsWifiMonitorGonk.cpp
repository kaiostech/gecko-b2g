/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Services.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

using namespace mozilla;

LazyLogModule gWifiMonitorLog("WifiMonitor");

// Helper functions:

bool AccessPointsEqual(nsCOMArray<nsWifiAccessPoint>& a,
                       nsCOMArray<nsWifiAccessPoint>& b) {
  if (a.Count() != b.Count()) {
    LOG(("AccessPoint lists have different lengths\n"));
    return false;
  }

  for (int32_t i = 0; i < a.Count(); i++) {
    LOG(("++ Looking for %s\n", a[i]->mSsid));
    bool found = false;
    for (int32_t j = 0; j < b.Count(); j++) {
      LOG(("   %s->%s | %s->%s\n", a[i]->mSsid, b[j]->mSsid, a[i]->mMac,
           b[j]->mMac));
      if (!strcmp(a[i]->mSsid, b[j]->mSsid) &&
          !strcmp(a[i]->mMac, b[j]->mMac) && a[i]->mSignal == b[j]->mSignal) {
        found = true;
      }
    }
    if (!found) return false;
  }
  LOG(("   match!\n"));
  return true;
}

void ReplaceArray(nsCOMArray<nsWifiAccessPoint>& a,
                  nsCOMArray<nsWifiAccessPoint>& b) {
  a.Clear();

  // better way to copy?
  for (int32_t i = 0; i < b.Count(); i++) {
    a.AppendObject(b[i]);
  }

  b.Clear();
}

NS_IMPL_ISUPPORTS(nsWifiMonitor, nsIWifiMonitor, nsIObserver,
                  nsIWifiScanResultsReady)

nsWifiMonitor::nsWifiMonitor() {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc) {
    obsSvc->AddObserver(this, "xpcom-shutdown", false);
  }
  LOG(("@@@@@ wifimonitor created\n"));
}

NS_IMETHODIMP
nsWifiMonitor::StartWatching(nsIWifiListener* aListener, bool aForcePolling) {
  LOG(("@@@@@ nsWifiMonitor::StartWatching\n"));
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!aListener) {
    return NS_ERROR_NULL_POINTER;
  }

  mListeners.AppendElement(WifiListenerHolder(aListener, aForcePolling));

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimer->Init(this, 5000, nsITimer::TYPE_REPEATING_SLACK);
  }

  StartScan();
  return NS_OK;
}

NS_IMETHODIMP
nsWifiMonitor::StopWatching(nsIWifiListener* aListener) {
  LOG(("@@@@@ nsWifiMonitor::StopWatching\n"));
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!aListener) {
    return NS_ERROR_NULL_POINTER;
  }

  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    if (mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      break;
    }
  }

  if (mListeners.Length() == 0) {
    ClearTimer();
  }
  return NS_OK;
}

void nsWifiMonitor::StartScan() {
  nsresult rv;
  nsCOMPtr<nsIWifi> wifi = do_GetService("@mozilla.org/wifi/worker;1", &rv);
  if (NS_FAILED(rv) || !wifi) {
    LOG(("Failed to create interface\n"));
    return;
  }

  wifi->GetWifiScanResults(this);
}

NS_IMETHODIMP
nsWifiMonitor::Observe(nsISupports* subject, const char* topic,
                       const char16_t* data) {
  if (!strcmp(topic, "timer-callback")) {
    LOG(("timer callback\n"));
    StartScan();
    return NS_OK;
  }

  if (!strcmp(topic, "xpcom-shutdown")) {
    LOG(("Shutting down\n"));
    ClearTimer();
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsWifiMonitor::Onready(uint32_t count, nsIWifiScanResult** results) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("@@@@@ About to send data to the wifi listeners\n"));

  nsCOMArray<nsWifiAccessPoint> accessPoints;

  for (uint32_t i = 0; i < count; i++) {
    RefPtr<nsWifiAccessPoint> ap = new nsWifiAccessPoint();

    nsString temp;
    results[i]->GetBssid(temp);
    //   00:00:00:00:00:00 --> 00-00-00-00-00-00
    for (int32_t x = 0; x < 6; x++) {
      temp.ReplaceSubstring(
          u":"_ns, u"-"_ns);  // would it be too much to ask for a ReplaceAll()?
    }

    nsCString mac;
    LossyCopyUTF16toASCII(temp, mac);

    results[i]->GetSsid(temp);

    nsCString ssid;
    LossyCopyUTF16toASCII(temp, ssid);

    int32_t signal;
    results[i]->GetSignalStrength(&signal);

    uint32_t frequency;
    results[i]->GetFrequency(&frequency);

    ap->setFrequency(frequency);
    ap->setSignal(signal);
    ap->setMacRaw(mac.get());
    ap->setSSIDRaw(ssid.get(), ssid.Length());

    accessPoints.AppendObject(ap);
  }

  bool accessPointsChanged =
      !AccessPointsEqual(accessPoints, mLastAccessPoints);
  ReplaceArray(mLastAccessPoints, accessPoints);

  uint32_t resultCount = mLastAccessPoints.Count();
  nsTArray<RefPtr<nsIWifiAccessPoint>> ac(resultCount);
  for (uint32_t i = 0; i < resultCount; i++) {
    ac.AppendElement(mLastAccessPoints[i]);
  }

  for (auto& listener : mListeners) {
    if (!listener.mHasSentData || accessPointsChanged) {
      listener.mHasSentData = true;
      listener.mListener->OnChange(ac);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWifiMonitor::Onfailure() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("@@@@@ About to send error to the wifi listeners\n"));
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i].mListener->OnError(NS_ERROR_UNEXPECTED);
  }

  ClearTimer();
  return NS_OK;
}
