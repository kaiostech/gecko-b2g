/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "BluetoothServiceBluedroid.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothAvrcpManager.h"
#include "BluetoothGattManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothMapSmsManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothPbapManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothUtils.h"
#include "BluetoothUuidHelper.h"

#include "mozilla/ArrayUtils.h"  // MOZ_ARRAY_LENGTH

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#define ENSURE_BLUETOOTH_IS_ENABLED(runnable, result)               \
  do {                                                              \
    if (!IsEnabled() || !sBtCoreInterface) {                        \
      DispatchReplyError(runnable, u"Bluetooth is not enabled"_ns); \
      return result;                                                \
    }                                                               \
  } while (0)

#define ENSURE_BLUETOOTH_IS_ENABLED_VOID(runnable)                  \
  do {                                                              \
    if (!IsEnabled() || !sBtCoreInterface) {                        \
      DispatchReplyError(runnable, u"Bluetooth is not enabled"_ns); \
      return;                                                       \
    }                                                               \
  } while (0)

#define ENSURE_GATT_MGR_IS_READY_VOID(gatt, runnable)               \
  do {                                                              \
    if (!gatt) {                                                    \
      DispatchReplyError(runnable, u"GattManager is not ready"_ns); \
      return;                                                       \
    }                                                               \
  } while (0)

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

static BluetoothInterface* sBtInterface;
static BluetoothCoreInterface* sBtCoreInterface;
static nsTArray<RefPtr<BluetoothProfileController>> sControllerArray;
static StaticMutex sServiceRecordLock;

class BluetoothServiceBluedroid::EnableResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Enable failed: %d", aStatus);

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

/* |ProfileInitResultHandler| collects the results of all profile
 * result handlers and calls |Proceed| after all results handlers
 * have been run.
 */
class BluetoothServiceBluedroid::ProfileInitResultHandler final
    : public BluetoothProfileResultHandler {
 public:
  explicit ProfileInitResultHandler(unsigned char aNumProfiles)
      : mNumProfiles(aNumProfiles) {
    MOZ_ASSERT(mNumProfiles);
  }

  void Init() override {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

  void OnError(nsresult aResult) override {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

 private:
  void Proceed() const {
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    sBtCoreInterface = sBtInterface->GetBluetoothCoreInterface();
    NS_ENSURE_TRUE_VOID(sBtCoreInterface);

    sBtCoreInterface->SetNotificationHandler(
        reinterpret_cast<BluetoothServiceBluedroid*>(bs));

    sBtCoreInterface->Enable(new EnableResultHandler());
  }

  unsigned char mNumProfiles;
};

class BluetoothServiceBluedroid::InitResultHandler final
    : public BluetoothResultHandler {
 public:
  void Init() override {
    static void (*const sInitManager[])(BluetoothProfileResultHandler*) = {
        BluetoothMapSmsManager::InitMapSmsInterface,
        BluetoothOppManager::InitOppInterface,
        BluetoothPbapManager::InitPbapInterface,
        BluetoothHidManager::InitHidInterface,
        BluetoothHfpManager::InitHfpInterface,
        BluetoothA2dpManager::InitA2dpInterface,
        BluetoothAvrcpManager::InitAvrcpInterface,
        BluetoothGattManager::InitGattInterface};

    MOZ_ASSERT(NS_IsMainThread());

    // Register all the bluedroid callbacks before enable() gets called. This is
    // required to register a2dp callbacks before a2dp media task starts up.
    // If any interface cannot be initialized, turn on bluetooth core anyway.
    RefPtr<ProfileInitResultHandler> res =
        new ProfileInitResultHandler(MOZ_ARRAY_LENGTH(sInitManager));

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sInitManager); ++i) {
      sInitManager[i](res);
    }
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Init failed: %d", aStatus);

    sBtInterface = nullptr;

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

nsresult BluetoothServiceBluedroid::StartGonkBluetooth() {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  if (bs->IsEnabled()) {
    // Keep current enable status
    BluetoothService::AcknowledgeToggleBt(true);
    return NS_OK;
  }

  sBtInterface->Init(reinterpret_cast<BluetoothServiceBluedroid*>(bs),
                     new InitResultHandler());

  return NS_OK;
}

class BluetoothServiceBluedroid::DisableResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Disable failed: %d", aStatus);

    // Always make progress; even on failures
    BluetoothService::AcknowledgeToggleBt(false);
  }
};

nsresult BluetoothServiceBluedroid::StopGonkBluetooth() {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  if (!bs->IsEnabled()) {
    // Keep current enable status
    BluetoothService::AcknowledgeToggleBt(false);
    return NS_OK;
  }

  NS_ENSURE_TRUE(sBtCoreInterface, NS_ERROR_FAILURE);

  sBtCoreInterface->Disable(new DisableResultHandler());

  return NS_OK;
}

/*
 *  Member functions
 */

BluetoothServiceBluedroid::BluetoothServiceBluedroid()
    : mEnabled(false),
      mDiscoverable(false),
      mDiscovering(false),
      mIsRestart(false),
      mIsFirstTimeToggleOffBt(false) {
  sBtInterface = BluetoothInterface::GetInstance();
  if (!sBtInterface) {
    BT_LOGR("Error! Failed to get instance of bluetooth interface");
    return;
  }
}

BluetoothServiceBluedroid::~BluetoothServiceBluedroid() {}

nsresult BluetoothServiceBluedroid::StartInternal(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  // aRunnable will be a nullptr while startup
  if (aRunnable) {
    mChangeAdapterStateRunnables.AppendElement(aRunnable);
  }

  nsresult ret = StartGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(false);

    // Reject Promise
    if (aRunnable) {
      DispatchReplyError(aRunnable, u"StartBluetoothError"_ns);
      mChangeAdapterStateRunnables.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
}

nsresult BluetoothServiceBluedroid::StopInternal(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothProfileManagerBase* sProfiles[] = {
      // BluetoothGattManager not handled here
      BluetoothAvrcpManager::Get(),  BluetoothA2dpManager::Get(),
      BluetoothHfpManager::Get(),    BluetoothHidManager::Get(),
      BluetoothPbapManager::Get(),   BluetoothOppManager::Get(),
      BluetoothMapSmsManager::Get(),
  };

  // Disconnect all connected profiles
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sProfiles); i++) {
    if (NS_WARN_IF(!sProfiles[i])) {
      BT_LOGR("Profile manager sProfiles[%d] is null", i);
      return NS_ERROR_FAILURE;
    }

    nsCString profileName;
    sProfiles[i]->GetName(profileName);

    if (sProfiles[i]->IsConnected()) {
      sProfiles[i]->Disconnect(nullptr);
    } else if (!profileName.EqualsLiteral("OPP") &&
               !profileName.EqualsLiteral("PBAP") &&
               !profileName.EqualsLiteral("MapSms")) {
      sProfiles[i]->Reset();
    }
  }

  // aRunnable will be a nullptr during starup and shutdown
  if (aRunnable) {
    mChangeAdapterStateRunnables.AppendElement(aRunnable);
  }

  nsresult ret = StopGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(true);

    // Reject Promise
    if (aRunnable) {
      DispatchReplyError(aRunnable, u"StopBluetoothError"_ns);
      mChangeAdapterStateRunnables.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
}

//
// GATT Client
//

void BluetoothServiceBluedroid::StartLeScanInternal(
    const nsTArray<BluetoothUuid>& aServiceUuids,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->StartLeScan(aServiceUuids, aRunnable);
}

void BluetoothServiceBluedroid::StopLeScanInternal(
    const BluetoothUuid& aScanUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->StopLeScan(aScanUuid, aRunnable);
}

void BluetoothServiceBluedroid::StartAdvertisingInternal(
    const BluetoothUuid& aAppUuid, const BluetoothGattAdvertisingData& aAdvData,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  if (aAdvData.mIncludeDevName) {
    auto& advData = const_cast<BluetoothGattAdvertisingData&>(aAdvData);
    advData.mDeviceName = mBdName;
  }

  gatt->StartAdvertising(aAppUuid, aAdvData, aRunnable);
}

