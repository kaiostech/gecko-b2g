/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothUuidHelper.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothAvrcpManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothMapSmsManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothPbapManager.h"

USING_BLUETOOTH_NAMESPACE

BluetoothServiceClass BluetoothUuidHelper::GetBluetoothServiceClass(
    const nsAString& aUuidStr) {
  // An example of input UUID string: 0000110D-0000-1000-8000-00805F9B34FB
  MOZ_ASSERT(aUuidStr.Length() == 36);

  /**
   * Extract uuid16 from input UUID string and return a value of enum
   * BluetoothServiceClass. If we failed to recognize the value,
   * BluetoothServiceClass::UNKNOWN is returned.
   */
  BluetoothServiceClass retValue = BluetoothServiceClass::UNKNOWN;
  nsString uuid(Substring(aUuidStr, 4, 4));

  nsresult rv;
  int32_t integer = uuid.ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, retValue);

  return GetBluetoothServiceClass(integer);
}

BluetoothServiceClass BluetoothUuidHelper::GetBluetoothServiceClass(
    uint16_t aServiceUuid) {
  BluetoothServiceClass retValue = BluetoothServiceClass::UNKNOWN;
  switch (aServiceUuid) {
    case BluetoothServiceClass::A2DP:
    case BluetoothServiceClass::A2DP_SINK:
    case BluetoothServiceClass::AVRCP:
    case BluetoothServiceClass::AVRCP_TARGET:
    case BluetoothServiceClass::AVRCP_CONTROLLER:
    case BluetoothServiceClass::HANDSFREE:
    case BluetoothServiceClass::HANDSFREE_AG:
    case BluetoothServiceClass::HEADSET:
    case BluetoothServiceClass::HEADSET_AG:
    case BluetoothServiceClass::HID:
    case BluetoothServiceClass::OBJECT_PUSH:
    case BluetoothServiceClass::PBAP_PCE:
    case BluetoothServiceClass::PBAP_PSE:
    case BluetoothServiceClass::MAP_MAS:
    case BluetoothServiceClass::MAP_MNS:
      retValue = (BluetoothServiceClass)aServiceUuid;
  }
  return retValue;
}

BluetoothProfileManagerBase* BluetoothUuidHelper::GetBluetoothProfileManager(
    uint16_t aServiceUuid) {
  BluetoothProfileManagerBase* profile;
  BluetoothServiceClass serviceClass = GetBluetoothServiceClass(aServiceUuid);
  switch (serviceClass) {
    case BluetoothServiceClass::HANDSFREE:
    case BluetoothServiceClass::HEADSET:
      profile = BluetoothHfpManager::Get();
      break;
    case BluetoothServiceClass::HID:
      profile = BluetoothHidManager::Get();
      break;
    case BluetoothServiceClass::A2DP:
      profile = BluetoothA2dpManager::Get();
      break;
    case BluetoothServiceClass::AVRCP:
      profile = BluetoothAvrcpManager::Get();
      break;
    case BluetoothServiceClass::OBJECT_PUSH:
      profile = BluetoothOppManager::Get();
      break;
    case BluetoothServiceClass::PBAP_PSE:
      profile = BluetoothPbapManager::Get();
      break;
    case BluetoothServiceClass::MAP_MAS:
      profile = BluetoothMapSmsManager::Get();
      break;
    case BluetoothServiceClass::MAP_MNS:
      profile = BluetoothMapSmsManager::Get();
      break;
    default:
      profile = nullptr;
  }
  return profile;
}

BluetoothSdpType BluetoothUuidHelper::GetBluetoothSdpType(uint16_t aServiceUuid) {
  BluetoothSdpType type = SDP_TYPE_RAW;

  switch (aServiceUuid) {
    case BluetoothServiceClass::PBAP_PSE:
      type = SDP_TYPE_PBAP_PSE;
      break;
    case BluetoothServiceClass::PBAP_PCE:
      type = SDP_TYPE_PBAP_PCE;
      break;
    case BluetoothServiceClass::MAP_MAS:
      type = SDP_TYPE_MAP_MAS;
      break;
    case BluetoothServiceClass::MAP_MNS:
      type = SDP_TYPE_MAP_MNS;
      break;
    default:
      break;
  }

  return type;
}
