/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>
#include <limits>
#include "BatteryManager.h"
#include "Constants.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/BatteryManagerBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Services.h"

/**
 * We have to use macros here because our leak analysis tool thinks we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define LEVELCHANGE_EVENT_NAME u"levelchange"_ns
#define CHARGINGCHANGE_EVENT_NAME u"chargingchange"_ns
#define DISCHARGINGTIMECHANGE_EVENT_NAME u"dischargingtimechange"_ns
#define CHARGINGTIMECHANGE_EVENT_NAME u"chargingtimechange"_ns
#define BATTERYHEALTHCHANGE_EVENT_NAME u"batteryhealthchange"_ns

namespace mozilla::dom::battery {

BatteryManager::BatteryManager(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mLevel(kDefaultLevel),
      mCharging(kDefaultCharging),
      mRemainingTime(kDefaultRemainingTime),
      mHealth(kDefaultHealth) {}

void BatteryManager::Init() {
  hal::RegisterBatteryObserver(this);

  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);

  UpdateFromBatteryInfo(batteryInfo);
}

void BatteryManager::Shutdown() { hal::UnregisterBatteryObserver(this); }

JSObject* BatteryManager::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return BatteryManager_Binding::Wrap(aCx, this, aGivenProto);
}

bool BatteryManager::Charging() const {
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return true;
  }
  if (Preferences::GetBool("dom.battery.test.charging", false)) {
    return true;
  }
  if (Preferences::GetBool("dom.battery.test.discharging", false)) {
    return false;
  }

  return mCharging;
}

double BatteryManager::DischargingTime() const {
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return std::numeric_limits<double>::infinity();
  }
  if (Preferences::GetBool("dom.battery.test.discharging", false)) {
    return 42.0;
  }

  if (Charging() || mRemainingTime == kUnknownRemainingTime) {
    return std::numeric_limits<double>::infinity();
  }

  return mRemainingTime;
}

double BatteryManager::ChargingTime() const {
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return 0.0;
  }
  if (Preferences::GetBool("dom.battery.test.charging", false)) {
    return 42.0;
  }

  if (!Charging() || mRemainingTime == kUnknownRemainingTime) {
    return std::numeric_limits<double>::infinity();
  }

  return mRemainingTime;
}

double BatteryManager::Level() const {
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default")) {
    return 1.0;
  } else if (Preferences::GetBool("dom.battery.test.dummy_battery_level",
                                  false)) {
    return 0.7;
  }

  return mLevel;
}

double BatteryManager::Temperature() const {
  MOZ_ASSERT(NS_IsMainThread());
  if (Preferences::GetBool("dom.battery.test.dummy_thermal_status", false)) {
    return kDefaultTemperature;
  }

  return hal::GetBatteryTemperature();
}

BatteryHealth BatteryManager::Health() const {
  MOZ_ASSERT(NS_IsMainThread());
  if (Preferences::GetBool("dom.battery.test.dummy_thermal_status", false)) {
    return BatteryHealth::Good;
  }

  return mHealth;
}

bool BatteryManager::Present() const {
  MOZ_ASSERT(NS_IsMainThread());
  return hal::IsBatteryPresent();
}

void BatteryManager::UpdateFromBatteryInfo(
    const hal::BatteryInformation& aBatteryInfo) {
  mLevel = aBatteryInfo.level();

  // On B2G, don't use round off algorithm. Only chrome process and core apps
  // can access battery API, they should get percise battery information.
#ifndef MOZ_B2G
  // Round to the nearest ten percent for non-chrome.
  Document* doc = GetOwner() ? GetOwner()->GetDoc() : nullptr;
#endif

  mCharging = aBatteryInfo.charging();
  mRemainingTime = aBatteryInfo.remainingTime();
  mHealth = aBatteryInfo.health();

#ifndef MOZ_B2G
  if (!nsContentUtils::IsChromeDoc(doc)) {
    mLevel = lround(mLevel * 10.0) / 10.0;
    if (mLevel == 1.0) {
      mRemainingTime =
          mCharging ? kDefaultRemainingTime : kUnknownRemainingTime;
    } else if (mRemainingTime != kUnknownRemainingTime) {
      // Round the remaining time to a multiple of 15 minutes and never zero
      const double MINUTES_15 = 15.0 * 60.0;
      mRemainingTime =
          fmax(lround(mRemainingTime / MINUTES_15) * MINUTES_15, MINUTES_15);
    }
  }
#endif

  // Add some guards to make sure the values are coherent.
  if (mLevel == 1.0 && mCharging == true &&
      mRemainingTime != kDefaultRemainingTime) {
    mRemainingTime = kDefaultRemainingTime;
    NS_ERROR(
        "Battery API: When charging and level at 1.0, remaining time "
        "should be 0. Please fix your backend!");
  }
}

void BatteryManager::Notify(const hal::BatteryInformation& aBatteryInfo) {
  double previousLevel = mLevel;
  bool previousCharging = mCharging;
  double previousRemainingTime = mRemainingTime;
  BatteryHealth previousHealth = mHealth;

  UpdateFromBatteryInfo(aBatteryInfo);

  if (previousCharging != mCharging) {
    DispatchTrustedEvent(CHARGINGCHANGE_EVENT_NAME);
  }

  /*
   * Don't fire levelchange event if dom.battery.test.dummy_battery_level is
   * true.
   */
  if (!Preferences::GetBool("dom.battery.test.dummy_battery_level", false) &&
      previousLevel != mLevel) {
    DispatchTrustedEvent(LEVELCHANGE_EVENT_NAME);
  }

  /*
   * Don't fire batteryhealthchange event if
   * dom.battery.test.dummy_thermal_status is true.
   */
  if (!Preferences::GetBool("dom.battery.test.dummy_thermal_status", false) &&
      previousHealth != mHealth) {
    DispatchTrustedEvent(BATTERYHEALTHCHANGE_EVENT_NAME);
  }
  /*
   * There are a few situations that could happen here:
   * 1. Charging state changed:
   *   a. Previous remaining time wasn't unkwonw, we have to fire an event for
   *      the change.
   *   b. New remaining time isn't unkwonw, we have to fire an event for it.
   * 2. Charging state didn't change but remainingTime did, we have to fire
   *    the event that correspond to the current charging state.
   * 3. If dom.battery.test.dummy_battery_level is true, won't dispatch event
   *    regarding estimated charging/discharging time.
   */
  if (!Preferences::GetBool("dom.battery.test.dummy_battery_level", false) &&
      mCharging != previousCharging) {
    if (previousRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEvent(previousCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                            : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
    if (mRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEvent(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                     : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
  } else if (previousRemainingTime != mRemainingTime) {
    DispatchTrustedEvent(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                   : DISCHARGINGTIMECHANGE_EVENT_NAME);
  }
}

// static
bool BatteryManager::HasPermission(JSContext* aContext, JSObject* aGlobal) {
  if (nsContentUtils::IsSystemCaller(aContext)) {
    return true;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal);
  if (NS_WARN_IF(!principal)) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
      mozilla::services::GetPermissionManager();
  if (NS_WARN_IF(!permissionManager)) {
    return false;
  }

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  permissionManager->TestExactPermissionFromPrincipal(principal, "battery"_ns,
                                                      &permission);

  if (permission == nsIPermissionManager::ALLOW_ACTION ||
      permission == nsIPermissionManager::PROMPT_ACTION) {
    return true;
  }

  return false;
}
}  // namespace mozilla::dom::battery
