/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardEventForwarderParent.h"
#include "KeyboardAppProxy.h"
#include "mozilla/dom/BrowserParent.h"

extern mozilla::LazyLogModule gKeyboardAppProxyLog;

using namespace mozilla::dom;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(KeyboardEventForwarderParent, nsIKeyboardEventForwarder)

KeyboardEventForwarderParent::KeyboardEventForwarderParent() {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderParent::Constructor[%p]", this));
}

KeyboardEventForwarderParent::~KeyboardEventForwarderParent() {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderParent::Destructor[%p]", this));
}

IPCResult KeyboardEventForwarderParent::RecvResponse(
    const KeyResponse& aResponse) {
  RefPtr<nsIKeyboardAppProxy> proxy = KeyboardAppProxy::GetInstance();
  proxy->ReplyKey(aResponse.eventType(), aResponse.defaultPrevented(),
                  aResponse.generation());
  return IPC_OK();
}

NS_IMETHODIMP
KeyboardEventForwarderParent::OnKeyboardEventReceived(
    const nsACString& aEventType, uint32_t aKeyCode, uint32_t aCharCode,
    const nsACString& aKey, uint64_t aTimeStamp, uint64_t aGeneration) {
  Unused << SendKey(KeyRequest(nsAutoCString(aEventType), aKeyCode, aCharCode,
                               nsAutoCString(aKey), aTimeStamp, aGeneration));
  return NS_OK;
}

NS_IMETHODIMP
KeyboardEventForwarderParent::OnTextChanged(const nsACString& aText) {
  Unused << SendTextChanged(nsAutoCString(aText));
  return NS_OK;
}

NS_IMETHODIMP
KeyboardEventForwarderParent::OnSelectionChanged(uint32_t aStartOffset, uint32_t aEndOffset) {
  Unused << SendSelectionChanged(aStartOffset, aEndOffset);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
