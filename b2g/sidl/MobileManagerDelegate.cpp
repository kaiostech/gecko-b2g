/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileManagerDelegate.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "mozilla/Hal.h"
#include "nsIIccService.h"
#include "nsIIccInfo.h"
#include "nsIMobileConnectionService.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileNetworkInfo.h"
#include "nsIMobileDeviceIdentities.h"
#include "nsINetworkInterface.h"
#include "nsINetworkManager.h"
#include "nsIRadioInterfaceLayer.h"

#define MOBILEMANAGERDELEGATE_DEBUG

#ifdef MOBILEMANAGERDELEGATE_DEBUG
static int s_mobile_debug = 1;
#  define MOBILEDEBUG(X) \
    if (s_mobile_debug) printf_stderr("MobileManagerDelegate:%s\n", X);
#else
#  define MOBILEDEBUG(X)
#endif

#define RETURN_IF_NULL_MSG(varname, msg) \
  if (NULL == varname) {                 \
    MOBILEDEBUG(msg)                     \
    return NS_ERROR_FAILURE;             \
  }

#define RETURN_IF_ERROR_MSG(varname, msg) \
  if (NS_OK != varname) {                 \
    MOBILEDEBUG(msg)                      \
    return NS_ERROR_FAILURE;              \
  }

#define RETURN_IF_EMPTY_MSG(varname, msg) \
  if (!varname.Length()) {                \
    MOBILEDEBUG(msg)                      \
    return NS_ERROR_FAILURE;              \
  }

using namespace mozilla;

namespace mozilla {

// The singleton MobileManagerDelegate service, to be used on the main thread.
static StaticRefPtr<MobileManagerDelegateService> gMobileManagerDelegateService;

NS_IMPL_ISUPPORTS(MobileManagerDelegateService, nsIMobileManagerDelegate)

MobileManagerDelegateService::MobileManagerDelegateService() {}

MobileManagerDelegateService::~MobileManagerDelegateService() {}

/* static */
already_AddRefed<MobileManagerDelegateService>
MobileManagerDelegateService::ConstructMobileManagerDelegate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gMobileManagerDelegateService) {
    gMobileManagerDelegateService = new MobileManagerDelegateService();
    ClearOnShutdown(&gMobileManagerDelegateService);
  }

  RefPtr<MobileManagerDelegateService> service =
      gMobileManagerDelegateService.get();
  return service.forget();
}

bool MobileManagerDelegateService::ValidateCardId(int cardId) {
  nsCOMPtr<nsIRadioInterfaceLayer> ril = do_GetService("@mozilla.org/ril;1");
  NS_ENSURE_TRUE(ril, false);

  uint32_t numRadioInterfaces = 0;
  nsresult rv = ril->GetNumRadioInterfaces(&numRadioInterfaces);
  if (NS_FAILED(rv)) {
    MOBILEDEBUG("Failed to get radio interface numbers")
    return false;
  }

  if (uint32_t(cardId) >= numRadioInterfaces) {
    MOBILEDEBUG("Invalid cardId")
    return false;
  }

  return true;
}

