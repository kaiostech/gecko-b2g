/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputMethodHandler.h"
#include "mozilla/dom/InputMethodService.h"
#include "mozilla/dom/InputMethodServiceChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/IMELog.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(InputMethodHandler, nsIEditableSupportListener)

/*static*/
already_AddRefed<InputMethodHandler> InputMethodHandler::Create(
    Promise* aPromise, InputMethodServiceChild* aServiceChild) {
  IME_LOGD("--InputMethodHandler::Create");
  RefPtr<InputMethodHandler> handler =
      new InputMethodHandler(aPromise, aServiceChild);

  return handler.forget();
}

/*static*/
already_AddRefed<InputMethodHandler> InputMethodHandler::Create(
    InputMethodServiceChild* aServiceChild) {
  IME_LOGD("--InputMethodHandler::Create without promise");
  RefPtr<InputMethodHandler> handler = new InputMethodHandler(aServiceChild);

  return handler.forget();
}

InputMethodHandler::InputMethodHandler(Promise* aPromise,
                                       InputMethodServiceChild* aServiceChild)
    : mPromise(aPromise), mServiceChild(aServiceChild) {}

InputMethodHandler::InputMethodHandler(InputMethodServiceChild* aServiceChild)
    : mServiceChild(aServiceChild) {}

// nsIEditableSupportListener methods.
NS_IMETHODIMP
InputMethodHandler::OnSetComposition(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetComposition");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnEndComposition(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnEndComposition");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnKeydown(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnKeydown");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnKeyup(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnKeyup");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSendKey(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSendKey");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnDeleteBackward(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnDeleteBackward");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnDeleteBackward: true");
      mPromise->MaybeResolve(true);
    } else {
      IME_LOGD("--InputMethodHandler::OnDeleteBackward: false");
      mPromise->MaybeReject(aStatus);
    }

    mPromise = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetSelectedOption(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetSelectedOption");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetSelectedOptions(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetSelectedOptions");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnRemoveFocus(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnRemoveFocus");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnGetSelectionRange(uint64_t aId, nsresult aStatus,
                                        int32_t aStart, int32_t aEnd) {
  IME_LOGD("--InputMethodHandler::OnGetSelectionRange");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnGetSelectionRange: %lu %lu", aStart,
               aEnd);
      nsTArray<int32_t> range;
      range.AppendElement(aStart);
      range.AppendElement(aEnd);
      mPromise->MaybeResolve(range);
    } else {
      IME_LOGD("--InputMethodHandler::OnGetSelectionRange failed");
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnGetText(uint64_t aId, nsresult aStatus,
                              const nsAString& aText) {
  IME_LOGD("--InputMethodHandler::OnGetText");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      IME_LOGD("--InputMethodHandler::OnGetText:[%s]",
               NS_ConvertUTF16toUTF8(aText).get());
      nsString text(aText);
      mPromise->MaybeResolve(text);
    } else {
      IME_LOGD("--InputMethodHandler::OnGetText failed");
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnSetValue(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnSetValue");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnClearAll(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnClearAll");
  return NS_OK;
}

NS_IMETHODIMP
InputMethodHandler::OnReplaceSurroundingText(uint64_t aId, nsresult aStatus) {
  IME_LOGD("--InputMethodHandler::OnReplaceSurroundingText");
  if (mPromise) {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mPromise->MaybeReject(aStatus);
    }
    mPromise = nullptr;
  }
  return NS_OK;
}

nsresult InputMethodHandler::SetComposition(uint64_t aId,
                                            const nsAString& aText,
                                            int32_t aOffset, int32_t aLength) {
  nsString text(aText);
  // TODO use a pure interface, and make it point to either the remote version
  // or the local version at Listener's creation.
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::SetComposition content process");
    // Call from content process.
    SendRequest(aId, SetCompositionRequest(aId, text, aOffset, aLength));
  } else {
    IME_LOGD("--InputMethodHandler::SetComposition in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SetComposition(aId, this, aText, aOffset, aLength);
  }
  return NS_OK;
}

nsresult InputMethodHandler::EndComposition(uint64_t aId,
                                            const nsAString& aText) {
  nsString text(aText);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::EndComposition content process");
    // Call from content process.
    SendRequest(aId, EndCompositionRequest(aId, text));
  } else {
    IME_LOGD("--InputMethodHandler::EndComposition in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->EndComposition(aId, this, aText);
  }
  return NS_OK;
}

