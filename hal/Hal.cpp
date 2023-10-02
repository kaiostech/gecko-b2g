/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "HalImpl.h"
#include "HalLog.h"
#include "HalSandbox.h"
#include "HalWakeLockInternal.h"
#include "mozilla/dom/Document.h"
#include "nsXULAppAPI.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Observer.h"
#include "mozilla/dom/ContentChild.h"
#include "WindowIdentifier.h"

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#endif

using namespace mozilla::services;
using namespace mozilla::dom;

#define PROXY_IF_SANDBOXED(_call)              \
  do {                                         \
    if (InSandbox()) {                         \
      if (!hal_sandbox::HalChildDestroyed()) { \
        hal_sandbox::_call;                    \
      }                                        \
    } else {                                   \
      hal_impl::_call;                         \
    }                                          \
  } while (0)

#define RETURN_PROXY_IF_SANDBOXED(_call, defValue) \
  do {                                             \
    if (InSandbox()) {                             \
      if (hal_sandbox::HalChildDestroyed()) {      \
        return defValue;                           \
      }                                            \
      return hal_sandbox::_call;                   \
    } else {                                       \
      return hal_impl::_call;                      \
    }                                              \
  } while (0)

namespace mozilla::hal {

static bool sInitialized = false;

mozilla::LogModule* GetHalLog() {
  static mozilla::LazyLogModule sHalLog("hal");
  return sHalLog;
}

namespace {

void AssertMainThread() { MOZ_ASSERT(NS_IsMainThread()); }

bool InSandbox() { return GeckoProcessType_Content == XRE_GetProcessType(); }

void AssertMainProcess() {
  MOZ_ASSERT(GeckoProcessType_Default == XRE_GetProcessType());
}

bool WindowIsActive(nsPIDOMWindowInner* aWindow) {
  dom::Document* document = aWindow->GetDoc();
  NS_ENSURE_TRUE(document, false);
  auto principal = document->NodePrincipal();
  NS_ENSURE_TRUE(principal, false);
  return !document->Hidden() || principal->IsSystemPrincipal();
}

StaticAutoPtr<WindowIdentifier::IDArrayType> gLastIDToVibrate;

static void RecordLastIDToVibrate(const WindowIdentifier& aId) {
  if (!InSandbox()) {
    *gLastIDToVibrate = aId.AsArray().Clone();
  }
}

static bool MayCancelVibration(const WindowIdentifier& aId) {
  // Although only active windows may start vibrations, a window may
  // cancel its own vibration even if it's no longer active.
  //
  // After a window is marked as inactive, it sends a CancelVibrate
  // request.  We want this request to cancel a playing vibration
  // started by that window, so we certainly don't want to reject the
  // cancellation request because the window is now inactive.
  //
  // But it could be the case that, after this window became inactive,
  // some other window came along and started a vibration.  We don't
  // want this window's cancellation request to cancel that window's
  // actively-playing vibration!
  //
  // To solve this problem, we keep track of the id of the last window
  // to start a vibration, and only accepts cancellation requests from
  // the same window.  All other cancellation requests are ignored.

  return InSandbox() || (*gLastIDToVibrate == aId.AsArray());
}

}  // namespace

void Vibrate(const nsTArray<uint32_t>& pattern, nsPIDOMWindowInner* window) {
  Vibrate(pattern, WindowIdentifier(window));
}

void Vibrate(const nsTArray<uint32_t>& pattern, WindowIdentifier&& id) {
  AssertMainThread();

  // Only active windows may start vibrations.  If |id| hasn't gone
  // through the IPC layer -- that is, if our caller is the outside
  // world, not hal_proxy -- check whether the window is active.  If
  // |id| has gone through IPC, don't check the window's visibility;
  // only the window corresponding to the bottommost process has its
  // visibility state set correctly.
  if (!id.HasTraveledThroughIPC() && !WindowIsActive(id.GetWindow())) {
    HAL_LOG("Vibrate: Window is inactive, dropping vibrate.");
    return;
  }

  RecordLastIDToVibrate(id);

  // Don't forward our ID if we are not in the sandbox, because hal_impl
  // doesn't need it, and we don't want it to be tempted to read it.  The
  // empty identifier will assert if it's used.
  PROXY_IF_SANDBOXED(
      Vibrate(pattern, InSandbox() ? std::move(id) : WindowIdentifier()));
}

void CancelVibrate(nsPIDOMWindowInner* window) {
  CancelVibrate(WindowIdentifier(window));
}

void CancelVibrate(WindowIdentifier&& id) {
  AssertMainThread();

  if (MayCancelVibration(id)) {
    // Don't forward our ID if we are not in the sandbox, because hal_impl
    // doesn't need it, and we don't want it to be tempted to read it.  The
    // empty identifier will assert if it's used.
    PROXY_IF_SANDBOXED(
        CancelVibrate(InSandbox() ? std::move(id) : WindowIdentifier()));
  }
}

template <class InfoType>
class ObserversManager {
 public:
  void AddObserver(Observer<InfoType>* aObserver) {
    mObservers.AddObserver(aObserver);

    if (mObservers.Length() == 1) {
      EnableNotifications();
    }
  }

