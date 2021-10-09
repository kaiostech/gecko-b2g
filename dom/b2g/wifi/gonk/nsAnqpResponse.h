/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAnqpResponse_H
#define nsAnqpResponse_H

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIAnqpResponse.h"
#include "nsISupports.h"

class nsI18Name final : public nsII18Name {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSII18NAME
  explicit nsI18Name(const nsAString& aLanguage, const nsAString& aLocale,
                     const nsACString& aText);

 private:
  ~nsI18Name() {}

  nsString mLanguage;
  nsString mLocale;
  nsCString mText;
};

class nsIpAvailability final : public nsIIpAvailability {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIPAVAILABILITY
  explicit nsIpAvailability(int32_t aIpv4Availability,
                            int32_t aIpv6Availability);

 private:
  ~nsIpAvailability() {}

  int32_t mIpv4Availability;
  int32_t mIpv6Availability;
};

class nsExpandedEapMethod final : public nsIExpandedEapMethod {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEXPANDEDEAPMETHOD
  explicit nsExpandedEapMethod(int32_t aVendorId, int64_t aVendorType);

 private:
  ~nsExpandedEapMethod() {}

  int32_t mVendorId;
  int64_t mVendorType;
};

class nsNonEapInnerAuth final : public nsINonEapInnerAuth {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINONEAPINNERAUTH
  explicit nsNonEapInnerAuth(const nsAString& aAuthType);

 private:
  ~nsNonEapInnerAuth() {}

  nsString mAuthType;
};

class nsInnerAuth final : public nsIInnerAuth {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINNERAUTH
  explicit nsInnerAuth(int32_t aEapMethodId);

 private:
  ~nsInnerAuth() {}

  int32_t mEapMethodId;
};

class nsCredentialType final : public nsICredentialType {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICREDENTIALTYPE
  explicit nsCredentialType(int32_t aCredentialType);

 private:
  ~nsCredentialType() {}

  int32_t mCredentialType;
};

class nsVendorSpecificAuth final : public nsIVendorSpecificAuth {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVENDORSPECIFICAUTH
  explicit nsVendorSpecificAuth(const nsAString& aAuthData);

 private:
  ~nsVendorSpecificAuth() {}

  nsString mAuthData;
};

class nsAuthParams final : public nsIAuthParams {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAUTHPARAMS
  explicit nsAuthParams();

  void updateAuthTypeId(const nsTArray<uint32_t>& aAuthTypeId);
  void updateExpandedEapMethod(
      const nsTArray<RefPtr<nsExpandedEapMethod>>& aExpandedEapMethod);
  void updateNonEapInnerAuth(
      const nsTArray<RefPtr<nsNonEapInnerAuth>>& aNonEapInnerAuth);
  void updateInnerAuth(const nsTArray<RefPtr<nsInnerAuth>>& aInnerAuth);
  void updateExpandedInnerEapMethod(
      const nsTArray<RefPtr<nsExpandedEapMethod>>& aExpandedInnerEapMethod);
  void updateCredential(const nsTArray<RefPtr<nsCredentialType>>& aCredential);
  void updateTunneledCredential(
      const nsTArray<RefPtr<nsCredentialType>>& aTunneledCredential);
  void updateVendorSpecificAuth(
      const nsTArray<RefPtr<nsVendorSpecificAuth>>& aVendorSpecificAuth);

 private:
  ~nsAuthParams() {}

  nsTArray<uint32_t> mAuthTypeId;
  nsTArray<RefPtr<nsExpandedEapMethod>> mExpandedEapMethod;
  nsTArray<RefPtr<nsNonEapInnerAuth>> mNonEapInnerAuth;
  nsTArray<RefPtr<nsInnerAuth>> mInnerAuth;
  nsTArray<RefPtr<nsExpandedEapMethod>> mExpandedInnerEapMethod;
  nsTArray<RefPtr<nsCredentialType>> mCredential;
  nsTArray<RefPtr<nsCredentialType>> mTunneledCredential;
  nsTArray<RefPtr<nsVendorSpecificAuth>> mVendorSpecificAuth;
};

class nsEapMethod final : public nsIEapMethod {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEAPMETHOD
  explicit nsEapMethod(int32_t aEapMethodId);

  void updateAuthParams(nsAuthParams* aAuthParams);

 private:
  ~nsEapMethod() {}

  int32_t mEapMethodId;
  RefPtr<nsIAuthParams> mAuthParams;
};

class nsNAIRealmData final : public nsINAIRealmData {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAIREALMDATA
  explicit nsNAIRealmData();

  void updateRealms(const nsTArray<nsString>& aRealms);
  void updateEapMethods(const nsTArray<RefPtr<nsEapMethod>>& aEapMethods);

 private:
  ~nsNAIRealmData() {}

  nsTArray<nsString> mRealms;
  nsTArray<RefPtr<nsEapMethod>> mEapMethods;
};

class nsCellularNetwork final : public nsICellularNetwork {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLULARNETWORK
  explicit nsCellularNetwork();

  void updatePlmnList(const nsTArray<nsString>& aPlmnList);

 private:
  ~nsCellularNetwork() {}

  nsTArray<nsString> mPlmnList;
};

