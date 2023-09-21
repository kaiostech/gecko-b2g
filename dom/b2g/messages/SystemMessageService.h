/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SystemMessageService_h
#define mozilla_dom_SystemMessageService_h

#include "mozilla/ClearOnShutdown.h"
#include "nsISystemMessageService.h"
#include "nsClassHashtable.h"

class nsISystemMessageListener;
class nsITimer;
class nsIURI;

namespace mozilla {
namespace dom {

class ContentParent;
class WakeLock;

class SystemMessageService final : public nsISystemMessageService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMMESSAGESERVICE

  static already_AddRefed<SystemMessageService> GetInstance();

  void DoSubscribe(const nsAString& aMessageName, const nsACString& aOrigin,
                   const nsACString& aScope, const nsACString& aOriginSuffix,
                   nsISystemMessageListener* aListener);

 private:
  SystemMessageService();
  ~SystemMessageService();

  bool HasPermission(const nsAString& aMessageName, const nsACString& aOrigin);

  void AcquireWakeLock();
  void ReleaseWakeLock();
  static void WakeLockTimerCallback(nsITimer* aTimer, void* aClosure);

  // For printing debug information.
  void DebugPrintSubscribersTable();

  struct SubscriberInfo {
    SubscriberInfo(const nsACString& aScope, const nsACString& aOriginSuffix,
                   nsIURI* aURI)
        : mScope(aScope), mOriginSuffix(aOriginSuffix), mURI(aURI) {}
    nsCString mScope;
    nsCString mOriginSuffix;
    nsCOMPtr<nsIURI> mURI;
  };
  // key: origin, value: [{scope, originSuffix, mURI}]
  typedef nsClassHashtable<nsCStringHashKey,
                           nsTArray<UniquePtr<SubscriberInfo>>>
      SubscriberTable;

  // key: message_name, value: {key: origin, value: [{scope, originSuffix,
  // mURI}]}
  nsClassHashtable<nsStringHashKey, SubscriberTable> mSubscribers;
  RefPtr<WakeLock> mMessageWakeLock;
  nsCOMPtr<nsITimer> mWakeLockTimer;
};

}  // namespace dom
}  // namespace mozilla
#endif  // mozilla_dom_SystemMessageService_h
