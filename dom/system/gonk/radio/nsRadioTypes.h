/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRadioTypes_H
#define nsRadioTypes_H

#include "nsCOMPtr.h"
#include "nsIRadioTypes.h"
#include "nsString.h"

#if ANDROID_VERSION >= 33
#include <aidl/android/hardware/radio/sim/BnRadioSimIndication.h>
#include <aidl/android/hardware/radio/sim/BnRadioSimResponse.h>
#include <aidl/android/hardware/radio/sim/IRadioSim.h>
#include <android/hardware/radio/1.6/IRadio.h>
#else
#include <android/hardware/radio/1.1/IRadio.h>
#endif

#include <nsISupportsImpl.h>
#include <nsTArray.h>

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using CdmaSmsDigitMode_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsDigitMode;
using CdmaSmsNumberMode_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsNumberMode;
using CdmaSmsNumberType_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsNumberType;
using CdmaSmsNumberPlan_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsNumberPlan;
using CdmaSmsSubaddressType_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsSubaddressType;
using CdmaSmsAddress_V1_1 = ::android::hardware::radio::V1_0::CdmaSmsAddress;
using CdmaSmsSubaddress_V1_1 =
  ::android::hardware::radio::V1_0::CdmaSmsSubaddress;
using CdmaSmsMessage_V1_1 = ::android::hardware::radio::V1_0::CdmaSmsMessage;

#if ANDROID_VERSION >= 33
//using BarringInfo_V1_5 = ::android::hardware::radio::V1_5::BarringInfo;
using PhonebookRecordInfo_V1_6 =
  ::android::hardware::radio::V1_6::PhonebookRecordInfo;
using RadioAccessSpecifier_V1_5 =
  ::android::hardware::radio::V1_5::RadioAccessSpecifier;
using namespace aidl::android::hardware::radio::sim;
using PhonebookRecordInfoA =
  aidl::android::hardware::radio::sim::PhonebookRecordInfo;
#endif
/*
 * When adding a network type to the list below, make sure to add the correct
 * icon to MobileSignalController.mapIconSets() as well as NETWORK_TYPES Do not
 * add negative types.
 */
/** Network type is unknown */
const int NETWORK_TYPE_UNKNOWN = 0;
/** Current network is GPRS */
const int NETWORK_TYPE_GPRS = 1;
/** Current network is EDGE */
const int NETWORK_TYPE_EDGE = 2;
/** Current network is UMTS */
const int NETWORK_TYPE_UMTS = 3;
/** Current network is CDMA: Either IS95A or IS95B*/
const int NETWORK_TYPE_CDMA = 4;
/** Current network is EVDO revision 0*/
const int NETWORK_TYPE_EVDO_0 = 5;
/** Current network is EVDO revision A*/
const int NETWORK_TYPE_EVDO_A = 6;
/** Current network is 1xRTT*/
const int NETWORK_TYPE_1xRTT = 7;
/** Current network is HSDPA */
const int NETWORK_TYPE_HSDPA = 8;
/** Current network is HSUPA */
const int NETWORK_TYPE_HSUPA = 9;
/** Current network is HSPA */
const int NETWORK_TYPE_HSPA = 10;
/** Current network is iDen */
const int NETWORK_TYPE_IDEN = 11;
/** Current network is EVDO revision B*/
const int NETWORK_TYPE_EVDO_B = 12;
/** Current network is LTE */
const int NETWORK_TYPE_LTE = 13;
/** Current network is eHRPD */
const int NETWORK_TYPE_EHRPD = 14;
/** Current network is HSPA+ */
const int NETWORK_TYPE_HSPAP = 15;
/** Current network is GSM */
const int NETWORK_TYPE_GSM = 16;
/** Current network is TD_SCDMA */
const int NETWORK_TYPE_TD_SCDMA = 17;
/** Current network is IWLAN */
const int NETWORK_TYPE_IWLAN = 18;
/** Current network is LTE_CA {@hide} */
// @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug =
// 170729553)
const int NETWORK_TYPE_LTE_CA = 19;
const int NETWORK_TYPE_NR = 20;

