/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FallbackScreenConfiguration.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace hal_impl {

void EnableScreenConfigurationNotifications() {}

void DisableScreenConfigurationNotifications() {}

void GetCurrentScreenConfiguration(
    hal::ScreenConfiguration* aScreenConfiguration) {
  fallback::GetCurrentScreenConfiguration(aScreenConfiguration);
}

RefPtr<mozilla::MozPromise<bool, bool, false>> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  return mozilla::MozPromise<bool, bool, false>::CreateAndReject(false,
                                                                 __func__);
}

void UnlockScreenOrientation() {}

bool GetScreenEnabled() { return true; }

void SetScreenEnabled(bool aEnabled) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "screen-state-changed",
                                     aEnabled ? u"on" : u"off");
  }
}

bool GetExtScreenEnabled() { return true; }

void SetExtScreenEnabled(bool aEnabled) {}

}  // namespace hal_impl
}  // namespace mozilla
