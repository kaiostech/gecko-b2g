/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputMethodServiceParent_h
#define mozilla_dom_InputMethodServiceParent_h

#include "mozilla/dom/PInputMethodServiceParent.h"
#include "InputMethodServiceCommon.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

class InputMethodServiceParent final
    : public InputMethodServiceCommon<PInputMethodServiceParent>,
      public nsIEditableSupport {
  friend class PInputMethodServiceParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITABLESUPPORT

  InputMethodServiceParent();

  nsIEditableSupport* GetEditableSupport() override;

 protected:
  IPCResult RecvRequest(const InputMethodRequest& aRequest);
  IPCResult RecvResponse(const InputMethodResponse& aResponse) {
    IPCResult result =
        InputMethodServiceCommon<PInputMethodServiceParent>::RecvResponse(
            aResponse);
    return result;
  }
  void ActorDestroy(ActorDestroyReason why) override;

 private:
  ~InputMethodServiceParent();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InputMethodServiceParent_h
