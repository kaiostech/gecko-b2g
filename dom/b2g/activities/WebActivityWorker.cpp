/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebActivityWorker.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"

#undef LOG
mozilla::LazyLogModule gWebActivityWorkerLog("WebActivityWorker");
#define LOG(...) \
  MOZ_LOG(gWebActivityWorkerLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace dom {

namespace {

class ActivityInitRunnable : public WorkerMainThreadRunnable,
                             public StructuredCloneHolder {
 public:
  ActivityInitRunnable(WorkerPrivate* aWorkerPrivate, WebActivity* aWebActivity)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "WebActivity :: Initialize"_ns),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mWebActivity(aWebActivity) {}

  bool MainThreadRun() override {
    AutoSafeJSContext cx;
    ErrorResult rv;
    JS::RootedValue options(cx);
    Read(mWebActivity->GetParentObject(), cx, &options, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return false;
    }
    JS_WrapValue(cx, &options);

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
    if (NS_WARN_IF(!principal)) {
      return false;
    }
    nsAutoCString origin;
    principal->GetWebExposedOriginSerialization(origin);

    nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
    proxy->Create(nullptr, options, origin, mWebActivity->mId);
    LOG("New Activity created: %s",
        NS_LossyConvertUTF16toASCII(mWebActivity->mId).get());
    return true;
  }

 private:
  ~ActivityInitRunnable() = default;
  RefPtr<WebActivity> mWebActivity;
};

class ActivityCheckPermissionRunnable : public WorkerMainThreadRunnable {
 public:
  ActivityCheckPermissionRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "WebActivity :: CheckPermission"_ns),
        mAllowed(false) {}

  bool MainThreadRun() override {
    nsCOMPtr<nsIPermissionManager> permissionManager =
        services::GetPermissionManager();
    if (NS_WARN_IF(!permissionManager)) {
      return false;
    }

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
    if (NS_WARN_IF(!principal)) {
      return false;
    }

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    nsresult rv = permissionManager->TestPermissionFromPrincipal(
        principal, "worker-activity"_ns, &permission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    mAllowed = permission == nsIPermissionManager::ALLOW_ACTION;

    return true;
  }

  bool mAllowed;

 private:
  ~ActivityCheckPermissionRunnable() = default;
};

class ActivityStartRunnable : public Runnable {
 public:
  ActivityStartRunnable(nsString aId, PromiseWorkerProxy* aPromiseWorkerProxy)
      : Runnable("dom::ActivityStartRunnable"),
        mId(aId),
        mPromiseWorkerProxy(aPromiseWorkerProxy) {}

  NS_IMETHOD
  Run() override {
    RefPtr<nsIActivityStartCallback> callback =
        new ActivityStartWorkerCallback(mPromiseWorkerProxy);
    nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
    proxy->Start(mId, callback);
    return NS_OK;
  }

 private:
  ~ActivityStartRunnable() = default;
  nsString mId;
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
};

class OnStartWorkerRunnable final : public WorkerRunnable,
                                    public StructuredCloneHolder {
 public:
  OnStartWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                        already_AddRefed<PromiseWorkerProxy>&& aProxy,
                        nsresult aStatus)
      : WorkerRunnable(aWorkerPrivate),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mProxy(std::move(aProxy)),
        mStatus(aStatus) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(mProxy);

    RefPtr<Promise> promise = mProxy->WorkerPromise();

    JS::RootedObject globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!global)) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    JS::RootedValue result(aCx);
    ErrorResult rv;
    Read(global, aCx, &result, rv);
    if (NS_WARN_IF(rv.Failed())) {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    JS_WrapValue(aCx, &result);

    LOG("Resolve OnStartWorkerRunnable with success: %d",
        NS_SUCCEEDED(mStatus));
    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(result);
    } else {
      promise->MaybeReject(result);
    }

    mProxy->CleanUp();
    return true;
  }

 private:
  ~OnStartWorkerRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
};

class ActivityCancelRunnable : public Runnable {
 public:
  explicit ActivityCancelRunnable(nsString aId)
      : Runnable("dom::ActivityCancelRunnable"), mId(aId) {}

  NS_IMETHOD
  Run() override {
    nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
    proxy->Cancel(mId);
    return NS_OK;
  }

 private:
  ~ActivityCancelRunnable() = default;
  nsString mId;
};

}  // namespace

NS_IMPL_ISUPPORTS(ActivityStartWorkerCallback, nsIActivityStartCallback)

ActivityStartWorkerCallback::ActivityStartWorkerCallback(
    PromiseWorkerProxy* aPromiseWorkerProxy)
    : mPromiseWorkerProxy(aPromiseWorkerProxy) {}

ActivityStartWorkerCallback::~ActivityStartWorkerCallback() {}

NS_IMETHODIMP
ActivityStartWorkerCallback::OnStart(nsresult aStatus, JS::HandleValue aResult,
                                     JSContext* aCx) {
  LOG("Resolve ActivityStartWorkerCallback");
  MutexAutoLock lock(mPromiseWorkerProxy->Lock());
  if (mPromiseWorkerProxy->CleanedUp()) {
    return NS_OK;
  }
  WorkerPrivate* worker = mPromiseWorkerProxy->GetWorkerPrivate();
  RefPtr<OnStartWorkerRunnable> r =
      new OnStartWorkerRunnable(worker, mPromiseWorkerProxy.forget(), aStatus);
  ErrorResult rv;
  r->Write(aCx, aResult, rv);
  MOZ_ALWAYS_TRUE(r->Dispatch());

  return NS_OK;
}

WebActivityWorker::WebActivityWorker() : mOuter(nullptr) {}

WebActivityWorker::~WebActivityWorker() { MOZ_DIAGNOSTIC_ASSERT(!mOuter); }

nsresult WebActivityWorker::PermissionCheck() {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  ErrorResult rv;
  RefPtr<ActivityCheckPermissionRunnable> runnable =
      new ActivityCheckPermissionRunnable(worker);
  runnable->Dispatch(Canceling, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return NS_ERROR_UNEXPECTED;
  }

  return runnable->mAllowed ? NS_OK : NS_ERROR_DOM_SECURITY_ERR;
}

nsresult WebActivityWorker::Initialize(const GlobalObject& aOwner,
                                       const WebActivityOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  JS::RootedValue optionsValue(aOwner.Context());
  if (NS_WARN_IF(!ToJSValue(aOwner.Context(), aOptions, &optionsValue))) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  RefPtr<ActivityInitRunnable> initRunnable =
      new ActivityInitRunnable(worker, mOuter);
  initRunnable->Write(aOwner.Context(), optionsValue, rv);
  initRunnable->Dispatch(Canceling, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void WebActivityWorker::Start(Promise* aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(worker, aPromise);
  if (!proxy) {
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  RefPtr<ActivityStartRunnable> r =
      new ActivityStartRunnable(mOuter->mId, proxy);
  NS_DispatchToMainThread(r);
}

void WebActivityWorker::Cancel() {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  RefPtr<ActivityCancelRunnable> r = new ActivityCancelRunnable(mOuter->mId);
  NS_DispatchToMainThread(r);
}

void WebActivityWorker::AddOuter(WebActivity* aOuter) {
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aOuter;
}

void WebActivityWorker::RemoveOuter(WebActivity* aOuter) {
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(mOuter == aOuter);
  mOuter = nullptr;
}

}  // namespace dom
}  // namespace mozilla
