/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothMapSmsManager.h"
#include "base/basictypes.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuidHelper.h"

#include "mozilla/dom/BluetoothMapParametersBinding.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsIInputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsStringStream.h"

#include "BluetoothSdpManager.h"

#define FILTER_NO_SMS_GSM 0x01
#define FILTER_NO_SMS_CDMA 0x02
#define FILTER_NO_EMAIL 0x04
#define FILTER_NO_MMS 0x08
#define SDP_TYPE_MNS 0x02

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
// UUID of Map Mas
static const BluetoothUuid kMapMas(MAP_MAS);

// UUID of Map Mns
static const BluetoothUuid kMapMns(MAP_MNS);

// UUID used in Map OBEX MAS target header
static const BluetoothUuid kMapMasObexTarget(0xBB, 0x58, 0x2B, 0x40, 0x42, 0x0C,
                                             0x11, 0xDB, 0xB0, 0xDE, 0x08, 0x00,
                                             0x20, 0x0C, 0x9A, 0x66);

// UUID used in Map OBEX MNS target header
static const BluetoothUuid kMapMnsObexTarget(0xBB, 0x58, 0x2B, 0x41, 0x42, 0x0C,
                                             0x11, 0xDB, 0xB0, 0xDE, 0x08, 0x00,
                                             0x20, 0x0C, 0x9A, 0x66);

StaticRefPtr<BluetoothMapSmsManager> sMapSmsManager;
static int sSdpMasHandle = -1;

// OBEX v1.5 defined SRM enabled (0x01), disabled (0x00), advertise remote
// device SRM support (0x02). 0x02 means request an additional request packet,
// and client must wait fo next response.
static bool sSrmEnabled = false;

// Bluetooth MAP 1.3 only supports 0x01 (wait). Local device is requesting the
// remote device to wait. Client enforces flow control for GET/PUT operation.
static bool sSrmpWaitEnabled = false;
static bool sSrmpWaitSingleReply = false;
// Stop sending SRM header after the first SRM header in response when SRMP
// wait parameter received.
static bool sFirstSrmHeaderSent = false;
static bool sFirstPutResponse = false;
static const size_t MAX_READ_SIZE_MAP = 1 << 16;
}  // namespace

BEGIN_BLUETOOTH_NAMESPACE

bool BluetoothMapSmsManager::sInShutdown = false;

NS_IMETHODIMP
BluetoothMapSmsManager::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  MOZ_ASSERT(sMapSmsManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "MapSmsManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void BluetoothMapSmsManager::HandleShutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  sInShutdown = true;
  Disconnect(nullptr);
  Uninit();

  sMapSmsManager = nullptr;
}

BluetoothMapSmsManager::BluetoothMapSmsManager()
    : mBodyRequired(false),
      mFractionDeliverRequired(false),
      mMasConnected(false),
      mMnsConnected(false),
      mNtfRequired(false),
      mConnectionId(0xFFFFFFFF),
      mLastCommand(0),
      mPutFinalFlag(false),
      mPutPacketLength(0),
      mPutReceivedPacketLength(0),
      mPutBodyLength(0),
      mReceivedRfcommLength(0),
      mRfcommPacketIncomplete(false),
      mIsMnsRfcomm(false) {
  BuildDefaultFolderStructure();
}

BluetoothMapSmsManager::~BluetoothMapSmsManager() {}

nsresult BluetoothMapSmsManager::Init() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  /**
   * We don't start listening here as BluetoothServiceBluedroid calls Listen()
   * immediately when BT stops.
   *
   * If we start listening here, the listening fails when device boots up since
   * Listen() is called again and restarts server socket. The restart causes
   * absence of read events when device boots up.
   */

  return NS_OK;
}

void BluetoothMapSmsManager::Uninit() {
  if (mMasServerSocket) {
    mMasServerSocket->SetObserver(nullptr);

    if (mMasServerSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
      mMasServerSocket->Close();
    }
    mMasServerSocket = nullptr;
  }

  if (mMasSocket) {
    mMasSocket->SetObserver(nullptr);

    if (mMasSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
      mMasSocket->Close();
    }
    mMasSocket = nullptr;
  }

  if (mMasRfcommSocket) {
    mMasRfcommSocket->SetObserver(nullptr);

    if (mMasRfcommSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
      mMasRfcommSocket->Close();
    }
    mMasRfcommSocket = nullptr;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  Unused << NS_WARN_IF(
      NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)));
}

void BluetoothMapSmsManager::SdpSearchResultHandle(BluetoothAddress& aDeviceAddress,
                                  std::vector<RefPtr<BluetoothSdpRecord>>& aSdpRecord) {
  for (auto it : aSdpRecord) {
    if (it->mType) {
      mMnsL2capPsm = it->mL2capPsm;
      mMnsRfcommChannel = it->mRfcommChannelNumber;

      BT_LOGD("Remote MNS L2CAP channel: %x, RFCOMM: %d", mMnsL2capPsm, mMnsRfcommChannel);

      // If MCE supports RFCOMM only, we shall got ffffffff on L2CAP PSM from bluetooth stack.
      if (mMnsL2capPsm == 0xFFFFFFFF) {
        BT_LOGD("CreateMnsObexConnection rfcomm: %d", mMnsRfcommChannel);
        mMnsSocket->Connect(mDeviceAddress, kMapMns, BluetoothSocketType::RFCOMM, mMnsRfcommChannel,
                            false, false);
        mIsMnsRfcomm = true;
      } else {
        BT_LOGD("CreateMnsObexConnection l2cap: %x", mMnsL2capPsm);
        mMnsSocket->Connect(mDeviceAddress, kMapMns, BluetoothSocketType::L2CAP, mMnsL2capPsm,
                            false, false);
        mIsMnsRfcomm = false;
      }
      BT_LOGD("Support Profile version: %d", it->mProfileVersion);

      break;
    }
  }
}

// static
void BluetoothMapSmsManager::InitMapSmsInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aRes) {
    aRes->Init();
  }
}

// static
void BluetoothMapSmsManager::DeinitMapSmsInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sMapSmsManager) {
    sMapSmsManager->Uninit();
    sMapSmsManager = nullptr;
  }

  if (aRes) {
    aRes->Deinit();
  }
}

// Dump raw packets for investigating packet data integrity
void BluetoothMapSmsManager::Dump(const UnixSocketBuffer* buffer) {
  if (!buffer) {
    BT_LOGD("Buffer is null. Cannot dump.");
    return;
  }

  const uint8_t* data = buffer->GetData();
  size_t size = buffer->GetSize();

  if (data == nullptr || size == 0) {
    BT_LOGD("Buffer is empty");
    return;
  }

  std::string output;
  const size_t maxChunkSize = 1000;

  for (size_t i = 0; i < size; i++) {
    char byte[4];
    sprintf(byte, "%02x ", data[i]);
    output += byte;
    if (output.length() > maxChunkSize) {
      BT_LOGD("%s", output.c_str());
      output.clear();
    }
  }

  if (!output.empty()) {
    BT_LOGD("%s", output.c_str());
  }
}

// static
BluetoothMapSmsManager* BluetoothMapSmsManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // Exit early if sMapSmsManager already exists
  if (sMapSmsManager) {
    return sMapSmsManager;
  }

  // Do not create a new instance if we're in shutdown
  if (NS_WARN_IF(sInShutdown)) {
    return nullptr;
  }

  // Create a new instance, register, and return
  RefPtr<BluetoothMapSmsManager> manager = new BluetoothMapSmsManager();
  if (NS_WARN_IF(NS_FAILED(manager->Init()))) {
    return nullptr;
  }

  sMapSmsManager = manager;

  return sMapSmsManager;
}

bool BluetoothMapSmsManager::Listen() {
  MOZ_ASSERT(NS_IsMainThread());

  // Fail to listen if |mMasSocket| already exists
  if (NS_WARN_IF(mMasSocket)) {
    BT_LOGR("Cannot listen: !mMasSocket");
    return false;
  }

  if (NS_WARN_IF(mMasRfcommSocket)) {
    BT_LOGR("Cannot listen mas rfcomm socket");
    return false;
  }

  /**
   * Restart server socket since its underlying fd becomes invalid when
   * BT stops; otherwise no more read events would be received even if
   * BT restarts.
   */
  if (mMasServerSocket &&
      mMasServerSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasServerSocket->Close();
  }
  mMasServerSocket = nullptr;

  mMasServerSocket = new BluetoothSocket(this);

  if (mMasRfcommServerSocket &&
      mMasRfcommServerSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasRfcommServerSocket->Close();
  }
  mMasRfcommServerSocket = nullptr;

  mMasRfcommServerSocket = new BluetoothSocket(this);

  nsString sdpString;
  /**
   * The way bluedroid handles MAP SDP record is very hacky.
   * In Lollipop version, SDP string format would be instanceId + msg type
   * + msg name. See add_maps_sdp in btif/src/btif_sock_sdp.c
   */
  // MAS instance id
  sdpString.AppendPrintf("%02x", SDP_SMS_MMS_INSTANCE_ID);
  // Supported message type
  sdpString.AppendPrintf("%02x", SDP_MESSAGE_TYPE_SMS_GSM |
                                     SDP_MESSAGE_TYPE_SMS_CDMA |
                                     SDP_MESSAGE_TYPE_MMS);

  // BlueDroid automatically assign channel number if the given numver is -1
  int rfcommChannel = -1;
  int l2capChannel = -1;
  rfcommChannel = DEFAULT_RFCOMM_CHANNEL_MAS;
  l2capChannel = DEFAULT_L2CAP_PSM_MAS;
  BluetoothMasRecord masRecord(rfcommChannel, l2capChannel, 0);  // instance id starts from 0
  BT_LOGD("Create sdp record for MAP_MAS");

  auto sdpResultHandle = [](int aType, int aHandle) {
    BT_LOGR("MAP handle CreateSdpRecord result: %d", aHandle);
    sSdpMasHandle = aHandle;
  };

  if (sSdpMasHandle != -1) {
    BluetoothSdpManager::RemoveSdpRecord(sSdpMasHandle, nullptr);
    sSdpMasHandle = -1;
  }

  BluetoothSdpManager::CreateSdpRecord(masRecord, sdpResultHandle);

  // SDP service name
  sdpString.AppendLiteral("SMS Message Access");
  nsresult rv;
  BT_LOGR("MAP SMS L2CAP Listening");
  rv = mMasServerSocket->Listen(sdpString, kMapMas, BluetoothSocketType::L2CAP,
                                l2capChannel, false, true);

  if (NS_FAILED(rv)) {
    BT_LOGR("Fail to listen on L2CAP channel.");
    mMasServerSocket = nullptr;
    return false;
  }

  BT_LOGR("MAP SMS RFCOMM Listening");
  rv = mMasRfcommServerSocket->Listen(sdpString, kMapMas, BluetoothSocketType::RFCOMM,
                               rfcommChannel, false, true);

  if (NS_FAILED(rv)) {
    BT_LOGR("Fail to listen on RFCOMM channel.");
    mMasRfcommServerSocket = nullptr;
    return false;
  }

  return true;
}

