/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputMethodServiceParent.h"
#include "mozilla/dom/InputMethodService.h"
#include "mozilla/dom/IMELog.h"
#include "mozilla/dom/nsInputContext.h"
namespace mozilla {
namespace dom {

void CarryIntoInputContext(nsIEditableSupport* aEditableSupport,
                           const HandleFocusRequest& aRequest,
                           nsInputContext* aInputContext) {
  aInputContext->SetType(aRequest.type());
  aInputContext->SetInputType(aRequest.inputType());
  aInputContext->SetValue(aRequest.value());
  aInputContext->SetMax(aRequest.max());
  aInputContext->SetMin(aRequest.min());
  aInputContext->SetLang(aRequest.lang());
  aInputContext->SetInputMode(aRequest.inputMode());
  aInputContext->SetVoiceInputSupported(aRequest.voiceInputSupported());
  aInputContext->SetName(aRequest.name());
  aInputContext->SetSelectionStart(aRequest.selectionStart());
  aInputContext->SetSelectionEnd(aRequest.selectionEnd());
  aInputContext->SetMaxLength(aRequest.maxLength());
  aInputContext->SetImeGroup(aRequest.imeGroup());
  aInputContext->SetLastImeGroup(aRequest.lastImeGroup());

  IME_LOGD("HandleFocusRequest: type:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.type()).get());
  IME_LOGD("HandleFocusRequest: inputType:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.inputType()).get());
  IME_LOGD("HandleFocusRequest: value:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.value()).get());
  IME_LOGD("HandleFocusRequest: max:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.max()).get());
  IME_LOGD("HandleFocusRequest: min:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.min()).get());
  IME_LOGD("HandleFocusRequest: lang:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.lang()).get());
  IME_LOGD("HandleFocusRequest: inputMode:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.inputMode()).get());
  IME_LOGD("HandleFocusRequest: voiceInputSupported:[%d]",
           aRequest.voiceInputSupported());
  IME_LOGD("HandleFocusRequest: name:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.name()).get());
  IME_LOGD("HandleFocusRequest: selectionStart:[%lu]",
           aRequest.selectionStart());
  IME_LOGD("HandleFocusRequest: selectionEnd:[%lu]", aRequest.selectionEnd());
  IME_LOGD("HandleFocusRequest: maxLength:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.maxLength()).get());
  IME_LOGD("HandleFocusRequest: imeGroup:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.imeGroup()).get());
  IME_LOGD("HandleFocusRequest: lastImeGroup:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.lastImeGroup()).get());

  RefPtr<nsInputContextChoices> inputContextChoices =
      new nsInputContextChoices();
  nsTArray<RefPtr<nsIInputContextChoicesInfo>> choices;
  choices.Clear();
  inputContextChoices->SetMultiple(aRequest.choices().multiple());

  for (uint32_t i = 0; i < aRequest.choices().choices().Length(); ++i) {
    const OptionDetailCollection& choice = aRequest.choices().choices()[i];
    if (choice.type() == OptionDetailCollection::TOptionDetail) {
      const OptionDetail& optionDetail = choice.get_OptionDetail();
      RefPtr<nsInputContextChoicesInfo> optInfo =
          new nsInputContextChoicesInfo();
      optInfo->SetGroup(optionDetail.group());
      optInfo->SetInGroup(optionDetail.inGroup());
      optInfo->SetValue(optionDetail.text());
      optInfo->SetText(optionDetail.text());
      optInfo->SetDisabled(optionDetail.disabled());
      optInfo->SetSelected(optionDetail.selected());
      optInfo->SetDefaultSelected(optionDetail.defaultSelected());
      optInfo->SetOptionIndex(optionDetail.optionIndex());
      choices.AppendElement(optInfo);
      IME_LOGD("HandleFocusRequest: OptionDetail:[%s]",
               NS_ConvertUTF16toUTF8(optionDetail.text()).get());
    } else {  // OptionGroupDetail
      const OptionGroupDetail& optionGroupDetail =
          choice.get_OptionGroupDetail();
      RefPtr<nsInputContextChoicesInfo> groupInfo =
          new nsInputContextChoicesInfo();
      groupInfo->SetGroup(optionGroupDetail.group());
      groupInfo->SetLabel(optionGroupDetail.label());
      groupInfo->SetDisabled(optionGroupDetail.disabled());
      choices.AppendElement(groupInfo);
      IME_LOGD("HandleFocusRequest: OptionGroupDetail:[%s]",
               NS_ConvertUTF16toUTF8(optionGroupDetail.label()).get());
    }
  }
  inputContextChoices->SetChoices(choices);
  aInputContext->SetInputContextChoices(inputContextChoices);
  aInputContext->SetEditableSupport(aEditableSupport);
}

void CarryIntoInputContext(const HandleBlurRequest& aRequest,
                           nsInputContext* aInputContext) {
  aInputContext->SetImeGroup(aRequest.imeGroup());
  aInputContext->SetLastImeGroup(aRequest.lastImeGroup());

  IME_LOGD("HandleFocusRequest: imeGroup:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.imeGroup()).get());
  IME_LOGD("HandleFocusRequest: lastImeGroup:[%s]",
           NS_ConvertUTF16toUTF8(aRequest.lastImeGroup()).get());
}

NS_IMPL_ISUPPORTS(InputMethodServiceParent, nsIEditableSupportListener,
                  nsIEditableSupport)

InputMethodServiceParent::InputMethodServiceParent() {
  IME_LOGD("InputMethodServiceParent::Constructor[%p]", this);
}

InputMethodServiceParent::~InputMethodServiceParent() {
  IME_LOGD("InputMethodServiceParent::Destructor[%p]", this);
}

nsIEditableSupport* InputMethodServiceParent::GetEditableSupport() {
  RefPtr<InputMethodService> instance = InputMethodService::GetInstance();
  return instance;
}

nsIEditableSupportListener*
InputMethodServiceParent::GetEditableSupportListener(uint32_t aId) {
  auto entry = mRequestMap.Lookup(aId);
  MOZ_ASSERT(entry);
  auto listener = entry.Data();
  IME_LOGD("InputMethodServiceParent::GetEditableSupportListener count [%d]",
           mRequestMap.Count());
  return listener;
}

IPCResult InputMethodServiceParent::RecvRequest(
    const InputMethodRequest& aRequest) {
  RefPtr<InputMethodService> service = InputMethodService::GetInstance();
  MOZ_ASSERT(service);

  switch (aRequest.type()) {
    case InputMethodRequest::THandleFocusRequest: {
      IME_LOGD("InputMethodServiceParent::RecvRequest:HandleFocusRequest");
      const HandleFocusRequest& request = aRequest;
      RefPtr<nsInputContext> inputContext = new nsInputContext();
      CarryIntoInputContext(this, request, inputContext);
      service->HandleFocus(this, static_cast<nsIInputContext*>(inputContext));
      break;
    }
    case InputMethodRequest::THandleBlurRequest: {
      IME_LOGD("InputMethodServiceParent::RecvRequest:HandleBlurRequest");
      const HandleBlurRequest& request = aRequest;
      RefPtr<nsInputContext> inputContext = new nsInputContext();
      CarryIntoInputContext(request, inputContext);
      service->HandleBlur(this, static_cast<nsIInputContext*>(inputContext));
      break;
    }
    case InputMethodRequest::THandleTextChangedRequest: {
      IME_LOGD(
          "InputMethodServiceParent::RecvRequest:HandleTextChangedRequest");
      const HandleTextChangedRequest& request = aRequest;
      service->HandleTextChanged(request.text());
      break;
    }
    case InputMethodRequest::THandleSelectionChangedRequest: {
      IME_LOGD(
          "InputMethodServiceParent::RecvRequest:HandleSelectionChangedRequest");
      const HandleSelectionChangedRequest& request = aRequest;
      service->HandleSelectionChanged(request.startOffset(), request.endOffset());
      break;
    }
    default: {
      return InputMethodServiceCommon<PInputMethodServiceParent>::RecvRequest(
          aRequest);
    }
  }
  return IPC_OK();
}

// nsIEditableSupport methods.
// When this is triggered on the chrome process, forward requests to the remote
// content.

// unique id on the chrome process to identify the request. When receiving the
// response from the child side, we get the listener by id and invoke the
// listener
static uint32_t sRequestId = 0;

NS_IMETHODIMP
InputMethodServiceParent::SetComposition(uint32_t aId,
                                         nsIEditableSupportListener* aListener,
                                         const nsAString& aText) {
  IME_LOGD("InputMethodServiceParent::SetComposition");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(SetCompositionRequest(sRequestId, nsString(aText)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::EndComposition(uint32_t aId,
                                         nsIEditableSupportListener* aListener,
                                         const nsAString& aText) {
  IME_LOGD("InputMethodServiceParent::EndComposition");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(EndCompositionRequest(sRequestId, nsString(aText)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::Keydown(uint32_t aId,
                                  nsIEditableSupportListener* aListener,
                                  const nsAString& aKey) {
  IME_LOGD("InputMethodServiceParent::Keydown");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(KeydownRequest(sRequestId, nsString(aKey)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::Keyup(uint32_t aId,
                                nsIEditableSupportListener* aListener,
                                const nsAString& aKey) {
  IME_LOGD("InputMethodServiceParent::Keyup");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(KeyupRequest(sRequestId, nsString(aKey)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::SendKey(uint32_t aId,
                                  nsIEditableSupportListener* aListener,
                                  const nsAString& aKey) {
  IME_LOGD("InputMethodServiceParent::SendKey");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(SendKeyRequest(sRequestId, nsString(aKey)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::DeleteBackward(
    uint32_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodServiceParent::DeleteBackward");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(DeleteBackwardRequest(sRequestId));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::SetSelectedOption(
    uint32_t aId, nsIEditableSupportListener* aListener, int32_t aOptionIndex) {
  IME_LOGD("InputMethodServiceParent::SetSelectedOption");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(SetSelectedOptionRequest(sRequestId, aOptionIndex));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::SetSelectedOptions(
    uint32_t aId, nsIEditableSupportListener* aListener,
    const nsTArray<int32_t>& aOptionIndexes) {
  IME_LOGD("InputMethodServiceParent::SetSelectedOptions");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(SetSelectedOptionsRequest(sRequestId, aOptionIndexes));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::RemoveFocus(uint32_t aId,
                                      nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodServiceParent::RemoveFocus");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  CommonRequest request(sRequestId, u"RemoveFocus"_ns);
  Unused << SendRequest(request);
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::GetSelectionRange(
    uint32_t aId, nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodServiceParent::GetSelectionRange");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(GetSelectionRangeRequest(sRequestId));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::GetText(uint32_t aId,
                                  nsIEditableSupportListener* aListener,
                                  int32_t aOffset, int32_t aLength) {
  IME_LOGD("InputMethodServiceParent::GetText");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(GetTextRequest(sRequestId, aOffset, aLength));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::SetValue(uint32_t aId,
                                   nsIEditableSupportListener* aListener,
                                   const nsAString& aValue) {
  IME_LOGD("InputMethodServiceParent::SetValue");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(SetValueRequest(sRequestId, nsString(aValue)));
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::ClearAll(uint32_t aId,
                                   nsIEditableSupportListener* aListener) {
  IME_LOGD("InputMethodServiceParent::ClearAll");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  CommonRequest request(sRequestId, u"ClearAll"_ns);
  Unused << SendRequest(request);
  return NS_OK;
}

NS_IMETHODIMP
InputMethodServiceParent::ReplaceSurroundingText(
    uint32_t aId, nsIEditableSupportListener* aListener, const nsAString& aText,
    int32_t aOffset, int32_t aLength) {
  IME_LOGD("InputMethodServiceParent::ReplaceSurroundingText");
  mRequestMap.InsertOrUpdate(++sRequestId, aListener);
  Unused << SendRequest(ReplaceSurroundingTextRequest(
      sRequestId, nsString(aText), aOffset, aLength));
  return NS_OK;
}

void InputMethodServiceParent::ActorDestroy(ActorDestroyReason why) {
  IME_LOGD("InputMethodServiceParent::ActorDestroy %p", this);
  RefPtr<InputMethodService> service = InputMethodService::GetInstance();
  if (service->GetRegisteredEditableSupport() == this) {
    RefPtr<nsInputContext> inputContext = new nsInputContext();
    service->HandleBlur(this, static_cast<nsIInputContext*>(inputContext));
  }
}

}  // namespace dom
}  // namespace mozilla
