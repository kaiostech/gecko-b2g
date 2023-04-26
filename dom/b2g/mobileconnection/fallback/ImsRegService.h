/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_FallbackImsRegService_h
#define mozilla_dom_mobileconnection_FallbackImsRegService_h

#include "nsCOMPtr.h"
#include "nsIImsRegService.h"

namespace mozilla::dom::mobileconnection {

class FallbackImsRegService final : public nsIImsRegService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGSERVICE

 private:
  ~FallbackImsRegService();
};

}  // namespace mozilla::dom::mobileconnection

#endif  // mozilla_dom_mobileconnection_FallbackImsRegService_h