  void RemoveObserver(Observer<InfoType>* aObserver) {
    bool removed = mObservers.RemoveObserver(aObserver);
    if (!removed) {
      return;
    }

    if (mObservers.Length() == 0) {
      DisableNotifications();
      OnNotificationsDisabled();
    }
  }

  void BroadcastInformation(const InfoType& aInfo) {
    mObservers.Broadcast(aInfo);
  }

 protected:
  ~ObserversManager() { MOZ_ASSERT(mObservers.Length() == 0); }

  virtual void EnableNotifications() = 0;
  virtual void DisableNotifications() = 0;
  virtual void OnNotificationsDisabled() {}

 private:
  mozilla::ObserverList<InfoType> mObservers;
};

template <class InfoType>
class CachingObserversManager : public ObserversManager<InfoType> {
 public:
  InfoType GetCurrentInformation() {
    if (mHasValidCache) {
      return mInfo;
    }

    GetCurrentInformationInternal(&mInfo);
    mHasValidCache = true;
    return mInfo;
  }

  void CacheInformation(const InfoType& aInfo) {
    mHasValidCache = true;
    mInfo = aInfo;
  }

  void BroadcastCachedInformation() { this->BroadcastInformation(mInfo); }

 protected:
  virtual void GetCurrentInformationInternal(InfoType*) = 0;

  void OnNotificationsDisabled() override { mHasValidCache = false; }

 private:
  InfoType mInfo;
  bool mHasValidCache;
};

class BatteryObserversManager final
    : public CachingObserversManager<BatteryInformation> {
 protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableBatteryNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableBatteryNotifications());
  }

  void GetCurrentInformationInternal(BatteryInformation* aInfo) override {
    PROXY_IF_SANDBOXED(GetCurrentBatteryInformation(aInfo));
  }
};

class NetworkObserversManager final
    : public CachingObserversManager<NetworkInformation> {
 protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableNetworkNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableNetworkNotifications());
  }

  void GetCurrentInformationInternal(NetworkInformation* aInfo) override {
    PROXY_IF_SANDBOXED(GetCurrentNetworkInformation(aInfo));
  }
};

class WakeLockObserversManager final
    : public ObserversManager<WakeLockInformation> {
 protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableWakeLockNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableWakeLockNotifications());
  }
};

typedef mozilla::ObserverList<SensorData> SensorObserverList;
StaticAutoPtr<SensorObserverList> sSensorObservers[NUM_SENSOR_TYPE];

static SensorObserverList* GetSensorObservers(SensorType sensor_type) {
  AssertMainThread();
  MOZ_ASSERT(sensor_type < NUM_SENSOR_TYPE);

  if (!sSensorObservers[sensor_type]) {
    sSensorObservers[sensor_type] = new SensorObserverList();
  }

  return sSensorObservers[sensor_type];
}

#define MOZ_IMPL_HAL_OBSERVER(name_)                             \
  StaticAutoPtr<name_##ObserversManager> s##name_##Observers;    \
                                                                 \
  static name_##ObserversManager* name_##Observers() {           \
    if (!s##name_##Observers) {                                  \
      s##name_##Observers = new name_##ObserversManager();       \
    }                                                            \
                                                                 \
    return s##name_##Observers;                                  \
  }                                                              \
                                                                 \
  void Register##name_##Observer(name_##Observer* aObserver) {   \
    name_##Observers()->AddObserver(aObserver);                  \
  }                                                              \
                                                                 \
  void Unregister##name_##Observer(name_##Observer* aObserver) { \
    name_##Observers()->RemoveObserver(aObserver);               \
  }

