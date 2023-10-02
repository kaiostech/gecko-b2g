/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Hal_h
#define mozilla_Hal_h

#include "base/platform_thread.h"
#include "nsTArray.h"
#include "mozilla/dom/FlashlightManager.h"
#include "mozilla/dom/FlipManager.h"
#include "mozilla/dom/powersupply/Types.h"
#include "mozilla/dom/usb/Types.h"
#include "mozilla/hal_sandbox/PHal.h"
#include "mozilla/HalScreenConfiguration.h"
#include "mozilla/HalBatteryInformation.h"
#include "mozilla/HalNetworkInformation.h"
#include "mozilla/HalWakeLockInformation.h"
#include "mozilla/HalTypes.h"
#include "mozilla/MozPromise.h"

#include <cstdint>

/*
 * Hal.h contains the public Hal API.
 *
 * By default, this file defines its functions in the hal namespace, but if
 * MOZ_HAL_NAMESPACE is defined, we'll define our functions in that namespace.
 *
 * This is used by HalImpl.h and HalSandbox.h, which define copies of all the
 * functions here in the hal_impl and hal_sandbox namespaces.
 */

class nsPIDOMWindowInner;

#ifndef MOZ_HAL_NAMESPACE
#  define MOZ_HAL_NAMESPACE hal
#  define MOZ_DEFINED_HAL_NAMESPACE 1
#endif