void BluetoothMapSmsManager::MnsDataHandler(UnixSocketBuffer* aMessage) {
  // Ensure valid access to data[0], i.e., opCode
  int receivedLength = aMessage->GetSize();
  if (receivedLength < 1) {
    BT_LOGR("Receive empty response packet");
    return;
  }

  BT_LOGD("MnsDataHandler received length %d", receivedLength);

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];

  uint8_t expectedOpCode = (mLastCommand == ObexRequestCode::Put)
                               ? ObexResponseCode::Continue
                               : ObexResponseCode::Success;

  if (opCode != expectedOpCode) {
    if (mLastCommand == ObexRequestCode::Put ||
        mLastCommand == ObexRequestCode::Abort ||
        mLastCommand == ObexRequestCode::PutFinal) {
      SendMnsDisconnectRequest();
    }
    return;
  }

  if (mLastCommand == ObexRequestCode::Connect) {
    BT_LOGR("MNS connected");

    ObexHeaderSet pktHeaders;
    // Section 3.3.1 "Connect", IrOBEX 1.2
    // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
    // [Headers:var]
    if (receivedLength < 7 ||
        !ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
      SendReply(ObexResponseCode::BadRequest);
      return;
    }
    mConnectionId =
        mozilla::NativeEndian::swapToBigEndian(pktHeaders.GetConnectionId());
  } else if (mLastCommand == ObexRequestCode::Disconnect) {
    BT_LOGR("MNS disconnected");

    mMnsSocket->Close();
    mMnsSocket = nullptr;
    mNtfRequired = false;
    mConnectionId = 0xFFFFFFFF;
  } else if (mLastCommand == ObexRequestCode::Abort) {
    SendMnsDisconnectRequest();
  } else if (mLastCommand == ObexRequestCode::Put) {
    MnsPutMultiRequest();
  } else if (mLastCommand == ObexRequestCode::PutFinal) {
    if (mMnsDataStream) {
      mMnsDataStream->Close();
      mMnsDataStream = nullptr;
    }
  } else {
    BT_WARNING("Unhandled ObexRequestCode %x", opCode);
    BT_LOGR("Current mLastCommand: %x", mLastCommand);
  }
}

