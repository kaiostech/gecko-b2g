/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoEditableSupport.h"

#include "PuppetWidget.h"
#include "nsIContent.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/TextEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsIDocShell.h"
#include "mozilla/dom/IMELog.h"
#include "mozilla/dom/InputMethodService.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/Document.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/Element.h"
#include "nsIDocumentEncoder.h"
#include "nsRange.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/nsInputContext.h"
#include "mozilla/dom/EditableUtils.h"
#include "nsIEditor.h"  // for nsIEditor, etc.
#include "nsIEditingSession.h"

using namespace mozilla::dom;

namespace mozilla {

namespace widget {

// In content editable node, when there are hidden elements such as <br>, it
// may need more than one (usually less than 3 times) move/extend operations
// to change the selection range. If we cannot change the selection range
// with more than 20 opertations, we are likely being blocked and cannot change
// the selection range any more.
#define MAX_BLOCKED_COUNT 20

bool isDateTimeTypes(nsAString& inputType) {
  return inputType.EqualsIgnoreCase("datetime") ||
         inputType.EqualsIgnoreCase("datetime-local") ||
         inputType.EqualsIgnoreCase("time") ||
         inputType.EqualsIgnoreCase("date");
}

bool isPlainTextField(Element* aElement) {
  if (!aElement) {
    return false;
  }
  RefPtr<HTMLTextAreaElement> textArea =
      HTMLTextAreaElement::FromNodeOrNull(aElement);
  if (textArea) {
    return true;
  }
  RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
  if (input) {
    return input->MozIsTextField(false);
  }
  return false;
}

bool isVoiceInputSupported(Element* aElement,
                           nsTArray<nsCString>& aExcludedXInputModes,
                           nsTArray<nsCString>& aSupportedTypes) {
  if (!aElement) {
    return false;
  }
  nsAutoString attributeStringValue;
  nsAutoCString attributeCStringValue;
  aElement->GetAttribute(u"x-inputmode"_ns, attributeStringValue);
  attributeCStringValue = NS_ConvertUTF16toUTF8(attributeStringValue);
  for (uint32_t i = 0; i < aExcludedXInputModes.Length(); ++i) {
    if (attributeCStringValue.Equals(aExcludedXInputModes[i],
                                     nsCaseInsensitiveCStringComparator)) {
      return false;
    }
  }
  aElement->GetAttribute(u"type"_ns, attributeStringValue);
  attributeCStringValue = NS_ConvertUTF16toUTF8(attributeStringValue);
  for (uint32_t i = 0; i < aSupportedTypes.Length(); ++i) {
    if (attributeCStringValue.Equals(aSupportedTypes[i],
                                     nsCaseInsensitiveCStringComparator)) {
      return true;
    }
  }
  if (EditableUtils::isContentEditable(aElement)) {
    return true;
  }
  return false;
}

already_AddRefed<nsIDocumentEncoder> getDocumentEncoder(Element* aElement) {
  nsCOMPtr<Document> document = aElement->OwnerDoc();
  nsAutoString contentType;
  document->GetContentType(contentType);
  nsCOMPtr<nsIDocumentEncoder> docEncoder = do_createDocumentEncoder(
      PromiseFlatCString(NS_ConvertUTF16toUTF8(contentType)).get());
  uint32_t flags = nsIDocumentEncoder::SkipInvisibleContent |
                   nsIDocumentEncoder::OutputRaw |
                   nsIDocumentEncoder::OutputDropInvisibleBreak |
                   nsIDocumentEncoder::OutputLFLineBreak;
  docEncoder->Init(document, u"text/plain"_ns, flags);
  return docEncoder.forget();
}

nsresult getContentEditableText(Element* aElement, nsAString& aText) {
  if (!aElement || !EditableUtils::isContentEditable(aElement)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Document> document = aElement->OwnerDoc();
  ErrorResult rv;
  RefPtr<nsRange> range = document->CreateRange(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  range->SelectNodeContents(*aElement, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
  if (!docEncoder) {
    return NS_ERROR_FAILURE;
  }
  docEncoder->SetRange(range);
  return docEncoder->EncodeToString(aText);
}

uint32_t getSelectionStart(Element* aElement) {
  uint32_t start = 0;
  if (isPlainTextField(aElement)) {
    ErrorResult rv;
    Nullable<uint32_t> _start;
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    if (textArea) {
      _start = textArea->GetSelectionStart(rv);
    }
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    if (input) {
      _start = input->GetSelectionStartIgnoringType(rv);
    }
    if (!_start.IsNull()) {
      start = _start.Value();
    }
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    RefPtr<Selection> selection = window->GetSelection();
    if (selection && selection->RangeCount() > 0) {
      ErrorResult rv;
      RefPtr<nsRange> range = document->CreateRange(rv);
      if (NS_WARN_IF(rv.Failed())) {
        return 0;
      }
      range->SetStart(aElement, 0);
      range->SetEnd(selection->GetAnchorNode(), selection->AnchorOffset());
      nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
      if (!docEncoder) {
        return 0;
      }
      docEncoder->SetRange(range);
      nsAutoString buffer;
      docEncoder->EncodeToString(buffer);
      start = buffer.Length();
    }
  }
  return start;
}

uint32_t getSelectionEnd(Element* aElement) {
  uint32_t end = 0;
  if (isPlainTextField(aElement)) {
    ErrorResult rv;
    Nullable<uint32_t> _end;
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    if (textArea) {
      _end = textArea->GetSelectionEnd(rv);
    }
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    if (input) {
      _end = input->GetSelectionEndIgnoringType(rv);
    }
    if (!_end.IsNull()) {
      end = _end.Value();
    }
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    RefPtr<Selection> selection = window->GetSelection();
    if (selection && selection->RangeCount() > 0) {
      uint32_t start = getSelectionStart(aElement);
      nsCOMPtr<nsIDocumentEncoder> docEncoder = getDocumentEncoder(aElement);
      if (!docEncoder) {
        return 0;
      }
      ErrorResult rv;
      docEncoder->SetRange(selection->GetRangeAt(0, rv));
      nsAutoString buffer;
      docEncoder->EncodeToString(buffer);
      uint32_t length = buffer.Length();
      end = start + length;
    }
  }
  return end;
}

nsIEditor* getEditor(Element* aElement) {
  nsCOMPtr<nsIEditor> editor = nullptr;
  if (isPlainTextField(aElement)) {
    RefPtr<TextControlElement> textControlElement =
        TextControlElement::FromNodeOrNull(aElement);
    if (!textControlElement) {
      return nullptr;
    }
    editor = textControlElement->GetTextEditor();
  } else if (EditableUtils::isContentEditable(aElement)) {
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
    nsCOMPtr<nsIEditingSession> editSession;
    nsresult rv = docShell->GetEditingSession(getter_AddRefs(editSession));
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    editSession->GetEditorForWindow(window, getter_AddRefs(editor));
  }

  return editor;
}

bool setSelectionRange(Element* aElement, uint32_t aStart, uint32_t aEnd) {
  if (!dom::EditableUtils::isContentEditable(aElement) &&
      !isPlainTextField(aElement)) {
    // Skip HTMLOptionElement and HTMLSelectElement elements, as they don't
    // support the operation of setSelectionRange
    return false;
  }
  nsAutoString text;
  nsresult rv = NS_ERROR_FAILURE;
  if (dom::EditableUtils::isContentEditable(aElement)) {
    rv = getContentEditableText(aElement, text);
  } else {
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    if (textArea) {
      textArea->GetValue(text);
      rv = NS_OK;
    } else if (input) {
      input->GetValue(text, CallerType::NonSystem);
      rv = NS_OK;
    } else {
      IME_LOGD(
          "GeckoEditableSupport: setSelectionRange: Fail, unknow plain text");
      return false;
    }
  }
  if (rv != NS_OK) {
    return false;
  }
  uint32_t length = text.Length();
  if (aEnd > length) {
    aEnd = length;
  }
  if (aStart > aEnd) {
    aStart = aEnd;
  }
  if (isPlainTextField(aElement)) {
    ErrorResult eRv;
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(aElement);
    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(aElement);
    Optional<nsAString> direction;
    nsAutoString dir(u"forward"_ns);
    direction = &dir;
    if (textArea) {
      textArea->SetSelectionRange(aStart, aEnd, direction, eRv);
    } else if (input) {
      input->SetSelectionRange(aStart, aEnd, direction, eRv);
    } else {
      IME_LOGD(
          "GeckoEditableSupport: setSelectionRange: Fail, unknow plain text");
    }
    if (NS_WARN_IF(eRv.Failed())) {
      IME_LOGD(
          "GeckoEditableSupport: setSelectionRange: Fail to SetSelectionRange");
      return false;
    }
    return true;
  } else if (dom::EditableUtils::isContentEditable(aElement)) {
    // set the selection range of contenteditable elements
    nsCOMPtr<Document> document = aElement->OwnerDoc();
    nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
    RefPtr<Selection> selection = window->GetSelection();

    // Move the caret to the start position
    ErrorResult eRv;
    selection->CollapseInLimiter(aElement, 0);
    for (uint32_t i = 0; i < aStart; i++) {
      selection->Modify(u"move"_ns, u"forward"_ns, u"character"_ns, eRv);
      if (NS_WARN_IF(eRv.Failed())) {
        IME_LOGD(
            "GeckoEditableSupport: setSelectionRange: Fail to "
            "SetSelectionRange");
        return false;
      }
    }

    // Avoid entering infinite loop in case we cannot change the selection
    // range. See bug https://bugzilla.mozilla.org/show_bug.cgi?id=978918

    uint32_t oldStart = getSelectionStart(aElement);
    uint32_t counter = 0;
    while (oldStart < aStart) {
      selection->Modify(u"move"_ns, u"forward"_ns, u"character"_ns, eRv);
      uint32_t newStart = getSelectionStart(aElement);
      if (oldStart == newStart) {
        counter++;
        if (counter > MAX_BLOCKED_COUNT) {
          return false;
        }
      } else {
        counter = 0;
        oldStart = newStart;
      }
    }

    // Extend the selection to the end position
    for (uint32_t i = aStart; i < aEnd; i++) {
      selection->Modify(u"extend"_ns, u"forward"_ns, u"character"_ns, eRv);
      if (NS_WARN_IF(eRv.Failed())) {
        IME_LOGD(
            "GeckoEditableSupport: setSelectionRange: Fail to "
            "SetSelectionRange");
        return false;
      }
    }

    // Avoid entering infinite loop in case we cannot change the selection
    // range. See bug https://bugzilla.mozilla.org/show_bug.cgi?id=978918
    counter = 0;
    int32_t selectionLength = aEnd - aStart;
    int32_t oldSelectionLength =
        getSelectionEnd(aElement) - getSelectionStart(aElement);
    while (oldSelectionLength < selectionLength) {
      selection->Modify(u"extend"_ns, u"forward"_ns, u"character"_ns, eRv);
      if (NS_WARN_IF(eRv.Failed())) {
        IME_LOGD(
            "GeckoEditableSupport: setSelectionRange: Fail to "
            "SetSelectionRange");
        return false;
      }
      int32_t newSelectionLength =
          getSelectionEnd(aElement) - getSelectionStart(aElement);
      if (oldSelectionLength == newSelectionLength) {
        counter++;
        if (counter > MAX_BLOCKED_COUNT) {
          return false;
        }
      } else {
        counter = 0;
        oldSelectionLength = newSelectionLength;
      }
    }
    return true;
  }
  IME_LOGD("GeckoEditableSupport: setSelectionRange: Fail, unknow input type");
  return false;
}

TextEventDispatcher* getTextEventDispatcherFromFocus() {
  RefPtr<nsFocusManager> focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) {
    return nullptr;
  }
  RefPtr<Element> focusedElement = focusManager->GetFocusedElement();
  if (!focusedElement) {
    return nullptr;
  }
  nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }
  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    return nullptr;
  }
  RefPtr<nsIWidget> widget = presContext->GetTextInputHandlingWidget();
  if (!widget) {
    return nullptr;
  }
  return widget->GetTextEventDispatcher();
}

HandleFocusRequest CreateFocusRequestFromInputContext(
    nsIInputContext* aInputContext) {
  nsAutoString typeValue, inputTypeValue, valueValue, maxValue, minValue,
      langValue, inputModeValue, nameValue, maxLength, imeGroup, lastImeGroup;
  uint32_t selectionStartValue, selectionEndValue;
  bool voiceInputSupportedValue;

  aInputContext->GetType(typeValue);
  aInputContext->GetInputType(inputTypeValue);
  aInputContext->GetValue(valueValue);
  aInputContext->GetMax(maxValue);
  aInputContext->GetMin(minValue);
  aInputContext->GetLang(langValue);
  aInputContext->GetInputMode(inputModeValue);
  aInputContext->GetVoiceInputSupported(&voiceInputSupportedValue);
  aInputContext->GetName(nameValue);
  aInputContext->GetSelectionStart(&selectionStartValue);
  aInputContext->GetSelectionEnd(&selectionEndValue);
  nsCOMPtr<nsIInputContextChoices> choices;
  aInputContext->GetChoices(getter_AddRefs(choices));
  aInputContext->GetMaxLength(maxLength);
  aInputContext->GetImeGroup(imeGroup);
  aInputContext->GetLastImeGroup(lastImeGroup);
  SelectionChoices selectionChoices;
  nsTArray<OptionDetailCollection> optionDetailArray;
  if (choices) {
    bool isMultiple;
    choices->GetMultiple(&isMultiple);
    nsTArray<RefPtr<nsIInputContextChoicesInfo>> infos;
    choices->GetChoices(infos);

    for (const auto& info : infos) {
      bool isGroup;
      info->GetGroup(&isGroup);
      if (isGroup) {  // OptionGroupDetail
        OptionGroupDetail groupDetail;
        bool isGroupDisabled;
        info->GetDisabled(&isGroupDisabled);
        nsAutoString label;
        info->GetLabel(label);
        groupDetail.group() = isGroup;
        groupDetail.label() = nsString(label);
        groupDetail.disabled() = isGroupDisabled;
        IME_LOGD("groupDetail.group:[%s]", isGroup ? "true" : "false");
        IME_LOGD("groupDetail.disabled:[%s]",
                 isGroupDisabled ? "true" : "false");
        IME_LOGD("groupDetail.label:[%s]", NS_ConvertUTF16toUTF8(label).get());
        optionDetailArray.AppendElement(groupDetail);
      } else {  // OptionDetail
        OptionDetail optionDetail;
        bool isInGroup, isDisabled, isSelected, isDefaultSelected;
        nsAutoString text;
        int32_t optionIndex;
        info->GetInGroup(&isInGroup);
        info->GetDisabled(&isDisabled);
        info->GetSelected(&isSelected);
        info->GetDefaultSelected(&isDefaultSelected);
        info->GetText(text);
        info->GetOptionIndex(&optionIndex);
        optionDetail.group() = isGroup;
        optionDetail.inGroup() = isInGroup;
        optionDetail.disabled() = isDisabled;
        optionDetail.selected() = isSelected;
        optionDetail.defaultSelected() = isDefaultSelected;
        optionDetail.text() = nsString(text);
        optionDetail.optionIndex() = optionIndex;
        optionDetailArray.AppendElement(optionDetail);
        IME_LOGD("optionDetail.group:[%s]", isGroup ? "true" : "false");
        IME_LOGD("optionDetail.inGroup:[%s]", isInGroup ? "true" : "false");
        IME_LOGD("optionDetail.disabled:[%s]", isDisabled ? "true" : "false");
        IME_LOGD("optionDetail.selected:[%s]", isSelected ? "true" : "false");
        IME_LOGD("optionDetail.defaultSelected:[%s]",
                 isDefaultSelected ? "true" : "false");
        IME_LOGD("optionDetail.text:[%s]", NS_ConvertUTF16toUTF8(text).get());
        IME_LOGD("optionDetail.optionIndex:[%ld]", optionIndex);
      }
    }
    selectionChoices.multiple() = isMultiple;
    selectionChoices.choices() = optionDetailArray.Clone();
  }

  HandleFocusRequest request(
      nsString(typeValue), nsString(inputTypeValue), nsString(valueValue),
      nsString(maxValue), nsString(minValue), nsString(langValue),
      nsString(inputModeValue), voiceInputSupportedValue, nsString(nameValue),
      selectionStartValue, selectionEndValue, selectionChoices,
      nsString(maxLength), nsString(imeGroup), nsString(lastImeGroup));
  return request;
}

HandleBlurRequest CreateBlurRequestFromInputContext(
    nsIInputContext* aInputContext) {
  nsAutoString imeGroup, lastImeGroup;
  aInputContext->GetImeGroup(imeGroup);
  aInputContext->GetLastImeGroup(lastImeGroup);
  HandleBlurRequest request((nsString(imeGroup)), (nsString(lastImeGroup)));
  return request;
}

NS_IMPL_ISUPPORTS(GeckoEditableSupport, TextEventDispatcherListener,
                  nsIEditableSupport, nsIDOMEventListener, nsIObserver,
                  nsISupportsWeakReference, nsIMutationObserver)

GeckoEditableSupport::GeckoEditableSupport(nsPIDOMWindowOuter* aDOMWindow)
    : mServiceChild(nullptr), mIsVoiceInputEnabled(false) {
  IME_LOGD("GeckoEditableSupport::Constructor[%p]", this);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "outer-window-destroyed", false);
  }
  nsCOMPtr<EventTarget> mChromeEventHandler =
      aDOMWindow->GetChromeEventHandler();
  if (mChromeEventHandler) {
    IME_LOGD("AddEventListener: focus & blur");
    mChromeEventHandler->AddEventListener(u"focus"_ns, this,
                                          /* useCapture = */ true);
    mChromeEventHandler->AddEventListener(u"blur"_ns, this,
                                          /* useCapture = */ true);
  }
  mIsVoiceInputEnabled = Preferences::GetBool("voice-input.enabled", false);
  nsAutoCString voiceInputSupportedTypes;
  if (NS_SUCCEEDED(Preferences::GetCString("voice-input.supported-types",
                                           voiceInputSupportedTypes))) {
    for (const auto& token :
         nsCCharSeparatedTokenizer(voiceInputSupportedTypes, ',').ToRange()) {
      nsAutoCString type(token);
      IME_LOGD(" voice input supported type: %s", type.get());
      mVoiceInputSupportedTypes.AppendElement(type);
    }
  }
  nsAutoCString voiceInputExcludedXInputModes;
  if (NS_SUCCEEDED(Preferences::GetCString("voice-input.excluded-x-inputmodes",
                                           voiceInputExcludedXInputModes))) {
    for (const auto& token :
         nsCCharSeparatedTokenizer(voiceInputExcludedXInputModes, ',')
             .ToRange()) {
      nsAutoCString mode(token);
      IME_LOGD(" voice input excluded-x-inputmodes: %s", mode.get());
      mVoiceInputExcludedXInputModes.AppendElement(mode);
    }
  }
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->AddObserver("voice-input.enabled", this, false);
    prefs->AddObserver("voice-input.supported-types", this, false);
  }
}

