/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <regex.h>
#include <sched.h>
#include <stdio.h>
#include <sys/klog.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "mozilla/DebugOnly.h"

#include "android/hardware/BnCameraServiceListener.h"
#include "android/hardware/ICameraService.h"
#include "android/hardware/ICameraServiceListener.h"
#include "binder/IServiceManager.h"
#include "android/log.h"
#include "hardware/hardware.h"
#include "hardware/lights.h"
#include "hardware_legacy/uevent.h"
#include "android/hardware/vibrator/1.0/IVibrator.h"
#if ANDROID_VERSION == 30
#include "android/hardware/vibrator/IVibrator.h"
#endif
#include "libdisplay/GonkDisplay.h"

#include "utils/threads.h"

#include "base/message_loop.h"
#include "base/task.h"

#include "Hal.h"
#include "HalImpl.h"
#include "HalLog.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/dom/NetworkInformationBinding.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsAlgorithm.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsINetworkInterface.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
// TODO: FIXME #include "nsIRecoveryService.h"
#include "nsIRunnable.h"
#include "ScreenHelperGonk.h"
#include "SystemProperty.h"
#include "nsThreadUtils.h"
#include "nsThreadUtils.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"
#include "OrientationObserver.h"
#include "UeventPoller.h"
#include "nsIWritablePropertyBag2.h"
#include <algorithm>
#include <dlfcn.h>

#define NsecPerMsec 1000000LL
#define NsecPerSec 1000000000

// The header linux/oom.h is not available in bionic libc. We
// redefine some of its constants here.

#ifndef OOM_DISABLE
#  define OOM_DISABLE (-17)
#endif

#ifndef OOM_ADJUST_MIN
#  define OOM_ADJUST_MIN (-16)
#endif

#ifndef OOM_ADJUST_MAX
#  define OOM_ADJUST_MAX 15
#endif

#ifndef OOM_SCORE_ADJ_MIN
#  define OOM_SCORE_ADJ_MIN (-1000)
#endif

#ifndef OOM_SCORE_ADJ_MAX
#  define OOM_SCORE_ADJ_MAX 1000
#endif

#define LED_GREEN_BRIGHTNESS "/sys/class/leds/green/brightness"
#define LED_RED_BRIGHTNESS "/sys/class/leds/red/brightness"
#define MAXIMUM_BRIGHTNESS "255"
#define MINIMUM_BRIGHTNESS "0"
#define POWER_SUPPLY_SUBSYSTEM "power_supply"
#define POWER_SUPPLY_SYSFS_PATH "/sys/class/" POWER_SUPPLY_SUBSYSTEM
#define POWER_SUPPLY_TYPE_AC 0x01
#define POWER_SUPPLY_TYPE_USB 0x02
#define POWER_SUPPLY_TYPE_WIRELESS 0x04

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;

#if ANDROID_VERSION == 30
namespace Aidl = ::android::hardware::vibrator;
#endif

namespace mozilla {
namespace hal_impl {

namespace {

/**
 * This runnable runs for the lifetime of the program, once started.  It's
 * responsible for "playing" vibration patterns.
 */
class VibratorRunnable final : public nsIRunnable, public nsIObserver {
 public:
  VibratorRunnable() : mMonitor("VibratorRunnable"), mIndex(0) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      NS_WARNING("Could not get observer service!");
      return;
    }

    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  // Run on the main thread, not the vibrator thread.
  void Vibrate(const nsTArray<uint32_t>& pattern);
  void CancelVibrate();

  static bool ShuttingDown() { return sShuttingDown; }

 protected:
  ~VibratorRunnable() {}

 private:
  Monitor mMonitor;

  // The currently-playing pattern.
  nsTArray<uint32_t> mPattern;

  // The index we're at in the currently-playing pattern.  If mIndex >=
  // mPattern.Length(), then we're not currently playing anything.
  uint32_t mIndex;

  // Set to true in our shutdown observer.  When this is true, we kill the
  // vibrator thread.
  static bool sShuttingDown;
};

NS_IMPL_ISUPPORTS(VibratorRunnable, nsIRunnable, nsIObserver);

bool VibratorRunnable::sShuttingDown = false;

static StaticRefPtr<VibratorRunnable> sVibratorRunnable;

// Helper class to abstract over HIDL and AIDL implementations.
class HalVibrator {
  public:
    HalVibrator() {
      // Get a handle to the HIDL vibrator service.
      hidl = android::hardware::vibrator::V1_0::IVibrator::getService();

#if ANDROID_VERSION == 30
      // If we can't get the HIDL vibrator service, try the AIDL service.
      if (!hidl) {
        aidl = android::waitForVintfService<Aidl::IVibrator>();
      }

      if (!hidl && aidl) {
        printf_stderr("Failed to run vibration pattern: vibrator == nullptr");
      }
#endif
    }

    void off() {
      if (hidl) {
        hidl->off();
      }
#if ANDROID_VERSION == 30
     else if (aidl) {
        aidl->off();
      }
#endif
    }

    void on(int32_t ms) {
      if (hidl) {
        hidl->on(ms);
      }
#if ANDROID_VERSION == 30
      else if (aidl) {
        aidl->on(ms, nullptr);
      }
#endif
    }

  private:
#if ANDROID_VERSION == 30
    android::sp<Aidl::IVibrator> aidl = nullptr;
#endif
    android::sp<android::hardware::vibrator::V1_0::IVibrator> hidl = nullptr;
};

NS_IMETHODIMP
VibratorRunnable::Run() {
  MonitorAutoLock lock(mMonitor);

  // We currently assume that mMonitor.Wait(X) waits for X milliseconds.  But in
  // reality, the kernel might not switch to this thread for some time after the
  // wait expires.  So there's potential for some inaccuracy here.
  //
  // This doesn't worry me too much.  Note that we don't even start vibrating
  // immediately when VibratorRunnable::Vibrate is called -- we go through a
  // condvar onto another thread.  Better just to be chill about small errors in
  // the timing here.

  HalVibrator vibrator;

  while (!sShuttingDown) {
    if (mIndex < mPattern.Length()) {
      uint32_t duration = mPattern[mIndex];
      if ((mPattern.Length() == 1) && (duration == 0)) {
        vibrator.off();
      } else if (mIndex % 2 == 0) {
        vibrator.on(duration);
      }
      mIndex++;
      mMonitor.Wait(TimeDuration::FromMilliseconds(duration));
    } else {
      mMonitor.Wait();
    }
  }
  sVibratorRunnable = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports* subject, const char* topic,
                          const char16_t* data) {
  MOZ_ASSERT(strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  MonitorAutoLock lock(mMonitor);
  sShuttingDown = true;
  mMonitor.Notify();

  return NS_OK;
}

void VibratorRunnable::Vibrate(const nsTArray<uint32_t>& pattern) {
  MonitorAutoLock lock(mMonitor);
  mPattern.Assign(pattern);
  mIndex = 0;
  mMonitor.Notify();
}

void VibratorRunnable::CancelVibrate() {
  MonitorAutoLock lock(mMonitor);
  mPattern.Clear();
  mPattern.AppendElement(0);
  mIndex = 0;
  mMonitor.Notify();
}

void EnsureVibratorThreadInitialized() {
  if (sVibratorRunnable) {
    return;
  }

  sVibratorRunnable = new VibratorRunnable();
  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("Vibrator", getter_AddRefs(thread), sVibratorRunnable);
}

}  // namespace

void Vibrate(const nsTArray<uint32_t>& pattern, hal::WindowIdentifier&&) {
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->Vibrate(pattern);
}

void CancelVibrate(hal::WindowIdentifier&&) {
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->CancelVibrate();
}

namespace {

class BatteryUpdater : public Runnable {
 public:
  explicit BatteryUpdater() : Runnable("hal::BatteryUpdater") {}
  NS_IMETHOD Run() override {
    hal::BatteryInformation info;
    hal_impl::GetCurrentBatteryInformation(&info);

    // Control the battery indicator (led light) here using BatteryInformation
    // we just retrieved.
    char red_path[system::Property::VALUE_MAX_LENGTH];
    char green_path[system::Property::VALUE_MAX_LENGTH];

    system::Property::Get("leds.red.brightness", red_path, LED_RED_BRIGHTNESS);
    system::Property::Get("leds.red.brightness", green_path,
                          LED_GREEN_BRIGHTNESS);

    if (info.charging() && (info.level() == 1)) {
      // Charging and battery full, it's green.
      WriteSysFile(green_path, MAXIMUM_BRIGHTNESS);
      WriteSysFile(red_path, MINIMUM_BRIGHTNESS);
    } else if (info.charging() && (info.level() < 1)) {
      // Charging but not full, it's red.
      WriteSysFile(green_path, MINIMUM_BRIGHTNESS);
      WriteSysFile(red_path, MAXIMUM_BRIGHTNESS);
    } else {
      // else turn off battery indicator.
      WriteSysFile(green_path, MINIMUM_BRIGHTNESS);
      WriteSysFile(red_path, MINIMUM_BRIGHTNESS);
    }

    hal::NotifyBatteryChange(info);

    {
      // bug 975667
      // Gecko gonk hal is required to emit battery charging/level notification
      // via nsIObserverService. This is useful for XPCOM components that are
      // not statically linked to Gecko and cannot call
      // hal::EnableBatteryNotifications
      nsCOMPtr<nsIObserverService> obsService =
          mozilla::services::GetObserverService();
      nsCOMPtr<nsIWritablePropertyBag2> propbag =
          do_CreateInstance("@mozilla.org/hash-property-bag;1");
      if (obsService && propbag) {
        propbag->SetPropertyAsBool(u"charging"_ns, info.charging());
        propbag->SetPropertyAsDouble(u"level"_ns, info.level());

        obsService->NotifyObservers(propbag, "gonkhal-battery-notifier",
                                    nullptr);
      }
    }

    return NS_OK;
  }
};

}  // namespace

class BatteryObserver final : public IUeventObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(BatteryObserver)

