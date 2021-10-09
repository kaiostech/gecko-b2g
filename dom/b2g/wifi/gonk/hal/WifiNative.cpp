/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "WifiNative"

#include "WifiNative.h"
#include <cutils/properties.h>
#include <string.h>
#include "js/CharacterEncoding.h"

using namespace mozilla::dom;
using namespace mozilla::dom::wifi;

static const int32_t CONNECTION_RETRY_INTERVAL_US = 100000;
static const int32_t CONNECTION_RETRY_TIMES = 50;

/* static */
WifiHal* WifiNative::sWifiHal = nullptr;
WificondControl* WifiNative::sWificondControl = nullptr;
SoftapManager* WifiNative::sSoftapManager = nullptr;
SupplicantStaManager* WifiNative::sSupplicantStaManager = nullptr;
RefPtr<PasspointHandler> WifiNative::sPasspointHandler;
android::sp<WifiEventCallback> WifiNative::sCallback;

WifiNative::WifiNative() : mSupportedFeatures(0) {
  sWifiHal = WifiHal::Get();
  sWificondControl = WificondControl::Get();
  sSoftapManager = SoftapManager::Get();
  sSupplicantStaManager = SupplicantStaManager::Get();
  sPasspointHandler = PasspointHandler::Get();
}

bool WifiNative::ExecuteCommand(CommandOptions& aOptions, nsWifiResult* aResult,
                                const nsCString& aInterface) {
  // Always correlate the opaque ids.
  aResult->mId = aOptions.mId;

  if (aOptions.mCmd == nsIWifiCommand::INITIALIZE) {
    aResult->mStatus = InitHal();
  } else if (aOptions.mCmd == nsIWifiCommand::GET_MODULE_VERSION) {
    aResult->mStatus =
        GetDriverModuleInfo(aResult->mDriverVersion, aResult->mFirmwareVersion);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_SUPPORTED_FEATURES) {
    aResult->mStatus = GetSupportedFeatures(aResult->mSupportedFeatures);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_LOW_LATENCY_MODE) {
    aResult->mStatus = SetLowLatencyMode(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_CONCURRENCY_PRIORITY) {
    aResult->mStatus = SetConcurrencyPriority(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::START_WIFI) {
    aResult->mStatus = StartWifi(aResult->mStaInterface);
  } else if (aOptions.mCmd == nsIWifiCommand::STOP_WIFI) {
    aResult->mStatus = StopWifi();
  } else if (aOptions.mCmd == nsIWifiCommand::GET_MAC_ADDRESS) {
    aResult->mStatus = GetMacAddress(aResult->mMacAddress);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_STA_IFACE) {
    aResult->mStatus = GetClientInterfaceName(aResult->mStaInterface);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_DEBUG_LEVEL) {
    aResult->mStatus = GetDebugLevel(aResult->mDebugLevel);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_DEBUG_LEVEL) {
    aResult->mStatus = SetDebugLevel(&aOptions.mDebugLevel);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_POWER_SAVE) {
    aResult->mStatus = SetPowerSave(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_SUSPEND_MODE) {
    aResult->mStatus = SetSuspendMode(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_EXTERNAL_SIM) {
    aResult->mStatus = SetExternalSim(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_AUTO_RECONNECT) {
    aResult->mStatus = SetAutoReconnect(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_COUNTRY_CODE) {
    aResult->mStatus = SetCountryCode(aOptions.mCountryCode);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_BT_COEXIST_MODE) {
    aResult->mStatus = SetBtCoexistenceMode(aOptions.mBtCoexistenceMode);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_BT_COEXIST_SCAN_MODE) {
    aResult->mStatus = SetBtCoexistenceScanMode(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_LINK_LAYER_STATS) {
    aResult->mStatus = GetLinkLayerStats(aResult);
  } else if (aOptions.mCmd == nsIWifiCommand::SIGNAL_POLL) {
    aResult->mStatus = SignalPoll(aResult);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_FIRMWARE_ROAMING) {
    aResult->mStatus = SetFirmwareRoaming(aOptions.mEnabled);
  } else if (aOptions.mCmd == nsIWifiCommand::CONFIG_FIRMWARE_ROAMING) {
    aResult->mStatus = ConfigureFirmwareRoaming(&aOptions.mRoamingConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::START_SINGLE_SCAN) {
    aResult->mStatus = StartSingleScan(&aOptions.mScanSettings);
  } else if (aOptions.mCmd == nsIWifiCommand::STOP_SINGLE_SCAN) {
    aResult->mStatus = StopSingleScan();
  } else if (aOptions.mCmd == nsIWifiCommand::START_PNO_SCAN) {
    aResult->mStatus = StartPnoScan(&aOptions.mPnoScanSettings);
  } else if (aOptions.mCmd == nsIWifiCommand::STOP_PNO_SCAN) {
    aResult->mStatus = StopPnoScan();
  } else if (aOptions.mCmd == nsIWifiCommand::GET_SCAN_RESULTS) {
    aResult->mStatus = GetScanResults(aOptions.mScanType, aResult);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_CHANNELS_FOR_BAND) {
    aResult->mStatus = GetChannelsForBand(aOptions.mBandMask, aResult);
  } else if (aOptions.mCmd == nsIWifiCommand::CONNECT) {
    aResult->mStatus = Connect(&aOptions.mConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::RECONNECT) {
    aResult->mStatus = Reconnect();
  } else if (aOptions.mCmd == nsIWifiCommand::REASSOCIATE) {
    aResult->mStatus = Reassociate();
  } else if (aOptions.mCmd == nsIWifiCommand::DISCONNECT) {
    aResult->mStatus = Disconnect();
  } else if (aOptions.mCmd == nsIWifiCommand::ENABLE_NETWORK) {
    aResult->mStatus = EnableNetwork();
  } else if (aOptions.mCmd == nsIWifiCommand::DISABLE_NETWORK) {
    aResult->mStatus = DisableNetwork();
  } else if (aOptions.mCmd == nsIWifiCommand::GET_NETWORK) {
    aResult->mStatus = GetNetwork(aResult);
  } else if (aOptions.mCmd == nsIWifiCommand::REMOVE_NETWORKS) {
    aResult->mStatus = RemoveNetworks();
  } else if (aOptions.mCmd == nsIWifiCommand::START_ROAMING) {
    aResult->mStatus = StartRoaming(&aOptions.mConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_IDENTITY_RESPONSE) {
    aResult->mStatus = SendEapSimIdentityResponse(&aOptions.mIdentityResp);
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_GSM_AUTH_RESPONSE) {
    aResult->mStatus = SendEapSimGsmAuthResponse(aOptions.mGsmAuthResp);
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_GSM_AUTH_FAILURE) {
    aResult->mStatus = SendEapSimGsmAuthFailure();
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_UMTS_AUTH_RESPONSE) {
    aResult->mStatus = SendEapSimUmtsAuthResponse(&aOptions.mUmtsAuthResp);
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_UMTS_AUTS_RESPONSE) {
    aResult->mStatus = SendEapSimUmtsAutsResponse(&aOptions.mUmtsAutsResp);
  } else if (aOptions.mCmd == nsIWifiCommand::SEND_UMTS_AUTH_FAILURE) {
    aResult->mStatus = SendEapSimUmtsAuthFailure();
  } else if (aOptions.mCmd == nsIWifiCommand::REQUEST_ANQP) {
    aResult->mStatus = RequestAnqp(&aOptions.mAnqpSettings);
  } else if (aOptions.mCmd == nsIWifiCommand::SET_WPS_DETAIL) {
    aResult->mStatus = InitWpsDetail();
  } else if (aOptions.mCmd == nsIWifiCommand::WPS_REGISTRAR) {
    aResult->mStatus = StartWpsRegistrar(&aOptions.mWpsConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::WPS_PBC) {
    aResult->mStatus = StartWpsPbc(&aOptions.mWpsConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::WPS_PIN_KEYPAD) {
    aResult->mStatus = StartWpsPinKeypad(&aOptions.mWpsConfig);
  } else if (aOptions.mCmd == nsIWifiCommand::WPS_PIN_DISPLAY) {
    aResult->mStatus =
        StartWpsPinDisplay(&aOptions.mWpsConfig, aResult->mGeneratedPin);
  } else if (aOptions.mCmd == nsIWifiCommand::WPS_CANCEL) {
    aResult->mStatus = CancelWps();
  } else if (aOptions.mCmd == nsIWifiCommand::START_SOFTAP) {
    aResult->mStatus =
        StartSoftAp(&aOptions.mSoftapConfig, aResult->mApInterface);
  } else if (aOptions.mCmd == nsIWifiCommand::STOP_SOFTAP) {
    aResult->mStatus = StopSoftAp();
  } else if (aOptions.mCmd == nsIWifiCommand::GET_AP_IFACE) {
    aResult->mStatus = GetSoftApInterfaceName(aResult->mApInterface);
  } else if (aOptions.mCmd == nsIWifiCommand::GET_SOFTAP_STATION_NUMBER) {
    aResult->mStatus = GetSoftapStations(aResult->mNumStations);
  } else {
    WIFI_LOGE(LOG_TAG, "ExecuteCommand: Unknown command %d", aOptions.mCmd);
    return false;
  }
  return true;
}

void WifiNative::RegisterEventCallback(WifiEventCallback* aCallback) {
  sCallback = aCallback;
  if (sSupplicantStaManager) {
    sSupplicantStaManager->RegisterEventCallback(sCallback);
  }
}

void WifiNative::UnregisterEventCallback() {
  if (sSupplicantStaManager) {
    sSupplicantStaManager->UnregisterEventCallback();
  }
  sCallback = nullptr;
}

Result_t WifiNative::InitHal() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // make sure wifi hal is ready
  result = sWifiHal->InitHalInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  result = sWificondControl->InitWificondInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  // init supplicant hal
  if (!sSupplicantStaManager->IsInterfaceInitializing()) {
    result = sSupplicantStaManager->InitInterface();
    if (result != nsIWifiResult::SUCCESS) {
      return result;
    }
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::DeinitHal() { return nsIWifiResult::SUCCESS; }

Result_t WifiNative::GetSupportedFeatures(uint32_t& aSupportedFeatures) {
  // Return the cached value if already set.
  // So that we can still query the result while wifi is off.
  if (mSupportedFeatures != 0) {
    aSupportedFeatures = mSupportedFeatures;
    return nsIWifiResult::SUCCESS;
  }

  if (sWifiHal->GetSupportedFeatures(aSupportedFeatures) !=
      nsIWifiResult::SUCCESS) {
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (sSupplicantStaManager->GetSupportedFeatures(aSupportedFeatures) !=
      nsIWifiResult::SUCCESS) {
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mSupportedFeatures = aSupportedFeatures;
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::GetDriverModuleInfo(nsAString& aDriverVersion,
                                         nsAString& aFirmwareVersion) {
  return sWifiHal->GetDriverModuleInfo(aDriverVersion, aFirmwareVersion);
}

Result_t WifiNative::SetLowLatencyMode(bool aEnable) {
  return sWifiHal->SetLowLatencyMode(aEnable);
}

Result_t WifiNative::SetConcurrencyPriority(bool aEnable) {
  return sSupplicantStaManager->SetConcurrencyPriority(aEnable);
}

/**
 * To enable wifi and start supplicant
 *
 * @param aIfaceName - output wlan module interface name
 *
 * 1. load wifi driver module, configure chip.
 * 2. setup client mode interface.
 * 3. start supplicant.
 */
Result_t WifiNative::StartWifi(nsAString& aIfaceName) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  if (sWifiHal->CheckWifiStarted()) {
    StopWifi();
  }

  result = sWifiHal->StartWifiModule();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to start wifi");
    return result;
  }

  WIFI_LOGD(LOG_TAG, "module loaded, try to configure...");
  result = sWifiHal->ConfigChipAndCreateIface(wifiNameSpaceV1_0::IfaceType::STA,
                                              mStaInterfaceName);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to create sta interface");
    return result;
  } else {
    sWifiHal->EnableLinkLayerStats();
  }

  result = StartSupplicant();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to initialize supplicant");
    return result;
  }

  // supplicant initialized, register death handler
  SupplicantDeathHandler* deathHandler = new SupplicantDeathHandler();
  sSupplicantStaManager->RegisterDeathHandler(deathHandler);

  result = sWificondControl->SetupClientIface(mStaInterfaceName, sCallback);

  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to setup iface in wificond");
    sWificondControl->TearDownClientInterface(mStaInterfaceName);
    return result;
  }

  // Initiate passpoint handler
  if (sPasspointHandler) {
    sPasspointHandler->SetSupplicantManager(sSupplicantStaManager);
    sPasspointHandler->RegisterEventCallback(sCallback);
  }

  result = sSupplicantStaManager->SetupStaInterface(mStaInterfaceName);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to setup iface in supplicant");
    return result;
  }

  nsString iface(NS_ConvertUTF8toUTF16(mStaInterfaceName.c_str()));
  aIfaceName.Assign(iface);
  return CHECK_SUCCESS(aIfaceName.Length() > 0);
}

/**
 * To disable wifi
 *
 * 1. clean supplicant hidl client and stop supplicant
 * 2. clean client interfaces in wificond
 * 3. clean wifi hidl client and unload wlan module
 */
Result_t WifiNative::StopWifi() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = StopSupplicant();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to stop supplicant");
    return result;
  }

  if (sPasspointHandler) {
    sPasspointHandler->Cleanup();
  }

  // TODO: It's the only method to unsubscribe regulatory domain change event,
  //       implement remain interface check if we support multiple interfaces
  //       case such as p2p or concurrent mode.
  result = sWificondControl->TearDownInterfaces();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown interfaces in wificond");
    return result;
  }

  // unregister supplicant death Handler
  sSupplicantStaManager->UnregisterDeathHandler();

  result = sWifiHal->TearDownInterface(wifiNameSpaceV1_0::IfaceType::STA);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to stop wifi");
    return result;
  }
  return nsIWifiResult::SUCCESS;
}

/**
 * Steps to setup supplicant.
 *
 * 1. initialize supplicant hidl client.
 * 2. start supplicant daemon through wificond or ctl.stat.
 * 3. wait for hidl client registration ready.
 */
Result_t WifiNative::StartSupplicant() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // start supplicant hal
  if (!sSupplicantStaManager->IsInterfaceReady()) {
    result = sSupplicantStaManager->InitInterface();
    if (result != nsIWifiResult::SUCCESS) {
      WIFI_LOGE(LOG_TAG, "Failed to initialize supplicant hal");
      return result;
    }
  }

  // start supplicant from wificond.
  result = sWificondControl->StartSupplicant();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to start supplicant daemon");
    return result;
  }

  bool connected = false;
  int32_t connectTries = 0;
  while (!connected && connectTries++ < CONNECTION_RETRY_TIMES) {
    // Check if the initialization is complete.
    if (sSupplicantStaManager->IsInterfaceReady()) {
      connected = true;
      break;
    }
    usleep(CONNECTION_RETRY_INTERVAL_US);
  }
  return CHECK_SUCCESS(connected);
}

Result_t WifiNative::StopSupplicant() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // teardown supplicant hal interfaces
  result = sSupplicantStaManager->DeinitInterface();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown iface in supplicant");
    return result;
  }

  result = sWificondControl->StopSupplicant();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to stop supplicant");
    return result;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::GetMacAddress(nsAString& aMacAddress) {
  return sSupplicantStaManager->GetMacAddress(aMacAddress);
}

Result_t WifiNative::GetClientInterfaceName(nsAString& aIfaceName) {
  nsString iface(NS_ConvertUTF8toUTF16(mStaInterfaceName.c_str()));
  aIfaceName.Assign(iface);
  return CHECK_SUCCESS(aIfaceName.Length() > 0);
}

Result_t WifiNative::GetSoftApInterfaceName(nsAString& aIfaceName) {
  nsString iface(NS_ConvertUTF8toUTF16(mApInterfaceName.c_str()));
  aIfaceName.Assign(iface);
  return CHECK_SUCCESS(aIfaceName.Length() > 0);
}

Result_t WifiNative::GetDebugLevel(uint32_t& aLevel) {
  return sSupplicantStaManager->GetSupplicantDebugLevel(aLevel);
}

Result_t WifiNative::SetDebugLevel(SupplicantDebugLevelOptions* aLevel) {
  CONFIG_WIFI_DEBUG(aLevel->mLogLevel < nsISupplicantDebugLevel::LOG_INFO);
  return sSupplicantStaManager->SetSupplicantDebugLevel(aLevel);
}

Result_t WifiNative::SetPowerSave(bool aEnable) {
  return sSupplicantStaManager->SetPowerSave(aEnable);
}

Result_t WifiNative::SetSuspendMode(bool aEnable) {
  return sSupplicantStaManager->SetSuspendMode(aEnable);
}

Result_t WifiNative::SetExternalSim(bool aEnable) {
  return sSupplicantStaManager->SetExternalSim(aEnable);
}

Result_t WifiNative::SetAutoReconnect(bool aEnable) {
  return sSupplicantStaManager->SetAutoReconnect(aEnable);
}

Result_t WifiNative::SetBtCoexistenceMode(uint8_t aMode) {
  return sSupplicantStaManager->SetBtCoexistenceMode(aMode);
}

Result_t WifiNative::SetBtCoexistenceScanMode(bool aEnable) {
  return sSupplicantStaManager->SetBtCoexistenceScanMode(aEnable);
}

Result_t WifiNative::SignalPoll(nsWifiResult* aResult) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;
  std::vector<int32_t> pollResult;

  result = sWificondControl->SignalPoll(pollResult);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to poll signal strength");
    return result;
  }

  size_t numResults = pollResult.size();
  if (numResults > 0) {
    nsTArray<int32_t> signalArray(numResults);
    for (int32_t& element : pollResult) {
      signalArray.AppendElement(element);
    }
    aResult->updateSignalPoll(signalArray);
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::GetLinkLayerStats(nsWifiResult* aResult) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;
  wifiNameSpaceV1_3::StaLinkLayerStats stats;

  result = sWifiHal->GetLinkLayerStats(stats);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to get link layer statistics");
    return result;
  }

  RefPtr<nsLinkLayerStats> linkLayerStats = new nsLinkLayerStats(
      stats.iface.beaconRx, stats.iface.avgRssiMgmt, stats.timeStampInMs);

  RefPtr<nsLinkLayerPacketStats> wmeBePktStats = new nsLinkLayerPacketStats(
      stats.iface.wmeBePktStats.rxMpdu, stats.iface.wmeBePktStats.txMpdu,
      stats.iface.wmeBePktStats.lostMpdu, stats.iface.wmeBePktStats.retries);

  RefPtr<nsLinkLayerPacketStats> wmeBkPktStats = new nsLinkLayerPacketStats(
      stats.iface.wmeBkPktStats.rxMpdu, stats.iface.wmeBkPktStats.txMpdu,
      stats.iface.wmeBkPktStats.lostMpdu, stats.iface.wmeBkPktStats.retries);

  RefPtr<nsLinkLayerPacketStats> wmeViPktStats = new nsLinkLayerPacketStats(
      stats.iface.wmeViPktStats.rxMpdu, stats.iface.wmeViPktStats.txMpdu,
      stats.iface.wmeViPktStats.lostMpdu, stats.iface.wmeViPktStats.retries);

  RefPtr<nsLinkLayerPacketStats> wmeVoPktStats = new nsLinkLayerPacketStats(
      stats.iface.wmeVoPktStats.rxMpdu, stats.iface.wmeVoPktStats.txMpdu,
      stats.iface.wmeVoPktStats.lostMpdu, stats.iface.wmeVoPktStats.retries);

  size_t numRadio = stats.radios.size();
  nsTArray<RefPtr<nsLinkLayerRadioStats>> radios(numRadio);

  for (auto& radio : stats.radios) {
    size_t numTxTime = radio.V1_0.txTimeInMsPerLevel.size();
    nsTArray<uint32_t> txTimeInMsPerLevel(numTxTime);

    for (auto& txTime : radio.V1_0.txTimeInMsPerLevel) {
      txTimeInMsPerLevel.AppendElement(txTime);
    }
    RefPtr<nsLinkLayerRadioStats> radioStats = new nsLinkLayerRadioStats(
        radio.V1_0.onTimeInMs, radio.V1_0.txTimeInMs, radio.V1_0.rxTimeInMs,
        radio.V1_0.onTimeInMsForScan, txTimeInMsPerLevel);
    radios.AppendElement(radioStats);
  }

  linkLayerStats->updatePacketStats(wmeBePktStats, wmeBkPktStats, wmeViPktStats,
                                    wmeVoPktStats);
  linkLayerStats->updateRadioStats(radios);
  aResult->updateLinkLayerStats(linkLayerStats);
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::SetCountryCode(const nsAString& aCountryCode) {
  std::string countryCode = NS_ConvertUTF16toUTF8(aCountryCode).get();
  return sSupplicantStaManager->SetCountryCode(countryCode);
}

Result_t WifiNative::SetFirmwareRoaming(bool aEnable) {
  return sWifiHal->SetFirmwareRoaming(aEnable);
}

Result_t WifiNative::ConfigureFirmwareRoaming(
    RoamingConfigurationOptions* aRoamingConfig) {
  return sWifiHal->ConfigureFirmwareRoaming(aRoamingConfig);
}

Result_t WifiNative::StartSingleScan(ScanSettingsOptions* aScanSettings) {
  return sWificondControl->StartSingleScan(aScanSettings);
}

Result_t WifiNative::StopSingleScan() {
  return sWificondControl->StopSingleScan();
}

Result_t WifiNative::StartPnoScan(PnoScanSettingsOptions* aPnoScanSettings) {
  return sWificondControl->StartPnoScan(aPnoScanSettings);
}

Result_t WifiNative::StopPnoScan() { return sWificondControl->StopPnoScan(); }

Result_t WifiNative::GetScanResults(int32_t aScanType, nsWifiResult* aResult) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;
  std::vector<Wificond::NativeScanResult> nativeScanResults;

  if (aScanType == nsIScanSettings::USE_SINGLE_SCAN) {
    result = GetSingleScanResults(nativeScanResults);
  } else if (aScanType == nsIScanSettings::USE_PNO_SCAN) {
    result = GetPnoScanResults(nativeScanResults);
  } else {
    WIFI_LOGE(LOG_TAG, "Invalid scan type: %d", aScanType);
  }

  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "GetScanResults failed");
    return result;
  }

  nsTArray<RefPtr<nsScanResult>> scanResults(nativeScanResults.size());
  for (auto nativeResult : nativeScanResults) {
    std::string ssidStr(nativeResult.ssid.begin(), nativeResult.ssid.end());
    std::string bssidStr = ConvertMacToString(nativeResult.bssid);
    nsString ssid(NS_ConvertUTF8toUTF16(ssidStr.c_str()));
    nsString bssid(NS_ConvertUTF8toUTF16(bssidStr.c_str()));

    nsTArray<uint8_t> infoElement(nativeResult.info_element.size());
    for (auto& element : nativeResult.info_element) {
      infoElement.AppendElement(element);
    }
    RefPtr<nsScanResult> scanResult =
        new nsScanResult(ssid, bssid, infoElement, nativeResult.frequency,
                         nativeResult.tsf, nativeResult.capability,
                         nativeResult.signal_mbm, nativeResult.associated);
    scanResults.AppendElement(scanResult);
  }
  aResult->updateScanResults(scanResults);
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::GetSingleScanResults(
    std::vector<Wificond::NativeScanResult>& aScanResults) {
  return sWificondControl->GetSingleScanResults(aScanResults);
}

