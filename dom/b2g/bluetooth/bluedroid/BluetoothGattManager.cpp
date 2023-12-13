/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattManager.h"

#include "BluetoothHashKeys.h"
#include "BluetoothInterface.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsTHashMap.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#define ENSURE_GATT_INTF_IS_READY_VOID(runnable)                               \
  do {                                                                         \
    if (NS_WARN_IF(!sBluetoothGattInterface)) {                                \
      DispatchReplyError(runnable, u"BluetoothGattInterface is not ready"_ns); \
      runnable = nullptr;                                                      \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client) \
  do {                                            \
    if (NS_WARN_IF(!sBluetoothGattInterface)) {   \
      client->NotifyDiscoverCompleted(false);     \
      return;                                     \
    }                                             \
  } while (0)

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

class BluetoothGattServer;

namespace {
StaticRefPtr<BluetoothGattManager> sBluetoothGattManager;
static BluetoothGattInterface* sBluetoothGattInterface;
}  // namespace

const int BluetoothGattManager::MAX_NUM_CLIENTS = 1;

bool BluetoothGattManager::mInShutdown = false;

static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattClient> > > sClients;
static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattServer> > > sServers;
static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattScanner> > > sScanners;
static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattAdvertiser> > > sAdvertisers;

struct BluetoothGattClientReadCharState {
  bool mAuthRetry;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(bool aAuthRetry, BluetoothReplyRunnable* aRunnable) {
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset() {
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientWriteCharState {
  BluetoothGattWriteType mWriteType;
  nsTArray<uint8_t> mWriteValue;
  bool mAuthRetry;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(BluetoothGattWriteType aWriteType,
              const nsTArray<uint8_t>& aWriteValue, bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable) {
    mWriteType = aWriteType;
    mWriteValue = aWriteValue.Clone();
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset() {
    mWriteType = GATT_WRITE_TYPE_NORMAL;
    mWriteValue.Clear();
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientReadDescState {
  bool mAuthRetry;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(bool aAuthRetry, BluetoothReplyRunnable* aRunnable) {
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset() {
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientWriteDescState {
  nsTArray<uint8_t> mWriteValue;
  bool mAuthRetry;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(const nsTArray<uint8_t>& aWriteValue, bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable) {
    mWriteValue = aWriteValue.Clone();
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset() {
    mWriteValue.Clear();
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

class mozilla::dom::bluetooth::BluetoothGattClient final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  BluetoothGattClient(const BluetoothUuid& aAppUuid,
                      const BluetoothAddress& aDeviceAddr)
      : mAppUuid(aAppUuid),
        mDeviceAddr(aDeviceAddr),
        mClientIf(0),
        mConnId(0) {}

  void NotifyDiscoverCompleted(bool aSuccess) {
    MOZ_ASSERT(!mAppUuid.IsCleared());
    MOZ_ASSERT(mDiscoverRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify application to clear the cache values of
    // service/characteristic/descriptor.
    bs->DistributeSignal(u"DiscoverCompleted"_ns, mAppUuid,
                         BluetoothValue(aSuccess));

    // Resolve/Reject the Promise.
    if (aSuccess) {
      DispatchReplySuccess(mDiscoverRunnable);
    } else {
      DispatchReplyError(mDiscoverRunnable, u"Discover failed"_ns);
    }

    // Cleanup
    mDbElements.Clear();
    mDiscoverRunnable = nullptr;
  }

  BluetoothUuid mAppUuid;
  BluetoothAddress mDeviceAddr;
  int mClientIf;
  int mConnId;
  RefPtr<BluetoothReplyRunnable> mConnectRunnable;
  RefPtr<BluetoothReplyRunnable> mDisconnectRunnable;
  RefPtr<BluetoothReplyRunnable> mDiscoverRunnable;
  RefPtr<BluetoothReplyRunnable> mReadRemoteRssiRunnable;
  RefPtr<BluetoothReplyRunnable> mRegisterNotificationsRunnable;
  RefPtr<BluetoothReplyRunnable> mDeregisterNotificationsRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterClientRunnable;

  BluetoothGattClientReadCharState mReadCharacteristicState;
  BluetoothGattClientWriteCharState mWriteCharacteristicState;
  BluetoothGattClientReadDescState mReadDescriptorState;
  BluetoothGattClientWriteDescState mWriteDescriptorState;

  // A temporary array used only during discover operations.
  // It would be empty if there are no ongoing discover operations.
  nsTArray<BluetoothGattDbElement> mDbElements;

 private:
  ~BluetoothGattClient() {}
};

NS_IMPL_ISUPPORTS0(BluetoothGattClient)

class mozilla::dom::bluetooth::BluetoothGattScanner final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  BluetoothGattScanner(const BluetoothUuid& aScanUuid)
      : mScanUuid(aScanUuid), mScannerId(0) {}

  BluetoothUuid mScanUuid;
  int mScannerId;
  RefPtr<BluetoothReplyRunnable> mStartLeScanRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterScannerRunnable;

 private:
  ~BluetoothGattScanner() {}
};

NS_IMPL_ISUPPORTS0(BluetoothGattScanner)

class mozilla::dom::bluetooth::BluetoothGattAdvertiser final
    : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  BluetoothGattAdvertiser(const BluetoothUuid& aAdvertiseUuid)
      : mAdvertiseUuid(aAdvertiseUuid), mAdvertiserId(0) {}

  BluetoothUuid mAdvertiseUuid;
  int mAdvertiserId;
  RefPtr<BluetoothReplyRunnable> mStartAdvertisingRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterAdvertiserRunnable;
  BluetoothGattAdvertisingData mAdvertisingData;

 private:
  ~BluetoothGattAdvertiser() {}
};

NS_IMPL_ISUPPORTS0(BluetoothGattAdvertiser)

struct BluetoothGattServerAddServiceState {
  nsTArray<BluetoothGattDbElement> mGattDb;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(const nsTArray<BluetoothGattDbElement>& aGattDb,
              BluetoothReplyRunnable* aRunnable) {
    mGattDb = aGattDb.Clone();
    mRunnable = aRunnable;
  }

  void Reset() {
    mGattDb.Clear();
    mRunnable = nullptr;
  }
};

class BluetoothGattServer final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit BluetoothGattServer(const BluetoothUuid& aAppUuid)
      : mAppUuid(aAppUuid), mServerIf(0), mIsRegistering(false) {}

  BluetoothUuid mAppUuid;
  int mServerIf;

  /*
   * Some actions will trigger the registration procedure:
   *  - Connect the GATT server to a peripheral client
   *  - Add a service to the GATT server
   * These actions will be taken only after the registration has been done
   * successfully. If the registration fails, all the existing actions above
   * should be rejected.
   */
  bool mIsRegistering;

  RefPtr<BluetoothReplyRunnable> mRegisterServerRunnable;
  RefPtr<BluetoothReplyRunnable> mConnectPeripheralRunnable;
  RefPtr<BluetoothReplyRunnable> mDisconnectPeripheralRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterServerRunnable;

  BluetoothGattServerAddServiceState mAddServiceState;

  RefPtr<BluetoothReplyRunnable> mRemoveServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mStartServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mStopServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mSendResponseRunnable;
  RefPtr<BluetoothReplyRunnable> mSendIndicationRunnable;

  // Map connection id from device address
  nsTHashMap<BluetoothAddressHashKey, int> mConnectionMap;

 private:
  ~BluetoothGattServer() {}
};

NS_IMPL_ISUPPORTS0(BluetoothGattServer)

class UuidComparator {
 public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient,
              const BluetoothUuid& aAppUuid) const {
    return aClient->mAppUuid == aAppUuid;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer,
              const BluetoothUuid& aAppUuid) const {
    return aServer->mAppUuid == aAppUuid;
  }

  bool Equals(const RefPtr<BluetoothGattScanner>& aScanner,
              const BluetoothUuid& aScanUuid) const {
    return aScanner->mScanUuid == aScanUuid;
  }

  bool Equals(const RefPtr<BluetoothGattAdvertiser>& aAdvertiser,
              const BluetoothUuid& aAdvertiseUuid) const {
    return aAdvertiser->mAdvertiseUuid == aAdvertiseUuid;
  }
};

class InterfaceIdComparator {
 public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient, int aClientIf) const {
    return aClient->mClientIf == aClientIf;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer, int aServerIf) const {
    return aServer->mServerIf == aServerIf;
  }

  bool Equals(const RefPtr<BluetoothGattScanner>& aScanner,
              int aScannerId) const {
    return aScanner->mScannerId == aScannerId;
  }

  bool Equals(const RefPtr<BluetoothGattAdvertiser>& aAdvertiser,
              int aAdvertiserId) const {
    return aAdvertiser->mAdvertiserId == aAdvertiserId;
  }
};

class ConnIdComparator {
 public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient, int aConnId) const {
    return aClient->mConnId == aConnId;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer, int aConnId) const {
    for (auto iter = aServer->mConnectionMap.Iter(); !iter.Done();
         iter.Next()) {
      if (aConnId == iter.Data()) {
        return true;
      }
    }
    return false;
  }
};

BluetoothGattManager* BluetoothGattManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothGattManager already exists, exit early
  if (sBluetoothGattManager) {
    return sBluetoothGattManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  // Create a new instance, register, and return
  RefPtr<BluetoothGattManager> manager = new BluetoothGattManager();
  NS_ENSURE_SUCCESS(manager->Init(), nullptr);

  sBluetoothGattManager = manager;

  return sBluetoothGattManager;
}

nsresult BluetoothGattManager::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_NOT_AVAILABLE);

  auto rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to add observers!");
    return rv;
  }

  return NS_OK;
}

void BluetoothGattManager::Uninit() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

class BluetoothGattManager::RegisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  RegisterModuleResultHandler(BluetoothGattInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
      : mInterface(aInterface), mRes(aRes) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for GATT: %d",
               (int)aStatus);

    mInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothGattInterface = mInterface;

    if (mRes) {
      mRes->Init();
    }
  }

 private:
  BluetoothGattInterface* mInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothGattManager::InitProfileResultHandlerRunnable final
    : public Runnable {
 public:
  InitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                   nsresult aRv)
      : Runnable("InitProfileResultHandlerRunnable"), mRes(aRes), mRv(aRv) {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Init();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void BluetoothGattManager::InitGattInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sBluetoothGattInterface) {
    BT_LOGR("Bluetooth GATT interface is already initalized.");
    RefPtr<Runnable> r = new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT Init runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no Bluetooth interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto gattInterface = btInf->GetBluetoothGattInterface();

  if (NS_WARN_IF(!gattInterface)) {
    // If there's no GATT interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  if (!sClients) {
    sClients = new nsTArray<RefPtr<BluetoothGattClient> >;
  }

  if (!sServers) {
    sServers = new nsTArray<RefPtr<BluetoothGattServer> >;
  }

  if (!sScanners) {
    sScanners = new nsTArray<RefPtr<BluetoothGattScanner> >;
  }

  if (!sAdvertisers) {
    sAdvertisers = new nsTArray<RefPtr<BluetoothGattAdvertiser> >;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  gattInterface->SetNotificationHandler(BluetoothGattManager::Get());

  setupInterface->RegisterModule(
      SETUP_SERVICE_ID_GATT, 0, MAX_NUM_CLIENTS,
      new RegisterModuleResultHandler(gattInterface, aRes));
}

class BluetoothGattManager::UnregisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  explicit UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
      : mRes(aRes) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for GATT: %d",
               (int)aStatus);

    if (sBluetoothGattInterface) {
      sBluetoothGattInterface->SetNotificationHandler(nullptr);
      sBluetoothGattInterface = nullptr;
    }

    sClients = nullptr;
    sServers = nullptr;
    sScanners = nullptr;
    sAdvertisers = nullptr;

    if (sBluetoothGattManager) {
      sBluetoothGattManager->Uninit();
      sBluetoothGattManager = nullptr;
    }

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (sBluetoothGattInterface) {
      sBluetoothGattInterface->SetNotificationHandler(nullptr);
      sBluetoothGattInterface = nullptr;
    }

    sClients = nullptr;
    sServers = nullptr;
    sScanners = nullptr;
    sAdvertisers = nullptr;

    if (sBluetoothGattManager) {
      sBluetoothGattManager->Uninit();
      sBluetoothGattManager = nullptr;
    }

    if (mRes) {
      mRes->Deinit();
    }
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothGattManager::DeinitProfileResultHandlerRunnable final
    : public Runnable {
 public:
  DeinitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                     nsresult aRv)
      : Runnable("DeinitProfileResultHandlerRunnable"), mRes(aRes), mRv(aRv) {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Deinit();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void BluetoothGattManager::DeinitGattInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBluetoothGattInterface) {
    BT_LOGR("Bluetooth GATT interface has not been initalized.");
    RefPtr<Runnable> r = new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT Deinit runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(SETUP_SERVICE_ID_GATT,
                                   new UnregisterModuleResultHandler(aRes));
}

class BluetoothGattManager::RegisterClientResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit RegisterClientResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::RegisterClient failed: %d",
               (int)aStatus);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, mClient->mAppUuid,
                         BluetoothValue(false));  // Disconnected

    // Reject the connect request
    if (mClient->mConnectRunnable) {
      DispatchReplyError(mClient->mConnectRunnable,
                         u"Register GATT client failed"_ns);
      mClient->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(mClient);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::RegisterScannerResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit RegisterScannerResultHandler(BluetoothGattScanner* aScanner)
      : mScanner(aScanner) {
    MOZ_ASSERT(mScanner);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::RegisterScanner failed: %d",
               (int)aStatus);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    sScanners->RemoveElement(mScanner);
  }

 private:
  RefPtr<BluetoothGattScanner> mScanner;
};

