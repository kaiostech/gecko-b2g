/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
extern "C" {
#include <arpa/inet.h>
#include "r_types.h"
#include "stun.h"
#include "addrs.h"
}

#include <vector>
#include <string>
#include "nsINetworkInterface.h"
#include "nsINetworkInterfaceListService.h"
#include "runnable_utils.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/SyncRunnable.h"

namespace {
struct NetworkInterface {
  std::vector<sockaddr_storage> addrs;
  std::string name;
  // See NR_INTERFACE_TYPE_* in nICEr/src/net/local_addrs.h
  int type;
};

nsresult GetInterfaces(std::vector<NetworkInterface>* aInterfaces) {
  MOZ_ASSERT(aInterfaces);

  // Obtain network interfaces from network manager.
  nsresult rv;
  nsCOMPtr<nsINetworkInterfaceListService> listService =
      do_GetService("@mozilla.org/network/interface-list-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t flags =
      nsINetworkInterfaceListService::LIST_NOT_INCLUDE_SUPL_INTERFACES |
      nsINetworkInterfaceListService::LIST_NOT_INCLUDE_MMS_INTERFACES |
      nsINetworkInterfaceListService::LIST_NOT_INCLUDE_IMS_INTERFACES |
      nsINetworkInterfaceListService::LIST_NOT_INCLUDE_DUN_INTERFACES |
      nsINetworkInterfaceListService::LIST_NOT_INCLUDE_FOTA_INTERFACES;
  nsCOMPtr<nsINetworkInterfaceList> networkList;
  NS_ENSURE_SUCCESS(
      listService->GetDataInterfaceList(flags, getter_AddRefs(networkList)),
      NS_ERROR_FAILURE);

  // Translate nsINetworkInterfaceList to NetworkInterface.
  int32_t listLength;
  NS_ENSURE_SUCCESS(networkList->GetNumberOfInterface(&listLength),
                    NS_ERROR_FAILURE);
  aInterfaces->clear();

  for (int32_t i = 0; i < listLength; i++) {
    nsCOMPtr<nsINetworkInfo> info;
    if (NS_FAILED(networkList->GetInterfaceInfo(i, getter_AddRefs(info)))) {
      continue;
    }

    char16_t** ips = nullptr;
    uint32_t* prefixs = nullptr;
    uint32_t count = 0;
    NetworkInterface interface;

    if (NS_FAILED(info->GetAddresses(&ips, &prefixs, &count))) {
      continue;
    }

    for (uint32_t j = 0; j < count; j++) {
      NS_ConvertUTF16toUTF8 ip(ips[j]);
      sockaddr_storage addr;  // guaranteed to be large enough to hold any
                              // socket address type
      memset(&addr, 0, sizeof(addr));
      sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&addr);
      sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(&addr);

      if (inet_pton(AF_INET, ip.get(), &(addr4->sin_addr)) == 1) {
        addr4->sin_family = AF_INET;
        interface.addrs.push_back(addr);
      } else if (inet_pton(AF_INET6, ip.get(), &(addr6->sin6_addr)) == 1) {
        addr6->sin6_family = AF_INET6;
        interface.addrs.push_back(addr);
      }
    }

    free(prefixs);

    if (interface.addrs.empty()) {
      continue;
    }

    nsAutoString ifaceName;
    if (NS_FAILED(info->GetName(ifaceName))) {
      continue;
    }
    interface.name = NS_ConvertUTF16toUTF8(ifaceName).get();

    int32_t type;
    if (NS_FAILED(info->GetType(&type))) {
      continue;
    }
    switch (type) {
      case nsINetworkInfo::NETWORK_TYPE_WIFI:
        interface.type = NR_INTERFACE_TYPE_WIFI;
        break;
      case nsINetworkInfo::NETWORK_TYPE_MOBILE:
        interface.type = NR_INTERFACE_TYPE_MOBILE;
        break;
    }

    aInterfaces->push_back(interface);
  }
  return NS_OK;
}
}  // anonymous namespace

int nr_stun_get_addrs(nr_local_addr aAddrs[], int aMaxAddrs, int* aCount) {
  nsresult rv;

  // Get network interface list.
  std::vector<NetworkInterface> interfaces;
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  mozilla::SyncRunnable::DispatchToThread(
      mainThread.get(),
      mozilla::WrapRunnableNMRet(&rv, &GetInterfaces, &interfaces), false);
  if (NS_FAILED(rv)) {
    return R_FAILED;
  }

  // Translate to nr_transport_addr.
  int32_t n = 0;
  for (auto& interface : interfaces) {
    for (auto& addr : interface.addrs) {
      if (nr_sockaddr_to_transport_addr(reinterpret_cast<sockaddr*>(&addr),
                                        IPPROTO_UDP, 0, &(aAddrs[n].addr))) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Problem transforming address");
        return R_FAILED;
      }
      strlcpy(aAddrs[n].addr.ifname, interface.name.c_str(),
              sizeof(aAddrs[n].addr.ifname));
      aAddrs[n].interface.type = interface.type;
      aAddrs[n].interface.estimated_speed = 0;
      if (++n >= aMaxAddrs) break;
    }
    if (n >= aMaxAddrs) break;
  }
  *aCount = n;

  for (int i = 0; i < *aCount; ++i) {
    char typestr[100];
    nr_local_addr_fmt_info_string(aAddrs + i, typestr, sizeof(typestr));
    r_log(NR_LOG_STUN, LOG_DEBUG, "Address %d: %s on %s, type: %s\n", i,
          aAddrs[i].addr.as_string, aAddrs[i].addr.ifname, typestr);
  }

  return 0;
}
