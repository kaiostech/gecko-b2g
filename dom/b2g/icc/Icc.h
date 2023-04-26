/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Icc_h
#define mozilla_dom_Icc_h

#include "mozilla/dom/IccBinding.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsIIcc;
class nsIIccInfo;
class nsIIccProvider;
class nsIStkProactiveCmd;

namespace mozilla {
namespace dom {

namespace icc {
class IccContact;
}

class DOMRequest;
class OwningIccInfoOrGsmIccInfoOrCdmaIccInfo;
class Promise;

class Icc final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Icc, DOMEventTargetHelper)
  // NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  Icc(nsPIDOMWindowInner* aWindow, nsIIcc* aHandler, nsIIccInfo* aIccInfo);

  void Shutdown();

  nsresult NotifyEvent(const nsAString& aName);

  nsresult NotifyStkEvent(const nsAString& aName,
                          nsIStkProactiveCmd* aStkProactiveCmd);

  nsString GetIccId() { return mIccId; }

  void UpdateIccInfo(nsIIccInfo* aIccInfo);

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Icc WebIDL
  void GetIccInfo(
      Nullable<OwningIccInfoOrGsmIccInfoOrCdmaIccInfo>& aIccInfo) const;

  Nullable<IccCardState> GetCardState() const;

  Nullable<IccCardState> GetPin2CardState() const;

  void SendStkResponse(const JSContext* aCx, JS::Handle<JS::Value> aCommand,
                       JS::Handle<JS::Value> aResponse, ErrorResult& aRv);

  void SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested,
                            ErrorResult& aRv);

  void SendStkTimerExpiration(const JSContext* aCx,
                              JS::Handle<JS::Value> aTimer, ErrorResult& aRv);

  void SendStkEventDownload(const JSContext* aCx, JS::Handle<JS::Value> aEvent,
                            ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetCardLock(IccLockType aLockType,
                                           ErrorResult& aRv);

  already_AddRefed<DOMRequest> UnlockCardLock(
      const IccUnlockCardLockOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> SetCardLock(
      const IccSetCardLockOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> ReadContacts(IccContactType aContactType,
                                            ErrorResult& aRv);

  already_AddRefed<DOMRequest> UpdateContact(IccContactType aContactType,
                                             const icc::IccContact& aContact,
                                             const nsAString& aPin2,
                                             ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetIccAuthentication(IccAppType aAppType,
                                                    IccAuthType aAuthType,
                                                    const nsAString& aAuthData,
                                                    ErrorResult& aRv);

  already_AddRefed<DOMRequest> MatchMvno(IccMvnoType aMvnoType,
                                         const nsAString& aMatchData,
                                         ErrorResult& aRv);

  already_AddRefed<Promise> GetServiceState(IccService aService,
                                            ErrorResult& aRv);

  IMPL_EVENT_HANDLER(iccinfochange)
  IMPL_EVENT_HANDLER(cardstatechange)
  IMPL_EVENT_HANDLER(stkcommand)
  IMPL_EVENT_HANDLER(stksessionend)

 private:
  // Put definition of the destructor in Icc.cpp to ensure forward declaration
  // of nsIIccProvider, nsIIcc for the auto-generated .cpp file (i.e.,
  // IccManagerBinding.cpp) that includes this header.
  ~Icc();

  bool mLive;
  nsString mIccId;
  // mHandler will be released at Shutdown(), so there is no need to join cycle
  // collection.
  nsCOMPtr<nsIIcc> mHandler;
  Nullable<OwningIccInfoOrGsmIccInfoOrCdmaIccInfo> mIccInfo;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_icc_Icc_h