// In the case of RFCOMM sockets, it's necessary to verify the OBEX total size
// and hold until all packet fragments have been received from the
// ReceivedSocketData callback. There's a possibility that the
// ReceivedSocketData callback might include fragmented OBEX packets.
// Conversely, for L2CAP sockets, each call to the ReceivedSocketData callback
// returns a fully formed OBEX packet.
void BluetoothMapSmsManager::MasDataHandler(UnixSocketBuffer* aMessage,
                                            bool aIsRfcomm) {
  /**
   * Ensure
   * - valid access to data[0], i.e., opCode
   * - received packet length smaller than max packet length
   */
  int receivedLength = aMessage->GetSize();
  BT_LOGD("MasDataHandler: socket buffer size: %d", receivedLength);

  if (gBluetoothDebugFlag) {
    Dump(aMessage);
  }

  if (receivedLength < 1 || receivedLength > MAX_PACKET_LENGTH) {
    SendReply(ObexResponseCode::BadRequest);
    return;
  }

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];

  BT_LOGD("opcode: %d", opCode);

  ObexHeaderSet pktHeaders;
  nsString type;
  int8_t srm;
  int8_t srmp;

  // Append data to buffer if via RFCOMM channel and packet is incomplete
  // mRfcommPacketIncomplete set true only if the first PUT packet
  // received. Reassemble packets from the RFCOMM channel and handle
  // HandleRfcommPackets function.
  if (aIsRfcomm && mRfcommPacketIncomplete) {
    mReceivedRfcommLength += receivedLength;
    mRfcommDataBuffer.insert(mRfcommDataBuffer.end(), data,
                             data + receivedLength);
    BT_LOGD("RFCOMM packet Total received length: %d", mReceivedRfcommLength);

    // Check if all packets have been received
    if (mReceivedRfcommLength == mObexTotalLength) {
      BT_LOGD("All RFCOMM chunks received.");
      mRfcommPacketIncomplete = false;
      // Parse headers from the reassembled packet and handle it
      ObexHeaderSet reassembledHeaders;

      // Calculate the size of the data in mRfcommDataBuffer
      size_t dataSize = mRfcommDataBuffer.size();

      // Convert the vector to UnixSocketBuffer
      auto completeMessage = MakeUnique<UnixSocketRawData>(MAX_READ_SIZE_MAP);
      // Append the data from mRfcommDataBuffer into the UnixSocketBuffer
      uint8_t* bufferData = completeMessage->Append(dataSize);

      if (bufferData) {
        std::memcpy(bufferData, mRfcommDataBuffer.data(), dataSize);
      }

      // First, socket data transfer to Packet data
      if (!ParsePutPacketHeaders(ObexRequestCode::PutFinal,
                                 completeMessage.get(), &reassembledHeaders)) {
        BT_LOGR("RFCOMM Cannot parse Put Packet Header, error! early returns");
        return;
      }

      BT_LOGD("Completed RFCOMM data size: %zu ", mRfcommDataBuffer.size());

      nsString type;
      reassembledHeaders.GetContentType(type);
      BT_LOGD("Type: %s", NS_ConvertUTF16toUTF8(type).get());
      if (type.EqualsLiteral("x-bt/message")) {
        HandleRfcommPushMessage(reassembledHeaders, bufferData[0]);
      }

      // Packet is finished.
      mReceivedRfcommLength = 0;
      mObexTotalLength = 0;
    }
  }

  switch (opCode) {
    case ObexRequestCode::Connect: {
      BT_LOGR("Received OBEX Connect");
      // Section 3.3.1 "Connect", IrOBEX 1.2
      // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
      // [Headers:var]
      if (receivedLength < 7 ||
          !ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
        BT_LOGR("Fail to parse OBEX header. Reply OBEX Connect BadRequest");
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      // "Establishing an OBEX Session"
      // The OBEX header target shall equal to MAS obex target UUID.
      if (!CompareHeaderTarget(pktHeaders)) {
        BT_LOGR("Fail to parse OBEX header. Reply OBEX Connect BadRequest");
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      mRemoteMaxPacketLength = BigEndian::readUint16(&data[5]);

      if (mRemoteMaxPacketLength < kObexLeastMaxSize) {
        BT_LOGD("Remote maximum packet length %d", mRemoteMaxPacketLength);
        mRemoteMaxPacketLength = 0;
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      // The user consent is required by the UX spec.
      // If user accept the connection request, the OBEX connection session
      // will be processed; Otherwise, the session will be rejected.
      ObexResponseCode response = NotifyConnectionRequest();
      if (response != ObexResponseCode::Success) {
        BT_LOGR("Notify connect failure");
        SendReply(response);
        return;
      }
      break;
    }
    case ObexRequestCode::Disconnect:
    case ObexRequestCode::Abort:
      // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
      // The format of request packet of "Disconnect" and "Abort" are the same
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToDisconnectOrAbort();
      AfterMapSmsDisconnected();
      break;
    case ObexRequestCode::SetPath: {
      // Section 3.3.6 "SetPath", IrOBEX 1.2
      // [opcode:1][length:2][flags:1][contents:1][Headers:var]
      if (receivedLength < 5 ||
          !ParseHeaders(&data[5], receivedLength - 5, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      uint8_t response = SetPath(data[3], pktHeaders);
      if (response != ObexResponseCode::Success) {
        SendReply(response);
        return;
      }

      ReplyToSetPath();
    } break;
    case ObexRequestCode::Put:
    case ObexRequestCode::PutFinal:
      BT_LOGD("Received opcode: Put/PutFinal");
      // It could be the first Put packet via RFCOMM channel. Then we read obex
      // header to get total length. mObexTotalLength = total rfcomm socket data
      // length
      if (aIsRfcomm) {
        mObexTotalLength = BigEndian::readUint16(&data[1]);
        BT_LOGD("OBEX total length (rfcomm): %d", mObexTotalLength);
        if (mReceivedRfcommLength == 0) {
          mReceivedRfcommLength += receivedLength;
          BT_LOGD("PUT current mReceivedRfcommLength is: %d",
                  mReceivedRfcommLength);
          mRfcommDataBuffer.insert(mRfcommDataBuffer.end(), data,
                                   data + receivedLength);
        }
        if (mObexTotalLength > mReceivedRfcommLength) {
          BT_LOGD("Expect More rfcomm chunks");
          mRfcommPacketIncomplete = true;
          return;
        }
      }

      mPutFinalFlag = (opCode == ObexRequestCode::PutFinal);

      // First, socket data transfer to Packet data
      if (!ParsePutPacketHeaders(opCode, aMessage, &pktHeaders)) {
        BT_LOGR("Cannot parse Put Packet Header, error! early returns");
        return;
      }

      // Multi-packet PUT request (0x02) may not contain Type header. Only body
      // left.
      if (!pktHeaders.Has(ObexHeaderId::Type)) {
        BT_LOGD(
            "Missing OBEX PUT request Type header, multiple OBEX "
            "fragmentation");
        ExtractBodyMultiPutPackets(pktHeaders, opCode);
        return;
      }

      pktHeaders.GetContentType(type);
      BT_LOGR("Type: %s", NS_ConvertUTF16toUTF8(type).get());

      // Validate SRM. 0x00, 0x01, 0x02 are only valid value.
      srm = pktHeaders.GetSRM();
      BT_LOGD("SRM: %d", srm);
      if (srm == 1) {
        sSrmEnabled = true;
      }

      if (srm > 2) {
        BT_LOGR("Ignore invalid SRM value, SRM disabled");
        // Ignore invalid SRM
        sSrmEnabled = false;
      }

      srmp = pktHeaders.GetSRMP();
      BT_LOGD("SRMP: %d", srmp);

      if (srmp == 1) {
        sSrmpWaitEnabled = true;
        BT_LOGR("SRMP enabled");
      }

      if (srmp == 0) {
        sSrmpWaitEnabled = false;
        BT_LOGR("SRMP disabled");
      }

      if (type.EqualsLiteral("x-bt/MAP-NotificationRegistration")) {
        /* We need to reply notification registration/de-registration "OK"
         * first, then handle registration/de-registration to send Mns
         * connect/disconnect obex command.
         */
        ReplyToPut(ObexResponseCode::Success);
        HandleNotificationRegistration(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/messageStatus")) {
        HandleSetMessageStatus(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/message")) {
        HandleSmsMmsPushMessage(pktHeaders, opCode);
      } else if (type.EqualsLiteral("x-bt/MAP-messageUpdate")) {
        /* MAP 5.9, There is no concept for Sms/Mms to update inbox. If the
         * MSE does NOT allowed the polling of its mailbox it shall answer
         * with a 'Not implemented' error response.
         */
        ReplyToPut(ObexResponseCode::NotImplemented);
      } else if (type.EqualsLiteral("x-bt/MAP-notification-filter")) {
        ReplyToPut(ObexResponseCode::Success);
      } else {
        BT_LOGR("Unknown MAP PUT request type: %s",
                NS_ConvertUTF16toUTF8(type).get());
        ReplyToPut(ObexResponseCode::NotImplemented);
      }
      break;
    case ObexRequestCode::Get:
    case ObexRequestCode::GetFinal: {
      BT_LOGD("Received opcode: Get/GetFinal");
      /* When |mMasDataStream| requires multiple response packets to complete,
       * the client should continue to issue GET requests until the final body
       * information (i.e., End-of-Body header) arrives, along with
       * ObexResponseCode::Success
       */

      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      // Per OBEX 1.5 specification, SRMP packet could contain no type header,
      // but only SRMP value. So we don't validate type.
      pktHeaders.GetContentType(type);

      srm = pktHeaders.GetSRM();

      BT_LOGD("Getting SRM value: %d", srm);

      if (srm == 1) {
        sSrmEnabled = true;
      } else if (srm == 0) {
        sSrmEnabled = false;
      }

      BT_LOGR("SRM enabled: %d", sSrmEnabled);

      // SRMP field could be empty, if previous wait condition is already
      // happened. Must consider if -1 is returned during wait condition.
      srmp = pktHeaders.GetSRMP();

      if (srmp == 1) {
        if (sSrmpWaitEnabled) {
          BT_LOGR("SRMP already enabled, Single reply then wait");
          sSrmpWaitSingleReply = true;
        }

        // The first SRMP wait request
        sSrmpWaitEnabled = true;
        BT_LOGR("SRMP enabled");
      } else if (srmp == 0) {
        sSrmpWaitEnabled = false;
        BT_LOGR("SRMP disabled");
      }

      // The remote is in SRM mode and previous command is Wait.
      // Now no more SRMP wait header with GET operation.
      // Only GET packet without any other SRM/SRMP or type header.
      // PTS test case MAP/MSE/GOEP/SRMP/BV-02-C.
      if (sSrmEnabled && sSrmpWaitEnabled && srmp == -1) {
        BT_LOGR("Disable SRMP mode");
        sSrmpWaitEnabled = false;
        sSrmpWaitSingleReply = false;
        while (mMasDataStream) {
          if (sSrmpWaitEnabled && !sSrmpWaitSingleReply) {
            BT_LOGR("SRMP enabled. DO NOT reply Continue, wait for next GET!");
            sSrmpWaitSingleReply = false;
            break;
          }

          auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);

          if (!ReplyToGetWithHeaderBody(std::move(res), kObexRespHeaderSize)) {
            BT_LOGR("Failed to reply to MAP GET request.");
            SendReply(ObexResponseCode::InternalServerError);
            sSrmpWaitSingleReply = false;
          }
        }
        return;
      }

      // For the case non-SRM mode
      if (mMasDataStream && !sSrmEnabled) {
        auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
        if (!ReplyToGetWithHeaderBody(std::move(res), kObexRespHeaderSize)) {
          BT_LOGR("Failed to reply to MAP GET request.");
          SendReply(ObexResponseCode::InternalServerError);
        }
        return;
      }

      if (type.EqualsLiteral("x-obex/folder-listing")) {
        HandleSmsMmsFolderListing(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/MAP-msg-listing")) {
        HandleSmsMmsMsgListing(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/message")) {
        HandleSmsMmsGetMessage(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/MASInstanceInformation")) {
        BT_LOGR("Handle MAS Instance information");
        HandleSmsMmsInstanceInfo(pktHeaders);
      } else if (type.EqualsLiteral("") && sSrmpWaitSingleReply) {
        BT_LOGR("SRMP packet with empty type and single reply");
        auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
        if (!ReplyToGetWithHeaderBody(std::move(res), kObexRespHeaderSize)) {
          BT_LOGR("Failed to reply to MAP GET request.");
          SendReply(ObexResponseCode::InternalServerError);
        }
      } else {
        BT_LOGR("Unknown MAP GET request type: %s",
                NS_ConvertUTF16toUTF8(type).get());
        if (sSrmEnabled) {
          // When SRM enabled, each OBEX packet may not include type header.
          BT_LOGR("SRM enabled, ignore unknown type");
          return;
        }
        SendReply(ObexResponseCode::NotImplemented);
      }
      break;
    }
    default:
      // If we got data from RFCOMM socket, unlike via L2CAP channel, we are
      // not going to have obex headers after the first callback from rfcomm
      // sock. So it's possible to get invalid OBEX opcode, since it's not
      // complete obex packet.
      if (aIsRfcomm) {
        BT_LOGR("RFCOMM ignore Unrecognized ObexRequestCode %x", opCode);
        return;
      }

      SendReply(ObexResponseCode::NotImplemented);
      BT_LOGR("Unrecognized ObexRequestCode %x", opCode);
      break;
  }
}

bool BluetoothMapSmsManager::ComposePacket(uint8_t aOpCode,
                                           UnixSocketBuffer* aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMessage);

  const uint8_t* data = aMessage->GetData();
  int frameHeaderLength = 0;

  // A PUT request from remote devices may be divided into multiple parts.
  // In other words, one request may need to be received multiple times.
  // Here, will start to compose these parts to a full PUT request packet.
  //
  // |mPutReceivedPacketLength| is to indicate that the current received packet
  // length of the PUT packet.
  // |mPutReceivedPacketLength == 0| means it's the is the first part of the
  // Put packet.
  // Section 3.3.3 "Put", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  frameHeaderLength = 3;

  // |mPutPacketLength| = packet length - field length
  mPutPacketLength = BigEndian::readUint16(&data[1]) - frameHeaderLength;

  // The current PUT request packet data buffer.
  mPutReceivedDataBuffer = MakeUnique<uint8_t[]>(mPutPacketLength);
  mPutFinalFlag = (aOpCode == ObexRequestCode::PutFinal);

  if (mPutReceivedPacketLength == 0) {
    sFirstPutResponse = true;
  }

  int dataLength = aMessage->GetSize() - frameHeaderLength;

  memcpy(mPutReceivedDataBuffer.get() + mPutReceivedPacketLength,
         &data[frameHeaderLength], dataLength);

  mPutReceivedPacketLength += dataLength;

  return true;
}

bool BluetoothMapSmsManager::ParsePutPacketHeaders(
    uint8_t aOpCode, mozilla::ipc::UnixSocketBuffer* aMessage,
    ObexHeaderSet* aPktHeaders) {
  if (!ComposePacket(aOpCode, aMessage)) {
    BT_LOGR("ParsePutPacketHeaders: return false after calling ComposePacket");
    return false;
  }

  bool ret =
      ParseHeaders(mPutReceivedDataBuffer.get(), mPutPacketLength, aPktHeaders);

  if (!ret) {
    BT_LOGR("ParsePutPacketHeaders bad request");
    ReplyToPut(ObexResponseCode::BadRequest);
  }

  // These variables can be reset here because this is where the complete
  // put packet is already handled.
  mPutPacketLength = 0;
  mPutReceivedPacketLength = 0;
  mPutReceivedDataBuffer = nullptr;

  return ret;
}

// Virtual function of class SocketConsumer
void BluetoothMapSmsManager::ReceiveSocketData(
    BluetoothSocket* aSocket, UniquePtr<UnixSocketBuffer>& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());

  bool isRfcommType = false;

  if (aSocket == mMnsSocket) {
    MnsDataHandler(aMessage.get());
  } else {
    if (aSocket == mMasRfcommSocket) {
      isRfcommType = true;
    } else if (aSocket == mMasSocket) {
      isRfcommType = false;
    }

    MasDataHandler(aMessage.get(), isRfcommType);
  }
}

bool BluetoothMapSmsManager::CompareHeaderTarget(const ObexHeaderSet& aHeader) {
  const ObexHeader* header = aHeader.GetHeader(ObexHeaderId::Target);

  if (!header) {
    BT_LOGR("No ObexHeaderId::Target in header");
    return false;
  }

  if (header->mDataLength != sizeof(BluetoothUuid)) {
    BT_LOGR("Length mismatch: %d != 16", header->mDataLength);
    return false;
  }

  for (uint8_t i = 0; i < sizeof(BluetoothUuid); i++) {
    if (header->mData[i] != kMapMasObexTarget.mUuid[i]) {
      BT_LOGR("UUID mismatch: received target[%d]=0x%x != 0x%x", i,
              header->mData[i], kMapMasObexTarget.mUuid[i]);
      return false;
    }
  }

  return true;
}

void BluetoothMapSmsManager::NotifyMapVersion(bool aIsRfcomm) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_LOGR("Failed to get Bluetooth service");
    return;
  }

  nsAutoString mapVersion;

  if (aIsRfcomm) {
    mapVersion.AssignLiteral(u"1.1");
    BT_LOGR("MAP version 1.1 DistributeSignal");
  } else {
    mapVersion.AssignLiteral(u"1.3");
    BT_LOGR("MAP version 1.3 DistributeSignal");
  }

  bs->DistributeSignal(BluetoothSignal(MAP_VERSION_ID, KEY_MAP, mapVersion));
}

ObexResponseCode BluetoothMapSmsManager::NotifyConnectionRequest() {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_LOGR("Failed to get Bluetooth service");
    return ObexResponseCode::PreconditionFailed;
  }

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  BT_LOGR("NotifyConnectionRequest DistributeSignal");
  bs->DistributeSignal(
      BluetoothSignal(MAP_CONNECTION_REQ_ID, KEY_MAP, deviceAddressStr));

  return ObexResponseCode::Success;
}

bool BluetoothMapSmsManager::ReplyToConnectionRequest(bool aAccept) {
  if (!aAccept) {
    return SendReply(ObexResponseCode::Forbidden);
  }

  bool ret = ReplyToConnect();
  if (ret) {
    AfterMapSmsConnected();
  }

  return ret;
}

uint8_t BluetoothMapSmsManager::SetPath(uint8_t flags,
                                        const ObexHeaderSet& aHeader) {
  // Section 5.2 "SetPath Function", MapSms 1.2
  // flags bit 1 must be 1 and bit 2~7 be 0
  if ((flags >> 1) != 1) {
    BT_LOGR("Illegal flags [0x%x]: bits 1~7 must be 0x01", flags);
    return ObexResponseCode::BadRequest;
  }

  /**
   * Three cases:
   * 1) Go up 1 level   - flags bit 0 is 1
   * 2) Go back to root - flags bit 0 is 0 AND name header is empty
   * 3) Go down 1 level - flags bit 0 is 0 AND name header is not empty,
   *                      where name header is the name of child folder
   */
  if (flags & 1) {
    // Go up 1 level
    BluetoothMapFolder* parent = mCurrentFolder->GetParentFolder();
    if (!parent) {
      mCurrentFolder = parent;
      BT_LOGR("MAS SetPath Go up 1 level");
    }
  } else {
    MOZ_ASSERT(aHeader.Has(ObexHeaderId::Name));

    nsString childFolderName;
    aHeader.GetName(childFolderName);

    if (childFolderName.IsEmpty()) {
      // Go back to root
      mCurrentFolder = mRootFolder;
      BT_LOGR("MAS SetPath Go back to root");
    } else {
      // Go down 1 level
      BluetoothMapFolder* child = mCurrentFolder->GetSubFolder(childFolderName);
      if (!child) {
        BT_LOGR("Illegal sub-folder name [%s]",
                NS_ConvertUTF16toUTF8(childFolderName).get());
        return ObexResponseCode::NotFound;
      }

      mCurrentFolder = child;
      BT_LOGR("MAS SetPath Go down to 1 level");
    }
  }

  mCurrentFolder->DumpFolderInfo();

  return ObexResponseCode::Success;
}

void BluetoothMapSmsManager::AfterMapSmsConnected() { mMasConnected = true; }

void BluetoothMapSmsManager::AfterMapSmsDisconnected() {
  mMasConnected = false;
  mBodyRequired = false;
  mFractionDeliverRequired = false;

  mPutFinalFlag = false;
  mPutPacketLength = 0;
  mPutReceivedPacketLength = 0;
  mPutReceivedDataBuffer = nullptr;

  mReceivedRfcommLength = 0;
  mRfcommPacketIncomplete = false;
  mIsMnsRfcomm = false;
  mMultiPutDataBuffer.clear();
  mRfcommDataBuffer.clear();
  // To ensure we close MNS connection
  DestroyMnsObexConnection();
}

bool BluetoothMapSmsManager::IsConnected() { return mMasConnected; }

void BluetoothMapSmsManager::GetAddress(BluetoothAddress& aDeviceAddress) {
  if (mMasSocket) {
    return mMasSocket->GetAddress(aDeviceAddress);
  } else if (mMasRfcommSocket) {
    return mMasRfcommSocket->GetAddress(aDeviceAddress);
  }
}

bool BluetoothMapSmsManager::ReplyToConnect() {
  if (mMasConnected) {
    return true;
  }

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10;  // version=1.0
  req[4] = 0x00;  // flag=0x00
  BigEndian::writeUint16(&req[5], BluetoothMapSmsManager::MAX_PACKET_LENGTH);

  // Section 6.4 "Establishing an OBEX Session", MapSms 1.2
  // Headers: [Who:16][Connection ID]
  index += AppendHeaderWho(&req[index], 255, kMapMasObexTarget.mUuid,
                           sizeof(BluetoothUuid));
  index += AppendHeaderConnectionId(&req[index], 0x01);

  return SendMasObexData(req, ObexResponseCode::Success, index);
}

void BluetoothMapSmsManager::ReplyToDisconnectOrAbort() {
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
  // The format of response packet of "Disconnect" and "Abort" are the same
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, ObexResponseCode::Success, index);
}

void BluetoothMapSmsManager::ReplyToSetPath() {
  if (!mMasConnected) {
    BT_LOGR("Can't reply to set-path when MAS isn't connected.");
    return;
  }

  // Section 3.3.6 "SetPath", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, ObexResponseCode::Success, index);
}

bool BluetoothMapSmsManager::ReplyToGetWithHeaderBody(
    UniquePtr<uint8_t[]> aResponse, unsigned int aIndex) {
  if (!mMasConnected) {
    return false;
  }

  /**
   * This response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * - Part 2a: [headerId:1][length:2][EndOfBody:0]
   *   or
   * - Part 2b: [headerId:1][length:2][Body:var]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index for them here
  uint64_t bytesAvailable = 0;
  nsresult rv = mMasDataStream->Available(&bytesAvailable);
  if (NS_FAILED(rv)) {
    BT_LOGR("Failed to get available bytes from input stream. rv=0x%x",
            static_cast<uint32_t>(rv));
    return false;
  }

  aIndex += AppendHeaderConnectionId(&aResponse[aIndex], 0x01);

  if (sSrmEnabled && !sFirstSrmHeaderSent) {
    aIndex += AppendHeaderSRM(&aResponse[aIndex], true);
  }

  /* In practice, some platforms can only handle zero length End-of-Body
   * header separately with Body header.
   * Thus, append End-of-Body only if the data stream had been sent out,
   * otherwise, send 'Continue' to request for next GET request.
   */
  unsigned int opcode;
  if (!bytesAvailable) {
    // ----  Part 2a: [headerId:1][length:2][EndOfBody:0] ---- //
    aIndex += AppendHeaderEndOfBody(&aResponse[aIndex]);

    // Close input stream
    mMasDataStream->Close();
    mMasDataStream = nullptr;

    opcode = ObexResponseCode::Success;
  } else {
    // ---- Part 2b: [headerId:1][length:2][Body:var] ---- //
    MOZ_ASSERT(mMasDataStream);

    // Compute remaining packet size to append Body, excluding Body's header
    uint32_t remainingPacketSize = mRemoteMaxPacketLength - aIndex;

    // Read blob data from input stream
    uint32_t numRead = 0;
    auto buf = MakeUnique<char[]>(remainingPacketSize);

    // Exclude body header.
    nsresult rv = mMasDataStream->Read(
        buf.get(), remainingPacketSize - kObexBodyHeaderSize, &numRead);
    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to read from input stream. rv=0x%x",
              static_cast<uint32_t>(rv));
      return false;
    }

    // |numRead| must be non-zero
    MOZ_ASSERT(numRead);

    BT_LOGD("ReplyToGetWithHeaderBody buffer: %s", buf.get());

    // Include header size => remainingPacketSize + kObexBodyHeaderSize
    aIndex += AppendHeaderBody(&aResponse[aIndex], remainingPacketSize,
                               reinterpret_cast<uint8_t*>(buf.get()), numRead);

    opcode = ObexResponseCode::Continue;
  }

  return SendMasObexData(std::move(aResponse), opcode, aIndex);
}