/**
 * network type bitmask unknown.
 */
const long NETWORK_TYPE_BITMASK_UNKNOWN = 0L;
/**
 * network type bitmask indicating the support of radio tech GSM.
 */
const long NETWORK_TYPE_BITMASK_GSM = (1 << (NETWORK_TYPE_GSM - 1));
/**
 * network type bitmask indicating the support of radio tech GPRS.
 */
const long NETWORK_TYPE_BITMASK_GPRS = (1 << (NETWORK_TYPE_GPRS - 1));
/**
 * network type bitmask indicating the support of radio tech EDGE.
 */
const long NETWORK_TYPE_BITMASK_EDGE = (1 << (NETWORK_TYPE_EDGE - 1));
/**
 * network type bitmask indicating the support of radio tech CDMA(IS95A/IS95B).
 */
const long NETWORK_TYPE_BITMASK_CDMA = (1 << (NETWORK_TYPE_CDMA - 1));
/**
 * network type bitmask indicating the support of radio tech 1xRTT.
 */
//@SuppressLint("AllUpper")
const long NETWORK_TYPE_BITMASK_1xRTT = (1 << (NETWORK_TYPE_1xRTT - 1));
// 3G
/**
 * network type bitmask indicating the support of radio tech EVDO 0.
 */
const long NETWORK_TYPE_BITMASK_EVDO_0 = (1 << (NETWORK_TYPE_EVDO_0 - 1));
/**
 * network type bitmask indicating the support of radio tech EVDO A.
 */
const long NETWORK_TYPE_BITMASK_EVDO_A = (1 << (NETWORK_TYPE_EVDO_A - 1));
/**
 * network type bitmask indicating the support of radio tech EVDO B.
 */
const long NETWORK_TYPE_BITMASK_EVDO_B = (1 << (NETWORK_TYPE_EVDO_B - 1));
/**
 * network type bitmask indicating the support of radio tech EHRPD.
 */
const long NETWORK_TYPE_BITMASK_EHRPD = (1 << (NETWORK_TYPE_EHRPD - 1));
/**
 * network type bitmask indicating the support of radio tech HSUPA.
 */
const long NETWORK_TYPE_BITMASK_HSUPA = (1 << (NETWORK_TYPE_HSUPA - 1));
/**
 * network type bitmask indicating the support of radio tech HSDPA.
 */
const long NETWORK_TYPE_BITMASK_HSDPA = (1 << (NETWORK_TYPE_HSDPA - 1));
/**
 * network type bitmask indicating the support of radio tech HSPA.
 */
const long NETWORK_TYPE_BITMASK_HSPA = (1 << (NETWORK_TYPE_HSPA - 1));
/**
 * network type bitmask indicating the support of radio tech HSPAP.
 */
const long NETWORK_TYPE_BITMASK_HSPAP = (1 << (NETWORK_TYPE_HSPAP - 1));
/**
 * network type bitmask indicating the support of radio tech UMTS.
 */
const long NETWORK_TYPE_BITMASK_UMTS = (1 << (NETWORK_TYPE_UMTS - 1));
/**
 * network type bitmask indicating the support of radio tech TD_SCDMA.
 */
const long NETWORK_TYPE_BITMASK_TD_SCDMA = (1 << (NETWORK_TYPE_TD_SCDMA - 1));
// 4G
/**
 * network type bitmask indicating the support of radio tech LTE.
 */
const long NETWORK_TYPE_BITMASK_LTE = (1 << (NETWORK_TYPE_LTE - 1));
/**
 * NOT USED; this bitmask is exposed accidentally, will be deprecated in U.
 * If used, will be converted to {@link #NETWORK_TYPE_BITMASK_LTE}.
 * network type bitmask indicating the support of radio tech LTE CA (carrier
 * aggregation).
 *
 * @see #NETWORK_TYPE_BITMASK_LTE
 */
