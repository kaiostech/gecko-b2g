/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSdpManager.h"
#include "base/basictypes.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuidHelper.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/File.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCExternalHandlerService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsIOutputStream.h"
#include "nsIThread.h"
#include "nsIVolumeService.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

#include "BluetoothCommon.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
StaticRefPtr<BluetoothSdpManager> sBtSdpManager;
static BluetoothSdpInterface* sBtSdpInterface = nullptr;

std::map<int, CreateSdpRecordResultHandle> sSdpResultHandleMap;

class SdpSearchRecord final {
 public:
  SdpSearchRecord(BluetoothAddress aAddress,
                  BluetoothUuid aUuid,
                  SdpSearchResultHandle aHandle) :
                  mAddress(aAddress),  mUuid(aUuid), mHandle(aHandle) {}

  BluetoothAddress      mAddress;
  BluetoothUuid         mUuid;
  SdpSearchResultHandle mHandle;
};

std::vector<SdpSearchRecord> sSdpSearchRecordDb;
}  // namespace

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSdpManager::InitProfileResultHandlerRunnable final
    : public Runnable {
 public:
  InitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                   nsresult aRv)
      : Runnable("InitProfileResultHandlerRunnable"), mRes(aRes), mRv(aRv) {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("InitProfileResultHandlerRunnable::Run()");

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

class BluetoothSdpManager::DeinitProfileResultHandlerRunnable final
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

class BluetoothSdpManager::RegisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  RegisterModuleResultHandler(BluetoothSdpInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
      : mSdpInterface(aInterface), mRes(aRes) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for SDP: %d",
               (int)aStatus);

    mSdpInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("SDP register complete");
    sBtSdpInterface = mSdpInterface;

    if (mRes) {
      mRes->Init();
    }
  }

 private:
  BluetoothSdpInterface* mSdpInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothSdpManager::UnregisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  explicit UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
      : mRes(aRes) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for SDP: %d",
               (int)aStatus);

    if (sBtSdpInterface) {
      sBtSdpInterface->SetNotificationHandler(nullptr);
      sBtSdpInterface = nullptr;
    }

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (sBtSdpInterface) {
      sBtSdpInterface->SetNotificationHandler(nullptr);
      sBtSdpInterface = nullptr;
    }

    if (mRes) {
      mRes->Deinit();
    }
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

// static
BluetoothSdpManager* BluetoothSdpManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (sBtSdpManager) {
    return sBtSdpManager;
  }

  // Create a new instance, register, and return
  BluetoothSdpManager* manager = new BluetoothSdpManager();

  sBtSdpManager = manager;

  return sBtSdpManager;
}

// static
void BluetoothSdpManager::InitSdpInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("Init");

  if (sBtSdpInterface) {
    BT_LOGR("Bluetooth SDP interface is already initalized.");
    RefPtr<Runnable> r = new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch SDP Init runnable");
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
      BT_LOGR("Failed to dispatch SDP OnError runnable");
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
      BT_LOGR("Failed to dispatch SDP OnError runnable");
    }
    return;
  }

  auto sdpInterface = btInf->GetBluetoothSdpInterface();

  if (NS_WARN_IF(!sdpInterface)) {
    // If there's no SDP interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch SDP OnError runnable");
    }
    return;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  sdpInterface->SetNotificationHandler(BluetoothSdpManager::Get());

  static const int MAX_NUM_CLIENTS = 1;
  setupInterface->RegisterModule(
      SETUP_SERVICE_ID_SDP, 0, MAX_NUM_CLIENTS,
      new RegisterModuleResultHandler(sdpInterface, aRes));
}

// static
void BluetoothSdpManager::DeinitSdpInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBtSdpInterface) {
    BT_LOGR("Bluetooth SDP interface has not been initalized.");
    RefPtr<Runnable> r = new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch MAP Deinit runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no Bluetooth interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch SDP OnError runnable");
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
      BT_LOGR("Failed to dispatch SDP OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(SETUP_SERVICE_ID_SDP,
                                   new UnregisterModuleResultHandler(aRes));
}

