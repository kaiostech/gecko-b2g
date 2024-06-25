/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothCommon_h
#define mozilla_dom_bluetooth_BluetoothCommon_h

#include <algorithm>
#include "mozilla/ArrayUtils.h"  // for MOZ_ARRAY_LENGTH
#include "mozilla/Compiler.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Observer.h"
#include "mozilla/UniquePtr.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"

extern bool gBluetoothDebugFlag;

#define SWITCH_BT_DEBUG(V) (gBluetoothDebugFlag = V)

#undef BT_LOG
#if defined(MOZ_WIDGET_GONK)
#  include <android/log.h>

/**
 * Prints 'D'EBUG logs, which shows only when developer setting
 * 'Bluetooth output in adb' is enabled.
 */
#  define BT_LOGD(msg, ...)                                                  \
    do {                                                                     \
      if (gBluetoothDebugFlag) {                                             \
        __android_log_print(ANDROID_LOG_DEBUG, "GeckoBluetooth", "%s: " msg, \
                            __FUNCTION__, ##__VA_ARGS__);                    \
      }                                                                      \
    } while (0)

/**
 * Prints 'I'NFO logs, which show even when developer setting
 * 'Bluetooth output in adb' is disabled.
 */
#  define BT_LOGR(msg, ...)                                             \
    __android_log_print(ANDROID_LOG_INFO, "GeckoBluetooth", "%s: " msg, \
                        __FUNCTION__, ##__VA_ARGS__)

/**
 * Prints 'W'ARN logs, which show only when developer setting
 * 'Bluetooth output in adb' is disabled.
 */
#  define BT_WARNING(msg, ...)                                            \
    do {                                                                  \
      __android_log_print(ANDROID_LOG_WARN, "GeckoBluetooth", "%s: " msg, \
                          __FUNCTION__, ##__VA_ARGS__);                   \
    } while (0)

#else
#  define BT_LOGD(msg, ...)                              \
    do {                                                 \
      if (gBluetoothDebugFlag) {                         \
        printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__); \
      }                                                  \
    } while (0)

#  define BT_LOGR(msg, ...) printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__)
#  define BT_WARNING(msg, ...) printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__)
#endif

/**
 * Convert an enum value to string and append it to a fallible array.
 */
#define BT_APPEND_ENUM_STRING_FALLIBLE(array, enumType, enumValue) \
  do {                                                             \
    uint32_t index = uint32_t(enumValue);                          \
    nsAutoString name;                                             \
    name.AssignASCII(enumType##Values::strings[index].value,       \
                     enumType##Values::strings[index].length);     \
    if (array.AppendElement(name, mozilla::fallible) == nullptr) { \
      BT_WARNING("failed to append element to nsArray");           \
    }                                                              \
  } while (0)

/**
 * Ensure success of system message broadcast with void return.
 */
#define BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters) \
  do {                                                         \
    nsCString message = NS_ConvertUTF16toUTF8(type);           \
    BT_LOGR("broadcast system message: [%s]", message.get());  \
    if (!BroadcastSystemMessage(type, parameters)) {           \
      BT_WARNING("Failed to broadcast [%s]", message.get());   \
      return;                                                  \
    }                                                          \
  } while (0)

/**
 * Resolve |promise| with |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE(x, promise, ret)         \
  do {                                                  \
    if (MOZ_UNLIKELY(!(x))) {                           \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE(" #x ") failed"); \
      (promise)->MaybeResolve(ret);                     \
      return (promise).forget();                        \
    }                                                   \
  } while (0)

/**
 * Reject |promise| with |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT(x, promise, ret)         \
  do {                                                 \
    if (MOZ_UNLIKELY(!(x))) {                          \
      BT_LOGR("BT_ENSURE_TRUE_REJECT(" #x ") failed"); \
      (promise)->MaybeReject(ret);                     \
      return (promise).forget();                       \
    }                                                  \
  } while (0)

/**
 * Reject |promise| with |ret| if nsresult |rv| is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT(rv, promise, ret) \
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv), promise, ret)

/**
 * Resolve |promise| with |value| and return |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE_RETURN(x, promise, value, ret)  \
  do {                                                         \
    if (MOZ_UNLIKELY(!(x))) {                                  \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE_RETURN(" #x ") failed"); \
      (promise)->MaybeResolve(value);                          \
      return ret;                                              \
    }                                                          \
  } while (0)

/**
 * Resolve |promise| with |value| and return if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE_VOID(x, promise, value)       \
  do {                                                       \
    if (MOZ_UNLIKELY(!(x))) {                                \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE_VOID(" #x ") failed"); \
      (promise)->MaybeResolve(value);                        \
      return;                                                \
    }                                                        \
  } while (0)

/**
 * Reject |promise| with |value| and return |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT_RETURN(x, promise, value, ret)  \
  do {                                                        \
    if (MOZ_UNLIKELY(!(x))) {                                 \
      BT_LOGR("BT_ENSURE_TRUE_REJECT_RETURN(" #x ") failed"); \
      (promise)->MaybeReject(value);                          \
      return ret;                                             \
    }                                                         \
  } while (0)

/**
 * Reject |promise| with |value| and return if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT_VOID(x, promise, value)       \
  do {                                                      \
    if (MOZ_UNLIKELY(!(x))) {                               \
      BT_LOGR("BT_ENSURE_TRUE_REJECT_VOID(" #x ") failed"); \
      (promise)->MaybeReject(value);                        \
      return;                                               \
    }                                                       \
  } while (0)

/**
 * Reject |promise| with |value| and return |ret| if nsresult |rv|
 * is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT_RETURN(rv, promise, value, ret) \
  BT_ENSURE_TRUE_REJECT_RETURN(NS_SUCCEEDED(rv), promise, value, ret)

/**
 * Reject |promise| with |value| and return if nsresult |rv|
 * is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT_VOID(rv, promise, value) \
  BT_ENSURE_TRUE_REJECT_VOID(NS_SUCCEEDED(rv), promise, value)

#define BEGIN_BLUETOOTH_NAMESPACE \
  namespace mozilla {             \
  namespace dom {                 \
  namespace bluetooth {
#define END_BLUETOOTH_NAMESPACE \
  } /* namespace bluetooth */   \
  } /* namespace dom */         \
  } /* namespace mozilla */
#define USING_BLUETOOTH_NAMESPACE using namespace mozilla::dom::bluetooth;

#define KEY_LOCAL_AGENT "/B2G/bluetooth/agent"
#define KEY_REMOTE_AGENT "/B2G/bluetooth/remote_device_agent"
#define KEY_MANAGER u"/B2G/bluetooth/manager"_ns
#define KEY_ADAPTER u"/B2G/bluetooth/adapter"_ns
#define KEY_PAIRING_LISTENER u"/B2G/bluetooth/pairing_listener"_ns
#define KEY_PBAP u"/B2G/bluetooth/pbap"_ns
#define KEY_MAP u"/B2G/bluetooth/map"_ns

/**
 * When the connection status of a Bluetooth profile is changed, we'll notify
 * observers which register the following topics.
 */
#define BLUETOOTH_A2DP_STATUS_CHANGED_ID "bluetooth-a2dp-status-changed"
#define BLUETOOTH_HFP_STATUS_CHANGED_ID u"bluetooth-hfp-status-changed"_ns
#define BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID "bluetooth-hfp-nrec-status-changed"
#define BLUETOOTH_HFP_WBS_STATUS_CHANGED_ID "bluetooth-hfp-wbs-status-changed"
#define BLUETOOTH_HID_STATUS_CHANGED_ID u"bluetooth-hid-status-changed"_ns
#define BLUETOOTH_SCO_STATUS_CHANGED_ID u"bluetooth-sco-status-changed"_ns

/**
 * When receiving the pbap connection request, will dispatch this event.
 */
#define PBAP_CONNECTION_REQ_ID u"pbapconnectionreq"_ns

/**
 * When receiving the map connection request, will dispatch this event.
 */
#define MAP_CONNECTION_REQ_ID u"mapconnectionreq"_ns

/**
 * Update MAP version.
 */
#define MAP_VERSION_ID u"mapversion"_ns

/**
 * When the connection status of a Bluetooth profile is changed, we'll
 * dispatch one of the following events.
 */
#define A2DP_STATUS_CHANGED_ID u"a2dpstatuschanged"_ns
#define HFP_STATUS_CHANGED_ID "hfpstatuschanged"
#define HID_STATUS_CHANGED_ID u"hidstatuschanged"_ns
#define SCO_STATUS_CHANGED_ID "scostatuschanged"

/**
 * Types of pairing requests for constructing BluetoothPairingEvent and
 * BluetoothPairingHandle.
 */
#define PAIRING_REQ_TYPE_DISPLAYPASSKEY "displaypasskeyreq"
#define PAIRING_REQ_TYPE_ENTERPINCODE u"enterpincodereq"_ns
#define PAIRING_REQ_TYPE_CONFIRMATION "pairingconfirmationreq"
#define PAIRING_REQ_TYPE_CONSENT "pairingconsentreq"

/**
 * System message to notify apps that the current pairing is aborted either by
 * remote device or local bluetooth adapter.
 */
#define SYS_MSG_BT_PAIRING_ABORTED u"bluetooth-pairing-aborted"_ns

/**
 * System message to launch bluetooth app if no pairing listener is ready to
 * receive pairing requests.
 */
#define SYS_MSG_BT_PAIRING_REQ u"bluetooth-pairing-request"_ns

/**
 * System message to launch bluetooth PBAP app,
 * if no PBAP app is ready to receive PBAP related requests.
 */
#define SYS_MSG_BT_PBAP_REQ u"bluetooth-pbap-request"_ns

/**
 * System message to launch bluetooth MAP app,
 * if no MAP app is ready to receive MAP related requests.
 */
#define SYS_MSG_BT_MAP_REQ u"bluetooth-map-request"_ns

/**
 * The preference name of bluetooth app origin of bluetooth app. The default
 * value is defined in b2g/app/b2g.js.
 */
#define PREF_BLUETOOTH_APP_ORIGIN "dom.bluetooth.app-origin"

/**
 * When a remote device gets paired / unpaired with local bluetooth adapter or
 * pairing process is aborted, we'll dispatch an event.
 */
#define DEVICE_PAIRED_ID u"devicepaired"_ns
#define DEVICE_UNPAIRED_ID u"deviceunpaired"_ns
#define PAIRING_ABORTED_ID u"pairingaborted"_ns

/**
 * When receiving a query about current play status from remote device, we'll
 * dispatch an event.
 */
#define REQUEST_MEDIA_PLAYSTATUS_ID u"requestmediaplaystatus"_ns

/**
 * When receiving an OBEX authenticate challenge request from a remote device,
 * we'll dispatch an event.
 */
#define OBEX_PASSWORD_REQ_ID u"obexpasswordreq"_ns

/**
 * When receiving a PBAP request from a remote device, we'll dispatch an event.
 */
#define PULL_PHONEBOOK_REQ_ID u"pullphonebookreq"_ns
#define PULL_VCARD_ENTRY_REQ_ID u"pullvcardentryreq"_ns
#define PULL_VCARD_LISTING_REQ_ID u"pullvcardlistingreq"_ns

/**
 * When receiving a MAP request from a remote device,
 * we'll dispatch an event.
 */
#define MAP_MESSAGES_LISTING_REQ_ID u"mapmessageslistingreq"_ns
#define MAP_GET_MESSAGE_REQ_ID u"mapgetmessagereq"_ns
#define MAP_SET_MESSAGE_STATUS_REQ_ID u"mapsetmessagestatusreq"_ns
#define MAP_SEND_MESSAGE_REQ_ID u"mapsendmessagereq"_ns
#define MAP_FOLDER_LISTING_REQ_ID u"mapfolderlistingreq"_ns
#define MAP_MESSAGE_UPDATE_REQ_ID u"mapmessageupdatereq"_ns

/**
 * When the value of a characteristic of a remote BLE device changes, we'll
 * dispatch an event
 */
#define GATT_CHARACTERISTIC_CHANGED_ID u"characteristicchanged"_ns

/**
 * When a remote BLE device gets connected / disconnected, we'll dispatch an
 * event.
 */
#define GATT_CONNECTION_STATE_CHANGED_ID u"connectionstatechanged"_ns

/**
 * When attributes of BluetoothManager, BluetoothAdapter, or BluetoothDevice
 * are changed, we'll dispatch an event.
 */
#define ATTRIBUTE_CHANGED_ID u"attributechanged"_ns

/**
 * When the local GATT server received attribute read/write requests, we'll
 * dispatch an event.
 */
#define ATTRIBUTE_READ_REQUEST u"attributereadreq"_ns
#define ATTRIBUTE_WRITE_REQUEST u"attributewritereq"_ns

// Bluetooth address format: xx:xx:xx:xx:xx:xx (or xx_xx_xx_xx_xx_xx)
#define BLUETOOTH_ADDRESS_LENGTH 17
#define BLUETOOTH_ADDRESS_NONE "00:00:00:00:00:00"
#define BLUETOOTH_ADDRESS_BYTES 6

// Bluetooth stack internal error, such as I/O error
#define ERR_INTERNAL_ERROR "InternalError"

/**
 * BT specification v4.1 defines the maximum attribute length as 512 octets.
 * Currently use 600 here to conform to bluedroid's BTGATT_MAX_ATTR_LEN.
 */
#define BLUETOOTH_GATT_MAX_ATTR_LEN 600

/**
 * The maximum descriptor length defined in BlueZ ipc spec.
 * Please refer to http://git.kernel.org/cgit/bluetooth/bluez.git/tree/\
 * android/hal-ipc-api.txt#n532
 */
#define BLUETOOTH_HID_MAX_DESC_LEN 884

BEGIN_BLUETOOTH_NAMESPACE

enum BluetoothStatus {
  STATUS_SUCCESS,
  STATUS_FAIL,
  STATUS_NOT_READY,
  STATUS_NOMEM,
  STATUS_BUSY,
  STATUS_DONE,
  STATUS_UNSUPPORTED,
  STATUS_PARM_INVALID,
  STATUS_UNHANDLED,
  STATUS_AUTH_FAILURE,
  STATUS_RMT_DEV_DOWN,
  STATUS_AUTH_REJECTED,
  NUM_STATUS
};

enum BluetoothAclState { ACL_STATE_CONNECTED, ACL_STATE_DISCONNECTED };

enum BluetoothBondState {
  BOND_STATE_NONE,
  BOND_STATE_BONDING,
  BOND_STATE_BONDED
};

enum BluetoothSetupServiceId {
  SETUP_SERVICE_ID_SETUP,
  SETUP_SERVICE_ID_CORE,
  SETUP_SERVICE_ID_SOCKET,
  SETUP_SERVICE_ID_HID,
  SETUP_SERVICE_ID_PAN,
  SETUP_SERVICE_ID_HANDSFREE,
  SETUP_SERVICE_ID_A2DP,
  SETUP_SERVICE_ID_HEALTH,
  SETUP_SERVICE_ID_AVRCP,
  SETUP_SERVICE_ID_GATT,
  SETUP_SERVICE_ID_HANDSFREE_CLIENT,
  SETUP_SERVICE_ID_MAP_CLIENT,
  SETUP_SERVICE_ID_AVRCP_CONTROLLER,
  SETUP_SERVICE_ID_A2DP_SINK,
  SETUP_SERVICE_ID_SDP  // not definded by BlueZ
};

/* Physical transport for GATT connections to remote dual-mode devices */
enum BluetoothTransport {
  TRANSPORT_AUTO,  /* No preference of physical transport */
  TRANSPORT_BREDR, /* Prefer BR/EDR transport */
  TRANSPORT_LE     /* Prefer LE transport */
};

enum BluetoothTypeOfDevice {
  TYPE_OF_DEVICE_BREDR,
  TYPE_OF_DEVICE_BLE,
  TYPE_OF_DEVICE_DUAL
};

enum BluetoothPropertyType {
  PROPERTY_UNKNOWN,
  PROPERTY_BDNAME,
  PROPERTY_BDADDR,
  PROPERTY_UUIDS,
  PROPERTY_CLASS_OF_DEVICE,
  PROPERTY_TYPE_OF_DEVICE,
  PROPERTY_SERVICE_RECORD,
  PROPERTY_ADAPTER_SCAN_MODE,
  PROPERTY_ADAPTER_BONDED_DEVICES,
  PROPERTY_ADAPTER_DISCOVERY_TIMEOUT,
  PROPERTY_REMOTE_FRIENDLY_NAME,
  PROPERTY_REMOTE_RSSI,
  PROPERTY_REMOTE_VERSION_INFO,
  PROPERTY_REMOTE_DEVICE_TIMESTAMP
};

enum BluetoothScanMode {
  SCAN_MODE_NONE,
  SCAN_MODE_CONNECTABLE,
  SCAN_MODE_CONNECTABLE_DISCOVERABLE
};

enum BluetoothSspVariant {
  SSP_VARIANT_PASSKEY_CONFIRMATION,
  SSP_VARIANT_PASSKEY_ENTRY,
  SSP_VARIANT_CONSENT,
  SSP_VARIANT_PASSKEY_NOTIFICATION,
  NUM_SSP_VARIANT
};

struct BluetoothActivityEnergyInfo {
  uint8_t mStatus;
  uint8_t mStackState;  /* stack reported state */
  uint64_t mTxTime;     /* in ms */
  uint64_t mRxTime;     /* in ms */
  uint64_t mIdleTime;   /* in ms */
  uint64_t mEnergyUsed; /* a product of mA, V and ms */
};

/**
 * |BluetoothAddress| stores the 6-byte MAC address of a Bluetooth
 * device. The constants returned from ANY(), ALL() and LOCAL()
 * represent addresses with special meaning.
 */
struct BluetoothAddress {
  static const BluetoothAddress& ANY();
  static const BluetoothAddress& ALL();
  static const BluetoothAddress& LOCAL();

  uint8_t mAddr[6];

  BluetoothAddress() {
    Clear();  // assign ANY()
  }

  MOZ_IMPLICIT BluetoothAddress(const BluetoothAddress&) = default;

  BluetoothAddress(uint8_t aAddr0, uint8_t aAddr1, uint8_t aAddr2,
                   uint8_t aAddr3, uint8_t aAddr4, uint8_t aAddr5) {
    mAddr[0] = aAddr0;
    mAddr[1] = aAddr1;
    mAddr[2] = aAddr2;
    mAddr[3] = aAddr3;
    mAddr[4] = aAddr4;
    mAddr[5] = aAddr5;
  }

  BluetoothAddress& operator=(const BluetoothAddress&) = default;

  bool operator==(const BluetoothAddress& aRhs) const {
    return !memcmp(mAddr, aRhs.mAddr, sizeof(mAddr));
  }

  bool operator!=(const BluetoothAddress& aRhs) const {
    return !operator==(aRhs);
  }

  /**
   * |Clear| assigns an invalid value (i.e., ANY()) to the address.
   */
  void Clear() { operator=(ANY()); }

  /**
   * |IsCleared| returns true if the address does not contain a
   * specific value (i.e., it contains ANY()).
   */
  bool IsCleared() const { return operator==(ANY()); }

  /*
   * Getter and setter methods for the address parts. The figure
   * below illustrates the mapping to bytes; from LSB to MSB.
   *
   *    |       LAP       | UAP |    NAP    |
   *    |  0  |  1  |  2  |  3  |  4  |  5  |
   *
   * See Bluetooth Core Spec 2.1, Sec 1.2.
   */

  uint32_t GetLAP() const {
    return (static_cast<uint32_t>(mAddr[0])) |
           (static_cast<uint32_t>(mAddr[1]) << 8) |
           (static_cast<uint32_t>(mAddr[2]) << 16);
  }

  void SetLAP(uint32_t aLAP) {
    MOZ_ASSERT(!(aLAP & 0xff000000));  // no top-8 bytes in LAP

    mAddr[0] = aLAP;
    mAddr[1] = aLAP >> 8;
    mAddr[2] = aLAP >> 16;
  }

  uint8_t GetUAP() const { return mAddr[3]; }

  void SetUAP(uint8_t aUAP) { mAddr[3] = aUAP; }

  uint16_t GetNAP() const { return LittleEndian::readUint16(&mAddr[4]); }

  void SetNAP(uint16_t aNAP) { LittleEndian::writeUint16(&mAddr[4], aNAP); }
};

struct BluetoothConfigurationParameter {
  uint8_t mType;
  uint16_t mLength;
  mozilla::UniquePtr<uint8_t[]> mValue;
};

/*
 * Service classes and Profile Identifiers
 *
 * Supported Bluetooth services for v1 are listed as below.
 *
 * The value of each service class is defined in "AssignedNumbers/Service
 * Discovery Protocol (SDP)/Service classes and Profile Identifiers" in the
 * Bluetooth Core Specification.
 */
enum BluetoothServiceClass {
  UNKNOWN = 0x0000,
  OBJECT_PUSH = 0x1105,
  HEADSET = 0x1108,
  A2DP_SINK = 0x110b,
  AVRCP_TARGET = 0x110c,
  A2DP = 0x110d,
  AVRCP = 0x110e,
  AVRCP_CONTROLLER = 0x110f,
  HEADSET_AG = 0x1112,
  HANDSFREE = 0x111e,
  HANDSFREE_AG = 0x111f,
  HID = 0x1124,
  PBAP_PCE = 0x112e,
  PBAP_PSE = 0x112f,
  MAP_MAS = 0x1132,
  MAP_MNS = 0x1133
};

struct BluetoothUuid {
  static const BluetoothUuid& ZERO();
  static const BluetoothUuid& BASE();

  uint8_t mUuid[16];  // store 128-bit UUID value in big-endian order

  BluetoothUuid() : BluetoothUuid(ZERO()) {}

  MOZ_IMPLICIT BluetoothUuid(const BluetoothUuid&) = default;

  BluetoothUuid(uint8_t aUuid0, uint8_t aUuid1, uint8_t aUuid2, uint8_t aUuid3,
                uint8_t aUuid4, uint8_t aUuid5, uint8_t aUuid6, uint8_t aUuid7,
                uint8_t aUuid8, uint8_t aUuid9, uint8_t aUuid10,
                uint8_t aUuid11, uint8_t aUuid12, uint8_t aUuid13,
                uint8_t aUuid14, uint8_t aUuid15) {
    mUuid[0] = aUuid0;
    mUuid[1] = aUuid1;
    mUuid[2] = aUuid2;
    mUuid[3] = aUuid3;
    mUuid[4] = aUuid4;
    mUuid[5] = aUuid5;
    mUuid[6] = aUuid6;
    mUuid[7] = aUuid7;
    mUuid[8] = aUuid8;
    mUuid[9] = aUuid9;
    mUuid[10] = aUuid10;
    mUuid[11] = aUuid11;
    mUuid[12] = aUuid12;
    mUuid[13] = aUuid13;
    mUuid[14] = aUuid14;
    mUuid[15] = aUuid15;
  }

  explicit BluetoothUuid(uint32_t aUuid32) { SetUuid32(aUuid32); }

  explicit BluetoothUuid(uint16_t aUuid16) { SetUuid16(aUuid16); }

  explicit BluetoothUuid(BluetoothServiceClass aServiceClass) {
    SetUuid16(static_cast<uint16_t>(aServiceClass));
  }

  BluetoothUuid& operator=(const BluetoothUuid& aRhs) = default;

  /**
   * |Clear| assigns an invalid value (i.e., zero) to the UUID.
   */
  void Clear() { operator=(ZERO()); }

  /**
   * |IsCleared| returns true if the UUID contains a value of
   * zero.
   */
  bool IsCleared() const { return operator==(ZERO()); }

  bool operator==(const BluetoothUuid& aRhs) const {
    return std::equal(aRhs.mUuid, aRhs.mUuid + MOZ_ARRAY_LENGTH(aRhs.mUuid),
                      mUuid);
  }

  bool operator!=(const BluetoothUuid& aRhs) const { return !operator==(aRhs); }

  /* This less-than operator is used for sorted insertion of nsTArray */
  bool operator<(const BluetoothUuid& aUuid) const {
    return memcmp(mUuid, aUuid.mUuid, sizeof(aUuid.mUuid)) < 0;
  };

  /*
   * Getter-setter methods for short UUIDS. The first 4 bytes in the
   * UUID are represented by the short notation UUID32, and bytes 3
   * and 4 (indices 2 and 3) are represented by UUID16. The rest of
   * the UUID is filled with the Bluetooth Base UUID.
   *
   * Below are helpers for accessing these values.
   */

  void SetUuid32(uint32_t aUuid32) {
    operator=(BASE());
    BigEndian::writeUint32(&mUuid[0], aUuid32);
  }

  uint32_t GetUuid32() const { return BigEndian::readUint32(&mUuid[0]); }

  void SetUuid16(uint16_t aUuid16) {
    operator=(BASE());
    BigEndian::writeUint16(&mUuid[2], aUuid16);
  }

  uint16_t GetUuid16() const { return BigEndian::readUint16(&mUuid[2]); }

  bool IsUuid16Convertible() const;
  bool IsUuid32Convertible() const;
};

struct BluetoothPinCode {
  uint8_t mPinCode[16]; /* not \0-terminated */
  uint8_t mLength;

  BluetoothPinCode() : mLength(0) {
    std::fill(mPinCode, mPinCode + MOZ_ARRAY_LENGTH(mPinCode), 0);
  }

  bool operator==(const BluetoothPinCode& aRhs) const {
    MOZ_ASSERT(mLength <= MOZ_ARRAY_LENGTH(mPinCode));
    return (mLength == aRhs.mLength) &&
           std::equal(aRhs.mPinCode, aRhs.mPinCode + aRhs.mLength, mPinCode);
  }

  bool operator!=(const BluetoothPinCode& aRhs) const {
    return !operator==(aRhs);
  }
};

struct BluetoothServiceName {
  uint8_t mName[255]; /* not \0-terminated */
};

struct BluetoothServiceRecord {
  BluetoothUuid mUuid;
  uint16_t mChannel;
  char mName[256];
};

struct BluetoothRemoteInfo {
  int mVerMajor;
  int mVerMinor;
  int mManufacturer;
};

struct BluetoothRemoteName {
  uint8_t mName[248]; /* not \0-terminated */
  uint8_t mLength;

  BluetoothRemoteName() : mLength(0) {}

  explicit BluetoothRemoteName(const nsACString& aString) : mLength(0) {
    MOZ_ASSERT(aString.Length() <= MOZ_ARRAY_LENGTH(mName));
    memcpy(mName, aString.Data(), aString.Length());
    mLength = aString.Length();
  }

  BluetoothRemoteName(const BluetoothRemoteName&) = default;

  BluetoothRemoteName& operator=(const BluetoothRemoteName&) = default;

  bool operator==(const BluetoothRemoteName& aRhs) const {
    MOZ_ASSERT(mLength <= MOZ_ARRAY_LENGTH(mName));
    return (mLength == aRhs.mLength) &&
           std::equal(aRhs.mName, aRhs.mName + aRhs.mLength, mName);
  }

  bool operator!=(const BluetoothRemoteName& aRhs) const {
    return !operator==(aRhs);
  }

  void Assign(const uint8_t* aName, size_t aLength) {
    MOZ_ASSERT(aLength <= MOZ_ARRAY_LENGTH(mName));
    memcpy(mName, aName, aLength);
    mLength = aLength;
  }

  void Clear() { mLength = 0; }

  bool IsCleared() const { return !mLength; }
};

struct BluetoothProperty {
  /* Type */
  BluetoothPropertyType mType;

  /* Value
   */

  /* PROPERTY_BDADDR */
  BluetoothAddress mBdAddress;

  /* PROPERTY_BDNAME */
  BluetoothRemoteName mRemoteName;

  /* PROPERTY_REMOTE_FRIENDLY_NAME */
  nsString mString;

  /* PROPERTY_UUIDS */
  nsTArray<BluetoothUuid> mUuidArray;

  /* PROPERTY_ADAPTER_BONDED_DEVICES */
  nsTArray<BluetoothAddress> mBdAddressArray;

  /* PROPERTY_CLASS_OF_DEVICE
     PROPERTY_ADAPTER_DISCOVERY_TIMEOUT */
  uint32_t mUint32;

  /* PROPERTY_RSSI_VALUE */
  int32_t mInt32;

  /* PROPERTY_TYPE_OF_DEVICE */
  BluetoothTypeOfDevice mTypeOfDevice;

  /* PROPERTY_SERVICE_RECORD */
  BluetoothServiceRecord mServiceRecord;

  /* PROPERTY_SCAN_MODE */
  BluetoothScanMode mScanMode;

  /* PROPERTY_REMOTE_VERSION_INFO */
  BluetoothRemoteInfo mRemoteInfo;

  BluetoothProperty() : mType(PROPERTY_UNKNOWN) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothAddress& aBdAddress)
      : mType(aType), mBdAddress(aBdAddress) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothRemoteName& aRemoteName)
      : mType(aType), mRemoteName(aRemoteName) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsAString& aString)
      : mType(aType), mString(aString) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsTArray<BluetoothUuid>& aUuidArray)
      : mType(aType), mUuidArray(aUuidArray.Clone()) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsTArray<BluetoothAddress>& aBdAddressArray)
      : mType(aType), mBdAddressArray(aBdAddressArray.Clone()) {}

  explicit BluetoothProperty(BluetoothPropertyType aType, uint32_t aUint32)
      : mType(aType), mUint32(aUint32) {}

  explicit BluetoothProperty(BluetoothPropertyType aType, int32_t aInt32)
      : mType(aType), mInt32(aInt32) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             BluetoothTypeOfDevice aTypeOfDevice)
      : mType(aType), mTypeOfDevice(aTypeOfDevice) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothServiceRecord& aServiceRecord)
      : mType(aType), mServiceRecord(aServiceRecord) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             BluetoothScanMode aScanMode)
      : mType(aType), mScanMode(aScanMode) {}

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothRemoteInfo& aRemoteInfo)
      : mType(aType), mRemoteInfo(aRemoteInfo) {}
};

