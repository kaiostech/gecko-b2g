/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// See https://xnux.eu/devices/feature/flash-pp.html#toc-pinephone-flash-led
// TODO: switch to v4l2 api.

#include <stdio.h>
#include <fcntl.h>
#include "nsIB2gLinuxHal.h"

using namespace mozilla::hal;

namespace mozilla::hal_impl {

#define GET_LINUX_HAL                 \
  nsCOMPtr<nsIB2gLinuxHal> linuxHal = \
      do_GetService("@mozilla.org/hal/b2g-linux;1")

bool GetFlashlightEnabled() {
  GET_LINUX_HAL;

  bool state = false;
  linuxHal->FlashlightState(&state);
  return state;
}

void SetFlashlightEnabled(bool aEnabled) {
  GET_LINUX_HAL;

  if (aEnabled) {
    linuxHal->EnableFlashlight();
  } else {
    linuxHal->DisableFlashlight();
  }

  bool present = false;
  linuxHal->IsFlashlighSupported(&present);

  hal::FlashlightInformation flashlightInfo;
  flashlightInfo.enabled() = aEnabled;
  flashlightInfo.present() = present;
  hal::UpdateFlashlightState(flashlightInfo);
}

bool IsFlashlightPresent() {
  GET_LINUX_HAL;

  bool result = false;
  linuxHal->IsFlashlighSupported(&result);
  return result;
}

void RequestCurrentFlashlightState() {
  GET_LINUX_HAL;

  bool present = false;
  linuxHal->IsFlashlighSupported(&present);

  bool state = false;
  linuxHal->FlashlightState(&state);

  hal::FlashlightInformation flashlightInfo;
  flashlightInfo.enabled() = state;
  flashlightInfo.present() = present;
  hal::UpdateFlashlightState(flashlightInfo);
}

void EnableFlashlightNotifications() {
  printf_stderr("TODO: EnableFlashlightNotifications\n");
}

void DisableFlashlightNotifications() {
  printf_stderr("TODO: DisableFlashlightNotifications\n");
}

}  // namespace mozilla::hal_impl