  BatteryObserver() : mUpdater(new BatteryUpdater()) {}

  virtual void Notify(const NetlinkEvent& aEvent) {
    // this will run on IO thread
    NetlinkEvent* event = const_cast<NetlinkEvent*>(&aEvent);
    const char* subsystem = event->getSubsystem();
    // e.g. DEVPATH=/devices/platform/sec-battery/power_supply/battery
    const char* devpath = event->findParam("DEVPATH");
    if (strcmp(subsystem, "power_supply") == 0 && strstr(devpath, "battery")) {
      // aEvent will be valid only in this method.
      NS_DispatchToMainThread(mUpdater);
    }
  }

 protected:
  ~BatteryObserver() {}

 private:
  RefPtr<BatteryUpdater> mUpdater;
};

// sBatteryObserver is owned by the IO thread. Only the IO thread may
// create or destroy it.
static StaticRefPtr<BatteryObserver> sBatteryObserver;

static void RegisterBatteryObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sBatteryObserver);

  sBatteryObserver = new BatteryObserver();
  RegisterUeventListener(sBatteryObserver);
}

void EnableBatteryNotifications() {
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "RegisterBatteryObserver", RegisterBatteryObserverIOThread));
}

static void UnregisterBatteryObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(sBatteryObserver);

  UnregisterUeventListener(sBatteryObserver);
  sBatteryObserver = nullptr;
}

static BatteryHealth GetCurrentBatteryHealth() {
  char health[16] = {0};
  bool success;

  // Get battery health and set to "Unknown" if read fails
  success = ReadSysFile("/sys/class/power_supply/battery/health", health,
                        sizeof(health));
  if (success) {
    if (!strcmp(health, "Good")) {
      return BatteryHealth::Good;
    } else if (!strcmp(health, "Overheat")) {
      return BatteryHealth::Overheat;
    } else if (!strcmp(health, "Cold")) {
      return BatteryHealth::Cold;
    } else if (!strcmp(health, "Warm")) {
      return BatteryHealth::Warm;
    } else if (!strcmp(health, "Cool")) {
      return BatteryHealth::Cool;
    } else {
      return BatteryHealth::Unknown;
    }
  } else {
    return dom::battery::kDefaultHealth;
  }
}

void DisableBatteryNotifications() {
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "UnregisterBatteryObserver", UnregisterBatteryObserverIOThread));
}

static bool GetCurrentBatteryCharge(int* aCharge) {
  bool success =
      ReadSysFile("/sys/class/power_supply/battery/capacity", aCharge);
  if (!success) {
    return false;
  }

#ifdef DEBUG
  if ((*aCharge < 0) || (*aCharge > 100)) {
    HAL_LOG("charge level contains unknown value: %d", *aCharge);
  }
#endif

  return (*aCharge >= 0) && (*aCharge <= 100);
}

static bool GetCurrentBatteryCharging(int* aCharging) {
  static const int BATTERY_NOT_CHARGING = 0;
  static const int BATTERY_CHARGING_USB = 1;
  static const int BATTERY_CHARGING_AC = 2;

  // Generic device support

  int chargingSrc;
  bool success = ReadSysFile("/sys/class/power_supply/battery/charging_source",
                             &chargingSrc);

  if (success) {
#ifdef DEBUG
    if (chargingSrc != BATTERY_NOT_CHARGING &&
        chargingSrc != BATTERY_CHARGING_USB &&
        chargingSrc != BATTERY_CHARGING_AC) {
      HAL_LOG("charging_source contained unknown value: %d", chargingSrc);
    }
#endif

    *aCharging = (chargingSrc == BATTERY_CHARGING_USB ||
                  chargingSrc == BATTERY_CHARGING_AC);
    return true;
  }

  // Otoro device support

  char chargingSrcString[16];

  success = ReadSysFile("/sys/class/power_supply/battery/status",
                        chargingSrcString, sizeof(chargingSrcString));
  if (success) {
    *aCharging = strcmp(chargingSrcString, "Charging") == 0 ||
                 strcmp(chargingSrcString, "Full") == 0;
    return true;
  }

  return false;
}

double GetBatteryTemperature() {
  int temperature;
  bool success =
      ReadSysFile("/sys/class/power_supply/battery/temp", &temperature);

  return success ? (double)temperature / 10.0
                 : dom::battery::kDefaultTemperature;
  ;
}

bool IsBatteryPresent() {
  bool present;
  bool success =
      ReadSysFile("/sys/class/power_supply/battery/present", &present);

  return success ? present : dom::battery::kDefaultPresent;
}

void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo) {
  int charge;
  static bool previousCharging = false;
  static double previousLevel = 0.0, remainingTime = 0.0;
  static struct timespec lastLevelChange;
  struct timespec now;
  double dtime, dlevel;

  if (GetCurrentBatteryCharge(&charge)) {
    aBatteryInfo->level() = (double)charge / 100.0;
  } else {
    aBatteryInfo->level() = dom::battery::kDefaultLevel;
  }

  aBatteryInfo->temperature() = GetBatteryTemperature();
  aBatteryInfo->health() = GetCurrentBatteryHealth();
  aBatteryInfo->present() = IsBatteryPresent();

  int charging;

  if (GetCurrentBatteryCharging(&charging)) {
    aBatteryInfo->charging() = charging;
  } else {
    aBatteryInfo->charging() = true;
  }

  if (aBatteryInfo->charging() != previousCharging) {
    aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
    memset(&lastLevelChange, 0, sizeof(struct timespec));
    remainingTime = 0.0;
  }

  if (aBatteryInfo->charging()) {
    if (aBatteryInfo->level() == 1.0) {
      aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
    } else if (aBatteryInfo->level() != previousLevel) {
      if (lastLevelChange.tv_sec != 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        dtime = now.tv_sec - lastLevelChange.tv_sec;
        dlevel = aBatteryInfo->level() - previousLevel;

        if (dlevel <= 0.0) {
          aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
        } else {
          remainingTime =
              (double)round(dtime / dlevel * (1.0 - aBatteryInfo->level()));
          aBatteryInfo->remainingTime() = remainingTime;
        }

        lastLevelChange = now;
      } else {  // lastLevelChange.tv_sec == 0
        clock_gettime(CLOCK_MONOTONIC, &lastLevelChange);
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    } else {
      clock_gettime(CLOCK_MONOTONIC, &now);
      dtime = now.tv_sec - lastLevelChange.tv_sec;
      if (dtime < remainingTime) {
        aBatteryInfo->remainingTime() = round(remainingTime - dtime);
      } else {
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }
    }

  } else {
    if (aBatteryInfo->level() == 0.0) {
      aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
    } else if (aBatteryInfo->level() != previousLevel) {
      if (lastLevelChange.tv_sec != 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        dtime = now.tv_sec - lastLevelChange.tv_sec;
        dlevel = previousLevel - aBatteryInfo->level();

        if (dlevel <= 0.0) {
          aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
        } else {
          remainingTime = (double)round(dtime / dlevel * aBatteryInfo->level());
          aBatteryInfo->remainingTime() = remainingTime;
        }

        lastLevelChange = now;
      } else {  // lastLevelChange.tv_sec == 0
        clock_gettime(CLOCK_MONOTONIC, &lastLevelChange);
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    } else {
      clock_gettime(CLOCK_MONOTONIC, &now);
      dtime = now.tv_sec - lastLevelChange.tv_sec;
      if (dtime < remainingTime) {
        aBatteryInfo->remainingTime() = round(remainingTime - dtime);
      } else {
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }
    }
  }

  previousCharging = aBatteryInfo->charging();
  previousLevel = aBatteryInfo->level();
}

namespace {

class UsbUpdater : public Runnable {
 public:
  explicit UsbUpdater() : Runnable("hal::UsbUpdater") {}
  NS_IMETHOD Run() {
    hal::UsbStatus info;
    hal_impl::GetCurrentUsbStatus(&info);

    hal::NotifyUsbStatus(info);

    {
      nsCOMPtr<nsIObserverService> obsService =
          mozilla::services::GetObserverService();
      nsCOMPtr<nsIWritablePropertyBag2> propbag =
          do_CreateInstance("@mozilla.org/hash-property-bag;1");
      if (obsService && propbag) {
        propbag->SetPropertyAsBool(u"deviceAttached"_ns, info.deviceAttached());
        propbag->SetPropertyAsBool(u"deviceConfigured"_ns,
                                   info.deviceConfigured());
        obsService->NotifyObservers(propbag, "gonkhal-usb-notifier", nullptr);
      }
    }
    return NS_OK;
  }
};

}  // anonymous namespace

class UsbObserver final : public IUeventObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(UsbObserver)

  UsbObserver() : mUpdater(new UsbUpdater()) {}

  virtual void Notify(const NetlinkEvent& aEvent) {
    // this will run on IO thread
    NetlinkEvent* event = const_cast<NetlinkEvent*>(&aEvent);
    const char* subsystem = event->getSubsystem();
    // e.g. DEVPATH=/devices/virtual/android_usb/android0
    const char* devpath = event->findParam("DEVPATH");
    if (strcmp(subsystem, "android_usb") == 0 &&
        strstr(devpath, "android_usb")) {
      // aEvent will be valid only in this method.
      NS_DispatchToMainThread(mUpdater);
    }
  }

 protected:
  ~UsbObserver() {}

 private:
  RefPtr<UsbUpdater> mUpdater;
};

// sUsbObserver is owned by the IO thread. Only the IO thread may
// create or destroy it.
static StaticRefPtr<UsbObserver> sUsbObserver;

static void RegisterUsbObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sUsbObserver);

  sUsbObserver = new UsbObserver();
  RegisterUeventListener(sUsbObserver);
}

