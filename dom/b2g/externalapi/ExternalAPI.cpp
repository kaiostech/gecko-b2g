/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalAPI.h"
#include "mozilla/dom/ExternalAPIBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "nsIPermission.h"
#include "nsIPermissionManager.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(ExternalAPI::SidlDefaultResponse)

NS_IMETHODIMP
ExternalAPI::SidlDefaultResponse::Resolve() {
  if (mExternalApi) {
    nsresult rv = mExternalApi->Resolve();
    mExternalApi = nullptr;
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
ExternalAPI::SidlDefaultResponse::Reject() {
  if (mExternalApi) {
    nsresult rv = mExternalApi->Reject();
    mExternalApi = nullptr;
    return rv;
  }
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExternalAPI, mGlobal, mPromises)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExternalAPI)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExternalAPI)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExternalAPI)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class PrepareMessageRunnable : public WorkerMainThreadRunnable {
  nsACString& mOrigin;
  nsACString& mToken;
  Sequence<nsString>& mPermissions;

 public:
  PrepareMessageRunnable(WorkerPrivate* aWorkerPrivate, nsACString& aOrigin,
                         nsACString& aToken, Sequence<nsString>& aPermissions)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "ExternalAPI :: Prepare Message"_ns),
        mOrigin(aOrigin),
        mToken(aToken),
        mPermissions(aPermissions) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      // Walk up to our containing page
      WorkerPrivate* wp = mWorkerPrivate;
      while (wp->GetParent()) {
        wp = wp->GetParent();
      }

      principal = wp->GetPrincipal();
    }

    ExternalAPI::GetOrigin(principal, mOrigin);
    ExternalAPI::GetPermissions(principal, mPermissions);
    ExternalAPI::GenerateToken(mToken);

    return true;
  }
};

ExternalAPI::ExternalAPI(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  MOZ_ASSERT(mGlobal);
  mBridge = do_GetService("@mozilla.org/sidl-native/bridge;1");
}

ExternalAPI::~ExternalAPI() {
  NS_ASSERT_OWNINGTHREAD(ExternalAPI);

  for (uint32_t i = 0; i < mPromises.Length(); ++i) {
    mPromises[i]->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  mPromises.Clear();
  mTokens.Clear();
}

nsresult ExternalAPI::Resolve() {
  // api-daemon successful response:
  // pop a promise from the stack(mPromises) and resolve it with a token,
  // which also poped from a stack of tokens(mTokens).
  if (mPromises.IsEmpty() || mTokens.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  mPromises[0]->MaybeResolve(NS_ConvertUTF8toUTF16(mTokens[0]));
  mPromises.RemoveElementAt(0);
  mTokens.RemoveElementAt(0);
  return NS_OK;
}

nsresult ExternalAPI::Reject() {
  // api-daemon error response: reject a promise if possible.
  PopAndRejectPromise();
  if (!mTokens.IsEmpty()) {
    mTokens.RemoveElementAt(0);
  }

  return NS_OK;
}

/* static */ already_AddRefed<ExternalAPI> ExternalAPI::Create(
    nsIGlobalObject* aGlobal) {
  RefPtr<ExternalAPI> external = new ExternalAPI(aGlobal);
  return external.forget();
}

already_AddRefed<Promise> ExternalAPI::GetToken() {
  NS_ASSERT_OWNINGTHREAD(ExternalAPI);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return nullptr;
  }

  rv = SendMessage();

  if (NS_WARN_IF(rv.Failed())) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  mPromises.AppendElement(promise);

  return promise.forget();
}

nsISupports* ExternalAPI::GetParentObject() const { return mGlobal; }

JSObject* ExternalAPI::WrapObject(JSContext* aContext,
                                  JS::Handle<JSObject*> aGivenProto) {
  return ExternalAPI_Binding::Wrap(aContext, this, aGivenProto);
}

/* static */ void ExternalAPI::GenerateToken(nsACString& aToken) {
  MOZ_ASSERT(NS_IsMainThread());

  nsID uuid;
  char uuidBuffer[NSID_LENGTH];
  nsCString asciiString;
  ErrorResult rv;

  rv = nsID::GenerateUUIDInPlace(uuid);
  if (rv.Failed()) {
    aToken.AssignLiteral("");
    return;
  }

  uuid.ToProvidedString(uuidBuffer);
  asciiString.AssignASCII(uuidBuffer);

  // Remove {} and the null terminator
  aToken.Assign(Substring(asciiString, 1, NSID_LENGTH - 3));
}

/* static */ void ExternalAPI::GetOrigin(nsIPrincipal* aPrincipal,
                                         nsACString& aOrigin) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aPrincipal) {
    return;
  }

  ErrorResult rv;
  bool isNullPrincipal;
  rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
  if (NS_WARN_IF(rv.Failed()) || NS_WARN_IF(isNullPrincipal)) {
    return;
  }

  rv = aPrincipal->GetOriginNoSuffix(aOrigin);
}

