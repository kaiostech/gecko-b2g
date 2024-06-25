/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPbapManager.h"
#include "BluetoothSdpManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuidHelper.h"

#include "mozilla/dom/BluetoothPbapParametersBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIInputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
// UUID of PBAP PSE
static const BluetoothUuid kPbapPSE(PBAP_PSE);

// UUID used in PBAP OBEX target header
static const BluetoothUuid kPbapObexTarget(0x79, 0x61, 0x35, 0xF0, 0xF0, 0xC5,
                                           0x11, 0xD8, 0x09, 0x66, 0x08, 0x00,
                                           0x20, 0x0C, 0x9A, 0x66);

// App parameters to pull phonebook
static const AppParameterTag sPhonebookTags[] = {
    AppParameterTag::Format, AppParameterTag::PropertySelector,
    AppParameterTag::MaxListCount, AppParameterTag::ListStartOffset,
    AppParameterTag::vCardSelector};

// App parameters to pull vCard listing
static const AppParameterTag sVCardListingTags[] = {
    AppParameterTag::Order,           AppParameterTag::SearchValue,
    AppParameterTag::SearchProperty,  AppParameterTag::MaxListCount,
    AppParameterTag::ListStartOffset, AppParameterTag::vCardSelector};

// App parameters to pull vCard entry
static const AppParameterTag sVCardEntryTags[] = {
    AppParameterTag::Format, AppParameterTag::PropertySelector};

StaticRefPtr<BluetoothPbapManager> sPbapManager;
static int sSdpPbapHandle = -1;

// `sPbapObexSrmEnabled` means that PBAP has received an SRM header,
// but it does not indicate that the SRM has already been activated,
// as PBAP needs to check the SRMP in the future.
static bool sPbapObexSrmEnabled = false;

// `sPbapObexSrmActive` represents that the SRM mode has been activated.
// It will send PBAP data to PCE with no-response mode.
static bool sPbapObexSrmActive  = false;

static bool sPbapObexSrmResposeRequestSend = false;

static const int sSrmProcessScheduleTime = 100;
}  // namespace

BEGIN_BLUETOOTH_NAMESPACE

bool BluetoothPbapManager::sInShutdown = false;

NS_IMETHODIMP
BluetoothPbapManager::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  MOZ_ASSERT(sPbapManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "PbapManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void BluetoothPbapManager::HandleShutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  sInShutdown = true;
  Disconnect(nullptr);
  Uninit();

  sPbapManager = nullptr;
}

BluetoothPbapManager::BluetoothPbapManager()
    : mPhonebookSizeRequired(false),
      mNewMissedCallsRequired(false),
      mConnected(false),
      mPasswordReqNeeded(false),
      mRemoteMaxPacketLength(0) {
  mCurrentPath.AssignLiteral("");
}

BluetoothPbapManager::~BluetoothPbapManager() {}

nsresult BluetoothPbapManager::Init() {
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

void BluetoothPbapManager::Uninit() {
  BluetoothSocket::Uninit(mRfcommServerSocket);
  mRfcommServerSocket = nullptr;

  BluetoothSocket::Uninit(mL2capServerSocket);
  mL2capServerSocket = nullptr;

  BluetoothSocket::Uninit(mSocket);
  mSocket = nullptr;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  Unused << NS_WARN_IF(
      NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)));
}

// static
void BluetoothPbapManager::InitPbapInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aRes) {
    aRes->Init();
  }
}

// static
void BluetoothPbapManager::DeinitPbapInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sPbapManager) {
    sPbapManager->Uninit();
    sPbapManager = nullptr;
  }

  if (aRes) {
    aRes->Deinit();
  }
}

// static
BluetoothPbapManager* BluetoothPbapManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // Exit early if sPbapManager already exists
  if (sPbapManager) {
    return sPbapManager;
  }

  // Do not create a new instance if we're in shutdown
  if (NS_WARN_IF(sInShutdown)) {
    return nullptr;
  }

  // Create a new instance, register, and return
  RefPtr<BluetoothPbapManager> manager = new BluetoothPbapManager();
  if (NS_WARN_IF(NS_FAILED(manager->Init()))) {
    return nullptr;
  }

  sPbapManager = manager;
  return sPbapManager;
}

bool BluetoothPbapManager::Listen() {
  MOZ_ASSERT(NS_IsMainThread());

  // Fail to listen if |mSocket| already exists
  if (NS_WARN_IF(mSocket)) {
    return false;
  }

  /**
   * Restart server socket since its underlying fd becomes invalid when
   * BT stops; otherwise no more read events would be received even if
   * BT restarts.
   */
  BluetoothSocket::Uninit(mRfcommServerSocket);
  mRfcommServerSocket = new BluetoothSocket(this);

  BluetoothSocket::Uninit(mL2capServerSocket);
  mL2capServerSocket = new BluetoothSocket(this);

  int rfcommChannel = DEFAULT_RFCOMM_CHANNEL_PBS;
  // the DEFAULT_L2CAP_PSM_PBAP value refer to:
  //packages/modules/Bluetooth/system/gd/hal/snoop_logger.cc
  int l2capChannel = DEFAULT_L2CAP_PSM_PBAP;

  uint32_t features = PBAP_SUPPORTED_FEATURES;
  uint32_t repositories = PBAP_SUPPORTED_REPOSITORIES;

  BLuetoothPbapSdpRecord pbapRecord(rfcommChannel, l2capChannel, features, repositories);

  auto sdpResultHandle = [](int aType, int aHandle) {
    BT_LOGR("PBAP handle CreateSdpRecord result: %d", aHandle);
    sSdpPbapHandle = aHandle;
  };

  if (sSdpPbapHandle != -1) {
    BluetoothSdpManager::RemoveSdpRecord(sSdpPbapHandle, nullptr);
    sSdpPbapHandle = -1;
  }

  BluetoothSdpManager::CreateSdpRecord(pbapRecord, sdpResultHandle);

  nsresult rv = mRfcommServerSocket->Listen(
      u"OBEX Phonebook Access Server"_ns, kPbapPSE, BluetoothSocketType::RFCOMM,
      rfcommChannel, false, true);

  if (NS_FAILED(rv)) {
    mRfcommServerSocket = nullptr;
    return false;
  }

  rv = mL2capServerSocket->Listen(
      u"OBEX Phonebook Access Server"_ns, kPbapPSE, BluetoothSocketType::L2CAP,
      l2capChannel, false, true);

  if (NS_FAILED(rv)) {
    mL2capServerSocket = nullptr;
    return false;
  }

  BT_LOGR("PBAP socket is listening");
  return true;
}

