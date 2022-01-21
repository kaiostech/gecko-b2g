/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmSessionInfo_H
#define GonkDrmSessionInfo_H

#include "mozilla/RefPtr.h"
#include "nsString.h"

#include <utils/RefBase.h>
#include <utils/Vector.h>

namespace mozilla {
class GonkDrmStorageProxy;
}

namespace android {

class GonkDrmSessionInfo : public RefBase {
  typedef mozilla::GonkDrmStorageProxy GonkDrmStorageProxy;
  typedef std::function<void()> SuccessCallback;
  typedef std::function<void(const nsACString& /* aReason */)> FailureCallback;

 public:
  // Create a temporary session info. EME session ID will be auto-generated.
  static sp<GonkDrmSessionInfo> CreateTemporary(GonkDrmStorageProxy* aStorage,
                                                const Vector<uint8_t>& aDrmId);

  // Create a persistent session info. If EME session ID is not provided, it
  // will be auto-generated.
  static sp<GonkDrmSessionInfo> CreatePersistent(GonkDrmStorageProxy* aStorage,
                                                 const Vector<uint8_t>& aDrmId,
                                                 const nsACString& aEmeId);

  void SaveToStorage(const SuccessCallback& aSuccessCb,
                     const FailureCallback& aFailureCb);

  void LoadFromStorage(const SuccessCallback& aSuccessCb,
                       const FailureCallback& aFailureCb);

  void EraseFromStorage(const SuccessCallback& aSuccessCb,
                        const FailureCallback& aFailureCb);

  void SetMimeType(const nsACString& aMimeType);

  void SetReleased();

  void SetKeySetId(const Vector<uint8_t>& aKeySetId);

  bool IsTemporary() const;

  bool IsReleased() const;

  const nsCString& MimeType() const { return mMimeType; }

  const nsCString& EmeId() const { return mEmeId; }

  const Vector<uint8_t>& DrmId() const { return mDrmId; }

  const Vector<uint8_t>& KeySetId() const { return mKeySetId; }

 private:
  GonkDrmSessionInfo(GonkDrmStorageProxy* aStorage,
                     const Vector<uint8_t>& aDrmId, const nsACString& aEmeId,
                     const nsACString& aSessionType);

  ~GonkDrmSessionInfo() = default;

  const Vector<uint8_t> mDrmId;
  const nsCString mEmeId;
  nsCString mMimeType;
  nsCString mSessionType;
  Vector<uint8_t> mKeySetId;
  RefPtr<GonkDrmStorageProxy> mStorage;
};

}  // namespace android

#endif