void BluetoothMapSmsManager::SendPutFinalRequest() {
  if (!mMnsConnected) {
    return;
  }

  /**
   * Section 2.2.9, "End-of-Body", IrObex 1.2
   * End-of-Body is used to identify the last chunk of the object body.
   * For most platforms, a PutFinal packet is sent with an zero length
   * End-of-Body header.
   */
  // [opcode:1][length:2]
  int index = 3;
  auto req = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);

  index += AppendHeaderConnectionId(&req[index], mConnectionId);

  if (sSrmEnabled) {
    index += AppendHeaderSRM(&req[index], true);
  }

  index += AppendHeaderEndOfBody(&req[index]);

  SendMnsObexData(req.get(), ObexRequestCode::PutFinal, index);
}

bool BluetoothMapSmsManager::SendMessageEvent(uint8_t aMasId, BlobImpl* aBlob) {
  if (!mMnsConnected) {
    return false;
  }

  // Check if Body/EndOfBody is required.
  if (!GetInputStreamFromBlob(aBlob, false)) {
    SendReply(ObexResponseCode::InternalServerError);
    return false;
  }

  // [opcode:1][length:2][connectionId:1][type:21][appParameter:3]
  // [Body/EndofBody:var]
  auto req = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);

  int index = kObexRespHeaderSize;
  uint8_t appParameter[3];

  index += AppendHeaderConnectionId(&req[index], mConnectionId);
  const char* type = "x-bt/MAP-event-report";
  index += AppendHeaderType(&req[index], mRemoteMaxPacketLength,
                            reinterpret_cast<uint8_t*>(const_cast<char*>(type)),
                            strlen(type) + 1);
  uint8_t masId[1];
  masId[0] = aMasId;

  AppendAppParameter(appParameter, sizeof(appParameter),
                     (uint8_t)Map::AppParametersTagId::MASInstanceId, masId,
                     sizeof(uint8_t));

  index += AppendHeaderAppParameters(&req[index], mRemoteMaxPacketLength,
                                     appParameter, sizeof(appParameter));

  // Compute remaining packet size to append Body, excluding Body's header
  // (index already starts with obex header body size.
  uint32_t remainingPacketSize = mRemoteMaxPacketLength - index;

  BT_LOGD("SendMessageEvent: remote max packet len: %d",
          mRemoteMaxPacketLength);
  BT_LOGD("SendMessageEvent: %d", index);
  BT_LOGD("SendMessageEvent: body header size: %d", kObexBodyHeaderSize);
  BT_LOGD("SendMessageEvent: remaining packet size: %d", remainingPacketSize);

  // Read messages-event-report object data from input stream
  uint32_t numRead = 0;
  auto buf = MakeUnique<char[]>(remainingPacketSize);
  // read buffer size must cut header size
  nsresult rv = mMnsDataStream->Read(
      buf.get(), remainingPacketSize - kObexBodyHeaderSize, &numRead);

  BT_LOGD("mns datastream buffer: %s, numRead: %d, buffer size: %d", buf.get(),
          numRead, remainingPacketSize - kObexBodyHeaderSize);

  if (NS_FAILED(rv)) {
    BT_LOGR("Failed to read from input stream. rv=0x%x",
            static_cast<uint32_t>(rv));
    return false;
  }

  // fallback for MAP 1.1
  if (mIsMnsRfcomm) {
    std::string bufferAsString(buf.get(), numRead);

    // Find the substring to replace
    std::string oldVersion = "<MAP-event-report version = \"1.1\">";
    std::string newVersion = "<MAP-event-report version = \"1.0\">";

    size_t pos = bufferAsString.find(oldVersion);
    if (pos != std::string::npos) {
      // Replace the found substring with newVersion
      bufferAsString.replace(pos, oldVersion.size(), newVersion);
    }

    std::copy(bufferAsString.begin(), bufferAsString.end(), buf.get());
  }

  // |numRead| must be non-zero
  MOZ_ASSERT(numRead);

  // Part 2b: [headerId:1][length:2][Body:var]
  index += AppendHeaderBody(&req[index], remainingPacketSize,
                            reinterpret_cast<uint8_t*>(buf.get()), numRead);

  SendMnsObexData(req.get(), ObexRequestCode::Put, index);

  return true;
}

bool BluetoothMapSmsManager::MnsPutMultiRequest() {
  uint64_t bytesAvailable = 0;
  nsresult rv = mMnsDataStream->Available(&bytesAvailable);

  if (NS_FAILED(rv)) {
    BT_LOGR("Failed to get available bytes from input stream. rv=0x%x",
            static_cast<uint32_t>(rv));
    return false;
  }

  if (!bytesAvailable) {
    SendPutFinalRequest();
  } else {
    auto req = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);

    /* [request code:1][length:2] will be set in |SendObexData|.
     * Reserve index for them here
     */
    unsigned int index = kObexRespHeaderSize;

    // Compute remaining packet size to append Body, excluding Body's header
    uint32_t remainingPacketSize = mRemoteMaxPacketLength - index;

    BT_LOGD("MnsPutMultiRequest: remote max packet len: %d",
            mRemoteMaxPacketLength);
    BT_LOGD("MnsPutMultiRequest: %d", index);
    BT_LOGD("MnsPutMultiRequest: body header size: %d", kObexBodyHeaderSize);
    BT_LOGD("MnsPutMultiRequest: remaining packet size: %d",
            remainingPacketSize);

    // Read blob data from input stream
    uint32_t numRead = 0;
    auto buf = MakeUnique<char[]>(remainingPacketSize);
    rv = mMnsDataStream->Read(
        buf.get(), remainingPacketSize - kObexBodyHeaderSize, &numRead);
    BT_LOGD("Mns buffer: %s, numRead: %d, buffer size: %d", buf.get(), numRead,
            remainingPacketSize - kObexBodyHeaderSize);

    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to read from input stream. rv=0x%x",
              static_cast<uint32_t>(rv));
      return false;
    }

    // |numRead| must be non-zero
    MOZ_ASSERT(numRead);

    // Part 2b: [headerId:1][length:2][Body:var]
    index += AppendHeaderBody(&req[index], remainingPacketSize,
                              reinterpret_cast<uint8_t*>(buf.get()), numRead);

    SendMnsObexData(req.get(), ObexRequestCode::Put, index);
  }

  return true;
}