void EnableUsbNotifications() {
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "RegisterUsbObserverIOThread", RegisterUsbObserverIOThread));
}

static void UnregisterUsbObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(sUsbObserver);

  UnregisterUeventListener(sUsbObserver);
  sUsbObserver = nullptr;
}

void DisableUsbNotifications() {
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "UnregisterUsbObserverIOThread", UnregisterUsbObserverIOThread));
}

void GetCurrentUsbStatus(hal::UsbStatus* aUsbStatus) {
  bool success;
  char usbStateString[16];
  // see http://androidxref.com/6.0.1_r10/xref/frameworks/base/services/
  //            usb/java/com/android/server/usb/UsbDeviceManager.java#95
  success = ReadSysFile("/sys/class/android_usb/android0/state", usbStateString,
                        sizeof(usbStateString));
  if (success) {
    if (strcmp(usbStateString, "CONNECTED") == 0) {
      aUsbStatus->deviceAttached() = true;
      aUsbStatus->deviceConfigured() = false;
    } else if (strcmp(usbStateString, "CONFIGURED") == 0) {
      aUsbStatus->deviceAttached() = true;
      aUsbStatus->deviceConfigured() = true;
    } else if (strcmp(usbStateString, "DISCONNECTED") == 0) {
      aUsbStatus->deviceAttached() = false;
      aUsbStatus->deviceConfigured() = false;
    } else {
      HAL_ERR("Unexpected usb state : %s ", usbStateString);
    }
  }
}

namespace {

class PowerSupplyUpdater : public Runnable {
 public:
  explicit PowerSupplyUpdater() : Runnable("hal::PowerSupplyUpdater") {}
  NS_IMETHOD Run() {
    hal::PowerSupplyStatus info;
    hal_impl::GetCurrentPowerSupplyStatus(&info);

    hal::NotifyPowerSupplyStatus(info);

    {
      nsCOMPtr<nsIObserverService> obsService =
          mozilla::services::GetObserverService();
      nsCOMPtr<nsIWritablePropertyBag2> propbag =
          do_CreateInstance("@mozilla.org/hash-property-bag;1");
      if (obsService && propbag) {
        propbag->SetPropertyAsBool(u"powerSupplyOnline"_ns,
                                   info.powerSupplyOnline());
        propbag->SetPropertyAsACString(u"powerSupplyType"_ns,
                                       info.powerSupplyType());
        obsService->NotifyObservers(propbag, "gonkhal-powersupply-notifier",
                                    nullptr);
      }
    }
    return NS_OK;
  }
};

}  // anonymous namespace

class PowerSupplyObserver : public IUeventObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(PowerSupplyObserver)

  PowerSupplyObserver() : mUpdater(new PowerSupplyUpdater()) {}

  virtual void Notify(const NetlinkEvent& aEvent) {
    // this will run on IO thread
    NetlinkEvent* event = const_cast<NetlinkEvent*>(&aEvent);
    const char* subsystem = event->getSubsystem();
    const char* devpath = event->findParam("DEVPATH");
    // e.g. DEVPATH=/devices/soc.0/78d9000.usb/power_supply/usb
    if (strcmp(subsystem, "power_supply") == 0 && strstr(devpath, "battery")) {
      // aEvent will be valid only in this method.
      NS_DispatchToMainThread(mUpdater);
    }
  }

 protected:
  ~PowerSupplyObserver() {}

 private:
  RefPtr<PowerSupplyUpdater> mUpdater;
};

// sPowerSupplyObserver is owned by the IO thread. Only the IO thread may
// create or destroy it.
static StaticRefPtr<PowerSupplyObserver> sPowerSupplyObserver;

static void RegisterPowerSupplyObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sPowerSupplyObserver);

  sPowerSupplyObserver = new PowerSupplyObserver();
  RegisterUeventListener(sPowerSupplyObserver);
}

void EnablePowerSupplyNotifications() {
  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction("RegisterPowerSupplyObserverIOThread",
                          RegisterPowerSupplyObserverIOThread));
}

static void UnregisterPowerSupplyObserverIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(sPowerSupplyObserver);

  UnregisterUeventListener(sPowerSupplyObserver);
  sPowerSupplyObserver = nullptr;
}

void DisablePowerSupplyNotifications() {
  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction("UnregisterPowerSupplyObserverIOThread",
                          UnregisterPowerSupplyObserverIOThread));
}

void UpdatePowerSupplyType(const char* powerType, int& powerSupplyOnline) {
  // Skip Unknown/Battery type because that means no powerSupply attached.
  if (!strcmp(powerType, "UPS") || !strcmp(powerType, "Mains") ||
      !strcmp(powerType, "USB_DCP") || !strcmp(powerType, "USB_CDP") ||
      !strcmp(powerType, "USB_ACA") || !strcmp(powerType, "USB_HVDCP") ||
      !strcmp(powerType, "USB_HVDCP_3")) {
    powerSupplyOnline |= (1 << 0);
  } else if (!strcmp(powerType, "USB")) {
    powerSupplyOnline |= (1 << 1);
  } else if (!strcmp(powerType, "Wireless") || !strcmp(powerType, "Wipower")) {
    powerSupplyOnline |= (1 << 2);
  }
}

const char* ConvertPowerSupplyType(int powerSupplyOnline) {
  if ((powerSupplyOnline & POWER_SUPPLY_TYPE_AC) == POWER_SUPPLY_TYPE_AC) {
    return "AC";
  } else if ((powerSupplyOnline & POWER_SUPPLY_TYPE_USB) ==
             POWER_SUPPLY_TYPE_USB) {
    return "USB";
  } else if ((powerSupplyOnline & POWER_SUPPLY_TYPE_WIRELESS) ==
             POWER_SUPPLY_TYPE_WIRELESS) {
    return "Wireless";
  } else {
    return "Unknown";
  }
}

void GetCurrentPowerSupplyStatus(hal::PowerSupplyStatus* aPowerSupplyStatus) {
  bool success;
  char powerOnline[16];
  char powerType[16];
  // Bit mask for power supply source.
  int powerSupplyOnline = 0;

  // see http://androidxref.com/6.0.1_r10/xref/system/core/healthd/
  //            BatteryMonitor.cpp#419
  DIR* dir = opendir(POWER_SUPPLY_SYSFS_PATH);
  if (dir == NULL) {
    HAL_ERR("Could not open %s\n", POWER_SUPPLY_SYSFS_PATH);
  } else {
    struct dirent* entry;
    while ((entry = readdir(dir))) {
      const char* name = entry->d_name;

      if (!strcmp(name, ".") || !strcmp(name, "..")) {
        continue;
      }

      // Look for "type" file in each subdirectory
      nsPrintfCString type("%s/%s/type", POWER_SUPPLY_SYSFS_PATH, name);
      success = ReadSysFile(type.get(), powerType, sizeof(powerType));
      if (!success) {
        continue;
      }
      HAL_LOG("power type: %s", powerType);
      nsPrintfCString online("%s/%s/online", POWER_SUPPLY_SYSFS_PATH, name);
      success = ReadSysFile(online.get(), powerOnline, sizeof(powerOnline));

      if (!success) {
        continue;
      }
      HAL_LOG("powerOnline: %s", powerOnline);
      // Don't update powerSupplyOnline bitmask if power supply is offline.
      if (!atoi(powerOnline)) {
        continue;
      }

      UpdatePowerSupplyType(powerType, powerSupplyOnline);
    }

    closedir(dir);

    aPowerSupplyStatus->powerSupplyOnline() =
        (powerSupplyOnline > 0 ? true : false);
    aPowerSupplyStatus->powerSupplyType() =
        ConvertPowerSupplyType(powerSupplyOnline);
  }
}

NetworkInformation sNetworkInfo(int32_t(mozilla::dom::ConnectionType::None), 0,
                                0);

void EnableNetworkNotifications() {}

void DisableNetworkNotifications() {}

void SetNetworkType(int32_t aType) { sNetworkInfo.type() = aType; }

void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo) {
  *aNetworkInfo = sNetworkInfo;
}

namespace {

// We can write to screenEnabledFilename to enable/disable the screen, but when
// we read, we always get "mem"!  So we have to keep track ourselves whether
// the screen is on or not.
bool sScreenEnabled = true;
bool sExtScreenEnabled = true;

// We can read wakeLockFilename to find out whether the cpu wake lock
// is already acquired, but reading and parsing it is a lot more work
// than tracking it ourselves, and it won't be accurate anyway (kernel
// internal wake locks aren't counted here.)
bool sCpuSleepAllowed = true;

// Some CPU wake locks may be acquired internally in HAL. We use a counter to
// keep track of these needs. Note we have to hold |sInternalLockCpuMutex|
// when reading or writing this variable to ensure thread-safe.
int32_t sInternalLockCpuCount = 0;

}  // namespace