GeckoEditableSupport::~GeckoEditableSupport() {
  IME_LOGD("GeckoEditableSupport::Destructor[%p]", this);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "outer-window-destroyed");
  }
}

NS_IMETHODIMP
GeckoEditableSupport::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  IME_LOGD("GeckoEditableSupport[%p], Observe[%s]", this, aTopic);
  if (!strcmp(aTopic, "outer-window-destroyed")) {
    if (!mChromeEventHandler) {
      return NS_ERROR_FAILURE;
    }
    mChromeEventHandler->RemoveEventListener(u"focus"_ns, this,
                                             /* useCapture = */ true);
    mChromeEventHandler->RemoveEventListener(u"blur"_ns, this,
                                             /* useCapture = */ true);
  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    if (!nsCRT::strcmp(aData, u"voice-input.enabled")) {
      mIsVoiceInputEnabled = Preferences::GetBool("voice-input.enabled", false);
      IME_LOGD(" voice-input.enabled: %s",
               mIsVoiceInputEnabled ? "true" : "false");
    } else if (!nsCRT::strcmp(aData, u"voice-input.supported-types")) {
      nsAutoCString voiceInputSupportedTypes;
      if (NS_SUCCEEDED(Preferences::GetCString("voice-input.supported-types",
                                               voiceInputSupportedTypes))) {
        mVoiceInputSupportedTypes.Clear();
        for (const auto& token :
             nsCCharSeparatedTokenizer(voiceInputSupportedTypes, ',')
                 .ToRange()) {
          nsAutoCString type(token);
          IME_LOGD(" voice input supported type: %s", type.get());
          mVoiceInputSupportedTypes.AppendElement(type);
        }
      }
    }
  }

  return NS_OK;
}

