/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h
#define mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/mobileconnection/MobileCallForwardingOptions.h"
#include "mozilla/dom/MobileCellInfo.h"
#include "mozilla/dom/MobileDeviceIdentities.h"
#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MobileSignalStrength.h"
#include "mozilla/dom/MobileConnectionBinding.h"
#include "mozilla/dom/ScriptSettings.h"

using mozilla::AutoJSContext;
using mozilla::dom::MobileCellInfo;
using mozilla::dom::MobileConnectionInfo;
using mozilla::dom::MobileDeviceIdentities;
using mozilla::dom::MobileNetworkInfo;
using mozilla::dom::MobileSignalStrength;
using mozilla::dom::mobileconnection::MobileCallForwardingOptions;

typedef nsIMobileCellInfo* nsMobileCellInfo;
typedef nsIMobileConnectionInfo* nsMobileConnectionInfo;
typedef nsIMobileNetworkInfo* nsMobileNetworkInfo;
typedef nsIMobileCallForwardingOptions* nsMobileCallForwardingOptions;
typedef nsIMobileDeviceIdentities* nsMobileDeviceIdentities;
typedef nsIMobileSignalStrength* nsMobileSignalStrength;

namespace IPC {
template <>
struct ParamTraits<nsIMobileCallForwardingOptions*> {
  typedef nsIMobileCallForwardingOptions* paramType;

  // Function to serialize a MobileCallForwardingOptions.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int16_t pShort;
    nsString pString;
    bool pBool;

    aParam->GetActive(&pBool);
    WriteParam(aWriter, pBool);

    aParam->GetAction(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetReason(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetNumber(pString);
    WriteParam(aWriter, pString);

    aParam->GetTimeSeconds(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetServiceClass(&pShort);
    WriteParam(aWriter, pShort);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileCallForwardingOptions.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    bool active;
    int16_t action;
    int16_t reason;
    nsString number;
    int16_t timeSeconds;
    int16_t serviceClass;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &active) && ReadParam(aReader, &action) &&
          ReadParam(aReader, &reason) && ReadParam(aReader, &number) &&
          ReadParam(aReader, &timeSeconds) &&
          ReadParam(aReader, &serviceClass))) {
      return false;
    }

