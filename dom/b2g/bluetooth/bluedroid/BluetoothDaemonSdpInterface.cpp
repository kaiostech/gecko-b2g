/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonSdpInterface.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "BluetoothUtils.h"
#include "BluetoothUuidHelper.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// SDP module
//

BluetoothSdpNotificationHandler* BluetoothDaemonSdpModule::sNotificationHandler;

void BluetoothDaemonSdpModule::SetNotificationHandler(
    BluetoothSdpNotificationHandler* aNotificationHandler) {
  sNotificationHandler = aNotificationHandler;
}

void BluetoothDaemonSdpModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                         DaemonSocketPDU& aPDU,
                                         DaemonSocketResultHandler* aRes) {
  static void (BluetoothDaemonSdpModule::*const HandleOp[])(
      const DaemonSocketPDUHeader&, DaemonSocketPDU&,
      DaemonSocketResultHandler*) = {
      [0] = &BluetoothDaemonSdpModule::HandleRsp,
      [1] = &BluetoothDaemonSdpModule::HandleNtf};

  MOZ_ASSERT(!NS_IsMainThread());
  // negate twice to map bit to 0/1
  unsigned int isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult BluetoothDaemonSdpModule::SdpSearchCmd(
    const BluetoothAddress& aRemoteAddr, const BluetoothUuid& aUuid,
    BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
      MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_SDP_SEARCH,
                                  6 + 16);  // address + UUID

  nsresult rv;
  rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aUuid, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();

  return NS_OK;
}

nsresult BluetoothDaemonSdpModule::CreateSdpRecordCmd(
    const BluetoothSdpRecord& aRecord,
    BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
      MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CREATE_SDP_RECORD,
                                  44);  // Bluetooth SDP record

  nsresult rv = PackPDU(aRecord, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();

  return NS_OK;
}

nsresult BluetoothDaemonSdpModule::RemoveSdpRecordCmd(
    int aSdpHandle, BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
      MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_REMOVE_SDP_RECORD,
                                  4);  // SDP handle

  nsresult rv = PackPDU(aSdpHandle, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();

  return NS_OK;
}

// Responses
//

void BluetoothDaemonSdpModule::ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                                        DaemonSocketPDU& aPDU,
                                        BluetoothSdpResultHandler* aRes) {
  ErrorRunnable::Dispatch(aRes, &BluetoothSdpResultHandler::OnError,
                          UnpackPDUInitOp(aPDU));
}

void BluetoothDaemonSdpModule::SdpSearchRsp(
    const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
    BluetoothSdpResultHandler* aRes) {
  ResultRunnable::Dispatch(aRes, &BluetoothSdpResultHandler::SdpSearch,
                           UnpackPDUInitOp(aPDU));
}

void BluetoothDaemonSdpModule::CreateSdpRecordRsp(
    const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
    BluetoothSdpResultHandler* aRes) {
  CreateSdpResultRunnable::Dispatch(aRes, &BluetoothSdpResultHandler::CreateSdpRecord,
                           UnpackPDUInitOp(aPDU));
}

void BluetoothDaemonSdpModule::RemoveSdpRecordRsp(
    const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
    BluetoothSdpResultHandler* aRes) {
  ResultRunnable::Dispatch(aRes, &BluetoothSdpResultHandler::RemoveSdpRecord,
                           UnpackPDUInitOp(aPDU));
}

