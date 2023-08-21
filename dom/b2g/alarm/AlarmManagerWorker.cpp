/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AlarmManagerWorker.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

static mozilla::LazyLogModule sAlarmManagerWorkerLog("AlarmManagerWorker");
#define LOG(...) \
  MOZ_LOG(sAlarmManagerWorkerLog, mozilla::LogLevel::Info, (__VA_ARGS__))

namespace mozilla {
namespace dom {

/*  Call flow of GetAll
 *  1. On Worker Thread:
 *     AlarmManagerWorker::GetAll
 *     new a AlarmGetAllRunnable and dispatch it to Main Thread.
 *
 *  2. On Main Thread:
 *     AlarmGetAllRunnable::Run
 *     new a AlarmGetAllRunnableCallback, pass to AlarmProxy.
 *
 *  3. On Main Thread:
 *     AlarmGetAllRunnableCallback::OnGetAll
 *     Invoked by AlarmProxy, new a AlarmGetAllReturnedRunnable to pass to the
 *     caller on the worker thread in 1.
 *
 *  4. On Worker Thread:
 *     AlarmGetAllReturnedRunnable::WorkerRun
 *     Wrap the return value, and then resolve or reject.
 */

class AlarmGetAllReturnedRunnable final : public WorkerRunnable,
                                          public StructuredCloneHolder {
 public:
  explicit AlarmGetAllReturnedRunnable(
      WorkerPrivate* aWorkerPrivate,
      already_AddRefed<PromiseWorkerProxy>&& aProxy, nsresult aStatus)
      : WorkerRunnable(aWorkerPrivate),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mProxy(std::move(aProxy)),
        mStatus(aStatus) {
    LOG("AlarmGetAllReturnedRunnable constructor. aWorkerPrivate:[%p] "
        "aStatus:[%x]",
        aWorkerPrivate, uint(aStatus));
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    LOG("AlarmGetAllReturnedRunnable::WorkerRun");

    if (NS_WARN_IF(!mProxy)) {
      LOG("!mProxy");
      return false;
    }

    RefPtr<Promise> promise = mProxy->WorkerPromise();

    JS::RootedObject globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      LOG("!globalObject");
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!global)) {
      LOG("!global");
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    JS::RootedValue result(aCx);
    ErrorResult rv;
    Read(global, aCx, &result, rv);
    if (NS_WARN_IF(rv.Failed())) {
      LOG("Read failed. rv:[%x]", rv.ErrorCodeAsInt());
      promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
      mProxy->CleanUp();
      return false;
    }
    JS_WrapValue(aCx, &result);

    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(result);
    } else {
      promise->MaybeReject(result);
    }

    mProxy->CleanUp();
    return true;
  }

 private:
  ~AlarmGetAllReturnedRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
};

class AlarmGetAllRunnableCallback final : public nsIAlarmGetAllCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMGETALLCALLBACK
  explicit AlarmGetAllRunnableCallback(PromiseWorkerProxy* aPromiseWorkerProxy)
      : mPromiseWorkerProxy(aPromiseWorkerProxy) {
    LOG("AlarmGetAllRunnableCallback constructor. aPromiseWorkerProxy:[%p]",
        aPromiseWorkerProxy);
  }

 protected:
  ~AlarmGetAllRunnableCallback() = default;

 private:
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
};

NS_IMPL_ISUPPORTS(AlarmGetAllRunnableCallback, nsIAlarmGetAllCallback)

NS_IMETHODIMP
AlarmGetAllRunnableCallback::OnGetAll(nsresult aStatus, JS::HandleValue aResult,
                                      JSContext* aCx) {
  LOG("OnGetAll aCx:[%p]", aCx);
  if (!mPromiseWorkerProxy) {
    LOG("!mPromiseWorkerProxy");
    return NS_ERROR_ABORT;
  }

  MutexAutoLock lock(mPromiseWorkerProxy->Lock());
  if (mPromiseWorkerProxy->CleanedUp()) {
    LOG("mPromiseWorkerProxy->CleanedUp(), return NS_OK.");
    return NS_OK;
  }

  WorkerPrivate* worker = mPromiseWorkerProxy->GetWorkerPrivate();
  RefPtr<AlarmGetAllReturnedRunnable> r = new AlarmGetAllReturnedRunnable(
      worker, mPromiseWorkerProxy.forget(), aStatus);
  ErrorResult rv;
  r->Write(aCx, aResult, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Write failed. rv:[%x]", rv.ErrorCodeAsInt());
  }

  MOZ_ALWAYS_TRUE(r->Dispatch());

  return NS_OK;
}