// Virtual function of class SocketConsumer
void BluetoothPbapManager::ReceiveSocketData(
    BluetoothSocket* aSocket, UniquePtr<UnixSocketBuffer>& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * Ensure
   * - valid access to data[0], i.e., opCode
   * - received packet length smaller than max packet length
   */
  int receivedLength = aMessage->GetSize();
  if (receivedLength < 1 || receivedLength > MAX_PACKET_LENGTH) {
    ReplyError(ObexResponseCode::BadRequest);
    return;
  }

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];
  ObexHeaderSet pktHeaders;

  switch (opCode) {
    case ObexRequestCode::Connect: {
      mPasswordReqNeeded = false;

      // revert back to the normal GOEP request/response model
      sPbapObexSrmEnabled = false;
      sPbapObexSrmActive  = false;
      sPbapObexSrmResposeRequestSend = false;

      // Section 3.3.1 "Connect", IrOBEX 1.2
      // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
      // [Headers:var]
      if (receivedLength < 7 ||
          !ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // Section 6.4 "Establishing an OBEX Session", PBAP 1.2
      // The OBEX header target shall equal to kPbapObexTarget.
      if (!CompareHeaderTarget(pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // Save the max packet length from remote information
      mRemoteMaxPacketLength = ((static_cast<int>(data[5]) << 8) | data[6]);

      if (mRemoteMaxPacketLength < kObexLeastMaxSize) {
        BT_LOGR("Remote maximum packet length %d is smaller than %d bytes",
                mRemoteMaxPacketLength, kObexLeastMaxSize);
        mRemoteMaxPacketLength = 0;
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // Section 3.5 "Authentication Procedure", IrOBEX 1.2
      // Get anthentication challenge data and nonce here and
      // will request an user input password after user accept
      // this OBEX connection.
      if (pktHeaders.Has(ObexHeaderId::AuthChallenge)) {
        GetRemoteNonce(pktHeaders);
        mPasswordReqNeeded = true;
      }

      // reset PCE supported features
      mPceSupportedFeatures = 0;

      uint8_t buf[64];
      if (pktHeaders.GetAppParameter(PbapSupportedFeatures, buf, 64)) {
        mPceSupportedFeatures = BigEndian::readUint32(buf);
      }

      BT_LOGR("remote PBAP features: %08x", mPceSupportedFeatures);

      // The user consent is required. If user accept the connection request,
      // the OBEX connection session will be processed;
      // Otherwise, the session will be rejected.
      ObexResponseCode response = NotifyConnectionRequest();
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }
      break;
    }
    case ObexRequestCode::Disconnect:
      [[fallthrough]];
    case ObexRequestCode::Abort:
      // revert back to the normal GOEP request/response model
      sPbapObexSrmEnabled = false;
      sPbapObexSrmActive  = false;
      sPbapObexSrmResposeRequestSend = false;

      // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
      // The format of request packet of "Disconnect" and "Abort" are the same
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToDisconnectOrAbort();
      AfterPbapDisconnected();
      break;
    case ObexRequestCode::SetPath: {
      // Section 3.3.6 "SetPath", IrOBEX 1.2
      // [opcode:1][length:2][flags:1][contants:1][Headers:var]
      if (receivedLength < 5 ||
          !ParseHeaders(&data[5], receivedLength - 5, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      ObexResponseCode response = SetPhoneBookPath(pktHeaders, data[3]);
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }

      ReplyToSetPath();
      break;
    }
    case ObexRequestCode::Get:
      /*
       * Section 6.2.2 "OBEX Headers in Multi-Packet Responses", IrOBEX 1.2
       * All OBEX request messages shall be sent as one OBEX packet containing
       * all the headers, i.e., OBEX GET with opcode 0x83 shall always be
       * used, and GET with opcode 0x03 shall never be used.
       */
      BT_LOGR("PBAP shall always use OBEX GetFinal instead of Get.");

      // no break. Treat 'Get' as 'GetFinal' for error tolerance.
      [[fallthrough]];
    case ObexRequestCode::GetFinal: {
      // Section 3.1 "Request format", IrOBEX 1.2
      // The format of an OBEX request is
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      auto srmp = pktHeaders.GetSRMP();

      if (1 == pktHeaders.GetSRM()) {
        sPbapObexSrmEnabled = true;

        // Request to send a response with SRM header
        sPbapObexSrmResposeRequestSend = true;

        // Assign a default status for SRM
        // And then re-setting the status accroding to SRMP
        sPbapObexSrmActive = true;

        if (srmp == 1) {
          // This SRMP process is within the SRM process loop.
          // It means if the PCE sends SRMP without SRM, the command should be ignored.
          // PTS case: PBAP/PSE/GOEP/SRMP/BI-02-C
          sPbapObexSrmActive = false;
        }
      }

      if (sPbapObexSrmEnabled && srmp == -1) {
        // GOEP_v2.1 table 4.3
        // If Gecko received SRM in previous GET operations
        // Once the client does not carry the SRMP, we need to revert back to SRM active mode
        sPbapObexSrmActive = true;
      }

      /*
       * When |mVCardDataStream| requires multiple response packets to complete,
       * the client should continue to issue GET requests until the final body
       * information (i.e., End-of-Body header) arrives, along with
       * ObexResponseCode::Success
       */
      if (mVCardDataStream) {
        if (!ReplyToGet()) {
          BT_LOGR("Failed to reply to PBAP GET request.");
          ReplyError(ObexResponseCode::InternalServerError);
        }
        return;
      }

      ObexResponseCode response = NotifyPbapRequest(pktHeaders);
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }

      // OBEX success response will be sent after gaia replies PBAP request
      break;
    }
    case ObexRequestCode::Put:
      [[fallthrough]];
    case ObexRequestCode::PutFinal:
      ReplyError(ObexResponseCode::BadRequest);
      BT_LOGR("Unsupported ObexRequestCode %x", opCode);
      break;
    default:
      ReplyError(ObexResponseCode::NotImplemented);
      BT_LOGR("Unrecognized ObexRequestCode %x", opCode);
      break;
  }
}

bool BluetoothPbapManager::CompareHeaderTarget(const ObexHeaderSet& aHeader) {
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
    if (header->mData[i] != kPbapObexTarget.mUuid[i]) {
      BT_LOGR("UUID mismatch: received target[%d]=0x%x != 0x%x", i,
              header->mData[i], kPbapObexTarget.mUuid[i]);
      return false;
    }
  }

  return true;
}

ObexResponseCode BluetoothPbapManager::NotifyConnectionRequest() {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_LOGR("Failed to get Bluetooth service");
    return ObexResponseCode::PreconditionFailed;
  }

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  bs->DistributeSignal(
      BluetoothSignal(PBAP_CONNECTION_REQ_ID, KEY_PBAP, deviceAddressStr));

  BT_LOGR("Notify front-end app of the PBAP connection request.");

  return ObexResponseCode::Success;
}

