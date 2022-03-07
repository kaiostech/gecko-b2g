/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h
#define mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothAddress> {
  typedef mozilla::dom::bluetooth::BluetoothAddress paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(aParam.mAddr); ++i) {
      WriteParam(aWriter, aParam.mAddr[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(aResult->mAddr); ++i) {
      if (!ReadParam(aReader, aResult->mAddr + i)) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothObjectType>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothObjectType,
          mozilla::dom::bluetooth::TYPE_MANAGER,
          mozilla::dom::bluetooth::NUM_TYPE> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothPinCode> {
  typedef mozilla::dom::bluetooth::BluetoothPinCode paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    auto length = aParam.mLength;
    if (length > MOZ_ARRAY_LENGTH(aParam.mPinCode)) {
      length = MOZ_ARRAY_LENGTH(aParam.mPinCode);
    }

    WriteParam(aWriter, length);
    for (uint8_t i = 0; i < length; ++i) {
      WriteParam(aWriter, aParam.mPinCode[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mLength)) {
      return false;
    }

    auto maxLength = MOZ_ARRAY_LENGTH(aResult->mPinCode);

    if (aResult->mLength > maxLength) {
      return false;
    }
    for (uint8_t i = 0; i < aResult->mLength; ++i) {
      if (!ReadParam(aReader, aResult->mPinCode + i)) {
        return false;
      }
    }
    for (uint8_t i = aResult->mLength; i < maxLength; ++i) {
      aResult->mPinCode[i] = 0;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothRemoteName> {
  typedef mozilla::dom::bluetooth::BluetoothRemoteName paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLength);
    for (size_t i = 0; i < aParam.mLength; ++i) {
      WriteParam(aWriter, aParam.mName[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mLength)) {
      return false;
    }
    if (aResult->mLength > MOZ_ARRAY_LENGTH(aResult->mName)) {
      return false;
    }
    for (uint8_t i = 0; i < aResult->mLength; ++i) {
      if (!ReadParam(aReader, aResult->mName + i)) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothSspVariant>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothSspVariant,
          mozilla::dom::bluetooth::SSP_VARIANT_PASSKEY_CONFIRMATION,
          mozilla::dom::bluetooth::NUM_SSP_VARIANT> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothStatus>
    : public ContiguousEnumSerializer<mozilla::dom::bluetooth::BluetoothStatus,
                                      mozilla::dom::bluetooth::STATUS_SUCCESS,
                                      mozilla::dom::bluetooth::NUM_STATUS> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattWriteType>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothGattWriteType,
          mozilla::dom::bluetooth::GATT_WRITE_TYPE_NO_RESPONSE,
          mozilla::dom::bluetooth::GATT_WRITE_TYPE_END_GUARD> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattAuthReq>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothGattAuthReq,
          mozilla::dom::bluetooth::GATT_AUTH_REQ_NONE,
          mozilla::dom::bluetooth::GATT_AUTH_REQ_END_GUARD> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattDbType>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothGattDbType,
          mozilla::dom::bluetooth::GATT_DB_TYPE_PRIMARY_SERVICE,
          mozilla::dom::bluetooth::GATT_DB_TYPE_END_GUARD> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothUuid> {
  typedef mozilla::dom::bluetooth::BluetoothUuid paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    for (uint8_t i = 0; i < 16; i++) {
      WriteParam(aWriter, aParam.mUuid[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    for (uint8_t i = 0; i < 16; i++) {
      if (!ReadParam(aReader, &(aResult->mUuid[i]))) {
        return false;
      }
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattId> {
  typedef mozilla::dom::bluetooth::BluetoothGattId paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mUuid);
    WriteParam(aWriter, aParam.mInstanceId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mUuid)) ||
        !ReadParam(aReader, &(aResult->mInstanceId))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattCharAttribute> {
  typedef mozilla::dom::bluetooth::BluetoothGattCharAttribute paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mId);
    WriteParam(aWriter, aParam.mProperties);
    WriteParam(aWriter, aParam.mWriteType);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mId)) ||
        !ReadParam(aReader, &(aResult->mProperties)) ||
        !ReadParam(aReader, &(aResult->mWriteType))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothAttributeHandle> {
  typedef mozilla::dom::bluetooth::BluetoothAttributeHandle paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mHandle);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mHandle))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattDbElement> {
  typedef mozilla::dom::bluetooth::BluetoothGattDbElement paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mId);
    WriteParam(aWriter, aParam.mUuid);
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mHandle);
    WriteParam(aWriter, aParam.mStartHandle);
    WriteParam(aWriter, aParam.mEndHandle);
    WriteParam(aWriter, aParam.mProperties);
    WriteParam(aWriter, aParam.mPermissions);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mId)) ||
        !ReadParam(aReader, &(aResult->mUuid)) ||
        !ReadParam(aReader, &(aResult->mType)) ||
        !ReadParam(aReader, &(aResult->mHandle)) ||
        !ReadParam(aReader, &(aResult->mStartHandle)) ||
        !ReadParam(aReader, &(aResult->mEndHandle)) ||
        !ReadParam(aReader, &(aResult->mProperties)) ||
        !ReadParam(aReader, &(aResult->mPermissions))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattResponse> {
  typedef mozilla::dom::bluetooth::BluetoothGattResponse paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    auto length = aParam.mLength;
    if (length > MOZ_ARRAY_LENGTH(aParam.mValue)) {
      length = MOZ_ARRAY_LENGTH(aParam.mValue);
    }

    WriteParam(aWriter, aParam.mHandle);
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, length);
    WriteParam(aWriter, aParam.mAuthReq);
    for (uint16_t i = 0; i < length; i++) {
      WriteParam(aWriter, aParam.mValue[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mHandle)) ||
        !ReadParam(aReader, &(aResult->mOffset)) ||
        !ReadParam(aReader, &(aResult->mLength)) ||
        !ReadParam(aReader, &(aResult->mAuthReq))) {
      return false;
    }

    if (aResult->mLength > MOZ_ARRAY_LENGTH(aResult->mValue)) {
      return false;
    }

    for (uint16_t i = 0; i < aResult->mLength; i++) {
      if (!ReadParam(aReader, &(aResult->mValue[i]))) {
        return false;
      }
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::ControlPlayStatus>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::ControlPlayStatus,
          mozilla::dom::bluetooth::ControlPlayStatus::PLAYSTATUS_STOPPED,
          mozilla::dom::bluetooth::ControlPlayStatus::PLAYSTATUS_ERROR> {};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattAdvertisingData> {
  typedef mozilla::dom::bluetooth::BluetoothGattAdvertisingData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mAppearance);
    WriteParam(aWriter, aParam.mIncludeDevName);
    WriteParam(aWriter, aParam.mIncludeTxPower);
    WriteParam(aWriter, aParam.mManufacturerData);
    WriteParam(aWriter, aParam.mServiceData);
    WriteParam(aWriter, aParam.mServiceUuids);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mAppearance)) ||
        !ReadParam(aReader, &(aResult->mIncludeDevName)) ||
        !ReadParam(aReader, &(aResult->mIncludeTxPower)) ||
        !ReadParam(aReader, &(aResult->mManufacturerData)) ||
        !ReadParam(aReader, &(aResult->mServiceData)) ||
        !ReadParam(aReader, &(aResult->mServiceUuids))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattStatus>
    : public ContiguousEnumSerializer<
          mozilla::dom::bluetooth::BluetoothGattStatus,
          mozilla::dom::bluetooth::GATT_STATUS_SUCCESS,
          mozilla::dom::bluetooth::GATT_STATUS_END_OF_ERROR> {};

}  // namespace IPC

#endif  // mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h
