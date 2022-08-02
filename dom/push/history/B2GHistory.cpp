/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "B2GHistory.h"

#include "nsIURI.h"
#include "nsIPushService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla;
B2GHistory::B2GHistory() {}

B2GHistory::~B2GHistory() {}

NS_IMPL_ISUPPORTS(B2GHistory, IHistory)

StaticRefPtr<B2GHistory> B2GHistory::sHistory;

/* static */
already_AddRefed<B2GHistory> B2GHistory::GetSingleton() {
  if (!sHistory) {
    sHistory = new B2GHistory();
    ClearOnShutdown(&sHistory);
  }
  RefPtr<B2GHistory> history = sHistory;
  return history.forget();
}

void B2GHistory::StartPendingVisitedQueries(PendingVisitedQueries&& aQueries) {
  return;
}

NS_IMETHODIMP
B2GHistory::VisitURI(nsIWidget* aWidget, nsIURI* aURI, nsIURI* aLastVisitedURI,
                     uint32_t aFlags, uint64_t aBrowserId) {
  nsAutoCString documentURI;
  aURI->GetSpec(documentURI);
  if (documentURI.EqualsASCII("about:blank")) {
    return NS_OK;
  }

  nsCOMPtr<nsIPushQuotaManager> pushQuotaManager =
      do_GetService("@mozilla.org/push/Service;1");

  if (!pushQuotaManager) {
    return NS_ERROR_FAILURE;
  }

  return pushQuotaManager->VisitURI(aURI);
}

NS_IMETHODIMP
B2GHistory::SetURITitle(nsIURI* aURI, const nsAString& aTitle) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
