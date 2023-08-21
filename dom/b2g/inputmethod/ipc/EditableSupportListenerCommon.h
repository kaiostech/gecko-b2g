/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EditableSupportListenerCommon_h
#define mozilla_dom_EditableSupportListenerCommon_h

#include "nsIEditableSupport.h"
#include "mozilla/dom/PInputMethodServiceParent.h"
#include "mozilla/dom/IMELog.h"

namespace mozilla {
namespace dom {

template <class T>
class EditableSupportListenerCommon : public nsIEditableSupportListener,
                                      public T {
 public:
  // nsIEditableSupportListener methods.
  NS_IMETHODIMP
  OnSetComposition(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnSetComposition");
    Unused << T::SendResponse(SetCompositionResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnEndComposition(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnEndComposition");
    Unused << T::SendResponse(EndCompositionResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnKeydown(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnKeydown");
    Unused << T::SendResponse(KeydownResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnKeyup(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnKeyup");
    Unused << T::SendResponse(KeyupResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnSendKey(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnSendKey");
    Unused << T::SendResponse(SendKeyResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnDeleteBackward(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnDeleteBackward");
    Unused << T::SendResponse(DeleteBackwardResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnSetSelectedOption(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnSetSelectedOption");
    Unused << T::SendResponse(SetSelectedOptionResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnSetSelectedOptions(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnSetSelectedOptions");
    Unused << T::SendResponse(SetSelectedOptionsResponse(aId, aStatus));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnRemoveFocus(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnRemoveFocus");
    Unused << T::SendResponse(CommonResponse(aId, aStatus, u"RemoveFocus"_ns));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnGetSelectionRange(uint64_t aId, nsresult aStatus, int32_t aStart,
                      int32_t aEnd) {
    Unused << T::SendResponse(
        GetSelectionRangeResponse(aId, aStatus, aStart, aEnd));
    IME_LOGD("Listener::OnGetSelectionRange: %d %d", aStart, aEnd);
    return NS_OK;
  }

  NS_IMETHODIMP
  OnGetText(uint64_t aId, nsresult aStatus, const nsAString& aText) {
    IME_LOGD("Listener::OnGetText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
    nsString text(aText);
    Unused << T::SendResponse(GetTextResponse(aId, aStatus, text));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnSetValue(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnSetValue");
    Unused << T::SendResponse(CommonResponse(aId, aStatus, u"SetValue"_ns));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnClearAll(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnClearAll");
    Unused << T::SendResponse(CommonResponse(aId, aStatus, u"ClearAll"_ns));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnReplaceSurroundingText(uint64_t aId, nsresult aStatus) {
    IME_LOGD("Listener::OnReplaceSurroundingText");
    Unused << T::SendResponse(
        CommonResponse(aId, aStatus, u"ReplaceSurroundingText"_ns));
    return NS_OK;
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EditableSupportListenerCommon_h