enum BluetoothSocketType { RFCOMM = 1, SCO = 2, L2CAP = 3, EL2CAP = 4 };

struct BluetoothHidInfoParam {
  uint16_t mAttributeMask;
  uint8_t mSubclass;
  uint8_t mApplicationId;
  uint16_t mVendorId;
  uint16_t mProductId;
  uint16_t mVersion;
  uint8_t mCountryCode;
  uint16_t mDescriptorLength;
  uint8_t mDescriptorValue[BLUETOOTH_HID_MAX_DESC_LEN];
};

struct BluetoothHidReport {
  nsTArray<uint8_t> mReportData;
};

enum BluetoothHidProtocolMode {
  HID_PROTOCOL_MODE_REPORT = 0x00,
  HID_PROTOCOL_MODE_BOOT = 0x01,
  HID_PROTOCOL_MODE_UNSUPPORTED = 0xff
};

enum BluetoothHidReportType {
  HID_REPORT_TYPE_INPUT = 0x01,
  HID_REPORT_TYPE_OUTPUT = 0x02,
  HID_REPORT_TYPE_FEATURE = 0x03
};

enum BluetoothHidConnectionState {
  HID_CONNECTION_STATE_CONNECTED,
  HID_CONNECTION_STATE_CONNECTING,
  HID_CONNECTION_STATE_DISCONNECTED,
  HID_CONNECTION_STATE_DISCONNECTING,
  HID_CONNECTION_STATE_FAILED_MOUSE_FROM_HOST,
  HID_CONNECTION_STATE_FAILED_KEYBOARD_FROM_HOST,
  HID_CONNECTION_STATE_FAILED_TOO_MANY_DEVICES,
  HID_CONNECTION_STATE_FAILED_NO_HID_DRIVER,
  HID_CONNECTION_STATE_FAILED_GENERIC,
  HID_CONNECTION_STATE_UNKNOWN
};

