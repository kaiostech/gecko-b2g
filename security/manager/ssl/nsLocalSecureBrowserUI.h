/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLocalSecureBrowserUI_h
#define nsLocalSecureBrowserUI_h

#include "nsISecureBrowserUI.h"
#include "nsWeakReference.h"
#include "mozilla/dom/BrowsingContext.h"

class nsLocalSecureBrowserUI : public nsISecureBrowserUI,
                               public nsSupportsWeakReference {
 public:
  explicit nsLocalSecureBrowserUI(
      mozilla::dom::BrowsingContext* aBrowsingContext);
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECUREBROWSERUI

  void RecomputeSecurityFlags();

 protected:
  virtual ~nsLocalSecureBrowserUI() = default;
  uint32_t mState;
  uint64_t mBrowsingContextId;
};

#endif  // nsLocalSecureBrowserUI_h
