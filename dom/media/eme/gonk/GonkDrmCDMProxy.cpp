/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmCDMProxy.h"

#include "GonkCryptoProxy.h"
#include "GonkDrmCDMCallbackProxy.h"
#include "GonkDrmSharedData.h"
#include "GonkDrmStorageProxy.h"
#include "GonkDrmSupport.h"
#include "GonkDrmUtils.h"

namespace mozilla {

/* static */
bool GonkDrmCDMProxy::IsSchemeSupported(const nsAString& aKeySystem) {
  return android::GonkDrmUtils::IsSchemeSupported(aKeySystem);
}

GonkDrmCDMProxy::GonkDrmCDMProxy(dom::MediaKeys* aKeys,
                                 const nsAString& aKeySystem,
                                 bool aDistinctiveIdentifierRequired,
                                 bool aPersistentStateRequired)
    : CDMProxy(aKeys, aKeySystem, aDistinctiveIdentifierRequired,
               aPersistentStateRequired),
      mSharedData(new android::GonkDrmSharedData()) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(GonkDrmCDMProxy);
}

GonkDrmCDMProxy::~GonkDrmCDMProxy() { MOZ_COUNT_DTOR(GonkDrmCDMProxy); }

void GonkDrmCDMProxy::Init(PromiseId aPromiseId, const nsAString& aOrigin,
                           const nsAString& aTopLevelOrigin,
                           const nsAString& aName) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("GonkDrmCDMProxy::Init (%s, %s) %s",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
          NS_ConvertUTF16toUTF8(aName).get());

  if (!mOwnerThread) {
    nsresult rv =
        NS_NewNamedThread("GDCDMThread", getter_AddRefs(mOwnerThread));
    if (NS_FAILED(rv)) {
      RejectPromiseWithStateError(
          aPromiseId,
          nsLiteralCString("Couldn't create CDM thread GonkDrmCDMProxy::Init"));
      return;
    }
  }

  mCDM = new android::GonkDrmSupport(mOwnerThread, aOrigin, mKeySystem);
  mCallback = MakeUnique<GonkDrmCDMCallbackProxy>(this);

  mStorage = MakeRefPtr<GonkDrmStorageProxy>(aOrigin, mKeySystem);
  if (!mStorage->Init()) {
    RejectPromiseWithStateError(
        aPromiseId, nsLiteralCString("Couldn't initialize storage"));
    return;
  }

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::Init",
      [aPromiseId, callback = mCallback.get(), cdm = mCDM, storage = mStorage,
       sharedData = mSharedData]() {
        cdm->Init(aPromiseId, callback, storage, sharedData);
      }));
}

void GonkDrmCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                    MediaKeySessionType aSessionType,
                                    PromiseId aPromiseId,
                                    const nsAString& aInitDataType,
                                    nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::CreateSession",
      [aCreateSessionToken, aPromiseId, aSessionType,
       initDataType = NS_ConvertUTF16toUTF8(aInitDataType),
       initData = std::move(aInitData), cdm = mCDM]() {
        cdm->CreateSession(aPromiseId, aCreateSessionToken, initDataType,
                           initData, aSessionType);
      }));
}

void GonkDrmCDMProxy::LoadSession(PromiseId aPromiseId,
                                  dom::MediaKeySessionType aSessionType,
                                  const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::LoadSession",
      [aPromiseId, sessionId = NS_ConvertUTF16toUTF8(aSessionId),
       cdm = mCDM]() { cdm->LoadSession(aPromiseId, sessionId); }));
}

void GonkDrmCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                           nsTArray<uint8_t>& aCert) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::SetServerCertificate",
      [aPromiseId, cert = std::move(aCert), cdm = mCDM]() {
        cdm->SetServerCertificate(aPromiseId, cert);
      }));
}

void GonkDrmCDMProxy::UpdateSession(const nsAString& aSessionId,
                                    PromiseId aPromiseId,
                                    nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::UpdateSession",
      [aPromiseId, sessionId = NS_ConvertUTF16toUTF8(aSessionId),
       response = std::move(aResponse),
       cdm = mCDM]() { cdm->UpdateSession(aPromiseId, sessionId, response); }));
}

void GonkDrmCDMProxy::CloseSession(const nsAString& aSessionId,
                                   PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::CloseSession",
      [aPromiseId, sessionId = NS_ConvertUTF16toUTF8(aSessionId),
       cdm = mCDM]() { cdm->CloseSession(aPromiseId, sessionId); }));
}

void GonkDrmCDMProxy::RemoveSession(const nsAString& aSessionId,
                                    PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::RemoveSession",
      [aPromiseId, sessionId = NS_ConvertUTF16toUTF8(aSessionId),
       cdm = mCDM]() { cdm->RemoveSession(aPromiseId, sessionId); }));
}