MOZ_IMPL_HAL_OBSERVER(Battery)

void GetCurrentBatteryInformation(BatteryInformation* aInfo) {
  *aInfo = BatteryObservers()->GetCurrentInformation();
}

void NotifyBatteryChange(const BatteryInformation& aInfo) {
  BatteryObservers()->CacheInformation(aInfo);
  BatteryObservers()->BroadcastCachedInformation();
}

double GetBatteryTemperature() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetBatteryTemperature(), 20.0);
}

bool IsBatteryPresent() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsBatteryPresent(), true);
}

class UsbObserversManager : public CachingObserversManager<UsbStatus> {
 protected:
  void EnableNotifications() { PROXY_IF_SANDBOXED(EnableUsbNotifications()); }

  void DisableNotifications() { PROXY_IF_SANDBOXED(DisableUsbNotifications()); }

  void GetCurrentInformationInternal(UsbStatus* aStatus) {
    PROXY_IF_SANDBOXED(GetCurrentUsbStatus(aStatus));
  }
};

static UsbObserversManager sUsbObservers;

class PowerSupplyObserversManager
    : public CachingObserversManager<PowerSupplyStatus> {
 protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnablePowerSupplyNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisablePowerSupplyNotifications());
  }

  void GetCurrentInformationInternal(PowerSupplyStatus* aStatus) {
    PROXY_IF_SANDBOXED(GetCurrentPowerSupplyStatus(aStatus));
  }
};

static PowerSupplyObserversManager sPowerSupplyObservers;

class FlashlightObserversManager
    : public ObserversManager<FlashlightInformation> {
 protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableFlashlightNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableFlashlightNotifications());
  }
};

static FlashlightObserversManager sFlashlightObservers;

void RegisterFlashlightObserver(FlashlightObserver* aObserver) {
  AssertMainThread();
  sFlashlightObservers.AddObserver(aObserver);
}

void UnregisterFlashlightObserver(FlashlightObserver* aObserver) {
  AssertMainThread();
  sFlashlightObservers.RemoveObserver(aObserver);
}

void UpdateFlashlightState(const FlashlightInformation& aFlashlightState) {
  AssertMainThread();
  sFlashlightObservers.BroadcastInformation(aFlashlightState);
}

void RequestCurrentFlashlightState() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(RequestCurrentFlashlightState());
}

bool GetFlashlightEnabled() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetFlashlightEnabled(), true);
}

void SetFlashlightEnabled(bool aEnabled) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetFlashlightEnabled(aEnabled));
}

bool IsFlashlightPresent() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsFlashlightPresent(), true);
}

void RegisterUsbObserver(UsbObserver* aUsbObserver) {
  AssertMainThread();
  sUsbObservers.AddObserver(aUsbObserver);
}

void UnregisterUsbObserver(UsbObserver* aUsbObserver) {
  AssertMainThread();
  sUsbObservers.RemoveObserver(aUsbObserver);
}

void GetCurrentUsbStatus(UsbStatus* aUsbStatus) {
  AssertMainThread();
  *aUsbStatus = sUsbObservers.GetCurrentInformation();
}

void NotifyUsbStatus(const UsbStatus& aUsbStatus) {
  AssertMainThread();
  sUsbObservers.CacheInformation(aUsbStatus);
  sUsbObservers.BroadcastCachedInformation();
}

void RegisterPowerSupplyObserver(PowerSupplyObserver* aPowerSupplyObserver) {
  AssertMainThread();
  sPowerSupplyObservers.AddObserver(aPowerSupplyObserver);
}

void UnregisterPowerSupplyObserver(PowerSupplyObserver* aPowerSupplyObserver) {
  AssertMainThread();
  sPowerSupplyObservers.RemoveObserver(aPowerSupplyObserver);
}

