/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmCDMCallbackProxy.h"

#include "GonkDrmCDMProxy.h"
#include "MainThreadUtils.h"

namespace mozilla {

GonkDrmCDMCallbackProxy::GonkDrmCDMCallbackProxy(GonkDrmCDMProxy* aProxy)
    : mProxy(aProxy), mMainThread(GetMainThreadSerialEventTarget()) {}

void GonkDrmCDMCallbackProxy::CDMCreated(uint32_t aPromiseId) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::CDMCreated",
      [aPromiseId, proxy = RefPtr<GonkDrmCDMProxy>(mProxy)]() {
        proxy->OnCDMCreated(aPromiseId);
      }));
}

void GonkDrmCDMCallbackProxy::SetSessionId(uint32_t aToken,
                                           const nsCString& aSessionId) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::SetSessionId",
      [aToken, sessionId = NS_ConvertUTF8toUTF16(aSessionId),
       proxy = RefPtr<CDMProxy>(mProxy)]() {
        proxy->OnSetSessionId(aToken, sessionId);
      }));
}

void GonkDrmCDMCallbackProxy::ResolveLoadSessionPromise(uint32_t aPromiseId,
                                                        bool aSuccess) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::ResolveLoadSessionPromise",
      [aPromiseId, aSuccess, proxy = RefPtr<CDMProxy>(mProxy)]() {
        proxy->OnResolveLoadSessionPromise(aPromiseId, aSuccess);
      }));
}

void GonkDrmCDMCallbackProxy::ResolvePromise(uint32_t aPromiseId) {
  mMainThread->Dispatch(
      NS_NewRunnableFunction("GonkDrmCDMCallbackProxy::ResolvePromise",
                             [aPromiseId, proxy = RefPtr<CDMProxy>(mProxy)]() {
                               proxy->ResolvePromise(aPromiseId);
                             }));
}

void GonkDrmCDMCallbackProxy::RejectPromise(uint32_t aPromiseId,
                                            ErrorResult&& aException,
                                            const nsCString& aReason) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::RejectPromise",
      [aPromiseId, exception = CopyableErrorResult(aException), aReason,
       proxy = RefPtr<CDMProxy>(mProxy)]() mutable {
        proxy->OnRejectPromise(aPromiseId, std::move(exception), aReason);
      }));
}

void GonkDrmCDMCallbackProxy::RejectPromiseWithStateError(
    uint32_t aPromiseId, const nsCString& aReason) {
  ErrorResult rv;
  rv.ThrowInvalidStateError(aReason);
  RejectPromise(aPromiseId, std::move(rv), aReason);
}

void GonkDrmCDMCallbackProxy::SessionMessage(
    const nsCString& aSessionId, dom::MediaKeyMessageType aMessageType,
    nsTArray<uint8_t>&& aMessage) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::SessionMessage",
      [sessionId = NS_ConvertUTF8toUTF16(aSessionId), aMessageType,
       message = std::move(aMessage), proxy = RefPtr<CDMProxy>(mProxy)]() {
        proxy->OnSessionMessage(sessionId, aMessageType, message);
      }));
}

void GonkDrmCDMCallbackProxy::ExpirationChange(const nsCString& aSessionId,
                                               UnixTime aExpiryTime) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::ExpirationChange",
      [sessionId = NS_ConvertUTF8toUTF16(aSessionId), aExpiryTime,
       proxy = RefPtr<CDMProxy>(mProxy)]() {
        proxy->OnExpirationChange(sessionId, aExpiryTime);
      }));
}

void GonkDrmCDMCallbackProxy::SessionClosed(const nsCString& aSessionId) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::SessionClosed",
      [sessionId = NS_ConvertUTF8toUTF16(aSessionId),
       proxy = RefPtr<CDMProxy>(mProxy)]() {
        bool keyStatusesChange = false;
        {
          auto caps = proxy->Capabilites().Lock();
          keyStatusesChange = caps->RemoveKeysForSession(sessionId);
        }
        if (keyStatusesChange) {
          proxy->OnKeyStatusesChange(sessionId);
        }
        proxy->OnSessionClosed(sessionId);
      }));
}

void GonkDrmCDMCallbackProxy::SessionError(const nsCString& aSessionId,
                                           nsresult aException,
                                           uint32_t aSystemCode,
                                           const nsCString& aMessage) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::SessionError",
      [sessionId = NS_ConvertUTF8toUTF16(aSessionId), aException, aSystemCode,
       message = NS_ConvertUTF8toUTF16(aMessage),
       proxy = RefPtr<CDMProxy>(mProxy)]() {
        proxy->OnSessionError(sessionId, aException, aSystemCode, message);
      }));
}

void GonkDrmCDMCallbackProxy::BatchedKeyStatusChanged(
    const nsCString& aSessionId, nsTArray<CDMKeyInfo>&& aKeyInfos) {
  mMainThread->Dispatch(NS_NewRunnableFunction(
      "GonkDrmCDMCallbackProxy::BatchedKeyStatusChanged",
      [sessionId = NS_ConvertUTF8toUTF16(aSessionId),
       keyInfos = std::move(aKeyInfos), proxy = RefPtr<CDMProxy>(mProxy)]() {
        bool keyStatusesChange = false;
        {
          auto caps = proxy->Capabilites().Lock();
          for (const auto& keyInfo : keyInfos) {
            keyStatusesChange |=
                caps->SetKeyStatus(keyInfo.mKeyId, sessionId, keyInfo.mStatus);
          }
        }
        if (keyStatusesChange) {
          proxy->OnKeyStatusesChange(sessionId);
        }
      }));
}

}  // namespace mozilla