Result_t WifiNative::GetPnoScanResults(
    std::vector<Wificond::NativeScanResult>& aPnoScanResults) {
  return sWificondControl->GetPnoScanResults(aPnoScanResults);
}

Result_t WifiNative::GetChannelsForBand(uint32_t aBandMask,
                                        nsWifiResult* aResult) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;
  std::vector<int32_t> channels;

  result = sWificondControl->GetChannelsForBand(aBandMask, channels);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to get supported channels");
    return result;
  }

  size_t numChannels = channels.size();
  if (numChannels > 0) {
    nsTArray<int32_t> channelArray(numChannels);
    for (int32_t& channel : channels) {
      channelArray.AppendElement(channel);
    }
    aResult->updateChannels(channelArray);
  }
  return nsIWifiResult::SUCCESS;
}

/**
 * To make wifi connection with assigned configuration
 *
 * @param aConfig - the network configuration to be set
 */
Result_t WifiNative::Connect(ConfigurationOptions* aConfig) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // abort ongoing scan before connect
  sWificondControl->StopSingleScan();

  result = sSupplicantStaManager->ConnectToNetwork(aConfig);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to connect %s",
              NS_ConvertUTF16toUTF8(aConfig->mSsid).get());
    return result;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::Reconnect() { return sSupplicantStaManager->Reconnect(); }

