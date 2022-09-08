/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AlarmManager.h"
#include "mozilla/dom/AlarmManagerWorker.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Logging.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsNetUtil.h"

static mozilla::LazyLogModule sAlarmManagerLog("AlarmManager");
#define LOG(...) \
  MOZ_LOG(sAlarmManagerLog, mozilla::LogLevel::Info, (__VA_ARGS__))

namespace mozilla {
namespace dom {

/*  Call flow of GetAll
 *  1. On Main Thread:
 *     AlarmManagerMain::GetAll
 *     new a AlarmGetAllCallback, pass to AlarmProxy.
 *
 *  2. On Main Thread:
 *     AlarmGetAllCallback::OnGetAll
 *     Invoked by AlarmProxy. Wrap the return value, and then resolve or
 *     reject.
 */

class AlarmGetAllCallback final : public nsIAlarmGetAllCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMGETALLCALLBACK
  explicit AlarmGetAllCallback(Promise* aPromise);

 protected:
  ~AlarmGetAllCallback() = default;

 private:
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(AlarmGetAllCallback, nsIAlarmGetAllCallback)

AlarmGetAllCallback::AlarmGetAllCallback(Promise* aPromise)
    : mPromise(aPromise) {
  LOG("AlarmGetAllCallback constructor.");
}

NS_IMETHODIMP
AlarmGetAllCallback::OnGetAll(nsresult aStatus, JS::HandleValue aResult,
                              JSContext* aCx) {
  LOG("OnGetAll. aCx:[%p]", aCx);

  if (NS_WARN_IF(!mPromise)) {
    LOG("!mPromise");
    return NS_ERROR_ABORT;
  }
  if (NS_SUCCEEDED(aStatus)) {
    mPromise->MaybeResolveWithClone(aCx, aResult);
  } else {
    mPromise->MaybeRejectWithClone(aCx, aResult);
  }
  return NS_OK;
}

class AlarmAddCallback final : public nsIAlarmAddCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMADDCALLBACK
  explicit AlarmAddCallback(Promise* aPromise);

 protected:
  ~AlarmAddCallback() = default;

 private:
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(AlarmAddCallback, nsIAlarmAddCallback)

AlarmAddCallback::AlarmAddCallback(Promise* aPromise) : mPromise(aPromise) {
  LOG("AlarmAddCallback constructor.");
}

NS_IMETHODIMP
AlarmAddCallback::OnAdd(nsresult aStatus, JS::HandleValue aResult,
                        JSContext* aCx) {
  LOG("OnAdd aCx:[%p]", aCx);

  if (NS_WARN_IF(!mPromise)) {
    LOG("!mPromise");
    return NS_ERROR_ABORT;
  }
  if (NS_SUCCEEDED(aStatus)) {
    mPromise->MaybeResolve(aResult);
  } else {
    mPromise->MaybeReject(aResult);
  }
  return NS_OK;
}

nsresult AlarmManagerMain::Init(nsIGlobalObject* aGlobal) {
  LOG("AlarmManagerMain::Init");
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("Error! AlarmManagerMain should be on main thread.");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  if (NS_WARN_IF(!aGlobal)) {
    LOG("!aGlobal");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  nsCOMPtr<nsIPrincipal> principal = aGlobal->PrincipalOrNull();
  if (NS_WARN_IF(!principal)) {
    LOG("!principal");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  return alarm::Initialize(principal, mUrl);
}

void AlarmManagerMain::GetAll(Promise* aPromise) {
  LOG("AlarmManagerMain::GetAll mUrl:[%s]", mUrl.get());
  if (NS_WARN_IF(!aPromise)) {
    LOG("!aPromise");
    return;
  }

  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  nsCOMPtr<nsIAlarmGetAllCallback> callback = new AlarmGetAllCallback(aPromise);
  alarmProxy->GetAll(mUrl, callback);
}

void AlarmManagerMain::Add(Promise* aPromise, JSContext* aCx,
                           const AlarmOptions& aOptions) {
  LOG("AlarmManagerMain::Add mUrl:[%s]", mUrl.get());
  if (NS_WARN_IF(!aPromise)) {
    LOG("!aPromise");
    return;
  }

  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // We're about to pass the dictionary to a JS-implemented component, so
  // rehydrate it in a system scode so that security wrappers don't get in the
  // way. See bug 1161748 comment 16.
  bool ok;
  JS::RootedValue optionsValue(aCx);
  {
    JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());
    ok = ToJSValue(aCx, aOptions, &optionsValue);
    if (!ok) {
      aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
      return;
    }
  }
  ok = JS_WrapValue(aCx, &optionsValue);
  if (!ok) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  nsCOMPtr<nsIAlarmAddCallback> callback = new AlarmAddCallback(aPromise);
  alarmProxy->Add(mUrl, optionsValue, callback);
}

void AlarmManagerMain::Remove(long aId) {
  nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
  if (NS_WARN_IF(!alarmProxy)) {
    LOG("!alarmProxy");
    return;
  }

  alarmProxy->Remove(mUrl, aId);
}

// static
already_AddRefed<AlarmManager> AlarmManager::Create(nsIGlobalObject* aGlobal,
                                                    nsresult& aRv) {
  if (NS_WARN_IF(!aGlobal)) {
    LOG("!aGlobal");
    aRv = NS_ERROR_DOM_ABORT_ERR;
    return nullptr;
  }

  RefPtr<AlarmManager> alarmManager = new AlarmManager(aGlobal);

  aRv = alarmManager->Init();
  if (NS_WARN_IF(NS_FAILED(aRv))) {
    LOG("Init failed. rv:[%x]", uint(aRv));
    return nullptr;
  }

  return alarmManager.forget();
}

AlarmManager::AlarmManager(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  LOG("AlarmManager constructor.");
}

nsresult AlarmManager::Init() {
  LOG("AlarmManager Init. NS_IsMainThread:[%s]",
      NS_IsMainThread() ? "true" : "false");

  if (NS_IsMainThread()) {
    mImpl = new AlarmManagerMain();
  } else {
    mImpl = new AlarmManagerWorker();
  }

  return mImpl->Init(mGlobal);
}

already_AddRefed<Promise> AlarmManager::GetAll() {
  LOG("AlarmManager::GetAll NS_IsMainThread:[%s]",
      NS_IsMainThread() ? "true" : "false");

  if (NS_WARN_IF(!mGlobal)) {
    LOG("!mGlobal");
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Promise::Create failed. [%x]", rv.ErrorCodeAsInt());
    return nullptr;
  }

  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
  }

  mImpl->GetAll(promise);

  return promise.forget();
}

already_AddRefed<Promise> AlarmManager::Add(JSContext* aCx,
                                            const AlarmOptions& aOptions) {
  LOG("AlarmManager::Add NS_IsMainThread:[%s]",
      NS_IsMainThread() ? "true" : "false");

  if (NS_WARN_IF(!mGlobal)) {
    LOG("!mGlobal");
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Promise::Create failed. [%x]", rv.ErrorCodeAsInt());
    return nullptr;
  }

  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
  }

