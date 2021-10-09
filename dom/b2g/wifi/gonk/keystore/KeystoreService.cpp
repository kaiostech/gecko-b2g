/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "WifiKeystore"

#include <binder/IPCThreadState.h>
#include <fcntl.h>
#include <limits.h>
#include <memory>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "KeystoreService.h"
#include "nsICryptoHash.h"
#include "nsComponentManagerUtils.h"

#include "plbase64.h"
#include "certdb.h"
#include "ScopedNSSTypes.h"
#include "mozilla/ClearOnShutdown.h"

#if defined(MOZ_WIDGET_GONK)
#  include <android/log.h>
#  define KEYSTORE_LOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, args)
#else
#  define KEYSTORE_LOG(args...) printf(args);
#endif

using namespace mozilla;
using namespace android;

static const char *CA_BEGIN = "-----BEGIN ", *CA_END = "-----END ",
                  *CA_TAILER = "-----\n";

static const char* KEYSTORE_ALLOWED_USERS[] = {"root", "wifi", nullptr};

static const char* KEYSTORE_ALLOWED_PREFIXES[] = {
    "WIFI_SERVERCERT_", "WIFI_USERCERT_", "WIFI_USERKEY_", nullptr};

static const int CA_LINE_SIZE = 64;

namespace mozilla {

// Transform base64 certification data into DER format
void FormatCaData(const char* aCaData, int aCaDataLength, const char* aName,
                  const uint8_t** aFormatData, size_t* aFormatDataLength) {
  size_t bufSize = strlen(CA_BEGIN) + strlen(CA_END) + strlen(CA_TAILER) * 2 +
                   strlen(aName) * 2 + aCaDataLength +
                   aCaDataLength / CA_LINE_SIZE + 2;
  char* buf = (char*)malloc(bufSize);
  if (!buf) {
    *aFormatData = nullptr;
    return;
  }

  *aFormatDataLength = bufSize;
  *aFormatData = (const uint8_t*)buf;

  char* ptr = buf;
  size_t len;

  // Create DER header.
  len = snprintf(ptr, bufSize, "%s%s%s", CA_BEGIN, aName, CA_TAILER);
  ptr += len;
  bufSize -= len;

  // Split base64 data in lines.
  int copySize;
  while (aCaDataLength > 0) {
    copySize = (aCaDataLength > CA_LINE_SIZE) ? CA_LINE_SIZE : aCaDataLength;

    memcpy(ptr, aCaData, copySize);
    ptr += copySize;
    aCaData += copySize;
    aCaDataLength -= copySize;
    bufSize -= copySize;

    *ptr = '\n';
    ptr++;
    bufSize--;
  }

  // Create DEA tailer.
  snprintf(ptr, bufSize, "%s%s%s", CA_END, aName, CA_TAILER);
}

StatusCode getCertificate(const char* aCertName, const uint8_t** aCertData,
                          size_t* aCertDataLength) {
  // certificate name prefix check.
  if (!aCertName) {
    return StatusCode::ERROR_UNKNOWN;
  }

  const char** prefix = KEYSTORE_ALLOWED_PREFIXES;
  for (; *prefix; prefix++) {
    if (!strncmp(*prefix, aCertName, strlen(*prefix))) {
      break;
    }
  }
  if (!(*prefix)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Get cert from NSS by name
  UniqueCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), aCertName));
  if (!cert) {
    return StatusCode::ERROR_UNKNOWN;
  }

  char* certDER = PL_Base64Encode((const char*)cert.get()->derCert.data,
                                  cert.get()->derCert.len, nullptr);
  if (!certDER) {
    return StatusCode::ERROR_UNKNOWN;
  }

  FormatCaData(certDER, strlen(certDER), "CERTIFICATE", aCertData,
               aCertDataLength);
  PL_strfree(certDER);

  if (!(*aCertData)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  return StatusCode::SUCCESS;
}