namespace mozilla {

// Predeclare void_t here, as including IPCMessageUtils brings in windows.h and
// causes binding compilation problems.
struct void_t;

namespace hal {

typedef Observer<void_t> AlarmObserver;

class WindowIdentifier;

}  // namespace hal

namespace MOZ_HAL_NAMESPACE {

/**
 * Initializes the HAL. This must be called before any other HAL function.
 */
void Init();

/**
 * Shuts down the HAL. Besides freeing all the used resources this will check
 * that all observers have been properly deregistered and assert if not.
 */
void Shutdown();

/**
 * Turn the default vibrator device on/off per the pattern specified
 * by |pattern|.  Each element in the pattern is the number of
 * milliseconds to turn the vibrator on or off.  The first element in
 * |pattern| is an "on" element, the next is "off", and so on.
 *
 * If |pattern| is empty, any in-progress vibration is canceled.
 *
 * Only an active window within an active tab may call Vibrate; calls
 * from inactive windows and windows on inactive tabs do nothing.
 *
 * If you're calling hal::Vibrate from the outside world, pass an
 * nsIDOMWindow* in place of the WindowIdentifier parameter.
 * The method with WindowIdentifier will be called automatically.
 */
void Vibrate(const nsTArray<uint32_t>& pattern, nsPIDOMWindowInner* aWindow);
void Vibrate(const nsTArray<uint32_t>& pattern, hal::WindowIdentifier&& id);

/**
 * Cancel a vibration started by the content window identified by
 * WindowIdentifier.
 *
 * If the window was the last window to start a vibration, the
 * cancellation request will go through even if the window is not
 * active.
 *
 * As with hal::Vibrate(), if you're calling hal::CancelVibrate from the outside
 * world, pass an nsIDOMWindow*. The method with WindowIdentifier will be called
 * automatically.
 */
void CancelVibrate(nsPIDOMWindowInner* aWindow);
void CancelVibrate(hal::WindowIdentifier&& id);

#define MOZ_DEFINE_HAL_OBSERVER(name_)                             \
  /**                                                              \
   * Inform the backend there is a new |name_| observer.           \
   * @param aObserver The observer that should be added.           \
   */                                                              \
  void Register##name_##Observer(hal::name_##Observer* aObserver); \
  /**                                                              \
   * Inform the backend a |name_| observer unregistered.           \
   * @param aObserver The observer that should be removed.         \
   */                                                              \
  void Unregister##name_##Observer(hal::name_##Observer* aObserver);

MOZ_DEFINE_HAL_OBSERVER(Battery);

/**
 * Returns the current battery information.
 */
void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

/**
 * Notify of a change in the battery state.
 * @param aBatteryInfo The new battery information.
 */
void NotifyBatteryChange(const hal::BatteryInformation& aBatteryInfo);

/**
 * Get battery temperature of device
 */
double GetBatteryTemperature();

/**
 * Determine the battery is present or not
 */
bool IsBatteryPresent();

/**
 * Inform the usb backend there is a new usb observer.
 * @param aUsbObserver The observer that should be added.
 */
void RegisterUsbObserver(UsbObserver* aUsbObserver);

/**
 * Inform the usb backend a usb observer unregistered.
 * @param aUsbObserver The observer that should be removed.
 */
void UnregisterUsbObserver(UsbObserver* aUsbObserver);

/**
 * Returns the current usb status.
 */
void GetCurrentUsbStatus(hal::UsbStatus* aUsbStatus);

/**
 * Notify of a change in the usb status.
 * @param aUsbStatus The new usb status.
 */
void NotifyUsbStatus(const hal::UsbStatus& aUsbStatus);

/**
 * Inform the powersupply backend there is a new powersupply observer.
 * @param aPowerSupplyObserver The observer that should be added.
 */
void RegisterPowerSupplyObserver(PowerSupplyObserver* aPowerSupplyObserver);

/**
 * Inform the powersupply backend a powersupply observer unregistered.
 * @param aPowerSupplyObserver The observer that should be removed.
 */
void UnregisterPowerSupplyObserver(PowerSupplyObserver* aPowerSupplyObserver);

/**
 * Returns the current power supply status.
 */
void GetCurrentPowerSupplyStatus(hal::PowerSupplyStatus* aPowerSupplyStatus);

/**
 * Notify of a change in the power supply status.
 * @param aPowerSupplyStatus The new power supply status.
 */
void NotifyPowerSupplyStatus(const hal::PowerSupplyStatus& aPowerSupplyStatus);

/**
 * Inform the flashlightmanager backend there is a new flashlight observer.
 * @param aFlashlightObserver The observer that should be added.
 */
void RegisterFlashlightObserver(
    mozilla::dom::FlashlightObserver* aFlashlightObserver);

/**
 * Inform the flashlightmanager backend a flashlight observer unregistered.
 * @param aFlashlightObserver The observer that should be removed.
 */
void UnregisterFlashlightObserver(
    mozilla::dom::FlashlightObserver* aFlashlightObserver);

/**
 * Request the current Flashlight state. If the request is made by content
 * process, result will be returned asynchronously.
 */
void RequestCurrentFlashlightState();

/**
 * Update the Flashlight state in each Flashlight observers.
 * @param aFlashlightState The new state of Flashlight.
 */
void UpdateFlashlightState(const hal::FlashlightInformation& aFlashlightInfo);

/**
 * Get the flashlight state which is enabled or not
 */
bool GetFlashlightEnabled();

/**
 * Enable or disable flashlight.
 */
void SetFlashlightEnabled(bool aEnabled);

/**
 * Determine the Flashlight is present or not.
 */
bool IsFlashlightPresent();

/**
 * Determine the Flip is open or not
 */
bool IsFlipOpened();

/**
 * Inform the flipmanager backend there is a new flip observer.
 * @param aFlipObserver The observer that should be added.
 */
void RegisterFlipObserver(mozilla::dom::FlipObserver* aFlipObserver);

/**
 * Inform the flipmanager backend a flip observer unregistered.
 * @param aFlipObserver The observer that should be removed.
 */
void UnregisterFlipObserver(mozilla::dom::FlipObserver* aFlipObserver);

/**
 * Notify of a change in the flip state from input device.
 * @param aFlipState The new flip state.
 */
void NotifyFlipStateFromInputDevice(bool aFlipState);

/**
 * Request the current flip state. If the request is made by content process,
 * result will be returned asynchronously.
 */
void RequestCurrentFlipState();

/**
 * Update the flip state in each flip observers.
 * @param aFlipState The new flip state.
 */
void UpdateFlipState(const bool& aFlipState);

/**
 * Determine whether the device's screen is currently enabled.
 */
bool GetScreenEnabled();

/**
 * Enable or disable the device's screen.
 *
 * Note that it may take a few seconds for the screen to turn on or off.
 */
void SetScreenEnabled(bool aEnabled);

/**
 * Determine whether the device's external screen is currently enabled.
 */
bool GetExtScreenEnabled();

/**
 * Enable or disable the device's external screen.
 */
void SetExtScreenEnabled(bool aEnabled);

/**
 * Register an observer for the sensor of given type.
 *
 * The observer will receive data whenever the data generated by the
 * sensor is avaiable.
 */
void RegisterSensorObserver(hal::SensorType aSensor,
                            hal::ISensorObserver* aObserver);

/**
 * Unregister an observer for the sensor of given type.
 */
void UnregisterSensorObserver(hal::SensorType aSensor,
                              hal::ISensorObserver* aObserver);

/**
 * Post a value generated by a sensor.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifySensorChange(const hal::SensorData& aSensorData);

/**
 * Enable sensor notifications from the backend
 *
 * This method is only visible from implementation of sensor manager.
 * Rest of the system should not try this.
 */
void EnableSensorNotifications(hal::SensorType aSensor);

/**
 * Disable sensor notifications from the backend
 *
 * This method is only visible from implementation of sensor manager.
 * Rest of the system should not try this.
 */
void DisableSensorNotifications(hal::SensorType aSensor);

MOZ_DEFINE_HAL_OBSERVER(Network);

/**
 * Returns the current network information.
 */
void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);

/**
 * Notify of a change in the network state.
 * @param aNetworkInfo The new network information.
 */
void NotifyNetworkChange(const hal::NetworkInformation& aNetworkInfo);

/**
 * Set active network connection type.
 * @param aType The network connection type.
 */
void SetNetworkType(int32_t aType);

/**
 * Enable wake lock notifications from the backend.
 *
 * This method is only used by WakeLockObserversManager.
 */
void EnableWakeLockNotifications();

/**
 * Disable wake lock notifications from the backend.
 *
 * This method is only used by WakeLockObserversManager.
 */
void DisableWakeLockNotifications();

MOZ_DEFINE_HAL_OBSERVER(WakeLock);

/**
 * Adjust a wake lock's counts on behalf of a given process.
 *
 * In most cases, you shouldn't need to pass the aProcessID argument; the
 * default of CONTENT_PROCESS_ID_UNKNOWN is probably what you want.
 *
 * @param aTopic        lock topic
 * @param aLockAdjust   to increase or decrease active locks
 * @param aHiddenAdjust to increase or decrease hidden locks
 * @param aProcessID    indicates which process we're modifying the wake lock
 *                      on behalf of.  It is interpreted as
 *
 *                      CONTENT_PROCESS_ID_UNKNOWN: The current process
 *                      CONTENT_PROCESS_ID_MAIN: The root process
 *                      X: The process with ContentChild::GetID() == X
 */
void ModifyWakeLock(const nsAString& aTopic, hal::WakeLockControl aLockAdjust,
                    hal::WakeLockControl aHiddenAdjust,
                    uint64_t aProcessID = hal::CONTENT_PROCESS_ID_UNKNOWN);

/**
 * Query the wake lock numbers of aTopic.
 * @param aTopic        lock topic
 * @param aWakeLockInfo wake lock numbers
 */
void GetWakeLockInfo(const nsAString& aTopic,
                     hal::WakeLockInformation* aWakeLockInfo);

/**
 * Notify of a change in the wake lock state.
 * @param aWakeLockInfo The new wake lock information.
 */
void NotifyWakeLockChange(const hal::WakeLockInformation& aWakeLockInfo);

/**
 * Lock the screen orientation to the specific orientation.
 * @return A promise indicating that the screen orientation has been locked.
 */
[[nodiscard]] RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation);

