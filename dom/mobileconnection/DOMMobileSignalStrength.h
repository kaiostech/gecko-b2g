/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMMobileSignalStrength_h
#define mozilla_dom_DOMMobileSignalStrength_h

#include "nsPIDOMWindow.h"
#include "nsIMobileSignalStrength.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class DOMMobileSignalStrength final : public nsISupports,
                                      public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMMobileSignalStrength)

  explicit DOMMobileSignalStrength(nsPIDOMWindowInner* aWindow);

  void Update(nsIMobileSignalStrength* aInfo);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  int16_t Level() const { return mLevel; }

  int16_t GsmSignalStrength() const { return mGsmSignalStrength; }

  int16_t GsmBitErrorRate() const { return mGsmSignalBitErrorRate; }

  int16_t CdmaDbm() const { return mCdmaDbm; }

  int16_t CdmaEcio() const { return mCdmaEcio; }

  int16_t CdmaEvdoDbm() const { return mCdmaEvdoDbm; }

  int16_t CdmaEvdoEcio() const { return mCdmaEvdoEcio; }

  int16_t CdmaEvdoSNR() const { return mCdmaEvdoSNR; }

  int16_t LteSignalStrength() const { return mLteSignalStrength; }

  int32_t LteRsrp() const { return mLteRsrp; }

  int32_t LteRsrq() const { return mLteRsrq; }

  int32_t LteRssnr() const { return mLteRssnr; }

  int32_t LteCqi() const { return mLteCqi; }

  int32_t LteTimingAdvance() const { return mLteTimingAdvance; }

  int32_t TdscdmaRscp() const { return mTdscdmaRscp; }

 private:
  ~DOMMobileSignalStrength(){};

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

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

#endif  // mozilla_dom_DOMMobileSignalStrength_h
