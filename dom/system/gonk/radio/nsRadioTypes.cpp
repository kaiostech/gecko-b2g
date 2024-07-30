/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRadioTypes.h"
#include "nsRilResult.h"
#include <android/log.h>
#include <cstdint>
#ifdef LOGD
#undef LOGD
#endif
#define LOGD(args...)                                                          \
  __android_log_print(ANDROID_LOG_INFO, "nsRadioTypes", ##args)
/* Logging related */
#undef LOG_TAG
#define LOG_TAG "nsRadioTypes"

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ##args)
#define ERROR(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ##args)
#define DEBUG(args...)                                                         \
  do {                                                                         \
    if (gRilDebug_isLoggingEnabled) {                                          \
      __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ##args);                 \
    }                                                                          \
  } while (0)

/*============================================================================
 *============ Implementation of Class nsICdmaSmsAddress ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsCdmaSmsAddress, nsICdmaSmsAddress)

nsCdmaSmsAddress::~nsCdmaSmsAddress() {}
nsCdmaSmsAddress::nsCdmaSmsAddress(int32_t aDigitMode,
                                   int32_t aNumberMode,
                                   int32_t aNumberType,
                                   int32_t aNumberPlan,
                                   nsTArray<uint8_t>& aDigits)
{
  mDigitMode = aDigitMode;
  mNumberMode = aNumberMode;
  mNumberType = aNumberType;
  mNumberPlan = aNumberPlan;
  mDigits.Assign(aDigits);
}

NS_IMETHODIMP
nsCdmaSmsAddress::GetDigitMode(int32_t* aDigitMode)
{
  *aDigitMode = mDigitMode;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsAddress::GetNumberMode(int32_t* aNumberMode)
{
  *aNumberMode = mNumberMode;
  return NS_OK;
}
NS_IMETHODIMP
nsCdmaSmsAddress::GetNumberType(int32_t* aNumberType)
{
  *aNumberType = mNumberType;
  return NS_OK;
}
NS_IMETHODIMP
nsCdmaSmsAddress::GetNumberPlan(int32_t* aNumberPlan)
{
  *aNumberPlan = mNumberPlan;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsAddress::GetDigits(nsTArray<uint8_t>& aCredentialId)
{
  aCredentialId.Assign(mDigits);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsCdmaSmsSubAddress ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsCdmaSmsSubAddress, nsICdmaSmsSubAddress)
nsCdmaSmsSubAddress::nsCdmaSmsSubAddress(int32_t aSubAddressType,
                                         bool aOdd,
                                         nsTArray<uint8_t> aDigits)
{
  mSubAddressType = aSubAddressType;
  mOdd = aOdd;
  mDigits.Assign(aDigits);
}

/*void nsCdmaSmsSubAddress::updateToDestion(CdmaSmsSubaddress_V1_1 &aSubAddress)
{ aSubAddress.subaddressType = (CdmaSmsSubaddressType_V1_1)mSubAddressType;
    aSubAddress.odd = mOdd;
    for (uint32_t i =0 ;i< mDigits.Length(); i++ ) {
        aSubAddress.digits[i] = mDigits[i];
    }
}*/

nsCdmaSmsSubAddress::~nsCdmaSmsSubAddress() {}

NS_IMETHODIMP
nsCdmaSmsSubAddress::GetSubAddressType(int32_t* aSubAddressType)
{
  *aSubAddressType = mSubAddressType;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsSubAddress::GetOdd(bool* aOdd)
{
  *aOdd = mOdd;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsSubAddress::GetDigits(nsTArray<uint8_t>& aDigits)
{
  aDigits.Assign(mDigits);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsCdmaSmsMessage ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsCdmaSmsMessage, nsICdmaSmsMessage)

nsCdmaSmsMessage::nsCdmaSmsMessage(
  int32_t aTeleserviceId,
  bool aIsServicePresent,
  int32_t aServiceCategory,
  RefPtr<nsICdmaSmsAddress>& aCdmaSmsAddress,
  RefPtr<nsICdmaSmsSubAddress>& aCdmaSmsSubAddress,
  nsTArray<uint8_t>& aBearerData)
{
  mTeleserviceId = aTeleserviceId;
  mServiceCategory = aServiceCategory;
  mIsServicePresent = aIsServicePresent;
  mAddress = aCdmaSmsAddress;
  mSubAddress = aCdmaSmsSubAddress;
  mBearerData.Assign(aBearerData);
}

nsCdmaSmsMessage::~nsCdmaSmsMessage() {}

NS_IMETHODIMP
nsCdmaSmsMessage::GetIsServicePresent(bool* aIsServicePresent)
{
  *aIsServicePresent = mIsServicePresent;
  return NS_OK;
}

void
nsCdmaSmsMessage::updateToDestion(CdmaSmsMessage_V1_1& aMessage)
{
  aMessage.teleserviceId = mTeleserviceId;
  aMessage.serviceCategory = mServiceCategory;
  aMessage.isServicePresent = mIsServicePresent;

  int32_t aDigitMode;
  mAddress->GetDigitMode(&aDigitMode);
  aMessage.address.digitMode = (CdmaSmsDigitMode_V1_1)aDigitMode;

  int32_t aNumberMode;
  mAddress->GetNumberMode(&aNumberMode);
  aMessage.address.numberMode = (CdmaSmsNumberMode_V1_1)aNumberMode;

  int32_t aNumberType;
  mAddress->GetNumberType(&aNumberType);
  aMessage.address.numberType = (CdmaSmsNumberType_V1_1)aNumberType;

  int32_t aNumberPlan;
  mAddress->GetNumberPlan(&aNumberPlan);
  aMessage.address.numberPlan = (CdmaSmsNumberPlan_V1_1)aNumberPlan;

  nsTArray<uint8_t> aAddressDigits;
  mAddress->GetDigits(aAddressDigits);
  for (uint32_t i = 0; i < aAddressDigits.Length(); i++) {
    aMessage.address.digits[i] = aAddressDigits[i];
  }

  int32_t aSubAddressType;
  mSubAddress->GetSubAddressType(&aSubAddressType);
  aMessage.subAddress.subaddressType =
    (CdmaSmsSubaddressType_V1_1)aSubAddressType;

  bool aOdd;
  mSubAddress->GetOdd(&aOdd);
  aMessage.subAddress.odd = aOdd;

  nsTArray<uint8_t> aSubAddressDigits;
  mSubAddress->GetDigits(aSubAddressDigits);

  for (uint32_t i = 0; i < aSubAddressDigits.Length(); i++) {
    aMessage.subAddress.digits[i] = aSubAddressDigits[i];
  }

  for (uint32_t i = 0; i < mBearerData.Length(); i++) {
    aMessage.bearerData[i] = mBearerData[i];
  }
}

NS_IMETHODIMP
nsCdmaSmsMessage::GetTeleserviceId(int32_t* aTeleserviceId)
{
  *aTeleserviceId = mTeleserviceId;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsMessage::GetServiceCategory(int32_t* aServiceCategory)
{
  *aServiceCategory = mServiceCategory;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsMessage::GetAddress(nsICdmaSmsAddress** aAddress)
{
  RefPtr<nsICdmaSmsAddress> address(mAddress);
  address.forget(aAddress);
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaSmsMessage::GetSubAddress(nsICdmaSmsSubAddress** aSubAddress)
{
  RefPtr<nsICdmaSmsSubAddress> subAddress(mSubAddress);
  subAddress.forget(aSubAddress);
  return NS_OK;
}
NS_IMETHODIMP
nsCdmaSmsMessage::GetBearerData(nsTArray<uint8_t>& aBearerData)
{
  aBearerData.Assign(mBearerData);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsIPhonebookRecordInfo
 *===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsPhonebookRecordInfo, nsIPhonebookRecordInfo)

nsPhonebookRecordInfo::nsPhonebookRecordInfo(
  int32_t aRecordId,
  const nsAString& aName,
  const nsAString& aNumber,
  nsTArray<nsString>& aEmails,
  nsTArray<nsString>& aAdditionalNumbers)
{
  mRecordId = aRecordId;
  mName = aName;
  mNumber = aNumber;
  mEmails = aEmails.Clone();
  mAdditionalNumbers = aAdditionalNumbers.Clone();
}

nsPhonebookRecordInfo::~nsPhonebookRecordInfo() {}

#if ANDROID_VERSION >= 33
void
nsPhonebookRecordInfo::updateToDestion(PhonebookRecordInfo_V1_6& aDestMessage)
{
  aDestMessage.recordId = mRecordId;
  aDestMessage.name = NS_ConvertUTF16toUTF8(mName).get();
  aDestMessage.number = NS_ConvertUTF16toUTF8(mNumber).get();

  for (uint32_t i = 0; i < mEmails.Length(); i++) {
    aDestMessage.emails[i] = NS_ConvertUTF16toUTF8(mEmails[i]).get();
  }

  for (uint32_t i = 0; i < mAdditionalNumbers.Length(); i++) {
    aDestMessage.additionalNumbers[i] =
      NS_ConvertUTF16toUTF8(mAdditionalNumbers[i]).get();
  }
}

void
nsPhonebookRecordInfo::updateToDestion(PhonebookRecordInfoA& aDestMessage)
{
  aDestMessage.recordId = mRecordId;
  aDestMessage.name = NS_ConvertUTF16toUTF8(mName).get();
  aDestMessage.number = NS_ConvertUTF16toUTF8(mNumber).get();

  for (uint32_t i = 0; i < mEmails.Length(); i++) {
    aDestMessage.emails[i] = NS_ConvertUTF16toUTF8(mEmails[i]).get();
  }

  for (uint32_t i = 0; i < mAdditionalNumbers.Length(); i++) {
    aDestMessage.additionalNumbers[i] =
      NS_ConvertUTF16toUTF8(mAdditionalNumbers[i]).get();
  }
}
#endif
NS_IMETHODIMP
nsPhonebookRecordInfo::GetRecordId(int32_t* aRecordId)
{
  *aRecordId = mRecordId;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookRecordInfo::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookRecordInfo::GetEmails(nsTArray<nsString>& aEmails)
{
  aEmails.Assign(mEmails);
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookRecordInfo::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookRecordInfo::GetAdditionalNumbers(
  nsTArray<nsString>& additionalNumbers)
{
  additionalNumbers.Assign(mAdditionalNumbers);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsSimPhonebookRecordsEvent
 *===================
 *============================================================================*/
/**
 * nsISimPhonebookRecordsEvent implementation
 */
NS_IMPL_ISUPPORTS(nsSimPhonebookRecordsEvent, nsISimPhonebookRecordsEvent)

nsSimPhonebookRecordsEvent::nsSimPhonebookRecordsEvent(
  uint32_t aStatus,
  nsTArray<RefPtr<nsPhonebookRecordInfo>>& aPhonebookRecordInfos)
  : mStatus(aStatus)
  , mPhonebookRecordInfos(aPhonebookRecordInfos.Clone())
{}

NS_IMETHODIMP
nsSimPhonebookRecordsEvent::GetStatus(uint32_t* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsSimPhonebookRecordsEvent::GetPhonebookRecordInfos(
  nsTArray<RefPtr<nsIPhonebookRecordInfo>>& aPhonebookRecordInfos)
{
  for (uint32_t i = 0; i < mPhonebookRecordInfos.Length(); i++) {
    aPhonebookRecordInfos.AppendElement(mPhonebookRecordInfos[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsIImsiEncryptionInfo ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsImsiEncryptionInfo, nsIImsiEncryptionInfo)
nsImsiEncryptionInfo::nsImsiEncryptionInfo(const nsAString& mcc,
                                           const nsAString& mnc,
                                           const nsTArray<uint8_t>& carrierKey,
                                           const nsAString& keyIdentifier,
                                           int64_t expirationTime)
  : mMcc(mcc)
  , mMnc(mnc)
  , mCarrierKey(carrierKey.Clone())
  , mKeyIdentifier(keyIdentifier)
  , mExpirationTime(expirationTime)
{}
nsImsiEncryptionInfo::~nsImsiEncryptionInfo() {}

NS_IMETHODIMP
nsImsiEncryptionInfo::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}
/* attribute AString mnc; */
NS_IMETHODIMP
nsImsiEncryptionInfo::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}
/* attribute Array<octet> carrierKey; */
NS_IMETHODIMP
nsImsiEncryptionInfo::GetCarrierKey(nsTArray<uint8_t>& aCarrierKey)
{
  aCarrierKey = mCarrierKey.Clone();
  return NS_OK;
}
/* attribute AString keyIdentifier; */
NS_IMETHODIMP
nsImsiEncryptionInfo::GetKeyIdentifier(nsAString& aKeyIdentifier)
{
  aKeyIdentifier = mKeyIdentifier;
  return NS_OK;
}

/* attribute long long expirationTime; */
NS_IMETHODIMP
nsImsiEncryptionInfo::GetExpirationTime(int64_t* aExpirationTime)
{
  *aExpirationTime = mExpirationTime;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsINetworkScanRequest ===================
 *============================================================================*/
/**
 * Implementation of Class nsIRadioAccessSpecifier
 */
NS_IMPL_ISUPPORTS(nsRadioAccessSpecifier, nsIRadioAccessSpecifier)
nsRadioAccessSpecifier::nsRadioAccessSpecifier(int32_t radioAccessNetwork,
                                               nsTArray<int32_t> geranBands,
                                               nsTArray<int32_t> utranBands,
                                               nsTArray<int32_t> eutranBands,
                                               nsTArray<int32_t> ngranBands,
                                               nsTArray<int32_t> channels)
  : mRadioAccessNetwork(radioAccessNetwork)
  , mGeranBands(geranBands.Clone())
  , mUtranBands(utranBands.Clone())
  , mEutranBands(eutranBands.Clone())
  , mNgranBands(ngranBands.Clone())
  , mChannels(channels.Clone())
{}

#if ANDROID_VERSION >= 33
nsRadioAccessSpecifier::nsRadioAccessSpecifier(
  const RadioAccessSpecifier_V1_5& aSpecifier)
{
  mRadioAccessNetwork = (int32_t)aSpecifier.radioAccessNetwork;
  switch (aSpecifier.bands.getDiscriminator()) {
    case RadioAccessSpecifier_V1_5::Bands::hidl_discriminator::geranBands:
      for (uint32_t i = 0; i < aSpecifier.bands.geranBands().size(); i++) {
        mGeranBands[i] = (int32_t)aSpecifier.bands.geranBands()[i];
      }
      break;
    case RadioAccessSpecifier_V1_5::Bands::hidl_discriminator::utranBands:
      for (uint32_t i = 0; i < aSpecifier.bands.utranBands().size(); i++) {
        mUtranBands[i] = (int32_t)aSpecifier.bands.utranBands()[i];
      }
      break;
    case RadioAccessSpecifier_V1_5::Bands::hidl_discriminator::eutranBands:
      for (uint32_t i = 0; i < aSpecifier.bands.eutranBands().size(); i++) {
        mEutranBands[i] = (int32_t)aSpecifier.bands.eutranBands()[i];
      }
      break;
    case RadioAccessSpecifier_V1_5::Bands::hidl_discriminator::ngranBands:
      for (uint32_t i = 0; i < aSpecifier.bands.ngranBands().size(); i++) {
        mNgranBands[i] = (int32_t)aSpecifier.bands.ngranBands()[i];
      }
      break;
    default:
      break;
  }

  for (uint32_t i = 0; i < aSpecifier.channels.size(); i++) {
    mChannels.AppendElement(aSpecifier.channels[i]);
  }
}
#endif

nsRadioAccessSpecifier::~nsRadioAccessSpecifier() {}

NS_IMETHODIMP
nsRadioAccessSpecifier::GetRadioAccessNetwork(int32_t* aRadioAccessNetwork)
{
  *aRadioAccessNetwork = mRadioAccessNetwork;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioAccessSpecifier::GetNgranBands(nsTArray<int32_t>& aNgranBands)
{
  aNgranBands = mNgranBands.Clone();
  return NS_OK;
}
NS_IMETHODIMP
nsRadioAccessSpecifier::GetGeranBands(nsTArray<int32_t>& aGeranBands)
{
  aGeranBands = mGeranBands.Clone();
  return NS_OK;
}
NS_IMETHODIMP
nsRadioAccessSpecifier::GetUtranBands(nsTArray<int32_t>& aUtranBands)
{
  aUtranBands = mUtranBands.Clone();
  return NS_OK;
}
NS_IMETHODIMP
nsRadioAccessSpecifier::GetEutranBands(nsTArray<int32_t>& aEutranBands)
{
  aEutranBands = mEutranBands.Clone();
  return NS_OK;
}
NS_IMETHODIMP
nsRadioAccessSpecifier::GetChannels(nsTArray<int32_t>& aChannels)
{
  aChannels = mChannels.Clone();
  return NS_OK;
}

/**
 * Implementation of Class nsINetworkScanRequest
 */
NS_IMPL_ISUPPORTS(nsNetworkScanRequest, nsINetworkScanRequest)
nsNetworkScanRequest::nsNetworkScanRequest(
  int32_t type,
  int32_t interval,
  nsTArray<RefPtr<nsIRadioAccessSpecifier>>& specifiers,
  int32_t maxSearchTime,
  bool incrementalResults,
  int32_t incrementalResultsPeriodicity,
  nsTArray<nsString> mccMncs)
  : mType(type)
  , mInterval(interval)
  , mSpecifiers(specifiers.Clone())
  , mMaxSearchTime(maxSearchTime)
  , mIncrementalResults(incrementalResults)
  , mIncrementalResultsPeriodicity(incrementalResultsPeriodicity)
  , mMccMncs(mccMncs.Clone())
{}

nsNetworkScanRequest::~nsNetworkScanRequest() {}

NS_IMETHODIMP
nsNetworkScanRequest::GetType(int32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetInterval(int32_t* aInterval)
{
  *aInterval = mInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetSpecifiers(
  nsTArray<RefPtr<nsIRadioAccessSpecifier>>& aSpecifiers)
{
  aSpecifiers = mSpecifiers.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetMaxSearchTime(int32_t* aMaxSearchTime)
{
  *aMaxSearchTime = mMaxSearchTime;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetIncrementalResults(bool* aIncrementalResults)
{
  *aIncrementalResults = mIncrementalResults;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetIncrementalResultsPeriodicity(
  int32_t* aIncrementalResultsPeriodicity)
{
  *aIncrementalResultsPeriodicity = mIncrementalResultsPeriodicity;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanRequest::GetMccMncs(nsTArray<nsString>& aMccMncs)
{
  aMccMncs = mMccMncs.Clone();
  return NS_OK;
}

/**
 * Implementation of Class nsICarrier
 */
NS_IMPL_ISUPPORTS(nsCarrier, nsICarrier)
nsCarrier::nsCarrier(nsAString& mcc,
                     nsAString& mnc,
                     uint8_t matchType,
                     nsAString& matchData)
  : mMcc(mcc)
  , mMnc(mnc)
  , mMatchType(matchType)
  , mMatchData(matchData)
{}
nsCarrier::~nsCarrier() {}
NS_IMETHODIMP
nsCarrier::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}
NS_IMETHODIMP
nsCarrier::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}
NS_IMETHODIMP
nsCarrier::GetMatchType(uint8_t* matchType)
{
  *matchType = mMatchType;
  return NS_OK;
}
NS_IMETHODIMP
nsCarrier::GetMatchData(nsAString& aMatchData)
{
  aMatchData = mMatchData;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCarrierRestrictionsWithPriority,
                  nsICarrierRestrictionsWithPriority)
nsCarrierRestrictionsWithPriority::nsCarrierRestrictionsWithPriority(
  nsTArray<RefPtr<nsCarrier>>& allowedCarriers,
  nsTArray<RefPtr<nsCarrier>>& excludedCarriers,
  bool allowedCarriersPrioritized)
  : mAllowedCarriers(allowedCarriers.Clone())
  , mExcludedCarriers(excludedCarriers.Clone())
  , mAllowedCarriersPrioritized(allowedCarriersPrioritized)
{}

nsCarrierRestrictionsWithPriority::~nsCarrierRestrictionsWithPriority() {}

NS_IMETHODIMP
nsCarrierRestrictionsWithPriority::GetAllowedCarriers(
  nsTArray<RefPtr<nsICarrier>>& aAllowedCarriers)
{
  for (uint32_t i = 0; i < mAllowedCarriers.Length(); i++) {
    aAllowedCarriers.AppendElement(mAllowedCarriers[i]);
  }
  return NS_OK;
}
NS_IMETHODIMP
nsCarrierRestrictionsWithPriority::GetExcludedCarriers(
  nsTArray<RefPtr<nsICarrier>>& aExcludedCarriers)
{

  for (uint32_t i = 0; i < mExcludedCarriers.Length(); i++) {
    aExcludedCarriers.AppendElement(mExcludedCarriers[i]);
  }
  return NS_OK;
}
NS_IMETHODIMP
nsCarrierRestrictionsWithPriority::GetAllowedCarriersPrioritized(
  bool* allowedCarriersPrioritized)
{
  *allowedCarriersPrioritized = mAllowedCarriersPrioritized;
  return NS_OK;
}
//~nsCarrierRestrictionsWithPriority() {}
//~nsNetworkScanRequest() {}
//~nsRadioAccessSpecifier() {}
//~nsImsiEncryptionInfo() {}
