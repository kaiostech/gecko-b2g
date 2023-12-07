/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmUtils_H
#define GonkDrmUtils_H

#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsTArray.h"

#include <binder/Parcel.h>
#include <hidl/HidlSupport.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include <string>
#include <utility>
#include <vector>

#define GD_LOG(level, fmt, args...) \
  MOZ_LOG(::mozilla::GetGonkDrmLog(), level, (fmt, ##args))
#define GD_LOGV_TEST() \
  MOZ_LOG_TEST(::mozilla::GetGonkDrmLog(), ::mozilla::LogLevel::Verbose)

#define GD_LOGE(args...) GD_LOG(::mozilla::LogLevel::Error, ##args)
#define GD_LOGW(args...) GD_LOG(::mozilla::LogLevel::Warning, ##args)
#define GD_LOGI(args...) GD_LOG(::mozilla::LogLevel::Info, ##args)
#define GD_LOGD(args...) GD_LOG(::mozilla::LogLevel::Debug, ##args)
#define GD_LOGV(args...) GD_LOG(::mozilla::LogLevel::Verbose, ##args)

#define GD_ASSERT(cond) MOZ_ASSERT(cond);

namespace mozilla {
LogModule* GetGonkDrmLog();
}  // namespace mozilla

namespace android {

class ICrypto;
class IDrm;
class Parcel;

class GonkDrmUtils final {
 public:
  typedef std::function<void(bool /* aSuccess */,
                             const nsACString& /* aResponse */)>
      ProvisioningCallback;

  static void StartProvisioning(const nsACString& aUrl,
                                const nsACString& aRequest,
                                const ProvisioningCallback& aCallback);

  static sp<IDrm> MakeDrm(const nsAString& aKeySystem);

  static sp<ICrypto> MakeCrypto(const nsAString& aKeySystem,
                                const Vector<uint8_t>& aSessionId);

  static bool IsSchemeSupported(const nsAString& aKeySystem);

  static Vector<uint8_t> ReadByteVectorFromParcel(const Parcel* aParcel);

  static nsTArray<uint8_t> ReadNsByteArrayFromParcel(const Parcel* aParcel);

  static nsCString EncodeBase64(const Vector<uint8_t>& aVector);

  static Vector<uint8_t> DecodeBase64(const nsACString& aString);

  static Vector<uint8_t> DecodeBase64URL(const nsACString& aString);

  static nsCString GenerateUUID();

 private:
  static const uint8_t* GetKeySystemUUID(const nsAString& aKeySystem);

  static sp<IDrm> MakeDrm();

  static sp<ICrypto> MakeCrypto();
};

class GonkDrmConverter final {
 public:
  template <typename T>
  static Vector<uint8_t> ToByteVector(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    Vector<uint8_t> vector;
    vector.appendArray(reinterpret_cast<const uint8_t*>(rawData.first),
                       rawData.second);
    return vector;
  }

  template <typename T>
  static String8 ToString8(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    return String8(reinterpret_cast<const char*>(rawData.first),
                   rawData.second);
  }

  template <typename T>
  static nsTArray<uint8_t> ToNsByteArray(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    return nsTArray<uint8_t>(reinterpret_cast<const uint8_t*>(rawData.first),
                             rawData.second);
  }

  template <typename T>
  static nsCString ToNsCString(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    return nsCString(reinterpret_cast<const char*>(rawData.first),
                     rawData.second);
  }

  template <typename T>
  static std::vector<uint8_t> ToStdByteVector(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    return std::vector<uint8_t>(
        reinterpret_cast<const uint8_t*>(rawData.first),
        reinterpret_cast<const uint8_t*>(rawData.first) + rawData.second);
  }

  template <typename T>
  static std::string ToStdString(const T& aBytes) {
    auto rawData = GetRawData(aBytes);
    return std::string(reinterpret_cast<const char*>(rawData.first),
                       rawData.second);
  }

 private:
  typedef std::pair<const void*, size_t> RawData;

  static inline RawData GetRawData(const Vector<uint8_t>& aVector) {
    return RawData{aVector.array(), aVector.size()};
  }

  static inline RawData GetRawData(const hardware::hidl_vec<uint8_t>& aVector) {
    return RawData{aVector.data(), aVector.size()};
  }

  static inline RawData GetRawData(const String8& aString) {
    return RawData{aString.c_str(), aString.length()};
  }

  template <typename Alloc>
  static inline RawData GetRawData(
      const nsTArray_Impl<uint8_t, Alloc>& aArray) {
    return RawData{aArray.Elements(), aArray.Length()};
  }

  static inline RawData GetRawData(const nsACString& aString) {
    return RawData{aString.Data(), aString.Length()};
  }

  static inline RawData GetRawData(const std::vector<uint8_t>& aVector) {
    return RawData{aVector.data(), aVector.size()};
  }

  static inline RawData GetRawData(const std::string& aString) {
    return RawData{aString.c_str(), aString.size()};
  }
};

}  // namespace android

#endif
