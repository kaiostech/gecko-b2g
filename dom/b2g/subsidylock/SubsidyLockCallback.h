/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_subsidy_SubsidyLockCallback_h
#define mozilla_dom_subsidy_SubsidyLockCallback_h

#include "mozilla/dom/DOMRequest.h"
#include "nsCOMPtr.h"
#include "nsISubsidyLockService.h"

namespace mozilla {
namespace dom {
namespace subsidylock {

class SubsidyLockCallback final : public nsISubsidyLockCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUBSIDYLOCKCALLBACK

  SubsidyLockCallback(nsPIDOMWindowInner* aWindow, DOMRequest* aRequest);

 private:
  ~SubsidyLockCallback() {}

  nsresult NotifySuccess(JS::Handle<JS::Value> aResult);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<DOMRequest> mRequest;
};

}  // namespace subsidylock
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_subsidy_SubsidyLockCallback_h