StatusCode getPrivateKey(const char* aKeyName, const uint8_t** aKeyData,
                         size_t* aKeyDataLength) {
  *aKeyData = nullptr;
  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s",
           aKeyName + 13);

  // Get private key from user certificate.
  UniqueCERTCertificate userCert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return StatusCode::ERROR_UNKNOWN;
  }

  UniqueSECKEYPrivateKey privateKey(
      PK11_FindKeyByAnyCert(userCert.get(), nullptr));
  if (!privateKey) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Export private key in PKCS#12 encrypted format, no password.
  unsigned char pwstr[] = {0, 0};
  SECItem password = {siBuffer, pwstr, sizeof(pwstr)};
  UniqueSECKEYEncryptedPrivateKeyInfo encryptedPrivateKey(
      PK11_ExportEncryptedPrivKeyInfo(
          privateKey.get()->pkcs11Slot,
          SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4, &password,
          privateKey.get(), 1, privateKey.get()->wincx));

  if (!encryptedPrivateKey) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Decrypt into RSA private key.
  //
  // Generate key for PKCS#12 encryption, we use SHA1 with 1 iteration, as the
  // parameters used in PK11_ExportEncryptedPrivKeyInfo() above.
  // see: PKCS#12 v1.0, B.2.
  //
  uint8_t DSP[192] = {0};
  memset(DSP, 0x01, 64);        // Diversifier part, ID = 1 for decryption.
  memset(DSP + 128, 0x00, 64);  // Password part, no password.

  uint8_t* S = &DSP[64];  // Salt part.
  uint8_t* salt = encryptedPrivateKey.get()->algorithm.parameters.data + 4;
  int saltLength = (int)encryptedPrivateKey.get()->algorithm.parameters.data[3];
  if (saltLength <= 0) {
    return StatusCode::ERROR_UNKNOWN;
  }
  for (int i = 0; i < 64; i++) {
    S[i] = salt[i % saltLength];
  }

  // Generate key by SHA-1
  nsresult rv;
  nsCOMPtr<nsICryptoHash> hash =
      do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  if (NS_FAILED(rv)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  rv = hash->Init(nsICryptoHash::SHA1);
  if (NS_FAILED(rv)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  rv = hash->Update(DSP, sizeof(DSP));
  if (NS_FAILED(rv)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  nsAutoCString hashResult;
  rv = hash->Finish(false, hashResult);
  if (NS_FAILED(rv)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // First 40-bit as key for RC4.
  uint8_t key[5];
  memcpy(key, hashResult.get(), sizeof(key));

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return StatusCode::ERROR_UNKNOWN;
  }

  SECItem keyItem = {siBuffer, key, sizeof(key)};
  UniquePK11SymKey symKey(PK11_ImportSymKey(
      slot.get(), CKM_RC4, PK11_OriginUnwrap, CKA_DECRYPT, &keyItem, nullptr));
  if (!symKey) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Get expected decrypted data size then allocate memory.
  uint8_t* encryptedData =
      (uint8_t*)encryptedPrivateKey.get()->encryptedData.data;
  unsigned int encryptedDataLen = encryptedPrivateKey.get()->encryptedData.len;
  unsigned int decryptedDataLen = encryptedDataLen;
  SECStatus srv =
      PK11_Decrypt(symKey.get(), CKM_RC4, &keyItem, nullptr, &decryptedDataLen,
                   encryptedDataLen, encryptedData, encryptedDataLen);
  if (srv != SECSuccess) {
    return StatusCode::ERROR_UNKNOWN;
  }

  UniqueSECItem decryptedData(
      ::SECITEM_AllocItem(nullptr, nullptr, decryptedDataLen));
  if (!decryptedData) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Decrypt by RC4.
  srv = PK11_Decrypt(symKey.get(), CKM_RC4, &keyItem, decryptedData.get()->data,
                     &decryptedDataLen, decryptedData.get()->len, encryptedData,
                     encryptedDataLen);
  if (srv != SECSuccess) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Export key in PEM format.
  char* keyPEM = PL_Base64Encode((const char*)decryptedData.get()->data,
                                 decryptedDataLen, nullptr);

  if (!keyPEM) {
    return StatusCode::ERROR_UNKNOWN;
  }

  FormatCaData(keyPEM, strlen(keyPEM), "PRIVATE KEY", aKeyData, aKeyDataLength);
  PL_strfree(keyPEM);

  if (!(*aKeyData)) {
    return StatusCode::ERROR_UNKNOWN;
  }

  return StatusCode::SUCCESS;
}

StatusCode getPublicKey(const char* aKeyName, const uint8_t** aKeyData,
                        size_t* aKeyDataLength) {
  *aKeyData = nullptr;
  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s",
           aKeyName + 13);

  // Get public key from user certificate.
  UniqueCERTCertificate userCert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return StatusCode::ERROR_UNKNOWN;
  }

  // Get public key.
  UniqueSECKEYPublicKey publicKey(CERT_ExtractPublicKey(userCert.get()));
  if (!publicKey) {
    return StatusCode::ERROR_UNKNOWN;
  }

  UniqueSECItem keyItem(PK11_DEREncodePublicKey(publicKey.get()));
  if (!keyItem) {
    return StatusCode::ERROR_UNKNOWN;
  }

  size_t bufSize = keyItem.get()->len;
  char* buf = (char*)malloc(bufSize);
  if (!buf) {
    return StatusCode::ERROR_UNKNOWN;
  }

  memcpy(buf, keyItem.get()->data, bufSize);
  *aKeyData = (const uint8_t*)buf;
  *aKeyDataLength = bufSize;

  return StatusCode::SUCCESS;
}