Result_t WifiNative::Reassociate() {
  return sSupplicantStaManager->Reassociate();
}

Result_t WifiNative::Disconnect() {
  return sSupplicantStaManager->Disconnect();
}

Result_t WifiNative::EnableNetwork() {
  return sSupplicantStaManager->EnableNetwork();
}

Result_t WifiNative::DisableNetwork() {
  return sSupplicantStaManager->DisableNetwork();
}

Result_t WifiNative::GetNetwork(nsWifiResult* aResult) {
  return sSupplicantStaManager->GetNetwork(aResult);
}

/**
 * To remove all configured networks in supplicant
 */
Result_t WifiNative::RemoveNetworks() {
  return sSupplicantStaManager->RemoveNetworks();
}

Result_t WifiNative::StartRoaming(ConfigurationOptions* aConfig) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = sSupplicantStaManager->RoamToNetwork(aConfig);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Roam to %s failed",
              NS_ConvertUTF16toUTF8(aConfig->mSsid).get());
  }
  return result;
}

Result_t WifiNative::SendEapSimIdentityResponse(
    SimIdentityRespDataOptions* aIdentity) {
  return sSupplicantStaManager->SendEapSimIdentityResponse(aIdentity);
}

Result_t WifiNative::SendEapSimGsmAuthResponse(
    const nsTArray<SimGsmAuthRespDataOptions>& aGsmAuthResp) {
  return sSupplicantStaManager->SendEapSimGsmAuthResponse(aGsmAuthResp);
}