void BluetoothMapSmsManager::ReplyToPut(uint8_t aResponse) {
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.3.2 "PutResponse", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[mRemoteMaxPacketLength];
  int index = kObexRespHeaderSize;

  if (sSrmEnabled) {
    BT_LOGD("SendPutFinalRequest AppendHeaderSRM");
    index += AppendHeaderSRM(&req[kObexRespHeaderSize], true);
  }

  SendMasObexData(req, aResponse, index);
}

bool BluetoothMapSmsManager::ReplyToFolderListing(
    uint8_t aMasId, const nsAString& aFolderlists) {
  // TODO: Implement this for future Email support
  return false;
}

bool BluetoothMapSmsManager::ReplyToMessagesListing(uint8_t aMasId,
                                                    BlobImpl* aBlob,
                                                    bool aNewMessage,
                                                    const nsAString& aTimestamp,
                                                    int aSize) {
  if (!mMasConnected) {
    BT_LOGR("Can't reply to message-listing when MAS isn't connected.");
    return false;
  }

  /* If the response code is 0x90 or 0xA0, response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * - Part 2: [headerId:1][length:2][appParam:var]
   *   where [appParam:var] includes:
   *   [NewMessage:3] = [tagId:1][length:1][value:1]
   *   [MseTime:var] = [tagId:1][length:1][value:var]
   *   [MessageListingSize:4] = [tagId:1][length:1][value:2]
   * If mBodyRequired is true,
   * - Part 3: [headerId:1][length:2][Body:var]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index here
  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  unsigned int index = kObexRespHeaderSize;

  // ---- Part 2: headerId:1][length:2][appParam:var] ---- //
  // MSETime - String with the current time basis and UTC-offset of the MSE
  nsCString timestampStr = NS_ConvertUTF16toUTF8(aTimestamp);
  const uint8_t* str = reinterpret_cast<const uint8_t*>(timestampStr.get());
  uint8_t len = timestampStr.Length();

  // Total length: [NewMessage:3] + [MseTime:var] + [MessageListingSize:4]
  auto appParameters = MakeUnique<uint8_t[]>(len + 9);
  uint8_t newMessage = aNewMessage ? 1 : 0;

  AppendAppParameter(&appParameters[0], 3,
                     (uint8_t)Map::AppParametersTagId::NewMessage, &newMessage,
                     sizeof(newMessage));

  AppendAppParameter(&appParameters[3], len + 2,
                     (uint8_t)Map::AppParametersTagId::MSETime, str, len);

  uint8_t msgListingSize[2];
  BigEndian::writeUint16(&msgListingSize[0], aSize);

  AppendAppParameter(&appParameters[5 + len], 4,
                     (uint8_t)Map::AppParametersTagId::MessagesListingSize,
                     msgListingSize, sizeof(msgListingSize));

  index += AppendHeaderAppParameters(&res[index], mRemoteMaxPacketLength,
                                     appParameters.get(), len + 9);

  bool ret = false;

  if (mBodyRequired) {
    // Open input stream only if |mBodyRequired| is true
    if (!GetInputStreamFromBlob(aBlob, true)) {
      BT_LOGR("Failed to reply to message listing due to the invalid blob");
      SendReply(ObexResponseCode::InternalServerError);
      return false;
    }

    // ---- Part 3: [headerId:1][length:2][Body:var] ---- //
    ret = ReplyToGetWithHeaderBody(std::move(res), index);

    // The first response of msg listing
    if (sSrmpWaitEnabled) {
      sFirstSrmHeaderSent = true;
    }

    // For SRM, directly reply the rest of packets
    while (mMasDataStream && sSrmEnabled) {
      if (sSrmpWaitEnabled && !sSrmpWaitSingleReply) {
        BT_LOGR("SRMP enabled. DO NOT send packets continue");
        sSrmpWaitSingleReply = false;
        break;
      }
      auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
      if (!ReplyToGetWithHeaderBody(std::move(res), kObexRespHeaderSize)) {
        BT_LOGR("Failed to reply to MAP GET request.");
        SendReply(ObexResponseCode::InternalServerError);
        sSrmpWaitSingleReply = false;
      }

      sSrmpWaitSingleReply = false;
    }

    // Reset flag
    mBodyRequired = false;
  } else {
    ret = SendMasObexData(std::move(res), ObexResponseCode::Success, index);
  }

  return ret;
}

bool BluetoothMapSmsManager::ReplyToGetMessage(uint8_t aMasId,
                                               BlobImpl* aBlob) {
  if (!GetInputStreamFromBlob(aBlob, true)) {
    BT_LOGR("Failed to reply to get-message due to the invalid blob");
    SendReply(ObexResponseCode::InternalServerError);
    return false;
  }

  /*
   * If the response code is 0x90 or 0xA0, response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * If mFractionDeliverRequired is true,
   * - Part 2: [headerId:1][length:2][appParameters:3]
   * - Part 3: [headerId:1][length:2][Body:var]
   *   where [appParameters] includes:
   *   [FractionDeliver:3] = [tagId:1][length:1][value: 1]
   * otherwise,
   * - Part 2: [headerId:1][length:2][appParameters:3]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index here
  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  unsigned int index = kObexRespHeaderSize;

  if (mFractionDeliverRequired) {
    // ---- Part 2: [headerId:1][length:2][appParam:3] ---- //
    uint8_t appParameters[3];
    // TODO: Support FractionDeliver, reply "1(last)" now.
    uint8_t fractionDeliver = 1;
    AppendAppParameter(appParameters, sizeof(appParameters),
                       (uint8_t)Map::AppParametersTagId::FractionDeliver,
                       &fractionDeliver, sizeof(fractionDeliver));

    index += AppendHeaderAppParameters(&res[index], mRemoteMaxPacketLength,
                                       appParameters, sizeof(appParameters));
  }

  // TODO: Support bMessage encoding in bug 1166652.
  // ---- Part 3: [headerId:1][length:2][Body:var] ---- //
  ReplyToGetWithHeaderBody(std::move(res), index);

  // For SRM, directly reply the rest of packets, except for SRMP with wait
  // parameter.
  while (mMasDataStream && sSrmEnabled) {
    auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
    if (!ReplyToGetWithHeaderBody(std::move(res), kObexRespHeaderSize)) {
      BT_LOGR("Failed to reply to MAP GET request.");
      SendReply(ObexResponseCode::InternalServerError);
    }
  }

  mFractionDeliverRequired = false;

  return true;
}

bool BluetoothMapSmsManager::ReplyToSendMessage(uint8_t aMasId,
                                                const nsAString& aHandleId,
                                                bool aStatus) {
  if (!mMasConnected) {
    BT_LOGR("Can't reply to send-message when MAS isn't connected.");
    return false;
  }

  if (!aStatus) {
    BT_LOGR("Failed to reply to send-message.");
    return SendReply(ObexResponseCode::InternalServerError);
  }

  /* Handle is mandatory if the response code is success (0x90 or 0xA0).
   * The Name header shall be used to contain the handle that was assigned by
   * the MSE device to the message that was pushed by the MCE device.
   * The handle shall be represented by a null-terminated Unicode text strings
   * with 16 hexadecimal digits.
   */
  int len = aHandleId.Length();
  auto handleId = MakeUnique<uint8_t[]>((len + 1) * 2);

  for (int i = 0; i < len; i++) {
    BigEndian::writeUint16(&handleId[i * 2], aHandleId[i]);
  }
  BigEndian::writeUint16(&handleId[len * 2], 0);

  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  int index = kObexRespHeaderSize;
  index += AppendHeaderName(&res[index], mRemoteMaxPacketLength - index,
                            handleId.get(), (len + 1) * 2);
  SendMasObexData(std::move(res), ObexResponseCode::Success, index);

  return true;
}

bool BluetoothMapSmsManager::ReplyToSetMessageStatus(uint8_t aMasId,
                                                     bool aStatus) {
  return SendReply(aStatus ? ObexResponseCode::Success
                           : ObexResponseCode::InternalServerError);
}

bool BluetoothMapSmsManager::ReplyToMessageUpdate(uint8_t aMasId,
                                                  bool aStatus) {
  return SendReply(aStatus ? ObexResponseCode::Success
                           : ObexResponseCode::InternalServerError);
}

void BluetoothMapSmsManager::CreateMnsObexConnection() {
  if (mMnsSocket) {
    return;
  }

  auto cb = [this](BluetoothAddress& aDeviceAddress,
                   std::vector<RefPtr<BluetoothSdpRecord>>& aSdpRecord) {
    this->SdpSearchResultHandle(aDeviceAddress, aSdpRecord);
  };

  BluetoothSdpManager::SdpSearch(mDeviceAddress, kMapMns, cb);

  mMnsSocket = new BluetoothSocket(this);
}

void BluetoothMapSmsManager::DestroyMnsObexConnection() {
  if (!mMnsSocket) {
    return;
  }

  SendMnsDisconnectRequest();
}

void BluetoothMapSmsManager::SendMnsConnectRequest() {
  MOZ_ASSERT(mMnsSocket);

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10;  // version=1.0
  req[4] = 0x00;  // flag=0x00
  req[5] = BluetoothMapSmsManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothMapSmsManager::MAX_PACKET_LENGTH;

  index += AppendHeaderTarget(&req[index], 255, kMapMnsObexTarget.mUuid,
                              sizeof(BluetoothUuid));
  SendMnsObexData(req, ObexRequestCode::Connect, index);
  mLastCommand = ObexRequestCode::Connect;
  // Notify system app for backward compatiblity
  NotifyMapVersion(mIsMnsRfcomm);
}

void BluetoothMapSmsManager::SendMnsDisconnectRequest() {
  MOZ_ASSERT(mMnsSocket);

  if (!mMasConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  index += AppendHeaderConnectionId(&req[index], mConnectionId);
  SendMnsObexData(req, ObexRequestCode::Disconnect, index);
}

void BluetoothMapSmsManager::HandleSmsMmsFolderListing(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mMasConnected) {
    return;
  }

  /* MAP specification 5.4.3.1
   * The maximum number of entries shall be 1,024 if this header is not
   * specified.
   */
  uint16_t maxListCount = 1024;

  uint8_t buf[64];
  if (aHeader.GetAppParameter(Map::AppParametersTagId::MaxListCount, buf, 64)) {
    maxListCount = BigEndian::readUint16(buf);
  }

  uint16_t startOffset = 0;
  if (aHeader.GetAppParameter(Map::AppParametersTagId::StartOffset, buf, 64)) {
    startOffset = BigEndian::readUint16(buf);
  }

  // Folder listing size
  int foldersize = mCurrentFolder->GetSubFolderCount();

  uint8_t folderListingSizeValue[2];
  BigEndian::writeUint16(&folderListingSizeValue[0], foldersize);

  // Section 3.3.4 "GetResponse", IrOBEX 1.2
  // [opcode:1][length:2][FolderListingSize:4][Headers:var] where
  // Application Parameter [FolderListingSize:4] = [tagId:1][length:1][value: 2]
  uint8_t appParameter[4];
  AppendAppParameter(appParameter, sizeof(appParameter),
                     (uint8_t)Map::AppParametersTagId::FolderListingSize,
                     folderListingSizeValue, sizeof(folderListingSizeValue));

  auto resp = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  int index = 3;
  index += AppendHeaderAppParameters(&resp[index], 255, appParameter,
                                     sizeof(appParameter));

  /*
   * MCE wants to query sub-folder size FolderListingSize AppParameter shall
   * be used in the response if the value of MaxListCount in the request is 0.
   * If MaxListCount = 0, the MSE shall ignore all other applications
   * parameters that may be presented in the request. The response shall
   * contain any Body header.
   */
  if (maxListCount) {
    nsCString folderListingObject;
    mCurrentFolder->GetFolderListingObjectCString(folderListingObject,
                                                  maxListCount, startOffset);

    if (mMasDataStream) {
      mMasDataStream->Close();
      mMasDataStream = nullptr;
    }

    nsresult rv = NS_NewCStringInputStream(getter_AddRefs(mMasDataStream),
                                           folderListingObject);
    if (NS_FAILED(rv)) {
      BT_LOGR(
          "Failed to get internal stream from folder-listing object. rv=0x%x",
          static_cast<uint32_t>(rv));
      SendReply(ObexResponseCode::InternalServerError);
      return;
    }

    if (!ReplyToGetWithHeaderBody(std::move(resp), index)) {
      SendReply(ObexResponseCode::InternalServerError);
    }
  } else {
    SendMasObexData(std::move(resp), ObexResponseCode::Success, index);
  }
}

