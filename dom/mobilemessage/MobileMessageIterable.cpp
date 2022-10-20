/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPIDOMWindow.h"
#include "MmsMessage.h"
#include "MmsMessageInternal.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/MobileMessageIterable.h"
#include "mozilla/dom/MobileMessageIterableBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileMessageIterable, mParent, mIterator)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileMessageIterable)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileMessageIterable)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileMessageIterable)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileMessageIterable::MobileMessageIterable(
    nsPIDOMWindowInner* aParent, nsICursorContinueCallback* aCallback)
    : mParent(aParent), mContinueCallback(aCallback), mDone(false) {}

// static
already_AddRefed<MobileMessageIterable> MobileMessageIterable::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<MobileMessageIterable> r = new MobileMessageIterable(window, nullptr);
  return r.forget();
}

JSObject* MobileMessageIterable::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return MobileMessageIterable_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* MobileMessageIterable::GetParentObject() const {
  return mParent;
}

already_AddRefed<Promise> MobileMessageIterable::GetNextIterationResult(
    Iterator* aIterator, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return nullptr;
  }

  JSContext* cx = jsapi.cx();

  if (!mIterator) {
    mIterator = aIterator;
  }

  if (mIterator != aIterator) {
    JS::Rooted<JS::Value> value(cx);
    Unused << ToJSValue(
        cx,
        NS_LITERAL_STRING_FROM_CSTRING("Mobilemessage iteration has started."),
        &value);
    promise->MaybeReject(value);
    return promise.forget();
  }

  if (mPendingResults.Length()) {
    LOG_DEBUG("%s mPendingResults.Length: %d", __func__,
              mPendingResults.Length());
    nsCOMPtr<nsISupports> result;
    nsCOMPtr<nsIMobileMessageThread> internalThread =
        do_QueryInterface(mPendingResults.LastElement());
    if (internalThread) {
      MobileMessageThreadInternal* thread =
          static_cast<MobileMessageThreadInternal*>(internalThread.get());
      result = new MobileMessageThread(GetParentObject(), thread);
    }

    if (!result) {
      nsCOMPtr<nsISmsMessage> internalSms =
          do_QueryInterface(mPendingResults.LastElement());
      if (internalSms) {
        SmsMessageInternal* sms =
            static_cast<SmsMessageInternal*>(internalSms.get());
        result = new SmsMessage(GetParentObject(), sms);
      }
    }

    if (!result) {
      nsCOMPtr<nsIMmsMessage> internalMms =
          do_QueryInterface(mPendingResults.LastElement());
      if (internalMms) {
        MmsMessageInternal* mms =
            static_cast<MmsMessageInternal*>(internalMms.get());
        result = new MmsMessage(GetParentObject(), mms);
      }
    }

    JS::Rooted<JS::Value> value(cx);
    Unused << ToJSValue(cx, result, &value);
    promise->MaybeResolve(value);
    mPendingResults.RemoveElementAt(mPendingResults.Length() - 1);
    return promise.forget();
  }

  if (mDone) {
    LOG_DEBUG("Mobilemessage iteration has been done");
    iterator_utils::ResolvePromiseForFinished(promise);
    return promise.forget();
  }

  IteratorData& data = mIterator->Data();
  data.mPromises.InsertElementAt(0, promise);
  mContinueCallback->HandleContinue();
  return promise.forget();
}

void MobileMessageIterable::FireSuccess() {
  if (!mIterator) {
    LOG_ERROR("Mobilemessage iterator is destroyed or has not initialized.");
    return;
  }

  IteratorData& data = mIterator->Data();
  auto promise = data.mPromises.PopLastElement();
  if (!promise) {
    LOG_ERROR("no promise to resolve.");
    return;
  }

  // We're going to pop the last element from mPendingResults, so it must not
  // be empty.
  MOZ_ASSERT(mPendingResults.Length());

  nsCOMPtr<nsISupports> result;
  nsCOMPtr<nsIMobileMessageThread> internalThread =
      do_QueryInterface(mPendingResults.LastElement());
  if (internalThread) {
    MobileMessageThreadInternal* thread =
        static_cast<MobileMessageThreadInternal*>(internalThread.get());
    result = new MobileMessageThread(GetParentObject(), thread);
  }

  if (!result) {
    nsCOMPtr<nsISmsMessage> internalSms =
        do_QueryInterface(mPendingResults.LastElement());
    if (internalSms) {
      SmsMessageInternal* sms =
          static_cast<SmsMessageInternal*>(internalSms.get());
      result = new SmsMessage(GetParentObject(), sms);
    }
  }

  if (!result) {
    nsCOMPtr<nsIMmsMessage> internalMms =
        do_QueryInterface(mPendingResults.LastElement());
    if (internalMms) {
      MmsMessageInternal* mms =
          static_cast<MmsMessageInternal*>(internalMms.get());
      result = new MmsMessage(GetParentObject(), mms);
    }
  }

  MOZ_ASSERT(result);
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);
  Unused << ToJSValue(cx, result, &value);
  promise->MaybeResolve(value);
  mPendingResults.RemoveElementAt(mPendingResults.Length() - 1);
}

void MobileMessageIterable::FireError(const nsAString& aReason) {
  if (!mIterator) {
    LOG_ERROR("File iterator is destroyed or has not initialized.");
    return;
  }

  IteratorData& data = mIterator->Data();
  auto promise = data.mPromises.PopLastElement();
  if (!promise) {
    LOG_ERROR("no promise to resolve.");
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);
  Unused << ToJSValue(cx, aReason, &value);
  promise->MaybeReject(value);
}

void MobileMessageIterable::FireDone() {
  mDone = true;
  if (!mIterator) {
    LOG_ERROR("File iterator is destroyed or has not initialized.");
    return;
  }

  IteratorData& data = mIterator->Data();
  auto promise = data.mPromises.PopLastElement();
  if (!promise) {
    LOG_ERROR("no promise to resolve.");
    return;
  }

  iterator_utils::ResolvePromiseForFinished(promise);
  LOG_DEBUG("Iteration complete, resolve next() with undefined.");
}

}  // namespace dom
}  // namespace mozilla