nsresult InputMethodHandler::Keydown(uint64_t aId, const nsAString& aKey) {
  nsString key(aKey);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::Keydown content process");
    // Call from content process.
    SendRequest(aId, KeydownRequest(aId, key));
  } else {
    IME_LOGD("--InputMethodHandler::Keydown in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->Keydown(aId, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::Keyup(uint64_t aId, const nsAString& aKey) {
  nsString key(aKey);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::Keyup content process");
    // Call from content process.
    SendRequest(aId, KeyupRequest(aId, key));
  } else {
    IME_LOGD("--InputMethodHandler::Keyup in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->Keyup(aId, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::SendKey(uint64_t aId, const nsAString& aKey) {
  nsString key(aKey);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::SendKey content process");
    // Call from content process.
    SendRequest(aId, SendKeyRequest(aId, key));
  } else {
    IME_LOGD("--InputMethodHandler::SendKey in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SendKey(aId, this, aKey);
  }
  return NS_OK;
}

nsresult InputMethodHandler::DeleteBackward(uint64_t aId) {
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::DeleteBackward content process");
    // Call from content process.
    SendRequest(aId, DeleteBackwardRequest(aId));
  } else {
    IME_LOGD("--InputMethodHandler::DeleteBackward in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->DeleteBackward(aId, this);
  }
  return NS_OK;
}

void InputMethodHandler::RemoveFocus(uint64_t aId) {
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::RemoveFocus content process");
    // Call from content process.
    CommonRequest request(aId, u"RemoveFocus"_ns);
    SendRequest(aId, request);
  } else {
    IME_LOGD("--InputMethodHandler::RemoveFocus in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->RemoveFocus(aId, this);
  }
}

nsresult InputMethodHandler::GetSelectionRange(uint64_t aId) {
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::GetSelectionRange content process");
    // Call from content process.
    SendRequest(aId, GetSelectionRangeRequest(aId));
  } else {
    IME_LOGD("--InputMethodHandler::GetSelectionRange in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->GetSelectionRange(aId, this);
  }
  return NS_OK;
}

nsresult InputMethodHandler::GetText(uint64_t aId, int32_t aOffset,
                                     int32_t aLength) {
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::GetText content process");
    // Call from content process.
    GetTextRequest request(aId, aOffset, aLength);
    SendRequest(aId, request);
  } else {
    IME_LOGD("--InputMethodHandler::GetText in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->GetText(aId, this, aOffset, aLength);
  }
  return NS_OK;
}

void InputMethodHandler::SetValue(uint64_t aId, const nsAString& aValue) {
  nsString value(aValue);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::SetValue content process");
    // Call from content process.
    SendRequest(aId, SetValueRequest(aId, value));
  } else {
    IME_LOGD("--InputMethodHandler::SetValue in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->SetValue(aId, this, aValue);
  }
}

void InputMethodHandler::ClearAll(uint64_t aId) {
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::ClearAll content process");
    // Call from content process.
    CommonRequest request(aId, u"ClearAll"_ns);
    SendRequest(aId, request);
  } else {
    IME_LOGD("--InputMethodHandler::ClearAll in-process");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->ClearAll(aId, this);
  }
}

nsresult InputMethodHandler::ReplaceSurroundingText(uint64_t aId,
                                                    const nsAString& aText,
                                                    int32_t aOffset,
                                                    int32_t aLength) {
  nsString text(aText);
  if (mServiceChild) {
    IME_LOGD("--InputMethodHandler::ReplaceSurroundingText content process");
    ReplaceSurroundingTextRequest request(aId, text, aOffset, aLength);
    SendRequest(aId, request);
  } else {
    IME_LOGD("--InputMethodHandler::ReplaceSurroundingText in-process");
    RefPtr<InputMethodService> service = InputMethodService::GetInstance();
    MOZ_ASSERT(service);
    service->ReplaceSurroundingText(aId, this, aText, aOffset, aLength);
  }
  return NS_OK;
}

void InputMethodHandler::SendRequest(uint64_t aId,
                                     const InputMethodRequest& aRequest) {
  mServiceChild->SetEditableSupportListener(aId, this);
  mServiceChild->SendRequest(aRequest);
}

}  // namespace dom
}  // namespace mozilla
