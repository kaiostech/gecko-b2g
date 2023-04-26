/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IccInfo.h"

#include "mozilla/dom/icc/PIccTypes.h"
#include "nsPIDOMWindow.h"

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)          \
  {                                                                         \
    uint32_t i = 0;                                                         \
    for (const EnumEntry* entry = _enumType##Values::strings; entry->value; \
         ++entry, ++i) {                                                    \
      if (_string.EqualsASCII(entry->value)) {                              \
        _enum.SetValue(static_cast<_enumType>(i));                          \
      }                                                                     \
    }                                                                       \
  }

using namespace mozilla::dom;

using mozilla::dom::icc::IccInfoData;

// nsIccInfo
NS_IMPL_ISUPPORTS(nsIccInfo, nsIIccInfo)

nsIccInfo::nsIccInfo() {
  mIccType.SetIsVoid(true);
  mIccid.SetIsVoid(true);
  mMcc.SetIsVoid(true);
  mMnc.SetIsVoid(true);
  mSpn.SetIsVoid(true);
  mImsi.SetIsVoid(true);
  mGid1.SetIsVoid(true);
  mGid2.SetIsVoid(true);
}

nsIccInfo::nsIccInfo(const IccInfoData& aData) {
  mIccType = aData.iccType();
  mIccid = aData.iccid();
  mMcc = aData.mcc();
  mMnc = aData.mnc();
  mSpn = aData.spn();
  mImsi = aData.imsi();
  mGid1 = aData.gid1();
  mGid2 = aData.gid2();
  mIsDisplayNetworkNameRequired = aData.isDisplayNetworkNameRequired();
  mIsDisplaySpnRequired = aData.isDisplaySpnRequired();
}

void nsIccInfo::Update(nsIIccInfo* aInfo) {
  NS_ASSERTION(aInfo, "aInfo is null");

  aInfo->GetIccType(mIccType);
  aInfo->GetIccid(mIccid);
  aInfo->GetMcc(mMcc);
  aInfo->GetMnc(mMnc);
  aInfo->GetSpn(mSpn);
  aInfo->GetImsi(mImsi);
  aInfo->GetGid1(mGid1);
  aInfo->GetGid2(mGid2);
  aInfo->GetIsDisplayNetworkNameRequired(&mIsDisplayNetworkNameRequired);
  aInfo->GetIsDisplaySpnRequired(&mIsDisplaySpnRequired);
}