void GetCurrentPowerSupplyStatus(PowerSupplyStatus* aPowerSupplyStatus) {
  AssertMainThread();
  *aPowerSupplyStatus = sPowerSupplyObservers.GetCurrentInformation();
}

void NotifyPowerSupplyStatus(const PowerSupplyStatus& aPowerSupplyStatus) {
  AssertMainThread();
  sPowerSupplyObservers.CacheInformation(aPowerSupplyStatus);
  sPowerSupplyObservers.BroadcastCachedInformation();
}

class FlipObserversManager final : public ObserversManager<bool> {
 protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableFlipNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableFlipNotifications());
  }
};

static FlipObserversManager sFlipObservers;

void RegisterFlipObserver(FlipObserver* aObserver) {
  AssertMainThread();
  sFlipObservers.AddObserver(aObserver);
}

void UnregisterFlipObserver(FlipObserver* aObserver) {
  AssertMainThread();
  sFlipObservers.RemoveObserver(aObserver);
}

void UpdateFlipState(const bool& aFlipState) {
  AssertMainThread();
  sFlipObservers.BroadcastInformation(aFlipState);
}

void NotifyFlipStateFromInputDevice(bool aFlipState) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(NotifyFlipStateFromInputDevice(aFlipState));
}

void RequestCurrentFlipState() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(RequestCurrentFlipState());
}

bool GetScreenEnabled() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenEnabled(), false);
}

void SetScreenEnabled(bool aEnabled) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenEnabled(aEnabled));
}

bool GetExtScreenEnabled() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetExtScreenEnabled(), false);
}

void SetExtScreenEnabled(bool aEnabled) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetExtScreenEnabled(aEnabled));
}

void EnableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSensorNotifications(aSensor));
}

void DisableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSensorNotifications(aSensor));
}

void RegisterSensorObserver(SensorType aSensor, ISensorObserver* aObserver) {
  SensorObserverList* observers = GetSensorObservers(aSensor);

  observers->AddObserver(aObserver);
  if (observers->Length() == 1) {
    EnableSensorNotifications(aSensor);
  }
}

void UnregisterSensorObserver(SensorType aSensor, ISensorObserver* aObserver) {
  SensorObserverList* observers = GetSensorObservers(aSensor);
  if (!observers->RemoveObserver(aObserver) || observers->Length() > 0) {
    return;
  }
  DisableSensorNotifications(aSensor);
}

void NotifySensorChange(const SensorData& aSensorData) {
  SensorObserverList* observers = GetSensorObservers(aSensorData.sensor());

  observers->Broadcast(aSensorData);
}

MOZ_IMPL_HAL_OBSERVER(Network)

void GetCurrentNetworkInformation(NetworkInformation* aInfo) {
  *aInfo = NetworkObservers()->GetCurrentInformation();
}

void NotifyNetworkChange(const NetworkInformation& aInfo) {
  NetworkObservers()->CacheInformation(aInfo);
  NetworkObservers()->BroadcastCachedInformation();
}

void SetNetworkType(int32_t aType) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetNetworkType(aType));
}

MOZ_IMPL_HAL_OBSERVER(WakeLock)

void ModifyWakeLock(const nsAString& aTopic, WakeLockControl aLockAdjust,
                    WakeLockControl aHiddenAdjust,
                    uint64_t aProcessID /* = CONTENT_PROCESS_ID_UNKNOWN */) {
  AssertMainThread();

  if (aProcessID == CONTENT_PROCESS_ID_UNKNOWN) {
    aProcessID = InSandbox() ? ContentChild::GetSingleton()->GetID()
                             : CONTENT_PROCESS_ID_MAIN;
  }

  PROXY_IF_SANDBOXED(
      ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust, aProcessID));
}

void GetWakeLockInfo(const nsAString& aTopic,
                     WakeLockInformation* aWakeLockInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetWakeLockInfo(aTopic, aWakeLockInfo));
}

void NotifyWakeLockChange(const WakeLockInformation& aInfo) {
  AssertMainThread();
  WakeLockObservers()->BroadcastInformation(aInfo);
}

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const ScreenOrientation& aOrientation) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(LockScreenOrientation(aOrientation), nullptr);
}

void UnlockScreenOrientation() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(UnlockScreenOrientation());
}

