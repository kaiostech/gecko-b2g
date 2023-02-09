/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmCDMProxy_H
#define GonkDrmCDMProxy_H

#include "mozilla/CDMProxy.h"
#include "mozilla/CDMCaps.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/MozPromise.h"
#include "nsString.h"

#include <utils/StrongPointer.h>

namespace android {
class GonkDrmSharedData;
class GonkDrmSupport;
class ICrypto;
}  // namespace android

namespace mozilla {

class GonkDrmCDMCallbackProxy;
class GonkDrmStorageProxy;

class GonkDrmCDMProxy : public CDMProxy {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GonkDrmCDMProxy, override)

  static bool IsSchemeSupported(const nsAString& aKeySystem);

  GonkDrmCDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
                  bool aDistinctiveIdentifierRequired,
                  bool aPersistentStateRequired);

  void Init(PromiseId aPromiseId, const nsAString& aOrigin,
            const nsAString& aTopLevelOrigin,
            const nsAString& aGMPName) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     MediaKeySessionType aSessionType, PromiseId aPromiseId,
                     const nsAString& aInitDataType,
                     nsTArray<uint8_t>& aInitData) override;

  void LoadSession(PromiseId aPromiseId, dom::MediaKeySessionType aSessionType,
                   const nsAString& aSessionId) override;

  void SetServerCertificate(PromiseId aPromiseId,
                            nsTArray<uint8_t>& aCert) override;

  void UpdateSession(const nsAString& aSessionId, PromiseId aPromiseId,
                     nsTArray<uint8_t>& aResponse) override;

  void CloseSession(const nsAString& aSessionId, PromiseId aPromiseId) override;

  void RemoveSession(const nsAString& aSessionId,
                     PromiseId aPromiseId) override;

  void QueryOutputProtectionStatus() override {}

  void NotifyOutputProtectionStatus(
      OutputProtectionCheckStatus aCheckStatus,
      OutputProtectionCaptureStatus aCaptureStatus) override {}

  void Shutdown() override;

  void Terminated() override;

  void OnSetSessionId(uint32_t aCreateSessionToken,
                      const nsAString& aSessionId) override;

  void OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess) override;

  void OnSessionMessage(const nsAString& aSessionId,
                        dom::MediaKeyMessageType aMessageType,
                        const nsTArray<uint8_t>& aMessage) override;

  void OnExpirationChange(const nsAString& aSessionId,
                          UnixTime aExpiryTime) override;

  void OnSessionClosed(const nsAString& aSessionId) override;

  void OnSessionError(const nsAString& aSessionId, nsresult aException,
                      uint32_t aSystemCode, const nsAString& aMsg) override;

  void OnRejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                       const nsCString& aMsg) override;

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) override;

  void OnDecrypted(uint32_t aId, DecryptStatus aResult,
                   const nsTArray<uint8_t>& aDecryptedData) override;

  // Reject promise with the given ErrorResult.
  // Can be called from any thread.
  void RejectPromise(PromiseId aId, ErrorResult&& aException,
                     const nsCString& aReason) override;

  // Reject promise with an InvalidStateError and the given message.
  void RejectPromiseWithStateError(PromiseId aId, const nsCString& aReason);

  // Resolves promise with "undefined".
  // Can be called from any thread.
  void ResolvePromise(PromiseId aId) override;

  // Threadsafe.
  void OnKeyStatusesChange(const nsAString& aSessionId) override;

  void GetStatusForPolicy(PromiseId aPromiseId,
                          const nsAString& aMinHdcpVersion) override;

#ifdef DEBUG
  bool IsOnOwnerThread() override;
#endif

  void OnCDMCreated(uint32_t aPromiseId);

  android::sp<android::ICrypto> CreateCrypto();

 private:
  virtual ~GonkDrmCDMProxy();

  android::sp<android::GonkDrmSharedData> mSharedData;
  android::sp<android::GonkDrmSupport> mCDM;
  UniquePtr<GonkDrmCDMCallbackProxy> mCallback;
  RefPtr<GonkDrmStorageProxy> mStorage;
};

}  // namespace mozilla

#endif  // GonkDrmCDMProxy_H