bool GetScreenEnabled() { return sScreenEnabled; }

void SetScreenEnabled(bool aEnabled) {
  GetGonkDisplay()->SetEnabled(aEnabled);
  sScreenEnabled = aEnabled;
}

bool GetExtScreenEnabled() { return sExtScreenEnabled; }

void SetExtScreenEnabled(bool aEnabled) {
  GetGonkDisplay()->SetExtEnabled(aEnabled);
  sExtScreenEnabled = aEnabled;
}

static StaticMutex sInternalLockCpuMutex;

static void UpdateCpuSleepState() {
  const char* wakeLockFilename = "/sys/power/wake_lock";
  const char* wakeUnlockFilename = "/sys/power/wake_unlock";

  sInternalLockCpuMutex.AssertCurrentThreadOwns();
  bool allowed = sCpuSleepAllowed && !sInternalLockCpuCount;
  WriteSysFile(allowed ? wakeUnlockFilename : wakeLockFilename, "gecko");
}

static void InternalLockCpu() {
  StaticMutexAutoLock lock(sInternalLockCpuMutex);
  ++sInternalLockCpuCount;
  UpdateCpuSleepState();
}

static void InternalUnlockCpu() {
  StaticMutexAutoLock lock(sInternalLockCpuMutex);
  --sInternalLockCpuCount;
  UpdateCpuSleepState();
}

bool GetCpuSleepAllowed() { return sCpuSleepAllowed; }

void SetCpuSleepAllowed(bool aAllowed) {
  StaticMutexAutoLock lock(sInternalLockCpuMutex);
  sCpuSleepAllowed = aAllowed;
  UpdateCpuSleepState();
}

#if 0  // TODO: FIXME

void
AdjustSystemClock(int64_t aDeltaMilliseconds)
{
  int fd;
  struct timespec now;

  if (aDeltaMilliseconds == 0) {
    return;
  }

  // Preventing context switch before setting system clock
  sched_yield();
  clock_gettime(CLOCK_REALTIME, &now);
  now.tv_sec += (time_t)(aDeltaMilliseconds / 1000LL);
  now.tv_nsec += (long)((aDeltaMilliseconds % 1000LL) * NsecPerMsec);
  if (now.tv_nsec >= NsecPerSec) {
    now.tv_sec += 1;
    now.tv_nsec -= NsecPerSec;
  }

  if (now.tv_nsec < 0) {
    now.tv_nsec += NsecPerSec;
    now.tv_sec -= 1;
  }

  do {
    fd = open("/dev/alarm", O_RDWR);
  } while (fd == -1 && errno == EINTR);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG("Failed to open /dev/alarm: %s", strerror(errno));
    return;
  }

  if (ioctl(fd, ANDROID_ALARM_SET_RTC, &now) < 0) {
    HAL_LOG("ANDROID_ALARM_SET_RTC failed: %s", strerror(errno));
  }

  hal::NotifySystemClockChange(aDeltaMilliseconds);
}

int32_t
GetTimezoneOffset()
{
  PRExplodedTime prTime;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &prTime);

  // Daylight saving time (DST) will be taken into account.
  int32_t offset = prTime.tm_params.tp_gmt_offset;
  offset += prTime.tm_params.tp_dst_offset;

  // Returns the timezone offset relative to UTC in minutes.
  return -(offset / 60);
}

static int32_t sKernelTimezoneOffset = 0;

static void
UpdateKernelTimezone(int32_t timezoneOffset)
{
  if (sKernelTimezoneOffset == timezoneOffset) {
    return;
  }

  // Tell the kernel about the new time zone as well, so that FAT filesystems
  // will get local timestamps rather than UTC timestamps.
  //
  // We assume that /init.rc has a sysclktz entry so that settimeofday has
  // already been called once before we call it (there is a side-effect in
  // the kernel the very first time settimeofday is called where it does some
  // special processing if you only set the timezone).
  struct timezone tz;
  memset(&tz, 0, sizeof(tz));
  tz.tz_minuteswest = timezoneOffset;
  settimeofday(nullptr, &tz);
  sKernelTimezoneOffset = timezoneOffset;
}

nsCString GetTimezone();

void
SetTimezone(const nsCString& aTimezoneSpec)
{
  if (aTimezoneSpec.Equals(GetTimezone())) {
    // Even though the timezone hasn't changed, we still need to tell the
    // kernel what the current timezone is. The timezone is persisted in
    // a property and doesn't change across reboots, but the kernel still
    // needs to be updated on every boot.
    UpdateKernelTimezone(GetTimezoneOffset());
    return;
  }

  int32_t oldTimezoneOffsetMinutes = GetTimezoneOffset();
  property_set("persist.sys.timezone", aTimezoneSpec.get());
  // This function is automatically called by the other time conversion
  // functions that depend on the timezone. To be safe, we call it manually.
  tzset();
  int32_t newTimezoneOffsetMinutes = GetTimezoneOffset();
  UpdateKernelTimezone(newTimezoneOffsetMinutes);
  hal::NotifySystemTimezoneChange(
    hal::SystemTimezoneChangeInformation(
      oldTimezoneOffsetMinutes, newTimezoneOffsetMinutes));
}

nsCString
GetTimezone()
{
  char timezone[32];
  property_get("persist.sys.timezone", timezone, "");
  return nsCString(timezone);
}

#endif

void EnableSystemClockChangeNotifications() {}

void DisableSystemClockChangeNotifications() {}

void EnableSystemTimezoneChangeNotifications() {}

void DisableSystemTimezoneChangeNotifications() {}

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  bool value =
      OrientationObserver::GetInstance()->LockScreenOrientation(aOrientation);
  return GenericNonExclusivePromise::CreateAndResolve(value, __func__);
}

void UnlockScreenOrientation() {
  OrientationObserver::GetInstance()->UnlockScreenOrientation();
}

// This thread will wait for the alarm firing by a blocking IO.
static pthread_t sAlarmFireWatcherThread;

// If |sAlarmData| is non-null, it's owned by the alarm-watcher thread.
struct AlarmData {
 public:
  explicit AlarmData(int aFd)
      : mFd(aFd), mGeneration(sNextGeneration++), mShuttingDown(false) {}
  ScopedClose mFd;
  int mGeneration;
  bool mShuttingDown;

  static int sNextGeneration;
};

int AlarmData::sNextGeneration = 0;

AlarmData* sAlarmData = nullptr;

class AlarmFiredEvent : public Runnable {
 public:
  explicit AlarmFiredEvent(int aGeneration)
      : Runnable("AlarmFiredEvent"), mGeneration(aGeneration) {}

  NS_IMETHOD Run() override {
    // Guard against spurious notifications caused by an alarm firing
    // concurrently with it being disabled.
    if (sAlarmData && !sAlarmData->mShuttingDown &&
        mGeneration == sAlarmData->mGeneration) {
      hal::NotifyAlarmFired();
    }
    // The fired alarm event has been delivered to the observer (if needed);
    // we can now release a CPU wake lock.
    InternalUnlockCpu();
    return NS_OK;
  }

 private:
  int mGeneration;
};

// Runs on alarm-watcher thread.
static void DestroyAlarmData(void* aData) {
  AlarmData* alarmData = static_cast<AlarmData*>(aData);
  delete alarmData;
}

// Runs on alarm-watcher thread.
void ShutDownAlarm(int aSigno) {
  if (aSigno == SIGUSR1 && sAlarmData) {
    sAlarmData->mShuttingDown = true;
  }
  return;
}

static void* WaitForAlarm(void* aData) {
  pthread_cleanup_push(DestroyAlarmData, aData);

  AlarmData* alarmData = static_cast<AlarmData*>(aData);

  while (!alarmData->mShuttingDown) {
    uint64_t expired;
    ssize_t err = read(alarmData->mFd, &expired, sizeof(expired));
    if (err < 0) {
      HAL_LOG("Failed to read alarm fd. %d %s", err, strerror(errno));
    } else if (expired != 1) {
      HAL_LOG("The number of expired alarms is not exact 1. [%d]", expired);
    }

    if (!alarmData->mShuttingDown) {
      // To make sure the observer can get the alarm firing notification
      // *on time* (the system won't sleep during the process in any way),
      // we need to acquire a CPU wake lock before firing the alarm event.
      InternalLockCpu();
      RefPtr<AlarmFiredEvent> event =
          new AlarmFiredEvent(alarmData->mGeneration);
      NS_DispatchToMainThread(event);
    }
  }

  pthread_cleanup_pop(1);
  return nullptr;
}