void EnableSwitchNotifications(SwitchDevice aDevice) {
#ifdef MOZ_WIDGET_GONK
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSwitchNotifications(aDevice));
#endif
}

void DisableSwitchNotifications(SwitchDevice aDevice) {
#ifdef MOZ_WIDGET_GONK
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSwitchNotifications(aDevice));
#endif
}

SwitchState GetCurrentSwitchState(SwitchDevice aDevice) {
#ifdef MOZ_WIDGET_GONK
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetCurrentSwitchState(aDevice),
                            SWITCH_STATE_UNKNOWN);
#endif
}

void NotifySwitchStateFromInputDevice(SwitchDevice aDevice,
                                      SwitchState aState) {
#ifdef MOZ_WIDGET_GONK
  AssertMainThread();
  PROXY_IF_SANDBOXED(NotifySwitchStateFromInputDevice(aDevice, aState));
#endif
}

typedef mozilla::ObserverList<SwitchEvent> SwitchObserverList;

static SwitchObserverList* sSwitchObserverLists = nullptr;

static SwitchObserverList& GetSwitchObserverList(SwitchDevice aDevice) {
  MOZ_ASSERT(0 <= aDevice && aDevice < NUM_SWITCH_DEVICE);
  if (sSwitchObserverLists == nullptr) {
    sSwitchObserverLists = new SwitchObserverList[NUM_SWITCH_DEVICE];
  }
  return sSwitchObserverLists[aDevice];
}

static void ReleaseObserversIfNeeded() {
  for (int i = 0; i < NUM_SWITCH_DEVICE; i++) {
    if (sSwitchObserverLists[i].Length() != 0) return;
  }

  // The length of every list is 0, no observer in the list.
  delete[] sSwitchObserverLists;
  sSwitchObserverLists = nullptr;
}

void RegisterSwitchObserver(SwitchDevice aDevice, SwitchObserver* aObserver) {
  AssertMainThread();
  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  observer.AddObserver(aObserver);
  if (observer.Length() == 1) {
    EnableSwitchNotifications(aDevice);
  }
}

void UnregisterSwitchObserver(SwitchDevice aDevice, SwitchObserver* aObserver) {
  AssertMainThread();

  if (!sSwitchObserverLists) {
    return;
  }

  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  if (!observer.RemoveObserver(aObserver) || observer.Length() > 0) {
    return;
  }

  DisableSwitchNotifications(aDevice);
  ReleaseObserversIfNeeded();
}

void NotifySwitchChange(const SwitchEvent& aEvent) {
  // When callback this notification, main thread may call unregister function
  // first. We should check if this pointer is valid.
  if (!sSwitchObserverLists) return;

  SwitchObserverList& observer = GetSwitchObserverList(aEvent.device());
  observer.Broadcast(aEvent);
}

static AlarmObserver* sAlarmObserver;

bool RegisterTheOneAlarmObserver(AlarmObserver* aObserver) {
  MOZ_ASSERT(!InSandbox());
  MOZ_ASSERT(!sAlarmObserver);

  sAlarmObserver = aObserver;
  RETURN_PROXY_IF_SANDBOXED(EnableAlarm(), false);
}

void UnregisterTheOneAlarmObserver() {
  if (sAlarmObserver) {
    sAlarmObserver = nullptr;
    PROXY_IF_SANDBOXED(DisableAlarm());
  }
}

void NotifyAlarmFired() {
  if (sAlarmObserver) {
    sAlarmObserver->Notify(void_t());
  }
}

bool SetAlarm(int32_t aSeconds, int32_t aNanoseconds) {
  // It's pointless to program an alarm nothing is going to observe ...
  MOZ_ASSERT(sAlarmObserver);
  RETURN_PROXY_IF_SANDBOXED(SetAlarm(aSeconds, aNanoseconds), false);
}

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  // n.b. The sandboxed implementation crashes; SetProcessPriority works only
  // from the main process.
  PROXY_IF_SANDBOXED(SetProcessPriority(aPid, aPriority));
}

uint32_t GetTotalSystemMemory() { return hal_impl::GetTotalSystemMemory(); }

