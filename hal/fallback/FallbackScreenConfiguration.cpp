/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "nsIObserverService.h"

namespace mozilla::hal_impl {

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  return GenericNonExclusivePromise::CreateAndReject(
      NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
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

}  // namespace mozilla::hal_impl