ObexResponseCode BluetoothPbapManager::SetPhoneBookPath(
    const ObexHeaderSet& aHeader, uint8_t flags) {
  // Section 5.2 "SetPhoneBook Function", PBAP 1.2
  // flags bit 1 must be 1 and bit 2~7 be 0
  if ((flags >> 1) != 1) {
    BT_LOGR("Illegal flags [0x%x]: bits 1~7 must be 0x01", flags);
    return ObexResponseCode::BadRequest;
  }

  nsString newPath = mCurrentPath;

  /**
   * Three cases:
   * 1) Go up 1 level   - flags bit 0 is 1
   * 2) Go back to root - flags bit 0 is 0 AND name header is empty
   * 3) Go down 1 level - flags bit 0 is 0 AND name header is not empty,
   *                      where name header is the name of child folder
   */
  if (flags & 1) {
    // Go up 1 level
    if (!newPath.IsEmpty()) {
      int32_t lastSlashIdx = newPath.RFindChar('/');
      if (lastSlashIdx != -1) {
        newPath = StringHead(newPath, lastSlashIdx);
      } else {
        // The parent folder is root.
        newPath.AssignLiteral("");
      }
    }
  } else {
    MOZ_ASSERT(aHeader.Has(ObexHeaderId::Name));

    nsString childFolderName;
    aHeader.GetName(childFolderName);
    if (childFolderName.IsEmpty()) {
      // Go back to root
      newPath.AssignLiteral("");
    } else {
      // Go down 1 level
      if (!newPath.IsEmpty()) {
        newPath.AppendLiteral("/");
      }
      newPath.Append(childFolderName);
    }
  }

  // Ensure the new path is legal
  if (!IsLegalPath(newPath)) {
    BT_LOGR("Illegal phone book path [%s]",
            NS_ConvertUTF16toUTF8(newPath).get());
    return ObexResponseCode::NotFound;
  }

  mCurrentPath = newPath;
  BT_LOGR("current path [%s]", NS_ConvertUTF16toUTF8(mCurrentPath).get());

  return ObexResponseCode::Success;
}

ObexResponseCode BluetoothPbapManager::NotifyPbapRequest(
    const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());

  // Clean up the flag of last PBAP request
  mPhonebookSizeRequired = false;
  mNewMissedCallsRequired = false;

  // Get content type and name
  nsString type, name;
  aHeader.GetContentType(type);
  aHeader.GetName(name);

  // Configure request based on content type
  nsString reqId;
  uint8_t tagCount;
  const AppParameterTag* tags;
  if (type.EqualsLiteral("x-bt/phonebook")) {
    reqId.Assign(PULL_PHONEBOOK_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sPhonebookTags);
    tags = sPhonebookTags;

    // Ensure the name of phonebook object is legal
    if (!IsLegalPhonebookName(name)) {
      BT_LOGR("Illegal phone book object name [%s]",
              NS_ConvertUTF16toUTF8(name).get());
      return ObexResponseCode::NotFound;
    } else {
      BT_LOGD("Pull phonebook [%s]", NS_ConvertUTF16toUTF8(name).get());
    }

    // Handle missed calls history request
    if (strstr(NS_ConvertUTF16toUTF8(name).get(), "mch")) {
      mNewMissedCallsRequired = true;
    }
  } else if (type.EqualsLiteral("x-bt/vcard-listing")) {
    reqId.Assign(PULL_VCARD_LISTING_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sVCardListingTags);
    tags = sVCardListingTags;

    // Section 5.3.3 "Name", PBAP 1.2:
    // ... PullvCardListing function uses relative paths. An empty name header
    // may be sent to retrieve the vCard Listing object of the current folder.
    name = name.IsEmpty() ? mCurrentPath : mCurrentPath + u"/"_ns + name;

    BT_LOGD("List phonebook [%s]", NS_ConvertUTF16toUTF8(name).get());

    // Handle missed calls history request
    if (strstr(NS_ConvertUTF16toUTF8(name).get(), "mch")) {
      mNewMissedCallsRequired = true;
    }
  } else if (type.EqualsLiteral("x-bt/vcard")) {
    reqId.Assign(PULL_VCARD_ENTRY_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sVCardEntryTags);
    tags = sVCardEntryTags;

    // Section 5.4 "PullvCardEntry Function", PBAP 1.2
    // The value of header "Name" is object name (*.vcf) or
    // X-BT-UID (X-BT-UID:*)
    if (name.Find(u".vcf"_ns) != kNotFound) {
      // Convert relative path to absolute path if the object name is *.vcf
      name = mCurrentPath + u"/"_ns + name;
    } else {
      // KaiOS curretly supports phonebook object with "vcf" format only
      BT_LOGR("Uhacceptable phonebook object name: %s",
              NS_ConvertUTF16toUTF8(name).get());
      return ObexResponseCode::NotAcceptable;
    }
  } else {
    BT_LOGR("Unknown PBAP request type: %s", NS_ConvertUTF16toUTF8(type).get());
    return ObexResponseCode::BadRequest;
  }

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_LOGR("Failed to get Bluetooth service");
    return ObexResponseCode::PreconditionFailed;
  }

  // Pack PBAP request
  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "name", name);
  for (uint8_t i = 0; i < tagCount; i++) {
    AppendNamedValueByTagId(aHeader, data, tags[i]);
  }

  bs->DistributeSignal(reqId, KEY_PBAP, data);

  return ObexResponseCode::Success;
}

