/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et sw=2 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserverService.h"
#include "nsNetworkLinkService.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "nsNetAddr.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Services.h"

#ifdef MOZ_WIDGET_GONK
#  include "nsINetworkInterface.h"
#  include "nsINetworkManager.h"
#  include "nsServiceManagerUtils.h"

static const char* kNetworkActiveChangedTopic = "network-active-changed";
#endif
using namespace mozilla;

static LazyLogModule gNotifyAddrLog("nsNetworkLinkService");
#define LOG(args) MOZ_LOG(gNotifyAddrLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(nsNetworkLinkService, nsINetworkLinkService, nsIObserver)

#ifdef MOZ_WIDGET_GONK
nsNetworkLinkService::nsNetworkLinkService()
    : mStatusIsKnown(false),
      mLinkType(nsINetworkLinkService::LINK_TYPE_UNKNOWN) {}
#else
nsNetworkLinkService::nsNetworkLinkService() : mStatusIsKnown(false) {}
#endif

NS_IMETHODIMP
nsNetworkLinkService::GetIsLinkUp(bool* aIsUp) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNetlinkSvc->GetIsLinkUp(aIsUp);
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkStatusKnown(bool* aIsKnown) {
  *aIsKnown = mStatusIsKnown;
  return NS_OK;
}

#ifdef MOZ_WIDGET_GONK
NS_IMETHODIMP
nsNetworkLinkService::GetLinkType(uint32_t* aLinkType) {
  NS_ENSURE_ARG_POINTER(aLinkType);

  *aLinkType = mLinkType;
  return NS_OK;
}
#else
NS_IMETHODIMP
nsNetworkLinkService::GetLinkType(uint32_t* aLinkType) {
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsNetworkLinkService::GetNetworkID(nsACString& aNetworkID) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNetlinkSvc->GetNetworkID(aNetworkID);
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetDnsSuffixList(nsTArray<nsCString>& aDnsSuffixList) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mNetlinkSvc->GetDnsSuffixList(aDnsSuffixList);
}

NS_IMETHODIMP
nsNetworkLinkService::GetResolvers(nsTArray<RefPtr<nsINetAddr>>& aResolvers) {
  nsTArray<mozilla::net::NetAddr> addresses;
  nsresult rv = GetNativeResolvers(addresses);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (const auto& addr : addresses) {
    aResolvers.AppendElement(MakeRefPtr<nsNetAddr>(&addr));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetNativeResolvers(
    nsTArray<mozilla::net::NetAddr>& aResolvers) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mNetlinkSvc->GetResolvers(aResolvers);
}

NS_IMETHODIMP
nsNetworkLinkService::GetPlatformDNSIndications(
    uint32_t* aPlatformDNSIndications) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef MOZ_WIDGET_GONK
void nsNetworkLinkService::UpdateLinkType(nsISupports* aNetworkInfo,
                                          bool aObserved) {
  nsCOMPtr<nsINetworkInfo> info;
  if (aNetworkInfo) {
    info = do_QueryInterface(aNetworkInfo);
  } else if (!aObserved) {
    nsCOMPtr<nsINetworkManager> networkManager =
        do_GetService("@mozilla.org/network/manager;1");
    if (!networkManager) {
      return;
    }
    networkManager->GetActiveNetworkInfo(getter_AddRefs(info));
  }

  int32_t type = nsINetworkInfo::NETWORK_TYPE_UNKNOWN;
  bool connected = false;

  if (info) {
    int32_t state;
    info->GetState(&state);
    connected = (state == nsINetworkInfo::NETWORK_STATE_CONNECTED);

    info->GetType(&type);
  }

  uint32_t linkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;

  // XXX: Since KaiOS does not support WIMAX, no need for service information
  // from mobile connection. Once KaiOS supports LINK_TYPE_ETHERNET and
  // LINK_TYPE_USB, determine the network connection type - Ethernet or USB.
  if (connected) {
    if (type == nsINetworkInfo::NETWORK_TYPE_WIFI) {
      linkType = nsINetworkLinkService::LINK_TYPE_WIFI;
    } else if (type == nsINetworkInfo::NETWORK_TYPE_MOBILE) {
      linkType = nsINetworkLinkService::LINK_TYPE_MOBILE;
    }
  }

  mLinkType = linkType;
}
#endif

NS_IMETHODIMP
nsNetworkLinkService::Observe(nsISupports* subject, const char* topic,
                              const char16_t* data) {
  if (!strcmp("xpcom-shutdown-threads", topic)) {
    Shutdown();
#ifdef MOZ_WIDGET_GONK
  } else if (!strcmp(kNetworkActiveChangedTopic, topic)) {
    UpdateLinkType(subject, true);
#endif
  }

  return NS_OK;
}

nsresult nsNetworkLinkService::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  rv = observerService->AddObserver(this, "xpcom-shutdown-threads", false);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef MOZ_WIDGET_GONK
  rv = observerService->AddObserver(this, kNetworkActiveChangedTopic, false);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  mNetlinkSvc = new mozilla::net::NetlinkService();
  rv = mNetlinkSvc->Init(this);
  if (NS_FAILED(rv)) {
    mNetlinkSvc = nullptr;
    LOG(("Cannot initialize NetlinkService [rv=0x%08" PRIx32 "]",
         static_cast<uint32_t>(rv)));
    return rv;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNetworkLinkService::Shutdown() {
  // remove xpcom shutdown observer
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "xpcom-shutdown-threads");
#ifdef MOZ_WIDGET_GONK
    observerService->RemoveObserver(this, kNetworkActiveChangedTopic);
#endif
  }

  if (mNetlinkSvc) {
    mNetlinkSvc->Shutdown();
    mNetlinkSvc = nullptr;
  }

  return NS_OK;
}