const long NETWORK_TYPE_BITMASK_LTE_CA = (1 << (NETWORK_TYPE_LTE_CA - 1));

/**
 * network type bitmask indicating the support of radio tech NR(New Radio) 5G.
 */
const long NETWORK_TYPE_BITMASK_NR = (1 << (NETWORK_TYPE_NR - 1));

/**
 * network type bitmask indicating the support of radio tech IWLAN.
 */
const long NETWORK_TYPE_BITMASK_IWLAN = (1 << (NETWORK_TYPE_IWLAN - 1));

class nsCdmaSmsAddress final : public nsICdmaSmsAddress
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICDMASMSADDRESS

  explicit nsCdmaSmsAddress(int32_t aDigitMode,
                            int32_t aNumberMode,
                            int32_t aNumberType,
                            int32_t aNumberPlan,
                            nsTArray<uint8_t>& aDigits);

private:
  ~nsCdmaSmsAddress();
  int32_t mDigitMode;
  int32_t mNumberMode;
  int32_t mNumberType;
  int32_t mNumberPlan;
  nsTArray<uint8_t> mDigits;
};

class nsCdmaSmsSubAddress final : public nsICdmaSmsSubAddress
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICDMASMSSUBADDRESS
  explicit nsCdmaSmsSubAddress(int32_t aSubaddressType,
                               bool aOdd,
                               nsTArray<uint8_t> aDigits);

private:
  ~nsCdmaSmsSubAddress();
  int32_t mSubAddressType;
  bool mOdd;
  nsTArray<uint8_t> mDigits;
};

class nsCdmaSmsMessage final : public nsICdmaSmsMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICDMASMSMESSAGE

  explicit nsCdmaSmsMessage(int32_t aTeleserviceId,
                            bool aIsServicePresent,
                            int32_t aServiceCategory,
                            RefPtr<nsICdmaSmsAddress>& aCdmaSmsAddress,
                            RefPtr<nsICdmaSmsSubAddress>& aCdmaSmsSubAddress,
                            nsTArray<uint8_t>& aBearerData);

  void updateToDestion(CdmaSmsMessage_V1_1& aMessage);

private:
  ~nsCdmaSmsMessage();

  int32_t mTeleserviceId;
  bool mIsServicePresent;
  int32_t mServiceCategory;
  RefPtr<nsICdmaSmsAddress> mAddress;
  RefPtr<nsICdmaSmsSubAddress> mSubAddress;
  nsTArray<uint8_t> mBearerData;
};

class nsPhonebookRecordInfo final : public nsIPhonebookRecordInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPHONEBOOKRECORDINFO

  nsPhonebookRecordInfo(int32_t aRecordId,
                        const nsAString& aName,
                        const nsAString& aNumber,
                        nsTArray<nsString>& aEmails,
                        nsTArray<nsString>& aAdditionalNumbers);
#if ANDROID_VERSION >= 33
  void updateToDestion(PhonebookRecordInfo_V1_6& aDesMessage);
  void updateToDestion(PhonebookRecordInfoA& aDesMessage);
#endif
private:
  ~nsPhonebookRecordInfo();

  int32_t mRecordId;
  nsString mName;
  nsString mNumber;
  nsTArray<nsString> mEmails;
  nsTArray<nsString> mAdditionalNumbers;
};

class nsSimPhonebookRecordsEvent final : public nsISimPhonebookRecordsEvent
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIMPHONEBOOKRECORDSEVENT
  nsSimPhonebookRecordsEvent(
    uint32_t aStatus,
    nsTArray<RefPtr<nsPhonebookRecordInfo>>& aPhonebookRecordInfos);

private:
  ~nsSimPhonebookRecordsEvent(){};
  uint32_t mStatus;
  nsTArray<RefPtr<nsPhonebookRecordInfo>> mPhonebookRecordInfos;
};