class BluetoothGattManager::RegisterAdvertiserResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit RegisterAdvertiserResultHandler(BluetoothGattAdvertiser* aAdvertiser)
      : mAdvertiser(aAdvertiser) {
    MOZ_ASSERT(mAdvertiser);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::RegisterAdvertiser failed: %d",
               (int)aStatus);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    sAdvertisers->RemoveElement(mAdvertiser);
  }

 private:
  RefPtr<BluetoothGattAdvertiser> mAdvertiser;
};

class BluetoothGattManager::UnregisterClientResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit UnregisterClientResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void UnregisterClient() override {
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt to clear the clientIf
    bs->DistributeSignal(u"ClientUnregistered"_ns, mClient->mAppUuid);

    // Resolve the unregister request
    DispatchReplySuccess(mClient->mUnregisterClientRunnable);
    mClient->mUnregisterClientRunnable = nullptr;

    sClients->RemoveElement(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::UnregisterClient failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);

    // Reject the unregister request
    DispatchReplyError(mClient->mUnregisterClientRunnable,
                       u"Unregister GATT client failed"_ns);
    mClient->mUnregisterClientRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::UnregisterScannerResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit UnregisterScannerResultHandler(BluetoothGattScanner* aScanner)
      : mScanner(aScanner) {
    MOZ_ASSERT(mScanner);
  }

  void UnregisterScanner() override {
    MOZ_ASSERT(mScanner->mUnregisterScannerRunnable);
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Resolve the unregister request
    DispatchReplySuccess(mScanner->mUnregisterScannerRunnable);
    mScanner->mUnregisterScannerRunnable = nullptr;

    sScanners->RemoveElement(mScanner);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::UnregisterScanner failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mScanner->mUnregisterScannerRunnable);

    // Reject the unregister request
    DispatchReplyError(mScanner->mUnregisterScannerRunnable,
                       u"Unregister GATT scanner failed"_ns);
    mScanner->mUnregisterScannerRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattScanner> mScanner;
};

class BluetoothGattManager::UnregisterAdvertiserResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit UnregisterAdvertiserResultHandler(
      BluetoothGattAdvertiser* aAdvertiser)
      : mAdvertiser(aAdvertiser) {
    MOZ_ASSERT(mAdvertiser);
  }

  void UnregisterAdvertiser() override {
    MOZ_ASSERT(mAdvertiser->mUnregisterAdvertiserRunnable);
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Resolve the unregister request
    DispatchReplySuccess(mAdvertiser->mUnregisterAdvertiserRunnable);
    mAdvertiser->mUnregisterAdvertiserRunnable = nullptr;

    sAdvertisers->RemoveElement(mAdvertiser);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::UnregisterAdvertiser failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mAdvertiser->mUnregisterAdvertiserRunnable);

    // Reject the unregister request
    DispatchReplyError(mAdvertiser->mUnregisterAdvertiserRunnable,
                       u"Unregister GATT advertiser failed"_ns);
    mAdvertiser->mUnregisterAdvertiserRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattAdvertiser> mAdvertiser;
};

