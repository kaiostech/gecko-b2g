/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/icc/IccIPCUtils.h"
#include "mozilla/dom/icc/PIccTypes.h"
#include "nsIIccContact.h"
#include "nsIIccInfo.h"

namespace mozilla {
namespace dom {
namespace icc {

/*static*/ void IccIPCUtils::GetIccInfoDataFromIccInfo(nsIIccInfo* aInInfo,
                                                       IccInfoData& aOutData) {
  aInInfo->GetIccType(aOutData.iccType());
  aInInfo->GetIccid(aOutData.iccid());
  aInInfo->GetMcc(aOutData.mcc());
  aInInfo->GetMnc(aOutData.mnc());
  aInInfo->GetSpn(aOutData.spn());
  aInInfo->GetImsi(aOutData.imsi());
  aInInfo->GetGid1(aOutData.gid1());
  aInInfo->GetGid2(aOutData.gid2());
  aInInfo->GetIsDisplayNetworkNameRequired(
      &aOutData.isDisplayNetworkNameRequired());
  aInInfo->GetIsDisplaySpnRequired(&aOutData.isDisplaySpnRequired());

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo(do_QueryInterface(aInInfo));
  if (gsmIccInfo) {
    gsmIccInfo->GetMsisdn(aOutData.phoneNumber());
  }

  nsCOMPtr<nsICdmaIccInfo> cdmaIccInfo(do_QueryInterface(aInInfo));
  if (cdmaIccInfo) {
    cdmaIccInfo->GetMdn(aOutData.phoneNumber());
    cdmaIccInfo->GetPrlVersion(&aOutData.prlVersion());
  }
}

/*static*/ void IccIPCUtils::GetIccContactDataFromIccContact(
    nsIIccContact* aContact, IccContactData& aOutData) {
  // Id
  nsresult rv = aContact->GetId(aOutData.id());
  NS_ENSURE_SUCCESS_VOID(rv);

  // Names
  nsTArray<nsString> rawStringArray;
  rv = aContact->GetNames(rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (rawStringArray.Length() > 0) {
    aOutData.names().AppendElements(rawStringArray);
  }

  // Numbers
  rv = aContact->GetNumbers(rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (rawStringArray.Length() > 0) {
    aOutData.numbers().AppendElements(rawStringArray);
  }

  // Emails
  rv = aContact->GetEmails(rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (rawStringArray.Length() > 0) {
    aOutData.emails().AppendElements(rawStringArray);
  }
}

}  // namespace icc
}  // namespace dom
}  // namespace mozilla