/**
 * Unlock the screen orientation.
 */
void UnlockScreenOrientation();

/**
 * Register an observer for the switch of given SwitchDevice.
 *
 * The observer will receive data whenever the data generated by the
 * given switch.
 */
void RegisterSwitchObserver(hal::SwitchDevice aDevice,
                            hal::SwitchObserver* aSwitchObserver);

/**
 * Unregister an observer for the switch of given SwitchDevice.
 */
void UnregisterSwitchObserver(hal::SwitchDevice aDevice,
                              hal::SwitchObserver* aSwitchObserver);

/**
 * Notify the state of the switch.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifySwitchChange(const hal::SwitchEvent& aEvent);

/**
 * Get current switch information.
 */
hal::SwitchState GetCurrentSwitchState(hal::SwitchDevice aDevice);

/**
 * Notify switch status change from input device.
 */
void NotifySwitchStateFromInputDevice(hal::SwitchDevice aDevice,
                                      hal::SwitchState aState);

/**
 * Register an observer that is notified when a programmed alarm
 * expires.
 *
 * Currently, there can only be 0 or 1 alarm observers.
 */
[[nodiscard]] bool RegisterTheOneAlarmObserver(hal::AlarmObserver* aObserver);

/**
 * Unregister the alarm observer.  Doing so will implicitly cancel any
 * programmed alarm.
 */
void UnregisterTheOneAlarmObserver();

/**
 * Notify that the programmed alarm has expired.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifyAlarmFired();

/**
 * Program the real-time clock to expire at the time |aSeconds|,
 * |aNanoseconds|.  These specify a point in real time relative to the
 * UNIX epoch.  The alarm will fire at this time point even if the
 * real-time clock is changed; that is, this alarm respects changes to
 * the real-time clock.  Return true iff the alarm was programmed.
 *
 * The alarm can be reprogrammed at any time.
 *
 * This API is currently only allowed to be used from non-sandboxed
 * contexts.
 */
[[nodiscard]] bool SetAlarm(int32_t aSeconds, int32_t aNanoseconds);

/**
 * Set the priority of the given process.
 *
 * Exactly what this does will vary between platforms.  On *nix we might give
 * background processes higher nice values.  On other platforms, we might
 * ignore this call entirely.
 */
void SetProcessPriority(int aPid, hal::ProcessPriority aPriority);

/**
 * Get total system memory of device being run on in bytes.
 *
 * Returns 0 if we are unable to determine this information from /proc/meminfo.
 */
uint32_t GetTotalSystemMemory();

