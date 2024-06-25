/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SupplicantStaManager_H
#define SupplicantStaManager_H

#include "WifiCommon.h"
#include "WifiEventCallback.h"
#include "SupplicantStaNetwork.h"
#include "SupplicantCallback.h"

// There is a conflict with the value of DEBUG in DEBUG builds.
#if defined(DEBUG)
#  define OLD_DEBUG = DEBUG
#  undef DEBUG
#endif

#include <android/hidl/manager/1.0/IServiceManager.h>
#include <android/hidl/manager/1.0/IServiceNotification.h>

// AIDL
#include <android/hardware/wifi/supplicant/ISupplicant.h>
#include <android/hardware/wifi/supplicant/ISupplicantCallback.h>
#include <android/hardware/wifi/supplicant/ISupplicantP2pIface.h>
#include <android/hardware/wifi/supplicant/ISupplicantStaIface.h>
#include <android/hardware/wifi/supplicant/BnSupplicantCallback.h>

#if defined(OLD_DEBUG)
#  define DEBUG = OLD_DEBUG
#  undef OLD_DEBUG
#endif

#include "mozilla/Mutex.h"

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::manager::V1_0::IServiceManager;

// AIDL
using ::android::hardware::wifi::supplicant::IfaceInfo;
using ::android::hardware::wifi::supplicant::ISupplicant;
using ::android::hardware::wifi::supplicant::ISupplicantP2pIface;
using ::android::hardware::wifi::supplicant::ISupplicantStaIface;

BEGIN_WIFI_NAMESPACE

/**
 * Class for supplicant AIDL client implementation.
 */