ObexResponseCode BluetoothPbapManager::NotifyPasswordRequest() {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    return ObexResponseCode::PreconditionFailed;
  }

  // Notify gaia of authentiation challenge
  // TODO: Append realm if 1) gaia needs to display it and
  //       2) it's in authenticate challenge header
  nsTArray<BluetoothNamedValue> props;
  bs->DistributeSignal(OBEX_PASSWORD_REQ_ID, KEY_ADAPTER, props);

  return ObexResponseCode::Success;
}

void BluetoothPbapManager::AppendNamedValueByTagId(
    const ObexHeaderSet& aHeader, nsTArray<BluetoothNamedValue>& aValues,
    const AppParameterTag aTagId) {
  uint8_t buf[64];
  if (!aHeader.GetAppParameter(aTagId, buf, 64)) {
    return;
  }

  switch (aTagId) {
    case AppParameterTag::Order: {
      using namespace mozilla::dom::vCardOrderTypeValues;
      uint32_t order = buf[0] < ArrayLength(strings)
                           ? static_cast<uint32_t>(buf[0])
                           : 0;  // default: indexed
      AppendNamedValue(aValues, "order", order);
      break;
    }
    case AppParameterTag::SearchProperty: {
      using namespace mozilla::dom::vCardSearchKeyTypeValues;
      uint32_t searchKey = buf[0] < ArrayLength(strings)
                               ? static_cast<uint32_t>(buf[0])
                               : 0;  // default: name
      AppendNamedValue(aValues, "searchKey", searchKey);
      break;
    }
    case AppParameterTag::SearchValue:
      // Section 5.3.4.3 "SearchValue {<text string>}", PBAP 1.2
      // The UTF-8 character set shall be used for <text string>.

      // Store UTF-8 string with nsCString to follow MDN:Internal_strings
      AppendNamedValue(aValues, "searchText", nsCString((char*)buf));
      break;
    case AppParameterTag::MaxListCount: {
      uint16_t maxListCount = BigEndian::readUint16(buf);
      AppendNamedValue(aValues, "maxListCount",
                       static_cast<uint32_t>(maxListCount));

      // Section 5 "Phone Book Access Profile Functions", PBAP 1.2
      // Replying 'PhonebookSize' is mandatory if 'MaxListCount' parameter
      // is present in the request with a value of 0, else it is excluded.
      mPhonebookSizeRequired = !maxListCount;
      break;
    }
    case AppParameterTag::ListStartOffset:
      AppendNamedValue(aValues, "listStartOffset",
                       static_cast<uint32_t>(BigEndian::readUint16(buf)));
      break;
    case AppParameterTag::PropertySelector:
      AppendNamedValue(aValues, "propSelector", PackPropertiesMask(buf, 64));
      break;
    case AppParameterTag::Format:
      AppendNamedValue(aValues, "format", static_cast<bool>(buf[0]));
      break;
    case AppParameterTag::vCardSelector: {
      bool hasSelectorOperator = aHeader.GetAppParameter(
          AppParameterTag::vCardSelectorOperator, buf, 64);
      AppendNamedValue(aValues,
                       hasSelectorOperator && buf[0] ? "vCardSelector_AND"
                                                     : "vCardSelector_OR",
                       PackPropertiesMask(buf, 64));
      break;
    }
    default:
      BT_LOGR("Unsupported AppParameterTag: %x", aTagId);
      break;
  }
}

bool BluetoothPbapManager::IsLegalPath(const nsAString& aPath) {
  static const char* sLegalPaths[] = {"",  // root
                                      "telecom",
                                      "telecom/pb",
                                      "telecom/ich",
                                      "telecom/och",
                                      "telecom/mch",
                                      "telecom/cch",
                                      "SIM1",
                                      "SIM1/telecom",
                                      "SIM1/telecom/pb",
                                      "SIM1/telecom/ich",
                                      "SIM1/telecom/och",
                                      "SIM1/telecom/mch",
                                      "SIM1/telecom/cch"};

  NS_ConvertUTF16toUTF8 path(aPath);
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sLegalPaths); i++) {
    if (!strcmp(path.get(), sLegalPaths[i])) {
      return true;
    }
  }

  return false;
}

bool BluetoothPbapManager::IsLegalPhonebookName(const nsAString& aName) {
  static const char* sLegalNames[] = {
      "telecom/pb.vcf",       "telecom/ich.vcf",      "telecom/och.vcf",
      "telecom/mch.vcf",      "telecom/cch.vcf",      "SIM1/telecom/pb.vcf",
      "SIM1/telecom/ich.vcf", "SIM1/telecom/och.vcf", "SIM1/telecom/mch.vcf",
      "SIM1/telecom/cch.vcf"};

  NS_ConvertUTF16toUTF8 name(aName);
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sLegalNames); i++) {
    if (!strcmp(name.get(), sLegalNames[i])) {
      return true;
    }
  }

  return false;
}

void BluetoothPbapManager::AfterPbapConnected() {
  mCurrentPath.AssignLiteral("");
  mConnected = true;
}

