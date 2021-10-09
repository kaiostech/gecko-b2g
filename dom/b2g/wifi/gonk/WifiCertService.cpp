/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WifiCertService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/File.h"
#include "cert.h"
#include "certdb.h"
#include "CryptoTask.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIWifiService.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsNSSCertificate.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

// The singleton Wifi Cert service, to be used on the main thread.
StaticRefPtr<WifiCertService> gWifiCertService;

class ImportCertTask final : public CryptoTask {
 public:
  ImportCertTask(int32_t aId, Blob* aCertBlob, const nsAString& aCertPassword,
                 const nsAString& aCertNickname)
      : mBlob(aCertBlob), mPassword(aCertPassword) {
    MOZ_ASSERT(NS_IsMainThread());

    mResult = new nsWifiResult();
    mResult->mId = aId;
    mResult->mStatus = nsIWifiResult::ERROR_UNKNOWN;
    mResult->mUsageFlag = 0;
    mResult->mNickname = aCertNickname;
    mResult->mDuplicated = false;
  }

 private:
  virtual nsresult CalculateResult() override {
    MOZ_ASSERT(!NS_IsMainThread());

    // read data from blob.
    nsCString blobBuf;
    nsresult rv = ReadBlob(blobBuf);
    if (NS_FAILED(rv)) {
      return rv;
    }

    char* buf;
    uint32_t size = blobBuf.GetMutableData(&buf);
    if (size == 0) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (NicknameExists()) {
      mResult->mDuplicated = true;
      return NS_ERROR_FAILURE;
    }

    // Try import as DER format first.
    rv = ImportDERBlob(buf, size);
    if (NS_SUCCEEDED(rv)) {
      mResult->mStatus = nsIWifiResult::SUCCESS;
      return rv;
    }

    // Try import as PKCS#12 format.
    return ImportPKCS12Blob(buf, size, mPassword);
  }

  virtual void CallCallback(nsresult rv) override {
    if (NS_FAILED(rv)) {
      mResult->mStatus = nsIWifiResult::ERROR_UNKNOWN;
    }

    gWifiCertService->DispatchResult(mResult);
  }

  bool NicknameExists() {
    nsCString certNickname, serverCertName, userCertName;
    CopyUTF16toUTF8(mResult->mNickname, certNickname);
    UniqueCERTCertificate cert;

    serverCertName.AssignLiteral("WIFI_SERVERCERT_");
    serverCertName += certNickname;
    cert = (UniqueCERTCertificate)CERT_FindCertByNickname(
        CERT_GetDefaultCertDB(), serverCertName.get());
    if (!cert) {
      userCertName.AssignLiteral("WIFI_USERCERT_");
      userCertName += certNickname;
      cert = (UniqueCERTCertificate)CERT_FindCertByNickname(
          CERT_GetDefaultCertDB(), userCertName.get());
    }

    return cert != nullptr;
  }

  nsresult CertificateExists(CERTCertificate* aCert, bool* exist) {
    *exist = false;

    RefPtr<nsNSSCertificate> nsscert = new nsNSSCertificate(aCert);
    if (!nsscert) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString dbKey;
    // Get dbKey to identify the certificate.
    nsresult rv = nsscert->GetDbKey(dbKey);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (dbKey.IsEmpty()) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    if (NS_WARN_IF(!certdb)) {
      return NS_ERROR_FAILURE;
    }

    nsTArray<RefPtr<nsIX509Cert>> certificateList;
    rv = certdb->GetCerts(certificateList);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Here traverse all of the certificate in database to see whether the DB
    // key already exists.
    for (auto& c : certificateList) {
      nsAutoCString importedDbKey;
      Unused << c->GetDbKey(importedDbKey);
      if (importedDbKey.Equals(dbKey)) {
        *exist = true;
        break;
      }
    }

    return NS_OK;
  }

  nsresult ImportDERBlob(char* buf, uint32_t size) {
    // Create certificate object.
    UniqueCERTCertificate cert(CERT_DecodeCertFromPackage(buf, size));
    if (!cert) {
      return MapSECStatus(SECFailure);
    }

    bool exist;
    nsresult rv = CertificateExists(cert.get(), &exist);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (exist) {
      // Certificate already exists in DB.
      mResult->mDuplicated = true;
      return NS_ERROR_FAILURE;
    }

    // Import certificate.
    return ImportCert(cert.get());
  }

