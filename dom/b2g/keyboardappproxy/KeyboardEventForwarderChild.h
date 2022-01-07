/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyboardEventForwarderChild_h
#define mozilla_dom_KeyboardEventForwarderChild_h

#include "mozilla/dom/PKeyboardEventForwarderChild.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

class KeyboardEventForwarderChild final : public PKeyboardEventForwarderChild {
 public:
  KeyboardEventForwarderChild();
  IPCResult RecvKey(const KeyRequest& aEvent);
  IPCResult RecvTextChanged(const nsCString& aText);
  IPCResult RecvSelectionChanged(uint32_t aStartOffset, uint32_t aEndOffset);

 private:
  ~KeyboardEventForwarderChild();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_KeyboardEventForwarderChild_h