class nsImsiEncryptionInfo final : public nsIImsiEncryptionInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSIENCRYPTIONINFO

  explicit nsImsiEncryptionInfo(const nsAString& mcc,
                                const nsAString& mnc,
                                const nsTArray<uint8_t>& carrierKey,
                                const nsAString& keyIdentifier,
                                int64_t expirationTime);

private:
  ~nsImsiEncryptionInfo();

  nsString mMcc;
  nsString mMnc;
  nsTArray<uint8_t> mCarrierKey;
  nsString mKeyIdentifier;
  int64_t mExpirationTime;
};

class nsRadioAccessSpecifier final : public nsIRadioAccessSpecifier
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRADIOACCESSSPECIFIER

  explicit nsRadioAccessSpecifier(int32_t radioAccessNetwork,
                                  nsTArray<int32_t> geranBands,
                                  nsTArray<int32_t> utranBands,
                                  nsTArray<int32_t> eutranBands,
                                  nsTArray<int32_t> ngranBands,
                                  nsTArray<int32_t> channels);
#if ANDROID_VERSION >= 33
  nsRadioAccessSpecifier(const RadioAccessSpecifier_V1_5& aSpecifier);
#endif
private:
  ~nsRadioAccessSpecifier();
  int32_t mRadioAccessNetwork;
  nsTArray<int32_t> mGeranBands;
  nsTArray<int32_t> mUtranBands;
  nsTArray<int32_t> mEutranBands;
  nsTArray<int32_t> mNgranBands;
  nsTArray<int32_t> mChannels;
};

class nsNetworkScanRequest final : public nsINetworkScanRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKSCANREQUEST

  explicit nsNetworkScanRequest(
    int32_t type,
    int32_t interval,
    nsTArray<RefPtr<nsIRadioAccessSpecifier>>& specifiers,
    int32_t maxSearchTime,
    bool incrementalResults,
    int32_t incrementalResultsPeriodicity,
    nsTArray<nsString> mccMncs);

private:
  ~nsNetworkScanRequest();
  int32_t mType;
  int32_t mInterval;
  nsTArray<RefPtr<nsIRadioAccessSpecifier>> mSpecifiers;
  int32_t mMaxSearchTime;
  bool mIncrementalResults;
  int32_t mIncrementalResultsPeriodicity;
  nsTArray<nsString> mMccMncs;
};

class nsCarrier final : public nsICarrier
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICARRIER

  explicit nsCarrier(nsAString& mcc,
                     nsAString& mnc,
                     uint8_t matchType,
                     nsAString& matchData);

private:
  ~nsCarrier();

  nsString mMcc;
  nsString mMnc;
  // enum matchType should be value of
  //  ALL = 0, Apply to all carriers with the same mcc/mnc
  //  SPN = 1, Use SPN and mcc/mnc to identify the carrier
  //  IMSI_PREFIX = 2, Use IMSI prefix and mcc/mnc to identify the carrier
  //  GID1 = 3, Use GID1 and mcc/mnc to identify the carrier
  //  GID2 = 4, Use GID2 and mcc/mnc to identify the carrier
  uint8_t mMatchType; // Specify match type for the carrier.
                      // If it’s ALL, matchData is empty string;
                      // otherwise, matchData is the value for the match type.
  nsString mMatchData;
};

class nsCarrierRestrictionsWithPriority final
  : public nsICarrierRestrictionsWithPriority
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICARRIERRESTRICTIONSWITHPRIORITY

  nsCarrierRestrictionsWithPriority(
    nsTArray<RefPtr<nsCarrier>>& allowedCarriers,
    nsTArray<RefPtr<nsCarrier>>& excludedCarriers,
    bool allowedCarriersPrioritized);

private:
  ~nsCarrierRestrictionsWithPriority();

  nsTArray<RefPtr<nsCarrier>> mAllowedCarriers;
  nsTArray<RefPtr<nsCarrier>> mExcludedCarriers;
  bool mAllowedCarriersPrioritized;
};
#endif