void BluetoothPbapManager::AfterPbapDisconnected() {
  mConnected = false;

  mRemoteMaxPacketLength = 0;
  mPhonebookSizeRequired = false;
  mNewMissedCallsRequired = false;

  if (mVCardDataStream) {
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }
}

bool BluetoothPbapManager::IsConnected() { return mConnected; }

void BluetoothPbapManager::GetAddress(BluetoothAddress& aDeviceAddress) {
  if (!mSocket) {
    aDeviceAddress.Clear();
    return;
  }

  return mSocket->GetAddress(aDeviceAddress);
}

bool BluetoothPbapManager::ReplyToConnect(const nsAString& aPassword) {
  if (mConnected) {
    return true;
  }

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = 7;

  res[3] = 0x10;  // version=1.0
  res[4] = 0x00;  // flag=0x00
  res[5] = BluetoothPbapManager::MAX_PACKET_LENGTH >> 8;
  res[6] = (uint8_t)BluetoothPbapManager::MAX_PACKET_LENGTH;

  // Section 6.4 "Establishing an OBEX Session", PBAP 1.2
  // Headers: [Connection Id][Who:16]
  //
  // According to section 2.2.11 "Connection Identifie", IrOBEX 1.2,
  // Connection Id header should be the first header.
  index += AppendHeaderConnectionId(&res[index], 0x01);
  index += AppendHeaderWho(&res[index], kObexLeastMaxSize,
                           kPbapObexTarget.mUuid, sizeof(BluetoothUuid));

  // Authentication response
  if (!aPassword.IsEmpty()) {
    // Section 3.5.2.1 "Request-digest", PBAP 1.2
    // The request-digest is required and calculated as follows:
    //   H(nonce ":" password)
    uint32_t hashStringLength = DIGEST_LENGTH + aPassword.Length() + 1;
    UniquePtr<char[]> hashString(new char[hashStringLength]);

    memcpy(hashString.get(), mRemoteNonce, DIGEST_LENGTH);
    hashString[DIGEST_LENGTH] = ':';
    memcpy(&hashString[DIGEST_LENGTH + 1],
           NS_ConvertUTF16toUTF8(aPassword).get(), aPassword.Length());
    MD5Hash(hashString.get(), hashStringLength);

    // 2 tag-length-value triplets: <request-digest:16><nonce:16>
    uint8_t digestResponse[(DIGEST_LENGTH + 2) * 2];
    int offset = AppendAppParameter(digestResponse, sizeof(digestResponse),
                                    ObexDigestResponse::ReqDigest, mHashRes,
                                    DIGEST_LENGTH);
    offset += AppendAppParameter(
        &digestResponse[offset], sizeof(digestResponse) - offset,
        ObexDigestResponse::NonceChallenged, mRemoteNonce, DIGEST_LENGTH);

    index += AppendAuthResponse(&res[index], kObexLeastMaxSize - index,
                                digestResponse, offset);
  }

  BT_LOGR("Reply to PBAP connection request.");

  return SendObexData(res, ObexResponseCode::Success, index);
}

nsresult BluetoothPbapManager::MD5Hash(char* buf, uint32_t len) {
  nsresult rv;

  // Cache a reference to the nsICryptoHash instance since we'll be calling
  // this function frequently.
  if (!mVerifier) {
    mVerifier = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      BT_LOGR("MD5Hash: no crypto hash!");
      return rv;
    }
  }

  rv = mVerifier->Init(nsICryptoHash::MD5);
  if (NS_FAILED(rv)) return rv;

  rv = mVerifier->Update((unsigned char*)buf, len);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString hashString;
  rv = mVerifier->Finish(false, hashString);
  if (NS_FAILED(rv)) return rv;

  NS_ENSURE_STATE(hashString.Length() == sizeof(mHashRes));
  memcpy(mHashRes, hashString.get(), hashString.Length());

  return rv;
}