void BluetoothGattManager::UnregisterClient(int aClientIf,
                                            BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sClients->IndexOf(aClientIf, 0 /* Start */, InterfaceIdComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mUnregisterClientRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterClient(
      aClientIf, new UnregisterClientResultHandler(client));
}

void BluetoothGattManager::UnregisterScanner(
    int aScannerId, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sScanners->IndexOf(aScannerId, 0 /* Start */, InterfaceIdComparator());
  if (NS_WARN_IF(index == sScanners->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattScanner> scanner = sScanners->ElementAt(index);
  scanner->mUnregisterScannerRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterScanner(
      aScannerId, new UnregisterScannerResultHandler(scanner));
}

void BluetoothGattManager::UnregisterAdvertiser(
    uint8_t aAdvertiserId, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sAdvertisers->IndexOf(aAdvertiserId, 0 /* Start */,
                                       InterfaceIdComparator());
  if (NS_WARN_IF(index == sAdvertisers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattAdvertiser> advertiser = sAdvertisers->ElementAt(index);
  advertiser->mUnregisterAdvertiserRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterAdvertiser(
      aAdvertiserId, new UnregisterAdvertiserResultHandler(advertiser));
}

class BluetoothGattManager::StartLeScanResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit StartLeScanResultHandler(BluetoothGattScanner* aScanner)
      : mScanner(aScanner) {}

  void Scan() override {
    MOZ_ASSERT(mScanner);

    DispatchReplySuccess(mScanner->mStartLeScanRunnable,
                         BluetoothValue(mScanner->mScanUuid));
    mScanner->mStartLeScanRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::StartLeScan failed: %d", (int)aStatus);
    MOZ_ASSERT(mScanner->mStartLeScanRunnable);

    // Unregister scanner if startLeScan failed
    if (mScanner->mScannerId > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      RefPtr<BluetoothVoidReplyRunnable> result =
          new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterScanner(mScanner->mScannerId, result);
    }

    DispatchReplyError(mScanner->mStartLeScanRunnable, aStatus);
    mScanner->mStartLeScanRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattScanner> mScanner;
};

class BluetoothGattManager::StopLeScanResultHandler final
    : public BluetoothGattResultHandler {
 public:
  StopLeScanResultHandler(BluetoothReplyRunnable* aRunnable, uint8_t aScannerId)
      : mRunnable(aRunnable), mScannerId(aScannerId) {}

  void Scan() override {
    DispatchReplySuccess(mRunnable);

    // Unregister scanner when stopLeScan succeeded
    if (mScannerId > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      RefPtr<BluetoothVoidReplyRunnable> result =
          new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterScanner(mScannerId, result);
    }
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::StopLeScan failed: %d", (int)aStatus);
    DispatchReplyError(mRunnable, aStatus);
  }

 private:
  RefPtr<BluetoothReplyRunnable> mRunnable;
  uint8_t mScannerId;
};

void BluetoothGattManager::StartLeScan(
    const nsTArray<BluetoothUuid>& aServiceUuids,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  BluetoothUuid scanUuid;
  if (NS_WARN_IF(NS_FAILED(GenerateUuid(scanUuid)))) {
    DispatchReplyError(aRunnable, u"start LE scan failed"_ns);
    return;
  }

  size_t index = sScanners->IndexOf(scanUuid, 0 /* Start */, UuidComparator());

  // Reject the startLeScan request if the scanner id is being used.
  if (NS_WARN_IF(index != sScanners->NoIndex)) {
    DispatchReplyError(aRunnable, u"start LE scan failed"_ns);
    return;
  }

  index = sScanners->Length();
  sScanners->AppendElement(new BluetoothGattScanner(scanUuid));
  RefPtr<BluetoothGattScanner> scanner = sScanners->ElementAt(index);
  scanner->mStartLeScanRunnable = aRunnable;

  // 'startLeScan' will be proceeded after scanner registered
  sBluetoothGattInterface->RegisterScanner(
      scanUuid, new RegisterScannerResultHandler(scanner));
}

void BluetoothGattManager::StopLeScan(const BluetoothUuid& aScanUuid,
                                      BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sScanners->IndexOf(aScanUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sScanners->NoIndex)) {
    // Reject the stop LE scan request
    DispatchReplyError(aRunnable, u"StopLeScan failed"_ns);
    return;
  }

  RefPtr<BluetoothGattScanner> scanner = sScanners->ElementAt(index);
  sBluetoothGattInterface->Scan(
      scanner->mScannerId, false /* Stop */,
      new StopLeScanResultHandler(aRunnable, scanner->mScannerId));
}

class BluetoothGattManager::StartAdvertisingResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit StartAdvertisingResultHandler(BluetoothGattAdvertiser* aAdvertiser)
      : mAdvertiser(aAdvertiser) {
    MOZ_ASSERT(mAdvertiser);
  }

  void Advertise() override {
    MOZ_ASSERT(mAdvertiser->mStartAdvertisingRunnable);

    DispatchReplySuccess(mAdvertiser->mStartAdvertisingRunnable);
    mAdvertiser->mStartAdvertisingRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::StartLeAdvertising failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mAdvertiser->mStartAdvertisingRunnable);

    // Unregister advertiser if startAdvertising failed
    if (mAdvertiser->mAdvertiserId > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      RefPtr<BluetoothVoidReplyRunnable> result =
          new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterAdvertiser(mAdvertiser->mAdvertiserId, result);
    }

    DispatchReplyError(mAdvertiser->mStartAdvertisingRunnable, aStatus);
    mAdvertiser->mStartAdvertisingRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattAdvertiser> mAdvertiser;
};

void BluetoothGattManager::StartAdvertising(
    const BluetoothUuid& aAppUuid, const BluetoothGattAdvertisingData& aData,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sAdvertisers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());

  // Reject the startAdvertising request if the advertiser is being used.
  if (NS_WARN_IF(index != sAdvertisers->NoIndex)) {
    DispatchReplyError(aRunnable, u"start advertising failed"_ns);
    return;
  }

  index = sAdvertisers->Length();
  sAdvertisers->AppendElement(new BluetoothGattAdvertiser(aAppUuid));
  RefPtr<BluetoothGattAdvertiser> advertiser = sAdvertisers->ElementAt(index);
  advertiser->mStartAdvertisingRunnable = aRunnable;
  advertiser->mAdvertisingData = aData;

  // 'startAdvertising' will be proceeded after advertiser registered
  sBluetoothGattInterface->RegisterAdvertiser(
      aAppUuid, new RegisterAdvertiserResultHandler(advertiser));
}

void BluetoothGattManager::StopAdvertising(const BluetoothUuid& aAppUuid,
                                           BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sAdvertisers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sAdvertisers->NoIndex)) {
    // Reject the stop advertising request
    DispatchReplyError(aRunnable, u"StopAdvertising failed"_ns);
    return;
  }

  RefPtr<BluetoothGattAdvertiser> advertiser = sAdvertisers->ElementAt(index);

  // Reject the stop advertising request if there is an ongoing one.
  if (advertiser->mUnregisterAdvertiserRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  advertiser->mUnregisterAdvertiserRunnable = aRunnable;
  sBluetoothGattInterface->UnregisterAdvertiser(
      advertiser->mAdvertiserId,
      new UnregisterAdvertiserResultHandler(advertiser));
}

class BluetoothGattManager::ConnectResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ConnectResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::Connect failed: %d", (int)aStatus);
    MOZ_ASSERT(mClient->mConnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, mClient->mAppUuid,
                         BluetoothValue(false));  // Disconnected

    // Reject the connect request
    DispatchReplyError(mClient->mConnectRunnable, u"Connect failed"_ns);
    mClient->mConnectRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::Connect(const BluetoothUuid& aAppUuid,
                                   const BluetoothAddress& aDeviceAddr,
                                   BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sClients->NoIndex) {
    index = sClients->Length();
    sClients->AppendElement(new BluetoothGattClient(aAppUuid, aDeviceAddr));
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mConnectRunnable = aRunnable;

  if (client->mClientIf > 0) {
    sBluetoothGattInterface->Connect(client->mClientIf, aDeviceAddr,
                                     true,  // direct connect
                                     TRANSPORT_AUTO,
                                     new ConnectResultHandler(client));
  } else {
    // connect will be proceeded after client registered
    sBluetoothGattInterface->RegisterClient(
        aAppUuid, new RegisterClientResultHandler(client));
  }
}

class BluetoothGattManager::DisconnectResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit DisconnectResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::Disconnect failed: %d", (int)aStatus);
    MOZ_ASSERT(mClient->mDisconnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt that the client remains connected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, mClient->mAppUuid,
                         BluetoothValue(true));  // Connected

    // Reject the disconnect request
    DispatchReplyError(mClient->mDisconnectRunnable, u"Disconnect failed"_ns);
    mClient->mDisconnectRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::Disconnect(const BluetoothUuid& aAppUuid,
                                      const BluetoothAddress& aDeviceAddr,
                                      BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mDisconnectRunnable = aRunnable;

  sBluetoothGattInterface->Disconnect(client->mClientIf, aDeviceAddr,
                                      client->mConnId,
                                      new DisconnectResultHandler(client));
}

class BluetoothGattManager::DiscoverResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit DiscoverResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::Discover failed: %d", (int)aStatus);

    mClient->NotifyDiscoverCompleted(false);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::Discover(const BluetoothUuid& aAppUuid,
                                    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mConnId > 0);
  MOZ_ASSERT(!client->mDiscoverRunnable);

  client->mDiscoverRunnable = aRunnable;

  /**
   * Discover all services/characteristics/descriptors offered by the remote
   * GATT server.
   *
   * The discover procesure includes following steps.
   * 1) Discover all services.
   * 2) After all services are discovered, for each service S, we will do
   *    following actions.
   *    2-1) Discover all included services of service S.
   *    2-2) Discover all characteristics of service S.
   *    2-3) Discover all descriptors of those characteristics discovered in
   *         2-2).
   */
  sBluetoothGattInterface->SearchService(client->mConnId,
                                         true,  // search all services
                                         BluetoothUuid(),
                                         new DiscoverResultHandler(client));
}

