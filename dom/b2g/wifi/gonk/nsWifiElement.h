/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWifiElement_H
#define nsWifiElement_H

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIWifiElement.h"

/**
 * nsIWifiConfiguration
 */
class nsWifiConfiguration final : public nsIWifiConfiguration {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWIFICONFIGURATION
  nsWifiConfiguration();
  nsWifiConfiguration(int32_t aNetId, const nsAString& aSsid,
                      const nsAString& aBssid, const nsAString& aKeyMgmt,
                      const nsAString& aPsk, const nsAString& aWepKey0,
                      const nsAString& aWepKey1, const nsAString& aWepKey2,
                      const nsAString& aWepKey3, int32_t aWepTxKeyIndex,
                      bool aScanSsid, bool aPmf, const nsAString& aProto,
                      const nsAString& aAuthAlg, const nsAString& aGroupCipher,
                      const nsAString& aPairwiseCipher, const nsAString& aEap,
                      const nsAString& aPhase2, const nsAString& aIdentity,
                      const nsAString& aAnonymousId, const nsAString& aPassword,
                      const nsAString& aClientCert, const nsAString& aCaCert,
                      const nsAString& aCaPath, const nsAString& aSubjectMatch,
                      const nsAString& aEngineId, bool aEngine,
                      const nsAString& aPrivateKeyId,
                      const nsAString& aAltSubjectMatch,
                      const nsAString& aDomainSuffixMatch,
                      bool aProactiveKeyCaching, int32_t aSimIndex);
  int32_t mNetId;
  nsString mSsid;
  nsString mBssid;
  nsString mKeyMgmt;
  nsString mPsk;
  nsString mWepKey0;
  nsString mWepKey1;
  nsString mWepKey2;
  nsString mWepKey3;
  int32_t mWepTxKeyIndex;
  bool mScanSsid;
  bool mPmf;
  nsString mProto;
  nsString mAuthAlg;
  nsString mGroupCipher;
  nsString mPairwiseCipher;
  nsString mEap;
  nsString mPhase2;
  nsString mIdentity;
  nsString mAnonymousId;
  nsString mPassword;
  nsString mClientCert;
  nsString mCaCert;
  nsString mCaPath;
  nsString mSubjectMatch;
  nsString mEngineId;
  bool mEngine;
  nsString mPrivateKeyId;
  nsString mAltSubjectMatch;
  nsString mDomainSuffixMatch;
  bool mProactiveKeyCaching;
  int32_t mSimIndex;

 private:
  ~nsWifiConfiguration(){};
};

class nsScanResult final : public nsIScanResult {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISCANRESULT
  nsScanResult(const nsAString& aSsid, const nsAString& aBssid,
               const nsTArray<uint8_t>& aInfoElement, uint32_t aFrequency,
               uint32_t aTsf, uint32_t aCapability, int32_t aSignal,
               bool aAssociated);

 private:
  ~nsScanResult() {}

  nsString mSsid;
  nsString mBssid;
  uint32_t mFrequency;
  uint32_t mTsf;
  uint32_t mCapability;
  int32_t mSignal;
  bool mAssociated;
  nsTArray<uint8_t> mInfoElement;
};

class nsStateChanged final : public nsIStateChanged {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTATECHANGED
  nsStateChanged(uint32_t aState, int32_t aId, const nsAString& aBssid,
                 const nsAString& aSsid);

 private:
  ~nsStateChanged() {}

  uint32_t mState;
  int32_t mId;
  nsString mBssid;
  nsString mSsid;
};

class nsLinkLayerPacketStats final : public nsILinkLayerPacketStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILINKLAYERPACKETSTATS
  nsLinkLayerPacketStats(uint64_t aRxMpdu, uint64_t aTxMpdu, uint64_t aLostMpdu,
                         uint64_t aRetries);

 private:
  ~nsLinkLayerPacketStats() {}

  uint64_t mRxMpdu;
  uint64_t mTxMpdu;
  uint64_t mLostMpdu;
  uint64_t mRetries;
};

class nsLinkLayerRadioStats final : public nsILinkLayerRadioStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILINKLAYERRADIOSTATS
  nsLinkLayerRadioStats(uint32_t aOnTimeInMs, uint32_t aTxTimeInMs,
                        uint32_t aRxTimeInMs, uint32_t aOnTimeInMsForScan,
                        const nsTArray<uint32_t>& aTxTimeInMsPerLevel);

 private:
  ~nsLinkLayerRadioStats() {}

  uint32_t mOnTimeInMs;
  uint32_t mTxTimeInMs;
  uint32_t mRxTimeInMs;
  uint32_t mOnTimeInMsForScan;
  nsTArray<uint32_t> mTxTimeInMsPerLevel;
};

class nsLinkLayerStats final : public nsILinkLayerStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILINKLAYERSTATS
  nsLinkLayerStats(uint32_t aBeaconRx, int32_t aAvgRssiMgmt,
                   uint64_t aTimeStampMs);

  void updatePacketStats(nsLinkLayerPacketStats* aWmeBePktStats,
                         nsLinkLayerPacketStats* aWmeBkPktStats,
                         nsLinkLayerPacketStats* aWmeViPktStats,
                         nsLinkLayerPacketStats* aWmeVoPktStats);

  void updateRadioStats(
      nsTArray<RefPtr<nsLinkLayerRadioStats>>& aLinkLayerRadioStats);

  RefPtr<nsILinkLayerPacketStats> mWmeBePktStats;
  RefPtr<nsILinkLayerPacketStats> mWmeBkPktStats;
  RefPtr<nsILinkLayerPacketStats> mWmeViPktStats;
  RefPtr<nsILinkLayerPacketStats> mWmeVoPktStats;
  nsTArray<RefPtr<nsLinkLayerRadioStats>> mLinkLayerRadioStats;

 private:
  ~nsLinkLayerStats() {}

  uint32_t mBeaconRx;
  int32_t mAvgRssiMgmt;
  uint64_t mTimeStampMs;
};

#endif  // nsWifiElement_H