Element* GeckoEditableSupport::GetActiveElement() {
  RefPtr<Element> ele = do_QueryReferent(mFocusedTarget);
  if (!ele) {
    IME_LOGD("Cannot get valid composedTarget.");
    return nullptr;
  }
  return ele;
}

NS_IMETHODIMP
GeckoEditableSupport::HandleEvent(Event* aEvent) {
  if (!aEvent) {
    return NS_OK;
  }
  nsCOMPtr<EventTarget> target = aEvent->GetComposedTarget();
  nsCOMPtr<nsINode> node(do_QueryInterface(target));
  if (!node || !node->IsHTMLElement()) {
    return NS_OK;
  }
  nsCOMPtr<Element> ele = node->AsElement();
  if (!ele) {
    return NS_OK;
  }

  bool isTargetEditable = EditableUtils::isContentEditable(ele) ||
                          EditableUtils::isFocusableElement(ele);

  nsAutoString eventType;
  aEvent->GetType(eventType);

  InternalFocusEvent* focusEvent = aEvent->WidgetEventPtr()->AsFocusEvent();
  if (!focusEvent) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> relatedTarget =
      do_QueryInterface(focusEvent->mRelatedTarget);
  nsIContent* nonChromeRelatedTarget =
      relatedTarget ? relatedTarget->FindFirstNonChromeOnlyAccessContent()
                    : nullptr;

  IME_LOGD(
      "GeckoEditableSupport::HandleEvent %s target %p editable %d related %p",
      NS_ConvertUTF16toUTF8(eventType).get(), node.get(), isTargetEditable,
      nonChromeRelatedTarget);

  if (node == nonChromeRelatedTarget) {
    return NS_OK;
  }

  if (eventType.EqualsLiteral("focus")) {
    if (!isTargetEditable) {
      // Focusing on other non-editable element and the blur event is missing.
      if (mFocusedTarget) {
        HandleBlur(ele);
      }
      return NS_OK;
    }
    HandleFocus(ele);
    RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
    if (dispatcher) {
      nsresult result = dispatcher->BeginInputTransaction(this);
      if (NS_WARN_IF(NS_FAILED(result))) {
        IME_LOGD("Fails to BeginInputTransaction");
      }
    }
  } else if (eventType.EqualsLiteral("blur")) {
    if (!isTargetEditable) {
      return NS_OK;
    }
    HandleBlur(nonChromeRelatedTarget ? nonChromeRelatedTarget->AsElement()
                                      : nullptr);
  }
  return NS_OK;
}