bool EnableAlarm() {
  MOZ_ASSERT(!sAlarmData);

  int alarmFd = timerfd_create(CLOCK_REALTIME_ALARM, 0);
  if (alarmFd < 0) {
    HAL_LOG("Failed to open alarm device: %s.", strerror(errno));
    return false;
  }

  UniquePtr<AlarmData> alarmData = MakeUnique<AlarmData>(alarmFd);

  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = ShutDownAlarm;
  if (sigaction(SIGUSR1, &actions, nullptr)) {
    HAL_LOG("Failed to set SIGUSR1 signal for alarm-watcher thread.");
    return false;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  int status = pthread_create(&sAlarmFireWatcherThread, &attr, WaitForAlarm,
                              alarmData.get());
  if (status) {
    alarmData.reset();
    HAL_LOG("Failed to create alarm-watcher thread. Status: %d.", status);
    return false;
  }

  pthread_attr_destroy(&attr);

  // The thread owns this now.  We only hold a pointer.
  sAlarmData = alarmData.release();
  return true;
}

void DisableAlarm() {
  MOZ_ASSERT(sAlarmData);

  // NB: this must happen-before the thread cancellation.
  sAlarmData = nullptr;

  // The cancel will interrupt the thread and destroy it, freeing the
  // data pointed at by sAlarmData.
  DebugOnly<int> err = pthread_kill(sAlarmFireWatcherThread, SIGUSR1);
  MOZ_ASSERT(!err);
}

bool SetAlarm(int32_t aSeconds, int32_t aNanoseconds) {
  if (!sAlarmData) {
    HAL_LOG("We should have enabled the alarm.");
    return false;
  }

  struct itimerspec spec;
  memset(&spec, 0, sizeof(spec));
  spec.it_value.tv_sec = aSeconds;
  spec.it_value.tv_nsec = aNanoseconds;

  const int result =
      timerfd_settime(sAlarmData->mFd, TFD_TIMER_ABSTIME, &spec, NULL);

  if (result < 0) {
    HAL_LOG("Unable to set alarm: %s.", strerror(errno));
    return false;
  }

  return true;
}

using namespace android;
using namespace android::hardware;
typedef binder::Status Status;

class FlashlightStateEvent : public Runnable {
 public:
  explicit FlashlightStateEvent(hal::FlashlightInformation info)
      : mozilla::Runnable("FlashlightStateEvent"), mInfo(info) {}

  NS_IMETHOD
  Run() override {
    hal::UpdateFlashlightState(mInfo);
    return NS_OK;
  }

 private:
  hal::FlashlightInformation mInfo;
};

class FlashlightListener : public BnCameraServiceListener {
  mutable android::Mutex mLock;
  uint32_t mTorchStatus = ICameraServiceListener::TORCH_STATUS_UNKNOWN;

 public:
  Status onStatusChanged(int32_t status, const String16& cameraId) override {
    // do nothing
    return Status::ok();
  }
  
  Status onPhysicalCameraStatusChanged (int32_t status, const ::android::String16& cameraId, const ::android::String16& physicalCameraId) override {
   // do nothing
   return Status::ok();
  }

  Status onCameraOpened(const ::android::String16& cameraId, const ::android::String16& clientPackageId) override {
   // do nothing
   return Status::ok();
  } 

  Status onCameraClosed(const ::android::String16& cameraId) override {
   // do nothing
   return Status::ok();
  }

  Status onTorchStatusChanged(int32_t status,
                              const String16& cameraId) override {
    AutoMutex l(mLock);
    if (mTorchStatus != status) {
      hal::FlashlightInformation flashlightInfo;
      flashlightInfo.enabled() =
          (status == ICameraServiceListener::TORCH_STATUS_AVAILABLE_ON) ? true
                                                                        : false;
      flashlightInfo.present() = true;
      RefPtr<FlashlightStateEvent> runnable =
          new FlashlightStateEvent(flashlightInfo);
      NS_DispatchToMainThread(runnable);
      mTorchStatus = status;
    }

    return Status::ok();
  }

  Status onCameraAccessPrioritiesChanged() override {
    // do nothing
    return Status::ok();
  }

  uint32_t getTorchStatus() const {
    AutoMutex l(mLock);
    return mTorchStatus;
  };
};

sp<IBinder> gBinder = nullptr;
sp<hardware::ICameraService> gCameraService = nullptr;
sp<FlashlightListener> gFlashlightListener = nullptr;

static nsresult DispatchToIOThread(already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

static bool initCameraService() {
  Status res;
  sp<IServiceManager> sm = defaultServiceManager();
  do {
    gBinder = sm->getService(String16("media.camera"));
    if (gBinder != 0) {
      break;
    }
    HAL_ERR("CameraService not published, waiting...");
    usleep(500000);
  } while (true);

  gCameraService = interface_cast<hardware::ICameraService>(gBinder);
  gFlashlightListener = new FlashlightListener();
  std::vector<hardware::CameraStatus> statuses;
  res = gCameraService->addListener(gFlashlightListener, &statuses);
  return res.isOk() ? true : false;
}

bool GetFlashlightEnabled() {
  if (gFlashlightListener) {
    return (gFlashlightListener->getTorchStatus() ==
            ICameraServiceListener::TORCH_STATUS_AVAILABLE_ON)
               ? true
               : false;
  } else {
    HAL_ERR("gFlashlightListener is not initialized yet!");
    return false;
  }
}

void SetFlashlightEnabled(bool aEnabled) {
  if (gCameraService) {
    // Dispatch to IO thread for avoiding dead lock occurred betwwen this
    // function and GonkCameraHardware::Connect in CameraService.
    DispatchToIOThread(NS_NewRunnableFunction(
        "mozilla::hal_impl::SetFlashlightEnabled", [=]() -> void {
          Status res;
          res = gCameraService->setTorchMode(String16("0"), aEnabled, gBinder);
          if (!res.isOk()) {
            HAL_ERR("Failed to get setTorchMode, early return!");
          }
        }));
  } else {
    HAL_ERR("CameraService haven't initialized yet, return directly!");
  }
}

bool IsFlashlightPresent() {
  if (gFlashlightListener) {
    // onTorchStatusChanged update mTorchStatus, and it will only be
    // invoked when flashlight unit is present. Otherwise mTorchStatus
    // will be default value.
    return (gFlashlightListener->getTorchStatus() !=
            ICameraServiceListener::TORCH_STATUS_UNKNOWN)
               ? true
               : false;
  } else {
    HAL_ERR("gFlashlightListener is not initialized yet!");
    return false;
  }
}

void RequestCurrentFlashlightState() {
  hal::FlashlightInformation flashlightInfo;
  flashlightInfo.enabled() = GetFlashlightEnabled();
  flashlightInfo.present() = IsFlashlightPresent();
  hal::UpdateFlashlightState(flashlightInfo);
}

void EnableFlashlightNotifications() {
  if (!gCameraService) {
    if (!initCameraService()) {
      HAL_ERR("Failed to init CameraService, operation failed!");
      return;
    }
  }
}

void DisableFlashlightNotifications() {
  if (gCameraService) {
    gCameraService->removeListener(gFlashlightListener);
    gBinder = nullptr;
    gCameraService = nullptr;
    gFlashlightListener = nullptr;
  }
}

bool IsFlipOpened() {
  bool status;
  char propValue[PROPERTY_VALUE_MAX];

  if (property_get("ro.kaios.flipstatus", propValue, NULL) <= 0) {
    return true;
  }

  bool success = ReadSysFile(propValue, &status);
  if (!success) {
    return true;
  }
  return !status;
}

void NotifyFlipStateFromInputDevice(bool aFlipState) {
  hal::UpdateFlipState(aFlipState);
}

void RequestCurrentFlipState() {
  bool flipState = IsFlipOpened();
  hal::UpdateFlipState(flipState);
}

// Main process receives notifications of flip state change from input device
// directly.
void EnableFlipNotifications() {}

void DisableFlipNotifications() {}

#if 0  // TODO: FIXME
static int
OomAdjOfOomScoreAdj(int aOomScoreAdj)
{
  // Convert OOM adjustment from the domain of /proc/<pid>/oom_score_adj
  // to the domain of /proc/<pid>/oom_adj.

  int adj;

  if (aOomScoreAdj < 0) {
    adj = (OOM_DISABLE * aOomScoreAdj) / OOM_SCORE_ADJ_MIN;
  } else {
    adj = (OOM_ADJUST_MAX * aOomScoreAdj) / OOM_SCORE_ADJ_MAX;
  }

  return adj;
}

static void
RoundOomScoreAdjUpWithLRU(int& aOomScoreAdj, uint32_t aLRU)
{
  // We want to add minimum value to round OomScoreAdj up according to
  // the steps by aLRU.
  aOomScoreAdj +=
    ceil(((float)OOM_SCORE_ADJ_MAX / OOM_ADJUST_MAX) * aLRU);
}

#  define OOM_LOG(level, args...) \
    __android_log_print(level, "OomLogger", ##args)
class OomVictimLogger final
  : public nsIObserver
{
public:
  OomVictimLogger()
    : mLastLineChecked(-1.0),
      mRegexes(nullptr)
  {
    // Enable timestamps in kernel's printk
    WriteSysFile("/sys/module/printk/parameters/time", "Y");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

protected:
  ~OomVictimLogger() {}

private:
  double mLastLineChecked;
  UniqueFreePtr<regex_t> mRegexes;
};
NS_IMPL_ISUPPORTS(OomVictimLogger, nsIObserver);

NS_IMETHODIMP
OomVictimLogger::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const char16_t* aData)
{
  nsDependentCString event_type(aTopic);
  if (!event_type.EqualsLiteral("ipc:content-shutdown")) {
    return NS_OK;
  }

  // OOM message finding regexes
  const char* const regexes_raw[] = {
    ".*select.*to kill.*",
    ".*send sigkill to.*",
    ".*lowmem_shrink.*",
    ".*[Oo]ut of [Mm]emory.*",
    ".*[Kk]ill [Pp]rocess.*",
    ".*[Kk]illed [Pp]rocess.*",
    ".*oom-killer.*",
    // The regexes below are for the output of dump_task from oom_kill.c
    // 1st - title 2nd - body lines (8 ints and a string)
    // oom_adj and oom_score_adj can be negative
    "\\[ pid \\]   uid  tgid total_vm      rss cpu oom_adj oom_score_adj name",
    "\\[.*[0-9][0-9]*\\][ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*.[0-9][0-9]*[ ]*.[0-9][0-9]*.*"
  };
  const size_t regex_count = ArrayLength(regexes_raw);

  // Compile our regex just in time
  if (!mRegexes) {
    UniqueFreePtr<regex_t> regexes(
      static_cast<regex_t*>(malloc(sizeof(regex_t) * regex_count))
    );
    mRegexes.swap(regexes);
    for (size_t i = 0; i < regex_count; i++) {
      int compilation_err =
        regcomp(&(mRegexes.get()[i]), regexes_raw[i], REG_NOSUB);
      if (compilation_err) {
        OOM_LOG(ANDROID_LOG_ERROR, "Cannot compile regex \"%s\"\n", regexes_raw[i]);
        return NS_OK;
      }
    }
  }

#  ifndef KLOG_SIZE_BUFFER
  // Upstream bionic in commit
  // e249b059637b49a285ed9f58a2a18bfd054e5d95
  // deprecated the old klog defs.
  // Our current bionic does not hit this
  // change yet so handle the future change.
  // (ICS doesn't have KLOG_SIZE_BUFFER but
  // JB and onwards does.)
#    define KLOG_SIZE_BUFFER KLOG_WRITE
#  endif
  // Retreive kernel log
  int msg_buf_size = klogctl(KLOG_SIZE_BUFFER, NULL, 0);
  UniqueFreePtr<char> msg_buf(static_cast<char *>(malloc(msg_buf_size + 1)));
  int read_size = klogctl(KLOG_READ_ALL, msg_buf.get(), msg_buf_size);

  // Turn buffer into cstring
  read_size = read_size > msg_buf_size ? msg_buf_size : read_size;
  msg_buf.get()[read_size] = '\0';

  // Foreach line
  char* line_end;
  char* line_begin = msg_buf.get();
  for (; (line_end = strchr(line_begin, '\n')); line_begin = line_end + 1) {
    // make line into cstring
    *line_end = '\0';

    // Note: Kernel messages look like:
    // <5>[63648.286409] sd 35:0:0:0: Attached scsi generic sg1 type 0
    // 5 is the loging level
    // [*] is the time timestamp, seconds since boot
    // last comes the logged message

    // Since the logging level can be a string we must
    // skip it since scanf lacks wildcard matching
    char*  timestamp_begin = strchr(line_begin, '[');
    char   after_float;
    double lineTimestamp = -1;
    bool   lineTimestampFound = false;
    if (timestamp_begin &&
         // Note: scanf treats a ' ' as [ ]*
         // Note: scanf treats [ %lf] as [ %lf thus we must check
         // for the closing bracket outselves.
         2 == sscanf(timestamp_begin, "[ %lf%c", &lineTimestamp, &after_float) &&
         after_float == ']') {
      if (lineTimestamp <= mLastLineChecked) {
        continue;
      }

      lineTimestampFound = true;
      mLastLineChecked = lineTimestamp;
    }

    // Log interesting lines
    for (size_t i = 0; i < regex_count; i++) {
      int matching = !regexec(&(mRegexes.get()[i]), line_begin, 0, NULL, 0);
      if (matching) {
        // Log content of kernel message. We try to skip the ], but if for
        // some reason (most likely due to buffer overflow/wraparound), we
        // can't find the ] then we just log the entire line.
        char* endOfTimestamp = strchr(line_begin, ']');
        if (endOfTimestamp && endOfTimestamp[1] == ' ') {
          // skip the ] and the space that follows it
          line_begin = endOfTimestamp + 2;
        }
        if (!lineTimestampFound) {
          OOM_LOG(ANDROID_LOG_WARN, "following kill message may be a duplicate");
        }
        OOM_LOG(ANDROID_LOG_ERROR, "[Kill]: %s\n", line_begin);
        break;
      }
    }
  }

  return NS_OK;
}

/**
 * Wraps a particular ProcessPriority, giving us easy access to the prefs that
 * are relevant to it.
 *
 * Creating a PriorityClass also ensures that the control group is created.
 */
class PriorityClass
{
public:
  /**
   * Create a PriorityClass for the given ProcessPriority.  This implicitly
   * reads the relevant prefs and opens the cgroup.procs file of the relevant
   * control group caching its file descriptor for later use.
   */
  PriorityClass(ProcessPriority aPriority);

  /**
   * Closes the file descriptor for the cgroup.procs file of the associated
   * control group.
   */
  ~PriorityClass();

  PriorityClass(const PriorityClass& aOther);
  PriorityClass& operator=(const PriorityClass& aOther);

  ProcessPriority Priority()
  {
    return mPriority;
  }

  int32_t OomScoreAdj()
  {
    return clamped<int32_t>(mOomScoreAdj, OOM_SCORE_ADJ_MIN, OOM_SCORE_ADJ_MAX);
  }

  int32_t KillUnderKB()
  {
    return mKillUnderKB;
  }

  nsCString CGroup()
  {
    return mGroup;
  }

  /**
   * Adds a process to this priority class, this moves the process' PID into
   * the associated control group.
   *
   * @param aPid The PID of the process to be added.
   */
  void AddProcess(int aPid);

private:
  ProcessPriority mPriority;
  int32_t mOomScoreAdj;
  int32_t mKillUnderKB;
  int mCpuCGroupProcsFd;
  int mMemCGroupProcsFd;
  nsCString mGroup;

  /**
   * Return a string that identifies where we can find the value of aPref
   * that's specific to mPriority.  For example, we might return
   * "hal.processPriorityManager.gonk.FOREGROUND_HIGH.oomScoreAdjust".
   */
  nsCString PriorityPrefName(const char* aPref)
  {
    return nsPrintfCString("hal.processPriorityManager.gonk.%s.%s",
                           ProcessPriorityToString(mPriority), aPref);
  }

  /**
   * Get the full path of the cgroup.procs file associated with the group.
   */
  nsCString CpuCGroupProcsFilename()
  {
    nsCString cgroupName = mGroup;

    /* If mGroup is empty, our cgroup.procs file is the root procs file,
     * located at /dev/cpuctl/cgroup.procs.  Otherwise our procs file is
     * /dev/cpuctl/NAME/cgroup.procs. */

    if (!mGroup.IsEmpty()) {
      cgroupName.AppendLiteral("/");
    }

    return "/dev/cpuctl/"_ns + cgroupName +
           "cgroup.procs"_ns;
  }

  nsCString MemCGroupProcsFilename()
  {
    nsCString cgroupName = mGroup;

    /* If mGroup is empty, our cgroup.procs file is the root procs file,
     * located at /sys/fs/cgroup/memory/cgroup.procs.  Otherwise our procs
     * file is /sys/fs/cgroup/memory/NAME/cgroup.procs. */

    if (!mGroup.IsEmpty()) {
      cgroupName.AppendLiteral("/");
    }

    return "/sys/fs/cgroup/memory/"_ns + cgroupName +
           "cgroup.procs"_ns;
  }

  int OpenCpuCGroupProcs()
  {
    return open(CpuCGroupProcsFilename().get(), O_WRONLY);
  }

  int OpenMemCGroupProcs()
  {
    return open(MemCGroupProcsFilename().get(), O_WRONLY);
  }
};

/**
 * Try to create the cgroup for the given PriorityClass, if it doesn't already
 * exist.  This essentially implements mkdir -p; that is, we create parent
 * cgroups as necessary.  The group parameters are also set according to
 * the corresponding preferences.
 *
 * @param aGroup The name of the group.
 * @return true if we successfully created the cgroup, or if it already
 * exists.  Otherwise, return false.
 */
static bool
EnsureCpuCGroupExists(const nsACString &aGroup)
{
  NS_NAMED_LITERAL_CSTRING(kDevCpuCtl, "/dev/cpuctl/");
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsAutoCString groupName(aGroup);
  HAL_LOG("EnsureCpuCGroupExists for group '%s'", groupName.get());

  nsAutoCString prefPrefix("hal.processPriorityManager.gonk.cgroups.");

  /* If cgroup is not empty, append the cgroup name and a dot to obtain the
   * group specific preferences. */
  if (!aGroup.IsEmpty()) {
    prefPrefix += aGroup + "."_ns;
  }

  nsAutoCString cpuSharesPref(prefPrefix + "cpu_shares"_ns);
  int cpuShares = Preferences::GetInt(cpuSharesPref.get());

  nsAutoCString cpuNotifyOnMigratePref(prefPrefix
    + "cpu_notify_on_migrate"_ns);
  int cpuNotifyOnMigrate = Preferences::GetInt(cpuNotifyOnMigratePref.get());

  // Create mCGroup and its parent directories, as necessary.
  nsCString cgroupIter = aGroup + kSlash;

  int32_t offset = 0;
  while ((offset = cgroupIter.FindChar('/', offset)) != -1) {
    nsAutoCString path = kDevCpuCtl + Substring(cgroupIter, 0, offset);
    int rv = mkdir(path.get(), 0744);

    if (rv == -1 && errno != EEXIST) {
      HAL_LOG("Could not create the %s control group.", path.get());
      return false;
    }

    offset++;
  }
  HAL_LOG("EnsureCpuCGroupExists created group '%s'", groupName.get());

  nsAutoCString pathPrefix(kDevCpuCtl + aGroup + kSlash);
  nsAutoCString cpuSharesPath(pathPrefix + "cpu.shares"_ns);
  if (cpuShares && !WriteSysFile(cpuSharesPath.get(),
                                 nsPrintfCString("%d", cpuShares).get())) {
    HAL_LOG("Could not set the cpu share for group %s", cpuSharesPath.get());
    return false;
  }

  nsAutoCString notifyOnMigratePath(pathPrefix
    + "cpu.notify_on_migrate"_ns);
  if (!WriteSysFile(notifyOnMigratePath.get(),
                    nsPrintfCString("%d", cpuNotifyOnMigrate).get())) {
    HAL_LOG("Could not set the cpu migration notification flag for group %s",
            notifyOnMigratePath.get());
    return false;
  }

  return true;
}

static bool
EnsureMemCGroupExists(const nsACString &aGroup)
{
  NS_NAMED_LITERAL_CSTRING(kMemCtl, "/sys/fs/cgroup/memory/");
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsAutoCString groupName(aGroup);
  HAL_LOG("EnsureMemCGroupExists for group '%s'", groupName.get());

  nsAutoCString prefPrefix("hal.processPriorityManager.gonk.cgroups.");

  /* If cgroup is not empty, append the cgroup name and a dot to obtain the
   * group specific preferences. */
  if (!aGroup.IsEmpty()) {
    prefPrefix += aGroup + "."_ns;
  }

  nsAutoCString memSwappinessPref(prefPrefix + "memory_swappiness"_ns);
  int memSwappiness = Preferences::GetInt(memSwappinessPref.get());

  // Create mCGroup and its parent directories, as necessary.
  nsCString cgroupIter = aGroup + kSlash;

  int32_t offset = 0;
  while ((offset = cgroupIter.FindChar('/', offset)) != -1) {
    nsAutoCString path = kMemCtl + Substring(cgroupIter, 0, offset);
    int rv = mkdir(path.get(), 0744);

    if (rv == -1 && errno != EEXIST) {
      HAL_LOG("Could not create the %s control group.", path.get());
      return false;
    }

    offset++;
  }
  HAL_LOG("EnsureMemCGroupExists created group '%s'", groupName.get());

  nsAutoCString pathPrefix(kMemCtl + aGroup + kSlash);
  nsAutoCString memSwappinessPath(pathPrefix + "memory.swappiness"_ns);
  if (!WriteSysFile(memSwappinessPath.get(),
                    nsPrintfCString("%d", memSwappiness).get())) {
    HAL_LOG("Could not set the memory.swappiness for group %s", memSwappinessPath.get());
    return false;
  }
  HAL_LOG("Set memory.swappiness for group %s to %d", memSwappinessPath.get(), memSwappiness);

  return true;
}

PriorityClass::PriorityClass(ProcessPriority aPriority)
  : mPriority(aPriority)
  , mOomScoreAdj(0)
  , mKillUnderKB(0)
  , mCpuCGroupProcsFd(-1)
  , mMemCGroupProcsFd(-1)
{
  DebugOnly<nsresult> rv;

  rv = Preferences::GetInt(PriorityPrefName("OomScoreAdjust").get(),
                           &mOomScoreAdj);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Missing oom_score_adj preference");

  rv = Preferences::GetInt(PriorityPrefName("KillUnderKB").get(),
                           &mKillUnderKB);

  rv = Preferences::GetCString(PriorityPrefName("cgroup").get(), &mGroup);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Missing control group preference");

  if (EnsureCpuCGroupExists(mGroup)) {
    mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  }
  if (EnsureMemCGroupExists(mGroup)) {
    mMemCGroupProcsFd = OpenMemCGroupProcs();
  }
}

PriorityClass::~PriorityClass()
{
  if (mCpuCGroupProcsFd != -1) {
    close(mCpuCGroupProcsFd);
  }
  if (mMemCGroupProcsFd != -1) {
    close(mMemCGroupProcsFd);
  }
}

PriorityClass::PriorityClass(const PriorityClass& aOther)
  : mPriority(aOther.mPriority)
  , mOomScoreAdj(aOther.mOomScoreAdj)
  , mKillUnderKB(aOther.mKillUnderKB)
  , mGroup(aOther.mGroup)
{
  mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  mMemCGroupProcsFd = OpenMemCGroupProcs();
}

PriorityClass& PriorityClass::operator=(const PriorityClass& aOther)
{
  mPriority = aOther.mPriority;
  mOomScoreAdj = aOther.mOomScoreAdj;
  mKillUnderKB = aOther.mKillUnderKB;
  mGroup = aOther.mGroup;
  mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  mMemCGroupProcsFd = OpenMemCGroupProcs();
  return *this;
}

void PriorityClass::AddProcess(int aPid)
{
  if (mCpuCGroupProcsFd >= 0) {
    nsPrintfCString str("%d", aPid);

    if (write(mCpuCGroupProcsFd, str.get(), strlen(str.get())) < 0) {
      HAL_ERR("Couldn't add PID %d to the %s cpu control group", aPid, mGroup.get());
    }
  }
  if (mMemCGroupProcsFd >= 0) {
    nsPrintfCString str("%d", aPid);

    if (write(mMemCGroupProcsFd, str.get(), strlen(str.get())) < 0) {
      HAL_ERR("Couldn't add PID %d to the %s memory control group", aPid, mGroup.get());
    }
  }
}

/**
 * Get the PriorityClass associated with the given ProcessPriority.
 *
 * If you pass an invalid ProcessPriority value, we return null.
 *
 * The pointers returned here are owned by GetPriorityClass (don't free them
 * yourself).  They are guaranteed to stick around until shutdown.
 */
PriorityClass*
GetPriorityClass(ProcessPriority aPriority)
{
  static StaticAutoPtr<nsTArray<PriorityClass>> priorityClasses;

  // Initialize priorityClasses if this is the first time we're running this
  // method.
  if (!priorityClasses) {
    priorityClasses = new nsTArray<PriorityClass>();
    ClearOnShutdown(&priorityClasses);

    for (int32_t i = 0; i < NUM_PROCESS_PRIORITY; i++) {
      priorityClasses->AppendElement(PriorityClass(ProcessPriority(i)));
    }
  }

  if (aPriority < 0 ||
      static_cast<uint32_t>(aPriority) >= priorityClasses->Length()) {
    return nullptr;
  }

  return &(*priorityClasses)[aPriority];
}

static void
EnsureKernelLowMemKillerParamsSet()
{
  static bool kernelLowMemKillerParamsSet;
  if (kernelLowMemKillerParamsSet) {
    return;
  }
  kernelLowMemKillerParamsSet = true;

  HAL_LOG("Setting kernel's low-mem killer parameters.");

  // Set /sys/module/lowmemorykiller/parameters/{adj,minfree,notify_trigger}
  // according to our prefs.  These files let us tune when the kernel kills
  // processes when we're low on memory, and when it notifies us that we're
  // running low on available memory.
  //
  // adj and minfree are both comma-separated lists of integers.  If adj="A,B"
  // and minfree="X,Y", then the kernel will kill processes with oom_adj
  // A or higher once we have fewer than X pages of memory free, and will kill
  // processes with oom_adj B or higher once we have fewer than Y pages of
  // memory free.
  //
  // notify_trigger is a single integer.   If we set notify_trigger=Z, then
  // we'll get notified when there are fewer than Z pages of memory free.  (See
  // GonkMemoryPressureMonitoring.cpp.)

  // Build the adj and minfree strings.
  nsAutoCString adjParams;
  nsAutoCString minfreeParams;

  DebugOnly<int32_t> lowerBoundOfNextOomScoreAdj = OOM_SCORE_ADJ_MIN - 1;
  DebugOnly<int32_t> lowerBoundOfNextKillUnderKB = 0;
  int32_t countOfLowmemorykillerParametersSets = 0;

  long page_size = sysconf(_SC_PAGESIZE);

  for (int i = NUM_PROCESS_PRIORITY - 1; i >= 0; i--) {
    // The system doesn't function correctly if we're missing these prefs, so
    // crash loudly.

    PriorityClass* pc = GetPriorityClass(static_cast<ProcessPriority>(i));

    int32_t oomScoreAdj = pc->OomScoreAdj();
    int32_t killUnderKB = pc->KillUnderKB();

    if (killUnderKB == 0) {
      // ProcessPriority values like PROCESS_PRIORITY_FOREGROUND_KEYBOARD,
      // which has only OomScoreAdjust but lacks KillUnderMB value, will not
      // create new LMK parameters.
      continue;
    }

    // The LMK in kernel silently malfunctions if we assign the parameters
    // in non-increasing order, so we add this assertion here. See bug 887192.
    MOZ_ASSERT(oomScoreAdj > lowerBoundOfNextOomScoreAdj);
    MOZ_ASSERT(killUnderKB > lowerBoundOfNextKillUnderKB);

    // The LMK in kernel only accept 6 sets of LMK parameters. See bug 914728.
    MOZ_ASSERT(countOfLowmemorykillerParametersSets < 6);

    // adj is in oom_adj units.
    adjParams.AppendPrintf("%d,", OomAdjOfOomScoreAdj(oomScoreAdj));

    // minfree is in pages.
    minfreeParams.AppendPrintf("%ld,", killUnderKB * 1024 / page_size);

    lowerBoundOfNextOomScoreAdj = oomScoreAdj;
    lowerBoundOfNextKillUnderKB = killUnderKB;
    countOfLowmemorykillerParametersSets++;
  }

  // Strip off trailing commas.
  adjParams.Cut(adjParams.Length() - 1, 1);
  minfreeParams.Cut(minfreeParams.Length() - 1, 1);
  if (!adjParams.IsEmpty() && !minfreeParams.IsEmpty()) {
    WriteSysFile("/sys/module/lowmemorykiller/parameters/adj", adjParams.get());
    WriteSysFile("/sys/module/lowmemorykiller/parameters/minfree",
                 minfreeParams.get());
  }

  // Set the low-memory-notification threshold.
  int32_t lowMemNotifyThresholdKB;
  if (NS_SUCCEEDED(Preferences::GetInt(
        "hal.processPriorityManager.gonk.notifyLowMemUnderKB",
        &lowMemNotifyThresholdKB))) {

    // notify_trigger is in pages.
    WriteSysFile("/sys/module/lowmemorykiller/parameters/notify_trigger",
      nsPrintfCString("%ld", lowMemNotifyThresholdKB * 1024 / page_size).get());
  }

  // Ensure OOM events appear in logcat
  RefPtr<OomVictimLogger> oomLogger = new OomVictimLogger();
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(oomLogger, "ipc:content-shutdown", false);
  }
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority, uint32_t aLRU)
{
  HAL_LOG("SetProcessPriority(pid=%d, priority=%d, LRU=%u)",
          aPid, aPriority, aLRU);

  // If this is the first time SetProcessPriority was called, set the kernel's
  // OOM parameters according to our prefs.
  //
  // We could/should do this on startup instead of waiting for the first
  // SetProcessPriorityCall.  But in practice, the master process needs to set
  // its priority early in the game, so we can reasonably rely on
  // SetProcessPriority being called early in startup.
  EnsureKernelLowMemKillerParamsSet();

  PriorityClass* pc = GetPriorityClass(aPriority);

  int oomScoreAdj = pc->OomScoreAdj();

  RoundOomScoreAdjUpWithLRU(oomScoreAdj, aLRU);

  // We try the newer interface first, and fall back to the older interface
  // on failure.
  if (!WriteSysFile(nsPrintfCString("/proc/%d/oom_score_adj", aPid).get(),
                    nsPrintfCString("%d", oomScoreAdj).get()))
  {
    WriteSysFile(nsPrintfCString("/proc/%d/oom_adj", aPid).get(),
                 nsPrintfCString("%d", OomAdjOfOomScoreAdj(oomScoreAdj)).get());
  }

  HAL_LOG("Assigning pid %d to cgroup %s", aPid, pc->CGroup().get());
  pc->AddProcess(aPid);
}

static bool
IsValidRealTimePriority(int aValue, int aSchedulePolicy)
{
  return (aValue >= sched_get_priority_min(aSchedulePolicy)) &&
         (aValue <= sched_get_priority_max(aSchedulePolicy));
}

static void
SetThreadNiceValue(pid_t aTid, ThreadPriority aThreadPriority, int aValue)
{
  MOZ_ASSERT(aThreadPriority < NUM_THREAD_PRIORITY);
  MOZ_ASSERT(aThreadPriority >= 0);

  HAL_LOG("Setting thread %d to priority level %s; nice level %d",
          aTid, ThreadPriorityToString(aThreadPriority), aValue);
  int rv = setpriority(PRIO_PROCESS, aTid, aValue);

  if (rv) {
    HAL_LOG("Failed to set thread %d to priority level %s; error %s", aTid,
            ThreadPriorityToString(aThreadPriority), strerror(errno));
  }
}

static void
SetRealTimeThreadPriority(pid_t aTid,
                          ThreadPriority aThreadPriority,
                          int aValue)
{
  int policy = SCHED_FIFO;

  MOZ_ASSERT(aThreadPriority < NUM_THREAD_PRIORITY);
  MOZ_ASSERT(aThreadPriority >= 0);
  MOZ_ASSERT(IsValidRealTimePriority(aValue, policy), "Invalid real time priority");

  // Setting real time priorities requires using sched_setscheduler
  HAL_LOG("Setting thread %d to priority level %s; Real Time priority %d, "
          "Schedule FIFO", aTid, ThreadPriorityToString(aThreadPriority),
          aValue);
  sched_param schedParam;
  schedParam.sched_priority = aValue;
  int rv = sched_setscheduler(aTid, policy, &schedParam);

  if (rv) {
    HAL_LOG("Failed to set thread %d to real time priority level %s; error %s",
            aTid, ThreadPriorityToString(aThreadPriority), strerror(errno));
  }
}

/*
 * Used to store the nice value adjustments and real time priorities for the
 * various thread priority levels.
 */
struct ThreadPriorityPrefs {
  bool initialized;
  struct {
    int nice;
    int realTime;
  } priorities[NUM_THREAD_PRIORITY];
};

/*
 * Reads the preferences for the various process priority levels and sets up
 * watchers so that if they're dynamically changed the change is reflected on
 * the appropriate variables.
 */
void
EnsureThreadPriorityPrefs(ThreadPriorityPrefs* prefs)
{
  if (prefs->initialized) {
    return;
  }

  for (int i = THREAD_PRIORITY_COMPOSITOR; i < NUM_THREAD_PRIORITY; i++) {
    ThreadPriority priority = static_cast<ThreadPriority>(i);

    // Read the nice values
    const char* threadPriorityStr = ThreadPriorityToString(priority);
    nsPrintfCString niceStr("hal.gonk.%s.nice", threadPriorityStr);
    Preferences::AddIntVarCache(&prefs->priorities[i].nice, niceStr.get());

    // Read the real-time priorities
    nsPrintfCString realTimeStr("hal.gonk.%s.rt_priority", threadPriorityStr);
    Preferences::AddIntVarCache(&prefs->priorities[i].realTime,
                                realTimeStr.get());
  }

  prefs->initialized = true;
}

static void
DoSetThreadPriority(pid_t aTid, hal::ThreadPriority aThreadPriority)
{
  // See bug 999115, we can only read preferences on the main thread otherwise
  // we create a race condition in HAL
  MOZ_ASSERT(NS_IsMainThread(), "Can only set thread priorities on main thread");
  MOZ_ASSERT(aThreadPriority >= 0);

  static ThreadPriorityPrefs prefs = { 0 };
  EnsureThreadPriorityPrefs(&prefs);

  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR:
      break;
    default:
      HAL_ERR("Unrecognized thread priority %d; Doing nothing",
              aThreadPriority);
      return;
  }

  int realTimePriority = prefs.priorities[aThreadPriority].realTime;

  if (IsValidRealTimePriority(realTimePriority, SCHED_FIFO)) {
    SetRealTimeThreadPriority(aTid, aThreadPriority, realTimePriority);
    return;
  }

  SetThreadNiceValue(aTid, aThreadPriority,
                     prefs.priorities[aThreadPriority].nice);
}

namespace {

/**
 * This class sets the priority of threads given the kernel thread's id and a
 * value taken from hal::ThreadPriority.
 *
 * This runnable must always be dispatched to the main thread otherwise it will fail.
 * We have to run this from the main thread since preferences can only be read on
 * main thread.
 */
class SetThreadPriorityRunnable : public Runnable
{
public:
  SetThreadPriorityRunnable(pid_t aThreadId, hal::ThreadPriority aThreadPriority)
    : mThreadId(aThreadId)
    , mThreadPriority(aThreadPriority)
  { }

  NS_IMETHOD Run() override
  {
    NS_ASSERTION(NS_IsMainThread(), "Can only set thread priorities on main thread");
    hal_impl::DoSetThreadPriority(mThreadId, mThreadPriority);
    return NS_OK;
  }

private:
  pid_t mThreadId;
  hal::ThreadPriority mThreadPriority;
};

} // namespace

void
SetCurrentThreadPriority(ThreadPriority aThreadPriority)
{
  pid_t threadId = gettid();
  hal_impl::SetThreadPriority(threadId, aThreadPriority);
}

void
SetThreadPriority(PlatformThreadId aThreadId,
                         ThreadPriority aThreadPriority)
{
  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR: {
      nsCOMPtr<nsIRunnable> runnable =
        new SetThreadPriorityRunnable(aThreadId, aThreadPriority);
      NS_DispatchToMainThread(runnable);
      break;
    }
    default:
      HAL_LOG("Unrecognized thread priority %d; Doing nothing",
              aThreadPriority);
      return;
  }
}

void
FactoryReset(FactoryResetReason& aReason)
{
#  if 0  // TODO: FIXME
  nsCOMPtr<nsIRecoveryService> recoveryService =
    do_GetService("@mozilla.org/recovery-service;1");
  if (!recoveryService) {
    NS_WARNING("Could not get recovery service!");
    return;
  }

  if (aReason == FactoryResetReason::Wipe) {
    recoveryService->FactoryReset("wipe");
  } else if (aReason == FactoryResetReason::Root) {
    recoveryService->FactoryReset("root");
  } else {
    recoveryService->FactoryReset("normal");
  }
#  endif
}

#endif

}  // namespace hal_impl
}  // namespace mozilla