enum BluetoothHidStatus {
  HID_STATUS_OK,
  HID_STATUS_HANDSHAKE_DEVICE_NOT_READY,
  HID_STATUS_HANDSHAKE_INVALID_REPORT_ID,
  HID_STATUS_HANDSHAKE_TRANSACTION_NOT_SPT,
  HID_STATUS_HANDSHAKE_INVALID_PARAMETER,
  HID_STATUS_HANDSHAKE_GENERIC_ERROR,
  HID_STATUS_GENERAL_ERROR,
  HID_STATUS_SDP_ERROR,
  HID_STATUS_SET_PROTOCOL_ERROR,
  HID_STATUS_DEVICE_DATABASE_FULL,
  HID_STATUS_DEVICE_TYPE_NOT_SUPPORTED,
  HID_STATUS_NO_RESOURCES,
  HID_STATUS_AUTHENTICATION_FAILED,
  HID_STATUS_HDL
};

enum BluetoothHandsfreeAtResponse { HFP_AT_RESPONSE_ERROR, HFP_AT_RESPONSE_OK };

enum BluetoothHandsfreeAudioState {
  HFP_AUDIO_STATE_DISCONNECTED,
  HFP_AUDIO_STATE_CONNECTING,
  HFP_AUDIO_STATE_CONNECTED,
  HFP_AUDIO_STATE_DISCONNECTING,
};

