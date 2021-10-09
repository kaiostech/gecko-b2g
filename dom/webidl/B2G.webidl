/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface MozWakeLockListener;

[Exposed=(Window,Worker)]
interface B2G : EventTarget {
  // objects implementing this interface also implement the interfaces given
  // below
};

[Exposed=Window]
partial interface B2G {
  attribute EventHandler onstoragefree;
  attribute EventHandler onstoragefull;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Func="B2G::HasWebAppsManagePermission"]
  readonly attribute ActivityUtils activityUtils;
};

partial interface B2G {
  [Throws, Exposed=(Window,Worker), Pref="dom.alarm.enabled"]
  readonly attribute AlarmManager alarmManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.downloads.enabled"]
  readonly attribute DownloadManager downloadManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.flashlight.enabled"]
  Promise<FlashlightManager> getFlashlightManager();
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.flip.enabled"]
  Promise<FlipManager> getFlipManager();
};

[Exposed=Window]
partial interface B2G {
  [Throws, Func="B2G::HasInputPermission", Pref="dom.inputmethod.enabled"]
  readonly attribute InputMethod inputMethod;
};

[Exposed=Window]
partial interface B2G {
  [Throws,  Func="B2G::HasTetheringManagerSupport"]
  readonly attribute TetheringManager tetheringManager;
};

#ifdef MOZ_B2G_RIL
[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.mobileconnection.enabled", Func="B2G::HasMobileConnectionAndNetworkSupport"]
  readonly attribute MobileConnectionArray mobileConnections;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.telephony.enabled", Func="B2G::HasTelephonySupport"]
  readonly attribute Telephony telephony;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.icc.enabled", Func="B2G::HasMobileConnectionSupport"]
  readonly attribute IccManager? iccManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.datacall.enabled", Func="B2G::HasDataCallSupport"]
  readonly attribute DataCallManager? dataCallManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.subsidylock.enabled", Func="B2G::HasMobileConnectionSupport"]
  readonly attribute SubsidyLockManager? subsidyLockManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.voicemail.enabled", Func="B2G::HasVoiceMailSupport"]
  readonly attribute Voicemail voicemail;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.cellbroadcast.enabled", Func="B2G::HasCellBroadcastSupport"]
  readonly attribute CellBroadcast cellBroadcast;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.sms.enabled", Func="B2G::HasMobileMessageSupport"]
  readonly attribute MobileMessageManager mobileMessageManager;
};
#endif //MOZ_B2G_RIL

[Exposed=(Window,Worker)]
partial interface B2G {
  [Throws]
  readonly attribute ExternalAPI externalapi;
};

#ifdef MOZ_B2G_BT
[Exposed=Window]
partial interface B2G {
  // This interface requires 'bluetooth' permission
  [Throws, Func="bluetooth::BluetoothManager::HasPermission"]
  readonly attribute BluetoothManager bluetooth;
};
#endif // MOZ_B2G_BT

#ifdef MOZ_B2G_CAMERA
// nsIDOMB2GCamera
partial interface B2G {
  [Throws, Func="B2G::HasCameraSupport"]
  readonly attribute CameraManager cameras;
};
#endif // MOZ_B2G_CAMERA

#if !defined(DISABLE_WIFI)
partial interface B2G {
  [Throws, Func="B2G::HasWifiManagerSupport", Exposed=Window]
  readonly attribute WifiManager wifiManager;
};
#endif // !DISABLE_WIFI

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
[Exposed=Window]
partial interface B2G {
  [Throws]
  readonly attribute AudioChannelManager audioChannelManager;
};
#endif // MOZ_AUDIO_CHANNEL_MANAGER

#ifdef MOZ_B2G_FM
[Exposed=Window]
partial interface B2G {
  [Throws, Func="B2G::HasFMRadioSupport"]
  readonly attribute FMRadio fmRadio;
};
#endif // MOZ_B2G_FM

#ifdef HAS_KOOST_MODULES
[Exposed=(Window,Worker)]
partial interface B2G {
  [Throws, Func="B2G::HasAuthorizationManagerSupport"]
  readonly attribute AuthorizationManager authorizationManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws]
  Promise<UserConsent> getUserConsent(DOMString secret);
};