/**
 * Determine whether the headphone switch event is from input device
 */
bool IsHeadphoneEventFromInputDev();

/**
 * Start the system service with the specified name and arguments.
 */
nsresult StartSystemService(const char* aSvcName, const char* aArgs);

/**
 * Stop the system service with the specified name.
 */
void StopSystemService(const char* aSvcName);

/**
 * Determine whether the system service with the specified name is running.
 */
bool SystemServiceIsRunning(const char* aSvcName);
bool SystemServiceIsStopped(const char* aSvcName);

/**
 * Register an observer for the FM radio.
 */
void RegisterFMRadioObserver(hal::FMRadioObserver* aRadioObserver);

/**
 * Unregister the observer for the FM radio.
 */
void UnregisterFMRadioObserver(hal::FMRadioObserver* aRadioObserver);

/**
 * Register an observer for the FM radio.
 */
void RegisterFMRadioRDSObserver(hal::FMRadioRDSObserver* aRDSObserver);

/**
 * Unregister the observer for the FM radio.
 */
void UnregisterFMRadioRDSObserver(hal::FMRadioRDSObserver* aRDSObserver);

/**
 * Notify observers that a call to EnableFMRadio, DisableFMRadio, or FMRadioSeek
 * has completed, and indicate what the call returned.
 */
void NotifyFMRadioStatus(const hal::FMRadioOperationInformation& aRadioState);

/**
 * Notify observers of new RDS data
 * This can be called on any thread.
 */
void NotifyFMRadioRDSGroup(const hal::FMRadioRDSGroup& aRDSGroup);

/**
 * Enable the FM radio and configure it according to the settings in aInfo.
 */
void EnableFMRadio(const hal::FMRadioSettings& aInfo);

/**
 * Disable the FM radio.
 */
void DisableFMRadio();

#if defined(PRODUCT_MANUFACTURER_SPRD) || defined(PRODUCT_MANUFACTURER_MTK)
/**
 * Update FM Internal Antenna state
 */
void SetFMRadioAntenna(const int32_t aStatus);
#endif

/**
 * Seek to an available FM radio station.
 *
 * This can be called off main thread, but all calls must be completed
 * before calling DisableFMRadio.
 */
void FMRadioSeek(const hal::FMRadioSeekDirection& aDirection);

/**
 * Get the current FM radio settings.
 */
void GetFMRadioSettings(hal::FMRadioSettings* aInfo);

/**
 * Set the FM radio's frequency.
 *
 * This can be called off main thread, but all calls must be completed
 * before calling DisableFMRadio.
 */
void SetFMRadioFrequency(const uint32_t frequency);

/**
 * Get the FM radio's frequency.
 */
uint32_t GetFMRadioFrequency();

/**
 * Get FM radio power state
 */
bool IsFMRadioOn();

/**
 * Get FM radio signal strength
 */
uint32_t GetFMRadioSignalStrength();

/**
 * Cancel FM radio seeking
 */
void CancelFMRadioSeek();

/**
 * Get FM radio band settings by country.
 */
hal::FMRadioSettings GetFMBandSettings(hal::FMRadioCountry aCountry);

/**
 * Enable RDS data reception
 */
bool EnableRDS(uint32_t aMask);

/**
 * Disable RDS data reception
 */
void DisableRDS();

/**
 * Start monitoring disk space for low space situations.
 *
 * This API is currently only allowed to be used from the main process.
 */
void StartDiskSpaceWatcher();

/**
 * Stop monitoring disk space for low space situations.
 *
 * This API is currently only allowed to be used from the main process.
 */
void StopDiskSpaceWatcher();

/*
 * Creates a PerformanceHintSession.
 *
 * A PerformanceHintSession represents a workload shared by a group of threads
 * that should be completed in a target duration each cycle.
 *
 * Each cycle, the actual work duration should be reported using
 * PerformanceHintSession::ReportActualWorkDuration(). The system can then
 * adjust the scheduling accordingly in order to achieve the target.
 */
UniquePtr<hal::PerformanceHintSession> CreatePerformanceHintSession(
    const nsTArray<PlatformThreadHandle>& aThreads,
    mozilla::TimeDuration aTargetWorkDuration);

/**
 * Returns information categorizing the CPUs on the system by performance class.
 *
 * Returns Nothing if we are unable to calculate the required information.
 *
 * See the definition of hal::HeterogeneousCpuInfo for more details.
 */
const Maybe<hal::HeterogeneousCpuInfo>& GetHeterogeneousCpuInfo();

}  // namespace MOZ_HAL_NAMESPACE
}  // namespace mozilla

#ifdef MOZ_DEFINED_HAL_NAMESPACE
#  undef MOZ_DEFINED_HAL_NAMESPACE
#  undef MOZ_HAL_NAMESPACE
#endif

#endif  // mozilla_Hal_h
