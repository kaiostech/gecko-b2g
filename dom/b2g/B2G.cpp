/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/B2G.h"
#include "mozilla/dom/B2GBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/EventStateManager.h"
#include "nsIPermissionManager.h"

namespace mozilla {
namespace dom {

B2G::B2G(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal), mOwner(aGlobal) {
  // Warning: The constructor and destructor are called on both MAIN and WORKER
  // threads, see Navigator::B2g() in WorkerNavigator::B2g().
  // For main thread's initialization and cleaup, use MainThreadInit() and
  // MainThreadShutdown().
  MOZ_ASSERT(aGlobal);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(B2G)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozWakeLockListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(B2G, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(B2G, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(B2G)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(B2G, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(B2G, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActivityUtils)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAlarmManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeviceStorageAreaListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFlashlightManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFlipManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputMethod)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPermissionsManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTetheringManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNetworkStatsManager)
#ifdef MOZ_B2G_RIL
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIccManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCellBroadcast)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoicemail)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileConnections)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDataCallManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSubsidyLocks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileMessageManager)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExternalAPI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVirtualCursor)
#ifdef MOZ_B2G_BT
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBluetooth)
#endif
#ifdef MOZ_B2G_CAMERA
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCameraManager)
#endif
#if !defined(DISABLE_WIFI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWifiManager)
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioChannelManager)
#endif
#ifdef MOZ_B2G_FM
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFMRadio)
#endif
#ifdef HAS_KOOST_MODULES
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAuthorizationManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUserConsent)
#  ifdef MOZ_WIDGET_GONK
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEngmodeManager)
#  endif
#  ifdef ENABLE_RSU
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRSU)
#  endif
#endif

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUsbManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPowerSupplyManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void B2G::MainThreadShutdown() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mFlashlightManager) {
    mFlashlightManager->Shutdown();
    mFlashlightManager = nullptr;
  }

  if (mFlipManager) {
    mFlipManager->Shutdown();
    mFlipManager = nullptr;
  }

#ifdef MOZ_B2G_RIL
  if (mIccManager) {
    mIccManager->Shutdown();
    mIccManager = nullptr;
  }

  if (mMobileMessageManager) {
    mMobileMessageManager->Shutdown();
    mMobileMessageManager = nullptr;
  }
#endif

#ifdef MOZ_B2G_FM
  if (mFMRadio) {
    mFMRadio->Shutdown();
    mFMRadio = nullptr;
  }
#endif

  if (mPowerSupplyManager) {
    mPowerSupplyManager->Shutdown();
    mPowerSupplyManager = nullptr;
  }

  mListeners.Clear();

  if (mUsbManager) {
    mUsbManager->Shutdown();
    mUsbManager = nullptr;
  }

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  if (mAudioChannelManager) {
    mAudioChannelManager->Shutdown();
    mAudioChannelManager = nullptr;
  }
#endif

#ifdef MOZ_B2G_CAMERA
  mCameraManager = nullptr;
#endif

  uint32_t len = mDeviceStorageStores.Length();
  for (uint32_t i = 0; i < len; ++i) {
    RefPtr<nsDOMDeviceStorage> ds = do_QueryReferent(mDeviceStorageStores[i]);
    if (ds) {
      ds->Shutdown();
    }
  }
  mDeviceStorageStores.Clear();

  if (mDeviceStorageAreaListener) {
    mDeviceStorageAreaListener = nullptr;
  }

  RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
  if (pmService) {
    pmService->RemoveWakeLockListener(this);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "b2g-disk-storage-state");
  }
}

JSObject* B2G::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) {
  return B2G_Binding::Wrap(cx, this, aGivenProto);
}

bool B2G::CheckPermission(const nsACString& aType,
                          nsPIDOMWindowInner* aWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Grab the principal of the document
  if (NS_WARN_IF(!aWindow)) {
    return false;
  }

  RefPtr<Document> doc = aWindow->GetDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }
  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  if (NS_WARN_IF(!principal)) {
    return false;
  }

  // Always grant all permissions to chrome callers.
  if (principal->IsSystemPrincipal()) {
    return true;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (NS_WARN_IF(!permMgr)) {
    return false;
  }

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv =
      permMgr->TestPermissionFromPrincipal(principal, aType, &permission);
  if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
    return false;
  }

  return true;
}