class AlarmGetAllRunnable : public Runnable {
 public:
  explicit AlarmGetAllRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                               const nsACString& aUrl)
      : Runnable("dom::AlarmGetAllRunnable"),
        mPromiseWorkerProxy(aPromiseWorkerProxy),
        mUrl(aUrl) {
    LOG("AlarmGetAllRunnable constructor. aPromiseWorkerProxy:[%p] mUrl:[%s]",
        aPromiseWorkerProxy, mUrl.get());
  }

  NS_IMETHOD
  Run() override {
    LOG("AlarmGetAllRunnable::Run");
    RefPtr<nsIAlarmGetAllCallback> callback =
        new AlarmGetAllRunnableCallback(mPromiseWorkerProxy);

    nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
    if (NS_WARN_IF(!alarmProxy)) {
      LOG("!alarmProxy");
      return NS_ERROR_ABORT;
    }

    alarmProxy->GetAll(mUrl, callback);
    return NS_OK;
  }

 private:
  ~AlarmGetAllRunnable() = default;
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  nsCString mUrl;
};

class AlarmAddReturnedRunnable final : public WorkerRunnable,
                                       public StructuredCloneHolder {
 public:
  explicit AlarmAddReturnedRunnable(
      WorkerPrivate* aWorkerPrivate,
      already_AddRefed<PromiseWorkerProxy>&& aProxy, nsresult aStatus)
      : WorkerRunnable(aWorkerPrivate),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mProxy(std::move(aProxy)),
        mStatus(aStatus) {
    LOG("AlarmAddReturnedRunnable constructor. aWorkerPrivate:[%p] "
        "aStatus:[%x]",
        aWorkerPrivate, uint(aStatus));
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    LOG("AlarmAddReturnedRunnable::WorkerRun");
    if (NS_WARN_IF(!mProxy)) {
      LOG("!mProxy");
      return false;
    }

    RefPtr<Promise> promise = mProxy->WorkerPromise();

    JS::RootedObject globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      LOG("!globalObject");
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!global)) {
      LOG("!global");
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    JS::RootedValue result(aCx);
    ErrorResult rv;
    Read(global, aCx, &result, rv);
    if (NS_WARN_IF(rv.Failed())) {
      LOG("Read failed. rv:[%x]", rv.ErrorCodeAsInt());
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      mProxy->CleanUp();
      return false;
    }
    JS_WrapValue(aCx, &result);

    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(result);
    } else {
      promise->MaybeReject(result);
    }

    mProxy->CleanUp();
    return true;
  }

 private:
  ~AlarmAddReturnedRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
};

class AlarmAddRunnableCallback final : public nsIAlarmAddCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALARMADDCALLBACK
  explicit AlarmAddRunnableCallback(PromiseWorkerProxy* aPromiseWorkerProxy)
      : mPromiseWorkerProxy(aPromiseWorkerProxy) {
    LOG("AlarmAddRunnableCallback constructor. aPromiseWorkerProxy:[%p]",
        aPromiseWorkerProxy);
  }

 protected:
  ~AlarmAddRunnableCallback() = default;

 private:
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
};

NS_IMPL_ISUPPORTS(AlarmAddRunnableCallback, nsIAlarmAddCallback)

NS_IMETHODIMP
AlarmAddRunnableCallback::OnAdd(nsresult aStatus, JS::HandleValue aResult,
                                JSContext* aCx) {
  LOG("OnAdd aCx:[%p]", aCx);
  if (!mPromiseWorkerProxy) {
    LOG("!mPromiseWorkerProxy");
    return NS_ERROR_ABORT;
  }

  MutexAutoLock lock(mPromiseWorkerProxy->Lock());
  if (mPromiseWorkerProxy->CleanedUp()) {
    LOG("mPromiseWorkerProxy->CleanedUp(), return NS_OK.");
    return NS_OK;
  }

  WorkerPrivate* worker = mPromiseWorkerProxy->GetWorkerPrivate();
  RefPtr<AlarmAddReturnedRunnable> r = new AlarmAddReturnedRunnable(
      worker, mPromiseWorkerProxy.forget(), aStatus);
  ErrorResult rv;
  r->Write(aCx, aResult, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Write failed. rv:[%x]", rv.ErrorCodeAsInt());
  }
  MOZ_ALWAYS_TRUE(r->Dispatch());

  return NS_OK;
}

class AlarmAddRunnable final : public Runnable, public StructuredCloneHolder {
 public:
  explicit AlarmAddRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                            const nsACString& aUrl)
      : Runnable("dom::AlarmAddRunnable"),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess),
        mPromiseWorkerProxy(aPromiseWorkerProxy),
        mUrl(aUrl) {
    LOG("AlarmAddRunnable constructor."
        "aPromiseWorkerProxy:[%p] mUrl:[%s]",
        aPromiseWorkerProxy, mUrl.get());
  }
  NS_IMETHOD
  Run() override {
    LOG("AlarmAddRunnable::Run");
    AssertIsOnMainThread();

    AutoSafeJSContext cx;
    ErrorResult rv;
    JS::RootedValue options(cx);
    nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(cx);
    if (NS_WARN_IF(!global)) {
      LOG("!global");
      return NS_ERROR_ABORT;
    }
    Read(global, cx, &options, rv);
    if (NS_WARN_IF(rv.Failed())) {
      LOG("Read failed. rv:[%x]", rv.ErrorCodeAsInt());
      return rv.StealNSResult();
    }
    JS_WrapValue(cx, &options);

    RefPtr<nsIAlarmAddCallback> callback =
        new AlarmAddRunnableCallback(mPromiseWorkerProxy);

    nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
    if (NS_WARN_IF(!alarmProxy)) {
      LOG("!alarmProxy");
      return NS_ERROR_ABORT;
    }

    alarmProxy->Add(mUrl, options, callback);
    return NS_OK;
  }

 private:
  ~AlarmAddRunnable() = default;
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  nsCString mUrl;
};

