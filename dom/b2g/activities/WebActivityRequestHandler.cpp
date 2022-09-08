/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebActivityRequestHandler.h"
#include "mozilla/dom/WebActivityRequestHandlerBinding.h"
#include "mozilla/dom/WebActivityBinding.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/HoldDropJSObjects.h"
#include "js/JSON.h"
#include "nsIActivityRequestHandlerProxy.h"

namespace mozilla {
namespace dom {

namespace {
class ActivityRequestHandlerProxyRunnable : public Runnable {
 public:
  ActivityRequestHandlerProxyRunnable(const nsAString& aId)
      : Runnable("dom::ActivityRequestHandlerProxyRunnable"), mId(aId) {}

  NS_IMETHOD
  Run() override {
    nsresult rv;
    nsCOMPtr<nsIActivityRequestHandlerProxy> proxy =
        do_CreateInstance("@mozilla.org/dom/activities/handlerproxy;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    ExecActivityProxy(proxy);

    return NS_OK;
  }

 protected:
  virtual void ExecActivityProxy(nsIActivityRequestHandlerProxy* aProxy) = 0;
  virtual ~ActivityRequestHandlerProxyRunnable() = default;

  nsString mId;
};

class NotifyReadyRunnable final : public ActivityRequestHandlerProxyRunnable {
 public:
  explicit NotifyReadyRunnable(const nsAString& aId)
      : ActivityRequestHandlerProxyRunnable(aId) {}

 protected:
  void ExecActivityProxy(nsIActivityRequestHandlerProxy* aProxy) {
    aProxy->NotifyActivityReady(mId);
  }

 private:
  ~NotifyReadyRunnable() = default;
};

class PostResultRunnable final : public ActivityRequestHandlerProxyRunnable,
                                 public StructuredCloneHolder {
 public:
  explicit PostResultRunnable(const nsAString& aId)
      : ActivityRequestHandlerProxyRunnable(aId),
        StructuredCloneHolder(CloningSupported, TransferringSupported,
                              StructuredCloneScope::SameProcess) {}

 protected:
  void ExecActivityProxy(nsIActivityRequestHandlerProxy* aProxy) {
    AutoSafeJSContext cx;
    JS::RootedObject globalObject(cx, JS::CurrentGlobalOrNull(cx));
    if (NS_WARN_IF(!globalObject)) {
      return;
    }
    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!global)) {
      return;
    }

    ErrorResult rv;
    JS::RootedValue result(cx);
    Read(global, cx, &result, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return;
    }
    JS_WrapValue(cx, &result);
    aProxy->PostActivityResult(mId, result);
  }

 private:
  ~PostResultRunnable() = default;
};

class PostErrorRunnable final : public ActivityRequestHandlerProxyRunnable {
 public:
  PostErrorRunnable(const nsAString& aId, const nsAString& aError)
      : ActivityRequestHandlerProxyRunnable(aId), mError(aError) {}

 protected:
  void ExecActivityProxy(nsIActivityRequestHandlerProxy* aProxy) {
    aProxy->PostActivityError(mId, mError);
  }

 private:
  ~PostErrorRunnable() = default;
  nsString mError;
};

}  // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_CLASS(WebActivityRequestHandler)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebActivityRequestHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WebActivityRequestHandler)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMessage)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebActivityRequestHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mMessage.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebActivityRequestHandler, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebActivityRequestHandler, Release)

WebActivityRequestHandler::WebActivityRequestHandler(const JS::Value& aMessage,
                                                     const nsAString& aId,
                                                     bool aReturnValue)
    : mActivityId(aId), mReturnValue(aReturnValue), mMessage(aMessage) {
  MOZ_COUNT_CTOR(WebActivityRequestHandler);
  mozilla::HoldJSObjects(this);
}

WebActivityRequestHandler::~WebActivityRequestHandler() {
  MOZ_COUNT_DTOR(WebActivityRequestHandler);
  mMessage.setUndefined();
  DropJSObjects(this);
}

JSObject* WebActivityRequestHandler::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WebActivityRequestHandler_Binding::Wrap(aCx, this, aGivenProto);
}

/*static*/
already_AddRefed<WebActivityRequestHandler> WebActivityRequestHandler::Create(
    nsIGlobalObject* aGlobal, const JS::Value& aMessage) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aGlobal))) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  if (NS_WARN_IF(!aMessage.isObject())) {
    return nullptr;
  }

  JS::RootedObject msgObj(cx, &aMessage.toObject());
  JS::RootedValue idVal(cx);
  if (NS_WARN_IF(!JS_GetProperty(cx, msgObj, "id", &idVal) ||
                 !idVal.isString())) {
    return nullptr;
  }
  JS::RootedString idStr(cx, idVal.toString());
  nsAutoString id;
  if (NS_WARN_IF(!AssignJSString(cx, id, idStr))) {
    return nullptr;
  }

  JS::RootedValue rvVal(cx);
  JS_GetProperty(cx, msgObj, "returnValue", &rvVal);
  bool returnValue = rvVal.isBoolean() ? rvVal.toBoolean() : false;

  RefPtr<NotifyReadyRunnable> r = new NotifyReadyRunnable(id);
  NS_DispatchToMainThread(r);

  RefPtr<WebActivityRequestHandler> handler =
      new WebActivityRequestHandler(aMessage, id, returnValue);

  return handler.forget();
}

void WebActivityRequestHandler::PostResult(JSContext* aCx,
                                           JS::Handle<JS::Value> aResult,
                                           ErrorResult& aRv) {
  if (mReturnValue) {
    RefPtr<PostResultRunnable> r = new PostResultRunnable(mActivityId);
    r->Write(aCx, aResult, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    NS_DispatchToMainThread(r);
  } else {
    // postResult() can't be called when 'returnValue': 'true' isn't declared
    // in manifest.webapp.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void WebActivityRequestHandler::PostError(const nsAString& aError) {
  RefPtr<PostErrorRunnable> r = new PostErrorRunnable(mActivityId, aError);
  NS_DispatchToMainThread(r);
}

void WebActivityRequestHandler::GetSource(JSContext* aCx,
                                          WebActivityOptions& aResult,
                                          ErrorResult& aRv) {
  JS::RootedObject msgObj(aCx, &mMessage.toObject());
  JS::RootedValue payloadVal(aCx);
  if (!JS_GetProperty(aCx, msgObj, "payload", &payloadVal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!aResult.Init(aCx, payloadVal)) {
    aRv.NoteJSContextException(aCx);
  }
}

}  // namespace dom
}  // namespace mozilla
