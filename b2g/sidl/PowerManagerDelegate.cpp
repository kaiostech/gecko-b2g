/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PowerManagerDelegate.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "mozilla/Hal.h"

using namespace mozilla;

namespace mozilla {

// The singleton PowerManagerDelegate service, to be used on the main thread.
static StaticRefPtr<PowerManagerDelegateService> gPowerManagerDelegateService;

NS_IMPL_ISUPPORTS(PowerManagerDelegateService, nsIPowerManagerDelegate)

PowerManagerDelegateService::PowerManagerDelegateService() {}

PowerManagerDelegateService::~PowerManagerDelegateService() {}

/* static */
already_AddRefed<PowerManagerDelegateService>
PowerManagerDelegateService::ConstructPowerManagerDelegate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gPowerManagerDelegateService) {
    gPowerManagerDelegateService = new PowerManagerDelegateService();
    ClearOnShutdown(&gPowerManagerDelegateService);
  }

  RefPtr<PowerManagerDelegateService> service =
      gPowerManagerDelegateService.get();
  return service.forget();
}

NS_IMETHODIMP
PowerManagerDelegateService::SetScreenEnabled(bool enabled,
                                              bool isExternalScreen) {
  if (isExternalScreen) {
    hal::SetExtScreenEnabled(enabled);
  } else {
    hal::SetScreenEnabled(enabled);
  }
  return NS_OK;
}

}  // namespace mozilla