void BluetoothServiceBluedroid::StopAdvertisingInternal(
    const BluetoothUuid& aAppUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->StopAdvertising(aAppUuid, aRunnable);
}

void BluetoothServiceBluedroid::ConnectGattClientInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Connect(aAppUuid, aDeviceAddress, aRunnable);
}

void BluetoothServiceBluedroid::DisconnectGattClientInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Disconnect(aAppUuid, aDeviceAddress, aRunnable);
}

void BluetoothServiceBluedroid::DiscoverGattServicesInternal(
    const BluetoothUuid& aAppUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Discover(aAppUuid, aRunnable);
}

void BluetoothServiceBluedroid::GattClientStartNotificationsInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->RegisterNotifications(aAppUuid, aHandle, aRunnable);
}

void BluetoothServiceBluedroid::GattClientStopNotificationsInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->DeregisterNotifications(aAppUuid, aHandle, aRunnable);
}

void BluetoothServiceBluedroid::UnregisterGattClientInternal(
    int aClientIf, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->UnregisterClient(aClientIf, aRunnable);
}

void BluetoothServiceBluedroid::GattClientReadRemoteRssiInternal(
    int aClientIf, const BluetoothAddress& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadRemoteRssi(aClientIf, aDeviceAddress, aRunnable);
}

void BluetoothServiceBluedroid::GattClientReadCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadCharacteristicValue(aAppUuid, aHandle, aRunnable);
}

void BluetoothServiceBluedroid::GattClientWriteCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    const BluetoothGattWriteType& aWriteType, const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->WriteCharacteristicValue(aAppUuid, aHandle, aWriteType, aValue,
                                 aRunnable);
}

void BluetoothServiceBluedroid::GattClientReadDescriptorValueInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadDescriptorValue(aAppUuid, aHandle, aRunnable);
}

void BluetoothServiceBluedroid::GattClientWriteDescriptorValueInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAttributeHandle& aHandle,
    const nsTArray<uint8_t>& aValue, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->WriteDescriptorValue(aAppUuid, aHandle, aValue, aRunnable);
}

// GATT Server
void BluetoothServiceBluedroid::GattServerRegisterInternal(
    const BluetoothUuid& aAppUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->RegisterServer(aAppUuid, aRunnable);
}

void BluetoothServiceBluedroid::GattServerConnectPeripheralInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ConnectPeripheral(aAppUuid, aAddress, aRunnable);
}

void BluetoothServiceBluedroid::GattServerDisconnectPeripheralInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->DisconnectPeripheral(aAppUuid, aAddress, aRunnable);
}

void BluetoothServiceBluedroid::UnregisterGattServerInternal(
    int aServerIf, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->UnregisterServer(aServerIf, aRunnable);
}

void BluetoothServiceBluedroid::GattServerAddServiceInternal(
    const BluetoothUuid& aAppUuid, const nsTArray<BluetoothGattDbElement>& aDb,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ServerAddService(aAppUuid, aDb, aRunnable);
}

void BluetoothServiceBluedroid::GattServerRemoveServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ServerRemoveService(aAppUuid, aServiceHandle, aRunnable);
}

void BluetoothServiceBluedroid::GattServerStopServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ServerStopService(aAppUuid, aServiceHandle, aRunnable);
}

void BluetoothServiceBluedroid::GattServerSendResponseInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    uint16_t aStatus, int32_t aRequestId, const BluetoothGattResponse& aRsp,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ServerSendResponse(aAppUuid, aAddress, aStatus, aRequestId, aRsp,
                           aRunnable);
}

void BluetoothServiceBluedroid::GattServerSendIndicationInternal(
    const BluetoothUuid& aAppUuid, const BluetoothAddress& aAddress,
    const BluetoothAttributeHandle& aCharacteristicHandle, bool aConfirm,
    const nsTArray<uint8_t>& aValue, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ServerSendIndication(aAppUuid, aAddress, aCharacteristicHandle,
                             aConfirm, aValue, aRunnable);
}

nsresult BluetoothServiceBluedroid::GetAdaptersInternal(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * Wrap BluetoothValue =
   *   BluetoothNamedValue[]
   *     |
   *     |__ BluetoothNamedValue =
   *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
   *     |
   *     |__ BluetoothNamedValue =
   *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
   *     ...
   */
  nsTArray<BluetoothNamedValue> adaptersProperties;
  uint32_t numAdapters = 1;  // Bluedroid supports single adapter only

  for (uint32_t i = 0; i < numAdapters; i++) {
    nsTArray<BluetoothNamedValue> properties;

    AppendNamedValue(properties, "State", mEnabled);
    AppendNamedValue(properties, "Address", mBdAddress);
    AppendNamedValue(properties, "Name", mBdName);
    AppendNamedValue(properties, "Discoverable", mDiscoverable);
    AppendNamedValue(properties, "Discovering", mDiscovering);
    AppendNamedValue(properties, "PairedDevices", mBondedAddresses);

    AppendNamedValue(adaptersProperties, "Adapter", BluetoothValue(properties));
  }

  DispatchReplySuccess(aRunnable, adaptersProperties);
  return NS_OK;
}

class BluetoothServiceBluedroid::GetRemoteDevicePropertiesResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  GetRemoteDevicePropertiesResultHandler(nsTArray<GetDeviceRequest>& aRequests,
                                         const BluetoothAddress& aDeviceAddress)
      : mRequests(aRequests), mDeviceAddress(aDeviceAddress) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mRequests.IsEmpty());

    nsAutoString addressString;
    AddressToString(mDeviceAddress, addressString);

    BT_WARNING("GetRemoteDeviceProperties(%s) failed: %d",
               NS_ConvertUTF16toUTF8(addressString).get(), aStatus);

    /* Dispatch result after the final pending operation */
    if (--mRequests[0].mDeviceCount == 0) {
      if (mRequests[0].mRunnable) {
        DispatchReplySuccess(mRequests[0].mRunnable, mRequests[0].mDevicesPack);
      }
      mRequests.RemoveElementAt(0);
    }
  }

 private:
  nsTArray<GetDeviceRequest>& mRequests;
  BluetoothAddress mDeviceAddress;
};

nsresult BluetoothServiceBluedroid::GetConnectedDevicePropertiesInternal(
    uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  BluetoothProfileManagerBase* profile =
      BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    DispatchReplyError(aRunnable, ERR_UNKNOWN_PROFILE);
    return NS_OK;
  }

  // Reply success if no device of this profile is connected
  if (!profile->IsConnected()) {
    DispatchReplySuccess(aRunnable, nsTArray<BluetoothNamedValue>());
    return NS_OK;
  }

  // Get address of the connected device
  BluetoothAddress address;
  profile->GetAddress(address);

  // Append request of the connected device
  GetDeviceRequest request(1, aRunnable);
  mGetDeviceRequests.AppendElement(request);

  sBtCoreInterface->GetRemoteDeviceProperties(
      address,
      new GetRemoteDevicePropertiesResultHandler(mGetDeviceRequests, address));

  return NS_OK;
}

nsresult BluetoothServiceBluedroid::GetPairedDevicePropertiesInternal(
    const nsTArray<BluetoothAddress>& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  if (aDeviceAddress.IsEmpty()) {
    DispatchReplySuccess(aRunnable);
    return NS_OK;
  }

  // Append request of all paired devices
  GetDeviceRequest request(aDeviceAddress.Length(), aRunnable);
  mGetDeviceRequests.AppendElement(request);

  for (uint8_t i = 0; i < aDeviceAddress.Length(); i++) {
    // Retrieve all properties of devices
    sBtCoreInterface->GetRemoteDeviceProperties(
        aDeviceAddress[i], new GetRemoteDevicePropertiesResultHandler(
                               mGetDeviceRequests, aDeviceAddress[i]));
  }

  return NS_OK;
}