enum BluetoothHandsfreeCallAddressType {
  HFP_CALL_ADDRESS_TYPE_UNKNOWN,
  HFP_CALL_ADDRESS_TYPE_INTERNATIONAL
};

enum BluetoothHandsfreeCallDirection {
  HFP_CALL_DIRECTION_OUTGOING,
  HFP_CALL_DIRECTION_INCOMING
};

enum BluetoothHandsfreeCallHoldType {
  HFP_CALL_HOLD_RELEASEHELD,
  HFP_CALL_HOLD_RELEASEACTIVE_ACCEPTHELD,
  HFP_CALL_HOLD_HOLDACTIVE_ACCEPTHELD,
  HFP_CALL_HOLD_ADDHELDTOCONF
};

enum BluetoothHandsfreeCallMode {
  HFP_CALL_MODE_VOICE,
  HFP_CALL_MODE_DATA,
  HFP_CALL_MODE_FAX
};

enum BluetoothHandsfreeCallMptyType {
  HFP_CALL_MPTY_TYPE_SINGLE,
  HFP_CALL_MPTY_TYPE_MULTI
};

enum BluetoothHandsfreeCallState {
  HFP_CALL_STATE_ACTIVE,
  HFP_CALL_STATE_HELD,
  HFP_CALL_STATE_DIALING,
  HFP_CALL_STATE_ALERTING,
  HFP_CALL_STATE_INCOMING,
  HFP_CALL_STATE_WAITING,
  HFP_CALL_STATE_IDLE
};

