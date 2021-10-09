/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnqpResponse.h"

/**
 * nsI18Name
 */
nsI18Name::nsI18Name(const nsAString& aLanguage, const nsAString& aLocale,
                     const nsACString& aText)
    : mLanguage(aLanguage), mLocale(aLocale), mText(aText) {}

NS_IMETHODIMP
nsI18Name::GetLanguage(nsAString& aLanguage) {
  aLanguage = mLanguage;
  return NS_OK;
}

NS_IMETHODIMP
nsI18Name::GetLocale(nsAString& aLocale) {
  aLocale = mLocale;
  return NS_OK;
}

NS_IMETHODIMP
nsI18Name::GetText(nsACString& aText) {
  aText = mText;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsI18Name, nsII18Name)

/**
 * nsIpAvailability
 */
nsIpAvailability::nsIpAvailability(int32_t aIpv4Availability,
                                   int32_t aIpv6Availability)
    : mIpv4Availability(aIpv4Availability),
      mIpv6Availability(aIpv6Availability) {}

NS_IMETHODIMP
nsIpAvailability::GetIpv4Availability(int32_t* aIpv4Availability) {
  *aIpv4Availability = mIpv4Availability;
  return NS_OK;
}

NS_IMETHODIMP
nsIpAvailability::GetIpv6Availability(int32_t* aIpv6Availability) {
  *aIpv6Availability = mIpv6Availability;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsIpAvailability, nsIIpAvailability)

/**
 * nsExpandedEapMethod
 */
nsExpandedEapMethod::nsExpandedEapMethod(int32_t aVendorId, int64_t aVendorType)
    : mVendorId(aVendorId), mVendorType(aVendorType) {}

