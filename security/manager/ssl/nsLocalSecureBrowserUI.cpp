/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLocalSecureBrowserUI.h"

#include "nsDocShell.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/WindowContext.h"

using namespace mozilla::dom;

nsLocalSecureBrowserUI::nsLocalSecureBrowserUI(
    BrowsingContext* aBrowsingContext)
    : mState(0) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // The BrowsingContext will own the nsLocalSecureBrowserUI object, we keep a
  // weak ref.
  mBrowsingContextId = aBrowsingContext->Id();
}

NS_IMPL_ISUPPORTS(nsLocalSecureBrowserUI, nsISecureBrowserUI,
                  nsISupportsWeakReference)

void nsLocalSecureBrowserUI::RecomputeSecurityFlags() {
  // Our BrowsingContext either has a new WindowGlobalParent, or the
  // existing one has mutated its security state.
  // Recompute our security state and fire notifications to listeners

  RefPtr<BrowsingContext> ctx = BrowsingContext::Get(mBrowsingContextId);
  if (!ctx) {
    return;
  }

  RefPtr<WindowContext> windowCtx = ctx->GetCurrentWindowContext();
  if (!windowCtx) {
    return;
  }

  RefPtr<Document> doc = ctx->GetDocument();
  bool isSecure = windowCtx->GetIsSecure();
  // if (doc) {
  //  nsCOMPtr<nsIURI> innerDocURI = NS_GetInnermostURI(doc->GetDocumentURI());
  //  isSecure = innerDocURI && innerDocURI->SchemeIs("https");
  //}

  mState = nsIWebProgressListener::STATE_IS_INSECURE;
  // Only https is considered secure (it is possible to have e.g. an http URI
  // with a channel that has a securityInfo that indicates the connection is
  // secure - e.g. h2/alt-svc or by visiting an http URI over an https proxy).
  if (isSecure) {
    nsCOMPtr<nsISupports> secInfo;
    nsresult rv = NS_OK;
    if (nsIChannel* failedChannel = doc->GetFailedChannel()) {
      rv = failedChannel->GetSecurityInfo(getter_AddRefs(secInfo));
    } else {
      // When there's no failed channel we should have a regular
      // security info on the document. In some cases there's no
      // security info at all, i.e. on HTTP sites.
      secInfo = doc->GetSecurityInfo();
    }
    nsCOMPtr<nsITransportSecurityInfo> securityInfo =
        do_QueryInterface(secInfo);

    if (securityInfo) {
      nsresult rv = securityInfo->GetSecurityState(&mState);

      // If the security state is STATE_IS_INSECURE, the TLS handshake never
      // completed. Don't set any further state.
      if (NS_SUCCEEDED(rv) &&
          mState != nsIWebProgressListener::STATE_IS_INSECURE) {
        bool isEV;
        rv = securityInfo->GetIsExtendedValidation(&isEV);
        if (NS_SUCCEEDED(rv) && isEV) {
          mState |= nsIWebProgressListener::STATE_IDENTITY_EV_TOPLEVEL;
        }
      }
    }
  }

  // Add upgraded-state flags when request has been
  // upgraded with HTTPS-Only Mode
  if (doc) {
    // Check if top-level load has been upgraded
    uint32_t httpsOnlyStatus = doc->HttpsOnlyStatus();
    if (!(httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UNINITIALIZED) &&
        !(httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_EXEMPT)) {
      mState |= nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADED;
    }

    mState |= windowCtx->GetSecurityState();

    // If we have loaded mixed content and this is a secure page,
    // then clear secure flags and add broken instead.
    static const uint32_t kLoadedMixedContentFlags =
        nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT |
        nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;

    if (isSecure && (mState & kLoadedMixedContentFlags)) {
      // reset state security flag
      mState = mState >> 4 << 4;
      // set state security flag to broken, since there is mixed content
      mState |= nsIWebProgressListener::STATE_IS_BROKEN;
    }
  }

  if (ctx->GetDocShell()) {
    nsDocShell* nativeDocShell = nsDocShell::Cast(ctx->GetDocShell());
    nativeDocShell->nsDocLoader::OnSecurityChange(nullptr, mState);
  }
}

NS_IMETHODIMP
nsLocalSecureBrowserUI::GetState(uint32_t* aState) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aState);
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalSecureBrowserUI::GetIsSecureContext(bool* aIsSecureContext) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aIsSecureContext);

  // if (WindowGlobalParent* parent = GetCurrentWindow()) {
  //  *aIsSecureContext = parent->GetIsSecureContext();
  //} else {
  *aIsSecureContext = false;
  //}
  return NS_OK;
}

NS_IMETHODIMP
nsLocalSecureBrowserUI::GetSecInfo(nsITransportSecurityInfo** result) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(result);

  // TBD

  NS_IF_ADDREF(*result);

  return NS_OK;
}