class SupplicantStaManager
    : public ::android::hardware::wifi::supplicant::BnSupplicantCallback {
 public:
  static SupplicantStaManager* Get();
  static void CleanUp();
  void RegisterEventCallback(const android::sp<WifiEventCallback>& aCallback);
  void UnregisterEventCallback();

  void RegisterPasspointCallback(PasspointEventCallback* aCallback);
  void UnregisterPasspointCallback();

  // HIDL initialization
  Result_t InitInterface();
  Result_t DeinitInterface();
  bool IsInterfaceInitializing();
  bool IsInterfaceReady();

  // functions to invoke supplicant APIs
  Result_t GetMacAddress(nsAString& aMacAddress);
  Result_t GetSupportedFeatures(uint32_t& aSupportedFeatures);
  Result_t GetSupplicantDebugLevel(uint32_t& aLevel);
  Result_t SetSupplicantDebugLevel(SupplicantDebugLevelOptions* aLevel);
  Result_t SetConcurrencyPriority(bool aEnable);
  Result_t SetPowerSave(bool aEnable);
  Result_t SetSuspendMode(bool aEnable);
  Result_t SetExternalSim(bool aEnable);
  Result_t SetAutoReconnect(bool aEnable);
  Result_t SetCountryCode(const std::string& aCountryCode);
  Result_t SetBtCoexistenceMode(uint8_t aMode);
  Result_t SetBtCoexistenceScanMode(bool aEnable);
  Result_t ConnectToNetwork(ConfigurationOptions* aConfig);
  Result_t SetupStaInterface(const std::string& aInterfaceName);
  Result_t SetupP2pInterface();
  Result_t Reconnect();
  Result_t Reassociate();
  Result_t Disconnect();
  Result_t EnableNetwork();
  Result_t DisableNetwork();
  Result_t GetNetwork(nsWifiResult* aResult);
  Result_t RemoveNetworks();
  Result_t RoamToNetwork(ConfigurationOptions* aConfig);

  Result_t SendEapSimIdentityResponse(SimIdentityRespDataOptions* aIdentity);
  Result_t SendEapSimGsmAuthResponse(
      const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp);
  Result_t SendEapSimGsmAuthFailure();
  Result_t SendEapSimUmtsAuthResponse(
      SimUmtsAuthRespDataOptions* aUmtsAuthResp);
  Result_t SendEapSimUmtsAutsResponse(
      SimUmtsAutsRespDataOptions* aUmtsAutsResp);
  Result_t SendEapSimUmtsAuthFailure();
  Result_t SendAnqpRequest(const std::array<uint8_t, 6>& aBssid,
                           const std::vector<uint32_t>& aInfoElements,
                           const std::vector<uint32_t>& aHs20SubTypes);

  Result_t InitWpsDetail();
  Result_t StartWpsRegistrar(const std::string& aBssid,
                             const std::string& aPinCode);
  Result_t StartWpsPbc(const std::string& aBssid);
  Result_t StartWpsPinKeypad(const std::string& aPinCode);
  Result_t StartWpsPinDisplay(const std::string& aBssid,
                              nsAString& aGeneratedPin);
  Result_t CancelWps();

  NetworkConfiguration GetCurrentConfiguration() const;
  int32_t GetCurrentNetworkId() const;

  bool IsCurrentEapNetwork();
  bool IsCurrentPskNetwork();
  bool IsCurrentSaeNetwork();
  bool IsCurrentWepNetwork();

  // death event handler
  void RegisterDeathHandler(SupplicantDeathEventHandler* aHandler);
  void UnregisterDeathHandler();

  Result_t InitSupplicantInterface();

  virtual ~SupplicantStaManager() {}

  //...................... ISupplicantCallback ......................../

  ::android::binder::Status onInterfaceCreated(
      const ::android::String16& /*ifaceName*/) override;
  ::android::binder::Status onInterfaceRemoved(
      const ::android::String16& /*ifaceName*/) override;

 private:
  struct ServiceManagerDeathRecipient : public hidl_death_recipient {
    explicit ServiceManagerDeathRecipient(SupplicantStaManager* aOuter)
        : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    SupplicantStaManager* mOuter;
  };

  struct SupplicantDeathRecipient : public hidl_death_recipient {
    explicit SupplicantDeathRecipient(SupplicantStaManager* aOuter)
        : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    SupplicantStaManager* mOuter;
  };

  SupplicantStaManager();

  Result_t InitServiceManager();
  Result_t TearDownInterface();

  Result_t SetWpsDeviceName(const std::string& aDeviceName);
  Result_t SetWpsDeviceType(const std::string& aDeviceType);
  Result_t SetWpsManufacturer(const std::string& aManufacturer);
  Result_t SetWpsModelName(const std::string& aModelName);
  Result_t SetWpsModelNumber(const std::string& aModelNumber);
  Result_t SetWpsSerialNumber(const std::string& aSerialNumber);
  Result_t SetWpsConfigMethods(const std::string& aConfigMethods);

  android::sp<IServiceManager> GetServiceManager();
  android::sp<ISupplicant> GetSupplicant();

  bool IsSupplicantV1_3();
  bool IsSupplicantV1_1();
  bool IsSupplicantV1_2();
  bool SupplicantVersionSupported(const std::string& name);

  android::sp<ISupplicantStaIface> GetSupplicantStaIface();
  android::sp<ISupplicantP2pIface> GetSupplicantP2pIface();
  Result_t FindIfaceOfType(::android::hardware::wifi::supplicant::IfaceType aDesired, IfaceInfo* aInfo);
  android::sp<SupplicantStaNetwork> CreateStaNetwork();
  android::sp<SupplicantStaNetwork> GetStaNetwork(uint32_t aNetId) const;
  android::sp<SupplicantStaNetwork> GetCurrentNetwork() const;

  bool CompareConfiguration(const NetworkConfiguration& aOld,
                            const NetworkConfiguration& aNew);
  bool CompareCredential(const NetworkConfiguration& aOld,
                         const NetworkConfiguration& aNew);
  void NotifyTerminating();
  void SupplicantServiceDiedHandler(int32_t aCookie);

  static int16_t ConvertToWpsConfigMethod(const std::string& aConfigMethod);

  static mozilla::Mutex sLock;
  static mozilla::Mutex sHashLock;

  android::sp<::android::hidl::manager::V1_0::IServiceManager> mServiceManager;
  android::sp<ISupplicant> mSupplicant;
  android::sp<ISupplicantStaIface> mSupplicantStaIface;
  android::sp<SupplicantStaIfaceCallback> mSupplicantStaIfaceCallback;
  android::sp<ServiceManagerDeathRecipient> mServiceManagerDeathRecipient;
  android::sp<SupplicantDeathRecipient> mSupplicantDeathRecipient;

  android::sp<SupplicantDeathEventHandler> mDeathEventHandler;
  android::sp<PasspointEventCallback> mPasspointCallback;
  android::sp<WifiEventCallback> mCallback;

  int32_t mDeathRecipientCookie;
  std::string mInterfaceName;

  // For current connecting network.
  enum {
    CLEAN_ALL,
    ERASE_CONFIG,
    ADD_CONFIG,
  };
  void ModifyConfigurationHash(int aAction,
                               const NetworkConfiguration& aConfig);
  std::unordered_map<std::string, NetworkConfiguration> mCurrentConfiguration;
  std::unordered_map<std::string, android::sp<SupplicantStaNetwork>>
      mCurrentNetwork;
  NetworkConfiguration mDummyNetworkConfiguration;

  DISALLOW_COPY_AND_ASSIGN(SupplicantStaManager);
};

END_WIFI_NAMESPACE

#endif  // SupplicantStaManager_H