class AlarmRemoveRunnable final : public Runnable {
 public:
  explicit AlarmRemoveRunnable(long aId, const nsACString& aUrl)
      : Runnable("dom::AlarmRemoveRunnable"), mId(aId), mUrl(aUrl) {
    LOG("AlarmRemoveRunnable constructor. mUrl:[%s] mId:[%ld]", mUrl.get(),
        mId);
  }

  NS_IMETHOD
  Run() override {
    LOG("AlarmRemoveRunnable::Run");
    if (NS_WARN_IF(!NS_IsMainThread())) {
      LOG("!NS_IsMainThread()");
      return NS_ERROR_ABORT;
    }

    nsCOMPtr<nsIAlarmProxy> alarmProxy = alarm::CreateAlarmProxy();
    if (NS_WARN_IF(!alarmProxy)) {
      LOG("!alarmProxy");
      return NS_ERROR_ABORT;
    }

    alarmProxy->Remove(mUrl, mId);
    return NS_OK;
  }

 private:
  ~AlarmRemoveRunnable() = default;
  long mId;
  nsCString mUrl;
};

class AlarmInitRunnable final : public WorkerMainThreadRunnable {
 public:
  explicit AlarmInitRunnable()
      : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate(),
                                 "dom::AlarmInitRunnable"_ns) {
    LOG("AlarmInitRunnable constructor.");
  }

  bool MainThreadRun() override {
    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
    mRv = alarm::Initialize(principal, mUrl);
    return true;
  }

  nsCString mUrl;
  nsresult mRv;

 private:
  ~AlarmInitRunnable() = default;
};

nsresult AlarmManagerWorker::Init(nsIGlobalObject* aGlobal) {
  LOG("AlarmManagerWorker::Init");
  if (NS_WARN_IF(NS_IsMainThread())) {
    LOG("Error! AlarmManagerWorker should NOT be on main thread.");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  RefPtr<AlarmInitRunnable> r = new AlarmInitRunnable();

  ErrorResult rv;
  r->Dispatch(Canceling, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Dispatch failed. rv:[%x]", rv.ErrorCodeAsInt());
    return NS_ERROR_DOM_ABORT_ERR;
  }

  mUrl = r->mUrl;
  return r->mRv;
}

void AlarmManagerWorker::GetAll(Promise* aPromise) {
  LOG("AlarmManagerWorker::GetAll mUrl:[%s]", mUrl.get());

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  if (NS_WARN_IF(!worker)) {
    LOG("!worker");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  worker->AssertIsOnWorkerThread();

  RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(worker, aPromise);
  if (NS_WARN_IF(!proxy)) {
    LOG("!proxy");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  RefPtr<AlarmGetAllRunnable> r = new AlarmGetAllRunnable(proxy, mUrl);
  nsresult dispatchNSResult = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(NS_FAILED(dispatchNSResult))) {
    LOG("NS_DispatchToMainThread failed. [%x]", uint(dispatchNSResult));
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }
}

void AlarmManagerWorker::Add(Promise* aPromise, JSContext* aCx,
                             const AlarmOptions& aOptions) {
  LOG("AlarmManagerWorker::Add mUrl:[%s]", mUrl.get());

  JS::RootedValue optionsValue(aCx);
  if (NS_WARN_IF(!ToJSValue(aCx, aOptions, &optionsValue))) {
    LOG("!ToJSValue");
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  if (NS_WARN_IF(!worker)) {
    LOG("!worker");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  worker->AssertIsOnWorkerThread();

  RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(worker, aPromise);
  if (!proxy) {
    LOG("!proxy");
    aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  ErrorResult rv;
  RefPtr<AlarmAddRunnable> r = new AlarmAddRunnable(proxy, mUrl);
  r->Write(aCx, optionsValue, rv);
  if (NS_WARN_IF(rv.Failed())) {
    LOG("Write failed. rv:[%x]", rv.ErrorCodeAsInt());
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsresult dispatchNSResult = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(NS_FAILED(dispatchNSResult))) {
    LOG("NS_DispatchToMainThread failed. [%x]", uint(dispatchNSResult));
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }
}

void AlarmManagerWorker::Remove(long aId) {
  RefPtr<AlarmRemoveRunnable> r = new AlarmRemoveRunnable(aId, mUrl);
  nsresult dispatchNSResult = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(NS_FAILED(dispatchNSResult))) {
    LOG("NS_DispatchToMainThread failed. [%x]", uint(dispatchNSResult));
  }

  return;
}

}  // namespace dom
}  // namespace mozilla