enum BluetoothHandsfreeConnectionState {
  HFP_CONNECTION_STATE_DISCONNECTED,
  HFP_CONNECTION_STATE_CONNECTING,
  HFP_CONNECTION_STATE_CONNECTED,
  HFP_CONNECTION_STATE_SLC_CONNECTED,
  HFP_CONNECTION_STATE_DISCONNECTING
};

enum BluetoothHandsfreeNetworkState {
  HFP_NETWORK_STATE_NOT_AVAILABLE,
  HFP_NETWORK_STATE_AVAILABLE
};

enum BluetoothHandsfreeNRECState { HFP_NREC_STOPPED, HFP_NREC_STARTED };

enum BluetoothHandsfreeServiceType {
  HFP_SERVICE_TYPE_HOME,
  HFP_SERVICE_TYPE_ROAMING
};

enum BluetoothHandsfreeVoiceRecognitionState {
  HFP_VOICE_RECOGNITION_STOPPED,
  HFP_VOICE_RECOGNITION_STARTED
};

enum BluetoothHandsfreeVolumeType {
  HFP_VOLUME_TYPE_SPEAKER,
  HFP_VOLUME_TYPE_MICROPHONE
};

enum BluetoothHandsfreeWbsConfig {
  HFP_WBS_NONE, /* Neither CVSD nor mSBC codec, but other optional codec.*/
  HFP_WBS_NO,   /* CVSD */
  HFP_WBS_YES   /* mSBC */
};

// HF Indicators HFP 1.7
enum BluetoothHandsfreeHfIndType {
  HFP_HF_IND_NONE,
  HFP_HF_IND_ENHANCED_DRIVER_SAFETY,
  HFP_HF_IND_BATTERY_LEVEL_STATUS,
};

class BluetoothSignal;

class BluetoothSignalObserver : public mozilla::Observer<BluetoothSignal> {
 public:
  BluetoothSignalObserver() : mSignalRegistered(false) {}

  void SetSignalRegistered(bool aSignalRegistered) {
    mSignalRegistered = aSignalRegistered;
  }

 protected:
  bool mSignalRegistered;
};

// Enums for object types, currently used for shared function lookups
// (get/setproperty, etc...). Possibly discernable via dbus paths, but this
// method is future-proofed for platform independence.
enum BluetoothObjectType {
  TYPE_MANAGER = 0,
  TYPE_ADAPTER = 1,
  TYPE_DEVICE = 2,
  NUM_TYPE
};

enum BluetoothA2dpAudioState {
  A2DP_AUDIO_STATE_REMOTE_SUSPEND,
  A2DP_AUDIO_STATE_STOPPED,
  A2DP_AUDIO_STATE_STARTED,
};

enum BluetoothA2dpConnectionState {
  A2DP_CONNECTION_STATE_DISCONNECTED,
  A2DP_CONNECTION_STATE_CONNECTING,
  A2DP_CONNECTION_STATE_CONNECTED,
  A2DP_CONNECTION_STATE_DISCONNECTING
};

// Note: Keep in sync with BluetoothMessageUtils.h IPC serializer.
enum ControlPlayStatus {
  PLAYSTATUS_STOPPED = 0x00,
  PLAYSTATUS_PLAYING = 0x01,
  PLAYSTATUS_PAUSED = 0x02,
  PLAYSTATUS_FWD_SEEK = 0x03,
  PLAYSTATUS_REV_SEEK = 0x04,
  PLAYSTATUS_UNKNOWN,
  PLAYSTATUS_ERROR = 0xFF,
};

enum { AVRCP_UID_SIZE = 8 };
enum { AVRCP_FEATURE_BIT_MASK_SIZE = 16 };

enum BluetoothAvrcpMediaAttribute {
  AVRCP_MEDIA_ATTRIBUTE_TITLE = 0x01,
  AVRCP_MEDIA_ATTRIBUTE_ARTIST = 0x02,
  AVRCP_MEDIA_ATTRIBUTE_ALBUM = 0x03,
  AVRCP_MEDIA_ATTRIBUTE_TRACK_NUM = 0x04,
  AVRCP_MEDIA_ATTRIBUTE_NUM_TRACKS = 0x05,
  AVRCP_MEDIA_ATTRIBUTE_GENRE = 0x06,
  AVRCP_MEDIA_ATTRIBUTE_PLAYING_TIME = 0x07
};

enum BluetoothAvrcpPlayerAttribute {
  AVRCP_PLAYER_ATTRIBUTE_EQUALIZER,
  AVRCP_PLAYER_ATTRIBUTE_REPEAT,
  AVRCP_PLAYER_ATTRIBUTE_SHUFFLE,
  AVRCP_PLAYER_ATTRIBUTE_SCAN
};

enum BluetoothAvrcpPlayerRepeatValue {
  AVRCP_PLAYER_VAL_OFF_REPEAT = 0x01,
  AVRCP_PLAYER_VAL_SINGLE_REPEAT = 0x02,
  AVRCP_PLAYER_VAL_ALL_REPEAT = 0x03,
  AVRCP_PLAYER_VAL_GROUP_REPEAT = 0x04
};

enum BluetoothAvrcpPlayerShuffleValue {
  AVRCP_PLAYER_VAL_OFF_SHUFFLE = 0x01,
  AVRCP_PLAYER_VAL_ALL_SHUFFLE = 0x02,
  AVRCP_PLAYER_VAL_GROUP_SHUFFLE = 0x03
};

enum BluetoothAvrcpStatus {
  AVRCP_STATUS_BAD_COMMAND = 0x00,    // Invalid command
  AVRCP_STATUS_BAD_PARAMETER = 0x01,  // Invalid parameter
  AVRCP_STATUS_NOT_FOUND = 0x02,  // Specified parameter is wrong or not found
  AVRCP_STATUS_INTERNAL_ERROR = 0x03,  // Internal Error
  AVRCP_STATUS_SUCCESS = 0x04,         // Operation Success
  AVRCP_STATUS_UID_CHANGED = 0x05,     // UIDs changed
  AVRCP_STATUS_RESERVED = 0x06,        // Reserved
  AVRCP_STATUS_INV_DIRN = 0x07,        // Invalid direction
  AVRCP_STATUS_INV_DIRECTORY = 0x08,   // Invalid directory
  AVRCP_STATUS_INV_ITEM = 0x09,        // Invalid Ite

