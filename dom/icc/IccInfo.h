/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccInfo_h
#define mozilla_dom_IccInfo_h

#include "IccInfoBinding.h"
#include "nsIIccInfo.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

namespace icc {
class IccInfoData;
}  // namespace icc

class nsIccInfo : public nsIIccInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCINFO

  explicit nsIccInfo();
  explicit nsIccInfo(const icc::IccInfoData& aData);

  void Update(nsIIccInfo* aInfo);

 protected:
  virtual ~nsIccInfo() {}

 private:
  nsString mIccType;
  nsString mIccid;
  nsString mMcc;
  nsString mMnc;
  nsString mSpn;
  nsString mImsi;
  nsString mGid1;
  nsString mGid2;
  bool mIsDisplayNetworkNameRequired;
  bool mIsDisplaySpnRequired;
};

class nsGsmIccInfo final : public nsIccInfo, public nsIGsmIccInfo {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGSMICCINFO
  NS_FORWARD_NSIICCINFO(nsIccInfo::)

  nsGsmIccInfo();
  explicit nsGsmIccInfo(const icc::IccInfoData& aData);

  void Update(nsIGsmIccInfo* aInfo);

 private:
  ~nsGsmIccInfo() {}

  nsString mPhoneNumber;
};

class nsCdmaIccInfo final : public nsIccInfo, public nsICdmaIccInfo {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICDMAICCINFO
  NS_FORWARD_NSIICCINFO(nsIccInfo::)

  nsCdmaIccInfo();
  explicit nsCdmaIccInfo(const icc::IccInfoData& aData);

  void Update(nsICdmaIccInfo* aInfo);

 private:
  ~nsCdmaIccInfo() {}

  nsString mPhoneNumber;
  int32_t mPrlVersion;
};

class IccInfo : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(IccInfo)

  explicit IccInfo(nsPIDOMWindowInner* aWindow);
  explicit IccInfo(const icc::IccInfoData& aData);

  void Update(nsIIccInfo* aInfo);
  void Update(IccInfo* aInfo);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  Nullable<IccType> GetIccType() const;

  void GetIccid(nsAString& aIccId) const;

  void GetMcc(nsAString& aMcc) const;

  void GetMnc(nsAString& aMnc) const;

  void GetSpn(nsAString& aSpn) const;

  void GetImsi(nsAString& aImsi) const;

  void GetGid1(nsAString& aGid1) const;

  void GetGid2(nsAString& aGid2) const;

  bool IsDisplayNetworkNameRequired() const;

  bool IsDisplaySpnRequired() const;

  void GetIccInfo(nsIIccInfo** aIccInfo) const;

 protected:
  virtual ~IccInfo() {}

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<nsIccInfo> mIccInfo;
};

class GsmIccInfo final : public IccInfo {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GsmIccInfo, IccInfo)

  explicit GsmIccInfo(nsPIDOMWindowInner* aWindow);
  explicit GsmIccInfo(const icc::IccInfoData& aData);

  void Update(nsIGsmIccInfo* aInfo);
  void Update(GsmIccInfo* aInfo);

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // GsmIccInfo WebIDL
  void GetMsisdn(nsAString& aMsisdn) const;

 private:
  RefPtr<nsGsmIccInfo> mGsmIccInfo;
  ~GsmIccInfo() {}
};

class CdmaIccInfo final : public IccInfo {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CdmaIccInfo, IccInfo)

  explicit CdmaIccInfo(nsPIDOMWindowInner* aWindow);
  explicit CdmaIccInfo(const icc::IccInfoData& aData);

  void Update(nsICdmaIccInfo* aInfo);
  void Update(CdmaIccInfo* aInfo);

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // CdmaIccInfo WebIDL
  void GetMdn(nsAString& aMdn) const;
  int32_t PrlVersion() const;

 private:
  RefPtr<nsCdmaIccInfo> mCdmaIccInfo;
  ~CdmaIccInfo() {}
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_IccInfo_h
