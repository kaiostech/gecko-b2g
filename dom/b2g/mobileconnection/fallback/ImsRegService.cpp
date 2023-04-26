/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImsRegService.h"
#include "nsIImsRegService.h"

namespace mozilla::dom::mobileconnection {

NS_IMPL_ISUPPORTS(FallbackImsRegService, nsIImsRegService)

FallbackImsRegService::~FallbackImsRegService() {}

// nsIImsRegService

NS_IMETHODIMP FallbackImsRegService::GetHandlerByServiceId(
    uint32_t aServiceId, nsIImsRegHandler** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP FallbackImsRegService::IsServiceReady(bool* retval) {
  *retval = false;
  return NS_OK;
}

}  // namespace mozilla::dom::mobileconnection