// nsIIccInfo
NS_IMETHODIMP
nsIccInfo::GetIccType(nsAString& aIccType) {
  aIccType = mIccType;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetIccid(nsAString& aIccid) {
  aIccid = mIccid;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetMcc(nsAString& aMcc) {
  aMcc = mMcc;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetMnc(nsAString& aMnc) {
  aMnc = mMnc;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetSpn(nsAString& aSpn) {
  aSpn = mSpn;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetImsi(nsAString& aImsi) {
  aImsi = mImsi;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetGid1(nsAString& aGid1) {
  aGid1 = mGid1;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetGid2(nsAString& aGid2) {
  aGid2 = mGid2;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetIsDisplayNetworkNameRequired(
    bool* aIsDisplayNetworkNameRequired) {
  *aIsDisplayNetworkNameRequired = mIsDisplayNetworkNameRequired;
  return NS_OK;
}

NS_IMETHODIMP
nsIccInfo::GetIsDisplaySpnRequired(bool* aIsDisplaySpnRequired) {
  *aIsDisplaySpnRequired = mIsDisplaySpnRequired;
  return NS_OK;
}

// nsGsmIccInfo

NS_IMPL_ISUPPORTS_INHERITED(nsGsmIccInfo, nsIccInfo, nsIGsmIccInfo)

nsGsmIccInfo::nsGsmIccInfo() : nsIccInfo() { mPhoneNumber.SetIsVoid(true); }

nsGsmIccInfo::nsGsmIccInfo(const IccInfoData& aData) : nsIccInfo(aData) {
  mPhoneNumber = aData.phoneNumber();
}

void nsGsmIccInfo::Update(nsIGsmIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  nsIccInfo::Update(aInfo);
  aInfo->GetMsisdn(mPhoneNumber);
}

// nsIGsmIccInfo

NS_IMETHODIMP
nsGsmIccInfo::GetMsisdn(nsAString& aMsisdn) {
  aMsisdn = mPhoneNumber;
  return NS_OK;
}

// nsCdmaInfo

NS_IMPL_ISUPPORTS_INHERITED(nsCdmaIccInfo, nsIccInfo, nsICdmaIccInfo)

nsCdmaIccInfo::nsCdmaIccInfo() : nsIccInfo() { mPhoneNumber.SetIsVoid(true); }
nsCdmaIccInfo::nsCdmaIccInfo(const IccInfoData& aData) : nsIccInfo(aData) {
  mPhoneNumber = aData.phoneNumber();
  mPrlVersion = aData.prlVersion();
}

void nsCdmaIccInfo::Update(nsICdmaIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  nsIccInfo::Update(aInfo);

  aInfo->GetMdn(mPhoneNumber);
  aInfo->GetPrlVersion(&mPrlVersion);
}

// nsICdmaIccInfo

NS_IMETHODIMP
nsCdmaIccInfo::GetMdn(nsAString& aMdn) {
  aMdn = mPhoneNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsCdmaIccInfo::GetPrlVersion(int32_t* aPrlVersion) {
  *aPrlVersion = mPrlVersion;
  return NS_OK;
}

// IccInfo
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IccInfo, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(IccInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IccInfo)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IccInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IccInfo::IccInfo(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
  mIccInfo = new nsIccInfo();
}

IccInfo::IccInfo(const IccInfoData& aData) { mIccInfo = new nsIccInfo(aData); }

void IccInfo::Update(nsIIccInfo* aInfo) {
  NS_ASSERTION(aInfo, "aInfo is null");
  mIccInfo->Update(aInfo);
}

void IccInfo::Update(IccInfo* aInfo) {
  nsIIccInfo* iccInfo;
  aInfo->GetIccInfo(&iccInfo);
  mIccInfo->Update(iccInfo);
}

void IccInfo::GetIccInfo(nsIIccInfo** aIccInfo) const {
  NS_IF_ADDREF(*aIccInfo = mIccInfo);
}

// WebIDL implementation

JSObject* IccInfo::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return IccInfo_Binding::Wrap(aCx, this, aGivenProto);
}

Nullable<IccType> IccInfo::GetIccType() const {
  nsAutoString type;
  mIccInfo->GetIccType(type);
  Nullable<IccType> iccType;

  CONVERT_STRING_TO_NULLABLE_ENUM(type, IccType, iccType);

  return iccType;
}

void IccInfo::GetIccid(nsAString& aIccId) const {
  SetDOMStringToNull(aIccId);
  mIccInfo->GetIccid(aIccId);
}

void IccInfo::GetMcc(nsAString& aMcc) const {
  SetDOMStringToNull(aMcc);
  mIccInfo->GetMcc(aMcc);
}

void IccInfo::GetMnc(nsAString& aMnc) const {
  SetDOMStringToNull(aMnc);
  mIccInfo->GetMnc(aMnc);
}

void IccInfo::GetSpn(nsAString& aSpn) const {
  SetDOMStringToNull(aSpn);
  mIccInfo->GetSpn(aSpn);
}

void IccInfo::GetImsi(nsAString& aImsi) const {
  SetDOMStringToNull(aImsi);
  mIccInfo->GetImsi(aImsi);
}

void IccInfo::GetGid1(nsAString& aGid1) const {
  SetDOMStringToNull(aGid1);
  mIccInfo->GetGid1(aGid1);
}

void IccInfo::GetGid2(nsAString& aGid2) const {
  SetDOMStringToNull(aGid2);
  mIccInfo->GetGid2(aGid2);
}

bool IccInfo::IsDisplayNetworkNameRequired() const {
  bool required = false;
  mIccInfo->GetIsDisplayNetworkNameRequired(&required);
  return required;
}

bool IccInfo::IsDisplaySpnRequired() const {
  bool required = false;
  mIccInfo->GetIsDisplaySpnRequired(&required);
  return required;
}

// GsmIccInfo
NS_IMPL_CYCLE_COLLECTION_CLASS(GsmIccInfo)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GsmIccInfo, IccInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GsmIccInfo, IccInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GsmIccInfo)
NS_INTERFACE_MAP_END_INHERITING(IccInfo)

NS_IMPL_ADDREF_INHERITED(GsmIccInfo, IccInfo)
NS_IMPL_RELEASE_INHERITED(GsmIccInfo, IccInfo)

GsmIccInfo::GsmIccInfo(nsPIDOMWindowInner* aWindow) : IccInfo(aWindow) {
  mIccInfo = new nsGsmIccInfo();
  mGsmIccInfo = static_cast<nsGsmIccInfo*>(mIccInfo.get());
}

GsmIccInfo::GsmIccInfo(const IccInfoData& aData) : IccInfo(aData) {
  mIccInfo = new nsGsmIccInfo(aData);
  mGsmIccInfo = static_cast<nsGsmIccInfo*>(mIccInfo.get());
}

// WebIDL implementation

void GsmIccInfo::Update(nsIGsmIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  mGsmIccInfo->Update(aInfo);
}

void GsmIccInfo::Update(GsmIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  nsIIccInfo* iccInfo;
  aInfo->GetIccInfo(&iccInfo);
  nsCOMPtr<nsIGsmIccInfo> gsmInfo = do_QueryInterface(iccInfo);
  mGsmIccInfo->Update(gsmInfo);
}

JSObject* GsmIccInfo::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return GsmIccInfo_Binding::Wrap(aCx, this, aGivenProto);
}

void GsmIccInfo::GetMsisdn(nsAString& aMsisdn) const {
  SetDOMStringToNull(aMsisdn);
  if (mGsmIccInfo) {
    mGsmIccInfo->GetMsisdn(aMsisdn);
  }
}

// CdmaIccInfo
NS_IMPL_CYCLE_COLLECTION_CLASS(CdmaIccInfo)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CdmaIccInfo, IccInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CdmaIccInfo, IccInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CdmaIccInfo)
NS_INTERFACE_MAP_END_INHERITING(IccInfo)

NS_IMPL_ADDREF_INHERITED(CdmaIccInfo, IccInfo)
NS_IMPL_RELEASE_INHERITED(CdmaIccInfo, IccInfo)

CdmaIccInfo::CdmaIccInfo(nsPIDOMWindowInner* aWindow) : IccInfo(aWindow) {
  mIccInfo = new nsCdmaIccInfo();
  mCdmaIccInfo = static_cast<nsCdmaIccInfo*>(mIccInfo.get());
}

CdmaIccInfo::CdmaIccInfo(const IccInfoData& aData) : IccInfo(aData) {
  mIccInfo = new nsCdmaIccInfo(aData);
  mCdmaIccInfo = static_cast<nsCdmaIccInfo*>(mIccInfo.get());
}

// WebIDL implementation

JSObject* CdmaIccInfo::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return CdmaIccInfo_Binding::Wrap(aCx, this, aGivenProto);
}

void CdmaIccInfo::GetMdn(nsAString& aMdn) const {
  SetDOMStringToNull(aMdn);
  if (mCdmaIccInfo) {
    mCdmaIccInfo->GetMdn(aMdn);
  }
}

int32_t CdmaIccInfo::PrlVersion() const {
  int32_t version = 0;
  if (mCdmaIccInfo) {
    mCdmaIccInfo->GetPrlVersion(&version);
  }
  return version;
}

void CdmaIccInfo::Update(nsICdmaIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  mCdmaIccInfo->Update(aInfo);
}

void CdmaIccInfo::Update(CdmaIccInfo* aInfo) {
  MOZ_ASSERT(aInfo);
  nsIIccInfo* iccInfo;
  aInfo->GetIccInfo(&iccInfo);
  nsCOMPtr<nsICdmaIccInfo> cdmaInfo = do_QueryInterface(iccInfo);
  mCdmaIccInfo->Update(cdmaInfo);
}