class BluetoothGattManager::ReadRemoteRssiResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ReadRemoteRssiResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::ReadRemoteRssi failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadRemoteRssiRunnable);

    // Reject the read remote rssi request
    DispatchReplyError(mClient->mReadRemoteRssiRunnable,
                       u"ReadRemoteRssi failed"_ns);
    mClient->mReadRemoteRssiRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::ReadRemoteRssi(int aClientIf,
                                          const BluetoothAddress& aDeviceAddr,
                                          BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sClients->IndexOf(aClientIf, 0 /* Start */, InterfaceIdComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mReadRemoteRssiRunnable = aRunnable;

  sBluetoothGattInterface->ReadRemoteRssi(
      aClientIf, aDeviceAddr, new ReadRemoteRssiResultHandler(client));
}

class BluetoothGattManager::RegisterNotificationsResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit RegisterNotificationsResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void RegisterNotification() override {
    MOZ_ASSERT(mClient->mRegisterNotificationsRunnable);

    /**
     * Resolve the promise directly if we successfully issued this request to
     * stack.
     *
     * We resolve the promise here since bluedroid stack always returns
     * incorrect connId in |RegisterNotificationNotification| and we cannot map
     * back to the target client because of it.
     * Please see Bug 1149043 for more information.
     */
    DispatchReplySuccess(mClient->mRegisterNotificationsRunnable);
    mClient->mRegisterNotificationsRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::RegisterNotifications failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mRegisterNotificationsRunnable);

    DispatchReplyError(mClient->mRegisterNotificationsRunnable,
                       u"RegisterNotifications failed"_ns);
    mClient->mRegisterNotificationsRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::RegisterNotifications(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request or client is already
  // disconnected
  if (client->mRegisterNotificationsRunnable || client->mConnId <= 0) {
    DispatchReplyError(aRunnable, u"RegisterNotifications failed"_ns);
    return;
  }

  client->mRegisterNotificationsRunnable = aRunnable;

  sBluetoothGattInterface->RegisterNotification(
      client->mClientIf, client->mDeviceAddr, aHandle,
      new RegisterNotificationsResultHandler(client));
}

class BluetoothGattManager::DeregisterNotificationsResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit DeregisterNotificationsResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void DeregisterNotification() override {
    MOZ_ASSERT(mClient->mDeregisterNotificationsRunnable);

    /**
     * Resolve the promise directly if we successfully issued this request to
     * stack.
     *
     * We resolve the promise here since bluedroid stack always returns
     * incorrect connId in |RegisterNotificationNotification| and we cannot map
     * back to the target client because of it.
     * Please see Bug 1149043 for more information.
     */
    DispatchReplySuccess(mClient->mDeregisterNotificationsRunnable);
    mClient->mDeregisterNotificationsRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::DeregisterNotifications failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mDeregisterNotificationsRunnable);

    DispatchReplyError(mClient->mDeregisterNotificationsRunnable,
                       u"DeregisterNotifications failed"_ns);
    mClient->mDeregisterNotificationsRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::DeregisterNotifications(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request
  if (client->mDeregisterNotificationsRunnable) {
    DispatchReplyError(aRunnable, u"DeregisterNotifications failed"_ns);
    return;
  }

  client->mDeregisterNotificationsRunnable = aRunnable;

  sBluetoothGattInterface->DeregisterNotification(
      client->mClientIf, client->mDeviceAddr, aHandle,
      new DeregisterNotificationsResultHandler(client));
}

class BluetoothGattManager::ReadCharacteristicValueResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ReadCharacteristicValueResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING(
        "BluetoothGattInterface::ReadCharacteristicValue failed"
        ": %d",
        (int)aStatus);
    MOZ_ASSERT(mClient->mReadCharacteristicState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
        mClient->mReadCharacteristicState.mRunnable;
    mClient->mReadCharacteristicState.Reset();

    // Reject the read characteristic value request
    DispatchReplyError(runnable, u"ReadCharacteristicValue failed"_ns);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::ReadCharacteristicValue(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent reading requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mReadCharacteristicState.mRunnable) {
    DispatchReplyError(aRunnable, u"ReadCharacteristicValue failed"_ns);
    return;
  }

  client->mReadCharacteristicState.Assign(false, aRunnable);

  /**
   * First, read the characteristic value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to read through an authenticated physical link.
   */
  sBluetoothGattInterface->ReadCharacteristic(
      client->mConnId, aHandle, GATT_AUTH_REQ_NONE,
      new ReadCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::WriteCharacteristicValueResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit WriteCharacteristicValueResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING(
        "BluetoothGattInterface::WriteCharacteristicValue failed"
        ": %d",
        (int)aStatus);
    MOZ_ASSERT(mClient->mWriteCharacteristicState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
        mClient->mWriteCharacteristicState.mRunnable;
    mClient->mWriteCharacteristicState.Reset();

    // Reject the write characteristic value request
    DispatchReplyError(runnable, u"WriteCharacteristicValue failed"_ns);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::WriteCharacteristicValue(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    const BluetoothGattWriteType& aWriteType, const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent writing requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mWriteCharacteristicState.mRunnable) {
    DispatchReplyError(aRunnable, u"WriteCharacteristicValue failed"_ns);
    return;
  }

  client->mWriteCharacteristicState.Assign(aWriteType, aValue, false,
                                           aRunnable);

  /**
   * First, write the characteristic value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to write through an authenticated physical link.
   */
  sBluetoothGattInterface->WriteCharacteristic(
      client->mConnId, aHandle, aWriteType, GATT_AUTH_REQ_NONE, aValue,
      new WriteCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::ReadDescriptorValueResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ReadDescriptorValueResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::ReadDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadDescriptorState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
        mClient->mReadDescriptorState.mRunnable;
    mClient->mReadDescriptorState.Reset();

    // Reject the read descriptor value request
    DispatchReplyError(runnable, u"ReadDescriptorValue failed"_ns);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::ReadDescriptorValue(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent reading requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mReadDescriptorState.mRunnable) {
    DispatchReplyError(aRunnable, u"ReadDescriptorValue failed"_ns);
    return;
  }

  client->mReadDescriptorState.Assign(false, aRunnable);

  /**
   * First, read the descriptor value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to read through an authenticated physical link.
   */
  sBluetoothGattInterface->ReadDescriptor(
      client->mConnId, aHandle, GATT_AUTH_REQ_NONE,
      new ReadDescriptorValueResultHandler(client));
}

class BluetoothGattManager::WriteDescriptorValueResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit WriteDescriptorValueResultHandler(BluetoothGattClient* aClient)
      : mClient(aClient) {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattInterface::WriteDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mWriteDescriptorState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
        mClient->mWriteDescriptorState.mRunnable;
    mClient->mWriteDescriptorState.Reset();

    // Reject the write descriptor value request
    DispatchReplyError(runnable, u"WriteDescriptorValue failed"_ns);
  }

 private:
  RefPtr<BluetoothGattClient> mClient;
};

void BluetoothGattManager::WriteDescriptorValue(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    const nsTArray<uint8_t>& aValue, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent writing requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mWriteDescriptorState.mRunnable) {
    DispatchReplyError(aRunnable, u"WriteDescriptorValue failed"_ns);
    return;
  }

  /**
   * First, write the descriptor value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to write through an authenticated physical link.
   */
  client->mWriteDescriptorState.Assign(aValue, false, aRunnable);

  sBluetoothGattInterface->WriteDescriptor(
      client->mConnId, aHandle, GATT_WRITE_TYPE_NORMAL, GATT_AUTH_REQ_NONE,
      aValue, new WriteDescriptorValueResultHandler(client));
}

class BluetoothGattManager::RegisterServerResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit RegisterServerResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(!mServer->mIsRegistering);

    mServer->mIsRegistering = true;
  }

  /*
   * Some actions will trigger the registration procedure. These actions will
   * be taken only after the registration has been done successfully.
   * If the registration fails, all the existing actions above should be
   * rejected.
   */
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::RegisterServer failed: %d",
               (int)aStatus);

    // Reject the connect request
    if (mServer->mConnectPeripheralRunnable) {
      DispatchReplyError(mServer->mConnectPeripheralRunnable,
                         u"Register GATT server failed"_ns);
      mServer->mConnectPeripheralRunnable = nullptr;
    }

    // Reject the add service request
    if (mServer->mAddServiceState.mRunnable) {
      DispatchReplyError(mServer->mAddServiceState.mRunnable,
                         u"Register GATT server failed"_ns);
      mServer->mAddServiceState.Reset();
    }

    if (mServer->mRegisterServerRunnable) {
      DispatchReplyError(mServer->mRegisterServerRunnable,
                         u"Register GATT server failed"_ns);
      mServer->mRegisterServerRunnable = nullptr;
    }

    mServer->mIsRegistering = false;
    sServers->RemoveElement(mServer);
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::RegisterServer(const BluetoothUuid& aAppUuid,
                                          BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    index = sServers->Length();
    sServers->AppendElement(new BluetoothGattServer(aAppUuid));
  }
  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  /**
   * There are four cases here for handling aRunnable.
   * 1) Server interface is already registered: Resolve the runnable.
   * 2) Server interface is not registered, but there is
   *    an existing |RegisterServerRunnable|: Reject with STATUS_BUSY.
   * 3) Server interface is registering without an existing
   *    |RegisterServerRunnable|: Save the runnable into |GattServer| and will
   *    resolve or reject it in |RegisterServerNotification| later.
   * 4) Server interface is neither registered nor registering: Save the
   *    the runnable into |GattServer| and trigger a registration procedure.
   *    The runnable will be resolved or rejected in
   *    |RegisterServerNotification| later.
   */
  if (server->mServerIf > 0) {
    DispatchReplySuccess(aRunnable);
  } else if (server->mRegisterServerRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
  } else if (server->mIsRegistering) {
    server->mRegisterServerRunnable = aRunnable;
  } else {
    server->mRegisterServerRunnable = aRunnable;
    sBluetoothGattInterface->RegisterServer(
        aAppUuid, new RegisterServerResultHandler(server));
  }
}

