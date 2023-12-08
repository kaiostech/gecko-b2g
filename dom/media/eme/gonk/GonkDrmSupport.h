/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmSupport_H
#define GonkDrmSupport_H

#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>

#include <map>

#define GONK_DRM_PEEK_CLEARKEY_KEY_STATUS

class nsISerialEventTarget;

namespace mozilla {
class CDMKeyInfo;
class GonkDrmCDMCallbackProxy;
class GonkDrmStorageProxy;
}  // namespace mozilla

namespace android {

class GonkDrmListener;
class GonkDrmSessionInfo;
class GonkDrmSharedData;
class IDrm;

class GonkDrmSupport : public RefBase {
  typedef mozilla::dom::MediaKeyMessageType MediaKeyMessageType;
  typedef mozilla::dom::MediaKeySessionType MediaKeySessionType;
  typedef mozilla::CDMKeyInfo CDMKeyInfo;
  typedef mozilla::GonkDrmCDMCallbackProxy GonkDrmCDMCallbackProxy;
  typedef mozilla::GonkDrmStorageProxy GonkDrmStorageProxy;

  friend class GonkDrmListener;

 public:
  GonkDrmSupport(nsISerialEventTarget* aOwnerThread, const nsAString& aOrigin,
                 const nsAString& aKeySystem);

  void Init(uint32_t aPromiseId, GonkDrmCDMCallbackProxy* aCallback,
            GonkDrmStorageProxy* aStorage,
            const sp<GonkDrmSharedData>& aSharedData);

  void Shutdown();

  void CreateSession(uint32_t aPromiseId, uint32_t aCreateSessionToken,
                     const nsCString& aInitDataType,
                     const nsTArray<uint8_t>& aInitData,
                     MediaKeySessionType aSessionType);

  void LoadSession(uint32_t aPromiseId, const nsCString& aEmeSessionId);

  void UpdateSession(uint32_t aPromiseId, const nsCString& aEmeSessionId,
                     const nsTArray<uint8_t>& aResponse);

  void CloseSession(uint32_t aPromiseId, const nsCString& aEmeSessionId);

  void RemoveSession(uint32_t aPromiseId, const nsCString& aEmeSessionId);

  void SetServerCertificate(uint32_t aPromiseId,
                            const nsTArray<uint8_t>& aCert);

 private:
  ~GonkDrmSupport();

  status_t OpenCryptoSession();

  sp<GonkDrmSessionInfo> OpenDrmSession(
      MediaKeySessionType aSessionType,
      const nsCString& aEmeSessionId = nsCString(), status_t* aErr = nullptr);

  status_t CloseDrmSession(const sp<GonkDrmSessionInfo>& aSession);

  void StartProvisioning();

  void UpdateProvisioningResponse(bool aSuccess, const nsACString& aResponse);

  void InitFailed();

  void InitCompleted();

  void Reset();

  typedef std::pair<MediaKeyMessageType, nsTArray<uint8_t>> KeyRequest;

  bool GetKeyRequest(const sp<GonkDrmSessionInfo>& aSession,
                     const nsTArray<uint8_t>& aInitData, KeyRequest* aRequest);

  void SendKeyRequest(const sp<GonkDrmSessionInfo>& aSession,
                      KeyRequest&& aRequest);

  typedef std::function<void()> SuccessCallback;
  typedef std::function<void(const nsACString& /* aReason */)> FailureCallback;

  void LoadSession(const sp<GonkDrmSessionInfo>& aSession,
                   SuccessCallback aSuccessCb, FailureCallback aFailureCb);

  void UpdateSession(const sp<GonkDrmSessionInfo>& aSession,
                     const nsTArray<uint8_t>& aResponse,
                     SuccessCallback aSuccessCb, FailureCallback aFailureCb);

  void RemoveSession(const sp<GonkDrmSessionInfo>& aSession,
                     SuccessCallback aSuccessCb, FailureCallback aFailureCb);

  void OnKeyNeeded(const Vector<uint8_t>& aSessionId,
                   const nsTArray<uint8_t>& aData);

  void OnExpirationUpdated(const Vector<uint8_t>& aSessionId,
                           int64_t aExpirationTime);

  void OnKeyStatusChanged(const Vector<uint8_t>& aSessionId,
                          nsTArray<CDMKeyInfo>&& aKeyInfos,
                          bool aHasNewUsableKey);

#ifdef GONK_DRM_PEEK_CLEARKEY_KEY_STATUS
  void PeekClearkeyKeyStatus(const sp<GonkDrmSessionInfo>& aSession,
                             const nsTArray<uint8_t>& aResponse);
#endif

  void NotifyKeyStatus(const sp<GonkDrmSessionInfo>& aSession,
                       nsTArray<CDMKeyInfo>&& aKeyInfos);

  sp<GonkDrmSupport> Self() { return this; }

  const nsCOMPtr<nsISerialEventTarget> mOwnerThread;
  const nsString mOrigin;
  const nsString mKeySystem;
  const nsTArray<uint8_t> mDummyKeyId = {0};
  uint32_t mInitPromiseId = 0;
  GonkDrmCDMCallbackProxy* mCallback = nullptr;
  RefPtr<GonkDrmStorageProxy> mStorage;
  sp<GonkDrmSharedData> mSharedData;
  sp<GonkDrmListener> mDrmListener;
  sp<IDrm> mDrm;

  class SessionManager final {
   public:
    void Add(const sp<GonkDrmSessionInfo>& aSession);
    void Remove(const sp<GonkDrmSessionInfo>& aSession);
    void Clear();
    std::vector<sp<GonkDrmSessionInfo>> All();
    sp<GonkDrmSessionInfo> FindByEmeId(const nsCString& aEmeId);
    sp<GonkDrmSessionInfo> FindByDrmId(const Vector<uint8_t>& aDrmId);

   private:
    std::map<nsCString, sp<GonkDrmSessionInfo>> mEmeSessionIdMap;
    std::map<std::vector<uint8_t>, sp<GonkDrmSessionInfo>> mDrmSessionIdMap;
  };

  SessionManager mSessionManager;
};

}  // namespace android

#endif  // GonkDrmSupport_H