void BluetoothDaemonSdpModule::HandleRsp(const DaemonSocketPDUHeader& aHeader,
                                         DaemonSocketPDU& aPDU,
                                         DaemonSocketResultHandler* aRes) {
  static void (BluetoothDaemonSdpModule::*const HandleRsp[])(
      const DaemonSocketPDUHeader&, DaemonSocketPDU&,
      BluetoothSdpResultHandler*) = {
      [OPCODE_ERROR] = &BluetoothDaemonSdpModule::ErrorRsp,
      [OPCODE_SDP_SEARCH] = &BluetoothDaemonSdpModule::SdpSearchRsp,
      [OPCODE_CREATE_SDP_RECORD] =
          &BluetoothDaemonSdpModule::CreateSdpRecordRsp,
      [OPCODE_REMOVE_SDP_RECORD] =
          &BluetoothDaemonSdpModule::RemoveSdpRecordRsp};

  MOZ_ASSERT(!NS_IsMainThread());  // I/O thread

  if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  RefPtr<BluetoothSdpResultHandler> res =
      static_cast<BluetoothSdpResultHandler*>(aRes);

  if (!res) {
    return;  // Return early if no result handler has been set for response
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

// Returns the current notification handler to a notification runnable
class BluetoothDaemonSdpModule::NotificationHandlerWrapper final {
 public:
  typedef BluetoothSdpNotificationHandler ObjectType;

  static ObjectType* GetInstance() {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

class BluetoothDaemonSdpModule::SdpPDUInitOp final :
    private PDUInitOp {
 public:
  explicit SdpPDUInitOp(DaemonSocketPDU& aPDU)
      : PDUInitOp(aPDU) {}

  nsresult operator()(int& aArg1, UniquePtr<uint8_t[]>& aArg2,
                      UniquePtr<uint8_t[]>& aArg3,
                      int& aArg4, UniquePtr<int[]>& aArg5) const {
    DaemonSocketPDU& pdu = GetPDU();

    /*
     * PDU format
     * +-----------+------+----------------+-----------+----------------------------+
     * | uuid size | uuid | device address | sdp count | sdp value (variable length)|
     * +-----------+------+----------------+-----------+----------------------------+
     */
    nsresult rv;

    /* Read uuid size */
    rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      BT_LOGR("UnpackPDU uuid size failed");
      return rv;
    }

    /* Read uuid data */
    rv = UnpackPDU(pdu, UnpackArray<uint8_t>(aArg2, aArg1));
    if (NS_FAILED(rv)) {
      BT_LOGR("UnpackPDU uuid data failed");
      return rv;
    }

    /* Read device address */
    rv = UnpackPDU(pdu, UnpackArray<uint8_t>(aArg3, 6));
    if (NS_FAILED(rv)) {
      BT_LOGR("UnpackPDU address failed");
      return rv;
    }

    /* Read sdp record size */
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      BT_LOGR("UnpackPDU sdp record size failed. %d", rv);
      return rv;
    }

    int sdpCount = aArg4;

    if (sdpCount != 0) {
      // each SDP record consists of a common header and private attributes
      // you can refer to the struct 'bluetooth_sdp_record' defined in Bluedroid
      // common header: 'bluetooth_sdp_hdr_overlay'
      //     i32: rfcomm_channel_number
      //     i32: l2cap_psm
      //     i32: profile_version
      int elementCount = 3;  /* 3 elements: common header */
      uint16_t uuid;

      switch (aArg1) {
        case 2:
          uuid = (static_cast<uint16_t>(aArg2[0]) << 8) + static_cast<uint16_t>(aArg2[1]);
          break;
        case 4:
          [[fallthrough]];
        case 16:
          uuid = (static_cast<uint16_t>(aArg2[2]) << 8) + static_cast<uint16_t>(aArg2[3]);
          break;
        default:
          BT_LOGR("UnpackPDU sdp record size failed (%d)", aArg1);
          return NS_ERROR_INVALID_ARG;
      }

      switch (uuid) {
        case MAP_MAS:
          // MAS has three private SDP attributes
          // refer to the struct 'bluetooth_sdp_mas_record' defined in Bluedroid
          // private SDP attributes:
          //     i32: mas_instance_id
          //     i32: supported_features
          //     i32: supported_message_types
          elementCount += 3;
          break;

        case MAP_MNS:
          // MNS has one private SDP attribute
          // refer to the struct 'bluetooth_sdp_mns_record' defined in Bluedroid
          // private SDP attributes:
          //     i32: supported_features
          elementCount += 1;
          break;

        case PBAP_PSE:
          // PSE has two private SDP attributes
          // refer to the struct 'bluetooth_sdp_pse_record' defined in Bluedroid
          // private SDP attributes:
          //     i32: supported_features
          //     i32: supported_repositories
          elementCount += 2;
          break;

        case PBAP_PCE: // no private sdp property
          break;

        default:
          BT_LOGR("UnpackPDU sdp record size failed");
          return NS_ERROR_INVALID_ARG;
      }

      BT_LOGR("UnpackPDU uuid 0x%04x (%d)", uuid, aArg1);

      /* Read sdp data */
      rv = UnpackPDU(pdu, UnpackArray<int>(aArg5, elementCount * sdpCount));
      if (NS_FAILED(rv)) {
        BT_LOGR("UnpackPDU sdp data failed (%d). element size %d, count %d",
                rv, elementCount, sdpCount);
        return rv;
      }
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void BluetoothDaemonSdpModule::SdpSearchNtf(
    const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU) {
  SdpSearchNotification::Dispatch(
      &BluetoothSdpNotificationHandler::SdpSearchNotification,
      SdpPDUInitOp(aPDU));
}

void BluetoothDaemonSdpModule::HandleNtf(const DaemonSocketPDUHeader& aHeader,
                                         DaemonSocketPDU& aPDU,
                                         DaemonSocketResultHandler* aRes) {
  static void (BluetoothDaemonSdpModule::*const HandleNtf[])(
      const DaemonSocketPDUHeader&,
      DaemonSocketPDU&) = {[0] = &BluetoothDaemonSdpModule::SdpSearchNtf};

  MOZ_ASSERT(!NS_IsMainThread());

  uint8_t index = aHeader.mOpcode - 0x81;

  if (NS_WARN_IF(!(index < MOZ_ARRAY_LENGTH(HandleNtf))) ||
      NS_WARN_IF(!HandleNtf[index])) {
    return;
  }

  (this->*(HandleNtf[index]))(aHeader, aPDU);
}

//
// SDP interface
//

BluetoothDaemonSdpInterface::BluetoothDaemonSdpInterface(
    BluetoothDaemonSdpModule* aModule)
    : mModule(aModule) {}

BluetoothDaemonSdpInterface::~BluetoothDaemonSdpInterface() {}

void BluetoothDaemonSdpInterface::SetNotificationHandler(
    BluetoothSdpNotificationHandler* aNotificationHandler) {
  MOZ_ASSERT(mModule);

  mModule->SetNotificationHandler(aNotificationHandler);
}

/* SdpSearch / CreateSdpRecord / RemoveSdpRecord */

void BluetoothDaemonSdpInterface::SdpSearch(const BluetoothAddress& aBdAddr,
                                            const BluetoothUuid& aUuid,
                                            BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SdpSearchCmd(aBdAddr, aUuid, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void BluetoothDaemonSdpInterface::CreateSdpRecord(
    const BluetoothSdpRecord& aRecord,
    BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->CreateSdpRecordCmd(aRecord, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void BluetoothDaemonSdpInterface::RemoveSdpRecord(
    int aSdpHandle, BluetoothSdpResultHandler* aRes) {
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->RemoveSdpRecordCmd(aSdpHandle, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void BluetoothDaemonSdpInterface::DispatchError(BluetoothSdpResultHandler* aRes,
                                                BluetoothStatus aStatus) {
  DaemonResultRunnable1<
      BluetoothSdpResultHandler, void, BluetoothStatus,
      BluetoothStatus>::Dispatch(aRes, &BluetoothSdpResultHandler::OnError,
                                 ConstantInitOp1<BluetoothStatus>(aStatus));
}

void BluetoothDaemonSdpInterface::DispatchError(BluetoothSdpResultHandler* aRes,
                                                nsresult aRv) {
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