  AVRCP_STATUS_INV_SCOPE = 0x0a,       // Invalid scope
  AVRCP_STATUS_INV_RANGE = 0x0b,       // Invalid range
  AVRCP_STATUS_DIRECTORY = 0x0c,       // UID is a directory
  AVRCP_STATUS_MEDIA_IN_USE = 0x0d,    // Media in use
  AVRCP_STATUS_PLAY_LIST_FULL = 0x0e,  // Playing list full
  AVRCP_STATUS_SRCH_NOT_SPRTD = 0x0f,  // Search not supported
  AVRCP_STATUS_SRCH_IN_PROG = 0x10,    // Search in progress
  AVRCP_STATUS_INV_PLAYER = 0x11,      // Invalid player
  AVRCP_STATUS_PLAY_NOT_BROW = 0x12,   // Player not browsable
  AVRCP_STATUS_PLAY_NOT_ADDR = 0x13,   // Player not addressed
  AVRCP_STATUS_INV_RESULTS = 0x14,     // Invalid results
  AVRCP_STATUS_NO_AVBL_PLAY = 0x15,    // No available players
  AVRCP_STATUS_ADDR_PLAY_CHGD = 0x16,  // Addressed player changed
};

enum BluetoothAvrcpEvent {
  AVRCP_EVENT_PLAY_STATUS_CHANGED = 0x01,
  AVRCP_EVENT_TRACK_CHANGE = 0x02,
  AVRCP_EVENT_TRACK_REACHED_END = 0x03,
  AVRCP_EVENT_TRACK_REACHED_START = 0x04,
  AVRCP_EVENT_PLAY_POS_CHANGED = 0x05,
  AVRCP_EVENT_APP_SETTINGS_CHANGED = 0x08,
  AVRCP_EVENT_NOW_PLAYING_CONTENT_CHANGED = 0x09,
  AVRCP_EVENT_AVAL_PLAYER_CHANGE = 0x0a,
  AVRCP_EVENT_ADDR_PLAYER_CHANGE = 0x0b,
  AVRCP_EVENT_UIDS_CHANGED = 0x0c,
  AVRCP_EVENT_VOL_CHANGED = 0x0d,
};

enum BluetoothAvrcpNotification { AVRCP_NTF_INTERIM, AVRCP_NTF_CHANGED };

enum BluetoothAvrcpRemoteFeatureBits {
  AVRCP_REMOTE_FEATURE_NONE,
  AVRCP_REMOTE_FEATURE_METADATA = 0x01,
  AVRCP_REMOTE_FEATURE_ABSOLUTE_VOLUME = 0x02,
  AVRCP_REMOTE_FEATURE_BROWSE = 0x04
};

enum BluetoothAvrcpItemType {
  AVRCP_ITEM_PLAYER = 0x01,
  AVRCP_ITEM_FOLDER = 0x02,
  AVRCP_ITEM_MEDIA = 0x03,
};

enum BluetoothAvrcpFolderScope : uint8_t {
  AVRCP_SCOPE_PLAYER_LIST = 0x00,  // Media Player List
  AVRCP_SCOPE_FILE_SYSTEM = 0x01,  // Virtual File System
  AVRCP_SCOPE_SEARCH = 0x02,       // Search
  AVRCP_SCOPE_NOW_PLAYING = 0x03,  // Now Playing
};

struct BluetoothAvrcpElementAttribute {
  uint32_t mId;
  nsString mValue;
};

struct BluetoothAvrcpNotificationParam {
  ControlPlayStatus mPlayStatus;
  uint8_t mTrack[8];
  uint32_t mSongPos;
  uint8_t mNumAttr;
  uint8_t mIds[256];
  uint8_t mValues[256];
  uint16_t mPlayerId;
  uint16_t mUidCounter;
};

struct BluetoothAvrcpPlayerSettings {
  uint8_t mNumAttr;
  uint8_t mIds[256];
  uint8_t mValues[256];
};

struct BluetoothAvrcpItemPlayer {
  uint16_t mPlayerId;
  uint8_t mMajorType;
  uint32_t mSubType;
  uint8_t mPlayStatus;
  uint8_t mFeatures[AVRCP_FEATURE_BIT_MASK_SIZE] = {0};
  uint16_t mCharsetId;
  nsString mName;
};

enum BluetoothAttRole { ATT_SERVER_ROLE, ATT_CLIENT_ROLE };

enum BluetoothGattStatus {
  GATT_STATUS_SUCCESS,
  GATT_STATUS_INVALID_HANDLE,
  GATT_STATUS_READ_NOT_PERMITTED,
  GATT_STATUS_WRITE_NOT_PERMITTED,
  GATT_STATUS_INVALID_PDU,
  GATT_STATUS_INSUFFICIENT_AUTHENTICATION,
  GATT_STATUS_REQUEST_NOT_SUPPORTED,
  GATT_STATUS_INVALID_OFFSET,
  GATT_STATUS_INSUFFICIENT_AUTHORIZATION,
  GATT_STATUS_PREPARE_QUEUE_FULL,
  GATT_STATUS_ATTRIBUTE_NOT_FOUND,
  GATT_STATUS_ATTRIBUTE_NOT_LONG,
  GATT_STATUS_INSUFFICIENT_ENCRYPTION_KEY_SIZE,
  GATT_STATUS_INVALID_ATTRIBUTE_LENGTH,
  GATT_STATUS_UNLIKELY_ERROR,
  GATT_STATUS_INSUFFICIENT_ENCRYPTION,
  GATT_STATUS_UNSUPPORTED_GROUP_TYPE,
  GATT_STATUS_INSUFFICIENT_RESOURCES,
  GATT_STATUS_UNKNOWN_ERROR,
  GATT_STATUS_BEGIN_OF_APPLICATION_ERROR = 0x80,
  GATT_STATUS_END_OF_APPLICATION_ERROR = 0x9f,
  GATT_STATUS_END_OF_ERROR = 0x100
};

enum BluetoothGattAuthReq {
  GATT_AUTH_REQ_NONE,
  GATT_AUTH_REQ_NO_MITM,
  GATT_AUTH_REQ_MITM,
  GATT_AUTH_REQ_SIGNED_NO_MITM,
  GATT_AUTH_REQ_SIGNED_MITM,
  GATT_AUTH_REQ_END_GUARD
};

enum BluetoothGattWriteType {
  GATT_WRITE_TYPE_NO_RESPONSE,
  GATT_WRITE_TYPE_NORMAL,
  GATT_WRITE_TYPE_PREPARE,
  GATT_WRITE_TYPE_SIGNED,
  GATT_WRITE_TYPE_END_GUARD
};

enum BluetoothGattDbType {
  GATT_DB_TYPE_PRIMARY_SERVICE,
  GATT_DB_TYPE_SECONDARY_SERVICE,
  GATT_DB_TYPE_INCLUDED_SERVICE,
  GATT_DB_TYPE_CHARACTERISTIC,
  GATT_DB_TYPE_DESCRIPTOR,
  GATT_DB_TYPE_END_GUARD
};

/*
 * Bluetooth GATT Characteristic Properties bit field
 */
enum BluetoothGattCharPropBit {
  GATT_CHAR_PROP_BIT_BROADCAST = (1 << 0),
  GATT_CHAR_PROP_BIT_READ = (1 << 1),
  GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE = (1 << 2),
  GATT_CHAR_PROP_BIT_WRITE = (1 << 3),
  GATT_CHAR_PROP_BIT_NOTIFY = (1 << 4),
  GATT_CHAR_PROP_BIT_INDICATE = (1 << 5),
  GATT_CHAR_PROP_BIT_SIGNED_WRITE = (1 << 6),
  GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES = (1 << 7)
};

/*
 * BluetoothGattCharProp is used to store a bit mask value which contains
 * each corresponding bit value of each BluetoothGattCharPropBit.
 */