void nsNetworkLinkService::OnNetworkChanged() {
  if (StaticPrefs::network_notify_changed()) {
    RefPtr<nsNetworkLinkService> self = this;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "nsNetworkLinkService::OnNetworkChanged", [self]() {
          self->NotifyObservers(NS_NETWORK_LINK_TOPIC,
                                NS_NETWORK_LINK_DATA_CHANGED);
        }));
  }
}

void nsNetworkLinkService::OnNetworkIDChanged() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsNetworkLinkService::OnNetworkIDChanged", [self]() {
        self->NotifyObservers(NS_NETWORK_ID_CHANGED_TOPIC, nullptr);
      }));
}

void nsNetworkLinkService::OnLinkUp() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("nsNetworkLinkService::OnLinkUp", [self]() {
        self->NotifyObservers(NS_NETWORK_LINK_TOPIC, NS_NETWORK_LINK_DATA_UP);
      }));
}

void nsNetworkLinkService::OnLinkDown() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("nsNetworkLinkService::OnLinkDown", [self]() {
        self->NotifyObservers(NS_NETWORK_LINK_TOPIC, NS_NETWORK_LINK_DATA_DOWN);
      }));
}

void nsNetworkLinkService::OnLinkStatusKnown() { mStatusIsKnown = true; }

void nsNetworkLinkService::OnDnsSuffixListUpdated() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsNetworkLinkService::OnDnsSuffixListUpdated", [self]() {
        self->NotifyObservers(NS_DNS_SUFFIX_LIST_UPDATED_TOPIC, nullptr);
      }));
}

/* Sends the given event. Assumes aTopic/aData never goes out of scope (static
 * strings are ideal).
 */
void nsNetworkLinkService::NotifyObservers(const char* aTopic,
                                           const char* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("nsNetworkLinkService::NotifyObservers: topic:%s data:%s\n", aTopic,
       aData ? aData : ""));

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
    observerService->NotifyObservers(
        static_cast<nsINetworkLinkService*>(this), aTopic,
        aData ? NS_ConvertASCIItoUTF16(aData).get() : nullptr);
  }
}
