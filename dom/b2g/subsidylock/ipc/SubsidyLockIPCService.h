/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_subsidy_SubsidyLockIPCService_h
#define mozilla_dom_subsidy_SubsidyLockIPCService_h

#include "nsCOMPtr.h"
#include "nsISubsidyLockService.h"

namespace mozilla {
namespace dom {
namespace subsidylock {

class SubsidyLockChild;

class SubsidyLockIPCService final : public nsISubsidyLockService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUBSIDYLOCKSERVICE

  SubsidyLockIPCService();

 private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~SubsidyLockIPCService();

  nsTArray<RefPtr<SubsidyLockChild>> mItems;
};

}  // namespace subsidylock
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_subsidy_SubsidyLockIPCService_h