// static API
void BluetoothSdpManager::SdpSearch(
    const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aUuid,
    SdpSearchResultHandle aResultHandle) {
  BT_LOGR("SdpSearch");

  sBtSdpInterface->SdpSearch(aDeviceAddress, aUuid,
                             nullptr/* No interest in command response,
                                     we simply process SDP search notifications. */);

  for (auto it : sSdpSearchRecordDb) {
    if (it.mAddress == aDeviceAddress && it.mUuid == aUuid) {
      // already in the search record db, not need to store it again
      BT_LOGR("SdpSearch does not need to store caller again");
      return;
    }
  }

  sSdpSearchRecordDb.push_back(SdpSearchRecord(aDeviceAddress, aUuid, aResultHandle));
}

void BluetoothSdpManager::SdpSearchNotification(int aUuidSize,
                                                const uint8_t* aUuid,
                                                const uint8_t* aDeviceAddres,
                                                int aSdpSize,
                                                const int* aSdpArray) {
  BluetoothSdpType sdpType;
  uint16_t uuid16;
  BluetoothUuid uuid;

  switch (aUuidSize) {
    case 2:
      uuid16 = (static_cast<uint16_t>(aUuid[0]) << 8) + static_cast<uint16_t>(aUuid[1]);
      break;
    case 4:
      [[fallthrough]];
    case 16:
      uuid16 = (static_cast<uint16_t>(aUuid[2]) << 8) + static_cast<uint16_t>(aUuid[3]);
      break;
    default:
      BT_LOGR("UnpackPDU sdp record size failed (%d)", aUuidSize);
      return;
  }

  sdpType = BluetoothUuidHelper::GetBluetoothSdpType(uuid16);

  BluetoothAddress addr(aDeviceAddres[0], aDeviceAddres[1], aDeviceAddres[2],
                        aDeviceAddres[3], aDeviceAddres[4], aDeviceAddres[5]);

  if (aUuidSize == 2) {
    uuid.SetUuid16((static_cast<uint16_t>(aUuid[0]) << 8) +
                    static_cast<uint16_t>(aUuid[1]));
  } else if (aUuidSize == 4) {
    uuid.SetUuid32((static_cast<uint32_t>(aUuid[0]) << 24) +
                   (static_cast<uint32_t>(aUuid[1]) << 16) +
                   (static_cast<uint32_t>(aUuid[2]) << 8) +
                   (static_cast<uint32_t>(aUuid[0])));
  } else if (aUuidSize == 16) {
    memcpy(uuid.mUuid, aUuid, 16);
  } else {
    BT_WARNING("Sdp search result: unsported uuid type");
    return;
  }

  BT_LOGR("Sdp search result:");
  BT_LOGR("    type: %d", sdpType);
  BT_LOGR("    uuid: %04x, size %d", uuid16, aUuidSize);
  BT_LOGR("    address: %02x%02x%02x%02x%02x%02x",
               aDeviceAddres[0], aDeviceAddres[1],
               aDeviceAddres[2], aDeviceAddres[3],
               aDeviceAddres[4], aDeviceAddres[5]);
  BT_LOGR("    sdp size: %d", aSdpSize);

  auto it = sSdpSearchRecordDb.begin();


  // step 1: to find who is interested in this SDP result
  while (it != sSdpSearchRecordDb.end()) {
    if (it->mAddress == addr && it->mUuid == uuid) {
      BT_LOGR("found who is interested in this SDP result");
      break;
    } else {
      ++it;
    }
  }

  // step 2: if we found someone who is interested in this SDP result,
  //   [2.a] we proceed to parse and forward the SDP record to the profile.
  //   [2.b] Otherwise, we should abort the process.

  // [2.b]
  if (it == sSdpSearchRecordDb.end()) {
    return;
  }

  std::vector<RefPtr<BluetoothSdpRecord>> result;

  // [2.a]
  while (aSdpSize) {
    RefPtr<BluetoothSdpRecord> record = nullptr;
    int offset = 0;

    /*
     * +---------+-----------------+
     * | array 0 | rfcomm channel  |
     * | array 1 | l2cap psm       |
     * | array 2 | profile version |
     * +---------+-----------------+
    */
    switch (sdpType) {
      case SDP_TYPE_MAP_MAS: {
        RefPtr<BluetoothMasRecord> mas = new BluetoothMasRecord();
        record = static_cast<BluetoothSdpRecord*>(mas);

        /*
         * +---------+-------------------------+
         * | array 3 | mas instance id         |
         * | array 4 | supported features      |
         * | array 5 | supported message types |
         * +---------+-------------------------+
        */
        mas->mInstanceId = aSdpArray[3];
        mas->mSupportedFeatures = aSdpArray[4];
        mas->mSupportedContentTypes = aSdpArray[5];

        offset = 3 + 3;
        break;
      }
      case SDP_TYPE_MAP_MNS: {
        RefPtr<BluetoothMnsRecord> mns = new BluetoothMnsRecord();
        record = static_cast<BluetoothSdpRecord*>(mns);

        /*
         * +---------+-------------------------+
         * | array 4 | supported features      |
         * +---------+-------------------------+
        */
        mns->mSupportedFeatures = aSdpArray[3];

        offset = 3 + 1;
        break;
      }
      case SDP_TYPE_PBAP_PSE: {
        RefPtr<BLuetoothPbapSdpRecord> pse = new BLuetoothPbapSdpRecord();
        record = static_cast<BluetoothSdpRecord*>(pse);

        /*
         * +---------+-------------------------+
         * | array 3 | supported features      |
         * | array 4 | supported message types |
         * +---------+-------------------------+
        */
        pse->mSupportedFeatures = aSdpArray[3];
        pse->mSupportedRepositories = aSdpArray[4];

        offset = 3 + 2;
        break;
      }
      case SDP_TYPE_PBAP_PCE:
        record = new BluetoothSdpRecord;

        offset = 3;
        break;
      default:
        // todo:
        // add SDP_TYPE_OPP_SERVER/SDP_TYPE_SAP_SERVER support
        BT_WARNING("Unsupported SDP type: %d", sdpType);
        break;
    }

    if (record) {
      record->mType = sdpType;
      record->mRfcommChannelNumber = aSdpArray[0];
      record->mL2capPsm = aSdpArray[1];
      record->mProfileVersion = aSdpArray[2];

      result.push_back(record);
    }

    aSdpArray += offset;

    aSdpSize--;
  }

  it->mHandle(addr, result);
  sSdpSearchRecordDb.erase(it);
}