StatusCode signData(const char* aKeyName, const uint8_t* data, size_t length,
                    uint8_t** out, size_t* outLength) {
  *out = nullptr;
  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s",
           aKeyName + 13);

  // Get private key from user certificate.
  UniqueCERTCertificate userCert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return StatusCode::ERROR_UNKNOWN;
  }

  UniqueSECKEYPrivateKey privateKey(
      PK11_FindKeyByAnyCert(userCert.get(), nullptr));
  if (!privateKey) {
    return StatusCode::ERROR_UNKNOWN;
  }

  //
  // Find hash data from incoming data.
  //
  // Incoming data might be padded by PKCS-1 format:
  // 00 01 FF FF ... FF 00 || Hash of length 36
  // If the padding part exists, we have to ignore them.
  //
  uint8_t* hash = (uint8_t*)data;
  const size_t HASH_LENGTH = 36;
  if (length < HASH_LENGTH) {
    return StatusCode::ERROR_UNKNOWN;
  }
  if (hash[0] == 0x00 && hash[1] == 0x01 && hash[2] == 0xFF &&
      hash[3] == 0xFF) {
    hash += 4;
    while (*hash == 0xFF) {
      if (hash + HASH_LENGTH > data + length) {
        return StatusCode::ERROR_UNKNOWN;
      }
      hash++;
    }
    if (*hash != 0x00) {
      return StatusCode::ERROR_UNKNOWN;
    }
    hash++;
  }
  if (hash + HASH_LENGTH != data + length) {
    return StatusCode::ERROR_UNKNOWN;
  }
  SECItem hashItem = {siBuffer, hash, HASH_LENGTH};

  // Sign hash.
  UniqueSECItem signItem(::SECITEM_AllocItem(
      nullptr, nullptr, PK11_SignatureLen(privateKey.get())));
  if (!signItem) {
    return StatusCode::ERROR_UNKNOWN;
  }

  SECStatus srv;
  srv = PK11_Sign(privateKey.get(), signItem.get(), &hashItem);
  if (srv != SECSuccess) {
    return StatusCode::ERROR_UNKNOWN;
  }

  uint8_t* buf = (uint8_t*)malloc(signItem->len);
  if (!buf) {
    return StatusCode::ERROR_UNKNOWN;
  }

  memcpy(buf, signItem->data, signItem->len);
  *out = buf;
  *outLength = signItem->len;

  return StatusCode::SUCCESS;
}

StatusCode checkPermission(uid_t uid) {
  struct passwd* userInfo = getpwuid(uid);
  for (const char** user = KEYSTORE_ALLOWED_USERS; *user; user++) {
    if (!strcmp(*user, userInfo->pw_name)) {
      return StatusCode::SUCCESS;
    }
  }
  return StatusCode::ERROR_UNKNOWN;
}

//
// Implement wifi keystore hidl service.
//
namespace wifi {
namespace keystore {

StaticRefPtr<mozilla::wifi::keystore::KeystoreService> gKeystoreService;

already_AddRefed<KeystoreService> KeystoreService::FactoryCreate() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gKeystoreService) {
    gKeystoreService = new mozilla::wifi::keystore::KeystoreService();
    ClearOnShutdown(&gKeystoreService);
  }