/* static */ void ExternalAPI::GetPermissions(
    nsIPrincipal* aPrincipal, Sequence<nsString>& aPermissions) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aPrincipal) {
    return;
  }

  nsCOMPtr<nsIPermissionManager> permManager = services::GetPermissionManager();
  if (!permManager) {
    return;
  }

  // The permission set for the calling principal.
  nsTArray<RefPtr<nsIPermission>> permissions;

  if (aPrincipal->IsSystemPrincipal()) {
    // If this is the system principal the call was initiated by the system app.
    // In that situation we use a content principal for the system app url
    // instead to match with the PermissionInstaller code.
    nsAutoCString systemAppUrl;
    if (NS_FAILED(
            Preferences::GetCString("b2g.system_startup_url", systemAppUrl))) {
      // Should never happen...
      printf_stderr("Failed to retrieve value of b2g.system_startup_url !!\n");
      return;
    };

    nsCOMPtr<nsIPrincipal> contentPrincipal =
        BasePrincipal::CreateContentPrincipal(systemAppUrl);
    if (!contentPrincipal) {
      printf_stderr(
          "Failed to create content principal for the system app !!\n");
      return;
    }

    if (NS_FAILED(
            permManager->GetAllForPrincipal(contentPrincipal, permissions))) {
      return;
    }
  } else {
    if (NS_FAILED(permManager->GetAllForPrincipal(aPrincipal, permissions))) {
      return;
    }
  }

  // Add all the permissions granted to this principal.
  // TODO: filter only the ones that are managed by api-daemon services.
  for (const auto& permission : permissions) {
    if (!permission) {
      continue;
    }

    nsAutoCString permissionType;
    if (NS_FAILED(permission->GetType(permissionType))) {
      continue;
    }

    uint32_t capability = 0;
    if (NS_SUCCEEDED(permission->GetCapability(&capability)) &&
        capability == nsIPermissionManager::ALLOW_ACTION) {
      if (!aPermissions.AppendElement(NS_ConvertUTF8toUTF16(permissionType),
                                      fallible)) {
        // Allocation failure...
        return;
      }
    }
  }
}

nsresult ExternalAPI::SendMessage() {
  NS_ASSERT_OWNINGTHREAD(ExternalAPI);

  nsAutoCString token;
  nsAutoCString origin;
  Sequence<nsString> permissions;
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIPrincipal> principal = mGlobal->PrincipalOrNull();
    GetOrigin(principal, origin);
    GetPermissions(principal, permissions);
    GenerateToken(token);
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

    ErrorResult rv;
    RefPtr<PrepareMessageRunnable> runnable =
        new PrepareMessageRunnable(workerPrivate, origin, token, permissions);
    runnable->Dispatch(dom::WorkerStatus::Canceling, rv);

    if (rv.Failed()) {
      return NS_ERROR_FAILURE;
    }
  }

  if (token.IsEmpty() || origin.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mBridge->RegisterToken(
      NS_ConvertUTF8toUTF16(token), NS_ConvertUTF8toUTF16(origin), permissions,
      new ExternalAPI::SidlDefaultResponse(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  mTokens.AppendElement(token);

  return NS_OK;
}

void ExternalAPI::PopAndRejectPromise() {
  if (!mPromises.IsEmpty()) {
    mPromises[0]->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    mPromises.RemoveElementAt(0);
  }
}

}  // namespace dom
}  // namespace mozilla
