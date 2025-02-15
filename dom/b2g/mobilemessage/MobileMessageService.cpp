/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessageInternal.h"
#include "MmsMessageInternal.h"
#include "MobileMessageThreadInternal.h"
#include "MobileMessageService.h"
#include "DeletedMessageInfo.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(MobileMessageService, nsIMobileMessageService)

NS_IMETHODIMP
MobileMessageService::CreateSmsMessage(
    int32_t aId, uint64_t aThreadId, const nsAString& aIccId,
    const nsAString& aDelivery, const nsAString& aDeliveryStatus,
    const nsAString& aSender, const nsAString& aReceiver,
    const nsAString& aBody, const nsAString& aMessageClass, uint64_t aTimestamp,
    uint64_t aSentTimestamp, uint64_t aDeliveryTimestamp, bool aRead,
    JSContext* aCx, nsISmsMessage** aMessage) {
  return SmsMessageInternal::Create(aId, aThreadId, aIccId, aDelivery,
                                    aDeliveryStatus, aSender, aReceiver, aBody,
                                    aMessageClass, aTimestamp, aSentTimestamp,
                                    aDeliveryTimestamp, aRead, aCx, aMessage);
}

NS_IMETHODIMP
MobileMessageService::CreateMmsMessage(
    int32_t aId, uint64_t aThreadId, const nsAString& aIccId,
    const nsAString& aDelivery, JS::Handle<JS::Value> aDeliveryInfo,
    const nsAString& aSender, JS::Handle<JS::Value> aReceivers,
    uint64_t aTimestamp, uint64_t aSentTimestamp, bool aRead,
    const nsAString& aSubject, const nsAString& aSmil,
    JS::Handle<JS::Value> aAttachments, uint64_t aExpiryDate,
    bool aReadReportRequested, bool aIsGroup, JSContext* aCx,
    nsIMmsMessage** aMessage) {
  return MmsMessageInternal::Create(
      aId, aThreadId, aIccId, aDelivery, aDeliveryInfo, aSender, aReceivers,
      aTimestamp, aSentTimestamp, aRead, aSubject, aSmil, aAttachments,
      aExpiryDate, aReadReportRequested, aIsGroup, aCx, aMessage);
}

NS_IMETHODIMP
MobileMessageService::CreateThread(
    uint64_t aId, JS::Handle<JS::Value> aParticipants, uint64_t aTimestamp,
    const nsAString& aLastMessageSubject, const nsAString& aBody,
    uint64_t aUnreadCount, const nsAString& aLastMessageType, bool aIsGroup,
    const nsAString& aLastMessageAttachmentStatus, JSContext* aCx,
    nsIMobileMessageThread** aThread) {
  return MobileMessageThreadInternal::Create(
      aId, aParticipants, aTimestamp, aLastMessageSubject, aBody, aUnreadCount,
      aLastMessageType, aIsGroup, aLastMessageAttachmentStatus, aCx, aThread);
}

NS_IMETHODIMP
MobileMessageService::CreateDeletedMessageInfo(
    int32_t* aMessageIds, uint32_t aMsgCount, uint64_t* aThreadIds,
    uint32_t aThreadCount, nsIDeletedMessageInfo** aDeletedInfo) {
  return DeletedMessageInfo::Create(aMessageIds, aMsgCount, aThreadIds,
                                    aThreadCount, aDeletedInfo);
}

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla
