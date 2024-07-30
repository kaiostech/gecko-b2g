/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileCellInfo.h"
#include "mozilla/dom/MobileCellInfoBinding.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileCellInfo, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileCellInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileCellInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileCellInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMobileCellInfo)
NS_INTERFACE_MAP_END

MobileCellInfo::MobileCellInfo(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow),
      mGsmLocationAreaCode(-1),
      mGsmCellId(-1),
      mCdmaBaseStationId(-1),
      mCdmaBaseStationLatitude(-1),
      mCdmaBaseStationLongitude(-1),
      mCdmaSystemId(-1),
      mCdmaNetworkId(-1),
      mCdmaRoamingIndicator(-1),
      mCdmaDefaultRoamingIndicator(-1),
      mCdmaSystemIsInPRL(false),
      mTac(-1),
      mCi(-1),
      mPci(-1),
      mArfcns(-1),
      mBands(0) {
      }

MobileCellInfo::MobileCellInfo(int32_t aGsmLocationAreaCode, int64_t aGsmCellId,
                               int32_t aCdmaBaseStationId,
                               int32_t aCdmaBaseStationLatitude,
                               int32_t aCdmaBaseStationLongitude,
                               int32_t aCdmaSystemId, int32_t aCdmaNetworkId,
                               int16_t aCdmaRoamingIndicator,
                               int16_t aCdmaDefaultRoamingIndicator,
                               bool aCdmaSystemIsInPRL,
                               int32_t aTac,
                               int64_t aCi,
                               int32_t aPci,
                               int32_t aArfcns,
                               int64_t aBands)
    : mGsmLocationAreaCode(aGsmLocationAreaCode),
      mGsmCellId(aGsmCellId),
      mCdmaBaseStationId(aCdmaBaseStationId),
      mCdmaBaseStationLatitude(aCdmaBaseStationLatitude),
      mCdmaBaseStationLongitude(aCdmaBaseStationLongitude),
      mCdmaSystemId(aCdmaSystemId),
      mCdmaNetworkId(aCdmaNetworkId),
      mCdmaRoamingIndicator(aCdmaRoamingIndicator),
      mCdmaDefaultRoamingIndicator(aCdmaDefaultRoamingIndicator),
      mCdmaSystemIsInPRL(aCdmaSystemIsInPRL),
      mTac(aTac),
      mCi(aCi),
      mPci(aPci),
      mArfcns(aArfcns),
      mBands(aBands) {
  // The instance created by this way is only used for IPC stuff. It won't be
  // exposed to JS directly, we will clone this instance to the one that is
  // maintained in MobileConnectionChild.
}

void MobileCellInfo::Update(nsIMobileCellInfo* aInfo) {
  if (!aInfo) {
    return;
  }

  aInfo->GetGsmLocationAreaCode(&mGsmLocationAreaCode);
  aInfo->GetGsmCellId(&mGsmCellId);
  aInfo->GetCdmaBaseStationId(&mCdmaBaseStationId);
  aInfo->GetCdmaBaseStationLatitude(&mCdmaBaseStationLatitude);
  aInfo->GetCdmaBaseStationLongitude(&mCdmaBaseStationLongitude);
  aInfo->GetCdmaSystemId(&mCdmaSystemId);
  aInfo->GetCdmaNetworkId(&mCdmaNetworkId);
  aInfo->GetCdmaRoamingIndicator(&mCdmaRoamingIndicator);
  aInfo->GetCdmaDefaultRoamingIndicator(&mCdmaDefaultRoamingIndicator);
  aInfo->GetCdmaSystemIsInPRL(&mCdmaSystemIsInPRL);
  aInfo->GetTac(&mTac);
  aInfo->GetCi(&mCi);
  aInfo->GetPci(&mPci);
  aInfo->GetArfcns(&mArfcns);
  aInfo->GetBands(&mBands);
}

JSObject* MobileCellInfo::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return MobileCellInfo_Binding::Wrap(aCx, this, aGivenProto);
}

// nsIMobileCellInfo

NS_IMETHODIMP
MobileCellInfo::GetGsmLocationAreaCode(int32_t* aGsmLocationAreaCode) {
  *aGsmLocationAreaCode = GsmLocationAreaCode();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetGsmCellId(int64_t* aGsmCellId) {
  *aGsmCellId = GsmCellId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationId(int32_t* aCdmaBaseStationId) {
  *aCdmaBaseStationId = CdmaBaseStationId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationLatitude(int32_t* aCdmaBaseStationLatitude) {
  *aCdmaBaseStationLatitude = CdmaBaseStationLatitude();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationLongitude(
    int32_t* aCdmaBaseStationLongitude) {
  *aCdmaBaseStationLongitude = CdmaBaseStationLongitude();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaSystemId(int32_t* aCdmaSystemId) {
  *aCdmaSystemId = CdmaSystemId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaNetworkId(int32_t* aCdmaNetworkId) {
  *aCdmaNetworkId = CdmaNetworkId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaRoamingIndicator(int16_t* aCdmaRoamingIndicator) {
  *aCdmaRoamingIndicator = CdmaRoamingIndicator();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaDefaultRoamingIndicator(
    int16_t* aCdmaDefaultRoamingIndicator) {
  *aCdmaDefaultRoamingIndicator = CdmaDefaultRoamingIndicator();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaSystemIsInPRL(bool* aCdmaSystemIsInPRL) {
  *aCdmaSystemIsInPRL = CdmaSystemIsInPRL();
  return NS_OK;
}
NS_IMETHODIMP
MobileCellInfo::GetTac(int32_t* aTac) {
  *aTac = Tac();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCi(int64_t* aCi) {
  *aCi = Ci();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetPci(int32_t* aPci) {
  *aPci = Pci();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetArfcns(int32_t* aArfcns) {
  *aArfcns = Arfcns();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetBands(int64_t* aBands) {
  *aBands = Bands();
  return NS_OK;
}