    *aResult = new MobileCallForwardingOptions(active, action, reason, number,
                                               timeSeconds, serviceClass);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileNetworkInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileNetworkInfo*> {
  typedef nsIMobileNetworkInfo* paramType;

  // Function to serialize a MobileNetworkInfo.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    nsString pString;
    aParam->GetShortName(pString);
    WriteParam(aWriter, pString);

    aParam->GetLongName(pString);
    WriteParam(aWriter, pString);

    aParam->GetMcc(pString);
    WriteParam(aWriter, pString);

    aParam->GetMnc(pString);
    WriteParam(aWriter, pString);

    aParam->GetState(pString);
    WriteParam(aWriter, pString);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileNetworkInfo.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    nsString shortName;
    nsString longName;
    nsString mcc;
    nsString mnc;
    nsString state;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &shortName) && ReadParam(aReader, &longName) &&
          ReadParam(aReader, &mcc) && ReadParam(aReader, &mnc) &&
          ReadParam(aReader, &state))) {
      return false;
    }

    *aResult = new MobileNetworkInfo(shortName, longName, mcc, mnc, state);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileCellInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileCellInfo*> {
  typedef nsIMobileCellInfo* paramType;

  // Function to serialize a MobileCellInfo.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int32_t pLong;
    int64_t pLongLong;
    int16_t pShort;
    bool pBool;

    aParam->GetGsmLocationAreaCode(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetGsmCellId(&pLongLong);
    WriteParam(aWriter, pLongLong);

    aParam->GetCdmaBaseStationId(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetCdmaBaseStationLatitude(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetCdmaBaseStationLongitude(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetCdmaSystemId(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetCdmaNetworkId(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetCdmaRoamingIndicator(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetCdmaDefaultRoamingIndicator(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetCdmaSystemIsInPRL(&pBool);
    WriteParam(aWriter, pBool);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileCellInfo.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    int32_t gsmLac;
    int64_t gsmCellId;
    int32_t cdmaBsId;
    int32_t cdmaBsLat;
    int32_t cdmaBsLong;
    int32_t cdmaSystemId;
    int32_t cdmaNetworkId;
    int16_t cdmaRoamingIndicator;
    int16_t cdmaDefaultRoamingIndicator;
    bool cdmaSystemIsInPRL;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &gsmLac) && ReadParam(aReader, &gsmCellId) &&
          ReadParam(aReader, &cdmaBsId) && ReadParam(aReader, &cdmaBsLat) &&
          ReadParam(aReader, &cdmaBsLong) &&
          ReadParam(aReader, &cdmaSystemId) &&
          ReadParam(aReader, &cdmaNetworkId) &&
          ReadParam(aReader, &cdmaRoamingIndicator) &&
          ReadParam(aReader, &cdmaDefaultRoamingIndicator) &&
          ReadParam(aReader, &cdmaSystemIsInPRL))) {
      return false;
    }

    *aResult =
        new MobileCellInfo(gsmLac, gsmCellId, cdmaBsId, cdmaBsLat, cdmaBsLong,
                           cdmaSystemId, cdmaNetworkId, cdmaRoamingIndicator,
                           cdmaDefaultRoamingIndicator, cdmaSystemIsInPRL);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileConnectionInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileConnectionInfo*> {
  typedef nsIMobileConnectionInfo* paramType;

  // Function to serialize a MobileConnectionInfo.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    AutoJSContext cx;
    nsString pString;
    bool pBool;
    nsCOMPtr<nsIMobileNetworkInfo> pNetworkInfo;
    nsCOMPtr<nsIMobileCellInfo> pCellInfo;
    int32_t pLong;
    JS::Rooted<JS::Value> pJsval(cx);

    aParam->GetState(pString);
    WriteParam(aWriter, pString);

    aParam->GetConnected(&pBool);
    WriteParam(aWriter, pBool);

    aParam->GetEmergencyCallsOnly(&pBool);
    WriteParam(aWriter, pBool);

    aParam->GetRoaming(&pBool);
    WriteParam(aWriter, pBool);

    aParam->GetType(pString);
    WriteParam(aWriter, pString);

    aParam->GetNetwork(getter_AddRefs(pNetworkInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aWriter, pNetworkInfo.forget().take());

    aParam->GetCell(getter_AddRefs(pCellInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aWriter, pCellInfo.forget().take());

    aParam->GetReasonDataDenied(&pLong);
    WriteParam(aWriter, pLong);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileConectionInfo.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    AutoJSContext cx;
    nsString state;
    bool connected;
    bool emergencyOnly;
    bool roaming;
    nsString type;
    nsIMobileNetworkInfo* networkInfo = nullptr;
    nsIMobileCellInfo* cellInfo = nullptr;
    int32_t reasonDataDenied;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &state) && ReadParam(aReader, &connected) &&
          ReadParam(aReader, &emergencyOnly) && ReadParam(aReader, &roaming) &&
          ReadParam(aReader, &type) && ReadParam(aReader, &networkInfo) &&
          ReadParam(aReader, &cellInfo) &&
          ReadParam(aReader, &reasonDataDenied))) {
      return false;
    }

    *aResult =
        new MobileConnectionInfo(state, connected, emergencyOnly, roaming,
                                 networkInfo, type, cellInfo, reasonDataDenied);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);
    // We already clone the data into MobileConnectionInfo, so release the ref
    // of networkInfo and cellInfo here.
    NS_IF_RELEASE(networkInfo);
    NS_IF_RELEASE(cellInfo);

    return true;
  }
};

/**
 * nsIMobileDeviceIdentities Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileDeviceIdentities*> {
  typedef nsIMobileDeviceIdentities* paramType;

  // Function to serialize a MobileDeviceIdentities.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);

    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    AutoJSContext cx;
    nsString pString;
    JS::Rooted<JS::Value> pJsval(cx);

    aParam->GetImei(pString);
    WriteParam(aWriter, pString);

    aParam->GetImeisv(pString);
    WriteParam(aWriter, pString);

    aParam->GetEsn(pString);
    WriteParam(aWriter, pString);

    aParam->GetMeid(pString);
    WriteParam(aWriter, pString);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileDeviceIdentities.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    AutoJSContext cx;
    nsString imei;
    nsString imeisv;
    nsString esn;
    nsString meid;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &imei) && ReadParam(aReader, &imeisv) &&
          ReadParam(aReader, &esn) && ReadParam(aReader, &meid))) {
      return false;
    }

    *aResult = new MobileDeviceIdentities(imei, imeisv, esn, meid);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileSignalStrength Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileSignalStrength*> {
  typedef nsIMobileSignalStrength* paramType;

  // Function to serialize a nsIMobileSignalStrength.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int16_t pShort;
    int32_t pLong;

    aParam->GetLevel(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetGsmSignalStrength(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetGsmBitErrorRate(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetCdmaDbm(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetCdmaEcio(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetCdmaEvdoDbm(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetCdmaEvdoEcio(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetCdmaEvdoSNR(&pShort);
    WriteParam(aWriter, pShort);

    aParam->GetLteSignalStrength(&pShort);
    WriteParam(aWriter, pShort);
    aParam->GetLteRsrp(&pLong);
    WriteParam(aWriter, pLong);
    aParam->GetLteRsrq(&pLong);
    WriteParam(aWriter, pLong);
    aParam->GetLteRssnr(&pLong);
    WriteParam(aWriter, pLong);
    aParam->GetLteCqi(&pLong);
    WriteParam(aWriter, pLong);
    aParam->GetLteTimingAdvance(&pLong);
    WriteParam(aWriter, pLong);

    aParam->GetTdscdmaRscp(&pLong);
    WriteParam(aWriter, pLong);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileDeviceIdentities.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    int16_t level;
    int16_t gsmSignalStrength;
    int16_t gsmBitErrorRate;
    int16_t cdmaDbm;
    int16_t cdmaEcio;
    int16_t cdmaEvdoDbm;
    int16_t cdmaEvdoEcio;
    int16_t cdmaEvdoSNR;
    int16_t lteSignalStrength;
    int32_t lteRsrp;
    int32_t lteRsrq;
    int32_t lteRssnr;
    int32_t lteCqi;
    int32_t lteTimingAdvance;
    int32_t tdscdmaRscp;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &level) &&
          ReadParam(aReader, &gsmSignalStrength) &&
          ReadParam(aReader, &gsmBitErrorRate) &&
          ReadParam(aReader, &cdmaDbm) && ReadParam(aReader, &cdmaEcio) &&
          ReadParam(aReader, &cdmaEvdoDbm) &&
          ReadParam(aReader, &cdmaEvdoEcio) &&
          ReadParam(aReader, &cdmaEvdoSNR) &&
          ReadParam(aReader, &lteSignalStrength) &&
          ReadParam(aReader, &lteRsrp) && ReadParam(aReader, &lteRsrq) &&
          ReadParam(aReader, &lteRssnr) && ReadParam(aReader, &lteCqi) &&
          ReadParam(aReader, &lteTimingAdvance) &&
          ReadParam(aReader, &tdscdmaRscp))) {
      return false;
    }

    *aResult = new MobileSignalStrength(
        level, gsmSignalStrength, gsmBitErrorRate, cdmaDbm, cdmaEcio,
        cdmaEvdoDbm, cdmaEvdoEcio, cdmaEvdoSNR, lteSignalStrength, lteRsrp,
        lteRsrq, lteRssnr, lteCqi, lteTimingAdvance, tdscdmaRscp);

    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_mobileconnection_MobileConnectionIPCSerialiser_h