// From HalTypes.h.
const char* ProcessPriorityToString(ProcessPriority aPriority) {
  switch (aPriority) {
    case PROCESS_PRIORITY_PARENT_PROCESS:
      return "PARENT_PROCESS";
    case PROCESS_PRIORITY_PREALLOC:
      return "PREALLOC";
    case PROCESS_PRIORITY_FOREGROUND_HIGH:
      return "FOREGROUND_HIGH";
    case PROCESS_PRIORITY_FOREGROUND:
      return "FOREGROUND";
    case PROCESS_PRIORITY_FOREGROUND_KEYBOARD:
      return "FOREGROUND_KEYBOARD";
    case PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
      return "BACKGROUND_PERCEIVABLE";
    case PROCESS_PRIORITY_BACKGROUND:
      return "BACKGROUND";
    case PROCESS_PRIORITY_UNKNOWN:
      return "UNKNOWN";
    default:
      MOZ_ASSERT(false);
      return "???";
  }
}

void StartDiskSpaceWatcher() {
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(StartDiskSpaceWatcher());
}

void StopDiskSpaceWatcher() {
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(StopDiskSpaceWatcher());
}

UniquePtr<hal::PerformanceHintSession> CreatePerformanceHintSession(
    const nsTArray<PlatformThreadHandle>& aThreads,
    mozilla::TimeDuration aTargetWorkDuration) {
  return hal_impl::CreatePerformanceHintSession(aThreads, aTargetWorkDuration);
}

const Maybe<hal::HeterogeneousCpuInfo>& GetHeterogeneousCpuInfo() {
  return hal_impl::GetHeterogeneousCpuInfo();
}

void Init() {
  MOZ_ASSERT(!sInitialized);

  if (!InSandbox()) {
    gLastIDToVibrate = new WindowIdentifier::IDArrayType();
  }

  WakeLockInit();

  sInitialized = true;
}

void Shutdown() {
  MOZ_ASSERT(sInitialized);

  gLastIDToVibrate = nullptr;

  sBatteryObservers = nullptr;
  sNetworkObservers = nullptr;
  sWakeLockObservers = nullptr;

  for (auto& sensorObserver : sSensorObservers) {
    sensorObserver = nullptr;
  }

  sInitialized = false;
}

bool IsHeadphoneEventFromInputDev() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsHeadphoneEventFromInputDev(), false);
}

nsresult StartSystemService(const char* aSvcName, const char* aArgs) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(StartSystemService(aSvcName, aArgs),
                            NS_ERROR_FAILURE);
}

void StopSystemService(const char* aSvcName) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(StopSystemService(aSvcName));
}

bool SystemServiceIsRunning(const char* aSvcName) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(SystemServiceIsRunning(aSvcName), false);
}

bool SystemServiceIsStopped(const char* aSvcName) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(SystemServiceIsStopped(aSvcName), false);
}

static StaticAutoPtr<ObserverList<FMRadioOperationInformation>>
    sFMRadioObservers;
static StaticAutoPtr<ObserverList<FMRadioRDSGroup>> sFMRadioRDSObservers;

static void InitializeFMRadioObserver() {
  if (!sFMRadioObservers) {
    sFMRadioObservers = new ObserverList<FMRadioOperationInformation>;
    sFMRadioRDSObservers = new ObserverList<FMRadioRDSGroup>;
    ClearOnShutdown(&sFMRadioRDSObservers);
    ClearOnShutdown(&sFMRadioObservers);
  }
}

void RegisterFMRadioObserver(FMRadioObserver* aFMRadioObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioObservers->AddObserver(aFMRadioObserver);
}

void UnregisterFMRadioObserver(FMRadioObserver* aFMRadioObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioObservers->RemoveObserver(aFMRadioObserver);
}

void NotifyFMRadioStatus(const FMRadioOperationInformation& aFMRadioState) {
  InitializeFMRadioObserver();
  sFMRadioObservers->Broadcast(aFMRadioState);
}

void RegisterFMRadioRDSObserver(FMRadioRDSObserver* aFMRadioRDSObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioRDSObservers->AddObserver(aFMRadioRDSObserver);
}

void UnregisterFMRadioRDSObserver(FMRadioRDSObserver* aFMRadioRDSObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioRDSObservers->RemoveObserver(aFMRadioRDSObserver);
}

