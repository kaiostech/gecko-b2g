/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileIterable.h"
#include "mozilla/dom/FileIterableBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IterableIterator.h"
#include "nsDeviceStorage.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileIterable, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileIterable)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileIterable)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileIterable)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FileIterable::FileIterable(nsIGlobalObject* aGlobal,
                           DeviceStorageCursorRequest* aRequest)
    : mGlobal(aGlobal),
      mRequest(aRequest),
      mState(EnumerateState::Initialized),
      mPendingContinue(0) {}

FileIterable::~FileIterable() {}

JSObject* FileIterable::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return FileIterable_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* FileIterable::GetParentObject() const { return mGlobal; }

void FileIterable::InitAsyncIterator(itrType* aIterator, ErrorResult& aError) {
  // From the spec of async iterator, an iterable can initialize multiple
  // iterators, and each iterator restarts the iteration from begining.
  // However, FileIterable as a replacement of DOMCursor, can only initialize
  // an iterator once, and iterate results once. This behavior is to match the
  // behavior of DOMCursor, and the original design of
  // DeviceStorage.enumerate().
  if (mIterator) {
    DS_LOG_WARN("This FileIterable has already initialized an iterator.");
    return;
  }
  mIterator = aIterator;
  UniquePtr<IteratorData> data(new IteratorData());
  aIterator->SetData((void*)data.release());
}

void FileIterable::DestroyAsyncIterator(itrType* aIterator) {
  if (mIterator == aIterator) {
    if (mRequest) {
      DS_LOG_WARN("Abort by iterator.");
    }

    auto data = reinterpret_cast<IteratorData*>(aIterator->GetData());
    delete data;
    mIterator = nullptr;
  }
}

already_AddRefed<Promise> FileIterable::GetNextPromise(JSContext* aCx,
                                                       itrType* aIterator,
                                                       ErrorResult& aRv) {
  DS_LOG_DEBUG("next() called by iterator.");
  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (mIterator != aIterator) {
    RejectWithReason(
        promise, NS_LITERAL_STRING_FROM_CSTRING("File iteration has started."));
    return promise.forget();
  }

  auto data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  if (!data) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  data->mPromises.InsertElementAt(0, promise);

  if (mState == EnumerateState::Done) {
    iterator_utils::ResolvePromiseForFinished(aCx, promise, aRv);
    DS_LOG_DEBUG("Iteration complete, resolve next() with undefined.");
  } else if (mState == EnumerateState::Abort) {
    RejectWithReason(promise, mRejectReason);
  } else if (mState < EnumerateState::Prepared) {
    DS_LOG_DEBUG("Add to pending continue queue.");
    mPendingContinue++;
  } else {
    aRv = mRequest->Continue();
  }

  return promise.forget();
}

void FileIterable::FireSuccess(JS::Handle<JS::Value> aResult) {
  mState = EnumerateState::Looping;
  if (!mIterator) {
    DS_LOG_ERROR("File iterator is destroyed or has not initialized.");
    return;
  }

  auto data = reinterpret_cast<IteratorData*>(mIterator->GetData());
  if (!data) {
    DS_LOG_ERROR("no data.");
    return;
  }
  auto promise = data->mPromises.PopLastElement();
  if (!promise) {
    DS_LOG_ERROR("no promise to resolve.");
    return;
  }
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    return;
  }
  JSContext* cx = jsapi.cx();
  ErrorResult rv;
  iterator_utils::ResolvePromiseWithKeyOrValue(cx, promise, aResult, rv);
  DS_LOG_DEBUG("Resolve next() with a file.");
}

void FileIterable::FireError(const nsString& aReason) {
  mState = EnumerateState::Abort;
  mRequest = nullptr;
  mRejectReason = aReason;
  if (!mIterator) {
    DS_LOG_ERROR("File iterator is destroyed or has not initialized.");
    return;
  }

  auto data = reinterpret_cast<IteratorData*>(mIterator->GetData());
  if (!data) {
    DS_LOG_ERROR("no data.");
    return;
  }
  auto promise = data->mPromises.PopLastElement();
  if (!promise) {
    DS_LOG_ERROR("no promise to resolve.");
    return;
  }

  RejectWithReason(promise, aReason);
}

void FileIterable::FireDone() {
  mState = EnumerateState::Done;
  mRequest = nullptr;
  if (!mIterator) {
    DS_LOG_ERROR("File iterator is destroyed or has not initialized.");
    return;
  }
  auto data = reinterpret_cast<IteratorData*>(mIterator->GetData());
  if (!data) {
    DS_LOG_ERROR("no data.");
    return;
  }
  auto promise = data->mPromises.PopLastElement();
  if (!promise) {
    DS_LOG_ERROR("no promise to resolve.");
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    return;
  }
  JSContext* cx = jsapi.cx();
  ErrorResult rv;
  iterator_utils::ResolvePromiseForFinished(cx, promise, rv);
  DS_LOG_DEBUG("Iteration complete, resolve next() with undefined.");
}

void FileIterable::EnumeratePrepared() {
  // EnumeratePrepared is called by DeviceStorageRequestChild when receiving
  // reponse value TEnumerationResponse from parent. Unlike DOMCursor, which
  // returns the first record of results without calling cursor.continue(),
  // FileIterable starts the iteration by calling itor.next() explicitly,
  // so normally we don't expect to do "continue" after setting up enumeration.
  // Another way to iterate FileIterable is using for-await, in this use case,
  // itor.next() is called right after its initialization. However, the
  // initialization is async (complete in DeviceStorageRequestChild) so we need
  // to queue the request of previous itor.next() and process them here.
  DS_LOG_DEBUG("Done preparing file list, has %u pending continue.",
               mPendingContinue);
  if (!mRequest) {
    DS_LOG_ERROR("DeviceStorageCursorRequest is destroyed!");
    return;
  }

  mState = EnumerateState::Prepared;
  for (uint32_t i = 0; i < mPendingContinue; ++i) {
    Unused << mRequest->Continue();
  }
  mPendingContinue = 0;
}

void FileIterable::RejectWithReason(Promise* aPromise,
                                    const nsString& aReason) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);
  Unused << ToJSValue(cx, aReason, &value);

  DS_LOG_DEBUG("Reject iteration with error");
  aPromise->MaybeReject(value);
}

}  // namespace dom
}  // namespace mozilla
