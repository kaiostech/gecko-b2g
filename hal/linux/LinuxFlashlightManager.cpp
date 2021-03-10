/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// See https://xnux.eu/devices/feature/flash-pp.html#toc-pinephone-flash-led
// TODO: switch to v4l2 api.

#include <stdio.h>
#include <fcntl.h>
#include "mozilla/Preferences.h"

using namespace mozilla::hal;

namespace mozilla::hal_impl {

int GetFlashLightFd() {
    nsAutoCString sysfs;
    mozilla::Preferences::GetCString("hal.linux.flashlight.path", sysfs);
    return open(sysfs.get(), O_RDWR | O_CLOEXEC);
}

bool GetFlashlightEnabled() {
  auto fd = GetFlashLightFd();
  if (fd < 0) {
    NS_WARNING("Failed to open flashlight fd");
    return false;
  }

  char enabled = '0';
  Unused << read(fd, &enabled, 1);
  close(fd);

  return enabled == '1';
}

void SetFlashlightEnabled(bool aEnabled) {
  auto fd = GetFlashLightFd();
  if (fd < 0) {
    NS_WARNING("Failed to open flashlight fd");
    return;
  }

  Unused << write(fd, aEnabled ? "1\n" : "0\n", 2);
  Unused << fsync(fd);
  close(fd);

  hal::FlashlightInformation flashlightInfo;
  flashlightInfo.enabled() = aEnabled;
  flashlightInfo.present() = true;
  hal::UpdateFlashlightState(flashlightInfo);
}

bool IsFlashlightPresent() {
  auto fd = GetFlashLightFd();
  if (fd < 0) {
      return false;
  }
  close(fd);
  return true;
}

void RequestCurrentFlashlightState() {
  hal::FlashlightInformation flashlightInfo;
  flashlightInfo.enabled() = GetFlashlightEnabled();
  flashlightInfo.present() = true;
  hal::UpdateFlashlightState(flashlightInfo);
}

void EnableFlashlightNotifications() {
  printf_stderr("TODO: EnableFlashlightNotifications\n");
}

void DisableFlashlightNotifications() {
  printf_stderr("TODO: DisableFlashlightNotifications\n");
}

}  // namespace mozilla::hal_impl