void BluetoothMapSmsManager::AppendBtNamedValueByTagId(
    const ObexHeaderSet& aHeader, nsTArray<BluetoothNamedValue>& aValues,
    const Map::AppParametersTagId aTagId) {
  uint8_t buf[64];
  if (!aHeader.GetAppParameter(aTagId, buf, 64)) {
    return;
  }

  /*
   * Follow MAP 6.3.1 Application Parameter Header
   */
  switch (aTagId) {
    case Map::AppParametersTagId::MaxListCount: {
      uint16_t maxListCount = BigEndian::readUint16(buf);
      /* MAP specification 5.4.3.1/5.5.4.1
       * If MaxListCount = 0, the response shall not contain the Body header.
       * The MSE shall ignore the request-parameters "ListStartOffset",
       * "SubjectLength" and "ParameterMask".
       */
      mBodyRequired = (maxListCount != 0);
      BT_LOGD("max list count: %d", maxListCount);
      AppendNamedValue(aValues, "maxListCount",
                       static_cast<uint32_t>(maxListCount));
      break;
    }
    case Map::AppParametersTagId::StartOffset: {
      uint16_t startOffset = BigEndian::readUint16(buf);
      BT_LOGD("start offset : %d", startOffset);
      AppendNamedValue(aValues, "startOffset",
                       static_cast<uint32_t>(startOffset));
      break;
    }
    case Map::AppParametersTagId::SubjectLength: {
      uint8_t subLength = *((uint8_t*)buf);
      BT_LOGD("msg subLength : %d", subLength);
      AppendNamedValue(aValues, "subLength", static_cast<uint32_t>(subLength));
      break;
    }
    case Map::AppParametersTagId::ParameterMask: {
      nsTArray<uint32_t> parameterMask = PackParameterMask(buf, 64);
      AppendNamedValue(aValues, "parameterMask", BluetoothValue(parameterMask));
      break;
    }
    case Map::AppParametersTagId::FilterMessageType: {
      /* Follow MAP 1.2, 6.3.1
       * 0000xxx1 = "SMS_GSM"
       * 0000xx1x = "SMS_CDMA"
       * 0000x1xx = "EMAIL"
       * 00001xxx = "MMS"
       * Where
       * 0 = "no filtering, get this type"
       * 1 = "filter out this type"
       */
      uint32_t filterMessageType = *((uint8_t*)buf);

      if (filterMessageType ==
              (FILTER_NO_EMAIL | FILTER_NO_MMS | FILTER_NO_SMS_GSM) ||
          filterMessageType ==
              (FILTER_NO_EMAIL | FILTER_NO_MMS | FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Sms);
      } else if (filterMessageType ==
                 (FILTER_NO_EMAIL | FILTER_NO_SMS_GSM | FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Mms);
      } else if (filterMessageType ==
                 (FILTER_NO_MMS | FILTER_NO_SMS_GSM | FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Email);
      } else {
        BT_LOGR("Unsupportted filter message type");
        filterMessageType = static_cast<uint32_t>(MessageType::Sms);
      }

      BT_LOGD("msg filterMessageType : %d", filterMessageType);
      AppendNamedValue(aValues, "filterMessageType",
                       static_cast<uint32_t>(filterMessageType));
      break;
    }
    case Map::AppParametersTagId::FilterPeriodBegin: {
      nsCString filterPeriodBegin((char*)buf);
      BT_LOGD("msg FilterPeriodBegin : %s", filterPeriodBegin.get());
      AppendNamedValue(aValues, "filterPeriodBegin",
                       NS_ConvertUTF8toUTF16(filterPeriodBegin));
      break;
    }
    case Map::AppParametersTagId::FilterPeriodEnd: {
      nsCString filterPeriodEnd((char*)buf);
      BT_LOGD("msg filterPeriodEnd : %s", filterPeriodEnd.get());
      AppendNamedValue(aValues, "filterPeriodEnd",
                       NS_ConvertUTF8toUTF16(filterPeriodEnd));
      break;
    }
    case Map::AppParametersTagId::FilterReadStatus: {
      using namespace mozilla::dom::ReadStatusValues;
      uint32_t filterReadStatus =
          buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;
      BT_LOGD("msg filter read status : %d", filterReadStatus);
      AppendNamedValue(aValues, "filterReadStatus", filterReadStatus);
      break;
    }
    case Map::AppParametersTagId::FilterRecipient: {
      // FilterRecipient encodes as UTF-8
      nsCString filterRecipient((char*)buf);
      BT_LOGD("msg filterRecipient : %s", filterRecipient.get());
      AppendNamedValue(aValues, "filterRecipient",
                       NS_ConvertUTF8toUTF16(filterRecipient));
      break;
    }
    case Map::AppParametersTagId::FilterOriginator: {
      // FilterOriginator encodes as UTF-8
      nsCString filterOriginator((char*)buf);
      BT_LOGD("msg filter Originator : %s", filterOriginator.get());
      AppendNamedValue(aValues, "filterOriginator",
                       NS_ConvertUTF8toUTF16(filterOriginator));
      break;
    }
    case Map::AppParametersTagId::FilterPriority: {
      using namespace mozilla::dom::PriorityValues;
      uint32_t filterPriority =
          buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGD("msg filter priority: %d", filterPriority);
      AppendNamedValue(aValues, "filterPriority", filterPriority);
      break;
    }
    case Map::AppParametersTagId::Attachment: {
      uint8_t attachment = *((uint8_t*)buf);
      BT_LOGD("msg filter attachment: %d", attachment);
      AppendNamedValue(aValues, "attachment",
                       static_cast<uint32_t>(attachment));
      break;
    }
    case Map::AppParametersTagId::Charset: {
      using namespace mozilla::dom::FilterCharsetValues;
      uint32_t filterCharset =
          buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGD("msg filter charset: %d", filterCharset);
      AppendNamedValue(aValues, "charset", filterCharset);
      break;
    }
    case Map::AppParametersTagId::FractionRequest: {
      mFractionDeliverRequired = true;
      AppendNamedValue(aValues, "fractionRequest", (buf[0] != 0));
      break;
    }
    case Map::AppParametersTagId::StatusIndicator: {
      using namespace mozilla::dom::StatusIndicatorsValues;
      uint32_t filterStatusIndicator =
          buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGD("msg filter statusIndicator: %d", filterStatusIndicator);
      AppendNamedValue(aValues, "statusIndicator", filterStatusIndicator);
      break;
    }
    case Map::AppParametersTagId::StatusValue: {
      uint8_t statusValue = *((uint8_t*)buf);
      BT_LOGD("msg filter statusvalue: %d", statusValue);
      AppendNamedValue(aValues, "statusValue",
                       static_cast<uint32_t>(statusValue));
      break;
    }
    case Map::AppParametersTagId::Transparent: {
      uint8_t transparent = *((uint8_t*)buf);
      BT_LOGD("msg filter statusvalue: %d", transparent);
      AppendNamedValue(aValues, "transparent",
                       static_cast<uint32_t>(transparent));
      break;
    }
    case Map::AppParametersTagId::Retry: {
      uint8_t retry = *((uint8_t*)buf);
      BT_LOGD("msg filter retry: %d", retry);
      AppendNamedValue(aValues, "retry", static_cast<uint32_t>(retry));
      break;
    }
    default:
      BT_LOGR("Unsupported AppParameterTag: %x", aTagId);
      break;
  }
}

nsTArray<uint32_t> BluetoothMapSmsManager::PackParameterMask(uint8_t* aData,
                                                             int aSize) {
  nsTArray<uint32_t> parameterMask;

  /* Table 6.5, MAP 6.3.1. ParameterMask is Bit 16-31 Reserved for future
   * use. The reserved bits shall be set to 0 by MCE and discarded by MSE.
   * convert big endian to little endian
   */
  uint32_t x = BigEndian::readUint32(aData);

  uint32_t count = 0;
  while (x) {
    if (x & 1) {
      parameterMask.AppendElement(count);
    }

    ++count;
    x >>= 1;
  }

  return parameterMask;
}

void BluetoothMapSmsManager::HandleSmsMmsMsgListing(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  // Section 5.5.2 "Name", MAP 1.2:
  // This property shall be used to indicate the folder from which the
  // Messages-Listing object is to be retrieved. The property shall be
  // empty in case the desired listing is that of the current folder or
  // shall be the name of a child folder.
  nsString name;
  aHeader.GetName(name);

  nsString currentFolderPath;
  mCurrentFolder->GetPath(currentFolderPath);

  // Sanity checks on folder path for enhancing fault tolerance.
  if (!name.IsEmpty() && currentFolderPath.Find(u"telecom/msg/"_ns) != -1) {
    BT_WARNING("The target folder of MAP-msg-listing is unsupproted.");
    // Go up 1 level
    BluetoothMapFolder* parent = mCurrentFolder->GetParentFolder();
    if (parent) {
      mCurrentFolder = parent;
      mCurrentFolder->GetPath(currentFolderPath);
    }
  }

  // Get the absolute path of the folder to be retrieved.
  name =
      name.IsEmpty() ? currentFolderPath : currentFolderPath + u"/"_ns + name;

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "name", name);

  {
    /* MAP specification 5.5.4.1
     * If 'MaxListCount'=0 in the request, the MSE shall respond with the
     * headers "NewMessage, MSETime”, and "MessagesListingSize" only.
     *
     * The maximum number of entries shall be 1,024 if 'MaxListCount' header is
     * not specified.
     */
    uint16_t maxListCount = 1024;

    uint8_t buf[64];
    if (aHeader.GetAppParameter(Map::AppParametersTagId::MaxListCount, buf,
                                64)) {
      maxListCount = BigEndian::readUint16(buf);

      BT_LOGD("max list count: %d", maxListCount);
    }

    // If MaxListCount = 0, the response shall not contain the Body header.
    mBodyRequired = (maxListCount != 0);

    AppendNamedValue(data, "maxListCount", static_cast<uint32_t>(maxListCount));
  }

  static Map::AppParametersTagId sMsgListingParameters[] = {
      [0] = Map::AppParametersTagId::StartOffset,
      [1] = Map::AppParametersTagId::SubjectLength,
      [2] = Map::AppParametersTagId::ParameterMask,
      [3] = Map::AppParametersTagId::FilterMessageType,
      [4] = Map::AppParametersTagId::FilterPeriodBegin,
      [5] = Map::AppParametersTagId::FilterPeriodEnd,
      [6] = Map::AppParametersTagId::FilterReadStatus,
      [7] = Map::AppParametersTagId::FilterRecipient,
      [8] = Map::AppParametersTagId::FilterOriginator,
      [9] = Map::AppParametersTagId::FilterPriority};

  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sMsgListingParameters); i++) {
    AppendBtNamedValueByTagId(aHeader, data, sMsgListingParameters[i]);
  }

  bs->DistributeSignal(MAP_MESSAGES_LISTING_REQ_ID, KEY_MAP, data);
}