class BluetoothServiceBluedroid::DispatchReplyErrorResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  DispatchReplyErrorResultHandler(
      nsTArray<RefPtr<BluetoothReplyRunnable>>& aRunnableArray,
      BluetoothReplyRunnable* aRunnable)
      : mRunnableArray(aRunnableArray), mRunnable(aRunnable) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    mRunnableArray.RemoveElement(mRunnable);
    if (mRunnable) {
      DispatchReplyError(mRunnable, aStatus);
    }
  }

 private:
  nsTArray<RefPtr<BluetoothReplyRunnable>>& mRunnableArray;
  RefPtr<BluetoothReplyRunnable> mRunnable;
};

void BluetoothServiceBluedroid::StartDiscoveryInternal(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("");
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtCoreInterface->StartDiscovery(new DispatchReplyErrorResultHandler(
      mChangeDiscoveryRunnables, aRunnable));
}

nsresult BluetoothServiceBluedroid::FetchUuidsInternal(
    const BluetoothAddress& aDeviceAddress, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  /*
   * get_remote_services request will not be performed by bluedroid
   * if it is currently discovering nearby remote devices.
   */
  if (mDiscovering) {
    StopDiscoveryInternal(aRunnable);
  }

  mFetchUuidsRunnables.AppendElement(aRunnable);
  sBtCoreInterface->GetRemoteServices(
      aDeviceAddress,
      new DispatchReplyErrorResultHandler(mFetchUuidsRunnables, aRunnable));

  return NS_OK;
}

void BluetoothServiceBluedroid::StopDiscoveryInternal(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("");
  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtCoreInterface->CancelDiscovery(new DispatchReplyErrorResultHandler(
      mChangeDiscoveryRunnables, aRunnable));
}

nsresult BluetoothServiceBluedroid::SetProperty(
    BluetoothObjectType aType, const BluetoothNamedValue& aValue,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  BluetoothProperty property;
  nsresult rv = NamedValueToProperty(aValue, property);
  if (NS_FAILED(rv)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return rv;
  }

  mSetAdapterPropertyRunnables.AppendElement(aRunnable);
  sBtCoreInterface->SetAdapterProperty(
      property, new DispatchReplyErrorResultHandler(
                    mSetAdapterPropertyRunnables, aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::GetRemoteServiceRecordResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  GetRemoteServiceRecordResultHandler(
      nsTArray<GetRemoteServiceRecordRequest>& aGetRemoteServiceRecordArray,
      const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aUuid)
      : mGetRemoteServiceRecordArray(aGetRemoteServiceRecordArray),
        mDeviceAddress(aDeviceAddress),
        mUuid(aUuid) {}

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("Failed to GetRemoteServiceRecord, status: %d", aStatus);

    StaticMutexAutoLock lock(sServiceRecordLock);

    // Find call in array

    ssize_t i = FindRequest();

    if (i == -1) {
      BT_WARNING("No GetRemoteService request found");
      return;
    }

    // Signal error to profile manager

    mGetRemoteServiceRecordArray[i].mManager->OnGetServiceChannel(
        mDeviceAddress, mUuid, -1);
    mGetRemoteServiceRecordArray.RemoveElementAt(i);
  }

  void CancelDiscovery() override {
    // Disabled discovery mode, now perform SDP operation.
    sBtCoreInterface->GetRemoteServiceRecord(mDeviceAddress, mUuid, this);
  }

 private:
  ssize_t FindRequest() const {
    for (size_t i = 0; i < mGetRemoteServiceRecordArray.Length(); ++i) {
      if ((mGetRemoteServiceRecordArray[i].mDeviceAddress == mDeviceAddress) &&
          (mGetRemoteServiceRecordArray[i].mUuid == mUuid)) {
        return i;
      }
    }

    return -1;
  }

  nsTArray<GetRemoteServiceRecordRequest>& mGetRemoteServiceRecordArray;
  BluetoothAddress mDeviceAddress;
  BluetoothUuid mUuid;
};

nsresult BluetoothServiceBluedroid::GetServiceChannel(
    const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aServiceUuid,
    BluetoothProfileManagerBase* aManager) {
  StaticMutexAutoLock lock(sServiceRecordLock);

  mGetRemoteServiceRecordArray.AppendElement(
      GetRemoteServiceRecordRequest(aDeviceAddress, aServiceUuid, aManager));

  RefPtr<BluetoothCoreResultHandler> res =
      new GetRemoteServiceRecordResultHandler(mGetRemoteServiceRecordArray,
                                              aDeviceAddress, aServiceUuid);

  /* Stop discovery of remote devices here, because SDP operations
   * won't be performed while the adapter is in discovery mode.
   */
  if (mDiscovering) {
    sBtCoreInterface->CancelDiscovery(res);
  } else {
    sBtCoreInterface->GetRemoteServiceRecord(aDeviceAddress, aServiceUuid, res);
  }

  return NS_OK;
}

class BluetoothServiceBluedroid::GetRemoteServicesResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  GetRemoteServicesResultHandler(
      nsTArray<GetRemoteServicesRequest>& aGetRemoteServicesArray,
      const BluetoothAddress& aDeviceAddress,
      BluetoothProfileManagerBase* aManager)
      : mGetRemoteServicesArray(aGetRemoteServicesArray),
        mDeviceAddress(aDeviceAddress),
        mManager(aManager) {}

  void OnError(BluetoothStatus aStatus) override {
    // Find call in array

    ssize_t i = FindRequest();

    if (i == -1) {
      BT_WARNING("No GetRemoteServices request found");
      return;
    }

    // Cleanup array
    mGetRemoteServicesArray.RemoveElementAt(i);

    // There's no error-signaling mechanism; just call manager
    mManager->OnUpdateSdpRecords(mDeviceAddress);
  }

  void CancelDiscovery() override {
    // Disabled discovery mode, now perform SDP operation.
    sBtCoreInterface->GetRemoteServices(mDeviceAddress, this);
  }

 private:
  ssize_t FindRequest() const {
    for (size_t i = 0; i < mGetRemoteServicesArray.Length(); ++i) {
      if ((mGetRemoteServicesArray[i].mDeviceAddress == mDeviceAddress) &&
          (mGetRemoteServicesArray[i].mManager == mManager)) {
        return i;
      }
    }

    return -1;
  }

  nsTArray<GetRemoteServicesRequest>& mGetRemoteServicesArray;
  BluetoothAddress mDeviceAddress;
  BluetoothProfileManagerBase* mManager;
};

bool BluetoothServiceBluedroid::UpdateSdpRecords(
    const BluetoothAddress& aDeviceAddress,
    BluetoothProfileManagerBase* aManager) {
  mGetRemoteServicesArray.AppendElement(
      GetRemoteServicesRequest(aDeviceAddress, aManager));

  RefPtr<BluetoothCoreResultHandler> res = new GetRemoteServicesResultHandler(
      mGetRemoteServicesArray, aDeviceAddress, aManager);

  /* Stop discovery of remote devices here, because SDP operations
   * won't be performed while the adapter is in discovery mode.
   */
  if (mDiscovering) {
    sBtCoreInterface->CancelDiscovery(res);
  } else {
    sBtCoreInterface->GetRemoteServices(aDeviceAddress, res);
  }

  return true;
}

nsresult BluetoothServiceBluedroid::CreatePairedDeviceInternal(
    const BluetoothAddress& aDeviceAddress, int aTimeout,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  mCreateBondRunnables.AppendElement(aRunnable);
  sBtCoreInterface->CreateBond(
      aDeviceAddress, TRANSPORT_AUTO,
      new DispatchReplyErrorResultHandler(mCreateBondRunnables, aRunnable));

  return NS_OK;
}