void GonkDrmCDMProxy::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  MOZ_ASSERT(mStorage);

  mOwnerThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMProxy::Shutdown", [cdm = mCDM]() { cdm->Shutdown(); }));

  mOwnerThread->Shutdown();
  mOwnerThread = nullptr;
  mStorage->Shutdown();  // must be called after mOwnerThread is shut down
  mStorage = nullptr;
  mCallback = nullptr;
  mCDM = nullptr;
}

void GonkDrmCDMProxy::Terminated() {
  // TODO: Implement Terminated.
  // Should find a way to handle the case when remote side MediaDrm crashed.
}

void GonkDrmCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
                                     const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }

  RefPtr<dom::MediaKeySession> session(
      mKeys->GetPendingSession(aCreateSessionToken));
  if (session) {
    session->SetSessionId(aSessionId);
  }
}

void GonkDrmCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                                  bool aSuccess) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

void GonkDrmCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                                       dom::MediaKeyMessageType aMessageType,
                                       const nsTArray<uint8_t>& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyMessage(aMessageType, aMessage);
  }
}

void GonkDrmCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                         UnixTime aExpiryTime) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    if (aExpiryTime) {
      session->SetExpiration(static_cast<double>(aExpiryTime));
    } else {
      // According to MediaDrm documentation, a time of 0 indicates that the
      // keys never expire.
      session->ResetExpiration();
    }
  }
}

void GonkDrmCDMProxy::OnSessionClosed(const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->OnClosed();
  }
}

void GonkDrmCDMProxy::OnSessionError(const nsAString& aSessionId,
                                     nsresult aException, uint32_t aSystemCode,
                                     const nsAString& aMsg) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyError(aSystemCode);
  }
}

void GonkDrmCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                                      ErrorResult&& aException,
                                      const nsCString& aMsg) {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, std::move(aException), aMsg);
}

RefPtr<DecryptPromise> GonkDrmCDMProxy::Decrypt(MediaRawData* aSample) {
  EME_LOG("GonkDrmCDMProxy::Decrypt, not supported");
  return DecryptPromise::CreateAndReject(
      DecryptResult(eme::AbortedErr, aSample), __func__);
}

void GonkDrmCDMProxy::OnDecrypted(uint32_t aPromiseId, DecryptStatus aResult,
                                  const nsTArray<uint8_t>& aDecryptedData) {
  EME_LOG("GonkDrmCDMProxy::OnDecrypted, not supported");
}

void GonkDrmCDMProxy::RejectPromise(PromiseId aPromiseId,
                                    ErrorResult&& aException,
                                    const nsCString& aReason) {
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->RejectPromise(aPromiseId, std::move(aException), aReason);
    }
  } else {
    mMainThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmCDMProxy::RejectPromise",
        [aPromiseId, exception = CopyableErrorResult(aException), aReason,
         self = RefPtr<GonkDrmCDMProxy>(this)]() mutable {
          self->RejectPromise(aPromiseId, std::move(exception), aReason);
        }));
  }
}

void GonkDrmCDMProxy::RejectPromiseWithStateError(PromiseId aPromiseId,
                                                  const nsCString& aReason) {
  ErrorResult rv;
  rv.ThrowInvalidStateError(aReason);
  RejectPromise(aPromiseId, std::move(rv), aReason);
}

void GonkDrmCDMProxy::ResolvePromise(PromiseId aPromiseId) {
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aPromiseId);
    } else {
      NS_WARNING("GonkDrmCDMProxy unable to resolve promise!");
    }
  } else {
    mMainThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmCDMProxy::ResolvePromise",
        [aPromiseId, self = RefPtr<GonkDrmCDMProxy>(this)]() {
          self->ResolvePromise(aPromiseId);
        }));
  }
}

void GonkDrmCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyStatusesChange();
  }
}

void GonkDrmCDMProxy::GetStatusForPolicy(PromiseId aPromiseId,
                                         const nsAString& aMinHdcpVersion) {
  // TODO: Implement GetStatusForPolicy.
  constexpr auto err = "Currently Gonk does not support GetStatusForPolicy"_ns;

  ErrorResult rv;
  rv.ThrowNotSupportedError(err);
  RejectPromise(aPromiseId, std::move(rv), err);
}

#ifdef DEBUG
bool GonkDrmCDMProxy::IsOnOwnerThread() {
  return NS_GetCurrentThread() == mOwnerThread;
}
#endif

void GonkDrmCDMProxy::OnCDMCreated(uint32_t aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }

  if (mCDM) {
    mKeys->OnCDMCreated(aPromiseId, 0);
    return;
  }

  // No CDM? Just reject the promise.
  constexpr auto err = "Null CDM in OnCDMCreated()"_ns;
  ErrorResult rv;
  rv.ThrowInvalidStateError(err);
  mKeys->RejectPromise(aPromiseId, std::move(rv), err);
}

// Called on any thread.
android::sp<android::ICrypto> GonkDrmCDMProxy::CreateCrypto() {
  return new android::GonkCryptoProxy(mKeySystem, mSharedData);
}

}  // namespace mozilla
