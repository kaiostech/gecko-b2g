/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#if ANDROID_VERSION >= 33
#  define AIDL_GNSS
#  define GNSS android::hardware::gnss
#endif

#include "mozilla/dom/GeolocationPosition.h"
#include "nsThreadUtils.h"

class HidlUpdateCapabilitiesEvent final : public mozilla::Runnable {
 public:
  explicit HidlUpdateCapabilitiesEvent(uint32_t aCapabilities)
      : mozilla::Runnable("UpdateCapabilitiesEvent"),
        mCapabilities(aCapabilities) {}
  NS_IMETHOD Run() override;

 private:
  uint32_t mCapabilities;
};

#ifdef AIDL_GNSS
class AidlUpdateCapabilitiesEvent final : public mozilla::Runnable {
 public:
  explicit AidlUpdateCapabilitiesEvent(uint32_t aCapabilities)
      : mozilla::Runnable("UpdateCapabilitiesEvent"),
        mCapabilities(aCapabilities) {}
  NS_IMETHOD Run() override;

 private:
  uint32_t mCapabilities;
};
#endif  // AIDL_GNSS

class UpdateLocationEvent final : public mozilla::Runnable {
 public:
  explicit UpdateLocationEvent(nsGeoPosition* aPosition)
      : mozilla::Runnable("UpdateLocationEvent"), mPosition(aPosition) {}
  NS_IMETHOD Run() override;

 private:
  RefPtr<nsGeoPosition> mPosition;
};