void BluetoothPbapManager::ReplyToDisconnectOrAbort() {
  if (!mConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
  // The format of response packet of "Disconnect" and "Abort" are the same
  // [opcode:1][length:2][Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = kObexRespHeaderSize;

  SendObexData(res, ObexResponseCode::Success, index);
}

void BluetoothPbapManager::ReplyToSetPath() {
  if (!mConnected) {
    return;
  }

  // Section 3.3.6 "SetPath", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = kObexRespHeaderSize;

  SendObexData(res, ObexResponseCode::Success, index);
}

nsTArray<uint32_t> BluetoothPbapManager::PackPropertiesMask(uint8_t* aData,
                                                            int aSize) {
  nsTArray<uint32_t> propSelector;

  // Table 5.1 "Property Mask", PBAP 1.2
  // PropertyMask is a 64-bit mask that indicates the properties contained in
  // the requested vCard objects. We only support bit 0~31 since the rest are
  // reserved for future use or vendor specific properties.

  uint32_t x = BigEndian::readUint32(&aData[4]);

  uint32_t count = 0;
  while (x) {
    if (x & 1) {
      propSelector.AppendElement(count);
    }

    ++count;
    x >>= 1;
  }

  return propSelector;
}

void BluetoothPbapManager::ReplyToAuthChallenge(const nsAString& aPassword) {
  mPasswordReqNeeded = false;

  // Cancel authentication
  if (aPassword.IsEmpty()) {
    ReplyError(ObexResponseCode::Unauthorized);
    return;
  }

  ReplyToConnect(aPassword);
  AfterPbapConnected();
}

bool BluetoothPbapManager::ReplyToConnectionRequest(bool aAccept) {
  if (!aAccept) {
    return ReplyError(ObexResponseCode::Forbidden);
  }

  if (mPasswordReqNeeded) {
    // An user input password is required to reply to authentication
    // challenge. The OBEX success response will be sent after gaia
    // replies correct password.
    ObexResponseCode response = NotifyPasswordRequest();
    if (response != ObexResponseCode::Success) {
      ReplyError(response);
      return false;
    }

    return true;
  }

  bool ret = ReplyToConnect();
  if (ret) {
    AfterPbapConnected();
  }

  return ret;
}

bool BluetoothPbapManager::ReplyToPullPhonebook(BlobImpl* aBlob,
                                                uint16_t aPhonebookSize) {
  if (!mConnected) {
    return false;
  }

  /**
   * Open vCard input stream only if |mPhonebookSizeRequired| is false,
   * according to section 2.1.4.3 "MaxListCount", PBAP 1.2:
   * "When MaxListCount = 0, ... The response shall not contain any
   *  Body header."
   */
  if (!mPhonebookSizeRequired && !GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet(aPhonebookSize);
}

bool BluetoothPbapManager::ReplyToPullvCardListing(BlobImpl* aBlob,
                                                   uint16_t aPhonebookSize) {
  if (!mConnected) {
    return false;
  }

  /**
   * Open vCard input stream only if |mPhonebookSizeRequired| is false,
   * according to section 5.3.4.4 "MaxListCount", PBAP 1.2:
   * "When MaxListCount = 0, ... The response shall not contain a Body header."
   */
  if (!mPhonebookSizeRequired && !GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet(aPhonebookSize);
}

bool BluetoothPbapManager::ReplyToPullvCardEntry(BlobImpl* aBlob) {
  if (!mConnected) {
    return false;
  }

  if (!GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet();
}

ObexResponseCode BluetoothPbapManager::FillObexBody(uint8_t aWhere[],
                                                    int aBufSize,
                                                    int* aConsumed) {
  *aConsumed = 0;

  if (mVCardDataStream == nullptr) {
    return ObexResponseCode::InternalServerError;
  }

  uint64_t bytesAvailable = 0;

  nsresult rv = mVCardDataStream->Available(&bytesAvailable);
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to get available bytes from input stream. rv=0x%x",
               static_cast<uint32_t>(rv));
    return ObexResponseCode::InternalServerError;
  }

  if (aBufSize <= kObexBodyHeaderSize) {
    BT_WARNING("Buffer size is too small. aBufSize=%d, kObexBodyHeaderSize=%d",
               aBufSize, kObexBodyHeaderSize);
    return ObexResponseCode::InternalServerError;
  }

  if (!bytesAvailable) {
    // append 'ObexHeaderId::EndOfBody' + 'length'
    *aConsumed = AppendHeaderEndOfBody(aWhere);

    // Close input stream
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;

    return ObexResponseCode::Success;
  } else {
    // Read vCard data from input stream
    uint32_t numRead = 0;

    UniquePtr<char[]> buf(new char[aBufSize - kObexBodyHeaderSize]);
    rv = mVCardDataStream->Read(buf.get(), aBufSize - kObexBodyHeaderSize, &numRead);
    if (NS_FAILED(rv)) {
      BT_WARNING("Failed to read from input stream. rv=0x%x",
                 static_cast<uint32_t>(rv));

      // Close input stream
      mVCardDataStream->Close();
      mVCardDataStream = nullptr;
      return ObexResponseCode::InternalServerError;
    }

    // For the normal case, the upper layer passes in 'aBufSize'
    // We read the phone book size as 'aBufSize - kObexBodyHeaderSize'
    // Because we need to reserve a 'kObexBodyHeaderSize' buffer size to save the OBEX header.
    // The OBEX header consists of 'ObexHeaderId::Body' and 'length'.
    // Finally, the size we use should be 'aBufSize', the same as what the user passed in.
    *aConsumed = AppendHeaderBody(aWhere, aBufSize,
                                  reinterpret_cast<uint8_t*>(buf.get()), numRead);

    return ObexResponseCode::Continue;
  }
}

class BluetoothPbapManager::SrmProcessTask final : public Runnable {
 public:
  explicit SrmProcessTask() : Runnable("SRM process") {

  }

  NS_IMETHOD Run() override {
    BT_LOGR("SrmProcessTask run..");

    uint8_t opcode;
    bool sendResult = false;

    if (sPbapManager == nullptr) {
      BT_WARNING("maybe PBAP already shutdown..");
      return NS_OK;
    }

    if (sPbapObexSrmActive && sPbapManager->mVCardDataStream) {
      if (sPbapManager->mRemoteMaxPacketLength < kObexRespHeaderSize) {
        BT_WARNING("Buffer size is too small. mRemoteMaxPacketLength=%d, kObexBodyHeaderSize=%d",
                   sPbapManager->mRemoteMaxPacketLength, kObexRespHeaderSize);
        return NS_OK;
      }

      auto res = MakeUnique<uint8_t[]>(sPbapManager->mRemoteMaxPacketLength);
      int consumed;

      // The 'SendObexData' function will consume 'kObexRespHeaderSize' bytes,
      // so we need to reserve space for them
      opcode = sPbapManager->FillObexBody(&res[kObexRespHeaderSize],
                                          sPbapManager->mRemoteMaxPacketLength - kObexRespHeaderSize,
                                          &consumed);

      if (opcode == ObexResponseCode::Continue || opcode == ObexResponseCode::Success) {
        sendResult = sPbapManager->SendObexData(std::move(res),
                                                opcode,
                                                consumed + kObexRespHeaderSize); // length included kObexRespHeaderSize self
      }
    }

    if (sendResult && opcode == ObexResponseCode::Continue) {
      RefPtr<SrmProcessTask> task = new SrmProcessTask();
      MessageLoop::current()->PostDelayedTask(task.forget(), sSrmProcessScheduleTime);
    }

    return NS_OK;
  }
};

bool BluetoothPbapManager::ReplyToGet(uint16_t aPhonebookSize) {
  MOZ_ASSERT(mRemoteMaxPacketLength >= kObexLeastMaxSize);

  if (!mConnected) {
    return false;
  }

  uint8_t opcode;
  bool sendResult;

  /**
   * This response consists of following parts:
   * - Part 1: [response code:1][length:2]
   *
   * If SRM has been enabled
   * - Part 2: [headerId:1][length:2][SRM]
   *
   * - Part 3: append necessary application parameters if needed
   *           possible parameters list:
   *             folder version (primary & secondary) defined in v12
   *             database identifier defined in v12
   *
   * If |mPhonebookSizeRequired| is true,
   * - Part 4: [headerId:1][length:2][AppParameters:var]
   * - Part 5: [headerId:1][length:2][EndOfBody:0]
   * Otherwise,
   * - Part 4(optional): [headerId:1][length:2][AppParameters:var]
   * - Part 5a: [headerId:1][length:2][EndOfBody:0]
   *   or
   * - Part 5b: [headerId:1][length:2][Body:var]
   */
  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);

  ObexAppParameters appParameters(mRemoteMaxPacketLength);

  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index for them here
  unsigned int index = kObexRespHeaderSize;

  // ---- Part 2: [headerId:1][length:2][SRM] ---- //
  if (sPbapObexSrmEnabled && sPbapObexSrmResposeRequestSend) {
    // SRM header only sent to PCE as SRM response
    // This means if PCE does not send an SRM header in this GET operation,
    // we should not append this header within the response
    index += AppendHeaderSRM(&res[index], true);
    sPbapObexSrmResposeRequestSend = false;
  }

  // ---- Part 3: append necessary application parameters if needed ---- //
  if (CheckFeatureSupport(PBAP_FEATURES_BIT_FOLDER_VER_COUNTERS)) {
    // PBAP v12 C5 defines mandatory parameters
    // todo: how to define our primary & secondary version?
    uint8_t folderVersion[16] = {0}; // 128 bits

    appParameters.Append(static_cast<uint8_t>(AppParameterTag::PrimaryVersionCounter),
                          folderVersion, sizeof(folderVersion));

    appParameters.Append(static_cast<uint8_t>(AppParameterTag::SecondaryVersionCounter),
                          folderVersion, sizeof(folderVersion));
  }

  if (CheckFeatureSupport(PBAP_FEATURES_BIT_DB_ID)) {
    // PBAP v12 C6 defines mandatory parameter
    // todo: make sure it uses a unique database ID

    // in PBAP v12 section: 5.1.4.10
    // A value of 0 for the database identifier signifies that the
    // Folder Version Counters and Contact X-BT-UIDs are not persistent on the server
    uint8_t databaseId[16] = {0}; // 128 bits

    appParameters.Append(static_cast<uint8_t>(AppParameterTag::DatabaseIdentifier),
                          databaseId, sizeof(databaseId));
  }

  if (mPhonebookSizeRequired) {
    // ---- Part 4: [headerId:1][length:2][AppParameters:var] ---- //
    // Section 6.2.1 "Application Parameters Header", PBAP 1.2
    // appParameters: [headerId:1][length:2][PhonebookSize:4],
    //                where [PhonebookSize:4]  = [tagId:1][length:1][value:2]

    uint8_t phonebookSize[2];
    BigEndian::writeUint16(&phonebookSize[0], aPhonebookSize);

    appParameters.Append(static_cast<uint8_t>(AppParameterTag::PhonebookSize),
                          phonebookSize, sizeof(phonebookSize));

    mPhonebookSizeRequired = false;

    // ---- Part 5: [headerId:1][length:2][EndOfBody:0] ---- //
    opcode = ObexResponseCode::Success;

    if (appParameters.HasData()) {
      index += AppendHeaderAppParameters(&res[index], mRemoteMaxPacketLength,
                                          appParameters.GetData(),
                                          appParameters.GetDataSize());
    }
  } else {
    MOZ_ASSERT(mVCardDataStream);

    uint64_t bytesAvailable = 0;
    nsresult rv = mVCardDataStream->Available(&bytesAvailable);
    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to get available bytes from input stream. rv=0x%x",
              static_cast<uint32_t>(rv));
      return false;
    }

    // ----  Part 4: [headerId:1][length:2][AppParameters:var] ---- //
    // Section 6.2.1 "Application Parameters Header", PBAP 1.2
    // appParameters: [headerId:1][length:2][NewMissedCalls:3],
    //                where [NewMissedCalls:3] = [tagId:1][length:1][value:1]
    if (mNewMissedCallsRequired) {
      // Since the frontend of KaiOS don't support NewMissedCalls feature, set
      // it to |aPhonebookSize| to pretend no missed call has been dismissed.
      uint8_t dummyNewMissedCalls = aPhonebookSize;

      appParameters.Append(static_cast<uint8_t>(AppParameterTag::NewMissedCalls),
                            &dummyNewMissedCalls, sizeof(dummyNewMissedCalls));

      mNewMissedCallsRequired = false;
    }

    if (appParameters.HasData()) {
      index += AppendHeaderAppParameters(&res[index], mRemoteMaxPacketLength,
                                          appParameters.GetData(),
                                          appParameters.GetDataSize());
    }

    /*
    * In practice, some platforms can only handle zero length End-of-Body
    * header separately with Body header.
    * Thus, append End-of-Body only if the data stream had been sent out,
    * otherwise, send 'Continue' to request for next GET request.
    */
    if (!bytesAvailable) {
      // ----  Part 5a: [headerId:1][length:2][EndOfBody:0] ---- //
      index += AppendHeaderEndOfBody(&res[index]);

      // Close input stream
      mVCardDataStream->Close();
      mVCardDataStream = nullptr;

      opcode = ObexResponseCode::Success;
    } else {
      // Compute remaining packet size to append Body, excluding Body's header
      uint32_t remainingPacketSize =
          mRemoteMaxPacketLength - kObexBodyHeaderSize - index;

      // Read vCard data from input stream
      uint32_t numRead = 0;
      UniquePtr<char[]> buf(new char[remainingPacketSize]);
      rv = mVCardDataStream->Read(buf.get(), remainingPacketSize, &numRead);
      if (NS_FAILED(rv)) {
        BT_LOGR("Failed to read from input stream. rv=0x%x",
                static_cast<uint32_t>(rv));
        return false;
      }

      // |numRead| must be non-zero as |bytesAvailable| is non-zero
      MOZ_ASSERT(numRead);

      // ----  Part 5b: [headerId:1][length:2][Body:var] ---- //
      index += AppendHeaderBody(&res[index],
                                remainingPacketSize + kObexBodyHeaderSize,
                                reinterpret_cast<uint8_t*>(buf.get()), numRead);

      opcode = ObexResponseCode::Continue;
    }
  }

  sendResult = SendObexData(std::move(res), opcode, index);

  if (sendResult && sPbapObexSrmActive && opcode == ObexResponseCode::Continue) {
    RefPtr<SrmProcessTask> task = new SrmProcessTask();
    MessageLoop::current()->PostDelayedTask(task.forget(), sSrmProcessScheduleTime);
  }

  return sendResult;
}

