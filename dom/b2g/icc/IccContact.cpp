/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccContact.h"
#include "nsReadableUtils.h"
#include "mozilla/dom/IccContactBinding.h"

using namespace mozilla::dom::icc;

NS_IMPL_ISUPPORTS(nsIccContact, nsIIccContact)

/*static*/ nsresult nsIccContact::Create(const nsAString& aId,
                                         const nsTArray<nsString>& aNames,
                                         const nsTArray<nsString>& aNumbers,
                                         const nsTArray<nsString>& aEmails,
                                         nsIIccContact** aIccContact) {
  *aIccContact = nullptr;

  nsCOMPtr<nsIIccContact> iccContact =
      new nsIccContact(aId, aNames, aNumbers, aEmails);
  iccContact.forget(aIccContact);

  return NS_OK;
}

nsIccContact::nsIccContact() {}

nsIccContact::nsIccContact(const nsAString& aId,
                           const nsTArray<nsString>& aNames,
                           const nsTArray<nsString>& aNumbers,
                           const nsTArray<nsString>& aEmails)
    : mId(aId),
      mNames(aNames.Clone()),
      mNumbers(aNumbers.Clone()),
      mEmails(aEmails.Clone()) {}

// nsIIccContact implementation.

NS_IMETHODIMP nsIccContact::GetId(nsAString& aId) {
  aId = mId;
  return NS_OK;
}

#define ICCCONTACT_IMPL_GET_CONTACT_FIELD(_field)                          \
  NS_IMETHODIMP nsIccContact::Get##_field(nsTArray<nsString>& a##_field) { \
    a##_field = m##_field.Clone();                                         \
    return NS_OK;                                                          \
  }

ICCCONTACT_IMPL_GET_CONTACT_FIELD(Names)
ICCCONTACT_IMPL_GET_CONTACT_FIELD(Numbers)
ICCCONTACT_IMPL_GET_CONTACT_FIELD(Emails)

#undef ICCCONTACT_GET_CONTACT_FIELD

// IccContact
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IccContact, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(IccContact)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IccContact)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IccContact)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IccContact::IccContact(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

/* static */ already_AddRefed<IccContact> IccContact::Constructor(
    const GlobalObject& aGlobal, const nsAString& aId, const nsAString& aName,
    const nsAString& aNumber, const nsAString& aEmail, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<IccContact> obj = new IccContact(window);
  obj->mId = aId;
  obj->mNames.AppendElement(aName);
  obj->mNumbers.AppendElement(aNumber);
  obj->mEmails.AppendElement(aEmail);
  return obj.forget();
}

void IccContact::Update(nsIIccContact* aIccContact) {
  aIccContact->GetId(mId);
  aIccContact->GetNames(mNames);
  aIccContact->GetNumbers(mNumbers);
  aIccContact->GetEmails(mEmails);
}

void IccContact::GetId(nsAString& aId) const {
  SetDOMStringToNull(aId);
  aId = mId;
}

void IccContact::GetNumber(nsAString& aNumber) const {
  SetDOMStringToNull(aNumber);
  if (mNumbers.Length() > 0) aNumber = mNumbers[0];
}

void IccContact::GetName(nsAString& aName) const {
  SetDOMStringToNull(aName);
  if (mNames.Length() > 0) aName = mNames[0];
}

void IccContact::GetEmail(nsAString& aEmail) const {
  SetDOMStringToNull(aEmail);
  if (mEmails.Length() > 0) aEmail = mEmails[0];
}

IccContact::~IccContact() {}

// WebIDL implementation
JSObject* IccContact::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return IccContact_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* IccContact::GetParentObject() const { return mWindow; }