void NotifyFMRadioRDSGroup(const FMRadioRDSGroup& aRDSGroup) {
  InitializeFMRadioObserver();
  sFMRadioRDSObservers->Broadcast(aRDSGroup);
}

void EnableFMRadio(const FMRadioSettings& aInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableFMRadio(aInfo));
}

void DisableFMRadio() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableFMRadio());
}

#if defined(PRODUCT_MANUFACTURER_SPRD) || defined(PRODUCT_MANUFACTURER_MTK)
void SetFMRadioAntenna(const int32_t aStatus) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetFMRadioAntenna(aStatus));
}
#endif

void FMRadioSeek(const FMRadioSeekDirection& aDirection) {
  PROXY_IF_SANDBOXED(FMRadioSeek(aDirection));
}

void GetFMRadioSettings(FMRadioSettings* aInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetFMRadioSettings(aInfo));
}

void SetFMRadioFrequency(const uint32_t aFrequency) {
  PROXY_IF_SANDBOXED(SetFMRadioFrequency(aFrequency));
}

uint32_t GetFMRadioFrequency() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetFMRadioFrequency(), 0);
}

bool IsFMRadioOn() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsFMRadioOn(), false);
}

uint32_t GetFMRadioSignalStrength() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetFMRadioSignalStrength(), 0);
}

void CancelFMRadioSeek() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(CancelFMRadioSeek());
}

bool EnableRDS(uint32_t aMask) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(EnableRDS(aMask), false);
}

void DisableRDS() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableRDS());
}

FMRadioSettings GetFMBandSettings(FMRadioCountry aCountry) {
  FMRadioSettings settings;

  switch (aCountry) {
    case FM_RADIO_COUNTRY_US:
    case FM_RADIO_COUNTRY_EU:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87800;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_JP_STANDARD:
      settings.upperLimit() = 76000;
      settings.lowerLimit() = 90000;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_CY:
    case FM_RADIO_COUNTRY_DE:
    case FM_RADIO_COUNTRY_DK:
    case FM_RADIO_COUNTRY_ES:
    case FM_RADIO_COUNTRY_FI:
    case FM_RADIO_COUNTRY_FR:
    case FM_RADIO_COUNTRY_HU:
    case FM_RADIO_COUNTRY_IR:
    case FM_RADIO_COUNTRY_IT:
    case FM_RADIO_COUNTRY_KW:
    case FM_RADIO_COUNTRY_LT:
    case FM_RADIO_COUNTRY_ML:
    case FM_RADIO_COUNTRY_NO:
    case FM_RADIO_COUNTRY_OM:
    case FM_RADIO_COUNTRY_PG:
    case FM_RADIO_COUNTRY_NL:
    case FM_RADIO_COUNTRY_CZ:
    case FM_RADIO_COUNTRY_UK:
    case FM_RADIO_COUNTRY_RW:
    case FM_RADIO_COUNTRY_SN:
    case FM_RADIO_COUNTRY_SI:
    case FM_RADIO_COUNTRY_ZA:
    case FM_RADIO_COUNTRY_SE:
    case FM_RADIO_COUNTRY_CH:
    case FM_RADIO_COUNTRY_TW:
    case FM_RADIO_COUNTRY_UA:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_VA:
    case FM_RADIO_COUNTRY_MA:
    case FM_RADIO_COUNTRY_TR:
      settings.upperLimit() = 10800;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 100;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_AU:
    case FM_RADIO_COUNTRY_BD:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_AW:
    case FM_RADIO_COUNTRY_BS:
    case FM_RADIO_COUNTRY_CO:
    case FM_RADIO_COUNTRY_KR:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_EC:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 0;
      break;
    case FM_RADIO_COUNTRY_GM:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 0;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_QA:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_SG:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_IN:
      settings.upperLimit() = 100000;
      settings.lowerLimit() = 108000;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_NZ:
      settings.upperLimit() = 100000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 50;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_USER_DEFINED:
      break;
    default:
      MOZ_ASSERT(0);
      break;
  }
  return settings;
}

bool IsFlipOpened() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsFlipOpened(), true);
}

}  // namespace mozilla::hal
