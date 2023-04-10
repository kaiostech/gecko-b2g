/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputMethod.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/InputMethodHandler.h"
#include "mozilla/dom/InputMethodServiceChild.h"
#include "nsIDocShell.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/IMELog.h"

using namespace mozilla::widget;

namespace mozilla {
namespace dom {

namespace {

static uint64_t sId = 0;

}  // namespace

InputMethod::InputMethod(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  MOZ_ASSERT(aGlobal);
  Init();
}

void InputMethod::Init() {
  if (sId == 0) {
    sId = uint64_t(getpid()) << 32;
  }
  ContentChild* cc = ContentChild::GetSingleton();
  if (cc && mServiceChild == nullptr) {
    mServiceChild = new InputMethodServiceChild();
    cc->SendPInputMethodServiceConstructor(mServiceChild);
  }
}

already_AddRefed<Promise> InputMethod::SetComposition(
    const nsAString& aText, const Optional<int32_t>& aOffset,
    const Optional<int32_t>& aLength) {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::SetComposition");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);

  int32_t offset = aOffset.WasPassed() ? aOffset.Value() : 0;
  int32_t length = aLength.WasPassed() ? aLength.Value() : 0;

  nsresult result = handler->SetComposition(++sId, aText, offset, length);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }
  return promise.forget();
}

already_AddRefed<Promise> InputMethod::EndComposition(
    const Optional<nsAString>& aText) {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::EndComposition");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->EndComposition(
      ++sId, aText.WasPassed() ? aText.Value() : VoidString());
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

already_AddRefed<Promise> InputMethod::Keydown(const nsAString& aKey) {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::Keydown");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->Keydown(++sId, aKey);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

already_AddRefed<Promise> InputMethod::Keyup(const nsAString& aKey) {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::Keyup");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->Keyup(++sId, aKey);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

already_AddRefed<Promise> InputMethod::SendKey(const nsAString& aKey) {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::SendKey");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->SendKey(++sId, aKey);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

already_AddRefed<Promise> InputMethod::DeleteBackward() {
  RefPtr<Promise> promise;
  ErrorResult rv;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);

  IME_LOGD("-- InputMethod::DeleteBackward");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->DeleteBackward(++sId);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

void InputMethod::RemoveFocus() {
  IME_LOGD("-- InputMethod::RemoveFocus");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(mServiceChild);
  handler->RemoveFocus(++sId);
}

already_AddRefed<Promise> InputMethod::GetSelectionRange() {
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);
  IME_LOGD("-- InputMethod::GetSelectionRange");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  nsresult result = handler->GetSelectionRange(++sId);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }
  return promise.forget();
}

already_AddRefed<Promise> InputMethod::GetText(
    const Optional<int32_t>& aOffset, const Optional<int32_t>& aLength) {
  ErrorResult rv;
  RefPtr<Promise> promise;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);
  IME_LOGD("-- InputMethod::GetText");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  int32_t offset = aOffset.WasPassed() ? aOffset.Value() : 0;
  int32_t length = aLength.WasPassed() ? aLength.Value() : -1;
  nsresult result = handler->GetText(++sId, offset, length);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

void InputMethod::SetValue(const nsAString& aValue) {
  IME_LOGD("-- InputMethod::SetValue:[%s]",
           NS_ConvertUTF16toUTF8(aValue).get());

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(mServiceChild);
  handler->SetValue(++sId, aValue);
}

void InputMethod::ClearAll() {
  IME_LOGD("-- InputMethod::ClearAll");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(mServiceChild);
  handler->ClearAll(++sId);
}

already_AddRefed<Promise> InputMethod::ReplaceSurroundingText(
    const nsAString& aText, const Optional<int32_t>& aOffset,
    const Optional<int32_t>& aLength) {
  ErrorResult rv;
  RefPtr<Promise> promise;
  promise = Promise::Create(mGlobal, rv);
  ENSURE_SUCCESS(rv, nullptr);
  IME_LOGD("-- InputMethod::ReplaceSurroundingText");

  RefPtr<InputMethodHandler> handler =
      InputMethodHandler::Create(promise, mServiceChild);
  int32_t offset = aOffset.WasPassed() ? aOffset.Value() : 0;
  int32_t length = aLength.WasPassed() ? aLength.Value() : 0;
  nsresult result =
      handler->ReplaceSurroundingText(++sId, aText, offset, length);
  if (NS_FAILED(result)) {
    promise->MaybeReject(result);
  }

  return promise.forget();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(InputMethod, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(InputMethod)
NS_IMPL_CYCLE_COLLECTING_RELEASE(InputMethod)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InputMethod)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* InputMethod::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return InputMethod_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
