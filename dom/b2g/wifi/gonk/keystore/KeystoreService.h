/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef KeystoreService_h
#define KeystoreService_h

#include <android-base/macros.h>
#include <android/system/wifi/keystore/1.0/IKeystore.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "cert.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "mozilla/Mutex.h"

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hidl::base::V1_0::DebugInfo;
using ::android::hidl::base::V1_0::IBase;
using ::android::system::wifi::keystore::V1_0::IKeystore;

using StatusCode = IKeystore::KeystoreStatusCode;

namespace mozilla {

void FormatCaData(const uint8_t* aCaData, int aCaDataLength, const char* aName,
                  const uint8_t** aFormatData, size_t* aFormatDataLength);

StatusCode getCertificate(const char* aCertName, const uint8_t** aCertData,
                          size_t* aCertDataLength);
StatusCode getPrivateKey(const char* aKeyName, const uint8_t** aKeyData,
                         size_t* aKeyDataLength);
StatusCode getPublicKey(const char* aKeyName, const uint8_t** aKeyData,
                        size_t* aKeyDataLength);
StatusCode signData(const char* aKeyName, const uint8_t* data, size_t length,
                    uint8_t** out, size_t* outLength);

StatusCode checkPermission(uid_t uid);

namespace wifi {
namespace keystore {

struct KeystoreService final : public IKeystore, public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<KeystoreService> FactoryCreate();

  Return<void> getBlob(const hidl_string& key, getBlob_cb _hidl_cb) override;
  Return<void> getPublicKey(const hidl_string& keyId,
                            getPublicKey_cb _hidl_cb) override;
  Return<void> sign(const hidl_string& keyId,
                    const hidl_vec<uint8_t>& dataToSign,
                    sign_cb _hidl_cb) override;

 private:
  KeystoreService() : mMutex("wifi.keystore") {}
  ~KeystoreService() = default;

  mozilla::Mutex mMutex;

  DISALLOW_COPY_AND_ASSIGN(KeystoreService);
};

}  // namespace keystore
}  // namespace wifi
}  // namespace mozilla

#endif  // KeystoreService_h