nsresult BluetoothServiceBluedroid::RemoveDeviceInternal(
    const BluetoothAddress& aDeviceAddress, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED(aRunnable, NS_OK);

  mRemoveBondRunnables.AppendElement(aRunnable);
  sBtCoreInterface->RemoveBond(
      aDeviceAddress,
      new DispatchReplyErrorResultHandler(mRemoveBondRunnables, aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::PinReplyResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  explicit PinReplyResultHandler(BluetoothReplyRunnable* aRunnable)
      : mRunnable(aRunnable) {}

  void PinReply() override { DispatchReplySuccess(mRunnable); }

  void OnError(BluetoothStatus aStatus) override {
    DispatchReplyError(mRunnable, aStatus);
  }

 private:
  RefPtr<BluetoothReplyRunnable> mRunnable;
};

class BluetoothServiceBluedroid::CancelBondResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  explicit CancelBondResultHandler(BluetoothReplyRunnable* aRunnable)
      : mRunnable(aRunnable) {}

  void CancelBond() override { DispatchReplySuccess(mRunnable); }

  void OnError(BluetoothStatus aStatus) override {
    DispatchReplyError(mRunnable, aStatus);
  }

 private:
  RefPtr<BluetoothReplyRunnable> mRunnable;
};

void BluetoothServiceBluedroid::PinReplyInternal(
    const BluetoothAddress& aDeviceAddress, bool aAccept,
    const BluetoothPinCode& aPinCode, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  if (aAccept && aPinCode.mLength) {
    sBtCoreInterface->PinReply(aDeviceAddress, aAccept, aPinCode,
                               new PinReplyResultHandler(aRunnable));
  } else {
    // Call CancelBond to trigger BondStateChangedNotification
    sBtCoreInterface->CancelBond(aDeviceAddress,
                                 new CancelBondResultHandler(aRunnable));
  }
}

class BluetoothServiceBluedroid::SspReplyResultHandler final
    : public BluetoothCoreResultHandler {
 public:
  explicit SspReplyResultHandler(BluetoothReplyRunnable* aRunnable)
      : mRunnable(aRunnable) {}

  void SspReply() override { DispatchReplySuccess(mRunnable); }

  void OnError(BluetoothStatus aStatus) override {
    DispatchReplyError(mRunnable, aStatus);
  }

 private:
  RefPtr<BluetoothReplyRunnable> mRunnable;
};

void BluetoothServiceBluedroid::SspReplyInternal(
    const BluetoothAddress& aDeviceAddress, BluetoothSspVariant aVariant,
    bool aAccept, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  if (aAccept) {
    sBtCoreInterface->SspReply(aDeviceAddress, aVariant, aAccept,
                               0 /* passkey */,
                               new SspReplyResultHandler(aRunnable));
  } else {
    // Call CancelBond to trigger BondStateChangedNotification
    sBtCoreInterface->CancelBond(aDeviceAddress,
                                 new CancelBondResultHandler(aRunnable));
  }
}

void BluetoothServiceBluedroid::NextBluetoothProfileController() {
  MOZ_ASSERT(NS_IsMainThread());

  // Remove the completed task at the head
  NS_ENSURE_FALSE_VOID(sControllerArray.IsEmpty());
  sControllerArray.RemoveElementAt(0);

  // Start the next task if task array is not empty
  if (!sControllerArray.IsEmpty()) {
    sControllerArray[0]->StartSession();
  }
}

void BluetoothServiceBluedroid::ConnectDisconnect(
    bool aConnect, const BluetoothAddress& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable, uint16_t aServiceUuid, uint32_t aCod) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  // If the previos connection/disconnection request targets on the same device
  // and profile, we just ignore this request.
  // Apps should not send a duplicate or conflicted request within a short
  // period of time. For example, two Connect() in row doesn't make sense.
  RefPtr<BluetoothProfileController> prev =
      sControllerArray.SafeLastElement(nullptr);
  if (prev && prev->GetAddress() == aDeviceAddress &&
      prev->GetServiceUuid() == aServiceUuid) {
    BT_WARNING("Ignore this request since it conflicts with the previous one.");
    return;
  }

  BluetoothProfileController* controller = new BluetoothProfileController(
      aConnect, aDeviceAddress, aRunnable, NextBluetoothProfileController,
      aServiceUuid, aCod);
  sControllerArray.AppendElement(controller);

  /**
   * If the request is the first element of the queue, start from here. Note
   * that other requests are pushed into the queue and popped out after the
   * first one is completed. See NextBluetoothProfileController() for details.
   */
  if (sControllerArray.Length() == 1) {
    sControllerArray[0]->StartSession();
  }
}

void BluetoothServiceBluedroid::Connect(const BluetoothAddress& aDeviceAddress,
                                        uint32_t aCod, uint16_t aServiceUuid,
                                        BluetoothReplyRunnable* aRunnable) {
  BT_LOGR("");
  ConnectDisconnect(true, aDeviceAddress, aRunnable, aServiceUuid, aCod);
}

void BluetoothServiceBluedroid::Disconnect(
    const BluetoothAddress& aDeviceAddress, uint16_t aServiceUuid,
    BluetoothReplyRunnable* aRunnable) {
  BT_LOGR("");
  ConnectDisconnect(false, aDeviceAddress, aRunnable, aServiceUuid);
}

void BluetoothServiceBluedroid::AcceptConnection(
    const uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  BluetoothProfileManagerBase* profile =
      BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    BT_WARNING("Can't find profile manager with uuid: %x", aServiceUuid);
    DispatchReplyError(aRunnable, u"Failed to get profile manager"_ns);
    return;
  }

  if (profile->ReplyToConnectionRequest(true)) {
    DispatchReplySuccess(aRunnable);
  } else {
    DispatchReplyError(aRunnable, u"Calling AcceptConnection() failed"_ns);
  }

  return;
}

void BluetoothServiceBluedroid::RejectConnection(
    const uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  BluetoothProfileManagerBase* profile =
      BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    BT_WARNING("Can't find profile manager with uuid: %x", aServiceUuid);
    DispatchReplyError(aRunnable, u"Failed to get profile manager"_ns);
    return;
  }

  if (profile->ReplyToConnectionRequest(false)) {
    DispatchReplySuccess(aRunnable);
  } else {
    DispatchReplyError(aRunnable, u"Calling RejectConnection() failed"_ns);
  }

  return;
}