#ifdef MOZ_WIDGET_GONK
[Exposed=Window]
partial interface B2G {
  [Throws, Func="B2G::HasEngmodeManagerSupport"]
  readonly attribute EngmodeManager engmodeManager;
};
#endif
#endif

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.usb.enabled", Func="B2G::HasUsbManagerSupport"]
  readonly attribute UsbManager usbManager;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.powersupply.enabled", Func="B2G::HasPowerSupplyManagerSupport"]
  readonly attribute PowerSupplyManager powerSupplyManager;
};

#ifdef ENABLE_RSU
[Exposed=Window]
partial interface B2G {
  [Throws, Func="B2G::HasRSUSupport"]
  readonly attribute RemoteSimUnlock? rsu;
};
#endif

partial interface B2G {
  /**
   * Request a wake lock for a resource.
   *
   * A page holds a wake lock to request that a resource not be turned
   * off (or otherwise made unavailable).
   *
   * The topic is the name of a resource that might be made unavailable for
   * various reasons. For example, on a mobile device the power manager might
   * decide to turn off the screen after a period of idle time to save power.
   *
   * The resource manager checks the lock state of a topic before turning off
   * the associated resource. For example, a page could hold a lock on the
   * "screen" topic to prevent the screensaver from appearing or the screen
   * from turning off.
   *
   * The resource manager defines what each topic means and sets policy.  For
   * example, the resource manager might decide to ignore 'screen' wake locks
   * held by pages which are not visible.
   *
   * One topic can be locked multiple times; it is considered released only when
   * all locks on the topic have been released.
   *
   * The returned WakeLock object is a token of the lock.  You can
   * unlock the lock via the object's |unlock| method.  The lock is released
   * automatically when its associated window is unloaded.
   *
   * @param aTopic resource name
   */
  [Throws, Pref="dom.wakelock.enabled", Func="B2G::HasWakeLockSupport", Exposed=Window]
  WakeLock requestWakeLock(DOMString aTopic);

  /**
   * The listeners are notified when a resource changes its lock state to:
   *  - unlocked
   *  - locked but not visible
   *  - locked and visible
   */
  [Exposed=Window]
  undefined addWakeLockListener(MozWakeLockListener aListener);
  [Exposed=Window]
  undefined removeWakeLockListener(MozWakeLockListener aListener);

  /**
   * Query the wake lock state of the topic.
   *
   * Possible states are:
   *
   *  - "unlocked" - nobody holds the wake lock.
   *
   *  - "locked-foreground" - at least one window holds the wake lock,
   *    and it is visible.
   *
   *  - "locked-background" - at least one window holds the wake lock,
   *    but all of them are hidden.
   *
   * @param aTopic The resource name related to the wake lock.
   */
  [Throws, Exposed=Window]
  DOMString getWakeLockState(DOMString aTopic);
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="device.storage.enabled"]
  readonly attribute DeviceStorageAreaListener deviceStorageAreaListener;
};

// nsIDOMNavigatorDeviceStorage
[Exposed=Window]
partial interface B2G {
  [Throws, Pref="device.storage.enabled"]
  DeviceStorage? getDeviceStorage(DOMString type);
  [Throws, Pref="device.storage.enabled"]
  sequence<DeviceStorage> getDeviceStorages(DOMString type);
  [Throws, Pref="device.storage.enabled"]
  DeviceStorage? getDeviceStorageByNameAndType(DOMString name, DOMString type);
};

partial interface B2G {
  [Throws,
   Pref="dom.permissions.manager.enabled",
   Func="B2G::HasPermissionsManagerSupport",
   Exposed=Window]
  readonly attribute PermissionsManager permissions;
};

[Exposed=Window]
partial interface B2G {
  [Throws, Func="DOMVirtualCursor::HasPermission", Pref="dom.virtualcursor.enabled"]
  readonly attribute DOMVirtualCursor virtualCursor;
};

[Exposed=Window]
partial interface B2G {
  /**
   * Request to dispatch functional key events to the content process first. By
   * default, the key events are dispatched on the chrome process and then the
   * content. In some use cases, we want to give the content process has the
   * chance to process and consume the events.
   */
  [ChromeOnly]
  undefined setDispatchKeyToContentFirst(boolean enable);
};

[Exposed=Window]
partial interface B2G {
  [Throws, Pref="dom.networkStats.enabled", Func="B2G::HasNetworkStatsSupport"]
  readonly attribute NetworkStatsManager networkStats;
};