class BluetoothGattManager::ConnectPeripheralResultHandler final
    : public BluetoothGattResultHandler {
 public:
  ConnectPeripheralResultHandler(BluetoothGattServer* aServer,
                                 const BluetoothAddress& aDeviceAddr)
      : mServer(aServer), mDeviceAddr(aDeviceAddr) {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(!mDeviceAddr.IsCleared());
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::ConnectPeripheral failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mConnectPeripheralRunnable);

    DispatchReplyError(mServer->mConnectPeripheralRunnable,
                       u"ConnectPeripheral failed"_ns);
    mServer->mConnectPeripheralRunnable = nullptr;
    mServer->mConnectionMap.Remove(mDeviceAddr);
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
  BluetoothAddress mDeviceAddr;
};

void BluetoothGattManager::ConnectPeripheral(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    index = sServers->Length();
    sServers->AppendElement(new BluetoothGattServer(aAppUuid));
  }
  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  /**
   * Early resolve or reject the request based on the current status before
   * sending a request to bluetooth stack.
   *
   * case 1) Connecting/Disconnecting: If connect/disconnect peripheral
   *         runnable exists, reject the request since the local GATT server is
   *         busy connecting or disconnecting to a device.
   * case 2) Connected: If there is an entry whose key is |aAddress| in the
   *         connection map, resolve the request. Since disconnected devices
   *         will not be in the map, all entries in the map are connected
   *         devices.
   */
  if (server->mConnectPeripheralRunnable ||
      server->mDisconnectPeripheralRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  if (server->mConnectionMap.Get(aAddress, &connId)) {
    MOZ_ASSERT(connId > 0);
    DispatchReplySuccess(aRunnable);
    return;
  }

  server->mConnectionMap.InsertOrUpdate(aAddress, 0);
  server->mConnectPeripheralRunnable = aRunnable;

  if (server->mServerIf > 0) {
    sBluetoothGattInterface->ConnectPeripheral(
        server->mServerIf, aAddress,
        true,  // direct connect
        TRANSPORT_AUTO, new ConnectPeripheralResultHandler(server, aAddress));
  } else if (!server->mIsRegistering) { /* avoid triggering another registration
                                         * procedure if there is an on-going one
                                         * already */
    // connect will be proceeded after server registered
    sBluetoothGattInterface->RegisterServer(
        aAppUuid, new RegisterServerResultHandler(server));
  }
}

class BluetoothGattManager::DisconnectPeripheralResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit DisconnectPeripheralResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::DisconnectPeripheral failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mDisconnectPeripheralRunnable);

    // Reject the disconnect request
    DispatchReplyError(mServer->mDisconnectPeripheralRunnable,
                       u"DisconnectPeripheral failed"_ns);
    mServer->mDisconnectPeripheralRunnable = nullptr;
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::DisconnectPeripheral(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  if (NS_WARN_IF(server->mServerIf <= 0)) {
    DispatchReplyError(aRunnable, u"Disconnect failed"_ns);
    return;
  }

  // Reject the request if there is an ongoing connect/disconnect request.
  if (server->mConnectPeripheralRunnable ||
      server->mDisconnectPeripheralRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  // Resolve the request if the device is not connected.
  int connId = 0;
  if (!server->mConnectionMap.Get(aAddress, &connId)) {
    DispatchReplySuccess(aRunnable);
    return;
  }

  server->mDisconnectPeripheralRunnable = aRunnable;

  sBluetoothGattInterface->DisconnectPeripheral(
      server->mServerIf, aAddress, connId,
      new DisconnectPeripheralResultHandler(server));
}

class BluetoothGattManager::UnregisterServerResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit UnregisterServerResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void UnregisterServer() override {
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGattServer to clear the serverIf
    bs->DistributeSignal(u"ServerUnregistered"_ns, mServer->mAppUuid);

    // Resolve the unregister request
    if (mServer->mUnregisterServerRunnable) {
      DispatchReplySuccess(mServer->mUnregisterServerRunnable);
      mServer->mUnregisterServerRunnable = nullptr;
    }

    sServers->RemoveElement(mServer);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::UnregisterServer failed: %d",
               (int)aStatus);

    // Reject the unregister request
    if (mServer->mUnregisterServerRunnable) {
      DispatchReplyError(mServer->mUnregisterServerRunnable,
                         u"Unregister GATT Server failed"_ns);
      mServer->mUnregisterServerRunnable = nullptr;
    }
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::UnregisterServer(int aServerIf,
                                            BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index =
      sServers->IndexOf(aServerIf, 0 /* Start */, InterfaceIdComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattServer> server = (*sServers)[index];
  server->mUnregisterServerRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterServer(
      aServerIf, new UnregisterServerResultHandler(server));
}

class BluetoothGattManager::ServerAddServiceResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ServerAddServiceResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::ServerAddService failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mAddServiceState.mRunnable);

    DispatchReplyError(mServer->mAddServiceState.mRunnable,
                       u"ServerAddService failed"_ns);
    mServer->mAddServiceState.Reset();
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::ServerAddService(
    const BluetoothUuid& aAppUuid, const nsTArray<BluetoothGattDbElement>& aDb,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    index = sServers->Length();
    sServers->AppendElement(new BluetoothGattServer(aAppUuid));
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if there is an ongoing add service request.
  if (server->mAddServiceState.mRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mAddServiceState.Assign(aDb, aRunnable);

  if (server->mServerIf > 0) {
    sBluetoothGattInterface->AddService(
        server->mServerIf, aDb, new ServerAddServiceResultHandler(server));
  } else if (!server->mIsRegistering) { /* avoid triggering another registration
                                         * procedure if there is an on-going one
                                         * already */
    // add service will be proceeded after server registered
    sBluetoothGattInterface->RegisterServer(
        aAppUuid, new RegisterServerResultHandler(server));
  }
}

class BluetoothGattManager::ServerRemoveDescriptorResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ServerRemoveDescriptorResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::RemoveService failed: %d",
               (int)aStatus);

    // Reject the remove service request
    if (mServer->mRemoveServiceRunnable) {
      DispatchReplyError(mServer->mRemoveServiceRunnable,
                         u"Remove GATT service failed"_ns);
      mServer->mRemoveServiceRunnable = nullptr;
    }
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::ServerRemoveService(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing remove service request.
  if (server->mRemoveServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mRemoveServiceRunnable = aRunnable;

  sBluetoothGattInterface->DeleteService(
      server->mServerIf, aServiceHandle,
      new ServerRemoveDescriptorResultHandler(server));
}

class BluetoothGattManager::ServerStopServiceResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ServerStopServiceResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::StopService failed: %d",
               (int)aStatus);

    // Reject the remove service request
    if (mServer->mStopServiceRunnable) {
      DispatchReplyError(mServer->mStopServiceRunnable,
                         u"Stop GATT service failed"_ns);
      mServer->mStopServiceRunnable = nullptr;
    }
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::ServerStopService(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing stop service request.
  if (server->mStopServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mStopServiceRunnable = aRunnable;

  sBluetoothGattInterface->StopService(
      server->mServerIf, aServiceHandle,
      new ServerStopServiceResultHandler(server));
}

class BluetoothGattManager::ServerSendResponseResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ServerSendResponseResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void SendResponse() override {
    if (mServer->mSendResponseRunnable) {
      DispatchReplySuccess(mServer->mSendResponseRunnable);
      mServer->mSendResponseRunnable = nullptr;
    }
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothGattServerInterface::SendResponse failed: %d",
               (int)aStatus);

    // Reject the send response request
    if (mServer->mSendResponseRunnable) {
      DispatchReplyError(mServer->mSendResponseRunnable,
                         u"Send response failed"_ns);
      mServer->mSendResponseRunnable = nullptr;
    }
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::ServerSendResponse(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    uint16_t aStatus, int aRequestId, const BluetoothGattResponse& aRsp,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (server->mSendResponseRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  if (!server->mConnectionMap.Get(aAddress, &connId)) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }

  sBluetoothGattInterface->SendResponse(
      connId, aRequestId, aStatus, aRsp,
      new ServerSendResponseResultHandler(server));
}

class BluetoothGattManager::ServerSendIndicationResultHandler final
    : public BluetoothGattResultHandler {
 public:
  explicit ServerSendIndicationResultHandler(BluetoothGattServer* aServer)
      : mServer(aServer) {
    MOZ_ASSERT(mServer);
  }

  void SendIndication() override {
    if (mServer->mSendIndicationRunnable) {
      DispatchReplySuccess(mServer->mSendIndicationRunnable);
      mServer->mSendIndicationRunnable = nullptr;
    }
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING(
        "BluetoothGattServerInterface::NotifyCharacteristicChanged"
        "failed: %d",
        (int)aStatus);

    // Reject the send indication request
    if (mServer->mSendIndicationRunnable) {
      DispatchReplyError(mServer->mSendIndicationRunnable,
                         u"Send GATT indication failed"_ns);
      mServer->mSendIndicationRunnable = nullptr;
    }
  }

 private:
  RefPtr<BluetoothGattServer> mServer;
};

void BluetoothGattManager::ServerSendIndication(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    const BluetoothAttributeHandle& aCharacteristicHandle, bool aConfirm,
    const nsTArray<uint8_t>& aValue, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  // Reject the request if the server has not been registered yet.
  if (index == sServers->NoIndex) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the server has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing send indication request.
  if (server->mSendIndicationRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  if (!server->mConnectionMap.Get(aAddress, &connId)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  if (!connId) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }

  server->mSendIndicationRunnable = aRunnable;

  sBluetoothGattInterface->SendIndication(
      server->mServerIf, aCharacteristicHandle, connId, aValue, aConfirm,
      new ServerSendIndicationResultHandler(server));
}

//
// Notification Handlers
//
void BluetoothGattManager::RegisterClientNotification(
    BluetoothGattStatus aStatus, int aClientIf, const BluetoothUuid& aAppUuid) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus != GATT_STATUS_SUCCESS) {
    nsAutoString appUuidStr;
    UuidToString(aAppUuid, appUuidStr);

    BT_LOGD("RegisterClient failed: clientIf = %d, status = %d, appUuid = %s",
            aClientIf, aStatus, NS_ConvertUTF16toUTF8(appUuidStr).get());

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, aAppUuid,
                         BluetoothValue(false));  // Disconnected

    if (client->mConnectRunnable) {
      // Reject the connect request
      DispatchReplyError(client->mConnectRunnable,
                         u"Connect failed due to registration failed"_ns);
      client->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(client);
    return;
  }

  client->mClientIf = aClientIf;

  // Notify BluetoothGatt to update the clientIf
  bs->DistributeSignal(u"ClientRegistered"_ns, aAppUuid,
                       BluetoothValue(uint32_t(aClientIf)));

  if (client->mConnectRunnable) {
    // Client just registered, proceed remaining connect request.
    ENSURE_GATT_INTF_IS_READY_VOID(client->mConnectRunnable);
    sBluetoothGattInterface->Connect(aClientIf, client->mDeviceAddr,
                                     true /* direct connect */, TRANSPORT_AUTO,
                                     new ConnectResultHandler(client));
  }
}

void BluetoothGattManager::RegisterScannerNotification(
    BluetoothGattStatus aStatus, uint8_t aScannerId,
    const BluetoothUuid& aScanUuid) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sScanners->IndexOf(aScanUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sScanners->NoIndex);

  RefPtr<BluetoothGattScanner> scanner = sScanners->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus != GATT_STATUS_SUCCESS) {
    nsAutoString scanUuidStr;
    UuidToString(aScanUuid, scanUuidStr);

    BT_LOGD("RegisterScanner failed: scannerId = %d, status = %d, appUuid = %s",
            aScannerId, aStatus, NS_ConvertUTF16toUTF8(scanUuidStr).get());

    if (scanner->mStartLeScanRunnable) {
      // Reject the LE scan request
      DispatchReplyError(scanner->mStartLeScanRunnable,
                         u"StartLeScan failed due to registration failed"_ns);
      scanner->mStartLeScanRunnable = nullptr;
    }

    sScanners->RemoveElement(scanner);
    return;
  }

  scanner->mScannerId = aScannerId;

  // Notify BluetoothGatt to update the scannerId
  bs->DistributeSignal(u"ScannerRegistered"_ns, aScanUuid,
                       BluetoothValue(aScannerId));

  if (scanner->mStartLeScanRunnable) {
    // Client just registered, proceed remaining startLeScan request.
    ENSURE_GATT_INTF_IS_READY_VOID(scanner->mStartLeScanRunnable);
    sBluetoothGattInterface->Scan(aScannerId, true /* start */,
                                  new StartLeScanResultHandler(scanner));
  }
}