bool BluetoothPbapManager::GetInputStreamFromBlob(BlobImpl* aBlob) {
  // PBAP can only handle one OBEX BODY transfer at the same time.
  if (mVCardDataStream) {
    BT_LOGR("Shouldn't handle multiple PBAP responses simultaneously");
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }

  ErrorResult rv;
  aBlob->CreateInputStream(getter_AddRefs(mVCardDataStream), rv);
  if (rv.Failed()) {
    BT_LOGR("Failed to get internal stream from blob. rv=0x%x",
            rv.ErrorCodeAsInt());
    return false;
  }

  return true;
}

void BluetoothPbapManager::GetRemoteNonce(const ObexHeaderSet& aHeader) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHeader.Has(ObexHeaderId::AuthChallenge));

  // Get authentication challenge data
  const ObexHeader* authHeader = aHeader.GetHeader(ObexHeaderId::AuthChallenge);

  // Get nonce from authentication challenge
  // Section 3.5.1 "Digest Challenge", IrOBEX spec 1.2
  // The tag-length-value triplet of nonce is
  //   [tagId:1][length:1][nonce:16]
  uint8_t offset = 0;
  do {
    uint8_t tagId = authHeader->mData[offset++];
    uint8_t length = authHeader->mData[offset++];

    BT_LOGR("AuthChallenge header includes tagId %d", tagId);
    if (tagId == ObexDigestChallenge::Nonce) {
      memcpy(mRemoteNonce, &authHeader->mData[offset], DIGEST_LENGTH);
    }

    offset += length;
  } while (offset < authHeader->mDataLength);
}

