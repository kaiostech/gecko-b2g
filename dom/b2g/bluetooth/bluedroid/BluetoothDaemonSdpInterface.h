/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSdpInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSdpInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

class BluetoothDaemonSdpModule {
 public:
  enum { SERVICE_ID = 0x0E };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_SDP_SEARCH = 0x01,
    OPCODE_CREATE_SDP_RECORD = 0x02,
    OPCODE_REMOVE_SDP_RECORD = 0x03
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  void SetNotificationHandler(
      BluetoothSdpNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult SdpSearchCmd(const BluetoothAddress& aBdAddr,
                        const BluetoothUuid& aUuid,
                        BluetoothSdpResultHandler* aRes);
  nsresult CreateSdpRecordCmd(const BluetoothSdpRecord& aRecord,
                              int& aRecordHandle,
                              BluetoothSdpResultHandler* aRes);
  nsresult RemoveSdpRecordCmd(int aSdpHandle, BluetoothSdpResultHandler* aRes);

 protected:
  void HandleSvc(const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<BluetoothSdpResultHandler, void>
      ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<BluetoothSdpResultHandler, void,
                                              BluetoothStatus, BluetoothStatus>
      ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
                BluetoothSdpResultHandler* aRes);

  void SdpSearchRsp(const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
                    BluetoothSdpResultHandler* aRes);

  void CreateSdpRecordRsp(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU,
                          BluetoothSdpResultHandler* aRes);

  void RemoveSdpRecordRsp(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU,
                          BluetoothSdpResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
      NotificationHandlerWrapper, void, int /* sdp type */ , int /* rfcomm */,
      int /* l2cap */, int /* profile version */, int /* features */,
      int, int, int, int, int>
      SdpSearchNotification;

  void SdpSearchNtf(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  static BluetoothSdpNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonSdpInterface final : public BluetoothSdpInterface {
 public:
  explicit BluetoothDaemonSdpInterface(BluetoothDaemonSdpModule* aModule);
  ~BluetoothDaemonSdpInterface();

  void SetNotificationHandler(
      BluetoothSdpNotificationHandler* aNotificationHandler) override;

  /* SDP interfaces from HAL */

  void SdpSearch(const BluetoothAddress& aBdAddr, const BluetoothUuid& aUuid,
                 BluetoothSdpResultHandler* aRes) override;

  void CreateSdpRecord(const BluetoothSdpRecord& aRecord, int& aRecordHandle,
                       BluetoothSdpResultHandler* aRes) override;
  void RemoveSdpRecord(int aSdpHandle,
                       BluetoothSdpResultHandler* aRes) override;

 private:
  void DispatchError(BluetoothSdpResultHandler* aRes, BluetoothStatus aStatus);
  void DispatchError(BluetoothSdpResultHandler* aRes, nsresult aRv);

  BluetoothDaemonSdpModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSdpInterface_h
