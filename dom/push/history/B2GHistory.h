/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef B2GHISTORY_H
#define B2GHISTORY_H

#include "mozilla/BaseHistory.h"
#include "mozilla/StaticPtr.h"
#include "nsIURI.h"

class B2GHistory final : public mozilla::BaseHistory {
 public:
  NS_DECL_ISUPPORTS

  // IHistory
  NS_IMETHOD VisitURI(nsIWidget*, nsIURI*, nsIURI* aLastVisitedURI,
                      uint32_t aFlags, uint64_t aBrowserId) final;
  NS_IMETHOD SetURITitle(nsIURI*, const nsAString&) final;

  // BaseHistory
  void StartPendingVisitedQueries(PendingVisitedQueries&&) final;

  static already_AddRefed<B2GHistory> GetSingleton();

  B2GHistory();

 private:
  virtual ~B2GHistory();

  static mozilla::StaticRefPtr<B2GHistory> sHistory;
};
#endif