  static SECItem* HandleNicknameCollision(SECItem* aOldNickname,
                                          PRBool* aCancel, void* aWincx) {
    const char* dummyName = "Imported User Cert";
    const size_t dummyNameLen = strlen(dummyName);
    SECItem* newNick = ::SECITEM_AllocItem(nullptr, nullptr, dummyNameLen + 1);
    if (!newNick) {
      return nullptr;
    }

    newNick->type = siAsciiString;
    // Dummy name, will be renamed later.
    memcpy(newNick->data, dummyName, dummyNameLen + 1);
    newNick->len = dummyNameLen;

    return newNick;
  }

  static SECStatus HandleNicknameUpdate(const CERTCertificate* aCert,
                                        const SECItem* default_nickname,
                                        SECItem** new_nickname, void* arg) {
    nsWifiResult* result = (nsWifiResult*)arg;

    nsCString userNickname;
    CopyUTF16toUTF8(result->mNickname, userNickname);

    nsCString fullNickname;
    if (aCert->isRoot && (aCert->nsCertType & NS_CERT_TYPE_SSL_CA)) {
      // Accept self-signed SSL CA as server certificate.
      fullNickname.AssignLiteral("WIFI_SERVERCERT_");
      fullNickname += userNickname;
      result->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_SERVER;
    } else if (aCert->nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
      // User Certificate
      fullNickname.AssignLiteral("WIFI_USERCERT_");
      fullNickname += userNickname;
      result->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER;
    }

    char* nickname;
    uint32_t length = fullNickname.GetMutableData(&nickname);

    SECItem* newNick = ::SECITEM_AllocItem(nullptr, nullptr, length + 1);
    if (!newNick) {
      return SECFailure;
    }

    newNick->type = siAsciiString;
    memcpy(newNick->data, nickname, length + 1);
    newNick->len = length;

    *new_nickname = newNick;
    return SECSuccess;
  }

  nsresult ImportPKCS12Blob(char* buf, uint32_t size,
                            const nsAString& aPassword) {
    SECStatus srv = SECFailure;
    nsString password(aPassword);

    // password is null-terminated wide-char string.
    // passwordItem is required to be big-endian form of password, stored in
    // char array, including the null-termination.
    uint32_t length = password.Length() + 1;
    UniqueSECItem passwordItem(::SECITEM_AllocItem(
        nullptr, nullptr, length * sizeof(nsString::char_type)));

    if (!passwordItem) {
      return NS_ERROR_FAILURE;
    }

    mozilla::NativeEndian::copyAndSwapToBigEndian(
        passwordItem->data, password.BeginReading(), length);
    // Create a decoder.
    UniqueSEC_PKCS12DecoderContext p12dcx(
        SEC_PKCS12DecoderStart(passwordItem.get(), nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr));

    if (!p12dcx) {
      return NS_ERROR_FAILURE;
    }

    // Assign data to decorder.
    srv = SEC_PKCS12DecoderUpdate(p12dcx.get(),
                                  reinterpret_cast<unsigned char*>(buf), size);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Verify certificates.
    srv = SEC_PKCS12DecoderVerify(p12dcx.get());
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Set certificate nickname and usage flag.
    srv = SEC_PKCS12DecoderRenameCertNicknames(p12dcx.get(),
                                               HandleNicknameUpdate, mResult);

    // Validate certificates.
    srv = SEC_PKCS12DecoderValidateBags(p12dcx.get(), HandleNicknameCollision);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Initialize slot.
    UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
    if (!slot) {
      return NS_ERROR_FAILURE;
    }
    if (PK11_NeedLogin(slot.get()) && PK11_NeedUserInit(slot.get())) {
      srv = PK11_InitPin(slot.get(), "", "");
      if (srv != SECSuccess) {
        return MapSECStatus(srv);
      }
    }

    // Import cert and key.
    srv = SEC_PKCS12DecoderImportBags(p12dcx.get());
    if (srv != SECSuccess) {
      PRErrorCode errCode = PR_GetError();
      if (errCode == SEC_ERROR_PKCS12_CERT_COLLISION ||
          errCode == SEC_ERROR_PKCS12_DUPLICATE_DATA) {
        mResult->mDuplicated = true;
      }
      return MapSECStatus(srv);
    }

    // User certificate must be imported from PKCS#12.
    if (!(mResult->mUsageFlag &
          nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER)) {
      return NS_ERROR_FAILURE;
    }

    mResult->mStatus = nsIWifiResult::SUCCESS;
    return NS_OK;
  }

