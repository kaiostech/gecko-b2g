/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceAsyncIterableSingle_h
#define mozilla_dom_TestInterfaceAsyncIterableSingle_h

#include "IterableIterator.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceAsyncIterableSingle final : public nsISupports,
                                               public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestInterfaceAsyncIterableSingle)

  explicit TestInterfaceAsyncIterableSingle(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableSingle> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  using itrType = AsyncIterableIterator<TestInterfaceAsyncIterableSingle>;
  void InitAsyncIterator(itrType* aIterator);
  void DestroyAsyncIterator(itrType* aIterator);
  already_AddRefed<Promise> GetNextPromise(JSContext* aCx, itrType* aIterator,
                                           ErrorResult& aRv);

 private:
  virtual ~TestInterfaceAsyncIterableSingle() = default;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  struct IteratorData {
    IteratorData(int32_t aIndex) : mIndex(aIndex) {}
    ~IteratorData() { mPromise = nullptr; }
    WeakPtr<Promise> mPromise;
    uint32_t mIndex;
  };
  nsTArray<WeakPtr<itrType>> mIterators;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceAsyncIterableSingle_h
