/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiNative_h
#define WifiNative_h

#include "PasspointHandler.h"
#include "WifiEventCallback.h"
#include "WifiHalManager.h"
#include "WificondControl.h"
#include "SoftapManager.h"
#include "SupplicantStaManager.h"
#include "nsString.h"
#include "nsWifiElement.h"
#include "nsIWifiCommand.h"
#include "mozilla/UniquePtr.h"

BEGIN_WIFI_NAMESPACE

class WifiNative {
 public:
  explicit WifiNative();

  bool ExecuteCommand(CommandOptions& aOptions, nsWifiResult* aResult,
                      const nsCString& aInterface);

  void RegisterEventCallback(WifiEventCallback* aCallback);
  void UnregisterEventCallback();

 private:
  Result_t InitHal();
  Result_t DeinitHal();
  Result_t GetSupportedFeatures(uint32_t& aSupportedFeatures);
  Result_t GetDriverModuleInfo(nsAString& aDriverVersion,
                               nsAString& aFirmwareVersion);
  Result_t SetLowLatencyMode(bool aEnable);
  Result_t SetConcurrencyPriority(bool aEnable);
  Result_t GetHostWakeReason(uint32_t& aCapabilities);

  Result_t StartWifi(nsAString& aIfaceName);
  Result_t StopWifi();
  Result_t GetMacAddress(nsAString& aMacAddress);
  Result_t GetClientInterfaceName(nsAString& aIfaceName);
  Result_t GetSoftApInterfaceName(nsAString& aIfaceName);

  Result_t StartSupplicant();
  Result_t StopSupplicant();
  Result_t GetDebugLevel(uint32_t& aDebugLevel);
  Result_t SetDebugLevel(SupplicantDebugLevelOptions* aLevel);
  Result_t SetPowerSave(bool aEnable);
  Result_t SetSuspendMode(bool aEnable);
  Result_t SetExternalSim(bool aEnable);
  Result_t SetAutoReconnect(bool aEnable);
  Result_t SetCountryCode(const nsAString& aCountryCode);
  Result_t SetBtCoexistenceMode(uint8_t aMode);
  Result_t SetBtCoexistenceScanMode(bool aEnable);
  Result_t SignalPoll(nsWifiResult* aResult);
  Result_t GetLinkLayerStats(nsWifiResult* aResult);
  Result_t SetFirmwareRoaming(bool aEnable);
  Result_t ConfigureFirmwareRoaming(
      RoamingConfigurationOptions* aRoamingConfig);

  Result_t StartSingleScan(ScanSettingsOptions* aScanSettings);
  Result_t StopSingleScan();
  Result_t StartPnoScan(PnoScanSettingsOptions* aPnoScanSettings);
  Result_t StopPnoScan();
  Result_t GetScanResults(int32_t aScanType, nsWifiResult* aResult);
  Result_t GetSingleScanResults(
      std::vector<Wificond::NativeScanResult>& aScanResults);
  Result_t GetPnoScanResults(
      std::vector<Wificond::NativeScanResult>& aPnoScanResults);
  Result_t GetChannelsForBand(uint32_t aBandMask, nsWifiResult* aResult);

  Result_t Connect(ConfigurationOptions* aConfig);
  Result_t Reconnect();
  Result_t Reassociate();
  Result_t Disconnect();
  Result_t EnableNetwork();
  Result_t DisableNetwork();
  Result_t GetNetwork(nsWifiResult* aResult);
  Result_t RemoveNetworks();
  Result_t StartRoaming(ConfigurationOptions* aConfig);
  Result_t SendEapSimIdentityResponse(SimIdentityRespDataOptions* aIdentity);
  Result_t SendEapSimGsmAuthResponse(
      const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp);
  Result_t SendEapSimGsmAuthFailure();
  Result_t SendEapSimUmtsAuthResponse(
      SimUmtsAuthRespDataOptions* aUmtsAuthResp);
  Result_t SendEapSimUmtsAutsResponse(
      SimUmtsAutsRespDataOptions* aUmtsAutsResp);
  Result_t SendEapSimUmtsAuthFailure();
  Result_t RequestAnqp(AnqpRequestSettingsOptions* aRequest);

  Result_t InitWpsDetail();
  Result_t StartWpsRegistrar(WpsConfigurationOptions* aConfig);
  Result_t StartWpsPbc(WpsConfigurationOptions* aConfig);
  Result_t StartWpsPinKeypad(WpsConfigurationOptions* aConfig);
  Result_t StartWpsPinDisplay(WpsConfigurationOptions* aConfig,
                              nsAString& aGeneratedPin);
  Result_t CancelWps();

  Result_t StartSoftAp(SoftapConfigurationOptions* aSoftapConfig,
                       nsAString& aIfaceName);
  Result_t StopSoftAp();
  Result_t StartAndConnectHostapd();
  Result_t StopHostapd();
  Result_t GetSoftapStations(uint32_t& aNumStations);

  class SupplicantDeathHandler : virtual public SupplicantDeathEventHandler {
    virtual void OnDeath() override;
  };

  static WifiHal* sWifiHal;
  static WificondControl* sWificondControl;
  static SupplicantStaManager* sSupplicantStaManager;
  static SoftapManager* sSoftapManager;
  static android::sp<WifiEventCallback> sCallback;
  static RefPtr<PasspointHandler> sPasspointHandler;

  std::string mStaInterfaceName;
  std::string mApInterfaceName;

  uint32_t mSupportedFeatures;
};

END_WIFI_NAMESPACE

#endif  // WifiNative_h