  nsresult ReadBlob(nsCString& aBuf) {
    NS_ENSURE_ARG_POINTER(mBlob);

    static const uint64_t MAX_FILE_SIZE = 16384;

    ErrorResult rv;
    uint64_t size = mBlob->GetSize(rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    if (size > MAX_FILE_SIZE) {
      return NS_ERROR_FILE_TOO_BIG;
    }

    nsCOMPtr<nsIInputStream> inputStream;
    mBlob->CreateInputStream(getter_AddRefs(inputStream), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    rv = NS_ReadInputStreamToString(inputStream, aBuf, (uint32_t)size);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    return NS_OK;
  }

  nsresult ImportCert(CERTCertificate* aCert) {
    SECStatus srv = SECFailure;
    nsCString userNickname, fullNickname;

    CopyUTF16toUTF8(mResult->mNickname, userNickname);
    // Determine certificate nickname by adding prefix according to its type.
    if (aCert->isRoot && (aCert->nsCertType & NS_CERT_TYPE_SSL_CA)) {
      // Accept self-signed SSL CA as server certificate.
      fullNickname.AssignLiteral("WIFI_SERVERCERT_");
      fullNickname += userNickname;
      mResult->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_SERVER;
    } else if (aCert->nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
      // User Certificate
      fullNickname.AssignLiteral("WIFI_USERCERT_");
      fullNickname += userNickname;
      mResult->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER;
    } else {
      return NS_ERROR_ABORT;
    }

    char* nickname;
    uint32_t length;
    length = fullNickname.GetMutableData(&nickname);
    if (length == 0) {
      return NS_ERROR_UNEXPECTED;
    }

    // Import certificate, duplicated nickname will cause error.
    UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
    srv =
        PK11_ImportCert(slot.get(), aCert, CK_INVALID_HANDLE, nickname, false);
    if (srv != SECSuccess) {
      mResult->mDuplicated = true;
      return MapSECStatus(srv);
    }

    // Create a cert trust
    CERTCertTrust trust = {CERTDB_TRUSTED_CLIENT_CA | CERTDB_NS_TRUSTED_CA |
                               CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
                           0, 0};

    // NSS ignores the first argument to CERT_ChangeCertTrust
    srv = CERT_ChangeCertTrust(nullptr, aCert, &trust);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return NS_OK;
  }

  RefPtr<Blob> mBlob;
  RefPtr<nsWifiResult> mResult;
  nsString mPassword;
};

class DeleteCertTask final : public CryptoTask {
 public:
  DeleteCertTask(int32_t aId, const nsAString& aCertNickname) {
    MOZ_ASSERT(NS_IsMainThread());
    mResult = new nsWifiResult();
    mResult->mId = aId;
    mResult->mStatus = nsIWifiResult::ERROR_UNKNOWN;
    mResult->mNickname = aCertNickname;
    mResult->mDuplicated = false;
  }

 private:
  virtual nsresult CalculateResult() override {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCString userNickname;
    CopyUTF16toUTF8(mResult->mNickname, userNickname);

    // Delete server certificate.
    nsCString serverCertName("WIFI_SERVERCERT_", 16);
    serverCertName += userNickname;
    nsresult rv = deleteCert(serverCertName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Delete user certificate and private key.
    nsCString userCertName("WIFI_USERCERT_", 14);
    userCertName += userNickname;
    rv = deleteCert(userCertName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mResult->mStatus = nsIWifiResult::SUCCESS;
    return NS_OK;
  }

  nsresult deleteCert(const nsCString& aCertNickname) {
    SECStatus srv = SECFailure;

    UniqueCERTCertificate cert(
        CERT_FindCertByNickname(CERT_GetDefaultCertDB(), aCertNickname.get()));
    // Because we delete certificates in blind, so it's acceptable to delete
    // a non-exist certificate.
    if (!cert) {
      return NS_OK;
    }

    UniquePK11SlotInfo slot(
        PK11_KeyForCertExists(cert.get(), nullptr, nullptr));
    if (slot) {
      // Delete private key along with certificate.
      srv = PK11_DeleteTokenCertAndKey(cert.get(), nullptr);
    } else {
      srv = SEC_DeletePermCertificate(cert.get());
    }

    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return NS_OK;
  }

  virtual void CallCallback(nsresult rv) override {
    if (NS_FAILED(rv)) {
      mResult->mStatus = nsIWifiResult::ERROR_UNKNOWN;
    }

    gWifiCertService->DispatchResult(mResult);
  }

  RefPtr<nsWifiResult> mResult;
};

NS_IMPL_ISUPPORTS(WifiCertService, nsIWifiCertService)

NS_IMETHODIMP
WifiCertService::Start(nsIWifiEventListener* aListener) {
  MOZ_ASSERT(aListener);

  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
WifiCertService::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  mListener = nullptr;

  return NS_OK;
}

void WifiCertService::DispatchResult(nsWifiResult* aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCString dummyInterface;

  mListener->OnCommandResult(aResult, dummyInterface);
}

WifiCertService::WifiCertService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWifiCertService);
}

WifiCertService::~WifiCertService() { MOZ_ASSERT(!gWifiCertService); }

already_AddRefed<WifiCertService> WifiCertService::FactoryCreate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gWifiCertService) {
    gWifiCertService = new WifiCertService();
    ClearOnShutdown(&gWifiCertService);
  }

