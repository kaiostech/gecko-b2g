/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputMethodServiceChild.h"

#include "mozilla/dom/ContentChild.h"
#include "nsIEditableSupport.h"
#include "mozilla/dom/IMELog.h"
namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(InputMethodServiceChild, nsIEditableSupportListener)

InputMethodServiceChild::InputMethodServiceChild() {
  IME_LOGD("InputMethodServiceChild::Constructor[%p]", this);
}

InputMethodServiceChild::~InputMethodServiceChild() {
  IME_LOGD("InputMethodServiceChild::Destructor[%p]", this);
}

}  // namespace dom
}  // namespace mozilla