void BluetoothServiceBluedroid::SendFile(const BluetoothAddress& aDeviceAddress,
                                         BlobImpl* aBlob,
                                         BluetoothReplyRunnable* aRunnable) {
  BT_LOGR("");
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  if (!opp || !opp->SendFile(aDeviceAddress, aBlob)) {
    DispatchReplyError(aRunnable, u"SendFile failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::StopSendingFile(
    const BluetoothAddress& aDeviceAddress, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->StopSendingFile()) {
    DispatchReplyError(aRunnable, u"StopSendingFile failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ConfirmReceivingFile(
    const BluetoothAddress& aDeviceAddress, bool aConfirm,
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("confirm: %d", aConfirm);

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->ConfirmReceivingFile(aConfirm)) {
    DispatchReplyError(aRunnable, u"ConfirmReceivingFile failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ConnectSco(BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("");

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->ConnectSco()) {
    DispatchReplyError(aRunnable, u"ConnectSco failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::DisconnectSco(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("");

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->DisconnectSco()) {
    DispatchReplyError(aRunnable, u"DisconnectSco failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::IsScoConnected(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp) {
    DispatchReplyError(aRunnable, u"IsScoConnected failed"_ns);
    return;
  }

  DispatchReplySuccess(aRunnable, BluetoothValue(hfp->IsScoConnected()));
}

void BluetoothServiceBluedroid::SetObexPassword(
    const nsAString& aPassword, BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable, u"Failed to set OBEX password"_ns);
    return;
  }

  pbap->ReplyToAuthChallenge(aPassword);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::RejectObexAuth(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_ENABLED_VOID(aRunnable);

  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       u"Failed to reject OBEX authentication request"_ns);
    return;
  }

  pbap->ReplyToAuthChallenge(EmptyString());
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyTovCardPulling(
    BlobImpl* aBlob, BluetoothReplyRunnable* aRunnable) {
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable, u"Reply to vCardPulling failed"_ns);
    return;
  }

  pbap->ReplyToPullvCardEntry(aBlob);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToPhonebookPulling(
    BlobImpl* aBlob, uint16_t aPhonebookSize,
    BluetoothReplyRunnable* aRunnable) {
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable, u"Reply to Phonebook Pulling failed"_ns);
    return;
  }

  pbap->ReplyToPullPhonebook(aBlob, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyTovCardListing(
    BlobImpl* aBlob, uint16_t aPhonebookSize,
    BluetoothReplyRunnable* aRunnable) {
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable, u"Reply to vCard Listing failed"_ns);
    return;
  }

  pbap->ReplyToPullvCardListing(aBlob, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToMapFolderListing(
    uint8_t aMasId, const nsAString& aFolderlists,
    BluetoothReplyRunnable* aRunnable) {
  // TODO: Implement for future Email support
}

void BluetoothServiceBluedroid::ReplyToMapMessagesListing(
    uint8_t aMasId, BlobImpl* aBlob, bool aNewMessage,
    const nsAString& aTimestamp, int aSize, BluetoothReplyRunnable* aRunnable) {
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"Reply to Messages Listing failed"_ns);
    return;
  }

  map->ReplyToMessagesListing(aMasId, aBlob, aNewMessage, aTimestamp, aSize);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToMapGetMessage(
    uint8_t aMasId, BlobImpl* aBlob, BluetoothReplyRunnable* aRunnable) {
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"Reply to Get Message failed"_ns);
    return;
  }

  map->ReplyToGetMessage(aMasId, aBlob);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToMapSetMessageStatus(
    uint8_t aMasId, bool aStatus, BluetoothReplyRunnable* aRunnable) {
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"Reply to Set Message failed"_ns);
    return;
  }

  map->ReplyToSetMessageStatus(aMasId, aStatus);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToMapSendMessage(
    uint8_t aMasId, const nsAString& aHandleId, bool aStatus,
    BluetoothReplyRunnable* aRunnable) {
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"Reply to Send Message failed"_ns);
    return;
  }

  map->ReplyToSendMessage(aMasId, aHandleId, aStatus);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::ReplyToMapMessageUpdate(
    uint8_t aMasId, bool aStatus, BluetoothReplyRunnable* aRunnable) {
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"Reply to MessageUpdate failed"_ns);
    return;
  }

  map->ReplyToMessageUpdate(aMasId, aStatus);
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::SendMetaData(
    const nsAString& aTitle, const nsAString& aArtist, const nsAString& aAlbum,
    int64_t aMediaNumber, int64_t aTotalMediaCount, int64_t aDuration,
    BluetoothReplyRunnable* aRunnable) {
  BT_LOGR("title: %s, duration: %lld", NS_ConvertUTF16toUTF8(aTitle).get(),
          aDuration);
  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  if (avrcp) {
    avrcp->UpdateMetaData(aTitle, aArtist, aAlbum, aMediaNumber,
                          aTotalMediaCount, aDuration);
  }
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::SendPlayStatus(
    int64_t aDuration, int64_t aPosition, ControlPlayStatus aPlayStatus,
    BluetoothReplyRunnable* aRunnable) {
  BT_LOGD("duration: %lld, position: %lld", aDuration, aPosition);
  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  if (avrcp) {
    avrcp->UpdatePlayStatus(aDuration, aPosition, aPlayStatus);
  }
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::SendMessageEvent(
    uint8_t aMasId, BlobImpl* aBlob, BluetoothReplyRunnable* aRunnable) {
  BT_LOGR("MAS ID: %u", aMasId);
  BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
  if (!map) {
    DispatchReplyError(aRunnable, u"SendMessageEvent failed"_ns);
    return;
  }

  map->SendMessageEvent(aMasId, aBlob);
  DispatchReplySuccess(aRunnable);
}

#ifdef MOZ_B2G_RIL
void BluetoothServiceBluedroid::AnswerWaitingCall(
    BluetoothReplyRunnable* aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp) {
    DispatchReplyError(aRunnable, u"Fail to get BluetoothHfpManager"_ns);
    return;
  }

  hfp->AnswerWaitingCall();
  DispatchReplySuccess(aRunnable);
}

void BluetoothServiceBluedroid::IgnoreWaitingCall(
    BluetoothReplyRunnable* aRunnable) {}

void BluetoothServiceBluedroid::ToggleCalls(BluetoothReplyRunnable* aRunnable) {
}
#endif  // MOZ_B2G_RIL

//
// Bluetooth notifications
//

class BluetoothServiceBluedroid::CleanupResultHandler final
    : public BluetoothResultHandler {
 public:
  void Cleanup() override {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService::AcknowledgeToggleBt(false);
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Cleanup failed: %d", aStatus);

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

/* |ProfileDeinitResultHandler| collects the results of all profile
 * result handlers and cleans up the Bluedroid driver after all handlers
 * have been run.
 */
class BluetoothServiceBluedroid::ProfileDeinitResultHandler final
    : public BluetoothProfileResultHandler {
 public:
  ProfileDeinitResultHandler(unsigned char aNumProfiles, bool aIsRestart)
      : mNumProfiles(aNumProfiles), mIsRestart(aIsRestart) {
    MOZ_ASSERT(mNumProfiles);
  }

  void Deinit() override {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

  void OnError(nsresult aResult) override {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

 private:
  void Proceed() const {
    if (mIsRestart) {
      BT_LOGR("ProfileDeinitResultHandler::Proceed cancel cleanup() ");
      return;
    }

    sBtCoreInterface = nullptr;
    sBtInterface->Cleanup(new CleanupResultHandler());
  }

  unsigned char mNumProfiles;
  bool mIsRestart;
};

class BluetoothServiceBluedroid::SetAdapterPropertyDiscoverableResultHandler
    final : public BluetoothCoreResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_LOGR("Fail to set: BT_SCAN_MODE_CONNECTABLE");
  }
};

void BluetoothServiceBluedroid::AdapterStateChangedNotification(bool aState) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("BT_STATE: %d", aState);

  if (mIsRestart && aState) {
    // daemon restarted, reset flag
    BT_LOGR("daemon restarted, reset flag");
    mIsRestart = false;
    mIsFirstTimeToggleOffBt = false;
  }

  mEnabled = aState;

  if (!mEnabled) {
    static void (*const sDeinitManager[])(BluetoothProfileResultHandler*) = {
        // Cleanup interfaces in opposite order to initialization.
        BluetoothGattManager::DeinitGattInterface,
        BluetoothAvrcpManager::DeinitAvrcpInterface,
        BluetoothA2dpManager::DeinitA2dpInterface,
        BluetoothHfpManager::DeinitHfpInterface,
        BluetoothHidManager::DeinitHidInterface,
        BluetoothPbapManager::DeinitPbapInterface,
        BluetoothOppManager::DeinitOppInterface,
        BluetoothMapSmsManager::DeinitMapSmsInterface,
    };

    // Return error if BluetoothService is unavailable
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Cleanup static adapter properties and notify adapter.
    mBdAddress.Clear();
    mBdName.Clear();

    nsTArray<BluetoothNamedValue> props;
    AppendNamedValue(props, "Name", mBdName);
    AppendNamedValue(props, "Address", mBdAddress);
    if (mDiscoverable) {
      mDiscoverable = false;
      AppendNamedValue(props, "Discoverable", false);
    }
    if (mDiscovering) {
      mDiscovering = false;
      AppendNamedValue(props, "Discovering", false);
    }

    bs->DistributeSignal(u"PropertyChanged"_ns, KEY_ADAPTER,
                         BluetoothValue(props));

    // Cleanup Bluetooth interfaces after state becomes BT_STATE_OFF. This
    // will also stop the Bluetooth daemon and disable the adapter.
    RefPtr<ProfileDeinitResultHandler> res = new ProfileDeinitResultHandler(
        MOZ_ARRAY_LENGTH(sDeinitManager), mIsRestart);

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sDeinitManager); ++i) {
      sDeinitManager[i](res);
    }

    // Bluetooth just disabled, clear profile constrollers.
    for (uint32_t i = 0; i < sControllerArray.Length(); ++i) {
      sControllerArray[i]->EndSession();
    }
    sControllerArray.Clear();
  }

  if (mEnabled) {
    // We enable the Bluetooth adapter here. Disabling is implemented
    // in |CleanupResultHandler|, which runs at the end of the shutdown
    // procedure. We cannot disable the adapter immediately, because re-
    // enabling it might interfere with the shutdown procedure.
    BluetoothService::AcknowledgeToggleBt(true);

    // Bluetooth just enabled, clear runnable arrays.
    mGetDeviceRequests.Clear();
    mChangeDiscoveryRunnables.Clear();
    mSetAdapterPropertyRunnables.Clear();
    mFetchUuidsRunnables.Clear();
    mCreateBondRunnables.Clear();
    mRemoveBondRunnables.Clear();
    mDeviceNameMap.Clear();
    mDeviceCodMap.Clear();

    // Bluetooth scan mode is SCAN_MODE_CONNECTABLE by default, i.e., it should
    // be connectable and non-discoverable.
    sBtCoreInterface->SetAdapterProperty(
        BluetoothProperty(PROPERTY_ADAPTER_SCAN_MODE, SCAN_MODE_CONNECTABLE),
        new SetAdapterPropertyDiscoverableResultHandler());

    // Trigger OPP & PBAP managers to listen
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_LOGR("Fail to start BluetoothOppManager listening");
    }

    BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
    if (!pbap || !pbap->Listen()) {
      BT_LOGR("Fail to start BluetoothPbapManager listening");
    }

    BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
    if (!map || !map->Listen()) {
      BT_LOGR("Fail to start BluetoothMapSmsManager listening");
    }
  }

  // Resolve promise if existed
  if (!mChangeAdapterStateRunnables.IsEmpty()) {
    DispatchReplySuccess(mChangeAdapterStateRunnables[0]);
    mChangeAdapterStateRunnables.RemoveElementAt(0);
  }

  // After ProfileManagers deinit and cleanup, now restart bluetooth daemon
  if (mIsRestart && !aState) {
    BT_LOGR("mIsRestart and off, now restart");
    StartBluetooth(false, nullptr);
  }
}

