/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include <android/hardware/wifi/supplicant/1.0/ISupplicantIface.h>
#include <android/hardware/wifi/supplicant/1.0/types.h>
#if ANDROID_VERSION >= 30
#  include <android/hardware/wifi/supplicant/1.3/ISupplicant.h>
#  include <android/hardware/wifi/supplicant/1.3/ISupplicantStaIface.h>
#else
#  include <android/hardware/wifi/supplicant/1.2/ISupplicant.h>
#  include <android/hardware/wifi/supplicant/1.2/ISupplicantStaIface.h>
#endif
#include <android/hardware/wifi/supplicant/1.2/ISupplicantP2pIface.h>

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
using ::android::hardware::wifi::supplicant::V1_0::Bssid;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicant;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicantIface;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicantP2pIface;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaIface;
using ::android::hardware::wifi::supplicant::V1_0::ISupplicantStaIfaceCallback;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatus;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatusCode;
using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::manager::V1_0::IServiceManager;

using ISupplicantV1_1 = android::hardware::wifi::supplicant::V1_1::ISupplicant;
using ISupplicantV1_2 = android::hardware::wifi::supplicant::V1_2::ISupplicant;
using ISupplicantStaIfaceV1_1 =
    android::hardware::wifi::supplicant::V1_1::ISupplicantStaIface;
using ISupplicantStaIfaceV1_2 =
    android::hardware::wifi::supplicant::V1_2::ISupplicantStaIface;
#if ANDROID_VERSION >= 30
using ISupplicantV1_3 = android::hardware::wifi::supplicant::V1_3::ISupplicant;
using ISupplicantStaIfaceV1_3 =
    android::hardware::wifi::supplicant::V1_3::ISupplicantStaIface;
#endif

using AnqpInfoId = ::android::hardware::wifi::supplicant::V1_0::
    ISupplicantStaIface::AnqpInfoId;
using Hs20AnqpSubtypes = ::android::hardware::wifi::supplicant::V1_0::
    ISupplicantStaIface::Hs20AnqpSubtypes;

namespace SupplicantNameSpaceV1_0 = ::android::hardware::wifi::supplicant::V1_0;

BEGIN_WIFI_NAMESPACE

/**
 * Class for supplicant HIDL client implementation.
 */
class SupplicantStaManager
    : virtual public android::hidl::manager::V1_0::IServiceNotification,
      virtual public SupplicantNameSpaceV1_0::ISupplicantCallback {
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

  virtual ~SupplicantStaManager() {}

  // IServiceNotification::onRegistration
  virtual Return<void> onRegistration(const hidl_string& fqName,
                                      const hidl_string& name,
                                      bool preexisting) override;

 private:
  //...................... ISupplicantCallback ......................../
  /**
   * Used to indicate that a new interface has been created.
   *
   * @param ifName Name of the network interface, e.g., wlan0
   */
  Return<void> onInterfaceCreated(const hidl_string& ifName) override;
  /**
   * Used to indicate that an interface has been removed.
   *
   * @param ifName Name of the network interface, e.g., wlan0
   */
  Return<void> onInterfaceRemoved(const hidl_string& ifName) override;
  /**
   * Used to indicate that the supplicant daemon is terminating.
   */
  Return<void> onTerminating() override;

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
  Result_t InitSupplicantInterface();
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
  android::sp<ISupplicantV1_1> GetSupplicantV1_1();
  android::sp<ISupplicantV1_2> GetSupplicantV1_2();
  android::sp<ISupplicantStaIfaceV1_1> GetSupplicantStaIfaceV1_1();
  android::sp<ISupplicantStaIfaceV1_2> GetSupplicantStaIfaceV1_2();
#if ANDROID_VERSION >= 30
  android::sp<ISupplicantV1_3> GetSupplicantV1_3();
  android::sp<ISupplicantStaIfaceV1_3> GetSupplicantStaIfaceV1_3();

  bool IsSupplicantV1_3();
#endif

  bool IsSupplicantV1_1();
  bool IsSupplicantV1_2();
  bool SupplicantVersionSupported(const std::string& name);

  android::sp<ISupplicantStaIface> GetSupplicantStaIface();
  android::sp<ISupplicantStaIface> AddSupplicantStaIface();
  android::sp<ISupplicantP2pIface> GetSupplicantP2pIface();
  Result_t FindIfaceOfType(SupplicantNameSpaceV1_0::IfaceType aDesired,
                           ISupplicant::IfaceInfo* aInfo);
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
  android::sp<ISupplicantStaIfaceCallback> mSupplicantStaIfaceCallback;
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
