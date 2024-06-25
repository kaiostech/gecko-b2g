/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothSdpManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothSdpManager_h

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Vector.h"
#include "nsICryptoHash.h"
#include "ObexBase.h"

BEGIN_BLUETOOTH_NAMESPACE

typedef void (*CreateSdpRecordResultHandle)(int aSdpType, int aRecordHandle);

typedef void (*RemoveSdpRecordResultHandle)(int aStatus);

typedef std::function<void(BluetoothAddress& aDeviceAddress,
                           std::vector<RefPtr<BluetoothSdpRecord>>& aSdpResult)>
             SdpSearchResultHandle;

//typedef void [this](*SdpSearchResultHandle)(BluetoothAddress& aDeviceAddress,
//                                      std::vector<BluetoothSdpRecord*>& aSdpRecord);

class BluetoothSdpManager : /*public nsIObserver,
                            public BluetoothProfileManagerBase,*/
                            public BluetoothSdpNotificationHandler,
                            public BluetoothSdpResultHandler {
 public:
  static BluetoothSdpManager* Get();

  static void InitSdpInterface(BluetoothProfileResultHandler* aRes);

  static void DeinitSdpInterface(BluetoothProfileResultHandler* aRes);

  static void SdpSearch(const BluetoothAddress& aDeviceAddress,
                        const BluetoothUuid& aUuid,
                        SdpSearchResultHandle aResultHandle);

  static void CreateSdpRecord(const BluetoothSdpRecord& aRecord,
                              CreateSdpRecordResultHandle aResultHandle);

  static void RemoveSdpRecord(int aSdpHandle, RemoveSdpRecordResultHandle aResultHandle);

  void SdpSearchNotification(int aUuidSize,
                             const uint8_t* aUuid,
                             const uint8_t* aDeviceAddres,
                             int aSdpSize,
                             const int* aSdpArray) override;

 private:
  class InitProfileResultHandlerRunnable;
  class DeinitProfileResultHandlerRunnable;
  class RegisterModuleResultHandler;
  class UnregisterModuleResultHandler;
  class CreateSdpRecordResultHandler;
};

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_bluedroid_BluetoothSdpManager_h
