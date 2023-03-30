/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SystemMessageService.h"
#include "mozilla/dom/SystemMessageServiceChild.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerCloneData.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/StaticPtr.h"
#include "nsISystemMessageListener.h"
#include "nsCharSeparatedTokenizer.h"

#undef LOG
mozilla::LazyLogModule gSystemMessageServiceLog("SystemMessageService");
#define LOG(...) \
  MOZ_LOG(gSystemMessageServiceLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace dom {

const uint32_t kWakeLockHoldTime = 5000;

static StaticRefPtr<SystemMessageService> sSystemMessageService;
static nsTHashMap<nsStringHashKey, nsCString> sSystemMessagePermissionsTable;

namespace {

/**
 * About sSystemMessagePermissionsTable.
 * Key: Name of system message.
 * Data: Name of permission. (Please lookup from the PermissionsTable.sys.mjs)
 *       For example, "alarm" messages require "alarms" permission, then do
 *       sSystemMessagePermissionsTable.InsertOrUpdate(u"alarm"_ns,
 * "alarms"_ns);
 *
 *       If your system message do not need to specify any permission, please
 *       set EmptyCString().
 *
 *       If your system message requires multiple permissions, please use ","
 *       to separate them, i.e. "camera,cellbroadcast".
 *
 *       If your system message requires access permission, such as
 *       "settings": [read, write], please expand them manually, i.e.
 *       "settings:read,settings:write".
 */
void BuildPermissionsTable() {
  /**
   * For efficient lookup and systematic indexing, please help to arrange the
   * key names (system message names) in alphabetical order.
   **/
  sSystemMessagePermissionsTable.InsertOrUpdate(u"activity"_ns, EmptyCString());
  sSystemMessagePermissionsTable.InsertOrUpdate(u"alarm"_ns, "alarms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"bluetooth-dialer-command"_ns,
                                                "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"bluetooth-map-request"_ns,
                                                "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"bluetooth-opp-receiving-file-confirmation"_ns,
      "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"bluetooth-opp-transfer-complete"_ns, "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"bluetooth-opp-transfer-start"_ns, "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"bluetooth-opp-update-progress"_ns, "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"bluetooth-pairing-aborted"_ns,
                                                "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"bluetooth-pairing-request"_ns,
                                                "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"bluetooth-pbap-request"_ns,
                                                "bluetooth-privileged"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"cellbroadcast-received"_ns,
                                                "cellbroadcast"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"data-sms-received"_ns,
                                                "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"icc-stkcommand"_ns, "settings:read,settings:write"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"media-button"_ns,
                                                EmptyCString());
  sSystemMessagePermissionsTable.InsertOrUpdate(u"networkstats-alarm"_ns, "networkstats-manage"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"sms-delivery-error"_ns,
                                                "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"sms-delivery-success"_ns,
                                                "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"sms-failed"_ns, "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"sms-received"_ns, "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"sms-sent"_ns, "sms"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"system-time-change"_ns,
                                                "system-time:read"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"telephony-call-ended"_ns,
                                                "telephony"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"telephony-hac-mode-changed"_ns, "telephony"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"telephony-new-call"_ns,
                                                "telephony"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(
      u"telephony-tty-mode-changed"_ns, "telephony"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"ussd-received"_ns,
                                                "mobileconnection"_ns);
  sSystemMessagePermissionsTable.InsertOrUpdate(u"wappush-received"_ns,
                                                "wappush"_ns);
  /**
   * Note: Please do NOT directly add new entries at the bottom of this table,
   * try to insert them alphabetically.
   **/
}

}  // anonymous namespace

NS_IMPL_ISUPPORTS(SystemMessageService, nsISystemMessageService)

SystemMessageService::SystemMessageService() {}

SystemMessageService::~SystemMessageService() {}

/*static*/
already_AddRefed<SystemMessageService> SystemMessageService::GetInstance() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  if (!sSystemMessageService) {
    sSystemMessageService = new SystemMessageService();
    BuildPermissionsTable();
    ClearOnShutdown(&sSystemMessageService);
  }

  RefPtr<SystemMessageService> service = sSystemMessageService;
  return service.forget();
}

// nsISystemMessageService methods
NS_IMETHODIMP
SystemMessageService::Subscribe(nsIPrincipal* aPrincipal,
                                const nsAString& aMessageName,
                                const nsACString& aScope) {
  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsAutoCString origin;
  rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString originSuffix;
  rv = aPrincipal->GetOriginSuffix(originSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DoSubscribe(aMessageName, origin, aScope, originSuffix, nullptr);

  return NS_OK;
}

NS_IMETHODIMP
SystemMessageService::Unsubscribe(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsAutoCString origin;
  rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
    auto& table = iter.Data();
    table->Remove(origin);
  }

  LOG("Unsubscribe: subscriber origin=%s", origin.get());
  DebugPrintSubscribersTable();

  return NS_OK;
}

NS_IMETHODIMP
SystemMessageService::SendMessage(const nsAString& aMessageName,
                                  JS::HandleValue aMessageData,
                                  const nsACString& aOrigin, JSContext* aCx) {
  LOG("SendMessage: name=%s", NS_LossyConvertUTF16toASCII(aMessageName).get());
  LOG("             to=%s", nsCString(aOrigin).get());

  SubscriberTable* table = mSubscribers.Get(aMessageName);
  if (!table) {
    LOG(" %s has no subscribers.",
        NS_LossyConvertUTF16toASCII(aMessageName).get());
    DebugPrintSubscribersTable();
    return NS_OK;
  }
  SubscriberInfo* info = table->Get(aOrigin);
  if (!info) {
    LOG(" %s did not subscribe %s.", (nsCString(aOrigin)).get(),
        NS_LossyConvertUTF16toASCII(aMessageName).get());
    DebugPrintSubscribersTable();
    return NS_OK;
  }

  ErrorResult rv;
  RefPtr<ServiceWorkerCloneData> messageData = new ServiceWorkerCloneData();
  messageData->Write(aCx, aMessageData, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return NS_OK;
  }

  // ServiceWorkerParentInterceptEnabled() is default to true in 73.0b2., which
  // whether the ServiceWorker is registered on parent processes or content
  // processes, its ServiceWorker will be spawned and executed on a content
  // process (This behavior is more secured as well).
  // As a result, we don't need to consider whether the SystemMessage.subscribe
  // is called on a content process or a parent process, and since this
  // SystemMessageService::SendMessage() is for used on chrome process only,
  // we can use ServiceWorkerManager to send SystemMessageEvent directly.
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    return NS_ERROR_FAILURE;
  }

  // Unlike the old implementation in gecko48, where it acquires wake lock per
  // requests, we are using a global wake lock here. File Bug 78954 to track
  // if we want to improve on that.
  AcquireWakeLock();
  NS_NewTimerWithFuncCallback(getter_AddRefs(mWakeLockTimer),
                              WakeLockTimerCallback, this, kWakeLockHoldTime,
                              nsITimer::TYPE_ONE_SHOT,
                              "SystemMessageService::SendMessage");

  LOG("Sending message %s to %s",
      NS_LossyConvertUTF16toASCII(aMessageName).get(), (info->mScope).get());
  return swm->SendSystemMessageEvent(info->mOriginSuffix, info->mScope,
                                     aMessageName, std::move(messageData));
}

NS_IMETHODIMP
SystemMessageService::BroadcastMessage(const nsAString& aMessageName,
                                       JS::HandleValue aMessageData,
                                       JSContext* aCx) {
  LOG("BroadcastMessage: name=%s",
      NS_LossyConvertUTF16toASCII(aMessageName).get());
  SubscriberTable* table = mSubscribers.Get(aMessageName);
  if (!table) {
    LOG(" %s has no subscribers.",
        NS_LossyConvertUTF16toASCII(aMessageName).get());
    DebugPrintSubscribersTable();
    return NS_OK;
  }

  ErrorResult rv;
  RefPtr<ServiceWorkerCloneData> messageData = new ServiceWorkerCloneData();
  messageData->Write(aCx, aMessageData, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return NS_OK;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    return NS_ERROR_FAILURE;
  }

  AcquireWakeLock();
  NS_NewTimerWithFuncCallback(getter_AddRefs(mWakeLockTimer),
                              WakeLockTimerCallback, this, kWakeLockHoldTime,
                              nsITimer::TYPE_ONE_SHOT,
                              "SystemMessageService::BroadcastMessage");

  for (auto iter = table->Iter(); !iter.Done(); iter.Next()) {
    auto& info = iter.Data();

    LOG("Sending message %s to %s",
        NS_LossyConvertUTF16toASCII(aMessageName).get(), (info->mScope).get());

    Unused << swm->SendSystemMessageEvent(info->mOriginSuffix, info->mScope,
                                          aMessageName, std::move(messageData));
  }

  return NS_OK;
}

void SystemMessageService::DoSubscribe(const nsAString& aMessageName,
                                       const nsACString& aOrigin,
                                       const nsACString& aScope,
                                       const nsACString& aOriginSuffix,
                                       nsISystemMessageListener* aListener) {
  if (!HasPermission(aMessageName, aOrigin)) {
    if (aListener) {
      aListener->OnSubscribe(NS_ERROR_DOM_SECURITY_ERR);
    }
    LOG("Permission denied.");
    return;
  }

  SubscriberTable* table = mSubscribers.GetOrInsertNew(aMessageName);
  UniquePtr<SubscriberInfo> info(new SubscriberInfo(aScope, aOriginSuffix));
  table->InsertOrUpdate(aOrigin, std::move(info));

  if (aListener) {
    aListener->OnSubscribe(NS_OK);
  }

  LOG("DoSubscribe: message name=%s",
      NS_LossyConvertUTF16toASCII(aMessageName).get());
  LOG("             subscriber origin=%s", nsCString(aOrigin).get());

  return;
}

bool SystemMessageService::HasPermission(const nsAString& aMessageName,
                                         const nsACString& aOrigin) {
  LOG("Checking permission: message name=%s",
      NS_LossyConvertUTF16toASCII(aMessageName).get());
  LOG("             subscriber origin=%s", nsCString(aOrigin).get());

  nsAutoCString permNames;
  if (!sSystemMessagePermissionsTable.Get(aMessageName, &permNames)) {
    LOG("Did not define in system message permissions table.");
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
      mozilla::services::GetPermissionManager();
  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aOrigin);

  if (principal && permMgr && !permNames.IsEmpty()) {
    nsCCharSeparatedTokenizer tokenizer(permNames, ',');
    while (tokenizer.hasMoreTokens()) {
      nsAutoCString permName(tokenizer.nextToken());
      if (permName.IsEmpty()) {
        continue;
      }
      uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
      permMgr->TestExactPermissionFromPrincipal(principal, permName, &perm);
      LOG("Permission of %s: %d", permName.get(), perm);
      if (perm != nsIPermissionManager::ALLOW_ACTION) {
        return false;
      }
    }
  }

  return true;
}

void SystemMessageService::AcquireWakeLock() {
  if (!mMessageWakeLock) {
    RefPtr<power::PowerManagerService> pmService =
        power::PowerManagerService::GetInstance();
    if (NS_WARN_IF(!pmService)) {
      return;
    }
    ErrorResult rv;
    mMessageWakeLock = pmService->NewWakeLock(u"cpu"_ns, nullptr, rv);
  }
}

void SystemMessageService::ReleaseWakeLock() {
  if (mMessageWakeLock) {
    ErrorResult rv;
    mMessageWakeLock->Unlock(rv);
    rv.SuppressException();
    mMessageWakeLock = nullptr;
  }
}

/*static*/
void SystemMessageService::WakeLockTimerCallback(nsITimer* aTimer,
                                                 void* aClosure) {
  auto smService = static_cast<SystemMessageService*>(aClosure);
  smService->ReleaseWakeLock();
  smService->mWakeLockTimer = nullptr;
}

void SystemMessageService::DebugPrintSubscribersTable() {
  LOG("Subscribed system messages count: %d\n", mSubscribers.Count());
  for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
    nsString key = nsString(iter.Key());
    LOG("key(message name): %s\n", NS_LossyConvertUTF16toASCII(key).get());
    auto& table = iter.Data();
    for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
      nsCString key = nsCString(iter2.Key());
      LOG(" subscriber table key(origin): %s\n", key.get());
      auto& entry = iter2.Data();
      LOG("                  scope: %s\n", (entry->mScope).get());
      LOG("                  originSuffix: %s\n", (entry->mOriginSuffix).get());
    }
  }
}

}  // namespace dom
}  // namespace mozilla