typedef uint8_t BluetoothGattCharProp;
#define BLUETOOTH_EMPTY_GATT_CHAR_PROP static_cast<BluetoothGattCharProp>(0x00)

/*
 * Bluetooth GATT Attribute Permissions bit field
 */
enum BluetoothGattAttrPermBit {
  GATT_ATTR_PERM_BIT_READ = (1 << 0),
  GATT_ATTR_PERM_BIT_READ_ENCRYPTED = (1 << 1),
  GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM = (1 << 2),
  GATT_ATTR_PERM_BIT_WRITE = (1 << 4),
  GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED = (1 << 5),
  GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM = (1 << 6),
  GATT_ATTR_PERM_BIT_WRITE_SIGNED = (1 << 7),
  GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM = (1 << 8)
};

/*
 * BluetoothGattAttrPerm is used to store a bit mask value which contains
 * each corresponding bit value of each BluetoothGattAttrPermBit.
 */
typedef uint16_t BluetoothGattAttrPerm;
#define BLUETOOTH_EMPTY_GATT_ATTR_PERM static_cast<BluetoothGattAttrPerm>(0x00)

struct BluetoothGattAdvData {
  uint8_t mAdvData[62];
};

struct BluetoothGattId {
  BluetoothUuid mUuid;
  uint8_t mInstanceId;

  bool operator==(const BluetoothGattId& aOther) const {
    return mUuid == aOther.mUuid && mInstanceId == aOther.mInstanceId;
  }
};

struct BluetoothGattCharAttribute {
  BluetoothGattId mId;
  BluetoothGattCharProp mProperties;
  BluetoothGattWriteType mWriteType;

  bool operator==(const BluetoothGattCharAttribute& aOther) const {
    return mId == aOther.mId && mProperties == aOther.mProperties &&
           mWriteType == aOther.mWriteType;
  }
};

struct BluetoothAttributeHandle {
  uint16_t mHandle;

  BluetoothAttributeHandle() : mHandle(0x0000) {}

  bool operator==(const BluetoothAttributeHandle& aOther) const {
    return mHandle == aOther.mHandle;
  }
};

struct BluetoothGattReadParam {
  BluetoothAttributeHandle mHandle;
  uint16_t mValueType;
  uint16_t mValueLength;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];
  uint8_t mStatus;
};

struct BluetoothGattNotifyParam {
  BluetoothAddress mBdAddr;
  BluetoothAttributeHandle mHandle;
  uint16_t mLength;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];
  bool mIsNotify;
};

struct BluetoothGattTestParam {
  BluetoothAddress mBdAddr;
  BluetoothUuid mUuid;
  uint16_t mU1;
  uint16_t mU2;
  uint16_t mU3;
  uint16_t mU4;
  uint16_t mU5;
};

struct BluetoothGattDbElement {
  uint16_t mId;
  BluetoothUuid mUuid;
  BluetoothGattDbType mType;
  BluetoothAttributeHandle mHandle;
  uint16_t mStartHandle;
  uint16_t mEndHandle;
  BluetoothGattCharProp mProperties;
  BluetoothGattAttrPerm mPermissions;

  bool operator==(const BluetoothGattDbElement& aOther) const {
    return mId == aOther.mId && mUuid == aOther.mUuid &&
           mType == aOther.mType && mHandle == aOther.mHandle &&
           mStartHandle == aOther.mStartHandle &&
           mEndHandle == aOther.mEndHandle &&
           mProperties == aOther.mProperties &&
           mPermissions == aOther.mPermissions;
  }
};

struct BluetoothGattResponse {
  BluetoothAttributeHandle mHandle;
  uint16_t mOffset;
  uint16_t mLength;
  BluetoothGattAuthReq mAuthReq;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];

  bool operator==(const BluetoothGattResponse& aOther) const {
    return mHandle == aOther.mHandle && mOffset == aOther.mOffset &&
           mLength == aOther.mLength && mAuthReq == aOther.mAuthReq &&
           !memcmp(mValue, aOther.mValue, mLength);
  }
};

/**
 * EIR Data Type, Advertising Data Type (AD Type) and OOB Data Type Definitions
 * Please refer to https://www.bluetooth.org/en-us/specification/\
 * assigned-numbers/generic-access-profile
 */
enum BluetoothGapDataType {
  GAP_INCOMPLETE_UUID16 =
      0X02,                    // Incomplete List of 16-bit Service Class UUIDs
  GAP_COMPLETE_UUID16 = 0X03,  // Complete List of 16-bit Service Class UUIDs
  GAP_INCOMPLETE_UUID32 =
      0X04,                    // Incomplete List of 32-bit Service Class UUIDs
  GAP_COMPLETE_UUID32 = 0X05,  // Complete List of 32-bit Service Class UUIDs
  GAP_INCOMPLETE_UUID128 =
      0X06,  // Incomplete List of 128-bit Service Class UUIDs
  GAP_COMPLETE_UUID128 = 0X07,  // Complete List of 128-bit Service Class UUIDs
  GAP_SHORTENED_NAME = 0X08,    // Shortened Local Name
  GAP_COMPLETE_NAME = 0X09,     // Complete Local Name
};

struct BluetoothGattAdvertisingData {
  /**
   * Uuid value of Appearance characteristic of the GAP service which can be
   * mapped to an icon or string that describes the physical representation of
   * the device during the device discovery procedure.
   */
  uint16_t mAppearance;

  /**
   * Whether to broadcast with device name or not.
   */
  bool mIncludeDevName;

  /**
   * Bluetooth device name
   */
  BluetoothRemoteName mDeviceName;

  /**
   * Whether to broadcast with TX power or not.
   */
  bool mIncludeTxPower;

  /**
   * Byte array of custom manufacturer specific data.
   *
   * The first 2 octets contain the Company Identifier Code followed by
   * additional manufacturer specific data. See Core Specification Supplement
   * (CSS) v6 1.4 for more details.
   */
  CopyableTArray<uint8_t> mManufacturerData;

  /**
   * Consists of a service UUID with the data associated with that service.
   * Please see Core Specification Supplement (CSS) v6 1.11 for more details.
   */
  CopyableTArray<uint8_t> mServiceData;

  /**
   * A list of Service or Service Class UUIDs.
   * Please see Core Specification Supplement (CSS) v6 1.1 for more details.
   */
  CopyableTArray<BluetoothUuid> mServiceUuids;

  BluetoothGattAdvertisingData()
      : mAppearance(0), mIncludeDevName(false), mIncludeTxPower(false) {}

  bool operator==(const BluetoothGattAdvertisingData& aOther) const {
    return mIncludeDevName == aOther.mIncludeDevName &&
           mIncludeTxPower == aOther.mIncludeTxPower &&
           mAppearance == aOther.mAppearance &&
           mManufacturerData == aOther.mManufacturerData &&
           mServiceData == aOther.mServiceData &&
           mServiceUuids == aOther.mServiceUuids;
  }
};

// Enums for SDP record types
enum BluetoothSdpType {
  SDP_TYPE_RAW,         // Used to carry raw SDP search data for unknown UUIDs
  SDP_TYPE_MAP_MAS,     // Message Access Profile - Server
  SDP_TYPE_MAP_MNS,     // Message Access Profile - Client (Notification Server)
  SDP_TYPE_PBAP_PSE,    // Phone Book Profile - Server
  SDP_TYPE_PBAP_PCE,    // Phone Book Profile - Client
  SDP_TYPE_OPP_SERVER,  // Object Push Profile
  SDP_TYPE_SAP_SERVER   // SIM Access Profile
};

struct BluetoothSdpRecord {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothSdpRecord);

  BluetoothSdpType mType;
  BluetoothUuid mUuid;
  nsCString mServiceName;
  int32_t mRfcommChannelNumber;
  int32_t mL2capPsm;
  int32_t mProfileVersion;

  BluetoothSdpRecord() {}

  // No l2cap psm
  BluetoothSdpRecord(BluetoothSdpType aType, BluetoothUuid aUuid,
                     const nsACString& aServiceName,
                     int32_t aRfcommChannelNumber,
                     int32_t aProfileVersion)
      : mType(aType),
        mUuid(aUuid),
        mServiceName(aServiceName),
        mRfcommChannelNumber(aRfcommChannelNumber),
        mProfileVersion(aProfileVersion) {}

  // l2cap psm
  BluetoothSdpRecord(BluetoothSdpType aType, BluetoothUuid aUuid,
                     const nsACString& aServiceName,
                     int32_t aRfcommChannelNumber, int32_t aL2capPsm,
                     int32_t aProfileVersion)
      : mType(aType),
        mUuid(aUuid),
        mServiceName(aServiceName),
        mRfcommChannelNumber(aRfcommChannelNumber),
        mL2capPsm(aL2capPsm),
        mProfileVersion(aProfileVersion) {}

 protected:
  ~BluetoothSdpRecord() {}
};