void BluetoothMapSmsManager::HandleSmsMmsGetMessage(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString name;
  aHeader.GetName(name);

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "name", name);

  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Attachment);
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Charset);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::FractionRequest);

  bs->DistributeSignal(MAP_GET_MESSAGE_REQ_ID, KEY_MAP, data);
}

void BluetoothMapSmsManager::BuildDefaultFolderStructure() {
  /* MAP specification defines virtual folders structure
   * "" (root folder)
   * "telecom"
   * "telecom/msg"
   * "telecom/msg/inbox"
   * "telecom/msg/outbox"
   * "telecom/msg/sent"
   */
  mRootFolder = new BluetoothMapFolder(u""_ns, nullptr);
  BluetoothMapFolder* folder = mRootFolder->AddSubFolder(u"telecom"_ns);
  folder = folder->AddSubFolder(u"msg"_ns);

  // Add mandatory folders
  folder->AddSubFolder(u"inbox"_ns);
  folder->AddSubFolder(u"sent"_ns);
  folder->AddSubFolder(u"outbox"_ns);
  // TODO: Add 'draft' and 'deleted' folder once they are supported by frond end
  //       The task is tracked by CORE-3628.

  mCurrentFolder = mRootFolder;
}

void BluetoothMapSmsManager::HandleNotificationRegistration(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t buf[64];
  if (!aHeader.GetAppParameter(Map::AppParametersTagId::NotificationStatus, buf,
                               64)) {
    return;
  }

  bool ntfRequired = static_cast<bool>(buf[0]);
  if (mNtfRequired == ntfRequired) {
    // Ignore request
    return;
  }

  mNtfRequired = ntfRequired;
  /*
   * Initialization sequence for a MAP session that uses both the Messsage
   * Access service and the Message Notification service. The MNS connection
   * shall be established by the first SetNotificationRegistration set to ON
   * during MAP session. Only one MNS connection per device pair.
   * Section 6.4.2, MAP
   * If the Message Access connection is disconnected after Message Notification
   * connection establishment, this will automatically indicate a MAS
   * Notification-Deregistration for this MAS instance.
   */
  if (mNtfRequired) {
    CreateMnsObexConnection();
  } else {
    /*
     * TODO: we shall check multiple MAS instances unregister notification to
     * drop MNS connection, but now we only support SMS/MMS, so drop connection
     * directly.
     */
    DestroyMnsObexConnection();
  }
}

void BluetoothMapSmsManager::HandleSetMessageStatus(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  /* In section 5.7.2 "Name", MAP 1.2:
   * The Name header shall contain the handle of the message the status of which
   * shall be modified. The handle shall be represented by a null-terminated
   * Unicode text string with 16 hexadecimal digits.
   */
  nsString name;
  aHeader.GetName(name);

  nsresult rv;
  uint32_t handleId = static_cast<uint32_t>(name.ToInteger(&rv));

  if (NS_FAILED(rv)) {
    BT_LOGR("Failed to convert handleId, error: 0x%x",
            static_cast<uint32_t>(rv));
  }

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "handleId", handleId);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::StatusIndicator);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::StatusValue);

  bs->DistributeSignal(MAP_SET_MESSAGE_STATUS_REQ_ID, KEY_MAP, data);
}

void BluetoothMapSmsManager::HandleSmsMmsInstanceInfo(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  // 5.10 GetMASInstanceInformation
  // [opcode:1][length:2][Headers:var]
  uint8_t req[mRemoteMaxPacketLength];
  int index = kObexRespHeaderSize;

  if (sSrmEnabled) {
    index += AppendHeaderSRM(&req[kObexRespHeaderSize], true);
  }

  // Compute remaining packet size to append Body, excluding Body's header
  uint32_t remainingPacketSize = mRemoteMaxPacketLength - index;

  nsAutoCString masinfo("SMS/MMS");
  index += AppendHeaderEndOfBody(
      &req[index], remainingPacketSize,
      reinterpret_cast<const uint8_t*>(masinfo.get()), masinfo.Length());

  SendMasObexData(req, ObexResponseCode::Success, index);
}

void BluetoothMapSmsManager::ExtractBodyMultiPutPackets(
    const ObexHeaderSet& aHeader, uint8_t aOpcode) {
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aOpcode == ObexRequestCode::PutFinal) {
    mPutFinalFlag = true;
  }

  if (!aHeader.Has(ObexHeaderId::Body) &&
      !aHeader.Has(ObexHeaderId::EndOfBody)) {
    BT_LOGR("Error! Fail to find Body/EndOfBody. Ignore this push request");
    return;
  }

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "folderName", mPutFolderName);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Transparent);
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Retry);

  // TODO: Support native format charset (mandatory format).
  //
  // Charset indicates Gaia application how to deal with encoding.
  // - Native: If the message object shall be delivered without trans-coding.
  // - UTF-8:  If the message text shall be trans-coded to UTF-8.
  //
  // We only support UTF-8 charset due to current SMS API limitation.
  //
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Charset);

  // Get Body
  uint8_t* bodyPtr = nullptr;
  aHeader.GetBody(&bodyPtr, &mBodySegmentLength);
  mBodySegment.reset(bodyPtr);

  mMultiPutDataBuffer.insert(mMultiPutDataBuffer.end(), bodyPtr,
                             bodyPtr + mBodySegmentLength);

  BT_LOGD("Body size: %zu ", mMultiPutDataBuffer.size());

  if (!sSrmEnabled && aOpcode != ObexRequestCode::PutFinal) {
    // non SRM mode, we sent continue whatever we got
    BT_LOGR("Not enabled SRM, send continue.");
    ReplyToPut(ObexResponseCode::Continue);
    return;
  }

  if (mPutFinalFlag) {
    std::string s(mMultiPutDataBuffer.begin(), mMultiPutDataBuffer.end());
    const nsAutoCString body(s.data(), s.size());
    BT_LOGD("Body: %s", body.get());

    RefPtr<BluetoothMapBMessage> bmsg = new BluetoothMapBMessage(body);

    nsCString subject;
    bmsg->GetBody(subject);

    BT_LOGD("subject: %s", subject.get());
    // It's possible that subject is empty, send it anyway
    AppendNamedValue(data, "messageBody", subject);

    nsTArray<RefPtr<VCard>> recipients;
    bmsg->GetRecipients(recipients);

    // Get the topmost level, only one recipient for SMS case
    if (!recipients.IsEmpty()) {
      nsCString recipient;
      recipients[0]->GetTelephone(recipient);
      BT_LOGR("recipient %s", recipient.get());
      AppendNamedValue(data, "recipient", recipient);
    }

    bs->DistributeSignal(MAP_SEND_MESSAGE_REQ_ID, KEY_MAP, data);
  }
}

void BluetoothMapSmsManager::HandleRfcommPushMessage(
    const ObexHeaderSet& aHeader, uint8_t aOpcode) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (!aHeader.Has(ObexHeaderId::Body) &&
      !aHeader.Has(ObexHeaderId::EndOfBody)) {
    BT_LOGR("Error! Fail to find Body/EndOfBody. Ignore this push request");
    return;
  }

  // Section 5.8.2 "Name", MAP 1.2:
  // In a request: This property shall be used to indicate the folder to which
  // the Message object is to be pushed. The property shall be empty in case the
  // desired listing is that of the current folder or shall be the name of a
  // child folder.
  aHeader.GetName(mPutFolderName);

  // Get the absolute path of the folder to be pushed.
  nsString currentFolderPath;
  mCurrentFolder->GetPath(currentFolderPath);
  mPutFolderName = mPutFolderName.IsEmpty()
                       ? currentFolderPath
                       : currentFolderPath + u"/"_ns + mPutFolderName;

  // If the message will to be pushed to 'outbox' folder
  //   1. Parse body to get SMS
  //   2. Get receipent subject
  //   3. Send it to Gaia
  // Otherwise reply NotAcceptable error code.
  if (mPutFolderName.Find(u"outbox"_ns) == -1) {
    BT_LOGR("Can't push message to any folder other than 'outbox'");
    ReplyToPut(ObexResponseCode::NotAcceptable);
    return;
  }

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "folderName", mPutFolderName);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Transparent);
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Retry);

  // TODO: Support native format charset (mandatory format).
  //
  // Charset indicates Gaia application how to deal with encoding.
  // - Native: If the message object shall be delivered without trans-coding.
  // - UTF-8:  If the message text shall be trans-coded to UTF-8.
  //
  // We only support UTF-8 charset due to current SMS API limitation.
  //
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Charset);

  // Get Body
  uint8_t* bodyPtr = nullptr;
  aHeader.GetBody(&bodyPtr, &mBodySegmentLength);
  mBodySegment.reset(bodyPtr);

  // If the first PUT packet is the put final packet, fallthrough
  RefPtr<BluetoothMapBMessage> bmsg =
      new BluetoothMapBMessage(bodyPtr, mBodySegmentLength);

  nsCString subject;
  bmsg->GetBody(subject);
  // It's possible that subject is empty, send it anyway
  AppendNamedValue(data, "messageBody", subject);

  nsTArray<RefPtr<VCard>> recipients;
  bmsg->GetRecipients(recipients);

  // Get the topmost level, only one recipient for SMS case
  if (!recipients.IsEmpty()) {
    nsCString recipient;
    recipients[0]->GetTelephone(recipient);
    AppendNamedValue(data, "recipient", recipient);
  }

  // DO NOT SEND Success opcode manually. After signal issued, ReplyToSend will
  // do.
  bs->DistributeSignal(MAP_SEND_MESSAGE_REQ_ID, KEY_MAP, data);
}

