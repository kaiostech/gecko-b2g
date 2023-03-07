/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WebActivity.h"
#include "mozilla/dom/WebActivityWorker.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIDocShell.h"
#include "nsIGlobalObject.h"
#include "nsServiceManagerUtils.h"

#undef LOG
mozilla::LazyLogModule gWebActivityLog("WebActivity");
#define LOG(...) \
  MOZ_LOG(gWebActivityLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace dom {

static nsCOMPtr<nsIActivityProxy> sActivityProxy;

NS_IMPL_ISUPPORTS(ActivityStartCallback, nsIActivityStartCallback)

ActivityStartCallback::ActivityStartCallback(Promise* aPromise)
    : mPromise(aPromise) {}

ActivityStartCallback::~ActivityStartCallback() {}

NS_IMETHODIMP
ActivityStartCallback::OnStart(nsresult aStatus, JS::HandleValue aResult,
                               JSContext* aCx) {
  LOG("Resolve ActivityStartCallback with success: %d", NS_SUCCEEDED(aStatus));
  if (NS_SUCCEEDED(aStatus)) {
    mPromise->MaybeResolve(aResult);
  } else {
    mPromise->MaybeReject(aResult);
  }

  return NS_OK;
}

WebActivity::WebActivity(nsIGlobalObject* aGlobal,
                         already_AddRefed<WebActivityImpl> aImpl)
    : mGlobal(aGlobal), mImpl(aImpl), mIsStarted(false) {
  mImpl->AddOuter(this);
}

WebActivity::~WebActivity() { mImpl->RemoveOuter(this); }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebActivity, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WebActivity)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebActivity)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebActivity)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* WebActivity::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return WebActivity_Binding::Wrap(aCx, this, aGivenProto);
}

/*static*/
already_AddRefed<nsIActivityProxy> WebActivity::GetOrCreateActivityProxy() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sActivityProxy) {
    sActivityProxy = do_CreateInstance("@mozilla.org/dom/activities/proxy;1");
    MOZ_ASSERT(sActivityProxy);
  }

  nsCOMPtr<nsIActivityProxy> proxy = sActivityProxy;
  return proxy.forget();
}

/*static*/
already_AddRefed<WebActivity> WebActivity::Constructor(
    const GlobalObject& aOwner, const nsAString& aName,
    JS::Handle<JS::Value> aData, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aOwner.GetAsSupports());

  RefPtr<WebActivityImpl> impl;
  if (NS_IsMainThread()) {
    impl = new WebActivityMain();
  } else {
    impl = new WebActivityWorker();
  }

  RefPtr<WebActivity> activity = new WebActivity(global, impl.forget());

  nsresult rv;
  rv = activity->PermissionCheck();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  RootedDictionary<WebActivityOptions> options(aOwner.Context());
  options.mName = aName;
  options.mData = aData;

  rv = activity->Initialize(aOwner, options);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  return activity.forget();
}

nsresult WebActivity::PermissionCheck() { return mImpl->PermissionCheck(); }

nsresult WebActivity::Initialize(const GlobalObject& aOwner,
                                 const WebActivityOptions& aOptions) {
  return mImpl->Initialize(aOwner, aOptions);
}

already_AddRefed<Promise> WebActivity::Start(ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (mIsStarted) {
    LOG("Activity %s already started.", NS_LossyConvertUTF16toASCII(mId).get());
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }
  mIsStarted = true;

  mImpl->Start(promise);

  return promise.forget();
}

void WebActivity::Cancel() { mImpl->Cancel(); }

WebActivityMain::WebActivityMain() : mOuter(nullptr) {}

WebActivityMain::~WebActivityMain() { MOZ_DIAGNOSTIC_ASSERT(!mOuter); }

nsresult WebActivityMain::PermissionCheck() {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  auto* window = mOuter->mGlobal->AsInnerWindow();
  if (!window) {
    return NS_ERROR_UNEXPECTED;
  }

  Document* document = window->GetExtantDoc();

  bool isActive = window->GetDocShell()->GetBrowsingContext()->IsActive();

  if (!isActive && !nsContentUtils::IsChromeDoc(document)) {
    nsCOMPtr<nsIConsoleService> console(
        do_GetService("@mozilla.org/consoleservice;1"));
    NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

    nsString message =
        u"Can only start activity from user input or chrome code"_ns;
    console->LogStringMessage(message.get());

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult WebActivityMain::Initialize(const GlobalObject& aOwner,
                                     const WebActivityOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  // We're about to pass the dictionary to a JS-implemented component, so
  // rehydrate it in a system scode so that security wrappers don't get in the
  // way. See bug 1161748 comment 16.
  bool ok;
  JS::RootedValue optionsValue(aOwner.Context());
  {
    JSAutoRealm ar(aOwner.Context(), xpc::PrivilegedJunkScope());
    ok = ToJSValue(aOwner.Context(), aOptions, &optionsValue);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  }
  ok = JS_WrapValue(aOwner.Context(), &optionsValue);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = aOwner.GetSubjectPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);
  nsAutoCString origin;
  if (principal->IsSystemPrincipal()) {
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(aOwner.GetAsSupports());
    nsCOMPtr<nsIURI> uri;
    uri = window->GetDocumentURI();
    // Use domain(host) as identifier for system principal callers.
    uri->GetHost(origin);
  } else {
    principal->GetOrigin(origin);
  }

  nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
  proxy->Create(aOwner.GetAsSupports(), optionsValue, origin, mOuter->mId);
  LOG("New Activity created: %s",
      NS_LossyConvertUTF16toASCII(mOuter->mId).get());
  return NS_OK;
}

void WebActivityMain::Start(Promise* aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  RefPtr<nsIActivityStartCallback> callback =
      new ActivityStartCallback(aPromise);
  nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
  proxy->Start(mOuter->mId, callback);
}

void WebActivityMain::Cancel() {
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  nsCOMPtr<nsIActivityProxy> proxy = WebActivity::GetOrCreateActivityProxy();
  proxy->Cancel(mOuter->mId);
}

void WebActivityMain::AddOuter(WebActivity* aOuter) {
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aOuter;
}

void WebActivityMain::RemoveOuter(WebActivity* aOuter) {
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(mOuter == aOuter);
  mOuter = nullptr;
}

}  // namespace dom
}  // namespace mozilla
