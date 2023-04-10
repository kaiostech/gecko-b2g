/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodServiceCommon_h
#define mozilla_dom_InputMethodServiceCommon_h

#include "nsIEditableSupport.h"
#include "EditableSupportListenerCommon.h"

#include "mozilla/dom/IMELog.h"

namespace mozilla {
namespace dom {

template <class T>
class InputMethodServiceCommon : public EditableSupportListenerCommon<T> {
 public:
  // May be the real 'EditableSupport' object (on the content process)
  // Or the helper object which forwards the requests (on the chrome process)
  virtual nsIEditableSupport* GetEditableSupport() = 0;

  // Listener may be the requester which should be informed (on the content
  // process). Or the helper object which forwards the responses (on the chrome
  // process).
  nsTHashMap<nsUint64HashKey, RefPtr<nsIEditableSupportListener>> mRequestMap;

  void SetEditableSupportListener(uint64_t aId,
                                  nsIEditableSupportListener* aListener) {
    mRequestMap.InsertOrUpdate(aId, aListener);
  }

  nsIEditableSupportListener* GetEditableSupportListener(uint64_t aId) {
    return mRequestMap.Lookup(aId).Data();
  }

 protected:
  mozilla::ipc::IPCResult RecvRequest(const InputMethodRequest& aRequest) {
    switch (aRequest.type()) {
      case InputMethodRequest::TSetCompositionRequest: {
        const SetCompositionRequest& request = aRequest;

        GetEditableSupport()->SetComposition(request.id(), this, request.text(),
                                             request.offset(),
                                             request.length());
        break;
      }
      case InputMethodRequest::TEndCompositionRequest: {
        const EndCompositionRequest& request = aRequest;
        GetEditableSupport()->EndComposition(request.id(), this,
                                             request.text());
        break;
      }
      case InputMethodRequest::TKeydownRequest: {
        const KeydownRequest& request = aRequest;
        GetEditableSupport()->Keydown(request.id(), this, request.key());
        break;
      }
      case InputMethodRequest::TKeyupRequest: {
        const KeyupRequest& request = aRequest;
        GetEditableSupport()->Keyup(request.id(), this, request.key());
        break;
      }
      case InputMethodRequest::TSendKeyRequest: {
        const SendKeyRequest& request = aRequest;
        GetEditableSupport()->SendKey(request.id(), this, request.key());
        break;
      }
      case InputMethodRequest::TDeleteBackwardRequest: {
        const DeleteBackwardRequest& request = aRequest;
        GetEditableSupport()->DeleteBackward(request.id(), this);
        break;
      }
      case InputMethodRequest::TSetSelectedOptionRequest: {
        const SetSelectedOptionRequest& request = aRequest;
        GetEditableSupport()->SetSelectedOption(request.id(), this,
                                                request.optionIndex());
        break;
      }
      case InputMethodRequest::TSetSelectedOptionsRequest: {
        const SetSelectedOptionsRequest& request = aRequest;
        GetEditableSupport()->SetSelectedOptions(request.id(), this,
                                                 request.optionIndexes());
        break;
      }
      case InputMethodRequest::TCommonRequest: {
        const CommonRequest& request = aRequest;
        if (request.requestName() == u"RemoveFocus"_ns) {
          GetEditableSupport()->RemoveFocus(request.id(), this);
        } else if (request.requestName() == u"ClearAll"_ns) {
          GetEditableSupport()->ClearAll(request.id(), this);
        }
        break;
      }
      case InputMethodRequest::TGetSelectionRangeRequest: {
        const GetSelectionRangeRequest& request = aRequest;
        GetEditableSupport()->GetSelectionRange(request.id(), this);
        break;
      }
      case InputMethodRequest::TGetTextRequest: {
        const GetTextRequest& request = aRequest;
        GetEditableSupport()->GetText(request.id(), this, request.offset(),
                                      request.length());
        break;
      }
      case InputMethodRequest::TSetValueRequest: {
        const SetValueRequest& request = aRequest;
        GetEditableSupport()->SetValue(request.id(), this, request.value());
        break;
      }
      case InputMethodRequest::TReplaceSurroundingTextRequest: {
        const ReplaceSurroundingTextRequest& request = aRequest;
        GetEditableSupport()->ReplaceSurroundingText(
            request.id(), this, request.text(), request.offset(),
            request.length());
        break;
      }
      default:
        return IPC_FAIL(
            this, "InputMethodServiceCommon RecvRequest unknown request type.");
    }

    return IPC_OK();
  }

  mozilla::ipc::IPCResult RecvResponse(const InputMethodResponse& aResponse) {
    uint64_t id = 0;
    switch (aResponse.type()) {
      case InputMethodResponse::TSetCompositionResponse: {
        const SetCompositionResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnSetComposition(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TEndCompositionResponse: {
        const EndCompositionResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnEndComposition(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TKeydownResponse: {
        const KeydownResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnKeydown(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TKeyupResponse: {
        const KeyupResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnKeyup(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TSendKeyResponse: {
        const SendKeyResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnSendKey(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TDeleteBackwardResponse: {
        const DeleteBackwardResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnDeleteBackward(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TSetSelectedOptionResponse: {
        const SetSelectedOptionResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnSetSelectedOption(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TSetSelectedOptionsResponse: {
        const SetSelectedOptionsResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnSetSelectedOptions(id, response.status());
        }
        break;
      }
      case InputMethodResponse::TCommonResponse: {
        const CommonResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          if (response.responseName() == u"RemoveFocus"_ns) {
            listener->OnRemoveFocus(id, response.status());
          } else if (response.responseName() == u"SetValue"_ns) {
            listener->OnSetValue(id, response.status());
          } else if (response.responseName() == u"ClearAll"_ns) {
            listener->OnClearAll(id, response.status());
          } else if (response.responseName() == u"ReplaceSurroundingText"_ns) {
            listener->OnReplaceSurroundingText(id, response.status());
          }
        }
        break;
      }
      case InputMethodResponse::TGetSelectionRangeResponse: {
        const GetSelectionRangeResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(response.id())) {
          listener->OnGetSelectionRange(response.id(), response.status(),
                                        response.start(), response.end());
        }
        break;
      }
      case InputMethodResponse::TGetTextResponse: {
        const GetTextResponse& response = aResponse;
        id = response.id();
        if (RefPtr<nsIEditableSupportListener> listener =
                GetEditableSupportListener(id)) {
          listener->OnGetText(id, response.status(), response.text());
        }
        break;
      }
      default: {
        return IPC_FAIL(
            this,
            "InputMethodServiceCommon RecvResponse unknown response type.");
      }
    }

    mRequestMap.Remove(id);

    return IPC_OK();
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethodServiceCommon_h
