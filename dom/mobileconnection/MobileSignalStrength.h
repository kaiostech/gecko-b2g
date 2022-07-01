/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileSignalStrength_h
#define mozilla_dom_MobileSignalStrength_h

#include "nsIMobileSignalStrength.h"
namespace mozilla {
namespace dom {
class MobileSignalStrength final : public nsIMobileSignalStrength {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILESIGNALSTRENGTH

  explicit MobileSignalStrength(){};

  explicit MobileSignalStrength(
      const int16_t& aLevel, const int16_t& aGsmSignalStrength,
      const int16_t& aGsmSignalBitErrorRate, const int16_t& aCdmaDbm,
      const int16_t& aCdmaEcio, const int16_t& aCdmaEvdoDbm,
      const int16_t& aCdmaEvdoEcio, const int16_t& aCdmaEvdoSNR,
      const int16_t& aLteSignalStrength, const int32_t& aLteRsrp,
      const int32_t& aLteRsrq, const int32_t& aLteRssnr, const int32_t& aLteCqi,
      const int32_t& aLteTimingAdvance, const int32_t& aTdscdmaRscp);

  void Update(nsIMobileSignalStrength* aInfo);

 private:
  ~MobileSignalStrength(){};

  int16_t mLevel;

  // GSM/UMTS category.
  int16_t mGsmSignalStrength;
  int16_t mGsmSignalBitErrorRate;

  // CDMA category.
  int16_t mCdmaDbm;
  int16_t mCdmaEcio;
  int16_t mCdmaEvdoDbm;
  int16_t mCdmaEvdoEcio;
  int16_t mCdmaEvdoSNR;

  // LTE category.
  int16_t mLteSignalStrength;
  int32_t mLteRsrp;
  int32_t mLteRsrq;
  int32_t mLteRssnr;
  int32_t mLteCqi;
  int32_t mLteTimingAdvance;

  // TDS-CDMA category.
  int32_t mTdscdmaRscp;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileSignalStrength_h