#define DEFAULT_RFCOMM_CHANNEL_OPS 12
#define DEFAULT_RFCOMM_CHANNEL_PBS 19
#define DEFAULT_RFCOMM_CHANNEL_MAS 21
#define DEFAULT_RFCOMM_CHANNEL_MNS 22
#define DEFAULT_L2CAP_PSM_MAS      0x1029
#define DEFAULT_L2CAP_PSM_PBAP     0x1025

// MAP 1.1
#define MAP_PROFILE_VERSION 0x0101
// MAP 1.3
#define MAP_PROFILE_VERSION_13 0x0103

/*
 * Gonk supports feature bit 0, 1, 2, 3 and 4
 * Bit 0 = Notification Registration Feature
 * Bit 1 = Notification Feature
 * Bit 2 = Browsing Feature
 * Bit 3 = Uploading Feature
 * Bit 4 = Delete Feature
 * Bit 5 = Instance Information Feature
 * Bit 6 = Extended Event Report 1.1
 * Bit 7 = Event Report Version 1.2
 * Bit 8 = Message Format Version 1.1
 * Bit 9 = Messages-Listing Format Version 1.1
 * Bit 10 = Persistent Message Handles
 * Bit 11 = Database Identifier
 * Bit 12 = Folder Version Counter
 * Bit 13 = Conversation Version Counters
 * Bit 14 = Participant Presence Change Notification
 * Bit 15 = Participant Chat State Change Notification
 * Bit 16 = PBAP Contact Cross Reference
 * Bit 17 = Notification Filtering
 * Bit 18 = UTC Offset Timestamp Format
 * Bit 19 = MapSupportedFeatures in Connect Request
 * Bit 20 = Conversation listing
 * Bit 21 = Owner Status
 */

// Backwards compatibility: If the MapSupportedFeatures attribute is not present
// 0x0000001F shall be assumed for a remote MSE.
#define MAP_SUPPORTED_FEATURES 0x0000001F

/*
 * Gonk supports feature bit 1 and 2
 * Bit 0 = EMAIL
 * Bit 1 = SMS_GSM
 * Bit 2 = SMS_CDMA
 * Bit 3 = MMS
 */
#define MAP_SUPPORTED_MSG_TYPES 0x06

struct BluetoothMasRecord : public BluetoothSdpRecord {
 public:
  uint32_t mSupportedFeatures;
  uint32_t mSupportedContentTypes;
  uint32_t mInstanceId;

  BluetoothMasRecord() {}

  BluetoothMasRecord(int32_t aRfcommChannelNumber, int32_t aInstanceId)
      : BluetoothSdpRecord(SDP_TYPE_MAP_MAS, BluetoothUuid(MAP_MAS),
                           "SMS Message Access"_ns, aRfcommChannelNumber,
                           // don't specify L2CAP PSM
                           MAP_PROFILE_VERSION),
        mSupportedFeatures(MAP_SUPPORTED_FEATURES),
        mSupportedContentTypes(MAP_SUPPORTED_MSG_TYPES),
        mInstanceId(aInstanceId) {}

  BluetoothMasRecord(int32_t aRfcommChannelNumber, int32_t aL2capPsm, int32_t aInstanceId)
      : BluetoothSdpRecord(SDP_TYPE_MAP_MAS, BluetoothUuid(MAP_MAS),
                           "SMS Message Access"_ns, aRfcommChannelNumber,
                           aL2capPsm,
                           MAP_PROFILE_VERSION),
        mSupportedFeatures(MAP_SUPPORTED_FEATURES),
        mSupportedContentTypes(MAP_SUPPORTED_MSG_TYPES),
        mInstanceId(aInstanceId) {}
};

struct BluetoothMnsRecord : public BluetoothSdpRecord {
 public:
  uint32_t mSupportedFeatures;

  BluetoothMnsRecord() {}
};

// PBAP version
#define PBAP_PROFILE_VERSION_11 0x0101
#define PBAP_PROFILE_VERSION_12 0x0102

/*
 * PBAP supported features
 * Bit 0 = Download
 * Bit 1 = Browsing
 * Bit 2 = Database Identifier
 * Bit 3 = Folder Version Counters
 * Bit 4 = vCard Selecting
 * Bit 5 = Enhanced Missed Calls
 * Bit 6 = X-BT-UCI vCard Property
 * Bit 7 = X-BT-UID vCard Property
 * Bit 8 = Contact Referencing
 * Bit 9 = Default Contact Image Format
 * Bit 10 ~ 31 Reserved
 */
#define PBAP_FEATURES_BIT_DOWNLOAD              0
#define PBAP_FEATURES_BIT_BROWSING              1
#define PBAP_FEATURES_BIT_DB_ID                 2
#define PBAP_FEATURES_BIT_FOLDER_VER_COUNTERS   3
#define PBAP_FEATURES_BIT_VCARD_SELECTING       4
#define PBAP_FEATURES_BIT_ENHANCED_MISSED_CALLS 5
#define PBAP_FEATURES_BIT_UCI_VCARD             6
#define PBAP_FEATURES_BIT_UID_VCARD             7
#define PBAP_FEATURES_BIT_CONTACT_REFERENCING   8
#define PBAP_FEATURES_BIT_DEF_CONTACT_IMAGE_FOR 9

// PSE mandatory support features are defined in the PBAP SPEC 4.1
#define PBAP_SUPPORTED_FEATURES (1 << PBAP_FEATURES_BIT_DOWNLOAD) | \
                                (1 << PBAP_FEATURES_BIT_BROWSING) | \
                                (1 << PBAP_FEATURES_BIT_DB_ID) | \
                                (1 << PBAP_FEATURES_BIT_FOLDER_VER_COUNTERS) | \
                                (1 << PBAP_FEATURES_BIT_VCARD_SELECTING) | \
                                (1 << PBAP_FEATURES_BIT_CONTACT_REFERENCING)

/*
 * PBAP supported repositories
 * Bit 0 = Local Phonebook
 * Bit 1 = SIM card
 * Bit 2 = Speed dial
 * Bit 3 = Favorites
 * Bit 4~7 reserved for future use
*/
#define PBAP_SUPPORTED_REPOSITORIES 0x03 /*local phonebook and SIM card*/

struct BLuetoothPbapSdpRecord : public BluetoothSdpRecord {
 public:
  uint32_t mSupportedFeatures;
  uint32_t mSupportedRepositories;

  BLuetoothPbapSdpRecord() {}

  BLuetoothPbapSdpRecord(int32_t aRfcommChannelNumber,
                         uint32_t aSupportedFeatures,
                         uint32_t aSupportedRepositories)
      : BluetoothSdpRecord(SDP_TYPE_PBAP_PSE, BluetoothUuid(PBAP_PSE),
                           "Phone Book Access"_ns, aRfcommChannelNumber,
                           // don't specify L2CAP PSM
                           PBAP_PROFILE_VERSION_12),
        mSupportedFeatures(aSupportedFeatures),
        mSupportedRepositories(aSupportedRepositories) {}

  BLuetoothPbapSdpRecord(int32_t aRfcommChannelNumber, int32_t aL2capPsm,
                         uint32_t aSupportedFeatures,
                         uint32_t aSupportedRepositories)
      : BluetoothSdpRecord(SDP_TYPE_PBAP_PSE, BluetoothUuid(PBAP_PSE),
                           "Phone Book Access"_ns, aRfcommChannelNumber,
                           aL2capPsm,
                           PBAP_PROFILE_VERSION_12),
        mSupportedFeatures(aSupportedFeatures),
        mSupportedRepositories(aSupportedRepositories) {}
};

// Default TX power of LE advertising
static const int16_t kDefaultTxPower = -7;

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_BluetoothCommon_h