void BluetoothMapSmsManager::HandleSmsMmsPushMessage(
    const ObexHeaderSet& aHeader, uint8_t aOpcode) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (!aHeader.Has(ObexHeaderId::Body) &&
      !aHeader.Has(ObexHeaderId::EndOfBody)) {
    BT_LOGR("Error! Fail to find Body/EndOfBody. Ignore this push request");
    return;
  }

  // Section 5.8.2 "Name", MAP 1.2:
  // In a request: This property shall be used to indicate the folder to which
  // the Message object is to be pushed. The property shall be empty in case the
  // desired listing is that of the current folder or shall be the name of a
  // child folder.
  aHeader.GetName(mPutFolderName);

  // Get the absolute path of the folder to be pushed.
  nsString currentFolderPath;
  mCurrentFolder->GetPath(currentFolderPath);
  mPutFolderName = mPutFolderName.IsEmpty()
                       ? currentFolderPath
                       : currentFolderPath + u"/"_ns + mPutFolderName;

  // If the message will to be pushed to 'outbox' folder
  //   1. Parse body to get SMS
  //   2. Get receipent subject
  //   3. Send it to Gaia
  // Otherwise reply NotAcceptable error code.
  if (mPutFolderName.Find(u"outbox"_ns) == -1) {
    BT_LOGR("Can't push message to any folder other than 'outbox'");
    ReplyToPut(ObexResponseCode::NotAcceptable);
    return;
  }

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "folderName", mPutFolderName);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Transparent);
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Retry);

  // TODO: Support native format charset (mandatory format).
  //
  // Charset indicates Gaia application how to deal with encoding.
  // - Native: If the message object shall be delivered without trans-coding.
  // - UTF-8:  If the message text shall be trans-coded to UTF-8.
  //
  // We only support UTF-8 charset due to current SMS API limitation.
  //
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Charset);

  // Get Body
  uint8_t* bodyPtr = nullptr;
  aHeader.GetBody(&bodyPtr, &mBodySegmentLength);
  mBodySegment.reset(bodyPtr);

  // The first PUT packet body section stored into buffer.
  mMultiPutDataBuffer.insert(mMultiPutDataBuffer.end(), bodyPtr,
                             bodyPtr + mBodySegmentLength);
  BT_LOGD("[BODY] The first PUT packet body length: %d", mBodySegmentLength);

  if (sSrmEnabled && aOpcode != ObexRequestCode::PutFinal) {
    BT_LOGR(
        "HandleSmsMmsPushMessage SRM enabled and not PutFinal, packet is NOT "
        "complete");
    // Reply continue for futher data due to SRM mode enabled.
    if (sSrmEnabled && sFirstPutResponse) {
      BT_LOGR(
          "HandleSmsMmsPushMessage SRM enabled and not PutFinal, Send "
          "Continue");
      ReplyToPut(ObexResponseCode::Continue);
      sFirstPutResponse = false;
    }

    return;
  }

  if (!sSrmEnabled) {
    // Non SRM mode, we sent continue whatever we got
    BT_LOGR("Not enabled SRM, send OK.");
    ReplyToPut(ObexResponseCode::Success);

    if (aOpcode != ObexRequestCode::PutFinal) {
      return;
    }
  }

  std::string s(mMultiPutDataBuffer.begin(), mMultiPutDataBuffer.end());
  const nsAutoCString body(s.data(), s.size());
  BT_LOGD("Body: %s, length: %zu", body.get(), body.Length());

  // If the first PUT packet is the put final packet, fallthrough
  RefPtr<BluetoothMapBMessage> bmsg = new BluetoothMapBMessage(body);

  nsCString subject;
  bmsg->GetBody(subject);
  // It's possible that subject is empty, send it anyway
  AppendNamedValue(data, "messageBody", subject);

  nsTArray<RefPtr<VCard>> recipients;
  bmsg->GetRecipients(recipients);

  // Get the topmost level, only one recipient for SMS case
  if (!recipients.IsEmpty()) {
    nsCString recipient;
    recipients[0]->GetTelephone(recipient);
    AppendNamedValue(data, "recipient", recipient);
  }

  // DO NOT SEND Success opcode manually. After signal issued, ReplyToSend will
  // do.
  bs->DistributeSignal(MAP_SEND_MESSAGE_REQ_ID, KEY_MAP, data);
}

bool BluetoothMapSmsManager::GetInputStreamFromBlob(BlobImpl* aBlob,
                                                    bool aIsMas) {
  if (mMasDataStream && aIsMas) {
    mMasDataStream->Close();
    mMasDataStream = nullptr;
  } else if (mMnsDataStream && !aIsMas) {
    mMnsDataStream->Close();
    mMnsDataStream = nullptr;
  }

  ErrorResult rv;
  if (aIsMas) {
    aBlob->CreateInputStream(getter_AddRefs(mMasDataStream), rv);
  } else {
    aBlob->CreateInputStream(getter_AddRefs(mMnsDataStream), rv);
  }

  if (rv.Failed()) {
    BT_LOGR("Failed to get internal stream from blob. rv=0x%x",
            rv.ErrorCodeAsInt());
    return false;
  }

  return true;
}

bool BluetoothMapSmsManager::SendReply(uint8_t aResponseCode) {
  BT_LOGD("[0x%x]", aResponseCode);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[kObexRespHeaderSize];

  return SendMasObexData(req, aResponseCode, kObexRespHeaderSize);
}

bool BluetoothMapSmsManager::SendMasObexData(uint8_t* aData, uint8_t aOpcode,
                                             int aSize) {
  if (mMasSocket) {
    SetObexPacketInfo(aData, aOpcode, aSize);
    mMasSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
  } else if (mMasRfcommSocket) {
    SetObexPacketInfo(aData, aOpcode, aSize);
    mMasRfcommSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
  } else {
    BT_LOGR("Can't send data. Rfcomm/L2cap socket connection doesn't exist!");
    return false;
  }

  return true;
}

bool BluetoothMapSmsManager::SendMasObexData(UniquePtr<uint8_t[]> aData,
                                             uint8_t aOpcode, int aSize) {
  if (mMasSocket) {
    SetObexPacketInfo(aData.get(), aOpcode, aSize);
    mMasSocket->SendSocketData(new UnixSocketRawData(std::move(aData), aSize));
  } else if (mMasRfcommSocket) {
    SetObexPacketInfo(aData.get(), aOpcode, aSize);
    mMasRfcommSocket->SendSocketData(
        new UnixSocketRawData(std::move(aData), aSize));
  } else {
    BT_LOGR("Can't send data. Rfcomm/L2cap socket connection doesn't exist!");
    return false;
  }

  return true;
}

void BluetoothMapSmsManager::SendMnsObexData(uint8_t* aData, uint8_t aOpcode,
                                             int aSize) {
  mLastCommand = aOpcode;
  SetObexPacketInfo(aData, aOpcode, aSize);
  if (mMnsSocket) {
    mMnsSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
  }
}

void BluetoothMapSmsManager::OnSocketConnectSuccess(BluetoothSocket* aSocket) {
  MOZ_ASSERT(aSocket);

  // MNS socket is connected
  if (aSocket == mMnsSocket) {
    mMnsConnected = true;
    SendMnsConnectRequest();
    return;
  }

  if ((aSocket == mMasServerSocket) && mMasServerSocket) {
    BT_LOGR("L2CAP server socket connected");
    // MAS socket is connected
    // Close server socket as only one session is allowed at a time
    mMasServerSocket.swap(mMasSocket);

    // Cache device address since we can't get socket address when a remote
    // device disconnect with us.
    mMasSocket->GetAddress(mDeviceAddress);
  }

  if ((aSocket == mMasRfcommServerSocket) && mMasRfcommServerSocket) {
    BT_LOGR("RFCOMM server socket connected");
    // MAS socket is connected
    // Close server socket as only one session is allowed at a time
    mMasRfcommServerSocket.swap(mMasRfcommSocket);

    // Cache device address since we can't get socket address when a remote
    // device disconnect with us.
    mMasRfcommSocket->GetAddress(mDeviceAddress);
  }
}

void BluetoothMapSmsManager::OnSocketConnectError(BluetoothSocket* aSocket) {
  // MNS socket connection error
  if (aSocket == mMnsSocket) {
    mMnsConnected = false;
    mMnsSocket = nullptr;
    return;
  }

  // MAS socket connection error

  if (mMasServerSocket &&
      mMasServerSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasServerSocket->Close();
  }
  mMasServerSocket = nullptr;

  if (mMasSocket && mMasSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasSocket->Close();
  }

  if (mMasRfcommServerSocket &&
      mMasRfcommServerSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasRfcommServerSocket->Close();
  }
  mMasRfcommServerSocket = nullptr;

  if (mMasRfcommSocket &&
      mMasRfcommSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    mMasRfcommSocket->Close();
  }

  mMasSocket = nullptr;
  mMasRfcommSocket = nullptr;
  sSrmEnabled = false;
  sFirstSrmHeaderSent = false;
}

void BluetoothMapSmsManager::OnSocketDisconnect(BluetoothSocket* aSocket) {
  MOZ_ASSERT(aSocket);

  if (mMasDataStream) {
    mMasDataStream->Close();
    mMasDataStream = nullptr;
  }

  // MNS socket is disconnected
  if (aSocket == mMnsSocket) {
    mMnsConnected = false;
    mMnsSocket = nullptr;
    mNtfRequired = false;
    mConnectionId = 0xFFFFFFFF;
    BT_LOGR("MNS socket disconnected");
    return;
  }

  // MAS server socket is closed
  if (aSocket != mMasSocket && aSocket != mMasRfcommSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  // MAS socket is disconnected
  AfterMapSmsDisconnected();
  mDeviceAddress.Clear();

  mMasSocket = nullptr;
  mMasRfcommSocket = nullptr;
  BT_LOGD("Socket disconnected set socket null");

  sSrmEnabled = false;
  sFirstSrmHeaderSent = false;
  sSrmpWaitEnabled = false;
  sSrmpWaitSingleReply = false;
  mReceivedRfcommLength = 0;
  mObexTotalLength = 0;

  BT_LOGR("Re-listen");
  Listen();
}

void BluetoothMapSmsManager::Disconnect(
    BluetoothProfileController* aController) {
  if (!mMasSocket && !mMasRfcommSocket) {
    BT_WARNING("%s: No ongoing connection to disconnect", __FUNCTION__);
    return;
  }

  if (mMasSocket) {
    BT_LOGD("Mas l2cap socket close");
    mMasSocket->Close();
  } else if (mMasRfcommSocket) {
    BT_LOGD("Mas rfcomm socket close");
    mMasRfcommSocket->Close();
  } else {
    BT_LOGR("No Mas socket could be to close.");
  }
}

NS_IMPL_ISUPPORTS(BluetoothMapSmsManager, nsIObserver)

void BluetoothMapSmsManager::Connect(const BluetoothAddress& aDeviceAddress,
                                     BluetoothProfileController* aController) {
  MOZ_ASSERT(false);
}

void BluetoothMapSmsManager::OnGetServiceChannel(
    const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aServiceUuid,
    int aChannel) {
  MOZ_ASSERT(false);
}

void BluetoothMapSmsManager::OnUpdateSdpRecords(
    const BluetoothAddress& aDeviceAddress) {
  auto cb = [this](BluetoothAddress& aDeviceAddress,
                   std::vector<RefPtr<BluetoothSdpRecord>>& aSdpRecord) {
    this->SdpSearchResultHandle(aDeviceAddress, aSdpRecord);
  };

  BluetoothSdpManager::SdpSearch(mDeviceAddress, kMapMns, cb);
}

void BluetoothMapSmsManager::OnConnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(false);
}

void BluetoothMapSmsManager::OnDisconnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(false);
}

void BluetoothMapSmsManager::Reset() { MOZ_ASSERT(false); }

END_BLUETOOTH_NAMESPACE