void GeckoEditableSupport::EnsureServiceChild() {
  if (mServiceChild == nullptr) {
    mServiceChild = new InputMethodServiceChild();
    ContentChild::GetSingleton()->SendPInputMethodServiceConstructor(
        mServiceChild);
  }
}

void GeckoEditableSupport::HandleFocus(Element* aFocusedElement) {
  RefPtr<nsInputContext> inputContext = new nsInputContext();
  nsresult rv = GetFocusInputContextBag(inputContext, aFocusedElement);
  if (NS_FAILED(rv)) {
    return;
  }
  if (nsCOMPtr<Document> document = aFocusedElement->OwnerDoc()) {
    document->AddMutationObserver(this);
  }
  mFocusedTarget = do_GetWeakReference(aFocusedElement);
  inputContext->GetImeGroup(mLastImeGroup);

  if (XRE_IsParentProcess()) {
    IME_LOGD("-- GeckoEditableSupport::HandleFocus in parent");
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleFocus(this, static_cast<nsIInputContext*>(inputContext));
  } else {
    IME_LOGD("-- GeckoEditableSupport::HandleFocus in content");
    EnsureServiceChild();
    mServiceChild->SetEditableSupport(this);
    mServiceChild->SendRequest(
        CreateFocusRequestFromInputContext(inputContext));
    // TODO Unable to delete here, find somewhere else to do so.
  }
}

void GeckoEditableSupport::HandleBlur(Element* aRelatedElement) {
  if (!mFocusedTarget) {
    return;
  }

  RefPtr<nsInputContext> inputContext = new nsInputContext();
  GetBlurInputContextBag(inputContext, aRelatedElement);
  RefPtr<Element> ele = do_QueryReferent(mFocusedTarget);
  if (ele) {
    if (nsCOMPtr<Document> document = ele->OwnerDoc()) {
      document->RemoveMutationObserver(this);
    }
  }
  if (!aRelatedElement) {
    // If bluring w/o next focused element, we have to clear mLastImeGroup.
    mLastImeGroup = nsAutoString(u""_ns);
  }
  mFocusedTarget = nullptr;

  if (XRE_IsParentProcess()) {
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleBlur(this, static_cast<nsIInputContext*>(inputContext));
  } else {
    EnsureServiceChild();
    mServiceChild->SetEditableSupport(this);
    mServiceChild->SendRequest(CreateBlurRequestFromInputContext(inputContext));
    Unused << mServiceChild->Send__delete__(mServiceChild);
    mServiceChild = nullptr;
  }
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (dispatcher) {
    dispatcher->EndInputTransaction(this);
    OnRemovedFrom(dispatcher);
  }
}

void GeckoEditableSupport::HandleTextChanged() {
  nsresult rv = NS_ERROR_ABORT;
  nsAutoString text;
  RefPtr<Element> activeElement = GetActiveElement();
  if (!activeElement) {
    return;
  }
  if (EditableUtils::isContentEditable(activeElement)) {
    rv = getContentEditableText(activeElement, text);
  } else {
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(activeElement);
    RefPtr<HTMLInputElement> input =
        HTMLInputElement::FromNodeOrNull(activeElement);
    if (textArea) {
      textArea->GetValue(text);
      rv = NS_OK;
    } else if (input) {
      input->GetValue(text, CallerType::NonSystem);
      rv = NS_OK;
    }
  }
  if (rv != NS_OK) {
    return;
  }
  if (XRE_IsParentProcess()) {
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleTextChanged(text);
  } else {
    EnsureServiceChild();
    mServiceChild->SetEditableSupport(this);
    HandleTextChangedRequest request(text);
    mServiceChild->SendRequest(request);
  }
}

void GeckoEditableSupport::HandleSelectionChanged(uint32_t aStartOffset,
                                                  uint32_t aEndOffset) {
  if (XRE_IsParentProcess()) {
    // Call from parent process (or in-proces app).
    RefPtr<InputMethodService> service = dom::InputMethodService::GetInstance();
    service->HandleSelectionChanged(aStartOffset, aEndOffset);
  } else {
    EnsureServiceChild();
    mServiceChild->SetEditableSupport(this);
    HandleSelectionChangedRequest request(aStartOffset, aEndOffset);
    mServiceChild->SendRequest(request);
  }
}

void GeckoEditableSupport::HandleInputPositionChange() {
  RefPtr<Element> ele = do_QueryReferent(mFocusedTarget);
  if (!ele) {
    return;
  }
  nsCOMPtr<Document> doc = ele->GetComposedDoc();
  if (!doc) {
    return;
  }
  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return;
  }
  presShell->ScrollContentIntoView(
      ele, ScrollAxis(WhereToScroll::Nearest, WhenToScroll::IfNotFullyVisible),
      ScrollAxis(WhereToScroll::Nearest, WhenToScroll::IfNotFullyVisible),
      ScrollFlags::ScrollOverflowHidden);
}