bool BluetoothPbapManager::ReplyError(uint8_t aError) {
  BT_LOGR("[0x%x]", aError);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [response code:1][length:2][data:var]
  uint8_t res[kObexLeastMaxSize];
  return SendObexData(res, aError, kObexBodyHeaderSize);
}

bool BluetoothPbapManager::SendObexData(uint8_t* aData, uint8_t aOpcode,
                                        int aSize) {
  if (!mSocket) {
    BT_LOGR("Failed to send PBAP obex data. aOpcode: [0x%x] ", aOpcode);
    return false;
  }

  SetObexPacketInfo(aData, aOpcode, aSize);
  mSocket->SendSocketData(new UnixSocketRawData(aData, aSize));

  return true;
}

bool BluetoothPbapManager::SendObexData(UniquePtr<uint8_t[]> aData,
                                        uint8_t aOpcode, int aSize) {
  SetObexPacketInfo(aData.get(), aOpcode, aSize);
  mSocket->SendSocketData(new UnixSocketRawData(std::move(aData), aSize));

  return true;
}

void BluetoothPbapManager::OnSocketConnectSuccess(BluetoothSocket* aSocket) {
  MOZ_ASSERT(aSocket);

  BT_LOGR("PBAP socket is connected");

  // todo: if `mSocket` already connected, how to process it ?

  if (aSocket == mRfcommServerSocket) {
    BT_LOGR("PBAP RFCOMM socket is connected");

    // Close server socket as only one session is allowed at a time
    mRfcommServerSocket.swap(mSocket);
  } else if (aSocket == mL2capServerSocket) {
    BT_LOGR("PBAP L2CAP socket is connected");

    // Close server socket as only one session is allowed at a time
    mL2capServerSocket.swap(mSocket);
  } else {
    BT_LOGR("!!!!!!!!!! failed OnSocketConnectSuccess");
  }

  // Cache device address since we can't get socket address when a remote
  // device disconnect with us.
  mSocket->GetAddress(mDeviceAddress);
}

void BluetoothPbapManager::OnSocketConnectError(BluetoothSocket* aSocket) {
  BluetoothSocket::Uninit(mRfcommServerSocket);
  mRfcommServerSocket = nullptr;

  BluetoothSocket::Uninit(mL2capServerSocket);
  mL2capServerSocket = nullptr;

  BluetoothSocket::Uninit(mSocket);
  mSocket = nullptr;
}

void BluetoothPbapManager::OnSocketDisconnect(BluetoothSocket* aSocket) {
  MOZ_ASSERT(aSocket);

  if (aSocket != mSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  AfterPbapDisconnected();
  mDeviceAddress.Clear();

  mSocket = nullptr;  // should already be closed

  Listen();
}

void BluetoothPbapManager::Disconnect(BluetoothProfileController* aController) {
  if (!mSocket) {
    BT_LOGR("No ongoing connection to disconnect");
    return;
  }

  mSocket->Close();
}

NS_IMPL_ISUPPORTS(BluetoothPbapManager, nsIObserver)

void BluetoothPbapManager::Connect(const BluetoothAddress& aDeviceAddress,
                                   BluetoothProfileController* aController) {
  MOZ_ASSERT(false);
}

void BluetoothPbapManager::OnGetServiceChannel(
    const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aServiceUuid,
    int aChannel) {
  MOZ_ASSERT(false);
}

void BluetoothPbapManager::OnUpdateSdpRecords(
    const BluetoothAddress& aDeviceAddress) {
  MOZ_ASSERT(false);
}

void BluetoothPbapManager::OnConnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(false);
}

void BluetoothPbapManager::OnDisconnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(false);
}

void BluetoothPbapManager::Reset() { MOZ_ASSERT(false); }

END_BLUETOOTH_NAMESPACE