NS_IMETHODIMP
nsExpandedEapMethod::GetVendorId(int32_t* aVendorId) {
  *aVendorId = mVendorId;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedEapMethod::GetVendorType(int64_t* aVendorType) {
  *aVendorType = mVendorType;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsExpandedEapMethod, nsIExpandedEapMethod)

/**
 * nsNonEapInnerAuth
 */
nsNonEapInnerAuth::nsNonEapInnerAuth(const nsAString& aAuthType)
    : mAuthType(aAuthType) {}

NS_IMETHODIMP
nsNonEapInnerAuth::GetAuthType(nsAString& aAuthType) {
  aAuthType = mAuthType;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNonEapInnerAuth, nsINonEapInnerAuth)

/**
 * nsInnerAuth
 */
nsInnerAuth::nsInnerAuth(int32_t aEapMethodId) : mEapMethodId(aEapMethodId) {}

NS_IMETHODIMP
nsInnerAuth::GetEapMethodId(int32_t* aEapMethodId) {
  *aEapMethodId = mEapMethodId;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsInnerAuth, nsIInnerAuth)

/**
 * nsCredentialType
 */
nsCredentialType::nsCredentialType(int32_t aCredentialType)
    : mCredentialType(aCredentialType) {}

NS_IMETHODIMP
nsCredentialType::GetCredentialType(int32_t* aCredentialType) {
  *aCredentialType = mCredentialType;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCredentialType, nsICredentialType)

/**
 * nsVendorSpecificAuth
 */
nsVendorSpecificAuth::nsVendorSpecificAuth(const nsAString& aAuthData)
    : mAuthData(aAuthData) {}

NS_IMETHODIMP
nsVendorSpecificAuth::GetAuthData(nsAString& aAuthData) {
  aAuthData = mAuthData;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsVendorSpecificAuth, nsIVendorSpecificAuth)

/**
 * nsAuthParams
 */
nsAuthParams::nsAuthParams() {}

void nsAuthParams::updateAuthTypeId(const nsTArray<uint32_t>& aAuthTypeId) {
  mAuthTypeId = aAuthTypeId.Clone();
}

void nsAuthParams::updateExpandedEapMethod(
    const nsTArray<RefPtr<nsExpandedEapMethod>>& aExpandedEapMethod) {
  mExpandedEapMethod = aExpandedEapMethod.Clone();
}

void nsAuthParams::updateNonEapInnerAuth(
    const nsTArray<RefPtr<nsNonEapInnerAuth>>& aNonEapInnerAuth) {
  mNonEapInnerAuth = aNonEapInnerAuth.Clone();
}

void nsAuthParams::updateInnerAuth(
    const nsTArray<RefPtr<nsInnerAuth>>& aInnerAuth) {
  mInnerAuth = aInnerAuth.Clone();
}

void nsAuthParams::updateExpandedInnerEapMethod(
    const nsTArray<RefPtr<nsExpandedEapMethod>>& aExpandedInnerEapMethod) {
  mExpandedInnerEapMethod = aExpandedInnerEapMethod.Clone();
}

void nsAuthParams::updateCredential(
    const nsTArray<RefPtr<nsCredentialType>>& aCredential) {
  mCredential = aCredential.Clone();
}

void nsAuthParams::updateTunneledCredential(
    const nsTArray<RefPtr<nsCredentialType>>& aTunneledCredential) {
  mTunneledCredential = aTunneledCredential.Clone();
}

void nsAuthParams::updateVendorSpecificAuth(
    const nsTArray<RefPtr<nsVendorSpecificAuth>>& aVendorSpecificAuth) {
  mVendorSpecificAuth = aVendorSpecificAuth.Clone();
}

NS_IMETHODIMP
nsAuthParams::GetAuthTypeId(nsTArray<uint32_t>& aAuthTypeId) {
  aAuthTypeId = mAuthTypeId.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetExpandedEapMethod(
    nsTArray<RefPtr<nsIExpandedEapMethod>>& aExpandedEapMethod) {
  for (uint32_t i = 0; i < mExpandedEapMethod.Length(); i++) {
    aExpandedEapMethod.AppendElement(mExpandedEapMethod[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetNonEapInnerAuth(
    nsTArray<RefPtr<nsINonEapInnerAuth>>& aNonEapInnerAuth) {
  for (uint32_t i = 0; i < mNonEapInnerAuth.Length(); i++) {
    aNonEapInnerAuth.AppendElement(mNonEapInnerAuth[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetInnerAuth(nsTArray<RefPtr<nsIInnerAuth>>& aInnerAuth) {
  for (uint32_t i = 0; i < mInnerAuth.Length(); i++) {
    aInnerAuth.AppendElement(mInnerAuth[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetExpandedInnerEapMethod(
    nsTArray<RefPtr<nsIExpandedEapMethod>>& aExpandedInnerEapMethod) {
  for (uint32_t i = 0; i < mExpandedInnerEapMethod.Length(); i++) {
    aExpandedInnerEapMethod.AppendElement(mExpandedInnerEapMethod[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetCredential(nsTArray<RefPtr<nsICredentialType>>& aCredential) {
  for (uint32_t i = 0; i < mCredential.Length(); i++) {
    aCredential.AppendElement(mCredential[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetTunneledCredential(
    nsTArray<RefPtr<nsICredentialType>>& aTunneledCredential) {
  for (uint32_t i = 0; i < mTunneledCredential.Length(); i++) {
    aTunneledCredential.AppendElement(mTunneledCredential[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAuthParams::GetVendorSpecificAuth(
    nsTArray<RefPtr<nsIVendorSpecificAuth>>& aVendorSpecificAuth) {
  for (uint32_t i = 0; i < mVendorSpecificAuth.Length(); i++) {
    aVendorSpecificAuth.AppendElement(mVendorSpecificAuth[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAuthParams, nsIAuthParams)

/**
 * nsEapMethod
 */
nsEapMethod::nsEapMethod(int32_t aEapMethodId) : mEapMethodId(aEapMethodId) {}

void nsEapMethod::updateAuthParams(nsAuthParams* aAuthParams) {
  mAuthParams = aAuthParams;
}

NS_IMETHODIMP
nsEapMethod::GetEapMethodId(int32_t* aEapMethodId) {
  *aEapMethodId = mEapMethodId;
  return NS_OK;
}

NS_IMETHODIMP
nsEapMethod::GetAuthParams(nsIAuthParams** aAuthParams) {
  RefPtr<nsIAuthParams> authParams(mAuthParams);
  authParams.forget(aAuthParams);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsEapMethod, nsIEapMethod)

/**
 * nsNAIRealmData
 */
nsNAIRealmData::nsNAIRealmData() {}

void nsNAIRealmData::updateRealms(const nsTArray<nsString>& aRealms) {
  mRealms = aRealms.Clone();
}

void nsNAIRealmData::updateEapMethods(
    const nsTArray<RefPtr<nsEapMethod>>& aEapMethods) {
  mEapMethods = aEapMethods.Clone();
}

NS_IMETHODIMP
nsNAIRealmData::GetRealms(nsTArray<nsString>& aRealms) {
  aRealms = mRealms.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsNAIRealmData::GetEapMethods(nsTArray<RefPtr<nsIEapMethod>>& aEapMethods) {
  for (uint32_t i = 0; i < mEapMethods.Length(); i++) {
    aEapMethods.AppendElement(mEapMethods[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNAIRealmData, nsINAIRealmData)

/**
 * nsCellularNetwork
 */
nsCellularNetwork::nsCellularNetwork() {}

void nsCellularNetwork::updatePlmnList(const nsTArray<nsString>& aPlmnList) {
  mPlmnList = aPlmnList.Clone();
}

NS_IMETHODIMP
nsCellularNetwork::GetPlmnList(nsTArray<nsString>& aPlmnList) {
  aPlmnList = mPlmnList.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellularNetwork, nsICellularNetwork)

/**
 * nsWanMetrics
 */
nsWanMetrics::nsWanMetrics(int32_t aStatus, bool aSymmetric, bool aCapped,
                           int64_t aDownlinkSpeed, int64_t aUplinkSpeed,
                           int32_t aDownlinkLoad, int32_t aUplinkLoad,
                           int32_t aLmd)
    : mStatus(aStatus),
      mSymmetric(aSymmetric),
      mCapped(aCapped),
      mDownlinkSpeed(aDownlinkSpeed),
      mUplinkSpeed(aUplinkSpeed),
      mDownlinkLoad(aDownlinkLoad),
      mUplinkLoad(aUplinkLoad),
      mLmd(aLmd) {}

NS_IMETHODIMP
nsWanMetrics::GetStatus(int32_t* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetSymmetric(bool* aSymmetric) {
  *aSymmetric = mSymmetric;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetCapped(bool* aCapped) {
  *aCapped = mCapped;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetDownlinkSpeed(int64_t* aDownlinkSpeed) {
  *aDownlinkSpeed = mDownlinkSpeed;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetUplinkSpeed(int64_t* aUplinkSpeed) {
  *aUplinkSpeed = mUplinkSpeed;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetDownlinkLoad(int32_t* aDownlinkLoad) {
  *aDownlinkLoad = mDownlinkLoad;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetUplinkLoad(int32_t* aUplinkLoad) {
  *aUplinkLoad = mUplinkLoad;
  return NS_OK;
}

NS_IMETHODIMP
nsWanMetrics::GetLmd(int32_t* aLmd) {
  *aLmd = mLmd;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsWanMetrics, nsIWanMetrics)

/**
 * nsProtocolPortTuple
 */
nsProtocolPortTuple::nsProtocolPortTuple(int32_t aProtocol, int32_t aPort,
                                         int32_t aStatus)
    : mProtocol(aProtocol), mPort(aPort), mStatus(aStatus) {}

NS_IMETHODIMP
nsProtocolPortTuple::GetProtocol(int32_t* aProtocol) {
  *aProtocol = mProtocol;
  return NS_OK;
}

NS_IMETHODIMP
nsProtocolPortTuple::GetPort(int32_t* aPort) {
  *aPort = mPort;
  return NS_OK;
}

NS_IMETHODIMP
nsProtocolPortTuple::GetStatus(int32_t* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsProtocolPortTuple, nsIProtocolPortTuple)

/**
 * nsIconInfo
 */
nsIconInfo::nsIconInfo(int32_t aWidth, int32_t aHeight,
                       const nsAString& aLanguage, const nsAString& aIconType,
                       const nsACString& aFileName)
    : mWidth(aWidth),
      mHeight(aHeight),
      mLanguage(aLanguage),
      mIconType(aIconType),
      mFileName(aFileName) {}

NS_IMETHODIMP
nsIconInfo::GetWidth(int32_t* aWidth) {
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsIconInfo::GetHeight(int32_t* aHeight) {
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsIconInfo::GetLanguage(nsAString& aLanguage) {
  aLanguage = mLanguage;
  return NS_OK;
}

NS_IMETHODIMP
nsIconInfo::GetIconType(nsAString& aIconType) {
  aIconType = mIconType;
  return NS_OK;
}

NS_IMETHODIMP
nsIconInfo::GetFileName(nsACString& aFileName) {
  aFileName = mFileName;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsIconInfo, nsIIconInfo)

/**
 * nsFriendlyNameMap
 */
nsFriendlyNameMap::nsFriendlyNameMap(const nsAString& aLanguage,
                                     const nsACString& aFriendlyName)
    : mLanguage(aLanguage), mFriendlyName(aFriendlyName) {}

NS_IMETHODIMP
nsFriendlyNameMap::GetLanguage(nsAString& aLanguage) {
  aLanguage = mLanguage;
  return NS_OK;
}

NS_IMETHODIMP
nsFriendlyNameMap::GetFriendlyName(nsACString& aFriendlyName) {
  aFriendlyName = mFriendlyName;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsFriendlyNameMap, nsIFriendlyNameMap)

/**
 * nsOsuProviderInfo
 */
nsOsuProviderInfo::nsOsuProviderInfo(const nsACString& aServerUri,
                                     const nsACString& aNAI)
    : mServerUri(aServerUri), mNetworkAccessIdentifier(aNAI) {}

void nsOsuProviderInfo::updateFriendlyNames(
    const nsTArray<RefPtr<nsFriendlyNameMap>>& aFriendlyNames) {
  mFriendlyNames = aFriendlyNames.Clone();
}

void nsOsuProviderInfo::updateMethodList(const nsTArray<int32_t>& aMethodList) {
  mMethodList = aMethodList.Clone();
}

void nsOsuProviderInfo::updateIconInfoList(
    const nsTArray<RefPtr<nsIconInfo>>& aIconInfoList) {
  mIconInfoList = aIconInfoList.Clone();
}

void nsOsuProviderInfo::updateServiceDescriptions(
    const nsTArray<RefPtr<nsI18Name>>& aServiceDescriptions) {
  mServiceDescriptions = aServiceDescriptions.Clone();
}

NS_IMETHODIMP
nsOsuProviderInfo::GetServerUri(nsACString& aServerUri) {
  aServerUri = mServerUri;
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviderInfo::GetNetworkAccessIdentifier(
    nsACString& aNetworkAccessIdentifier) {
  aNetworkAccessIdentifier = mNetworkAccessIdentifier;
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviderInfo::GetFriendlyNames(
    nsTArray<RefPtr<nsIFriendlyNameMap>>& aFriendlyNames) {
  for (size_t i = 0; i < mFriendlyNames.Length(); i++) {
    aFriendlyNames.AppendElement(mFriendlyNames[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviderInfo::GetMethodList(nsTArray<int32_t>& aMethodList) {
  aMethodList = mMethodList.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviderInfo::GetIconInfoList(
    nsTArray<RefPtr<nsIIconInfo>>& aIconInfoList) {
  for (size_t i = 0; i < mIconInfoList.Length(); i++) {
    aIconInfoList.AppendElement(mIconInfoList[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviderInfo::GetServiceDescriptions(
    nsTArray<RefPtr<nsII18Name>>& aServiceDescriptions) {
  for (size_t i = 0; i < mServiceDescriptions.Length(); i++) {
    aServiceDescriptions.AppendElement(mServiceDescriptions[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsOsuProviderInfo, nsIOsuProviderInfo)

/**
 * nsOsuProviders
 */
nsOsuProviders::nsOsuProviders(const nsAString& aOsuSsid)
    : mOsuSsid(aOsuSsid) {}

void nsOsuProviders::updateProviders(
    const nsTArray<RefPtr<nsOsuProviderInfo>>& aProviders) {
  mProviders = aProviders.Clone();
}

NS_IMETHODIMP
nsOsuProviders::GetOsuSsid(nsAString& aOsuSsid) {
  aOsuSsid = mOsuSsid;
  return NS_OK;
}

NS_IMETHODIMP
nsOsuProviders::GetProviders(nsTArray<RefPtr<nsIOsuProviderInfo>>& aProviders) {
  for (size_t i = 0; i < mProviders.Length(); i++) {
    aProviders.AppendElement(mProviders[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsOsuProviders, nsIOsuProviders)

/**
 * nsAnqpResponse
 */
nsAnqpResponse::nsAnqpResponse(const nsAString& aBssid) : mBssid(aBssid) {}

void nsAnqpResponse::updateIpAvailability(nsIpAvailability* aIpAvailability) {
  mIpAvailability = aIpAvailability;
}

void nsAnqpResponse::updateWanMetrics(nsWanMetrics* aWanMetrics) {
  mWanMetrics = aWanMetrics;
}

void nsAnqpResponse::updateOsuProviders(nsOsuProviders* aOsuProviders) {
  mOsuProviders = aOsuProviders;
}

void nsAnqpResponse::updateVenueName(
    const nsTArray<RefPtr<nsI18Name>>& aVenueName) {
  mVenueName = aVenueName.Clone();
}

void nsAnqpResponse::updateRoamingConsortiumOIs(
    const nsTArray<int64_t>& aRoamingConsortiumOIs) {
  mRoamingConsortiumOIs = aRoamingConsortiumOIs.Clone();
}

void nsAnqpResponse::updateNaiRealmList(
    const nsTArray<RefPtr<nsNAIRealmData>>& aNaiRealmList) {
  mNaiRealmList = aNaiRealmList.Clone();
}

void nsAnqpResponse::updateCellularNetwork(
    const nsTArray<RefPtr<nsCellularNetwork>>& aCellularNetwork) {
  mCellularNetwork = aCellularNetwork.Clone();
}

void nsAnqpResponse::updateDomainName(const nsTArray<nsString>& aDomainName) {
  mDomainName = aDomainName.Clone();
}

void nsAnqpResponse::updateOperatorFriendlyName(
    const nsTArray<RefPtr<nsI18Name>>& aOperatorFriendlyName) {
  mOperatorFriendlyName = aOperatorFriendlyName.Clone();
}

void nsAnqpResponse::updateConnectionCapability(
    const nsTArray<RefPtr<nsProtocolPortTuple>>& aConnectionCapability) {
  mConnectionCapability = aConnectionCapability.Clone();
}

NS_IMETHODIMP
nsAnqpResponse::GetBssid(nsAString& aBssid) {
  aBssid = mBssid;
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetIpAvailability(nsIIpAvailability** aIpAvailability) {
  RefPtr<nsIIpAvailability> availability(mIpAvailability);
  availability.forget(aIpAvailability);
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetHsWanMetrics(nsIWanMetrics** aWanMetrics) {
  RefPtr<nsIWanMetrics> wanMetrics(mWanMetrics);
  wanMetrics.forget(aWanMetrics);
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetHsOsuProviders(nsIOsuProviders** aOsuProviders) {
  RefPtr<nsIOsuProviders> providers(mOsuProviders);
  providers.forget(aOsuProviders);
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetVenueName(nsTArray<RefPtr<nsII18Name>>& aVenueName) {
  for (uint32_t i = 0; i < mVenueName.Length(); i++) {
    aVenueName.AppendElement(mVenueName[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetRoamingConsortiumOIs(
    nsTArray<int64_t>& aRoamingConsortiumOIs) {
  aRoamingConsortiumOIs = mRoamingConsortiumOIs.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetNaiRealmList(
    nsTArray<RefPtr<nsINAIRealmData>>& aNaiRealmList) {
  for (uint32_t i = 0; i < mNaiRealmList.Length(); i++) {
    aNaiRealmList.AppendElement(mNaiRealmList[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetCellularNetwork(
    nsTArray<RefPtr<nsICellularNetwork>>& aCellularNetwork) {
  for (uint32_t i = 0; i < mCellularNetwork.Length(); i++) {
    aCellularNetwork.AppendElement(mCellularNetwork[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetDomainName(nsTArray<nsString>& aDomainName) {
  aDomainName = mDomainName.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetOperatorFriendlyName(
    nsTArray<RefPtr<nsII18Name>>& aOperatorFriendlyName) {
  for (uint32_t i = 0; i < mOperatorFriendlyName.Length(); i++) {
    aOperatorFriendlyName.AppendElement(mOperatorFriendlyName[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAnqpResponse::GetConnectionCapability(
    nsTArray<RefPtr<nsIProtocolPortTuple>>& aConnectionCapability) {
  for (uint32_t i = 0; i < mConnectionCapability.Length(); i++) {
    aConnectionCapability.AppendElement(mConnectionCapability[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAnqpResponse, nsIAnqpResponse)