/**
 * AdapterPropertiesNotification will be called after enable() but before
 * AdapterStateChangeCallback is called. At that moment, both BluetoothManager
 * and BluetoothAdapter have not registered observer yet.
 */
void BluetoothServiceBluedroid::AdapterPropertiesNotification(
    BluetoothStatus aStatus, int aNumProperties,
    const BluetoothProperty* aProperties) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {
    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      mBdAddress = p.mBdAddress;
      AppendNamedValue(propertiesArray, "Address", mBdAddress);

    } else if (p.mType == PROPERTY_BDNAME) {
      mBdName = p.mRemoteName;
      AppendNamedValue(propertiesArray, "Name", mBdName);

    } else if (p.mType == PROPERTY_ADAPTER_SCAN_MODE) {
      // If BT is not enabled, Bluetooth scan mode should be non-discoverable
      // by defalut. |AdapterStateChangedNotification| would set default
      // properties to bluetooth backend once Bluetooth is enabled.
      if (IsEnabled()) {
        mDiscoverable = (p.mScanMode == SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        AppendNamedValue(propertiesArray, "Discoverable", mDiscoverable);
      }
    } else if (p.mType == PROPERTY_ADAPTER_BONDED_DEVICES) {
      // We have to cache addresses of bonded devices. Unlike BlueZ,
      // Bluedroid would not send another PROPERTY_ADAPTER_BONDED_DEVICES
      // event after bond completed.
      BT_LOGD("Adapter property: BONDED_DEVICES. Count: %d",
              p.mBdAddressArray.Length());

      // Whenever reloading paired devices, force refresh
      mBondedAddresses.Clear();
      mBondedAddresses.AppendElements(p.mBdAddressArray);

      AppendNamedValue(propertiesArray, "PairedDevices", mBondedAddresses);
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Unhandled adapter property type: %d", p.mType);
      continue;
    }
  }

  NS_ENSURE_TRUE_VOID(propertiesArray.Length() > 0);

  DistributeSignal(u"PropertyChanged"_ns, KEY_ADAPTER,
                   BluetoothValue(propertiesArray));

  // Send reply for SetProperty
  if (!mSetAdapterPropertyRunnables.IsEmpty()) {
    DispatchReplySuccess(mSetAdapterPropertyRunnables[0]);
    mSetAdapterPropertyRunnables.RemoveElementAt(0);
  }
}

/**
 * RemoteDevicePropertiesNotification will be called
 *
 *   (1) automatically by Bluedroid when BT is turning on, or
 *   (2) as result of remote device properties update during discovery, or
 *   (3) as result of CreateBond, or
 *   (4) as result of GetRemoteDeviceProperties, or
 *   (5) as result of GetRemoteServices.
 */
void BluetoothServiceBluedroid::RemoteDevicePropertiesNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aBdAddr,
    int aNumProperties, const BluetoothProperty* aProperties) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<BluetoothNamedValue> propertiesArray;

  AppendNamedValue(propertiesArray, "Address", aBdAddr);

  for (int i = 0; i < aNumProperties; ++i) {
    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDNAME) {
      AppendNamedValue(propertiesArray, "Name", p.mRemoteName);

      // Update <address, name> mapping
      mDeviceNameMap.Remove(aBdAddr);
      mDeviceNameMap.InsertOrUpdate(aBdAddr, p.mRemoteName);
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = p.mUint32;
      AppendNamedValue(propertiesArray, "Cod", cod);

    } else if (p.mType == PROPERTY_UUIDS) {
      size_t index;

      // Handler for |UpdateSdpRecords|

      for (index = 0; index < mGetRemoteServicesArray.Length(); ++index) {
        if (mGetRemoteServicesArray[index].mDeviceAddress == aBdAddr) {
          break;
        }
      }

      if (index < mGetRemoteServicesArray.Length()) {
        mGetRemoteServicesArray[index].mManager->OnUpdateSdpRecords(aBdAddr);
        mGetRemoteServicesArray.RemoveElementAt(index);
        continue;  // continue with outer loop
      }

      // Handler for |FetchUuidsInternal|
      AppendNamedValue(propertiesArray, "UUIDs", p.mUuidArray);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Type",
                       static_cast<uint32_t>(p.mTypeOfDevice));

    } else if (p.mType == PROPERTY_SERVICE_RECORD) {
      StaticMutexAutoLock lock(sServiceRecordLock);
      size_t i;

      // Find call in array

      for (i = 0; i < mGetRemoteServiceRecordArray.Length(); ++i) {
        if ((mGetRemoteServiceRecordArray[i].mDeviceAddress == aBdAddr) &&
            (mGetRemoteServiceRecordArray[i].mUuid == p.mServiceRecord.mUuid)) {
          // Signal channel to profile manager
          mGetRemoteServiceRecordArray[i].mManager->OnGetServiceChannel(
              aBdAddr, mGetRemoteServiceRecordArray[i].mUuid,
              p.mServiceRecord.mChannel);

          mGetRemoteServiceRecordArray.RemoveElementAt(i);
          break;
        }
      }
      Unused << NS_WARN_IF(i == mGetRemoteServiceRecordArray.Length());
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Other non-handled device properties. Type: %d", p.mType);
    }
  }