class CheckPermissionRunnable final : public WorkerMainThreadRunnable {
 public:
  explicit CheckPermissionRunnable(const nsCString& aType)
      : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate(),
                                 "B2G::CheckPermissionRunnable"_ns),
        mIsAllowed(false),
        mType(aType) {}

  bool MainThreadRun() override {
    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
    nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    nsresult rv =
        permMgr->TestPermissionFromPrincipal(principal, mType, &permission);
    if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
      mIsAllowed = false;
    } else {
      mIsAllowed = true;
    }
    return true;
  }

  bool mIsAllowed;

 private:
  ~CheckPermissionRunnable() = default;
  nsCString mType;
};

bool B2G::CheckPermissionOnWorkerThread(const nsACString& aType) {
  RefPtr<CheckPermissionRunnable> r =
      new CheckPermissionRunnable((nsCString)aType);

  ErrorResult rv;
  r->Dispatch(Canceling, rv);
  if (rv.Failed()) {
    return false;
  }

  return r->mIsAllowed;
}

ActivityUtils* B2G::GetActivityUtils(ErrorResult& aRv) {
  if (!mActivityUtils) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mActivityUtils = ConstructJSImplementation<ActivityUtils>(
        "@mozilla.org/activity-utils;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mActivityUtils;
}

/* static */
bool B2G::HasWebAppsManagePermission(JSContext* /* unused */,
                                     JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return B2G::CheckPermission("webapps-manage"_ns, innerWindow);
}

AlarmManager* B2G::GetAlarmManager(ErrorResult& aRv) {
  if (!mAlarmManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsresult rv;
    mAlarmManager = AlarmManager::Create(GetParentObject(), rv);

    if (NS_WARN_IF(NS_FAILED(rv)) || !mAlarmManager) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  return mAlarmManager;
}

already_AddRefed<Promise> B2G::GetFlashlightManager(ErrorResult& aRv) {
  if (!mFlashlightManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    if (!CheckPermission("flashlight"_ns, innerWindow)) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    mFlashlightManager = new FlashlightManager(innerWindow);
    mFlashlightManager->Init();
  }

  RefPtr<Promise> p = mFlashlightManager->GetPromise(aRv);
  return p.forget();
}

already_AddRefed<Promise> B2G::GetFlipManager(ErrorResult& aRv) {
  if (!mFlipManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    if (!CheckPermission("flip"_ns, innerWindow)) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    mFlipManager = new FlipManager(innerWindow);
    mFlipManager->Init();
  }

  RefPtr<Promise> p = mFlipManager->GetPromise(aRv);
  return p.forget();
}

/* static */
bool B2G::HasInputPermission(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return B2G::CheckPermission("input"_ns, innerWindow);
}

InputMethod* B2G::GetInputMethod(ErrorResult& aRv) {
  if (!mInputMethod) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    if (!CheckPermission("input"_ns, innerWindow)) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
    mInputMethod = new InputMethod(GetParentObject());
  }
  return mInputMethod;
}

TetheringManager* B2G::GetTetheringManager(ErrorResult& aRv) {
  if (!mTetheringManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mTetheringManager = ConstructJSImplementation<TetheringManager>(
        "@mozilla.org/tetheringmanager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return mTetheringManager;
}

#ifdef MOZ_B2G_RIL
IccManager* B2G::GetIccManager(ErrorResult& aRv) {
  if (!mIccManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mIccManager = new IccManager(GetParentObject());
  }

  return mIccManager;
}

CellBroadcast* B2G::GetCellBroadcast(ErrorResult& aRv) {
  if (!mCellBroadcast) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mCellBroadcast = CellBroadcast::Create(GetParentObject(), aRv);
  }

  return mCellBroadcast;
}

Voicemail* B2G::GetVoicemail(ErrorResult& aRv) {
  if (!mVoicemail) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mVoicemail = Voicemail::Create(GetParentObject(), aRv);
  }

  return mVoicemail;
}

MobileConnectionArray* B2G::GetMobileConnections(ErrorResult& aRv) {
  if (!mMobileConnections) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mMobileConnections = new MobileConnectionArray(innerWindow);
  }

  return mMobileConnections;
}