  // Here register as a hidl service.
  RefPtr<KeystoreService> service = gKeystoreService;
  if (service->registerAsService() != android::OK) {
    KEYSTORE_LOG("Failed to register IKeystore service");
  }

  return service.forget();
}

Return<void> KeystoreService::getBlob(const hidl_string& key,
                                      getBlob_cb _hidl_cb) {
  uid_t callingUid = IPCThreadState::self()->getCallingUid();
  if (checkPermission(callingUid) != StatusCode::SUCCESS) {
    _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
    return Void();
  }

  uint8_t* data = nullptr;
  size_t dataLength = 0;
  std::vector<uint8_t> value;
  String8 certName(key.c_str());

  if (!strncmp(certName.string(), "WIFI_USERKEY_", 13)) {
    if (::getPrivateKey(certName.string(), (const uint8_t**)&data,
                        &dataLength) != StatusCode::SUCCESS) {
      _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
      return Void();
    }

    value = std::vector<uint8_t>(data, data + dataLength);
    _hidl_cb(StatusCode::SUCCESS, (hidl_vec<uint8_t>)value);
    return Void();
  }

  if (::getCertificate(certName.string(), (const uint8_t**)&data,
                       &dataLength) != StatusCode::SUCCESS) {
    value = std::vector<uint8_t>(data, data + dataLength);
    _hidl_cb(StatusCode::ERROR_UNKNOWN, (hidl_vec<uint8_t>)value);
    return Void();
  }

  value = std::vector<uint8_t>(data, data + dataLength);
  _hidl_cb(StatusCode::SUCCESS, (hidl_vec<uint8_t>)value);
  return Void();
}

Return<void> KeystoreService::getPublicKey(const hidl_string& keyId,
                                           getPublicKey_cb _hidl_cb) {
  uid_t callingUid = IPCThreadState::self()->getCallingUid();
  if (checkPermission(callingUid) != StatusCode::SUCCESS) {
    _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
    return Void();
  }

  uint8_t* pubkey = nullptr;
  size_t pubkeyLength = 0;
  String8 keyName(keyId.c_str());

  if (!strncmp(keyName.string(), "WIFI_USERKEY_", 13)) {
    if (::getPublicKey(keyName.string(), (const uint8_t**)&pubkey,
                       &pubkeyLength) != StatusCode::SUCCESS) {
      _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
      return Void();
    }
  }

  std::vector<uint8_t> value(pubkey, pubkey + pubkeyLength);
  _hidl_cb(StatusCode::SUCCESS, (hidl_vec<uint8_t>)value);
  return Void();
}

Return<void> KeystoreService::sign(const hidl_string& keyId,
                                   const hidl_vec<uint8_t>& dataToSign,
                                   sign_cb _hidl_cb) {
  uid_t callingUid = IPCThreadState::self()->getCallingUid();
  if (checkPermission(callingUid) != StatusCode::SUCCESS) {
    _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
    return Void();
  }

  std::vector<uint8_t> dataVec(dataToSign);
  if (dataVec.empty()) {
    _hidl_cb(StatusCode::ERROR_UNKNOWN, {});
    return Void();
  }

  uint8_t* out = nullptr;
  size_t outLength = 0;
  std::string dataStr(dataVec.begin(), dataVec.end());
  const uint8_t* data = reinterpret_cast<const uint8_t*>(dataStr.c_str());
  size_t dataLength = dataStr.length();

  String8 keyName(keyId.c_str());

  if (!strncmp(keyName.string(), "WIFI_USERKEY_", 13)) {
    if (::signData(keyName.string(), (const uint8_t*)data, dataLength,
                   (uint8_t**)&out, &outLength) != StatusCode::SUCCESS) {
      _hidl_cb(StatusCode::SUCCESS, {});
      return Void();
    }
  }

  std::vector<uint8_t> value(out, out + outLength);
  _hidl_cb(StatusCode::SUCCESS, (hidl_vec<uint8_t>)value);
  return Void();
}

NS_IMETHODIMP
KeystoreService::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  return NS_OK;
}

NS_IMPL_ISUPPORTS(KeystoreService, nsIObserver)

}  // namespace keystore
}  // namespace wifi
}  // namespace mozilla
