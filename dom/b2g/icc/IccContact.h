/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccContact_h
#define mozilla_dom_icc_IccContact_h

#include "nsIIccContact.h"

namespace mozilla {
namespace dom {
namespace icc {

class nsIccContact : public nsIIccContact {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCCONTACT
  explicit nsIccContact();

  static nsresult Create(const nsAString& aId, const nsTArray<nsString>& aNames,
                         const nsTArray<nsString>& aNumbers,
                         const nsTArray<nsString>& aEmails,
                         nsIIccContact** aIccContact);

  nsIccContact(const nsAString& aId, const nsTArray<nsString>& aNames,
               const nsTArray<nsString>& aNumbers,
               const nsTArray<nsString>& aEmails);

 private:
  virtual ~nsIccContact() {}

  nsString mId;
  nsTArray<nsString> mNames;
  nsTArray<nsString> mNumbers;
  nsTArray<nsString> mEmails;
};

class IccContact final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(IccContact)

  explicit IccContact(nsPIDOMWindowInner* aWindow);

  static already_AddRefed<IccContact> Constructor(
      const GlobalObject& aGlobal, const nsAString& aId, const nsAString& aName,
      const nsAString& aNumber, const nsAString& aEmail, ErrorResult& aRv);

  void Update(nsIIccContact* aIccContact);

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetId(nsAString& aId) const;

  void GetNumber(nsAString& aNumber) const;

  void GetName(nsAString& aName) const;

  void GetEmail(nsAString& aEmail) const;

 protected:
  ~IccContact();

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mId;
  nsTArray<nsString> mNames;
  nsTArray<nsString> mNumbers;
  nsTArray<nsString> mEmails;
};

}  // namespace icc
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_icc_IccContact_h