NS_IMETHODIMP
GeckoEditableSupport::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                                const IMENotification& aNotification) {
  IME_LOGD("NotifyIME(aNotification={ mMessage=%s }",
           ToChar(aNotification.mMessage));

  switch (aNotification.mMessage) {
    case NOTIFY_IME_OF_SELECTION_CHANGE: {
      IME_LOGD("SelectionChangeData=%s",
               ToString(aNotification.mSelectionChangeData).c_str());
      uint32_t start = aNotification.mSelectionChangeData.StartOffset();
      uint32_t end = aNotification.mSelectionChangeData.EndOffset();
      IME_LOGD("SelectionChange start:[%u], end:[%u]", start, end);
      HandleSelectionChanged(start, end);
      break;
    }
    case NOTIFY_IME_OF_TEXT_CHANGE: {
      IME_LOGD("TextChangeData=%s",
               ToString(aNotification.mTextChangeData).c_str());
      HandleTextChanged();
      break;
    }
    case NOTIFY_IME_OF_FOCUS:
    case NOTIFY_IME_OF_BLUR: {
      if (!mIsVoiceInputEnabled) {
        break;
      }

      RefPtr<nsFocusManager> focusManager = nsFocusManager::GetFocusManager();
      if (!focusManager) {
        break;
      }

      RefPtr<Element> focusedElement = focusManager->GetFocusedElement();
      if (focusedElement) {
        nsCOMPtr<Document> doc = focusedElement->GetComposedDoc();
        if (!doc) {
          break;
        }

        nsString eventName = aNotification.mMessage == NOTIFY_IME_OF_FOCUS
                                 ? u"IMEFocus"_ns
                                 : u"IMEBlur"_ns;
        nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
            "GeckoEditableSupport::NotifyIMEFocusOrBlur",
            [doc = doc, element = focusedElement, name = eventName]() {
              nsContentUtils::DispatchChromeEvent(
                  doc, element, name, CanBubble::eYes, Cancelable::eNo);
            }));
      }
      break;
    }
    case NOTIFY_IME_OF_POSITION_CHANGE: {
      HandleInputPositionChange();
      break;
    }
    default:
      break;
  }

  return NS_OK;
}