#ifdef MOZ_B2G_RIL
  // Append battery level if it's a HFP connected device
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (hfp) {
    BluetoothAddress deviceAddress;
    hfp->GetAddress(deviceAddress);
    if (deviceAddress == aBdAddr) {
      int batteryLevel = hfp->GetDeviceBatteryLevel();
      AppendNamedValue(propertiesArray, "BatteryLevel", batteryLevel);
    }
  }
#endif  // MOZ_B2G_RIL

  // The order of operations below is
  //
  //  (1) modify global state (i.e., the variables starting with 's'),
  //  (2) distribute the signal, and finally
  //  (3) send any pending Bluetooth replies.
  //
  // |DispatchReplySuccess| creates its own internal runnable, which is
  // always run after we completed the current method. This means that we
  // can exchange |DispatchReplySuccess| with other operations without
  // changing the order of (1,2) and (3).

  // Update to registered BluetoothDevice objects
  nsAutoString bdAddrStr;
  AddressToString(aBdAddr, bdAddrStr);
  BluetoothSignal signal(u"PropertyChanged"_ns, bdAddrStr, propertiesArray);

  // FetchUuids task
  if (!mFetchUuidsRunnables.IsEmpty()) {
    // propertiesArray contains Address and Uuids only
    DispatchReplySuccess(mFetchUuidsRunnables[0],
                         propertiesArray[1].value()); /* Uuids */
    mFetchUuidsRunnables.RemoveElementAt(0);
    DistributeSignal(signal);
    return;
  }

  // GetDevices task
  if (mGetDeviceRequests.IsEmpty()) {
    // Callback is called after Bluetooth is turned on
    DistributeSignal(signal);
    return;
  }

  // Use address as the index
  mGetDeviceRequests[0].mDevicesPack.AppendElement(
      BluetoothNamedValue(bdAddrStr, propertiesArray));

  if (--mGetDeviceRequests[0].mDeviceCount == 0) {
    if (mGetDeviceRequests[0].mRunnable) {
      DispatchReplySuccess(mGetDeviceRequests[0].mRunnable,
                           mGetDeviceRequests[0].mDevicesPack);
    }
    mGetDeviceRequests.RemoveElementAt(0);
  }

  DistributeSignal(signal);
}

void BluetoothServiceBluedroid::DeviceFoundNotification(
    int aNumProperties, const BluetoothProperty* aProperties) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<BluetoothNamedValue> propertiesArray;

  BluetoothAddress bdAddr;
  BluetoothRemoteName bdName;

  for (int i = 0; i < aNumProperties; i++) {
    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      AppendNamedValue(propertiesArray, "Address", p.mBdAddress);
      bdAddr = p.mBdAddress;
    } else if (p.mType == PROPERTY_BDNAME) {
      AppendNamedValue(propertiesArray, "Name", p.mRemoteName);
      bdName = p.mRemoteName;
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Cod", p.mUint32);

    } else if (p.mType == PROPERTY_UUIDS) {
      AppendNamedValue(propertiesArray, "UUIDs", p.mUuidArray);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Type",
                       static_cast<uint32_t>(p.mTypeOfDevice));

    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Not handled remote device property: %d", p.mType);
    }
  }

  // Update <address, name> mapping
  mDeviceNameMap.Remove(bdAddr);
  mDeviceNameMap.InsertOrUpdate(bdAddr, bdName);

  DistributeSignal(u"DeviceFound"_ns, KEY_ADAPTER,
                   BluetoothValue(propertiesArray));
}

void BluetoothServiceBluedroid::DiscoveryStateChangedNotification(bool aState) {
  MOZ_ASSERT(NS_IsMainThread());

  mDiscovering = aState;

  // Fire PropertyChanged of Discovering
  nsTArray<BluetoothNamedValue> propertiesArray;
  AppendNamedValue(propertiesArray, "Discovering", mDiscovering);

  DistributeSignal(u"PropertyChanged"_ns, KEY_ADAPTER,
                   BluetoothValue(propertiesArray));

  // Resolve all the Promise objects when state changed.
  // The runnables may be added by either starDiscovery or stopDiscovery,
  // however, BluetoothAdapter wouldn't queue different types at the same.
  // Therefore, all the Promise should be replied here.
  for (size_t i = 0; i < mChangeDiscoveryRunnables.Length(); ++i) {
    DispatchReplySuccess(mChangeDiscoveryRunnables[i]);
  }
  mChangeDiscoveryRunnables.Clear();
}

void BluetoothServiceBluedroid::PinRequestNotification(
    const BluetoothAddress& aRemoteBdAddr, const BluetoothRemoteName& aBdName,
    uint32_t aCod) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("addr: %02x:%02x:%02x:%02x:%02x:%02x", aRemoteBdAddr.mAddr[0],
          aRemoteBdAddr.mAddr[1], aRemoteBdAddr.mAddr[2],
          aRemoteBdAddr.mAddr[3], aRemoteBdAddr.mAddr[4],
          aRemoteBdAddr.mAddr[5]);

  BluetoothRemoteName bdName;
  nsTArray<BluetoothNamedValue> propertiesArray;

  // If |aBdName| is empty, get device name from |mDeviceNameMap|;
  // Otherwise update <address, name> mapping with |aBdName|
  if (aBdName.IsCleared()) {
    if (!mDeviceNameMap.Get(aRemoteBdAddr, &bdName)) {
      BT_LOGD("BD name is unknown");
    }
  } else {
    bdName.Assign(aBdName.mName, aBdName.mLength);
    mDeviceNameMap.Remove(aRemoteBdAddr);
    mDeviceNameMap.InsertOrUpdate(aRemoteBdAddr, bdName);
  }

  // Update <address, cod> mapping
  mDeviceCodMap.Remove(aRemoteBdAddr);
  mDeviceCodMap.InsertOrUpdate(aRemoteBdAddr, aCod);

  AppendNamedValue(propertiesArray, "address", aRemoteBdAddr);
  AppendNamedValue(propertiesArray, "name", bdName);
  AppendNamedValue(propertiesArray, "passkey", EmptyString());
  AppendNamedValue(propertiesArray, "type",
                   static_cast<const mozilla::dom::bluetooth::BluetoothValue>(
                       PAIRING_REQ_TYPE_ENTERPINCODE));

  DistributeSignal(u"PairingRequest"_ns, KEY_PAIRING_LISTENER,
                   BluetoothValue(propertiesArray));
}

void BluetoothServiceBluedroid::SspRequestNotification(
    const BluetoothAddress& aRemoteBdAddr, const BluetoothRemoteName& aBdName,
    uint32_t aCod, BluetoothSspVariant aPairingVariant, uint32_t aPassKey) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("variant: %d, addr: %02x:%02x:%02x:%02x:%02x:%02x",
          static_cast<int>(aPairingVariant), aRemoteBdAddr.mAddr[0],
          aRemoteBdAddr.mAddr[1], aRemoteBdAddr.mAddr[2],
          aRemoteBdAddr.mAddr[3], aRemoteBdAddr.mAddr[4],
          aRemoteBdAddr.mAddr[5]);

  BluetoothRemoteName bdName;
  nsTArray<BluetoothNamedValue> propertiesArray;

  // If |aBdName| is empty, get device name from |mDeviceNameMap|;
  // Otherwise update <address, name> mapping with |aBdName|
  if (aBdName.IsCleared()) {
    if (!mDeviceNameMap.Get(aRemoteBdAddr, &bdName)) {
      BT_LOGD("BD name is unknown");
    }
  } else {
    bdName.Assign(aBdName.mName, aBdName.mLength);
    mDeviceNameMap.Remove(aRemoteBdAddr);
    mDeviceNameMap.InsertOrUpdate(aRemoteBdAddr, bdName);
  }

  // Update <address, cod> mapping
  mDeviceCodMap.Remove(aRemoteBdAddr);
  mDeviceCodMap.InsertOrUpdate(aRemoteBdAddr, aCod);

  /**
   * Assign pairing request type and passkey based on the pairing variant.
   *
   * passkey value based on pairing request type:
   * 1) aPasskey: PAIRING_REQ_TYPE_CONFIRMATION and
   *              PAIRING_REQ_TYPE_DISPLAYPASSKEY
   * 2) empty string: PAIRING_REQ_TYPE_CONSENT
   */
  nsAutoString passkey;
  nsAutoString pairingType;
  switch (aPairingVariant) {
    case SSP_VARIANT_PASSKEY_CONFIRMATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONFIRMATION);
      passkey.AppendInt(aPassKey);
      break;
    case SSP_VARIANT_PASSKEY_NOTIFICATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_DISPLAYPASSKEY);
      passkey.AppendInt(aPassKey);
      break;
    case SSP_VARIANT_CONSENT:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONSENT);
      break;
    default:
      BT_WARNING("Unhandled SSP Bonding Variant: %d", aPairingVariant);
      return;
  }

  AppendNamedValue(propertiesArray, "address", aRemoteBdAddr);
  AppendNamedValue(propertiesArray, "name", bdName);
  AppendNamedValue(propertiesArray, "passkey", passkey);
  AppendNamedValue(propertiesArray, "type", pairingType);

  DistributeSignal(u"PairingRequest"_ns, KEY_PAIRING_LISTENER,
                   BluetoothValue(propertiesArray));
}