Telephony* B2G::GetTelephony(ErrorResult& aRv) {
  if (!mTelephony) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mTelephony = Telephony::Create(GetParentObject(), aRv);
  }

  return mTelephony;
}

DataCallManager* B2G::GetDataCallManager(ErrorResult& aRv) {
  if (!mDataCallManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mDataCallManager = ConstructJSImplementation<DataCallManager>(
        "@mozilla.org/datacallmanager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mDataCallManager;
}

SubsidyLockManager* B2G::GetSubsidyLockManager(ErrorResult& aRv) {
  if (!mSubsidyLocks) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
  }
  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  mSubsidyLocks = new SubsidyLockManager(innerWindow);

  return mSubsidyLocks;
}

MobileMessageManager* B2G::GetMobileMessageManager(ErrorResult& aRv) {
  if (!mMobileMessageManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mMobileMessageManager = new MobileMessageManager(innerWindow);
    if (mMobileMessageManager) {
      mMobileMessageManager->Init();
    }
  }

  return mMobileMessageManager;
}

/* static */
bool B2G::HasVoiceMailSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("voicemail"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasMobileNetworkSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("mobilenetwork"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasMobileConnectionSupport(JSContext* /* unused */,
                                     JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("mobileconnection"_ns, innerWindow)
                     : false;
}

/* static */
bool B2G::HasMobileConnectionAndNetworkSupport(JSContext* /* unused */,
                                               JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? (CheckPermission("mobilenetwork"_ns, innerWindow) ||
                        CheckPermission("mobileconnection"_ns, innerWindow))
                     : false;
}

/* static */
bool B2G::HasTelephonySupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("telephony"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasDataCallSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("datacall"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasCellBroadcastSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("cellbroadcast"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasMobileMessageSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("sms"_ns, innerWindow) : false;
}
#endif

ExternalAPI* B2G::GetExternalapi(ErrorResult& aRv) {
  if (!mExternalAPI) {
    mExternalAPI = ExternalAPI::Create(mOwner);
  }

  return mExternalAPI;
}

DOMVirtualCursor* B2G::GetVirtualCursor(ErrorResult& aRv) {
  if (!mVirtualCursor) {
    mVirtualCursor = DOMVirtualCursor::Create(mOwner);
  }

  return mVirtualCursor;
}

#ifdef MOZ_B2G_BT
bluetooth::BluetoothManager* B2G::GetBluetooth(ErrorResult& aRv) {
  if (!mBluetooth) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mBluetooth = bluetooth::BluetoothManager::Create(innerWindow);
  }
  return mBluetooth;
}
#endif  // MOZ_B2G_BT

#ifdef MOZ_B2G_CAMERA
nsDOMCameraManager* B2G::GetCameras(ErrorResult& aRv) {
  if (!mCameraManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mCameraManager = nsDOMCameraManager::CreateInstance(innerWindow);
  }

  return mCameraManager;
}
#endif  // MOZ_B2G_CAMERA

#if !defined(DISABLE_WIFI)
WifiManager* B2G::GetWifiManager(ErrorResult& aRv) {
  if (!mWifiManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mWifiManager = ConstructJSImplementation<WifiManager>(
        "@mozilla.org/wifimanager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mWifiManager;
}
#endif  // !DISABLE_WIFI

/* static */
bool B2G::HasCameraSupport(JSContext* /* unused */, JSObject* aGlobal) {
#ifndef MOZ_B2G_CAMERA
  return false;
#else
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("camera"_ns, innerWindow) : false;
#endif
}

/* static */
bool B2G::HasWifiManagerSupport(JSContext* /* unused */, JSObject* aGlobal) {
#if defined(DISABLE_WIFI)
  return false;
#endif

  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("wifi-manage"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasTetheringManagerSupport(JSContext* /* unused */,
                                     JSObject* aGlobal) {
#if !defined(MOZ_WIDGET_GONK)
  return false;
#endif

  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("tethering"_ns, innerWindow) : false;
}

/* static */
bool B2G::HasNetworkStatsSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("networkstats-manage"_ns, innerWindow)
                     : false;
}

DownloadManager* B2G::GetDownloadManager(ErrorResult& aRv) {
  if (!mDownloadManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mDownloadManager = ConstructJSImplementation<DownloadManager>(
        "@mozilla.org/download/manager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mDownloadManager;
}

/* static */
bool B2G::HasDownloadsPermission(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return B2G::CheckPermission("downloads"_ns, innerWindow);
}

/* static */
bool B2G::HasPermissionsManagerSupport(JSContext* /* unused */,
                                       JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return B2G::CheckPermission("permissions"_ns, innerWindow);
}

PermissionsManager* B2G::GetPermissions(ErrorResult& aRv) {
  if (!mPermissionsManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mPermissionsManager = ConstructJSImplementation<PermissionsManager>(
        "@mozilla.org/permissions-manager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mPermissionsManager;
}

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
system::AudioChannelManager* B2G::GetAudioChannelManager(ErrorResult& aRv) {
  if (!mAudioChannelManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mAudioChannelManager = new system::AudioChannelManager();
    mAudioChannelManager->Init(mOwner);
  }
  return mAudioChannelManager;
}
#endif

#ifdef MOZ_B2G_FM
FMRadio* B2G::GetFmRadio(ErrorResult& aRv) {
  if (!mFMRadio) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mFMRadio = new FMRadio();
    mFMRadio->Init(mOwner);
  }
  return mFMRadio;
}

/* static */
bool B2G::HasFMRadioSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("fmradio"_ns, innerWindow) : false;
}
#endif

#ifdef HAS_KOOST_MODULES
AuthorizationManager* B2G::GetAuthorizationManager(ErrorResult& aRv) {
  if (!mAuthorizationManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    // for worker thread, we check permission here instead of by webIDL
    // visibility func
    if (!NS_IsMainThread() &&
        !CheckPermissionOnWorkerThread("cloud-authorization"_ns)) {
      aRv.Throw(NS_ERROR_DOM_NOT_ALLOWED_ERR);
      return nullptr;
    }

    mAuthorizationManager = new AuthorizationManager(GetParentObject());
  }
  return mAuthorizationManager;
}

/* static */
bool B2G::HasAuthorizationManagerSupport(JSContext* /* unused */,
                                         JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);

  if (NS_IsMainThread()) {
    return innerWindow ? CheckPermission("cloud-authorization"_ns, innerWindow)
                       : false;
  } else {
    // defer checking permission to the time of construction
    return true;
  }
}

already_AddRefed<Promise> B2G::GetUserConsent(const nsAString& secret,
                                              ErrorResult& aRv) {
  if (!mUserConsent) {
    mUserConsent = UserConsent::Create(mOwner);
  }

  RefPtr<Promise> p = mUserConsent->GetUserConsent(secret);
  return p.forget();
}

#  ifdef MOZ_WIDGET_GONK
EngmodeManager* B2G::GetEngmodeManager(ErrorResult& aRv) {
  if (!mEngmodeManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mEngmodeManager = ConstructJSImplementation<EngmodeManager>(
        "@kaiostech.com/engmode/manager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return mEngmodeManager;
}

/* static */
bool B2G::HasEngmodeManagerSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return B2G::CheckPermission("engmode"_ns, innerWindow);
}
#  endif
#  ifdef ENABLE_RSU
RemoteSimUnlock* B2G::GetRsu(ErrorResult& aRv) {
  if (mRSU) {
    return mRSU;
  }
  if (!mOwner) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  mRSU = new RemoteSimUnlock(mOwner);
  return mRSU;
}

/* static */
bool B2G::HasRSUSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  if (NS_IsMainThread()) {
    return innerWindow ? CheckPermission("rsu"_ns, innerWindow) : false;
  } else {
    return CheckPermissionOnWorkerThread("rsu"_ns);
  }
}
#  endif
#endif

/* static */
bool B2G::HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */) {
  nsCOMPtr<nsIPowerManagerService> pmService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  // No service means no wake lock support
  return !!pmService;
}

already_AddRefed<WakeLock> B2G::RequestWakeLock(const nsAString& aTopic,
                                                ErrorResult& aRv) {
  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
  // Maybe it went away for some reason... Or maybe we're just called
  // from our XPCOM method.
  if (!pmService) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return pmService->NewWakeLock(aTopic, innerWindow, aRv);
}

void B2G::AddWakeLockListener(nsIDOMMozWakeLockListener* aListener) {
  if (!mListeners.Contains(aListener)) {
    mListeners.AppendElement(aListener);
  }
}

void B2G::RemoveWakeLockListener(nsIDOMMozWakeLockListener* aListener) {
  mListeners.RemoveElement(aListener);
}

void B2G::GetWakeLockState(const nsAString& aTopic, nsAString& aState,
                           ErrorResult& aRv) {
  RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();

  if (pmService) {
    aRv = pmService->GetWakeLockState(aTopic, aState);
  } else {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

NS_IMETHODIMP
B2G::Callback(const nsAString& aTopic, const nsAString& aState) {
  /**
   * We maintain a local listener list instead of using the global
   * list so that when the window is destroyed we don't have to
   * cleanup the mess.
   * Copy the listeners list before we walk through the callbacks
   * because the callbacks may install new listeners. We expect no
   * more than one listener per window, so it shouldn't be too long.
   */
  const CopyableAutoTArray<nsCOMPtr<nsIDOMMozWakeLockListener>, 2> listeners =
      mListeners;

  for (uint32_t i = 0; i < listeners.Length(); ++i) {
    listeners[i]->Callback(aTopic, aState);
  }

  return NS_OK;
}

DeviceStorageAreaListener* B2G::GetDeviceStorageAreaListener(ErrorResult& aRv) {
  if (!mDeviceStorageAreaListener) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    if (!innerWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    if (!innerWindow || !innerWindow->GetOuterWindow() ||
        !innerWindow->GetDocShell()) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    mDeviceStorageAreaListener = new DeviceStorageAreaListener(innerWindow);
  }

  return mDeviceStorageAreaListener;
}

already_AddRefed<nsDOMDeviceStorage> B2G::FindDeviceStorage(
    const nsAString& aName, const nsAString& aType) {
  auto i = mDeviceStorageStores.Length();

  if (!mOwner) {
    return nullptr;
  }

  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    return nullptr;
  }

  while (i > 0) {
    --i;
    RefPtr<nsDOMDeviceStorage> storage =
        do_QueryReferent(mDeviceStorageStores[i]);
    if (storage) {
      if (storage->Equals(innerWindow, aName, aType)) {
        return storage.forget();
      }
    } else {
      mDeviceStorageStores.RemoveElementAt(i);
    }
  }
  return nullptr;
}

already_AddRefed<nsDOMDeviceStorage> B2G::GetDeviceStorage(
    const nsAString& aType, ErrorResult& aRv) {
  if (!mOwner) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!innerWindow || !innerWindow->GetOuterWindow() ||
      !innerWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsString name;
  nsDOMDeviceStorage::GetDefaultStorageName(aType, name);
  RefPtr<nsDOMDeviceStorage> storage = FindDeviceStorage(name, aType);
  if (storage) {
    return storage.forget();
  }

  nsDOMDeviceStorage::CreateDeviceStorageFor(innerWindow, aType,
                                             getter_AddRefs(storage));

  if (!storage) {
    return nullptr;
  }

  mDeviceStorageStores.AppendElement(
      do_GetWeakReference(static_cast<DOMEventTargetHelper*>(storage)));
  return storage.forget();
}

void B2G::GetDeviceStorages(const nsAString& aType,
                            nsTArray<RefPtr<nsDOMDeviceStorage>>& aStores,
                            ErrorResult& aRv) {
  if (!mOwner) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!innerWindow || !innerWindow->GetOuterWindow() ||
      !innerWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsDOMDeviceStorage::VolumeNameArray volumes;
  nsDOMDeviceStorage::GetOrderedVolumeNames(aType, volumes);
  if (volumes.IsEmpty()) {
    RefPtr<nsDOMDeviceStorage> storage = GetDeviceStorage(aType, aRv);
    if (storage) {
      aStores.AppendElement(storage.forget());
    }
  } else {
    uint32_t len = volumes.Length();
    aStores.SetCapacity(len);
    for (uint32_t i = 0; i < len; ++i) {
      RefPtr<nsDOMDeviceStorage> storage =
          GetDeviceStorageByNameAndType(volumes[i], aType, aRv);
      if (aRv.Failed()) {
        break;
      }

      if (storage) {
        aStores.AppendElement(storage.forget());
      }
    }
  }
}

already_AddRefed<nsDOMDeviceStorage> B2G::GetDeviceStorageByNameAndType(
    const nsAString& aName, const nsAString& aType, ErrorResult& aRv) {
  if (!mOwner) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
  if (!innerWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!innerWindow || !innerWindow->GetOuterWindow() ||
      !innerWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<nsDOMDeviceStorage> storage = FindDeviceStorage(aName, aType);
  if (storage) {
    return storage.forget();
  }
  nsDOMDeviceStorage::CreateDeviceStorageByNameAndType(
      innerWindow, aName, aType, getter_AddRefs(storage));

  if (!storage) {
    return nullptr;
  }

  mDeviceStorageStores.AppendElement(
      do_GetWeakReference(static_cast<DOMEventTargetHelper*>(storage)));
  return storage.forget();
}

/* static */
bool B2G::HasPowerSupplyManagerSupport(JSContext* /* unused */,
                                       JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("powersupply"_ns, innerWindow) : false;
}

PowerSupplyManager* B2G::GetPowerSupplyManager(ErrorResult& aRv) {
  if (!mPowerSupplyManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    mPowerSupplyManager = new PowerSupplyManager(innerWindow);
    mPowerSupplyManager->Init();
  }

  return mPowerSupplyManager;
}

/* static */
bool B2G::HasUsbManagerSupport(JSContext* /* unused */, JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = xpc::WindowOrNull(aGlobal);
  return innerWindow ? CheckPermission("usb"_ns, innerWindow) : false;
}

UsbManager* B2G::GetUsbManager(ErrorResult& aRv) {
  if (!mUsbManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsPIDOMWindowInner* innerWindow = mOwner->AsInnerWindow();
    mUsbManager = new UsbManager(innerWindow);
    mUsbManager->Init();
  }

  return mUsbManager;
}

void B2G::SetDispatchKeyToContentFirst(bool aEnable) {
  EventStateManager::SetDispatchKeyToContentFirst(aEnable);
}

NS_IMETHODIMP B2G::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  if (!strcmp(aTopic, "b2g-disk-storage-state")) {
    if (u"full"_ns.Equals(aData)) {
      return DispatchTrustedEvent(u"storagefull"_ns);
    } else if (u"free"_ns.Equals(aData)) {
      return DispatchTrustedEvent(u"storagefree"_ns);
    }
  }

  return NS_OK;
}

nsresult B2G::MainThreadInit() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;

  RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();

  if (pmService) {
    pmService->AddWakeLockListener(this);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "b2g-disk-storage-state", false);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NetworkStatsManager* B2G::GetNetworkStats(ErrorResult& aRv) {
  if (!mNetworkStatsManager) {
    if (!mOwner) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    mNetworkStatsManager = ConstructJSImplementation<NetworkStatsManager>(
        "@mozilla.org/networkStatsManager;1", GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return mNetworkStatsManager;
}

}  // namespace dom
}  // namespace mozilla