  RefPtr<WifiCertService> service = gWifiCertService.get();
  return service.forget();
}

NS_IMETHODIMP
WifiCertService::ImportCert(int32_t aId, Blob* aCertBlob,
                            const nsAString& aCertPassword,
                            const nsAString& aCertNickname) {
  RefPtr<CryptoTask> task =
      new ImportCertTask(aId, aCertBlob, aCertPassword, aCertNickname);
  return task->Dispatch();
}

NS_IMETHODIMP
WifiCertService::DeleteCert(int32_t aId, const nsAString& aCertNickname) {
  RefPtr<CryptoTask> task = new DeleteCertTask(aId, aCertNickname);
  return task->Dispatch();
}

NS_IMETHODIMP
WifiCertService::FilterCert(const nsTArray<nsString>& aCertList,
                            nsTArray<nsString>& aFilteredList) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCString certNickname, fullNickname, serverCertName, userCertName;
  serverCertName.AssignLiteral("WIFI_SERVERCERT_");
  userCertName.AssignLiteral("WIFI_USERCERT_");
  UniqueCERTCertificate cert;

  for (auto& nickname : aCertList) {
    CopyUTF16toUTF8(nickname, certNickname);

    fullNickname = serverCertName + certNickname;
    cert = (UniqueCERTCertificate)CERT_FindCertByNickname(
        CERT_GetDefaultCertDB(), fullNickname.get());
    if (cert) {
      aFilteredList.AppendElement(NS_ConvertUTF8toUTF16(fullNickname.get()));
    }

    fullNickname = userCertName + certNickname;
    cert = (UniqueCERTCertificate)CERT_FindCertByNickname(
        CERT_GetDefaultCertDB(), fullNickname.get());
    if (cert) {
      aFilteredList.AppendElement(NS_ConvertUTF8toUTF16(fullNickname.get()));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
WifiCertService::HasPrivateKey(const nsAString& aCertNickname, bool* aHasKey) {
  *aHasKey = false;

  nsCString certNickname;
  CopyUTF16toUTF8(aCertNickname, certNickname);

  UniqueCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), certNickname.get()));
  if (!cert) {
    return NS_OK;
  }

  UniquePK11SlotInfo slot(PK11_KeyForCertExists(cert.get(), nullptr, nullptr));
  if (slot) {
    *aHasKey = true;
  }

  return NS_OK;
}

}  // namespace mozilla