class nsWanMetrics final : public nsIWanMetrics {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWANMETRICS
  explicit nsWanMetrics(int32_t aStatus, bool aSymmetric, bool aCapped,
                        int64_t aDownlinkSpeed, int64_t aUplinkSpeed,
                        int32_t aDownlinkLoad, int32_t aUplinkLoad,
                        int32_t aLmd);

 private:
  ~nsWanMetrics() {}

  int32_t mStatus;
  bool mSymmetric;
  bool mCapped;
  int64_t mDownlinkSpeed;
  int64_t mUplinkSpeed;
  int32_t mDownlinkLoad;
  int32_t mUplinkLoad;
  int32_t mLmd;
};

class nsProtocolPortTuple final : public nsIProtocolPortTuple {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLPORTTUPLE
  explicit nsProtocolPortTuple(int32_t aProtocol, int32_t aPort,
                               int32_t aStatus);

 private:
  ~nsProtocolPortTuple() {}

  int32_t mProtocol;
  int32_t mPort;
  int32_t mStatus;
};

class nsIconInfo final : public nsIIconInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIICONINFO
  explicit nsIconInfo(int32_t aWidth, int32_t aHeight,
                      const nsAString& aLanguage, const nsAString& aIconType,
                      const nsACString& aFileName);

 private:
  ~nsIconInfo() {}

  int32_t mWidth;
  int32_t mHeight;
  nsString mLanguage;
  nsString mIconType;
  nsCString mFileName;
};

class nsFriendlyNameMap final : public nsIFriendlyNameMap {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFRIENDLYNAMEMAP
  explicit nsFriendlyNameMap(const nsAString& mLanguage,
                             const nsACString& mFriendlyName);

 private:
  ~nsFriendlyNameMap() {}

  nsString mLanguage;
  nsCString mFriendlyName;
};

class nsOsuProviderInfo final : public nsIOsuProviderInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOSUPROVIDERINFO
  explicit nsOsuProviderInfo(const nsACString& aServerUri,
                             const nsACString& aNAI);

  void updateFriendlyNames(
      const nsTArray<RefPtr<nsFriendlyNameMap>>& aFriendlyNames);
  void updateMethodList(const nsTArray<int32_t>& aMethodList);
  void updateIconInfoList(const nsTArray<RefPtr<nsIconInfo>>& aIconInfoList);
  void updateServiceDescriptions(
      const nsTArray<RefPtr<nsI18Name>>& aServiceDescriptions);

 private:
  ~nsOsuProviderInfo() {}

  nsCString mServerUri;
  nsCString mNetworkAccessIdentifier;

  nsTArray<RefPtr<nsFriendlyNameMap>> mFriendlyNames;
  nsTArray<int32_t> mMethodList;
  nsTArray<RefPtr<nsIconInfo>> mIconInfoList;
  nsTArray<RefPtr<nsI18Name>> mServiceDescriptions;
};

class nsOsuProviders final : public nsIOsuProviders {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOSUPROVIDERS
  explicit nsOsuProviders(const nsAString& aOsuSsid);

  void updateProviders(const nsTArray<RefPtr<nsOsuProviderInfo>>& aProviders);

 private:
  ~nsOsuProviders() {}

  nsString mOsuSsid;
  nsTArray<RefPtr<nsOsuProviderInfo>> mProviders;
};

class nsAnqpResponse final : public nsIAnqpResponse {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIANQPRESPONSE

  explicit nsAnqpResponse(const nsAString& aBssid);

  void updateIpAvailability(nsIpAvailability* aIpAvailability);
  void updateWanMetrics(nsWanMetrics* aWanMetrics);
  void updateOsuProviders(nsOsuProviders* aOsuProviders);
  void updateVenueName(const nsTArray<RefPtr<nsI18Name>>& aVenueName);
  void updateRoamingConsortiumOIs(
      const nsTArray<int64_t>& aRoamingConsortiumOIs);
  void updateNaiRealmList(
      const nsTArray<RefPtr<nsNAIRealmData>>& aNaiRealmList);
  void updateCellularNetwork(
      const nsTArray<RefPtr<nsCellularNetwork>>& aCellularNetwork);
  void updateDomainName(const nsTArray<nsString>& aDomainName);
  void updateOperatorFriendlyName(
      const nsTArray<RefPtr<nsI18Name>>& aOperatorFriendlyName);
  void updateConnectionCapability(
      const nsTArray<RefPtr<nsProtocolPortTuple>>& aConnectionCapability);

 private:
  ~nsAnqpResponse() {}

  nsString mBssid;
  RefPtr<nsIIpAvailability> mIpAvailability;
  RefPtr<nsIWanMetrics> mWanMetrics;
  RefPtr<nsIOsuProviders> mOsuProviders;
  nsTArray<RefPtr<nsI18Name>> mVenueName;
  nsTArray<int64_t> mRoamingConsortiumOIs;
  nsTArray<RefPtr<nsNAIRealmData>> mNaiRealmList;
  nsTArray<RefPtr<nsCellularNetwork>> mCellularNetwork;
  nsTArray<nsString> mDomainName;
  nsTArray<RefPtr<nsI18Name>> mOperatorFriendlyName;
  nsTArray<RefPtr<nsProtocolPortTuple>> mConnectionCapability;
};

#endif  // nsAnqpResponse_H