Result_t WifiNative::SendEapSimGsmAuthFailure() {
  return sSupplicantStaManager->SendEapSimGsmAuthFailure();
}

Result_t WifiNative::SendEapSimUmtsAuthResponse(
    SimUmtsAuthRespDataOptions* aUmtsAuthResp) {
  return sSupplicantStaManager->SendEapSimUmtsAuthResponse(aUmtsAuthResp);
}

Result_t WifiNative::SendEapSimUmtsAutsResponse(
    SimUmtsAutsRespDataOptions* aUmtsAutsResp) {
  return sSupplicantStaManager->SendEapSimUmtsAutsResponse(aUmtsAutsResp);
}

Result_t WifiNative::SendEapSimUmtsAuthFailure() {
  return sSupplicantStaManager->SendEapSimUmtsAuthFailure();
}

Result_t WifiNative::RequestAnqp(AnqpRequestSettingsOptions* aRequest) {
  if (aRequest == nullptr || aRequest->mBssid.IsEmpty()) {
    WIFI_LOGE(LOG_TAG, "Invalid ANQP request settings");
    return nsIWifiResult::ERROR_INVALID_ARGS;
  }

  if (!sPasspointHandler) {
    WIFI_LOGE(LOG_TAG, "Passpoint handler is not ready yet");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  return sPasspointHandler->RequestAnqp(aRequest->mAnqpKey, aRequest->mBssid,
                                        aRequest->mRoamingConsortiumOIs,
                                        aRequest->mSupportRelease2);
}

Result_t WifiNative::InitWpsDetail() {
  return sSupplicantStaManager->InitWpsDetail();
}

Result_t WifiNative::StartWpsRegistrar(WpsConfigurationOptions* aConfig) {
  return sSupplicantStaManager->StartWpsRegistrar(
      NS_ConvertUTF16toUTF8(aConfig->mBssid).get(),
      NS_ConvertUTF16toUTF8(aConfig->mPinCode).get());
}

Result_t WifiNative::StartWpsPbc(WpsConfigurationOptions* aConfig) {
  return sSupplicantStaManager->StartWpsPbc(
      NS_ConvertUTF16toUTF8(aConfig->mBssid).get());
}

Result_t WifiNative::StartWpsPinKeypad(WpsConfigurationOptions* aConfig) {
  return sSupplicantStaManager->StartWpsPinKeypad(
      NS_ConvertUTF16toUTF8(aConfig->mPinCode).get());
}

Result_t WifiNative::StartWpsPinDisplay(WpsConfigurationOptions* aConfig,
                                        nsAString& aGeneratedPin) {
  return sSupplicantStaManager->StartWpsPinDisplay(
      NS_ConvertUTF16toUTF8(aConfig->mBssid).get(), aGeneratedPin);
}

Result_t WifiNative::CancelWps() { return sSupplicantStaManager->CancelWps(); }

/**
 * To enable wifi hotspot
 *
 * @param aSoftapConfig - the softap configuration to be set
 * @param aIfaceName - out the interface name for AP mode
 *
 * 1. load driver module and configure chip as AP mode
 * 2. start hostapd hidl service an register callback
 * 3. with lazy hal designed, hostapd daemon should be
 *    started while getService() of IHostapd
 * 4. setup ap in wificond, which will listen to event from driver
 */
Result_t WifiNative::StartSoftAp(SoftapConfigurationOptions* aSoftapConfig,
                                 nsAString& aIfaceName) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // Load wifi driver module and configure as ap mode.
  result = sWifiHal->StartWifiModule();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  result = StartAndConnectHostapd();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  result = sWifiHal->ConfigChipAndCreateIface(wifiNameSpaceV1_0::IfaceType::AP,
                                              mApInterfaceName);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to create AP interface");
    return result;
  }

  result = sWificondControl->SetupApIface(mApInterfaceName, sCallback);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to setup softap iface in wificond");
    sWificondControl->TearDownSoftapInterface(mApInterfaceName);
    return result;
  }

  // Up to now, ap interface should be ready to setup country code.
  std::string countryCode =
      NS_ConvertUTF16toUTF8(aSoftapConfig->mCountryCode).get();
  result = sWifiHal->SetSoftapCountryCode(countryCode);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to set country code");
  }

  // start softap from hostapd.
  result =
      sSoftapManager->StartSoftap(mApInterfaceName, countryCode, aSoftapConfig);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to start softap");
    return result;
  }

  nsString iface(NS_ConvertUTF8toUTF16(mApInterfaceName.c_str()));
  aIfaceName.Assign(iface);
  return CHECK_SUCCESS(aIfaceName.Length() > 0);
}