void BluetoothServiceBluedroid::BondStateChangedNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aRemoteBdAddr,
    BluetoothBondState aState) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aState == BOND_STATE_BONDING) {
    // No need to handle bonding state
    return;
  }

  BT_LOGR("Bond state: %d status: %d", aState, aStatus);

  bool bonded = (aState == BOND_STATE_BONDED);
  if (aStatus != STATUS_SUCCESS) {
    if (!bonded) {  // Active/passive pair failed

      // Abort pairing when the authentication has stopped
      if (aStatus == STATUS_AUTH_FAILURE || aStatus == STATUS_RMT_DEV_DOWN ||
          aStatus == STATUS_AUTH_REJECTED) {
        BT_LOGR("Pair failed! Abort pairing.");
        // Notify adapter of pairing aborted
        nsAutoString deviceAddressStr;
        AddressToString(aRemoteBdAddr, deviceAddressStr);
        DistributeSignal(
            BluetoothSignal(PAIRING_ABORTED_ID, KEY_ADAPTER, deviceAddressStr));

        // Query pairing device name from hash table
        BluetoothRemoteName remotebdName;
        if (!mDeviceNameMap.Get(aRemoteBdAddr, &remotebdName)) {
          BT_LOGD("BD name is unknown");
        }
        // Since mName is not 0-terminated, mLength is required here.
        NS_ConvertUTF8toUTF16 bdName(
            reinterpret_cast<char*>(remotebdName.mName), remotebdName.mLength);

        BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(SYS_MSG_BT_PAIRING_ABORTED,
                                             BluetoothValue(bdName));
      }

      // Reject pair promise
      if (!mCreateBondRunnables.IsEmpty()) {
        DispatchReplyError(mCreateBondRunnables[0], aStatus);
        mCreateBondRunnables.RemoveElementAt(0);
      }
    } else if (!mRemoveBondRunnables.IsEmpty()) {  // Active unpair failed
      // Reject unpair promise
      DispatchReplyError(mRemoveBondRunnables[0], aStatus);
      mRemoveBondRunnables.RemoveElementAt(0);
    }

    return;
  }

  // Update bonded address array and append pairing device name
  nsTArray<BluetoothNamedValue> propertiesArray;
  if (!bonded) {
    mBondedAddresses.RemoveElement(aRemoteBdAddr);
  } else {
    if (!mBondedAddresses.Contains(aRemoteBdAddr)) {
      mBondedAddresses.AppendElement(aRemoteBdAddr);
    }

    // Query pairing device name from hash table
    BluetoothRemoteName remotebdName;
    if (!mDeviceNameMap.Get(aRemoteBdAddr, &remotebdName)) {
      BT_LOGD("BD name is unknown");
    }

    // We don't assert |!remotebdName.IsEmpty()| since empty string is also
    // valid, according to Bluetooth Core Spec. v3.0 - Sec. 6.22:
    // "a valid Bluetooth name is a UTF-8 encoding string which is up to 248
    // bytes in length."
    AppendNamedValue(propertiesArray, "Name", remotebdName);

    // Use the cached CoD which is got from pairing request
    uint32_t remoteCod = 0;
    if (!mDeviceCodMap.Get(aRemoteBdAddr, &remoteCod)) {
      AppendNamedValue(propertiesArray, "Cod", remoteCod);
    }
  }

  // Notify device of attribute changed
  nsAutoString remoteBdAddr;
  AddressToString(aRemoteBdAddr, remoteBdAddr);
  AppendNamedValue(propertiesArray, "Paired", bonded);
  DistributeSignal(u"PropertyChanged"_ns, remoteBdAddr,
                   BluetoothValue(propertiesArray));

  // Notify adapter of device paired/unpaired
  InsertNamedValue(propertiesArray, 0, "Address", aRemoteBdAddr);
  DistributeSignal(bonded ? DEVICE_PAIRED_ID : DEVICE_UNPAIRED_ID, KEY_ADAPTER,
                   BluetoothValue(propertiesArray));

  // Resolve existing pair/unpair promise
  if (bonded && !mCreateBondRunnables.IsEmpty()) {
    DispatchReplySuccess(mCreateBondRunnables[0]);
    mCreateBondRunnables.RemoveElementAt(0);
  } else if (!bonded && !mRemoveBondRunnables.IsEmpty()) {
    DispatchReplySuccess(mRemoveBondRunnables[0]);
    mRemoveBondRunnables.RemoveElementAt(0);
  }
}

void BluetoothServiceBluedroid::AclStateChangedNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aRemoteBdAddr,
    BluetoothAclState aState) {
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void BluetoothServiceBluedroid::DutModeRecvNotification(uint16_t aOpcode,
                                                        const uint8_t* aBuf,
                                                        uint8_t aLen) {
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void BluetoothServiceBluedroid::LeTestModeNotification(BluetoothStatus aStatus,
                                                       uint16_t aNumPackets) {
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void BluetoothServiceBluedroid::EnergyInfoNotification(
    const BluetoothActivityEnergyInfo& aInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void BluetoothServiceBluedroid::BackendErrorNotification(bool aCrashed) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCrashed) {
    return;
  }

  /*
   * Reset following profile manager states for unexpected backend crash.
   * - HFP: connection state and audio state
   * - A2DP: connection state
   * - HID: connection state
   */
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE_VOID(hfp);
  hfp->HandleBackendError();
  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  a2dp->HandleBackendError();
  BluetoothHidManager* hid = BluetoothHidManager::Get();
  NS_ENSURE_TRUE_VOID(hid);
  hid->HandleBackendError();

  mIsRestart = true;
  BT_LOGR("Recovery step2: stop bluetooth");
  StopBluetooth(false, nullptr);
}

void BluetoothServiceBluedroid::CompleteToggleBt(bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mIsRestart && !aEnabled && mIsFirstTimeToggleOffBt) {
    // Both StopBluetooth and AdapterStateChangedNotification
    // trigger CompleteToggleBt. We don't need to call CompleteToggleBt again
  } else if (mIsRestart && !aEnabled && !mIsFirstTimeToggleOffBt) {
    // Recovery step 3: cleanup and deinit Profile managers
    BT_LOGR("CompleteToggleBt set mIsFirstTimeToggleOffBt = true");
    mIsFirstTimeToggleOffBt = true;
    BluetoothService::CompleteToggleBt(aEnabled);
    AdapterStateChangedNotification(false);
  } else {
    BluetoothService::CompleteToggleBt(aEnabled);
  }
}