void BluetoothGattManager::RegisterAdvertiserNotification(
    BluetoothGattStatus aStatus, uint8_t aAdvertiserId,
    const BluetoothUuid& aAdvertiseUuid) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index =
      sAdvertisers->IndexOf(aAdvertiseUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sAdvertisers->NoIndex);

  RefPtr<BluetoothGattAdvertiser> advertiser = sAdvertisers->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus != GATT_STATUS_SUCCESS) {
    nsAutoString advertiseUuidStr;
    UuidToString(aAdvertiseUuid, advertiseUuidStr);

    BT_LOGD(
        "RegisterAdvertiser failed: advertiserId = %d, status = %d, appUuid = "
        "%s",
        aAdvertiserId, aStatus, NS_ConvertUTF16toUTF8(advertiseUuidStr).get());

    if (advertiser->mStartAdvertisingRunnable) {
      // Reject the advertise request
      DispatchReplyError(advertiser->mStartAdvertisingRunnable,
                         u"Advertise failed due to registration failed"_ns);
      advertiser->mStartAdvertisingRunnable = nullptr;
    }

    sAdvertisers->RemoveElement(advertiser);
    return;
  }

  advertiser->mAdvertiserId = aAdvertiserId;

  // Notify BluetoothGatt to update the advertiserId
  bs->DistributeSignal(u"AdvertiserRegistered"_ns, aAdvertiseUuid,
                       BluetoothValue(aAdvertiserId));

  if (advertiser->mStartAdvertisingRunnable) {
    // Advertiser just registered, proceed remaining advertise request.
    ENSURE_GATT_INTF_IS_READY_VOID(advertiser->mStartAdvertisingRunnable);
    sBluetoothGattInterface->Advertise(
        aAdvertiserId, advertiser->mAdvertisingData,
        new StartAdvertisingResultHandler(advertiser));
  }
}

class BluetoothGattManager::ScanDeviceTypeResultHandler final
    : public BluetoothGattResultHandler {
 public:
  ScanDeviceTypeResultHandler(const BluetoothAddress& aBdAddr, int aRssi,
                              const BluetoothGattAdvData& aAdvData)
      : mBdAddr(aBdAddr), mRssi(static_cast<int32_t>(aRssi)) {
    mAdvData.AppendElements(aAdvData.mAdvData, sizeof(aAdvData.mAdvData));
  }

  void GetDeviceType(BluetoothTypeOfDevice type) override {
    DistributeSignalDeviceFound(type);
  }

  void OnError(BluetoothStatus aStatus) override {
    DistributeSignalDeviceFound(TYPE_OF_DEVICE_BLE);
  }

 private:
  void DistributeSignalDeviceFound(BluetoothTypeOfDevice type) {
    MOZ_ASSERT(NS_IsMainThread());

    nsTArray<BluetoothNamedValue> properties;

    AppendNamedValue(properties, "Address", mBdAddr);
    AppendNamedValue(properties, "Rssi", mRssi);
    AppendNamedValue(properties, "GattAdv", mAdvData);
    AppendNamedValue(properties, "Type", static_cast<uint32_t>(type));

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    bs->DistributeSignal(u"LeDeviceFound"_ns, KEY_ADAPTER,
                         BluetoothValue(properties));
  }

  BluetoothAddress mBdAddr;
  int32_t mRssi;
  nsTArray<uint8_t> mAdvData;
};

void BluetoothGattManager::ScanResultNotification(
    const BluetoothAddress& aBdAddr, int aRssi,
    const BluetoothGattAdvData& aAdvData) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);

  // Distribute "LeDeviceFound" signal after we know the corresponding
  // BluetoothTypeOfDevice of the device
  sBluetoothGattInterface->GetDeviceType(
      aBdAddr, new ScanDeviceTypeResultHandler(aBdAddr, aRssi, aAdvData));
}

void BluetoothGattManager::ConnectNotification(
    int aConnId, BluetoothGattStatus aStatus, int aClientIf,
    const BluetoothAddress& aDeviceAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index =
      sClients->IndexOf(aClientIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    BT_LOGD("Connect failed: clientIf = %d, connId = %d, status = %d",
            aClientIf, aConnId, aStatus);

    // Notify BluetoothGatt that the client remains disconnected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, client->mAppUuid,
                         BluetoothValue(false));  // Disconnected

    // Reject the connect request
    if (client->mConnectRunnable) {
      DispatchReplyError(client->mConnectRunnable, u"Connect failed"_ns);
      client->mConnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = aConnId;

  // Notify BluetoothGatt for client connected
  bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, client->mAppUuid,
                       BluetoothValue(true));  // Connected

  // Resolve the connect request
  if (client->mConnectRunnable) {
    DispatchReplySuccess(client->mConnectRunnable);
    client->mConnectRunnable = nullptr;
  }
}

