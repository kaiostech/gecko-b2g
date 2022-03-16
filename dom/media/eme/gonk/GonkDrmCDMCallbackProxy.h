/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmCDMCallbackProxy_H
#define GonkDrmCDMCallbackProxy_H

#include "mozilla/CDMProxy.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {

class GonkDrmCDMProxy;

// Proxies call backs from GonkDrmSupport back to the MediaKeys object on the
// main thread.
class GonkDrmCDMCallbackProxy {
 public:
  explicit GonkDrmCDMCallbackProxy(GonkDrmCDMProxy* aProxy);

  ~GonkDrmCDMCallbackProxy() {}

  void CDMCreated(uint32_t aPromiseId);

  void SetSessionId(uint32_t aCreateSessionToken, const nsCString& aSessionId);

  void ResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess);

  void ResolvePromise(uint32_t aPromiseId);

  void RejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                     const nsCString& aReason);

  void RejectPromiseWithStateError(uint32_t aPromiseId,
                                   const nsCString& aReason);

  void SessionMessage(const nsCString& aSessionId,
                      dom::MediaKeyMessageType aMessageType,
                      nsTArray<uint8_t>&& aMessage);

  void ExpirationChange(const nsCString& aSessionId, UnixTime aExpiryTime);

  void SessionClosed(const nsCString& aSessionId);

  void SessionError(const nsCString& aSessionId, nsresult aException,
                    uint32_t aSystemCode, const nsCString& aMessage);

  void BatchedKeyStatusChanged(const nsCString& aSessionId,
                               nsTArray<CDMKeyInfo>&& aKeyInfos);

 private:
  // Warning: Weak ref.
  GonkDrmCDMProxy* mProxy;
  const nsCOMPtr<nsISerialEventTarget> mMainThread;
};

}  // namespace mozilla

#endif