  mImpl->Add(promise, aCx, aOptions);

  return promise.forget();
}

void AlarmManager::Remove(long aId) {
  LOG("AlarmManager::Remove aId:[%ld] NS_IsMainThread:[%s]", aId,
      NS_IsMainThread() ? "true" : "false");
  if (NS_WARN_IF(!mImpl)) {
    LOG("!mImpl");
    return;
  }

  return mImpl->Remove(aId);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AlarmManager, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AlarmManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AlarmManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AlarmManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* AlarmManager::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return AlarmManager_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsIAlarmProxy> alarm::CreateAlarmProxy() {
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("CreateAlarmProxy can only be called on main threads.");
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIAlarmProxy> alarmProxy =
      do_CreateInstance("@mozilla.org/dom/alarm/proxy;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!alarmProxy)) {
    LOG("do_CreateInstance of nsIAlarmProxy failed. rv:[%x]", uint(rv));
    return nullptr;
  }

  return alarmProxy.forget();
}

nsresult alarm::Initialize(const nsCOMPtr<nsIPrincipal>& aPrincipal,
                           nsCString& aUrl) {
  if (NS_WARN_IF(!NS_IsMainThread())) {
    LOG("Initialize can only be called on main threads.");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  if (NS_WARN_IF(!aPrincipal)) {
    LOG("!aPrincipal");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  if (aPrincipal->IsSystemPrincipal()) {
    // System app might call AlarmManager from the system startup chrome url.
    // To ensure proper system message receiver, replace the url with system
    // app's origin.
    // Other modules or services in Gecko should send messages to AlarmService
    // directly, not through AlarmManager or AlarmProxy.

#if !defined(API_DAEMON_PORT) || API_DAEMON_PORT == 80
    aUrl = "http://system.localhost"_ns;
#else
    aUrl = nsLiteralCString(
        "http://system.localhost:" MOZ_STRINGIFY(API_DAEMON_PORT));
#endif
    LOG("system principal");
    return NS_OK;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  if (NS_WARN_IF(!permissionManager)) {
    LOG("!permissionManager");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  nsresult rv;
  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  rv = permissionManager->TestPermissionFromPrincipal(aPrincipal, "alarms"_ns,
                                                      &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG("TestPermissionFromPrincipal failed. rv[%x]", uint(rv));
    return rv;
  }

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  rv = aPrincipal->GetAsciiOrigin(aUrl);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG("GetAsciiOrigin failed. [%x]", uint(rv));
    return rv;
  }

  LOG("aUrl:[%s]", aUrl.get());

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
