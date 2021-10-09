/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_B2G_h
#define mozilla_dom_B2G_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ActivityUtilsBinding.h"
#include "mozilla/dom/AlarmManager.h"
#include "mozilla/dom/DOMVirtualCursor.h"
#include "mozilla/dom/ExternalAPI.h"
#include "mozilla/dom/FlashlightManager.h"
#include "mozilla/dom/FlipManager.h"
#include "mozilla/dom/InputMethod.h"
#include "mozilla/dom/TetheringManagerBinding.h"
#include "mozilla/dom/NetworkStatsManagerBinding.h"

#ifdef MOZ_B2G_RIL
#  include "mozilla/dom/CellBroadcast.h"
#  include "mozilla/dom/IccManager.h"
#  include "mozilla/dom/Voicemail.h"
#  include "mozilla/dom/Telephony.h"
#  include "mozilla/dom/MobileConnectionArray.h"
#  include "mozilla/dom/DataCallManagerBinding.h"
#  include "mozilla/dom/SubsidyLockManager.h"
#  include "mozilla/dom/MobileMessageManager.h"
#endif

#ifdef MOZ_B2G_BT
#  include "mozilla/dom/bluetooth/BluetoothManager.h"
#endif
#ifdef MOZ_B2G_CAMERA
#  include "DOMCameraManager.h"
#endif
#if !defined(DISABLE_WIFI)
#  include "mozilla/dom/WifiManagerBinding.h"
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
#  include "AudioChannelManager.h"
#endif
#ifdef MOZ_B2G_FM
#  include "mozilla/dom/FMRadio.h"
#endif
#ifdef HAS_KOOST_MODULES
#  include "mozilla/dom/AuthorizationManager.h"
#  include "mozilla/dom/UserConsent.h"
#  ifdef MOZ_WIDGET_GONK
#    include "mozilla/dom/EngmodeManagerBinding.h"
#  endif
#  ifdef ENABLE_RSU
#    include "mozilla/dom/RemoteSimUnlock.h"
#  endif
#endif

#include "mozilla/dom/usb/UsbManager.h"
#include "DeviceStorage.h"
#include "mozilla/dom/DeviceStorageAreaListener.h"
#include "mozilla/dom/DownloadManagerBinding.h"
#include "mozilla/dom/PermissionsManagerBinding.h"

#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/powersupply/PowerSupplyManager.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIObserver.h"

class nsDOMDeviceStorage;

