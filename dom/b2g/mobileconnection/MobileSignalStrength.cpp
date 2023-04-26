/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileSignalStrength.h"
#include "mozilla/dom/DOMMobileSignalStrength.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(MobileSignalStrength, nsIMobileSignalStrength)

MobileSignalStrength::MobileSignalStrength(
    const int16_t& aLevel, const int16_t& aGsmSignalStrength,
    const int16_t& aGsmSignalBitErrorRate, const int16_t& aCdmaDbm,
    const int16_t& aCdmaEcio, const int16_t& aCdmaEvdoDbm,
    const int16_t& aCdmaEvdoEcio, const int16_t& aCdmaEvdoSNR,
    const int16_t& aLteSignalStrength, const int32_t& aLteRsrp,
    const int32_t& aLteRsrq, const int32_t& aLteRssnr, const int32_t& aLteCqi,
    const int32_t& aLteTimingAdvance, const int32_t& aTdscdmaRscp)
    : mLevel(aLevel),
      mGsmSignalStrength(aGsmSignalStrength),
      mGsmSignalBitErrorRate(aGsmSignalBitErrorRate),
      mCdmaDbm(aCdmaDbm),
      mCdmaEcio(aCdmaEcio),
      mCdmaEvdoDbm(aCdmaEvdoDbm),
      mCdmaEvdoEcio(aCdmaEvdoEcio),
      mCdmaEvdoSNR(aCdmaEvdoSNR),
      mLteSignalStrength(aLteSignalStrength),
      mLteRsrp(aLteRsrp),
      mLteRsrq(aLteRsrq),
      mLteRssnr(aLteRssnr),
      mLteCqi(aLteCqi),
      mLteTimingAdvance(aLteTimingAdvance),
      mTdscdmaRscp(aTdscdmaRscp) {
  // The parent object is nullptr when MobileSignalStrength is created by this
  // way, and it won't be exposed to web content.
}

void MobileSignalStrength::Update(nsIMobileSignalStrength* aInfo) {
  if (!aInfo) {
    return;
  }

  aInfo->GetLevel(&mLevel);
  aInfo->GetGsmSignalStrength(&mGsmSignalStrength);
  aInfo->GetGsmBitErrorRate(&mGsmSignalBitErrorRate);
  aInfo->GetCdmaDbm(&mCdmaDbm);
  aInfo->GetCdmaEcio(&mCdmaEcio);
  aInfo->GetCdmaEvdoDbm(&mCdmaEvdoDbm);
  aInfo->GetCdmaEvdoEcio(&mCdmaEvdoEcio);
  aInfo->GetCdmaEvdoSNR(&mCdmaEvdoSNR);
  aInfo->GetLteSignalStrength(&mLteSignalStrength);
  aInfo->GetLteRsrp(&mLteRsrp);
  aInfo->GetLteRsrq(&mLteRsrq);
  aInfo->GetLteRssnr(&mLteRssnr);
  aInfo->GetLteCqi(&mLteCqi);
  aInfo->GetLteTimingAdvance(&mLteTimingAdvance);
  aInfo->GetTdscdmaRscp(&mTdscdmaRscp);
}

// nsIMobileSignalStrength
NS_IMETHODIMP
MobileSignalStrength::GetLevel(int16_t* aLevel) {
  *aLevel = mLevel;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetGsmSignalStrength(int16_t* aGsmSignalStrength) {
  *aGsmSignalStrength = mGsmSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetGsmBitErrorRate(int16_t* aGsmBitErrorRate) {
  *aGsmBitErrorRate = mGsmSignalBitErrorRate;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetCdmaDbm(int16_t* aCdmaDbm) {
  *aCdmaDbm = mCdmaDbm;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetCdmaEcio(int16_t* aCdmaEcio) {
  *aCdmaEcio = mCdmaEcio;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetCdmaEvdoDbm(int16_t* aCdmaEvdoDbm) {
  *aCdmaEvdoDbm = mCdmaEvdoDbm;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetCdmaEvdoEcio(int16_t* aCdmaEvdoEcio) {
  *aCdmaEvdoEcio = mCdmaEvdoEcio;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetCdmaEvdoSNR(int16_t* aCdmaEvdoSNR) {
  *aCdmaEvdoSNR = mCdmaEvdoSNR;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteSignalStrength(int16_t* aLteSignalStrength) {
  *aLteSignalStrength = mLteSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteRsrp(int32_t* aLteRsrp) {
  *aLteRsrp = mLteRsrp;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteRsrq(int32_t* aLteRsrq) {
  *aLteRsrq = mLteRsrq;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteRssnr(int32_t* aLteRssnr) {
  *aLteRssnr = mLteRssnr;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteCqi(int32_t* aLteCqi) {
  *aLteCqi = mLteCqi;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetLteTimingAdvance(int32_t* aLteTimingAdvance) {
  *aLteTimingAdvance = mLteTimingAdvance;
  return NS_OK;
}

NS_IMETHODIMP
MobileSignalStrength::GetTdscdmaRscp(int32_t* aTdscdmaRscp) {
  *aTdscdmaRscp = mTdscdmaRscp;
  return NS_OK;
}