class BluetoothSdpManager::CreateSdpRecordResultHandler final
    : public BluetoothSdpResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_LOGR("BluetoothSdpManager::CreateSdpRecord failed: %d", (int)aStatus);
  }

  void CreateSdpRecord(int aSdpType, int aRecordHandle) override {
    BT_LOGR("CreateSdpRecordResultHandler: type %d, handle %d", aSdpType, aRecordHandle);

    auto it = sSdpResultHandleMap.find(aSdpType);

    if (it != sSdpResultHandleMap.end()) {
      it->second(aSdpType, aRecordHandle);

      sSdpResultHandleMap.erase(it);
    }
  }
};

// static
void BluetoothSdpManager::CreateSdpRecord(
    const BluetoothSdpRecord& aRecord,
    CreateSdpRecordResultHandle aResultHandle) {
  BT_LOGR("BluetoothSdpManager::CreateSdpRecord type: %d", aRecord.mType);

  auto it = sSdpResultHandleMap.find(aRecord.mType);

  if (it == sSdpResultHandleMap.end()) {
    sSdpResultHandleMap[aRecord.mType] = aResultHandle;
  }

  sBtSdpInterface->CreateSdpRecord(aRecord, new CreateSdpRecordResultHandler());
}

// static
void BluetoothSdpManager::RemoveSdpRecord(
    int aSdpHandle,
    RemoveSdpRecordResultHandle aResultHandle) {
  BT_LOGR("BluetoothSdpManager::RemoveSdpRecord handle: %d", aSdpHandle);

  // todo: pass remove result to upper profile layer

  sBtSdpInterface->RemoveSdpRecord(aSdpHandle, nullptr);
}

END_BLUETOOTH_NAMESPACE