namespace mozilla {
namespace dom {

class DeviceStorageAreaListener;

class B2G final : public DOMEventTargetHelper,
                  public nsIDOMMozWakeLockListener,
                  public nsIObserver {
  nsCOMPtr<nsIGlobalObject> mOwner;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(B2G, DOMEventTargetHelper)
  NS_DECL_NSIDOMMOZWAKELOCKLISTENER
  NS_DECL_NSIOBSERVER

  explicit B2G(nsIGlobalObject* aGlobal);

  nsIGlobalObject* GetParentObject() const { return mOwner; }
  virtual JSObject* WrapObject(JSContext* aCtx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static bool CheckPermission(const nsACString& aType,
                              nsPIDOMWindowInner* aWindow);
  static bool CheckPermissionOnWorkerThread(const nsACString& aType);

  ActivityUtils* GetActivityUtils(ErrorResult& aRv);
  static bool HasWebAppsManagePermission(JSContext* /* unused */,
                                         JSObject* aGlobal);
  AlarmManager* GetAlarmManager(ErrorResult& aRv);
  already_AddRefed<Promise> GetFlashlightManager(ErrorResult& aRv);
  already_AddRefed<Promise> GetFlipManager(ErrorResult& aRv);
  InputMethod* GetInputMethod(ErrorResult& aRv);
  static bool HasInputPermission(JSContext* /* unused */, JSObject* aGlobal);
  TetheringManager* GetTetheringManager(ErrorResult& aRv);
  NetworkStatsManager* GetNetworkStats(ErrorResult& aRv);

#ifdef MOZ_B2G_RIL
  IccManager* GetIccManager(ErrorResult& aRv);
  Voicemail* GetVoicemail(ErrorResult& aRv);
  MobileConnectionArray* GetMobileConnections(ErrorResult& aRv);
  Telephony* GetTelephony(ErrorResult& aRv);
  DataCallManager* GetDataCallManager(ErrorResult& aRv);
  CellBroadcast* GetCellBroadcast(ErrorResult& aRv);
  SubsidyLockManager* GetSubsidyLockManager(ErrorResult& aRv);
  MobileMessageManager* GetMobileMessageManager(ErrorResult& aRv);
  static bool HasVoiceMailSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasMobileConnectionAndNetworkSupport(JSContext* /* unused */,
                                                   JSObject* aGlobal);
  static bool HasMobileNetworkSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
  static bool HasMobileConnectionSupport(JSContext* /* unused */,
                                         JSObject* aGlobal);
  static bool HasTelephonySupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasDataCallSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasCellBroadcastSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
  static bool HasMobileMessageSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
#endif  // MOZ_B2G_RIL

  ExternalAPI* GetExternalapi(ErrorResult& aRv);
  DOMVirtualCursor* GetVirtualCursor(ErrorResult& aRv);
  static bool HasNetworkStatsSupport(JSContext* /* unused */,
                                     JSObject* aGlobal);

#ifdef MOZ_B2G_BT
  bluetooth::BluetoothManager* GetBluetooth(ErrorResult& aRv);
#endif
#ifdef MOZ_B2G_CAMERA
  nsDOMCameraManager* GetCameras(ErrorResult& aRv);
#endif
#if !defined(DISABLE_WIFI)
  WifiManager* GetWifiManager(ErrorResult& aRv);
#endif
  static bool HasCameraSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasWifiManagerSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasTetheringManagerSupport(JSContext* /* unused */,
                                         JSObject* aGlobal);
  static bool HasUsbManagerSupport(JSContext* /* unused */, JSObject* aGlobal);
  static bool HasPowerSupplyManagerSupport(JSContext* /* unused */,
                                           JSObject* aGlobal);

  DownloadManager* GetDownloadManager(ErrorResult& aRv);
  static bool HasDownloadsPermission(JSContext* /* unused */,
                                     JSObject* aGlobal);

  static bool HasPermissionsManagerSupport(JSContext* /* unused */,
                                           JSObject* aGlobal);
  PermissionsManager* GetPermissions(ErrorResult& aRv);

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  system::AudioChannelManager* GetAudioChannelManager(ErrorResult& aRv);
#endif
#ifdef MOZ_B2G_FM
  FMRadio* GetFmRadio(ErrorResult& aRv);
  static bool HasFMRadioSupport(JSContext* /* unused */, JSObject* aGlobal);
#endif
#ifdef HAS_KOOST_MODULES
  AuthorizationManager* GetAuthorizationManager(ErrorResult& aRv);
  static bool HasAuthorizationManagerSupport(JSContext* /* unused */,
                                             JSObject* aGlobal);
  already_AddRefed<Promise> GetUserConsent(const nsAString& secret,
                                           ErrorResult& aRv);
#  ifdef MOZ_WIDGET_GONK
  EngmodeManager* GetEngmodeManager(ErrorResult& aRv);
  static bool HasEngmodeManagerSupport(JSContext* /* unused */,
                                       JSObject* aGlobal);
#  endif
#  ifdef ENABLE_RSU
  RemoteSimUnlock* GetRsu(ErrorResult& aRv);
  static bool HasRSUSupport(JSContext* /* unused */, JSObject* aGlobal);
#  endif
#endif

  UsbManager* GetUsbManager(ErrorResult& aRv);

  PowerSupplyManager* GetPowerSupplyManager(ErrorResult& aRv);

  static bool HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */);

  already_AddRefed<WakeLock> RequestWakeLock(const nsAString& aTopic,
                                             ErrorResult& aRv);

  void AddWakeLockListener(nsIDOMMozWakeLockListener* aListener);
  void RemoveWakeLockListener(nsIDOMMozWakeLockListener* aListener);
  void GetWakeLockState(const nsAString& aTopic, nsAString& aState,
                        ErrorResult& aRv);

  DeviceStorageAreaListener* GetDeviceStorageAreaListener(ErrorResult& aRv);

  already_AddRefed<nsDOMDeviceStorage> GetDeviceStorage(const nsAString& aType,
                                                        ErrorResult& aRv);

  void GetDeviceStorages(const nsAString& aType,
                         nsTArray<RefPtr<nsDOMDeviceStorage>>& aStores,
                         ErrorResult& aRv);

  already_AddRefed<nsDOMDeviceStorage> GetDeviceStorageByNameAndType(
      const nsAString& aName, const nsAString& aType, ErrorResult& aRv);

  void SetDispatchKeyToContentFirst(bool aEnable);

  IMPL_EVENT_HANDLER(storagefull);
  IMPL_EVENT_HANDLER(storagefree);

  // Initialization, main thread only
  nsresult MainThreadInit();
  // Shutting down, main thread only
  void MainThreadShutdown();

 private:
  // Warning: The constructor and destructor are called on both MAIN and WORKER
  // threads, see Navigator::B2g() in WorkerNavigator::B2g().
  // For main thread's initialization and cleaup, use MainThreadInit() and
  // MainThreadShutdown().
  ~B2G() = default;
  already_AddRefed<nsDOMDeviceStorage> FindDeviceStorage(
      const nsAString& aName, const nsAString& aType);

  nsTArray<nsWeakPtr> mDeviceStorageStores;
  RefPtr<ActivityUtils> mActivityUtils;
  RefPtr<AlarmManager> mAlarmManager;
  RefPtr<DeviceStorageAreaListener> mDeviceStorageAreaListener;
  RefPtr<FlashlightManager> mFlashlightManager;
  RefPtr<FlipManager> mFlipManager;
  RefPtr<InputMethod> mInputMethod;
  RefPtr<TetheringManager> mTetheringManager;
  RefPtr<NetworkStatsManager> mNetworkStatsManager;
#ifdef MOZ_B2G_RIL
  RefPtr<CellBroadcast> mCellBroadcast;
  RefPtr<IccManager> mIccManager;
  RefPtr<Voicemail> mVoicemail;
  RefPtr<MobileConnectionArray> mMobileConnections;
  RefPtr<Telephony> mTelephony;
  RefPtr<DataCallManager> mDataCallManager;
  RefPtr<SubsidyLockManager> mSubsidyLocks;
  RefPtr<MobileMessageManager> mMobileMessageManager;
#endif

  RefPtr<ExternalAPI> mExternalAPI;
  RefPtr<DOMVirtualCursor> mVirtualCursor;

#ifdef MOZ_B2G_BT
  RefPtr<bluetooth::BluetoothManager> mBluetooth;
#endif
#ifdef MOZ_B2G_CAMERA
  RefPtr<nsDOMCameraManager> mCameraManager;
#endif
#if !defined(DISABLE_WIFI)
  RefPtr<WifiManager> mWifiManager;
#endif

  RefPtr<DownloadManager> mDownloadManager;

  RefPtr<PermissionsManager> mPermissionsManager;
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  RefPtr<system::AudioChannelManager> mAudioChannelManager;
#endif
#ifdef MOZ_B2G_FM
  RefPtr<FMRadio> mFMRadio;
#endif
#ifdef HAS_KOOST_MODULES
  RefPtr<AuthorizationManager> mAuthorizationManager;
  RefPtr<UserConsent> mUserConsent;
#  ifdef MOZ_WIDGET_GONK
  RefPtr<EngmodeManager> mEngmodeManager;
#  endif
#  ifdef ENABLE_RSU
  RefPtr<RemoteSimUnlock> mRSU;
#  endif
#endif
  RefPtr<UsbManager> mUsbManager;
  RefPtr<PowerSupplyManager> mPowerSupplyManager;

  nsTArray<nsCOMPtr<nsIDOMMozWakeLockListener>> mListeners;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_B2G_h
