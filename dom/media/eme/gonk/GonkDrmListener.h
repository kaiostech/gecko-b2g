/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmListener_H
#define GonkDrmListener_H

#include "nsCOMPtr.h"

#include <mediadrm/IDrmClient.h>

class nsISerialEventTarget;

namespace android {

class GonkDrmSupport;

#if ANDROID_VERSION >= 30
class GonkDrmListener : public IDrmClient {
#else
class GonkDrmListener : public BnDrmClient {
#endif
 public:
  GonkDrmListener(const wp<GonkDrmSupport>& aCDM,
                  nsISerialEventTarget* aOwnerThread)
      : mCDM(aCDM), mOwnerThread(aOwnerThread) {}

 private:
#if ANDROID_VERSION >= 30
  void sendEvent(DrmPlugin::EventType aEventType,
                 const hardware::hidl_vec<uint8_t>& aSessionId,
                 const hardware::hidl_vec<uint8_t>& aData) override;

  void sendExpirationUpdate(const hardware::hidl_vec<uint8_t>& aSessionId,
                            int64_t aExpiryTimeInMS) override;

  void sendKeysChange(const hardware::hidl_vec<uint8_t>& aSessionId,
                      const std::vector<DrmKeyStatus>& aKeyStatusList,
                      bool aHasNewUsableKey) override;

  void sendSessionLostState(
      const hardware::hidl_vec<uint8_t>& aSessionId) override;

#else
  void notify(DrmPlugin::EventType aEventType, int aExtra,
              const Parcel* aObj) override;
#endif

  sp<GonkDrmListener> Self() { return this; }

  const wp<GonkDrmSupport> mCDM;
  const nsCOMPtr<nsISerialEventTarget> mOwnerThread;
};

}  // namespace android

#endif
