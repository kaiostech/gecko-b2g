/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileMessageIterable_h
#define mozilla_dom_MobileMessageIterable_h

#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

#ifdef MOBILEMESSAGE_DEBUG
#  include <android/log.h>
#  define LOG_DEBUG(args...) \
    __android_log_print(ANDROID_LOG_DEBUG, "MobileMessageIterable", ##args);
#  define LOG_INFO(args...) \
    __android_log_print(ANDROID_LOG_INFO, "MobileMessageIterable", ##args);
#  define LOG_WARN(args...) \
    __android_log_print(ANDROID_LOG_WARN, "MobileMessageIterable", ##args);
#  define LOG_ERROR(args...) \
    __android_log_print(ANDROID_LOG_ERROR, "MobileMessageIterable", ##args);
#else
#  define LOG_DEBUG(...)
#  define LOG_INFO(...)
#  define LOG_WARN(...)
#  define LOG_ERROR(...)
#endif

class nsPIDOMWindowInner;
class nsICursorContinueCallback;

namespace mozilla {
class ErrorResult;
namespace dom {

class MmsMessage;
class OwningSmsMessageOrMmsMessage;
class SmsMessage;

namespace mobilemessage {
class MobileMessageCursorCallback;
}  // namespace mobilemessage

class MobileMessageIterable final : public nsISupports, public nsWrapperCache {
  friend class mobilemessage::MobileMessageCursorCallback;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MobileMessageIterable)
  explicit MobileMessageIterable(nsPIDOMWindowInner* aParent,
                                 nsICursorContinueCallback* aCallback);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<MobileMessageIterable> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  struct IteratorData {
    nsTArray<RefPtr<Promise>> mPromises;
  };
  using Iterator = AsyncIterableIterator<MobileMessageIterable>;
  void InitAsyncIteratorData(IteratorData& aData, Iterator::IteratorType aType,
                             ErrorResult& aError) {}
  already_AddRefed<Promise> GetNextIterationResult(Iterator* aIterator,
                                                   ErrorResult& aRv);

  void FireSuccess();
  void FireError(const nsAString& aReason);
  void FireDone();

 private:
  virtual ~MobileMessageIterable() = default;
  RefPtr<Iterator> mIterator;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsCOMPtr<nsICursorContinueCallback> mContinueCallback;
  nsTArray<nsCOMPtr<nsISupports>> mPendingResults;
  bool mDone;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MobileMessageIterable_h