void BluetoothGattManager::DisconnectNotification(
    int aConnId, BluetoothGattStatus aStatus, int aClientIf,
    const BluetoothAddress& aDeviceAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index =
      sClients->IndexOf(aClientIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    // Notify BluetoothGatt that the client remains connected
    bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, client->mAppUuid,
                         BluetoothValue(true));  // Connected

    // Reject the disconnect request
    if (client->mDisconnectRunnable) {
      DispatchReplyError(client->mDisconnectRunnable, u"Disconnect failed"_ns);
      client->mDisconnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = 0;

  // Notify BluetoothGatt for client disconnected
  bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, client->mAppUuid,
                       BluetoothValue(false));  // Disconnected

  // Resolve the disconnect request
  if (client->mDisconnectRunnable) {
    DispatchReplySuccess(client->mDisconnectRunnable);
    client->mDisconnectRunnable = nullptr;
  }
}

void BluetoothGattManager::SearchCompleteNotification(
    int aConnId, BluetoothGattStatus aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus != GATT_STATUS_SUCCESS) {
    client->NotifyDiscoverCompleted(false);
    return;
  }

  // All services are discovered, continue to search included services of each
  // service if existed, otherwise, notify application that discover completed
  if (client->mDbElements.IsEmpty()) {
    ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client);

    sBluetoothGattInterface->GetGattDb(aConnId,
                                       new DiscoverResultHandler(client));
  } else {
    client->NotifyDiscoverCompleted(true);
  }
}

void BluetoothGattManager::RegisterNotificationNotification(
    int aConnId, int aIsRegister, BluetoothGattStatus aStatus,
    const BluetoothAttributeHandle& aHandle) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGD("aStatus = %d, aConnId = %d, aIsRegister = %d", aStatus, aConnId,
          aIsRegister);
}

void BluetoothGattManager::NotifyNotification(
    int aConnId, const BluetoothGattNotifyParam& aNotifyParam) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Notify BluetoothGattCharacteristic to update characteristic value
  nsString path;
  GeneratePathFromHandle(aNotifyParam.mHandle, path);

  nsTArray<uint8_t> value;
  value.AppendElements(aNotifyParam.mValue, aNotifyParam.mLength);

  bs->DistributeSignal(u"CharacteristicValueUpdated"_ns, path,
                       BluetoothValue(value));

  // Notify BluetoothGatt for characteristic changed
  nsTArray<BluetoothNamedValue> ids;
  ids.AppendElement(
      BluetoothNamedValue(u"gattHandle"_ns, aNotifyParam.mHandle));

  bs->DistributeSignal(GATT_CHARACTERISTIC_CHANGED_ID, client->mAppUuid,
                       BluetoothValue(ids));
}

void BluetoothGattManager::ReadCharacteristicNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattReadParam& aReadParam) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadCharacteristicState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
      client->mReadCharacteristicState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mReadCharacteristicState.Reset();
    // Notify BluetoothGattCharacteristic to update characteristic value
    nsString path;
    GeneratePathFromHandle(aReadParam.mHandle, path);

    nsTArray<uint8_t> value;
    value.AppendElements(aReadParam.mValue, aReadParam.mValueLength);

    bs->DistributeSignal(u"CharacteristicValueUpdated"_ns, path,
                         BluetoothValue(value));

    // Notify BluetoothGatt for characteristic changed
    nsTArray<BluetoothNamedValue> ids;
    ids.AppendElement(
        BluetoothNamedValue(u"gattHandle"_ns, aReadParam.mHandle));
    bs->DistributeSignal(GATT_CHARACTERISTIC_CHANGED_ID, client->mAppUuid,
                         BluetoothValue(ids));

    // Resolve the promise
    DispatchReplySuccess(runnable, BluetoothValue(value));
  } else if (!client->mReadCharacteristicState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mReadCharacteristicState.Reset();
      // Reject the promise
      DispatchReplyError(runnable, u"ReadCharacteristicValue failed"_ns);
      return;
    }

    client->mReadCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->ReadCharacteristic(
        aConnId, aReadParam.mHandle, GATT_AUTH_REQ_MITM,
        new ReadCharacteristicValueResultHandler(client));
  } else {
    client->mReadCharacteristicState.Reset();
    // Reject the promise
    DispatchReplyError(runnable, u"ReadCharacteristicValue failed"_ns);
  }
}

void BluetoothGattManager::WriteCharacteristicNotification(
    int aConnId, BluetoothGattStatus aStatus, uint16_t aHandle) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteCharacteristicState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
      client->mWriteCharacteristicState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteCharacteristicState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteCharacteristicState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mWriteCharacteristicState.Reset();
      // Reject the promise
      DispatchReplyError(runnable, u"WriteCharacteristicValue failed"_ns);
      return;
    }

    client->mWriteCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    BluetoothAttributeHandle handleStruct;
    handleStruct.mHandle = aHandle;

    sBluetoothGattInterface->WriteCharacteristic(
        aConnId, handleStruct, client->mWriteCharacteristicState.mWriteType,
        GATT_AUTH_REQ_MITM, client->mWriteCharacteristicState.mWriteValue,
        new WriteCharacteristicValueResultHandler(client));
  } else {
    client->mWriteCharacteristicState.Reset();
    // Reject the promise
    DispatchReplyError(runnable, u"WriteCharacteristicValue failed"_ns);
  }
}

void BluetoothGattManager::ReadDescriptorNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattReadParam& aReadParam) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadDescriptorState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
      client->mReadDescriptorState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mReadDescriptorState.Reset();
    // Notify BluetoothGattDescriptor to update descriptor value
    nsString path;
    GeneratePathFromHandle(aReadParam.mHandle, path);

    nsTArray<uint8_t> value;
    value.AppendElements(aReadParam.mValue, aReadParam.mValueLength);

    bs->DistributeSignal(u"DescriptorValueUpdated"_ns, path,
                         BluetoothValue(value));

    // Resolve the promise
    DispatchReplySuccess(runnable, BluetoothValue(value));
  } else if (!client->mReadDescriptorState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mReadDescriptorState.Reset();
      // Reject the promise
      DispatchReplyError(runnable, u"ReadDescriptorValue failed"_ns);
      return;
    }

    client->mReadDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->ReadDescriptor(
        aConnId, aReadParam.mHandle, GATT_AUTH_REQ_MITM,
        new ReadDescriptorValueResultHandler(client));
  } else {
    client->mReadDescriptorState.Reset();
    // Reject the promise
    DispatchReplyError(runnable, u"ReadDescriptorValue failed"_ns);
  }
}

void BluetoothGattManager::WriteDescriptorNotification(
    int aConnId, BluetoothGattStatus aStatus, uint16_t aHandle) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteDescriptorState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
      client->mWriteDescriptorState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteDescriptorState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteDescriptorState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mWriteDescriptorState.Reset();
      // Reject the promise
      DispatchReplyError(runnable, u"WriteDescriptorValue failed"_ns);
      return;
    }

    client->mWriteDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    BluetoothAttributeHandle handleStruct;
    handleStruct.mHandle = aHandle;

    sBluetoothGattInterface->WriteDescriptor(
        aConnId, handleStruct, GATT_WRITE_TYPE_NORMAL, GATT_AUTH_REQ_MITM,
        client->mWriteDescriptorState.mWriteValue,
        new WriteDescriptorValueResultHandler(client));
  } else {
    client->mWriteDescriptorState.Reset();
    // Reject the promise
    DispatchReplyError(runnable, u"WriteDescriptorValue failed"_ns);
  }
}

void BluetoothGattManager::ExecuteWriteNotification(
    int aConnId, BluetoothGattStatus aStatus) {}

