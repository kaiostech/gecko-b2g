/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputMethodService.h"
#include "mozilla/dom/InputMethodServiceChild.h"
#include "nsIEditableSupport.h"
#include "nsIInputContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/StaticPtr.h"
#include "js/JSON.h"
#include "mozilla/dom/IMELog.h"
#include "mozilla/Services.h"
#include "mozilla/dom/KeyboardAppProxy.h"
namespace mozilla {
namespace dom {

LogModule* GetIMELog() {
  static LazyLogModule sIMELog("IME");
  return sIMELog;
}

static StaticRefPtr<InputMethodService> sInputMethodService;

NS_IMPL_ISUPPORTS(InputMethodService, nsISupports)

/*static*/
already_AddRefed<InputMethodService> InputMethodService::GetInstance() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  if (!sInputMethodService) {
    sInputMethodService = new InputMethodService();
    ClearOnShutdown(&sInputMethodService);
  }

  RefPtr<InputMethodService> service = sInputMethodService;
  return service.forget();
}

// interfaces of nsIEditableSupport
NS_IMETHODIMP InputMethodService::SetComposition(
    uint64_t aId, nsIEditableSupportListener* aListener, const nsAString& aText,
    int32_t aOffset, int32_t aLength) {
  IME_LOGD("InputMethodService::SetComposition");
  if (mEditableSupport) {
    mEditableSupport->SetComposition(aId, aListener, aText, aOffset, aLength);
  } else if (aListener) {
    aListener->OnSetComposition(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::EndComposition(
    uint64_t aId, nsIEditableSupportListener* aListener,
    const nsAString& aText) {
  IME_LOGD("InputMethodService::EndComposition. mEditableSupport:[%p]",
           mEditableSupport.get());
  if (mEditableSupport) {
    mEditableSupport->EndComposition(aId, aListener, aText);
  } else if (aListener) {
    aListener->OnEndComposition(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::Keydown(uint64_t aId,
                                          nsIEditableSupportListener* aListener,
                                          const nsAString& aKey) {
  IME_LOGD("InputMethodService::Keydown");
  if (mEditableSupport) {
    mEditableSupport->Keydown(aId, aListener, aKey);
  } else if (aListener) {
    aListener->OnKeydown(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::Keyup(uint64_t aId,
                                        nsIEditableSupportListener* aListener,
                                        const nsAString& aKey) {
  IME_LOGD("InputMethodService::Keyup");
  if (mEditableSupport) {
    mEditableSupport->Keyup(aId, aListener, aKey);
  } else if (aListener) {
    aListener->OnKeyup(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::SendKey(uint64_t aId,
                                          nsIEditableSupportListener* aListener,
                                          const nsAString& aKey) {
  IME_LOGD("InputMethodService::SendKey");
  if (mEditableSupport) {
    mEditableSupport->SendKey(aId, aListener, aKey);
  } else if (aListener) {
    aListener->OnSendKey(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::DeleteBackward(
    uint64_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodService::DeleteBackward");
  if (mEditableSupport) {
    mEditableSupport->DeleteBackward(aId, aListener);
  } else if (aListener) {
    aListener->OnDeleteBackward(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::SetSelectedOption(
    uint64_t aId, nsIEditableSupportListener* aListener, int32_t aOptionIndex) {
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::SetSelectedOptions(
    uint64_t aId, nsIEditableSupportListener* aListener,
    const nsTArray<int32_t>& aOptionIndexes) {
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::RemoveFocus(
    uint64_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodService::RemoveFocus");
  if (mEditableSupport) {
    mEditableSupport->RemoveFocus(aId, aListener);
  } else if (aListener) {
    aListener->OnRemoveFocus(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::GetSelectionRange(
    uint64_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodService::GetSelectionRange");
  if (mEditableSupport) {
    mEditableSupport->GetSelectionRange(aId, aListener);
  } else if (aListener) {
    aListener->OnGetSelectionRange(aId, NS_ERROR_ABORT, 0, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::GetText(uint64_t aId,
                                          nsIEditableSupportListener* aListener,
                                          int32_t aOffset, int32_t aLength) {
  IME_LOGD("InputMethodService::GetText");
  if (mEditableSupport) {
    mEditableSupport->GetText(aId, aListener, aOffset, aLength);
  } else if (aListener) {
    aListener->OnGetText(aId, NS_ERROR_ABORT, EmptyString());
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::SetValue(
    uint64_t aId, nsIEditableSupportListener* aListener,
    const nsAString& aValue) {
  IME_LOGD("InputMethodService::SetValue:[%s]",
           NS_ConvertUTF16toUTF8(aValue).get());
  if (mEditableSupport) {
    mEditableSupport->SetValue(aId, aListener, aValue);
  } else if (aListener) {
    aListener->OnSetValue(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::ClearAll(
    uint64_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodService::ClearAll");
  if (mEditableSupport) {
    mEditableSupport->ClearAll(aId, aListener);
  } else if (aListener) {
    aListener->OnClearAll(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

NS_IMETHODIMP InputMethodService::ReplaceSurroundingText(
    uint64_t aId, nsIEditableSupportListener* aListener, const nsAString& aText,
    int32_t aOffset, int32_t aLength) {
  IME_LOGD("InputMethodService::ReplaceSurroundingText");
  if (mEditableSupport) {
    mEditableSupport->ReplaceSurroundingText(aId, aListener, aText, aOffset,
                                             aLength);
  } else if (aListener) {
    aListener->OnReplaceSurroundingText(aId, NS_ERROR_ABORT);
  }
  return NS_OK;
}

void InputMethodService::HandleFocus(nsIEditableSupport* aEditableSupport,
                                     nsIInputContext* aPropBag) {
  IME_LOGD("InputMethodService::HandleFocus");
  RegisterEditableSupport(aEditableSupport);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aPropBag, "inputmethod-contextchange", u"focus");
  }
}

void InputMethodService::HandleBlur(nsIEditableSupport* aEditableSupport,
                                    nsIInputContext* aPropBag) {
  IME_LOGD("InputMethodService::HandleBlur");
  UnregisterEditableSupport(aEditableSupport);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // Blur does not need input field information.
    obs->NotifyObservers(aPropBag, "inputmethod-contextchange", u"blur");
  }
}

void InputMethodService::HandleTextChanged(const nsAString& aText) {
  IME_LOGD("InputMethodService::HandleTextChanged");
  RefPtr<KeyboardAppProxy> proxy = KeyboardAppProxy::GetInstance();
  if (!proxy) {
    return;
  }
  proxy->OnTextChanged(NS_ConvertUTF16toUTF8(aText));
}

void InputMethodService::HandleSelectionChanged(uint32_t aStartOffset,
                                                uint32_t aEndOffset) {
  IME_LOGD("InputMethodService::HandleSelectionChanged");
  RefPtr<KeyboardAppProxy> proxy = KeyboardAppProxy::GetInstance();
  if (!proxy) {
    return;
  }
  proxy->OnSelectionChanged(aStartOffset, aEndOffset);
}

}  // namespace dom
}  // namespace mozilla
