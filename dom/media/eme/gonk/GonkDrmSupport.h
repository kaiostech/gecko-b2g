/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmSupport_H
#define GonkDrmSupport_H

#include "mozilla/dom/MediaKeysBinding.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include <mediadrm/IDrmClient.h>

class nsISerialEventTarget;

namespace mozilla {
class GonkDrmCDMCallbackProxy;
}

namespace android {

class IDrm;

class GonkDrmSupport : public BnDrmClient {
  typedef mozilla::dom::MediaKeyMessageType MediaKeyMessageType;
  typedef mozilla::dom::MediaKeySessionType MediaKeySessionType;
  typedef mozilla::CDMKeyInfo CDMKeyInfo;
  typedef mozilla::GonkDrmCDMCallbackProxy GonkDrmCDMCallbackProxy;

 public:
  GonkDrmSupport(nsISerialEventTarget* aOwnerThread,
                 const nsAString& aKeySystem);

  void Init(uint32_t aPromiseId, GonkDrmCDMCallbackProxy* aCallback);

  void Shutdown();

  void CreateSession(uint32_t aPromiseId, uint32_t aCreateSessionToken,
                     const nsCString& aInitDataType,
                     const nsTArray<uint8_t>& aInitData,
                     MediaKeySessionType aSessionType);

  void UpdateSession(uint32_t aPromiseId, const nsCString& aSessionId,
                     const nsTArray<uint8_t>& aResponse);

  void CloseSession(uint32_t aPromiseId, const nsCString& aSessionId);

  void SetServerCertificate(uint32_t aPromiseId,
                            const nsTArray<uint8_t>& aCert);

 private:
  ~GonkDrmSupport();

  status_t OpenCryptoSession();

  void StartProvisioning();

  void UpdateProvisioningResponse(bool aSuccess, const nsACString& aResponse);

  void InitFailed();

  void InitCompleted();

  void Reset();

  // IDrmClient interface
  void notify(DrmPlugin::EventType aEventType, int aExtra,
              const Parcel* aObj) override;

  void Notify(DrmPlugin::EventType aEventType, int aExtra, const Parcel* aObj);

  void OnKeyStatusChanged(const Parcel* aParcel);

  sp<GonkDrmSupport> Self() { return this; }

  const nsCOMPtr<nsISerialEventTarget> mOwnerThread;
  const nsString mKeySystem;
  uint32_t mInitPromiseId = 0;
  GonkDrmCDMCallbackProxy* mCallback = nullptr;
  sp<IDrm> mDrm;
};

}  // namespace android

#endif  // GonkDrmSupport_H