NS_IMETHODIMP
MobileManagerDelegateService::GetCardInfo(int cardId, int type,
                                          nsAString& result) {
  if (!ValidateCardId(cardId)) {
    return NS_ERROR_FAILURE;
  }

  if (CIT_IMEI == type) {  // IMEI
    nsCOMPtr<nsIMobileConnectionService> service =
        do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
    RETURN_IF_NULL_MSG(service, "Invalid nsIMobileConnectionService")

    nsCOMPtr<nsIMobileConnection> connection;
    service->GetItemByServiceId(cardId, getter_AddRefs(connection));
    RETURN_IF_NULL_MSG(connection, "Invalid nsIMobileConnection")

    nsCOMPtr<nsIMobileDeviceIdentities> deviceIdentities;
    connection->GetDeviceIdentities(getter_AddRefs(deviceIdentities));
    RETURN_IF_NULL_MSG(deviceIdentities, "Invalid nsIMobileDeviceIdentities")

    RETURN_IF_ERROR_MSG(deviceIdentities->GetImei(result), "Failed to get IMEI")

    RETURN_IF_EMPTY_MSG(result, "Imei is empty")
  } else {  // IMSI and MSISDN both used icc
    nsCOMPtr<nsIIccService> iccService = do_GetService(ICC_SERVICE_CONTRACTID);

    nsCOMPtr<nsIIcc> icc;
    iccService->GetIccByServiceId(cardId, getter_AddRefs(icc));
    RETURN_IF_NULL_MSG(icc, "Invalid nsIIcc")

    if (CIT_IMSI == type) {  // IMSI ends here
      RETURN_IF_ERROR_MSG(icc->GetImsi(result), "Failed to get IMSI")

      RETURN_IF_EMPTY_MSG(result, "Imsi is empty")
    } else {  // MSISDN should do something more
      nsCOMPtr<nsIIccInfo> iccInfo;
      RETURN_IF_ERROR_MSG(icc->GetIccInfo(getter_AddRefs(iccInfo)),
                          "Invalid nsIIccInfo")

      nsCOMPtr<nsIGsmIccInfo> gsmIccInfo = do_QueryInterface(iccInfo);
      if (gsmIccInfo) {
        RETURN_IF_ERROR_MSG(gsmIccInfo->GetMsisdn(result),
                            "Failed to get MSISDN")

        RETURN_IF_EMPTY_MSG(result, "MSISDN is empty")
      } else {
        nsCOMPtr<nsICdmaIccInfo> cdmaIccInfo = do_QueryInterface(iccInfo);
        RETURN_IF_NULL_MSG(cdmaIccInfo, "Invalid nsICdmaIccInfo")
        RETURN_IF_ERROR_MSG(cdmaIccInfo->GetMdn(result), "Failed to get MDN")

        RETURN_IF_EMPTY_MSG(result, "MDN is empty")
      }
    }  // end of IMSI and MSISDN
  }    // end of IMEI and IMSI and MSISDN

  return NS_OK;
}

NS_IMETHODIMP
MobileManagerDelegateService::GetMncMcc(int cardId, bool isSim, nsAString& mnc,
                                        nsAString& mcc) {
  if (!ValidateCardId(cardId)) {
    return NS_ERROR_FAILURE;
  }

  if (isSim) {
    // Get simcard mnc/mcc.
    nsCOMPtr<nsIIccService> iccService = do_GetService(ICC_SERVICE_CONTRACTID);
    RETURN_IF_NULL_MSG(iccService, "Invalid nsIIccService");

    nsCOMPtr<nsIIcc> icc;
    iccService->GetIccByServiceId(cardId, getter_AddRefs(icc));
    RETURN_IF_NULL_MSG(iccService, "Invalid nsIIcc");

    nsCOMPtr<nsIIccInfo> iccInfo;
    icc->GetIccInfo(getter_AddRefs(iccInfo));
    RETURN_IF_NULL_MSG(iccInfo, "Invalid nsIIccInfo");

    RETURN_IF_ERROR_MSG(iccInfo->GetMnc(mnc), "Failed to get MNC")
    RETURN_IF_ERROR_MSG(iccInfo->GetMcc(mcc), "Failed to get MCC")
  } else {
    // Get network mnc/mcc.
    nsCOMPtr<nsIMobileConnectionService> service =
        do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
    RETURN_IF_NULL_MSG(service, "Invalid nsIMobileConnectionService");

    nsCOMPtr<nsIMobileConnection> connection;
    service->GetItemByServiceId(cardId, getter_AddRefs(connection));
    RETURN_IF_NULL_MSG(connection, "Invalid nsIMobileConnection");

    nsCOMPtr<nsIMobileConnectionInfo> connectionInfo;
    connection->GetVoice(getter_AddRefs(connectionInfo));
    RETURN_IF_NULL_MSG(connectionInfo, "Invalid nsIMobileConnectionInfo");

    nsCOMPtr<nsIMobileNetworkInfo> networkInfo;
    connectionInfo->GetNetwork(getter_AddRefs(networkInfo));
    RETURN_IF_NULL_MSG(networkInfo, "Invalid nsIMobileNetworkInfo");

    RETURN_IF_ERROR_MSG(networkInfo->GetMnc(mnc), "Failed to get MNC");
    RETURN_IF_ERROR_MSG(networkInfo->GetMcc(mcc), "Failed to get MCC");
  }
  return NS_OK;
}

}  // namespace mozilla
