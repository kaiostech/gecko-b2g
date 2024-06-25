/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <netinet/in.h>

#include "BluetoothUtils.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "jsapi.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/BluetoothGattServerBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsISystemMessageService.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

BEGIN_BLUETOOTH_NAMESPACE

void AddressToString(const BluetoothAddress& aAddress, nsAString& aString) {
  char str[BLUETOOTH_ADDRESS_LENGTH + 1];

  int res = SprintfLiteral(
      str, "%02x:%02x:%02x:%02x:%02x:%02x", static_cast<int>(aAddress.mAddr[0]),
      static_cast<int>(aAddress.mAddr[1]), static_cast<int>(aAddress.mAddr[2]),
      static_cast<int>(aAddress.mAddr[3]), static_cast<int>(aAddress.mAddr[4]),
      static_cast<int>(aAddress.mAddr[5]));

  if ((res == EOF) || (res < 0) || (static_cast<size_t>(res) >= sizeof(str))) {
    /* Conversion should have succeeded or (a) we're out of memory, or
     * (b) our code is massively broken. We should crash in both cases.
     */
    MOZ_CRASH("Failed to convert Bluetooth address to string");
  }

  aString = NS_ConvertUTF8toUTF16(str);
}

nsresult StringToAddress(const nsAString& aString, BluetoothAddress& aAddress) {
  int res = sscanf(NS_ConvertUTF16toUTF8(aString).get(),
                   "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                   &aAddress.mAddr[0], &aAddress.mAddr[1], &aAddress.mAddr[2],
                   &aAddress.mAddr[3], &aAddress.mAddr[4], &aAddress.mAddr[5]);
  if (res < static_cast<ssize_t>(MOZ_ARRAY_LENGTH(aAddress.mAddr))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

nsresult PinCodeToString(const BluetoothPinCode& aPinCode, nsAString& aString) {
  if (aPinCode.mLength > sizeof(aPinCode.mPinCode)) {
    BT_LOGR("Pin-code string too long");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aString = NS_ConvertUTF8toUTF16(nsCString(
      reinterpret_cast<const char*>(aPinCode.mPinCode), aPinCode.mLength));

  return NS_OK;
}

nsresult StringToPinCode(const nsAString& aString, BluetoothPinCode& aPinCode) {
  NS_ConvertUTF16toUTF8 stringUTF8(aString);

  auto len = stringUTF8.Length();

  if (len > sizeof(aPinCode.mPinCode)) {
    BT_LOGR("Pin-code string too long");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  auto str = stringUTF8.get();

  memcpy(aPinCode.mPinCode, str, len);
  memset(aPinCode.mPinCode + len, 0, sizeof(aPinCode.mPinCode) - len);
  aPinCode.mLength = len;

  return NS_OK;
}

nsresult StringToControlPlayStatus(const nsAString& aString,
                                   ControlPlayStatus& aPlayStatus) {
  if (aString.EqualsLiteral("STOPPED")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_STOPPED;
  } else if (aString.EqualsLiteral("PLAYING")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_PLAYING;
  } else if (aString.EqualsLiteral("PAUSED")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_PAUSED;
  } else if (aString.EqualsLiteral("FWD_SEEK")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_FWD_SEEK;
  } else if (aString.EqualsLiteral("REV_SEEK")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_REV_SEEK;
  } else if (aString.EqualsLiteral("ERROR")) {
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_ERROR;
  } else {
    BT_LOGR("Invalid play status: %s", NS_ConvertUTF16toUTF8(aString).get());
    aPlayStatus = ControlPlayStatus::PLAYSTATUS_UNKNOWN;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

nsresult StringToPropertyType(const nsAString& aString,
                              BluetoothPropertyType& aType) {
  if (aString.EqualsLiteral("Name")) {
    aType = PROPERTY_BDNAME;
  } else if (aString.EqualsLiteral("Discoverable")) {
    aType = PROPERTY_ADAPTER_SCAN_MODE;
  } else if (aString.EqualsLiteral("DiscoverableTimeout")) {
    aType = PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
  } else {
    BT_LOGR("Invalid property name: %s", NS_ConvertUTF16toUTF8(aString).get());
    aType = PROPERTY_UNKNOWN;  // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

nsresult NamedValueToProperty(const BluetoothNamedValue& aValue,
                              BluetoothProperty& aProperty) {
  nsresult rv = StringToPropertyType(aValue.name(), aProperty.mType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (aProperty.mType) {
    case PROPERTY_BDNAME:
      if (aValue.value().type() != BluetoothValue::TBluetoothRemoteName) {
        BT_LOGR("Bluetooth property value is not a remote name");
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // Set name
      aProperty.mRemoteName = aValue.value().get_BluetoothRemoteName();
      break;

    case PROPERTY_ADAPTER_SCAN_MODE:
      if (aValue.value().type() != BluetoothValue::Tbool) {
        BT_LOGR("Bluetooth property value is not a boolean");
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // Set scan mode
      if (aValue.value().get_bool()) {
        aProperty.mScanMode = SCAN_MODE_CONNECTABLE_DISCOVERABLE;
      } else {
        aProperty.mScanMode = SCAN_MODE_CONNECTABLE;
      }
      break;

    case PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
      if (aValue.value().type() != BluetoothValue::Tuint32_t) {
        BT_LOGR("Bluetooth property value is not an unsigned integer");
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // Set discoverable timeout
      aProperty.mUint32 = aValue.value().get_uint32_t();
      break;

    default:
      BT_LOGR("Invalid property value type");
      return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

void RemoteNameToString(const BluetoothRemoteName& aRemoteName,
                        nsAString& aString) {
  MOZ_ASSERT(aRemoteName.mLength <= sizeof(aRemoteName.mName));

  auto name = reinterpret_cast<const char*>(aRemoteName.mName);

  /* The content in |BluetoothRemoteName| is not a C string and not
   * terminated by \0. We use |mLength| to limit its length.
   */
  aString = NS_ConvertUTF8toUTF16(name, aRemoteName.mLength);
}

nsresult StringToServiceName(const nsAString& aString,
                             BluetoothServiceName& aServiceName) {
  NS_ConvertUTF16toUTF8 serviceNameUTF8(aString);

  auto len = serviceNameUTF8.Length();

  if (len > sizeof(aServiceName.mName)) {
    BT_LOGR("Service-name string too long");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  auto str = serviceNameUTF8.get();

  memcpy(aServiceName.mName, str, len);
  memset(aServiceName.mName + len, 0, sizeof(aServiceName.mName) - len);

  return NS_OK;
}

void UuidToString(const BluetoothUuid& aUuid, nsAString& aString) {
  char uuidStr[37];
  uint32_t uuid0, uuid4;
  uint16_t uuid1, uuid2, uuid3, uuid5;

  memcpy(&uuid0, &aUuid.mUuid[0], sizeof(uint32_t));
  memcpy(&uuid1, &aUuid.mUuid[4], sizeof(uint16_t));
  memcpy(&uuid2, &aUuid.mUuid[6], sizeof(uint16_t));
  memcpy(&uuid3, &aUuid.mUuid[8], sizeof(uint16_t));
  memcpy(&uuid4, &aUuid.mUuid[10], sizeof(uint32_t));
  memcpy(&uuid5, &aUuid.mUuid[14], sizeof(uint16_t));

  SprintfLiteral(uuidStr, "%.8x-%.4x-%.4x-%.4x-%.8x%.4x", ntohl(uuid0),
                 ntohs(uuid1), ntohs(uuid2), ntohs(uuid3), ntohl(uuid4),
                 ntohs(uuid5));

  aString.Truncate();
  aString.AssignLiteral(uuidStr);
}

nsresult StringToUuid(const nsAString& aString, BluetoothUuid& aUuid) {
  uint32_t uuid0, uuid4;
  uint16_t uuid1, uuid2, uuid3, uuid5;

  auto res = sscanf(NS_ConvertUTF16toUTF8(aString).get(),
                    "%08x-%04hx-%04hx-%04hx-%08x%04hx", &uuid0, &uuid1, &uuid2,
                    &uuid3, &uuid4, &uuid5);
  if (res == EOF || res < 6) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uuid0 = htonl(uuid0);
  uuid1 = htons(uuid1);
  uuid2 = htons(uuid2);
  uuid3 = htons(uuid3);
  uuid4 = htonl(uuid4);
  uuid5 = htons(uuid5);

  memcpy(&aUuid.mUuid[0], &uuid0, sizeof(uint32_t));
  memcpy(&aUuid.mUuid[4], &uuid1, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[6], &uuid2, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[8], &uuid3, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[10], &uuid4, sizeof(uint32_t));
  memcpy(&aUuid.mUuid[14], &uuid5, sizeof(uint16_t));

  return NS_OK;
}

nsresult BytesToUuid(const nsTArray<uint8_t>& aArray,
                     nsTArray<uint8_t>::index_type aOffset,
                     BluetoothUuidType aType, BluetoothProfileEndian aEndian,
                     BluetoothUuid& aUuid) {
  MOZ_ASSERT(aType == UUID_16_BIT || aType == UUID_32_BIT ||
             aType == UUID_128_BIT);
  MOZ_ASSERT(aEndian == ENDIAN_BIG || aEndian == ENDIAN_LITTLE);

  size_t index = (aType == UUID_16_BIT) ? 2 : 0;
  size_t length = 0;

  if (aType == UUID_16_BIT) {
    length = sizeof(uint16_t);
  } else if (aType == UUID_32_BIT) {
    length = sizeof(uint32_t);
  } else {
    length = MOZ_ARRAY_LENGTH(aUuid.mUuid);
  }

  if (aArray.Length() < aOffset + length) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aUuid = BluetoothUuid::BASE();

  if (aEndian == ENDIAN_BIG) {
    for (size_t i = 0; i < length; ++i) {
      aUuid.mUuid[index + i] = aArray[aOffset + i];
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      aUuid.mUuid[index + i] = aArray[aOffset + length - i - 1];
    }
  }

  return NS_OK;
}

nsresult UuidToBytes(const BluetoothUuid& aUuid, BluetoothUuidType aType,
                     BluetoothProfileEndian aEndian, nsTArray<uint8_t>& aArray,
                     nsTArray<uint8_t>::index_type aOffset) {
  MOZ_ASSERT(aType == UUID_16_BIT || aType == UUID_32_BIT ||
             aType == UUID_128_BIT);
  MOZ_ASSERT(aEndian == ENDIAN_BIG || aEndian == ENDIAN_LITTLE);

  size_t index = (aType == UUID_16_BIT) ? 2 : 0;
  size_t length = 0;

  if (aType == UUID_16_BIT) {
    length = sizeof(uint16_t);
  } else if (aType == UUID_32_BIT) {
    length = sizeof(uint32_t);
  } else {
    length = MOZ_ARRAY_LENGTH(aUuid.mUuid);
  }

  aArray.SetCapacity(aOffset + length);

  if (aEndian == ENDIAN_BIG) {
    for (size_t i = 0; i < length; ++i) {
      aArray[aOffset + i] = aUuid.mUuid[index + i];
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      aArray[aOffset + length - i - 1] = aUuid.mUuid[index + i];
    }
  }

  return NS_OK;
}

nsresult GenerateUuid(BluetoothUuid& aUuid) {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS(rv, rv);

  aUuid = BluetoothUuid(uuid.m0 >> 24, uuid.m0 >> 16, uuid.m0 >> 8, uuid.m0,
                        uuid.m1 >> 8, uuid.m1, uuid.m2 >> 8, uuid.m2,
                        uuid.m3[0], uuid.m3[1], uuid.m3[2], uuid.m3[3],
                        uuid.m3[4], uuid.m3[5], uuid.m3[6], uuid.m3[7]);
  return NS_OK;
}

nsresult GenerateUuid(nsAString& aUuidString) {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build a string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format
  char uuidBuffer[NSID_LENGTH];
  uuid.ToProvidedString(uuidBuffer);
  NS_ConvertASCIItoUTF16 uuidString(uuidBuffer);

  // Remove {} and the null terminator
  aUuidString.Assign(Substring(uuidString, 1, NSID_LENGTH - 3));

  return NS_OK;
}

void GattPermissionsToDictionary(BluetoothGattAttrPerm aBits,
                                 GattPermissions& aPermissions) {
  aPermissions.mRead = aBits & GATT_ATTR_PERM_BIT_READ;
  aPermissions.mReadEncrypted = aBits & GATT_ATTR_PERM_BIT_READ_ENCRYPTED;
  aPermissions.mReadEncryptedMITM =
      aBits & GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM;
  aPermissions.mWrite = aBits & GATT_ATTR_PERM_BIT_WRITE;
  aPermissions.mWriteEncrypted = aBits & GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED;
  aPermissions.mWriteEncryptedMITM =
      aBits & GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM;
  aPermissions.mWriteSigned = aBits & GATT_ATTR_PERM_BIT_WRITE_SIGNED;
  aPermissions.mWriteSignedMITM = aBits & GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM;
}

void GattPermissionsToBits(const GattPermissions& aPermissions,
                           BluetoothGattAttrPerm& aBits) {
  aBits = BLUETOOTH_EMPTY_GATT_ATTR_PERM;

  if (aPermissions.mRead) {
    aBits |= GATT_ATTR_PERM_BIT_READ;
  }
  if (aPermissions.mReadEncrypted) {
    aBits |= GATT_ATTR_PERM_BIT_READ_ENCRYPTED;
  }
  if (aPermissions.mReadEncryptedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM;
  }
  if (aPermissions.mWrite) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE;
  }
  if (aPermissions.mWriteEncrypted) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED;
  }
  if (aPermissions.mWriteEncryptedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM;
  }
  if (aPermissions.mWriteSigned) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_SIGNED;
  }
  if (aPermissions.mWriteSignedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM;
  }
}

void GattPropertiesToDictionary(BluetoothGattCharProp aBits,
                                GattCharacteristicProperties& aProperties) {
  aProperties.mBroadcast = aBits & GATT_CHAR_PROP_BIT_BROADCAST;
  aProperties.mRead = aBits & GATT_CHAR_PROP_BIT_READ;
  aProperties.mWriteNoResponse = aBits & GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE;
  aProperties.mWrite = aBits & GATT_CHAR_PROP_BIT_WRITE;
  aProperties.mNotify = aBits & GATT_CHAR_PROP_BIT_NOTIFY;
  aProperties.mIndicate = aBits & GATT_CHAR_PROP_BIT_INDICATE;
  aProperties.mSignedWrite = aBits & GATT_CHAR_PROP_BIT_SIGNED_WRITE;
  aProperties.mExtendedProps = aBits & GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES;
}

void GattPropertiesToBits(const GattCharacteristicProperties& aProperties,
                          BluetoothGattCharProp& aBits) {
  aBits = BLUETOOTH_EMPTY_GATT_CHAR_PROP;

  if (aProperties.mBroadcast) {
    aBits |= GATT_CHAR_PROP_BIT_BROADCAST;
  }
  if (aProperties.mRead) {
    aBits |= GATT_CHAR_PROP_BIT_READ;
  }
  if (aProperties.mWriteNoResponse) {
    aBits |= GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE;
  }
  if (aProperties.mWrite) {
    aBits |= GATT_CHAR_PROP_BIT_WRITE;
  }
  if (aProperties.mNotify) {
    aBits |= GATT_CHAR_PROP_BIT_NOTIFY;
  }
  if (aProperties.mIndicate) {
    aBits |= GATT_CHAR_PROP_BIT_INDICATE;
  }
  if (aProperties.mSignedWrite) {
    aBits |= GATT_CHAR_PROP_BIT_SIGNED_WRITE;
  }
  if (aProperties.mExtendedProps) {
    aBits |= GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES;
  }
}

nsresult AdvertisingDataToGattAdvertisingData(
    const BluetoothAdvertisingData& aAdvData,
    BluetoothGattAdvertisingData& aGattAdvData) {
  aGattAdvData.mAppearance = aAdvData.mAppearance;
  aGattAdvData.mIncludeDevName = aAdvData.mIncludeDevName;
  aGattAdvData.mIncludeTxPower = aAdvData.mIncludeTxPower;

  for (size_t i = 0; i < aAdvData.mServiceUuids.Length(); i++) {
    BluetoothUuid uuid;
    if (NS_WARN_IF(NS_FAILED(StringToUuid(aAdvData.mServiceUuids[i], uuid)))) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    aGattAdvData.mServiceUuids.AppendElement(uuid);
  }

  if (!aAdvData.mManufacturerData.IsNull()) {
    const ArrayBuffer& manufacturerData = aAdvData.mManufacturerData.Value();

    auto length = manufacturerData.ProcessFixedData(
        [](const Span<uint8_t>& aData) { return aData.Length(); });

    // Manufacturer Specific Data, 0xff
    aGattAdvData.mManufacturerData.SetLength(4);
    aGattAdvData.mServiceData[0] = 3 + length;
    aGattAdvData.mServiceData[1] = 0xff;
    // 3rd and 4th bytes are manufacturer ID in little-endian.
    LittleEndian::writeUint16(aGattAdvData.mManufacturerData.Elements(),
                              aAdvData.mManufacturerId);

    // Concatenate custom manufacturer data.
    manufacturerData.AppendDataTo(aGattAdvData.mManufacturerData);
  }

  if (!aAdvData.mServiceData.IsNull()) {
    BluetoothUuid uuid;
    if (NS_WARN_IF(NS_FAILED(StringToUuid(aAdvData.mServiceUuid, uuid)))) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    const ArrayBuffer& serviceData = aAdvData.mServiceData.Value();
    auto length = serviceData.ProcessFixedData(
        [](const Span<uint8_t>& aData) { return aData.Length(); });

    // Convert 128-bit UUID into 16-bit/32-bit if it's posible.
    if (uuid.IsUuid16Convertible()) {
      BT_LOGR("Sending serviceData with 16-bit UUID");
      // Service Data - 16-bit UUID,  0x16
      aGattAdvData.mServiceData.SetLength(4);
      aGattAdvData.mServiceData[0] = 3 + length;
      aGattAdvData.mServiceData[1] = 0x16;

      // 3rd and 4th bytes are 16-bit service UUID in little-endian.
      // Extract 16-bit UUID from a 128-bit UUID.
      // BASE_UUID: 00000000-0000-1000-8000-00805F9B34FB
      //                ^^^^
      //                16-bit UUID is composed by mUuid[2] and mUuid[3]
      aGattAdvData.mServiceData[3] = uuid.mUuid[3];
      aGattAdvData.mServiceData[4] = uuid.mUuid[2];
    } else if (uuid.IsUuid32Convertible()) {
      BT_LOGR("Sending serviceData with 32-bit UUID");
      // Service Data - 32-bit UUID,  0x20
      aGattAdvData.mServiceData.SetLength(6);
      aGattAdvData.mServiceData[0] = 5 + length;
      aGattAdvData.mServiceData[1] = 0x16;

      // 3rd - 6th bytes are 32-bit service UUID in little-endian.
      // Extract 32-bit UUID from a 128-bit UUID.
      // BASE_UUID: 00000000-0000-1000-8000-00805F9B34FB
      //            ^^^^^^^^
      //                32-bit UUID is composed by mUuid[0] ~ mUuid[3]
      aGattAdvData.mServiceData[3] = uuid.mUuid[3];
      aGattAdvData.mServiceData[4] = uuid.mUuid[2];
      aGattAdvData.mServiceData[5] = uuid.mUuid[1];
      aGattAdvData.mServiceData[6] = uuid.mUuid[0];
    } else {
      BT_LOGR("Sending serviceData with 128-bit UUID");
      // Service Data - 128-bit UUID, 0x21
      aGattAdvData.mServiceData.SetLength(18);
      aGattAdvData.mServiceData[0] = 17 + length;
      aGattAdvData.mServiceData[1] = 0x21;
      // 2nd - 18th bytes are service UUID in little-endian.
      for (size_t i = 0; i < sizeof(uuid.mUuid); i++) {
        aGattAdvData.mServiceData[i + 2] =
            uuid.mUuid[sizeof(uuid.mUuid) - i - 1];
      }
    }

    // Concatenate custom service data.
    serviceData.AppendDataTo(aGattAdvData.mServiceData);
  }

  return NS_OK;
}

void GattAdvertisingDataToBytes(const BluetoothGattAdvertisingData& aData,
                                nsTArray<uint8_t>& aArray) {
  aArray.Clear();

  // Service UUIDs
  if (!aData.mServiceUuids.IsEmpty()) {
    nsTArray<BluetoothUuid> uuid16s, uuid32s, uuid128s;
    for (size_t i = 0; i < aData.mServiceUuids.Length(); i++) {
      if (aData.mServiceUuids[i].IsUuid16Convertible()) {
        uuid16s.AppendElement(aData.mServiceUuids[i]);
      } else if (aData.mServiceUuids[i].IsUuid32Convertible()) {
        uuid32s.AppendElement(aData.mServiceUuids[i]);
      } else {
        uuid128s.AppendElement(aData.mServiceUuids[i]);
      }
    }

    // Incomplete List of 16-bit Service Class UUIDs,  0x02
    // Complete List of 16-bit Service Class UUIDs,    0x03
    if (!uuid16s.IsEmpty()) {
      size_t len = uuid16s.Length();
      uint8_t adType;
      if (len > 4) {
        len = 4;
        adType = 0x02;  // Incomplete List of 16-bit UUIDs
      } else {
        adType = 0x03;  // Complete List of 16-bit UUIDs
      }
      aArray.AppendElement(1 + len * 2);
      aArray.AppendElement(adType);
      for (size_t i = 0; i < len; i++) {
        uint16_t uuid16 = uuid16s[i].GetUuid16();
        aArray.AppendElement(uuid16 & 0xFF);
        aArray.AppendElement((uuid16 >> 8) & 0xFF);
      }
    }

    // Incomplete List of 32-bit Service Class UUIDs,  0x04
    // Complete List of 32-bit Service Class UUIDs,    0x05
    if (!uuid32s.IsEmpty()) {
      size_t len = uuid32s.Length();
      uint8_t adType;
      if (len > 2) {
        len = 2;
        adType = 0x04;  // Incomplete List of 32-bit UUIDs
      } else {
        adType = 0x05;  // Complete List of 32-bit UUIDs
      }
      aArray.AppendElement(1 + len * 4);
      aArray.AppendElement(adType);
      for (size_t i = 0; i < len; i++) {
        uint32_t uuid32 = uuid32s[i].GetUuid32();
        aArray.AppendElement(uuid32 & 0xFF);
        aArray.AppendElement((uuid32 >> 8) & 0xFF);
        aArray.AppendElement((uuid32 >> 16) & 0xFF);
        aArray.AppendElement((uuid32 >> 24) & 0xFF);
      }
    }

    // Incomplete List of 128-bit Service Class UUIDs, 0x06
    // Complete List of 128-bit Service Class UUIDs,   0x07
    if (!uuid128s.IsEmpty()) {
      size_t len = uuid128s.Length();
      uint8_t adType;
      if (len > 1) {
        len = 1;
        adType = 0x06;  // Incomplete List of 128-bit UUIDs
      } else {
        adType = 0x07;  // Complete List of 128-bit UUIDs
      }
      aArray.AppendElement(1 + len * 16);
      aArray.AppendElement(adType);
      for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < sizeof(uuid128s[i].mUuid); j++) {
          // little-endian
          aArray.AppendElement(
              uuid128s[i].mUuid[sizeof(uuid128s[i].mUuid) - j - 1]);
        }
      }
    }
  }

  // Shortened Local Name, 0x08
  // Complete Local Name,  0x09
  if (aData.mIncludeDevName && aData.mDeviceName.mLength > 0) {
    uint8_t len = aData.mDeviceName.mLength;
    uint8_t adType;
    if (len > 10) {
      len = 10;
      adType = 0x08;  // Shortened Local Name
    } else {
      adType = 0x09;  // Complete Local Name
    }

    aArray.AppendElement(1 + len);
    aArray.AppendElement(adType);
    for (size_t i = 0; i < len; i++) {
      aArray.AppendElement(aData.mDeviceName.mName[i]);
    }
  }

  // Service Data - 16-bit UUID,  0x16
  // Service Data - 32-bit UUID,  0x20
  // Service Data - 128-bit UUID, 0x21
  if (!aData.mServiceData.IsEmpty()) {
    aArray.AppendElements(&aData.mServiceData[0], aData.mServiceData.Length());
  }

  // Appearance, 0x19
  if (aData.mAppearance) {
    aArray.AppendElement(3);
    aArray.AppendElement(0x19);

    // little endian, 2 bytes
    aArray.AppendElement(aData.mAppearance & 0xFF);
    aArray.AppendElement((aData.mAppearance >> 8) & 0xFF);
  }

  // TX Power Level, 0x0A
  if (aData.mIncludeTxPower) {
    aArray.AppendElement(2);
    aArray.AppendElement(0x0A);
    aArray.AppendElement(kDefaultTxPower);
  }

  // Manufacturer Specific Data, 0xff
  if (!aData.mManufacturerData.IsEmpty()) {
    aArray.AppendElements(&aData.mManufacturerData[0],
                          aData.mManufacturerData.Length());
  }
}

void GeneratePathFromHandle(const BluetoothAttributeHandle& aHandle,
                            nsAString& aPath) {
  aPath.AppendLiteral("gatt_handle_");
  aPath.AppendInt(aHandle.mHandle);
}

void RegisterBluetoothSignalHandler(const nsAString& aPath,
                                    BluetoothSignalObserver* aHandler) {
  MOZ_ASSERT(!aPath.IsEmpty());
  MOZ_ASSERT(aHandler);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->RegisterBluetoothSignalHandler(aPath, aHandler);
  aHandler->SetSignalRegistered(true);
}

void RegisterBluetoothSignalHandler(const BluetoothAddress& aAddress,
                                    BluetoothSignalObserver* aHandler) {
  nsAutoString path;
  AddressToString(aAddress, path);

  RegisterBluetoothSignalHandler(path, aHandler);
}

void RegisterBluetoothSignalHandler(const BluetoothUuid& aUuid,
                                    BluetoothSignalObserver* aHandler) {
  nsAutoString path;
  UuidToString(aUuid, path);

  RegisterBluetoothSignalHandler(path, aHandler);
}

void UnregisterBluetoothSignalHandler(const nsAString& aPath,
                                      BluetoothSignalObserver* aHandler) {
  MOZ_ASSERT(!aPath.IsEmpty());
  MOZ_ASSERT(aHandler);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->UnregisterBluetoothSignalHandler(aPath, aHandler);
  aHandler->SetSignalRegistered(false);
}

void UnregisterBluetoothSignalHandler(const BluetoothAddress& aAddress,
                                      BluetoothSignalObserver* aHandler) {
  nsAutoString path;
  AddressToString(aAddress, path);

  UnregisterBluetoothSignalHandler(path, aHandler);
}

void UnregisterBluetoothSignalHandler(const BluetoothUuid& aUuid,
                                      BluetoothSignalObserver* aHandler) {
  nsAutoString path;
  UuidToString(aUuid, path);

  UnregisterBluetoothSignalHandler(path, aHandler);
}

/**
 * |SetJsObject| is an internal function used by |BroadcastSystemMessage| only
 */
static bool SetJsObject(JSContext* aContext, const BluetoothValue& aValue,
                        JS::Handle<JSObject*> aObj) {
  MOZ_ASSERT(aContext && aObj);

  if (aValue.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
    BT_WARNING("SetJsObject: Invalid parameter type");
    return false;
  }

  const nsTArray<BluetoothNamedValue>& arr =
      aValue.get_ArrayOfBluetoothNamedValue();

  for (uint32_t i = 0; i < arr.Length(); i++) {
    JS::Rooted<JS::Value> val(aContext);
    const BluetoothValue& v = arr[i].value();

    switch (v.type()) {
      case BluetoothValue::TBluetoothAddress: {
        nsAutoString addressStr;
        AddressToString(v.get_BluetoothAddress(), addressStr);

        JSString* jsData = JS_NewUCStringCopyN(
            aContext, addressStr.BeginReading(), addressStr.Length());
        NS_ENSURE_TRUE(jsData, false);
        val.setString(jsData);
        break;
      }
      case BluetoothValue::TnsString: {
        JSString* jsData =
            JS_NewUCStringCopyN(aContext, v.get_nsString().BeginReading(),
                                v.get_nsString().Length());
        NS_ENSURE_TRUE(jsData, false);
        val.setString(jsData);
        break;
      }
      case BluetoothValue::Tuint32_t:
        val.setNumber(v.get_uint32_t());
        break;
      case BluetoothValue::Tbool:
        val.setBoolean(v.get_bool());
        break;
      default:
        BT_WARNING("SetJsObject: Parameter is not handled");
        break;
    }

    if (!JS_SetProperty(aContext, aObj,
                        NS_ConvertUTF16toUTF8(arr[i].name()).get(), val)) {
      BT_WARNING("Failed to set property");
      return false;
    }
  }

  return true;
}

bool BroadcastSystemMessage(const nsAString& aType,
                            const BluetoothValue& aData) {
  nsCOMPtr<nsISystemMessageService> systemMessenger =
      do_GetService("@mozilla.org/systemmessage-service;1");
  NS_ENSURE_TRUE(systemMessenger, false);

  mozilla::AutoSafeJSContext cx;
  MOZ_ASSERT(!::JS_IsExceptionPending(cx),
             "Shouldn't get here when an exception is pending!");

  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    BT_WARNING("Failed to new JSObject for system message!");
    return false;
  }

  if (aData.type() == BluetoothValue::TnsString) {
    JSString* jsData = JS_NewUCStringCopyN(
        cx, aData.get_nsString().BeginReading(), aData.get_nsString().Length());
    JS::Rooted<JS::Value> msgVal(cx, JS::StringValue(jsData));
    if (!JS_SetProperty(cx, obj, "message", msgVal)) {
      BT_WARNING("Failed to set property");
      return false;
    }
  } else if (aData.type() == BluetoothValue::TArrayOfBluetoothNamedValue) {
    if (!SetJsObject(cx, aData, obj)) {
      BT_WARNING("Failed to set properties of system message!");
      return false;
    }
  } else {
    BT_WARNING("Not support the unknown BluetoothValue type");
    return false;
  }

  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*obj));
  systemMessenger->BroadcastMessage(aType, value, cx);

  return true;
}

bool BroadcastSystemMessage(const nsAString& aType,
                            const nsTArray<BluetoothNamedValue>& aData) {
  nsCOMPtr<nsISystemMessageService> systemMessenger =
      do_GetService("@mozilla.org/systemmessage-service;1");
  NS_ENSURE_TRUE(systemMessenger, false);

  mozilla::AutoSafeJSContext cx;
  MOZ_ASSERT(!::JS_IsExceptionPending(cx),
             "Shouldn't get here when an exception is pending!");

  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    BT_WARNING("Failed to new JSObject for system message!");
    return false;
  }

  if (!SetJsObject(cx, aData, obj)) {
    BT_WARNING("Failed to set properties of system message!");
    return false;
  }

  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*obj));
  systemMessenger->BroadcastMessage(aType, value, cx);

  return true;
}

void DispatchReplySuccess(BluetoothReplyRunnable* aRunnable) {
  DispatchReplySuccess(aRunnable, BluetoothValue(true));
}

void DispatchReplySuccess(BluetoothReplyRunnable* aRunnable,
                          const BluetoothValue& aValue) {
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aValue.type() != BluetoothValue::T__None);

  BluetoothReply* reply = new BluetoothReply(BluetoothReplySuccess(aValue));

  aRunnable->SetReply(reply);  // runnable will delete reply after Run()
  Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                        const nsAString& aErrorStr) {
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(!aErrorStr.IsEmpty());

  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply =
      new BluetoothReply(BluetoothReplyError(STATUS_FAIL, nsString(aErrorStr)));

  aRunnable->SetReply(reply);
  Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                        const enum BluetoothStatus aStatus) {
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aStatus != STATUS_SUCCESS);

  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply =
      new BluetoothReply(BluetoothReplyError(aStatus, EmptyString()));

  aRunnable->SetReply(reply);
  Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                        const enum BluetoothGattStatus aGattStatus) {
  MOZ_ASSERT(aRunnable);

  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply =
      new BluetoothReply(BluetoothReplyError(aGattStatus, EmptyString()));

  aRunnable->SetReply(reply);
  Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void DispatchStatusChangedEvent(const nsAString& aType,
                                const BluetoothAddress& aAddress,
                                bool aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "address", aAddress);
  AppendNamedValue(data, "status", aStatus);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->DistributeSignal(aType, KEY_ADAPTER, data);
}

void DispatchHfBatteryChangedEvent(const BluetoothAddress& aDeviceAddress,
                                   int32_t aBatteryLevel) {
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "BatteryLevel", aBatteryLevel);

  bs->DistributeSignal(u"PropertyChanged"_ns, aDeviceAddress, data);
}

void AppendNamedValue(nsTArray<BluetoothNamedValue>& aArray, const char* aName,
                      const BluetoothValue& aValue) {
  nsString name;
  name.AssignASCII(aName);

  aArray.AppendElement(BluetoothNamedValue(name, aValue));
}

void InsertNamedValue(nsTArray<BluetoothNamedValue>& aArray, uint8_t aIndex,
                      const char* aName, const BluetoothValue& aValue) {
  nsString name;
  name.AssignASCII(aName);

  aArray.InsertElementAt(aIndex, BluetoothNamedValue(name, aValue));
}

END_BLUETOOTH_NAMESPACE