void BluetoothGattManager::ReadRemoteRssiNotification(
    int aClientIf, const BluetoothAddress& aBdAddr, int aRssi,
    BluetoothGattStatus aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index =
      sClients->IndexOf(aClientIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {  // operation failed
    nsAutoString addressStr;
    AddressToString(aBdAddr, addressStr);
    BT_LOGD(
        "ReadRemoteRssi failed: clientIf = %d, bdAddr = %s, rssi = %d, "
        "status = %d",
        aClientIf, NS_ConvertUTF16toUTF8(addressStr).get(), aRssi,
        (int)aStatus);

    // Reject the read remote rssi request
    if (client->mReadRemoteRssiRunnable) {
      DispatchReplyError(client->mReadRemoteRssiRunnable,
                         u"ReadRemoteRssi failed"_ns);
      client->mReadRemoteRssiRunnable = nullptr;
    }

    return;
  }

  // Resolve the read remote rssi request
  if (client->mReadRemoteRssiRunnable) {
    DispatchReplySuccess(client->mReadRemoteRssiRunnable,
                         BluetoothValue(static_cast<int32_t>(aRssi)));
    client->mReadRemoteRssiRunnable = nullptr;
  }
}

/*
 * Some actions will trigger the registration procedure. These actions will
 * be taken only after the registration has been done successfully.
 * If the registration fails, all the existing actions above should be
 * rejected.
 */
void BluetoothGattManager::RegisterServerNotification(
    BluetoothGattStatus aStatus, int aServerIf, const BluetoothUuid& aAppUuid) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  server->mIsRegistering = false;

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS || !sBluetoothGattInterface) {
    nsAutoString appUuidStr;
    UuidToString(aAppUuid, appUuidStr);

    BT_LOGD("RegisterServer failed: serverIf = %d, status = %d, appUuid = %s",
            aServerIf, aStatus, NS_ConvertUTF16toUTF8(appUuidStr).get());

    if (server->mConnectPeripheralRunnable) {
      // Reject the connect peripheral request
      DispatchReplyError(
          server->mConnectPeripheralRunnable,
          u"ConnectPeripheral failed due to registration failed"_ns);
      server->mConnectPeripheralRunnable = nullptr;
    }

    if (server->mAddServiceState.mRunnable) {
      // Reject the add service request
      DispatchReplyError(server->mAddServiceState.mRunnable,
                         u"AddService failed due to registration failed"_ns);
      server->mAddServiceState.Reset();
    }

    if (server->mRegisterServerRunnable) {
      // Reject the register server request
      DispatchReplyError(server->mRegisterServerRunnable,
                         u"Register server failed"_ns);
      server->mRegisterServerRunnable = nullptr;
    }

    sServers->RemoveElement(server);
    return;
  }

  server->mServerIf = aServerIf;

  // Notify BluetoothGattServer to update the serverIf
  bs->DistributeSignal(u"ServerRegistered"_ns, aAppUuid,
                       BluetoothValue(uint32_t(aServerIf)));

  if (server->mConnectPeripheralRunnable) {
    // Only one entry exists in the map during first connect peripheral request
    const BluetoothAddress& deviceAddr = server->mConnectionMap.Iter().Key();

    sBluetoothGattInterface->ConnectPeripheral(
        aServerIf, deviceAddr, true /* direct connect */, TRANSPORT_AUTO,
        new ConnectPeripheralResultHandler(server, deviceAddr));
  }

  if (server->mAddServiceState.mRunnable) {
    sBluetoothGattInterface->AddService(
        server->mServerIf, server->mAddServiceState.mGattDb,
        new ServerAddServiceResultHandler(server));
  }

  if (server->mRegisterServerRunnable) {
    DispatchReplySuccess(server->mRegisterServerRunnable);
    server->mRegisterServerRunnable = nullptr;
  }
}

void BluetoothGattManager::GetGattDbNotification(
    int aConnId, const nsTArray<BluetoothGattDbElement>& aDb) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  for (unsigned int i = 0; i < aDb.Length(); i++) {
    client->mDbElements.AppendElement(aDb[i]);
  }

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  // Notify BluetoothGatt to create all services
  bs->DistributeSignal(u"ServicesDiscovered"_ns, client->mAppUuid,
                       BluetoothValue(client->mDbElements));

  client->NotifyDiscoverCompleted(true);
}

void BluetoothGattManager::ConnectionNotification(
    int aConnId, int aServerIf, bool aConnected,
    const BluetoothAddress& aBdAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index =
      sServers->IndexOf(aServerIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Update the connection map based on the connection status
  if (aConnected) {
    server->mConnectionMap.InsertOrUpdate(aBdAddr, aConnId);
  } else {
    server->mConnectionMap.Remove(aBdAddr);
  }

  // Notify BluetoothGattServer that connection status changed
  nsTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "Connected", aConnected);
  AppendNamedValue(props, "Address", aBdAddr);
  bs->DistributeSignal(GATT_CONNECTION_STATE_CHANGED_ID, server->mAppUuid,
                       BluetoothValue(props));

  // Resolve or reject connect/disconnect peripheral requests
  if (server->mConnectPeripheralRunnable) {
    if (aConnected) {
      DispatchReplySuccess(server->mConnectPeripheralRunnable);
    } else {
      DispatchReplyError(server->mConnectPeripheralRunnable,
                         u"ConnectPeripheral failed"_ns);
    }
    server->mConnectPeripheralRunnable = nullptr;
  } else if (server->mDisconnectPeripheralRunnable) {
    if (!aConnected) {
      DispatchReplySuccess(server->mDisconnectPeripheralRunnable);
    } else {
      DispatchReplyError(server->mDisconnectPeripheralRunnable,
                         u"DisconnectPeripheral failed"_ns);
    }
    server->mDisconnectPeripheralRunnable = nullptr;
  }
}

void BluetoothGattManager::ServiceAddedNotification(
    BluetoothGattStatus aStatus, int aServerIf,
    const nsTArray<BluetoothGattDbElement>& aDb) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index =
      sServers->IndexOf(aServerIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS) {
    if (server->mAddServiceState.mRunnable) {
      DispatchReplyError(server->mAddServiceState.mRunnable,
                         u"ServiceAddedNotification failed"_ns);
      server->mAddServiceState.Reset();
    }
    return;
  }

  // Notify BluetoothGattServer to update service handle
  bs->DistributeSignal(u"ServerServiceUpdated"_ns, server->mAppUuid,
                       BluetoothValue(aDb));

  if (server->mAddServiceState.mRunnable) {
    DispatchReplySuccess(server->mAddServiceState.mRunnable);
    server->mAddServiceState.Reset();
  }
}

void BluetoothGattManager::ServiceStoppedNotification(
    BluetoothGattStatus aStatus, int aServerIf,
    const BluetoothAttributeHandle& aServiceHandle) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index =
      sServers->IndexOf(aServerIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mStopServiceRunnable) {
      DispatchReplyError(server->mStopServiceRunnable,
                         u"ServiceStoppedNotification failed"_ns);
      server->mStopServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mStopServiceRunnable) {
    DispatchReplySuccess(server->mStopServiceRunnable);
    server->mStopServiceRunnable = nullptr;
  }
}

void BluetoothGattManager::ServiceDeletedNotification(
    BluetoothGattStatus aStatus, int aServerIf,
    const BluetoothAttributeHandle& aServiceHandle) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t index =
      sServers->IndexOf(aServerIf, 0 /* Start */, InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mRemoveServiceRunnable) {
      DispatchReplyError(server->mRemoveServiceRunnable,
                         u"ServiceStoppedNotification failed"_ns);
      server->mRemoveServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mRemoveServiceRunnable) {
    DispatchReplySuccess(server->mRemoveServiceRunnable);
    server->mRemoveServiceRunnable = nullptr;
  }
}

void BluetoothGattManager::RequestReadNotification(
    int aConnId, int aTransId, const BluetoothAddress& aBdAddr,
    const BluetoothAttributeHandle& aAttributeHandle, int aOffset,
    bool aIsLong) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aConnId);
  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sServers->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Send an error response for unsupported requests
  if (aIsLong || aOffset > 0) {
    BT_LOGR("Unsupported long attribute read requests");
    BluetoothGattResponse response;
    memset(&response, 0, sizeof(BluetoothGattResponse));
    sBluetoothGattInterface->SendResponse(
        aConnId, aTransId, GATT_STATUS_REQUEST_NOT_SUPPORTED, response,
        new ServerSendResponseResultHandler(server));
    return;
  }

  // Distribute a signal to gattServer
  nsTArray<BluetoothNamedValue> properties;

  AppendNamedValue(properties, "TransId", aTransId);
  AppendNamedValue(properties, "AttrHandle", aAttributeHandle);
  AppendNamedValue(properties, "Address", aBdAddr);
  AppendNamedValue(properties, "NeedResponse", true);
  AppendNamedValue(properties, "Value", nsTArray<uint8_t>());

  bs->DistributeSignal(u"ReadRequested"_ns, server->mAppUuid, properties);
}

void BluetoothGattManager::RequestWriteNotification(
    int aConnId, int aTransId, const BluetoothAddress& aBdAddr,
    const BluetoothAttributeHandle& aAttributeHandle, int aOffset, int aLength,
    const uint8_t* aValue, bool aNeedResponse, bool aIsPrepareWrite) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aConnId);
  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sServers->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Send an error response for unsupported requests
  if (aIsPrepareWrite || aOffset > 0) {
    BT_LOGR("Unsupported prepare write or long attribute write requests");
    if (aNeedResponse) {
      BluetoothGattResponse response;
      memset(&response, 0, sizeof(BluetoothGattResponse));
      sBluetoothGattInterface->SendResponse(
          aConnId, aTransId, GATT_STATUS_REQUEST_NOT_SUPPORTED, response,
          new ServerSendResponseResultHandler(server));
    }
    return;
  }

  // Distribute a signal to gattServer
  nsTArray<BluetoothNamedValue> properties;

  AppendNamedValue(properties, "TransId", aTransId);
  AppendNamedValue(properties, "AttrHandle", aAttributeHandle);
  AppendNamedValue(properties, "Address", aBdAddr);
  AppendNamedValue(properties, "NeedResponse", aNeedResponse);

  nsTArray<uint8_t> value;
  value.AppendElements(aValue, aLength);
  AppendNamedValue(properties, "Value", value);

  bs->DistributeSignal(u"WriteRequested"_ns, server->mAppUuid, properties);
}

BluetoothGattManager::BluetoothGattManager() {}

BluetoothGattManager::~BluetoothGattManager() {}

NS_IMETHODIMP
BluetoothGattManager::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  MOZ_ASSERT(sBluetoothGattManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothGattManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void BluetoothGattManager::HandleShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  mInShutdown = true;
  sBluetoothGattManager = nullptr;
  sClients = nullptr;
  sServers = nullptr;
  sScanners = nullptr;
  sAdvertisers = nullptr;
}

NS_IMPL_ISUPPORTS(BluetoothGattManager, nsIObserver)
