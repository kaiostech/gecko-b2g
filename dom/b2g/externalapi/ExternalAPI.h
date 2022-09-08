/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ExternalAPI_h
#define mozilla_dom_ExternalAPI_h

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ExternalAPIBinding.h"
#include "nsIGeckoBridge.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;
class WebSocket;
class MessageEvent;
class PrepareMessageRunnable;
class GetWebSocketServerInfoRunnable;

class ExternalAPI final : public nsISupports,
                          public SupportsWeakPtr,
                          public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ExternalAPI)

  static already_AddRefed<ExternalAPI> Create(nsIGlobalObject* aGlobal);

  // webidl
  already_AddRefed<Promise> GetToken();

  // binding methods
  nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aContext,
                               JS::Handle<JSObject*> aGivenProto) override;

  friend PrepareMessageRunnable;
  friend GetWebSocketServerInfoRunnable;

 private:
  class SidlDefaultResponse final : public nsISidlDefaultResponse {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIDLDEFAULTRESPONSE

    SidlDefaultResponse(ExternalAPI* aExternalAPI)
        : mExternalApi(aExternalAPI) {}

   private:
    ~SidlDefaultResponse() = default;
    WeakPtr<ExternalAPI> mExternalApi;
  };

  explicit ExternalAPI(nsIGlobalObject* aGlobal);
  ~ExternalAPI();

  static void GenerateToken(nsACString& aToken);
  static void GetOrigin(nsIPrincipal* aPrincipal, nsACString& aOrigin);
  static void GetPermissions(nsIPrincipal* aPrincipal,
                             Sequence<nsString>& aPermissions);
  nsresult SendMessage();
  void PopAndRejectPromise();
  nsresult Resolve();
  nsresult Reject();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIGeckoBridge> mBridge;
  nsTArray<RefPtr<Promise>> mPromises;
  nsTArray<nsCString> mTokens;
};

}  // namespace dom
}  // namespace mozilla
#endif