/**
 * To disable wifi hotspot
 *
 * 1. clean hostapd hidl client and stop daemon
 * 2. clean ap interfaces in wificond
 * 3. clean wifi hidl client and unload wlan module
 */
Result_t WifiNative::StopSoftAp() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = sSoftapManager->StopSoftap(mApInterfaceName);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to stop softap");
    return result;
  }

  // TODO: It's the only method to unsubscribe regulatory domain change event,
  //       implement remain interface check if we support multiple interfaces
  //       case such as p2p or concurrent mode.
  result = sWificondControl->TearDownInterfaces();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown interfaces in wificond");
    return result;
  }

  result = StopHostapd();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to stop hostapd");
    return result;
  }

  result = sWifiHal->TearDownInterface(wifiNameSpaceV1_0::IfaceType::AP);
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown softap interface");
    return result;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::StartAndConnectHostapd() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = sSoftapManager->InitInterface();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to initialize hostapd interface");
    return result;
  }

  bool connected = false;
  int32_t connectTries = 0;
  while (!connected && connectTries++ < CONNECTION_RETRY_TIMES) {
    // Check if the initialization is complete.
    if (sSoftapManager->IsInterfaceReady()) {
      connected = true;
      break;
    }
    usleep(CONNECTION_RETRY_INTERVAL_US);
  }
  return CHECK_SUCCESS(connected);
}

Result_t WifiNative::StopHostapd() {
  Result_t result = sSoftapManager->DeinitInterface();
  if (result != nsIWifiResult::SUCCESS) {
    WIFI_LOGE(LOG_TAG, "Failed to tear down hostapd interface");
    return result;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WifiNative::GetSoftapStations(uint32_t& aNumStations) {
  return sWificondControl->GetSoftapStations(aNumStations);
}

void WifiNative::SupplicantDeathHandler::OnDeath() {
  // supplicant died, start to clean up.
  WIFI_LOGE(LOG_TAG, "Supplicant DIED: ##############################");
}
