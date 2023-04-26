/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SubsidyLockCallback.h"

#include "mozilla/dom/SubsidyLockBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace subsidylock {

NS_IMPL_ISUPPORTS(SubsidyLockCallback, nsISubsidyLockCallback)

SubsidyLockCallback::SubsidyLockCallback(nsPIDOMWindowInner* aWindow,
                                         DOMRequest* aRequest)
    : mWindow(aWindow), mRequest(aRequest) {}

nsresult SubsidyLockCallback::NotifySuccess(JS::Handle<JS::Value> aResult) {
  nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireSuccessAsync(mRequest, aResult);
}

// nsISubsidyLockCallback

NS_IMETHODIMP
SubsidyLockCallback::NotifyGetSubsidyLockStatusSuccess(uint32_t aCount,
                                                       uint32_t* aTypes) {
  nsTArray<uint32_t> result;
  for (uint32_t i = 0; i < aCount; i++) {
    uint32_t type = aTypes[i];
    result.AppendElement(type);
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, result, &jsResult)) {
    jsapi.ClearException();
    return NS_ERROR_UNEXPECTED;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
SubsidyLockCallback::NotifyError(const nsAString& aError) {
  nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireErrorAsync(mRequest, aError);
}

NS_IMETHODIMP
SubsidyLockCallback::NotifySuccess() {
  return NotifySuccess(JS::UndefinedHandleValue);
}

NS_IMETHODIMP
SubsidyLockCallback::NotifyUnlockSubsidyLockError(const nsAString& aError,
                                                  int32_t aRetryCount) {
  SubsidyCardLockError result;
  result.mError = aError;
  result.mRetryCount = aRetryCount;

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);
  if (!ToJSValue(cx, result, &jsResult)) {
    jsapi.ClearException();
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  return NotifySuccess(jsResult);
}

}  // namespace subsidylock
}  // namespace dom
}  // namespace mozilla