// nsIEditableSupport methods.
NS_IMETHODIMP
GeckoEditableSupport::SetComposition(uint64_t aId,
                                     nsIEditableSupportListener* aListener,
                                     const nsAString& aText, int32_t aOffset,
                                     int32_t aLength) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::SetComposition");
  IME_LOGD("-- EditableSupport aText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  do {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
    if (aText.Length() == 0) {
      dispatcher->SetPendingCompositionString(EmptyString());
    } else {
      dispatcher->SetPendingCompositionString(aText);
      dispatcher->SetCaretInPendingComposition(aText.Length(), 0);

      if (aLength) {
        // aOffset and aLength defines the range of text to be highlight.
        dispatcher->AppendClauseToPendingComposition(aOffset,
                                                     TextRangeType::eRawClause);
        dispatcher->AppendClauseToPendingComposition(
            aLength, TextRangeType::eSelectedClause);
        dispatcher->AppendClauseToPendingComposition(
            aText.Length() - aOffset - aLength, TextRangeType::eRawClause);
      } else {
        dispatcher->AppendClauseToPendingComposition(aText.Length(),
                                                     TextRangeType::eRawClause);
      }
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = dispatcher->FlushPendingComposition(status);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetComposition(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::EndComposition(uint64_t aId,
                                     nsIEditableSupportListener* aListener,
                                     const nsAString& aText) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::EndComposition");
  IME_LOGD("-- EditableSupport aText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  do {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!dispatcher->IsComposing()) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = dispatcher->CommitComposition(status, &aText);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnEndComposition(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::Keydown(uint64_t aId,
                              nsIEditableSupportListener* aListener,
                              const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::Keydown");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = dispatcher->GetWidget();
    WidgetKeyboardEvent event(true, eKeyDown, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport OnKeydown: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    if (dispatcher->DispatchKeyboardEvent(eKeyDown, event, status)) {
      dispatcher->MaybeDispatchKeypressEvents(event, status);
    }
  }

  if (aListener) {
    aListener->OnKeydown(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::Keyup(uint64_t aId, nsIEditableSupportListener* aListener,
                            const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::Keyup");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  // Handle value
  nsresult rv = dispatcher->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = dispatcher->GetWidget();
    WidgetKeyboardEvent event(true, eKeyUp, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport OnKeyup: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    dispatcher->DispatchKeyboardEvent(eKeyUp, event, status);
  }

  if (aListener) {
    aListener->OnKeyup(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SendKey(uint64_t aId,
                              nsIEditableSupportListener* aListener,
                              const nsAString& aKey) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- EditableSupport SendKey");
  IME_LOGD("-- EditableSupport aKey:[%s]", NS_ConvertUTF16toUTF8(aKey).get());
  IME_LOGD("-- EditableSupport dispatcher:[%p]", dispatcher.get());
  RefPtr<TextEventDispatcher> kungFuDeathGrip(dispatcher);
  nsresult rv = kungFuDeathGrip->GetState();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> widget = kungFuDeathGrip->GetWidget();
    WidgetKeyboardEvent event(true, eVoidEvent, widget);
    event.mHandledByIME = true;
    event.mKeyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aKey);
    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      event.mKeyValue = aKey;
    } else if (event.mCodeNameIndex == CODE_NAME_INDEX_UNKNOWN) {
      event.mKeyCode = WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
          event.mKeyNameIndex);
      IME_LOGD("-- EditableSupport SendKey: mKeyCode:[%d]", event.mKeyCode);
    }
    nsEventStatus status = nsEventStatus_eIgnore;
    if (kungFuDeathGrip->DispatchKeyboardEvent(eKeyDown, event, status)) {
      kungFuDeathGrip->MaybeDispatchKeypressEvents(event, status);
    }
    kungFuDeathGrip->DispatchKeyboardEvent(eKeyUp, event, status);
  }

  if (aListener) {
    aListener->OnSendKey(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::DeleteBackward(uint64_t aId,
                                     nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::DeleteBackward");

  nsresult rv = NS_ERROR_ABORT;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }
    nsCOMPtr<nsIEditor> editor = getEditor(activeElement);
    if (!editor) {
      break;
    }

    rv = editor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
  } while (0);

  if (aListener) {
    aListener->OnDeleteBackward(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetSelectedOption(uint64_t aId,
                                        nsIEditableSupportListener* aListener,
                                        int32_t aOptionIndex) {
  IME_LOGD("-- EditableSupport SetSelectedOption");
  IME_LOGD("-- EditableSupport aOptionIndex:[%ld]", aOptionIndex);
  nsresult rv = NS_ERROR_ABORT;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }

    if (aOptionIndex < 0) {
      break;
    }

    RefPtr<HTMLSelectElement> selectElement =
        HTMLSelectElement::FromNodeOrNull(activeElement);
    if (!selectElement) {
      break;
    }
    // Safe to convert to unit since index is not negative
    if ((uint32_t)aOptionIndex > selectElement->Length()) {
      break;
    }

    rv = NS_OK;
    if (selectElement->SelectedIndex() == aOptionIndex) {
      break;
    }

    selectElement->SetSelectedIndex(aOptionIndex);

    nsCOMPtr<Document> document = selectElement->OwnerDoc();
    RefPtr<Event> event = NS_NewDOMEvent(document, nullptr, nullptr);
    event->InitEvent(u"change"_ns, true, true);
    event->SetTrusted(true);

    ErrorResult result;
    selectElement->DispatchEvent(*event, result);
    if (result.Failed()) {
      IME_LOGD("Failed to dispatch change event!!!");
      rv = NS_ERROR_ABORT;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetSelectedOption(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetSelectedOptions(
    uint64_t aId, nsIEditableSupportListener* aListener,
    const nsTArray<int32_t>& aOptionIndexes) {
  IME_LOGD("-- EditableSupport SetSelectedOption");
  IME_LOGD("-- EditableSupport aOptionIndexes.Length():[%ld]",
           aOptionIndexes.Length());

  nsresult rv = NS_ERROR_ABORT;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }

    RefPtr<HTMLSelectElement> selectElement =
        HTMLSelectElement::FromNodeOrNull(activeElement);
    if (!selectElement) {
      break;
    }

    // only fire onchange event if any selected option is changed
    bool changed = false;
    RefPtr<dom::HTMLOptionsCollection> options = selectElement->GetOptions();
    uint32_t numOptions = options->Length();
    for (int32_t idx = 0; idx < int(numOptions); idx++) {
      bool newValue = (aOptionIndexes.IndexOf(idx) != aOptionIndexes.NoIndex);
      bool oldValue = options->ItemAsOption(idx)->Selected();
      IME_LOGD("-- SetSelectedOptions options[%ld] old:[%d] -> new:[%d]", idx,
               oldValue, newValue);
      if (oldValue != newValue) {
        options->ItemAsOption(idx)->SetSelected(newValue);
        changed = true;
      }
    }
    rv = NS_OK;
    if (!changed) {
      break;
    }

    nsCOMPtr<Document> document = selectElement->OwnerDoc();
    RefPtr<Event> event = NS_NewDOMEvent(document, nullptr, nullptr);
    event->InitEvent(u"change"_ns, true, true);
    event->SetTrusted(true);

    ErrorResult result;
    selectElement->DispatchEvent(*event, result);
    if (result.Failed()) {
      IME_LOGD("Failed to dispatch change event!!!");
      rv = NS_ERROR_ABORT;
      break;
    }
  } while (0);

  if (aListener) {
    aListener->OnSetSelectedOptions(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::RemoveFocus(uint64_t aId,
                                  nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::RemoveFocus");
  nsresult rv = NS_ERROR_ABORT;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }

    ErrorResult result;
    activeElement->Blur(result);
    rv = NS_OK;
    if (NS_WARN_IF(result.Failed())) {
      rv = result.StealNSResult();
    }
  } while (0);

  if (aListener) {
    aListener->OnRemoveFocus(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::GetSelectionRange(uint64_t aId,
                                        nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport GetSelectionRange");

  nsresult rv = NS_ERROR_ABORT;
  int32_t start = 0, end = 0;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }

    rv = NS_OK;
    start = (int32_t)getSelectionStart(activeElement);
    end = (int32_t)getSelectionEnd(activeElement);
  } while (0);

  IME_LOGD("GeckoEditableSupport: GetSelectionRange:[rv:%d start:%lu end:%lu]",
           rv, start, end);
  if (aListener) {
    aListener->OnGetSelectionRange(aId, rv, start, end);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::GetText(uint64_t aId,
                              nsIEditableSupportListener* aListener,
                              int32_t aOffset, int32_t aLength) {
  IME_LOGD("-- GeckoEditableSupport GetText");

  nsresult rv = NS_ERROR_ABORT;
  nsAutoString text;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }
    if (EditableUtils::isContentEditable(activeElement)) {
      rv = getContentEditableText(activeElement, text);
    } else {
      RefPtr<HTMLTextAreaElement> textArea =
          HTMLTextAreaElement::FromNodeOrNull(activeElement);
      RefPtr<HTMLInputElement> input =
          HTMLInputElement::FromNodeOrNull(activeElement);
      if (textArea) {
        textArea->GetValue(text);
        rv = NS_OK;
      } else if (input) {
        input->GetValue(text, CallerType::NonSystem);
        rv = NS_OK;
      } else {
        IME_LOGD(
            "GeckoEditableSupport: GetText: Fail due to not supported input");
      }
    }
  } while (0);
  text = Substring(text, aOffset, aLength);
  IME_LOGD("GeckoEditableSupport: GetText: rv:[%d] text:[%s]", rv,
           NS_ConvertUTF16toUTF8(text).get());
  if (aListener) {
    aListener->OnGetText(aId, rv, text);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::SetValue(uint64_t aId,
                               nsIEditableSupportListener* aListener,
                               const nsAString& aValue) {
  RefPtr<TextEventDispatcher> dispatcher = getTextEventDispatcherFromFocus();
  if (!dispatcher) {
    IME_LOGD("Invalid dispatcher");
    return NS_ERROR_ABORT;
  }
  IME_LOGD("-- GeckoEditableSupport::SetValue dispatcher:[%p]",
           dispatcher.get());
  nsresult rv = NS_ERROR_ABORT;
  do {
    // End current composition
    if (dispatcher->IsComposing()) {
      rv = EndComposition(0, nullptr, EmptyString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    }

    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }
    nsCOMPtr<Document> doc = activeElement->GetComposedDoc();
    RefPtr<HTMLInputElement> inputElement =
        HTMLInputElement::FromNodeOrNull(activeElement);
    RefPtr<HTMLTextAreaElement> textArea =
        HTMLTextAreaElement::FromNodeOrNull(activeElement);
    ErrorResult erv;
    if (inputElement) {
      // Only file input need systemtype.
      inputElement->SetValue(aValue, CallerType::NonSystem, erv);
    } else if (textArea) {
      textArea->SetValue(aValue, erv);
    } else if (dom::EditableUtils::isContentEditable(activeElement)) {
      nsGenericHTMLElement* genericElement =
          nsGenericHTMLElement::FromNode(activeElement);
      genericElement->SetInnerText(aValue);
    } else {
      IME_LOGD("GeckoEditableSupport::SetValue, element type not supported.");
      break;
    }
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD("-- GeckoEditableSupport::SetValue, Fail to set value");
      break;
    }
    RefPtr<Event> event = NS_NewDOMEvent(doc, nullptr, nullptr);
    event->InitEvent(u"input"_ns, true, false);
    event->SetTrusted(true);
    activeElement->DispatchEvent(*event, erv);
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD(
          "GeckoEditableSupport::SetValue, Failed to dispatch input event.");
      break;
    }
    nsAutoString type;
    activeElement->GetAttribute(u"type"_ns, type);
    if (isDateTimeTypes(type)) {
      RefPtr<Event> changeEvent = NS_NewDOMEvent(doc, nullptr, nullptr);
      changeEvent->InitEvent(u"change"_ns, true, false);
      changeEvent->SetTrusted(true);
      activeElement->DispatchEvent(*changeEvent, erv);
      if (NS_WARN_IF(erv.Failed())) {
        IME_LOGD(
            "GeckoEditableSupport::DoSetValue, Failed to dispatch change "
            "event.");
        break;
      }
    }
    rv = NS_OK;
  } while (0);
  IME_LOGD("GeckoEditableSupport: SetValue:[rv:%d, value:[%s]", rv,
           NS_ConvertUTF16toUTF8(aValue).get());
  if (aListener) {
    aListener->OnSetValue(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::ClearAll(uint64_t aId,
                               nsIEditableSupportListener* aListener) {
  IME_LOGD("-- GeckoEditableSupport::ClearAll");
  nsresult rv = NS_ERROR_ABORT;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }
    nsCOMPtr<Document> doc = activeElement->GetComposedDoc();
    RefPtr<HTMLInputElement> inputElement =
        HTMLInputElement::FromNodeOrNull(activeElement);
    if (inputElement) {
      rv = SetValue(0, nullptr, EmptyString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    } else {
      // non-input type such as TextAreaElement
      nsCOMPtr<nsIEditor> editor = getEditor(activeElement);
      if (!editor) {
        break;
      }
      rv = editor->SelectAll();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
      rv = editor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    }
    // Fire change event.
    ErrorResult erv;
    RefPtr<Event> changeEvent = NS_NewDOMEvent(doc, nullptr, nullptr);
    changeEvent->InitEvent(u"change"_ns, true, false);
    changeEvent->SetTrusted(true);
    activeElement->DispatchEvent(*changeEvent, erv);
    if (NS_WARN_IF(erv.Failed())) {
      IME_LOGD(
          "GeckoEditableSupport::ClearAll, Failed to dispatch change "
          "event.");
      break;
    }

  } while (0);

  if (aListener) {
    aListener->OnClearAll(aId, rv);
  }
  return rv;
}

NS_IMETHODIMP
GeckoEditableSupport::ReplaceSurroundingText(
    uint64_t aId, nsIEditableSupportListener* aListener, const nsAString& aText,
    int32_t aOffset, int32_t aLength) {
  IME_LOGD("-- GeckoEditableSupport::ReplaceSurroundingText");
  IME_LOGD("-- EditableSupport aText:[%s]", NS_ConvertUTF16toUTF8(aText).get());
  nsresult rv = NS_ERROR_ABORT;
  int32_t start = 0, end = 0;
  do {
    Element* activeElement = GetActiveElement();
    if (!activeElement) {
      break;
    }

    nsCOMPtr<nsIEditor> editor = getEditor(activeElement);
    if (!editor) {
      break;
    }
    if (aLength < 0) {
      break;
    }

    // Change selection range before replacing. For content editable element,
    // searching the node for setting selection range is not needed when the
    // selection is collapsed within a text node.
    bool fastPathHit = false;
    if (!isPlainTextField(activeElement)) {
      nsCOMPtr<Document> document = activeElement->GetComposedDoc();
      nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
      RefPtr<Selection> selection = window->GetSelection();
      RefPtr<nsINode> anchorNode = selection->GetAnchorNode();
      if (selection->IsCollapsed() && anchorNode &&
          anchorNode->NodeType() == nsINode::TEXT_NODE) {
        start = selection->AnchorOffset() + aOffset;
        end = start + aLength;
        // Fallback to setSelectionRange() if the replacement span multiple
        // nodes.
        nsAutoString textContent;
        ErrorResult eRv;
        anchorNode->GetTextContent(textContent, eRv);
        if (NS_WARN_IF(eRv.Failed())) {
          break;
        }
        if (start >= 0 && end <= static_cast<int32_t>(textContent.Length())) {
          fastPathHit = true;
          selection->CollapseInLimiter(anchorNode, start);
          selection->Extend(anchorNode, end);
        }
      }
    }
    if (!fastPathHit) {
      start = (int32_t)getSelectionStart(activeElement) + aOffset;
      if (start < 0) {
        start = 0;
      }
      end = start + aLength;
      if (start != (int32_t)getSelectionStart(activeElement) ||
          end != (int32_t)getSelectionEnd(activeElement)) {
        if (!setSelectionRange(activeElement, start, end)) {
          IME_LOGD("-- GeckoEditableSupport::ReplaceSurroundingText Failed.");
          break;
        }
      }
    }

    if (aLength) {
      // Delete the selected text.
      editor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
    }
    if (!aText.IsVoid()) {
      //// We don't use CR but LF
      //// see https://bugzilla.mozilla.org/show_bug.cgi?id=902847
      nsString text(aText);
      text.ReplaceSubstring(u"\r", u"\n");
      //// Insert the text to be replaced with.
      editor->InsertText(text);
    }
    rv = NS_OK;
  } while (0);

  if (aListener) {
    aListener->OnReplaceSurroundingText(aId, rv);
  }
  return rv;
}

nsresult GeckoEditableSupport::GetFocusInputContextBag(
    nsInputContext* aInputContext, Element* aFocusedElement) {
  if (!aFocusedElement) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<HTMLInputElement> inputElement =
      HTMLInputElement::FromNodeOrNull(aFocusedElement);
  RefPtr<HTMLTextAreaElement> textArea =
      HTMLTextAreaElement::FromNodeOrNull(aFocusedElement);
  nsAutoString attributeValue;

  // type
  aFocusedElement->GetTagName(attributeValue);
  aInputContext->SetType(attributeValue);
  IME_LOGD("InputContext: type:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // inputType
  aFocusedElement->GetAttribute(u"type"_ns, attributeValue);
  aInputContext->SetInputType(attributeValue);
  IME_LOGD("InputContext: inputType:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // value
  if (inputElement) {
    inputElement->GetValue(attributeValue, CallerType::NonSystem);
  } else if (textArea) {
    textArea->GetValue(attributeValue);
  } else {
    attributeValue = EmptyString();
  }
  aInputContext->SetValue(attributeValue);
  IME_LOGD("InputContext: value:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // max Using string since if inputType is date then max could be string.
  aFocusedElement->GetAttribute(u"max"_ns, attributeValue);
  aInputContext->SetMax(attributeValue);
  IME_LOGD("InputContext: max:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // min Same as max
  aFocusedElement->GetAttribute(u"min"_ns, attributeValue);
  aInputContext->SetMin(attributeValue);
  IME_LOGD("InputContext: min:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // lang
  aFocusedElement->GetAttribute(u"lang"_ns, attributeValue);
  aInputContext->SetLang(attributeValue);
  IME_LOGD("InputContext: lang:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // inputMode
  aFocusedElement->GetAttribute(u"x-inputmode"_ns, attributeValue);
  aInputContext->SetInputMode(attributeValue);
  IME_LOGD("InputContext: inputMode:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // voiceInputSupported
  bool supported =
      isVoiceInputSupported(aFocusedElement, mVoiceInputExcludedXInputModes,
                            mVoiceInputSupportedTypes);
  aFocusedElement->SetAttribute(u"voice-input-supported"_ns,
                                supported ? u"true"_ns : u"false"_ns,
                                IgnoreErrors());
  aInputContext->SetVoiceInputSupported(supported);
  IME_LOGD("InputContext: voiceInputSupported:[%s]",
           supported ? "true" : "false");

  // name
  aFocusedElement->GetAttribute(u"name"_ns, attributeValue);
  aInputContext->SetName(attributeValue);
  IME_LOGD("InputContext: name:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  // selectionStart
  uint32_t start = getSelectionStart(aFocusedElement);
  aInputContext->SetSelectionStart(start);
  IME_LOGD("InputContext: selectionStart:[%lu]", start);

  // selectionEnd
  uint32_t end = getSelectionEnd(aFocusedElement);
  aInputContext->SetSelectionEnd(end);
  IME_LOGD("InputContext: selectionEnd:[%lu]", end);

  // maxLength
  aFocusedElement->GetAttribute(u"maxlength"_ns, attributeValue);
  aInputContext->SetMaxLength(attributeValue);
  IME_LOGD("InputContext: maxlength:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get());

  aFocusedElement->GetAttribute(u"imegroup"_ns, attributeValue);
  aInputContext->SetImeGroup(attributeValue);
  aInputContext->SetLastImeGroup(mLastImeGroup);
  IME_LOGD("InputContext: imegroup:[%s] lastimegroup:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get(),
           NS_ConvertUTF16toUTF8(mLastImeGroup).get());

  // Treat contenteditable element as a special text area field
  if (EditableUtils::isContentEditable(aFocusedElement)) {
    aInputContext->SetType(u"contenteditable"_ns);
    aInputContext->SetInputType(u"textarea"_ns);
    nsresult rv = getContentEditableText(aFocusedElement, attributeValue);
    if (rv != NS_OK) {
      return rv;
    }
    aInputContext->SetValue(attributeValue);
    IME_LOGD("InputContext: value:[%s]",
             NS_ConvertUTF16toUTF8(attributeValue).get());
  }

  // Choices
  RefPtr<HTMLSelectElement> selectElement =
      HTMLSelectElement::FromNodeOrNull(aFocusedElement);
  if (!selectElement) {
    return NS_OK;
  }

  RefPtr<nsInputContextChoices> inputContextChoices =
      new nsInputContextChoices();
  nsTArray<RefPtr<nsIInputContextChoicesInfo>> choices;
  choices.Clear();

  bool isMultiple = selectElement->Multiple();
  inputContextChoices->SetMultiple(isMultiple);
  IME_LOGD("HTMLSelectElement: multiple:[%s]", isMultiple ? "true" : "false");
  nsCOMPtr<nsINodeList> children;
  children = aFocusedElement->ChildNodes();
  for (uint32_t i = 0; i < children->Length(); i++) {
    nsCOMPtr<nsINode> child = children->Item(i);
    RefPtr<HTMLOptGroupElement> optGroupElement =
        HTMLOptGroupElement::FromNodeOrNull(child);
    RefPtr<HTMLOptionElement> optElement =
        HTMLOptionElement::FromNodeOrNull(child);
    if (optGroupElement) {
      RefPtr<nsInputContextChoicesInfo> groupInfo =
          new nsInputContextChoicesInfo();
      groupInfo->SetGroup(true);
      IME_LOGD("groupInfo: group:[true]");
      optGroupElement->GetAttribute(u"label"_ns, attributeValue);
      groupInfo->SetLabel(attributeValue);
      IME_LOGD("groupInfo: label:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      groupInfo->SetDisabled(optGroupElement->Disabled());
      IME_LOGD("groupInfo: disabled:[%s]",
               optGroupElement->Disabled() ? "true" : "false");
      choices.AppendElement(groupInfo);
      nsCOMPtr<nsINodeList> subChildren;
      subChildren = optGroupElement->ChildNodes();
      for (uint32_t j = 0; j < subChildren->Length(); j++) {
        nsCOMPtr<nsINode> subChild = subChildren->Item(j);
        RefPtr<HTMLOptionElement> subOptElement =
            HTMLOptionElement::FromNodeOrNull(subChild);
        if (!subOptElement) {
          continue;
        }
        RefPtr<nsInputContextChoicesInfo> subOptInfo =
            new nsInputContextChoicesInfo();
        subOptInfo->SetGroup(false);
        IME_LOGD("subOptInfo: group:[false]");
        subOptInfo->SetInGroup(true);
        IME_LOGD("subOptInfo: inGroup:[true]");
        subOptElement->GetAttribute(u"label"_ns, attributeValue);
        subOptInfo->SetLabel(attributeValue);
        IME_LOGD("subOptInfo: label:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptElement->GetAttribute(u"value"_ns, attributeValue);
        subOptInfo->SetValue(attributeValue);
        IME_LOGD("subOptInfo: value:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptElement->GetText(attributeValue);
        subOptInfo->SetText(attributeValue);
        IME_LOGD("subOptInfo: text:[%s]",
                 NS_ConvertUTF16toUTF8(attributeValue).get());
        subOptInfo->SetDisabled(subOptElement->Disabled());
        IME_LOGD("subOptInfo: Disabled:[%s]",
                 subOptElement->Disabled() ? "true" : "false");
        subOptInfo->SetSelected(subOptElement->Selected());
        IME_LOGD("subOptInfo: Selected:[%s]",
                 subOptElement->Selected() ? "true" : "false");
        subOptInfo->SetDefaultSelected(subOptElement->DefaultSelected());
        IME_LOGD("subOptInfo: DefaultSelected:[%s]",
                 subOptElement->DefaultSelected() ? "true" : "false");
        int32_t subIndex = subOptElement->Index();
        subOptInfo->SetOptionIndex(subIndex);
        IME_LOGD("subOptInfo: optionIndex:[%ld]", subIndex);
        choices.AppendElement(subOptInfo);
      }
    } else if (optElement) {
      RefPtr<nsInputContextChoicesInfo> optInfo =
          new nsInputContextChoicesInfo();
      optInfo->SetGroup(false);
      IME_LOGD("optInfo: group:[false]");
      optInfo->SetInGroup(false);
      IME_LOGD("optInfo: inGroup:[false]");
      optElement->GetAttribute(u"label"_ns, attributeValue);
      optInfo->SetLabel(attributeValue);
      IME_LOGD("optInfo: label:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optElement->GetAttribute(u"value"_ns, attributeValue);
      optInfo->SetValue(attributeValue);
      IME_LOGD("optInfo: value:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optElement->GetText(attributeValue);
      optInfo->SetText(attributeValue);
      IME_LOGD("optInfo: text:[%s]",
               NS_ConvertUTF16toUTF8(attributeValue).get());
      optInfo->SetDisabled(optElement->Disabled());
      IME_LOGD("optInfo: Disabled:[%s]",
               optElement->Disabled() ? "true" : "false");
      optInfo->SetSelected(optElement->Selected());
      IME_LOGD("optInfo: Selected:[%s]",
               optElement->Selected() ? "true" : "false");
      optInfo->SetDefaultSelected(optElement->DefaultSelected());
      IME_LOGD("optInfo: DefaultSelected:[%s]",
               optElement->DefaultSelected() ? "true" : "false");
      int32_t optionIndex = optElement->Index();
      optInfo->SetOptionIndex(optionIndex);
      IME_LOGD("optInfo: optionIndex:[%ld]", optionIndex);
      choices.AppendElement(optInfo);
    }
  }
  inputContextChoices->SetChoices(choices);
  aInputContext->SetInputContextChoices(inputContextChoices);
  aInputContext->SetEditableSupport(this);
  return NS_OK;
}

void GeckoEditableSupport::GetBlurInputContextBag(nsInputContext* aInputContext,
                                                  Element* aFocusedElement) {
  nsAutoString attributeValue(u""_ns);
  if (aFocusedElement) {
    aFocusedElement->GetAttribute(u"imegroup"_ns, attributeValue);
  }
  aInputContext->SetImeGroup(attributeValue);
  aInputContext->SetLastImeGroup(mLastImeGroup);
  IME_LOGD("InputContext: imegroup:[%s] lastimegroup:[%s]",
           NS_ConvertUTF16toUTF8(attributeValue).get(),
           NS_ConvertUTF16toUTF8(mLastImeGroup).get());
}

void GeckoEditableSupport::ContentRemoved(nsIContent* aChild,
                                          nsIContent* aPreviousSibling) {
  RefPtr<Element> ele = do_QueryReferent(mFocusedTarget);
  if (ele && ele->IsShadowIncludingInclusiveDescendantOf(aChild)) {
    // When focusing on a content editable element and removing it from the DOM
    // tree, we won't receive the blur event. We have to blur IME here. Can't
    // handle it with NOTIFY_IME_OF_BLUR because it also is triggered when
    // dispatching focus/blur events and we need the correct related target
    // to fetch the 'imegroup' attribute.
    HandleBlur(nullptr);
  }
}

}  // namespace widget
}  // namespace mozilla
