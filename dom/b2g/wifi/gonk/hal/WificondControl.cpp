/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "WificondControl"

#include "WificondControl.h"
#include "nsIWifiElement.h"
#include "nsIMutableArray.h"

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/IMemory.h>
#include <binder/IPCThreadState.h>
#include <utils/String16.h>
#include <cutils/properties.h>

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::interface_cast;
using ::android::String16;
using ::android::net::wifi::nl80211::IApInterfaceEventCallback;
using ::android::net::wifi::nl80211::IPnoScanEvent;
using ::android::net::wifi::nl80211::IScanEvent;

using namespace mozilla::dom::wifi;

static const char* CTL_START_PROPERTY = "ctl.start";
static const char* CTL_STOP_PROPERTY = "ctl.stop";
static const char* SUPPLICANT_SERVICE_NAME = "wpa_supplicant";
static const char* WIFICOND_SERVICE_NAME = "wifinl80211";
static const int32_t WIFICOND_POLL_DELAY = 500000;
static const int32_t WIFICOND_RETRY_COUNT = 20;

static StaticAutoPtr<WificondControl> sWificondControl;

WificondControl::WificondControl()
    : mWificond(nullptr),
      mClientInterface(nullptr),
      mApInterface(nullptr),
      mScanner(nullptr),
      mScanEventService(nullptr),
      mPnoScanEventService(nullptr),
      mSoftapEventService(nullptr) {
  // To make sure remote process is not in any unexpected state,
  // cleanup remote interfaces here.
  TearDownInterfaces();
}

WificondControl* WificondControl::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sWificondControl) {
    return sWificondControl;
  }

  // Create new instance
  sWificondControl = new WificondControl();
  ClearOnShutdown(&sWificondControl);
  return sWificondControl;
}

/**
 * android::IBinder::DeathRecipient
 */
void WificondControl::WificondDeathRecipient::binderDied(
    const android::wp<IBinder>& who __unused) {
  WIFI_LOGE(LOG_TAG, "wificond died...");
}

Result_t WificondControl::InitWificondInterface() {
  if (mWificond != nullptr) {
    return nsIWifiResult::SUCCESS;
  }

  android::sp<::android::IServiceManager> sm = defaultServiceManager();
  android::sp<IBinder> binder;
  int32_t retry = 0;
  do {
    binder = sm->getService(String16(WIFICOND_SERVICE_NAME));
    if (binder != nullptr) {
      break;
    }
    usleep(WIFICOND_POLL_DELAY);
  } while (retry++ < WIFICOND_RETRY_COUNT);

  if (binder != nullptr) {
    mWificond = interface_cast<IWificond>(binder);

    mWificondDeathRecipient = new WificondDeathRecipient();
    binder->linkToDeath(mWificondDeathRecipient);
  } else {
    WIFI_LOGE(LOG_TAG, "Failed to create wificond instance");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::StartWificond() {
  // start wificond
  char value[PROPERTY_VALUE_MAX];
  property_get("init.svc.wificond", value, "");
  if (!strncmp(value, "running", 7)) {
    WIFI_LOGD(LOG_TAG, "wificond is already running...");
    return nsIWifiResult::SUCCESS;
  }

  if (property_set(CTL_START_PROPERTY, WIFICOND_SERVICE_NAME) != 0) {
    WIFI_LOGE(LOG_TAG, "start wificond failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::StopWificond() { return nsIWifiResult::SUCCESS; }

Result_t WificondControl::TearDownClientInterface(
    const std::string& aIfaceName) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = InitWificondInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  CleanupScanEvent();

  bool success = false;
  if (!mWificond->tearDownClientInterface(aIfaceName, &success).isOk()) {
    WIFI_LOGE(LOG_TAG,
              "Failed to teardown client interface due to remote exception");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (!success) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown client interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mWificond = nullptr;
  mClientInterface = nullptr;
  mScanner = nullptr;
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::TearDownSoftapInterface(
    const std::string& aIfaceName) {
  if (mWificond == nullptr || mApInterface == nullptr) {
    WIFI_LOGE(LOG_TAG, "No valid interface handler");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  if (mSoftapEventService) {
    mSoftapEventService->Cleanup();
    mSoftapEventService = nullptr;
  }

  bool success = false;
  if (!mWificond->tearDownApInterface(aIfaceName, &success).isOk()) {
    WIFI_LOGE(LOG_TAG,
              "Failed to teardown ap interface due to remote exception");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (!success) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown ap interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mApInterface = nullptr;
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::TearDownInterfaces() {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = InitWificondInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  CleanupScanEvent();

  if (!mWificond->tearDownInterfaces().isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to teardown interfaces due to remote exception");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mWificond = nullptr;
  mClientInterface = nullptr;
  mApInterface = nullptr;
  mScanner = nullptr;
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::CleanupScanEvent() {
  if (mScanner != nullptr) {
    mScanner->unsubscribeScanEvents();
    mScanner->unsubscribePnoScanEvents();
  }

  if (mScanEventService) {
    mScanEventService->Cleanup();
    mScanEventService = nullptr;
  }

  if (mPnoScanEventService) {
    mPnoScanEventService->Cleanup();
    mPnoScanEventService = nullptr;
  }
  return nsIWifiResult::SUCCESS;
}

/**
 * StartSupplicant() - To start supplicant daemon.
 *
 * android 8: IClientInterface -> libwifi-system.so -> ctl.start
 * android 9: IWificon -> libwifi-system.so -> ctl.start
 * android 10: ctl.start directly
 */
Result_t WificondControl::StartSupplicant() {
  bool supplicantStarted = false;
  supplicantStarted =
      (property_set(CTL_START_PROPERTY, SUPPLICANT_SERVICE_NAME) == 0);
  return CHECK_SUCCESS(supplicantStarted);
}

Result_t WificondControl::StopSupplicant() {
  bool supplicantStopped = false;
  supplicantStopped =
      (property_set(CTL_STOP_PROPERTY, SUPPLICANT_SERVICE_NAME) == 0);
  return CHECK_SUCCESS(supplicantStopped);
}

Result_t WificondControl::InitiateScanEvent(
    const std::string& aIfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  if (mScanner == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  // here create scan and pno scan event service,
  // which implement scan callback from wificond
  mScanEventService = ScanEventService::CreateService(aIfaceName, aCallback);
  if (mScanEventService == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to create scan event service");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mPnoScanEventService =
      PnoScanEventService::CreateService(aIfaceName, aCallback);
  if (mPnoScanEventService == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to create pno scan event service");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  if (!mScanner
           ->subscribeScanEvents(
               android::interface_cast<android::net::wifi::nl80211::IScanEvent>(
                   mScanEventService))
           .isOk()) {
    WIFI_LOGE(LOG_TAG, "subscribe scan event failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  if (!mScanner
           ->subscribePnoScanEvents(
               android::interface_cast<android::net::wifi::nl80211::IPnoScanEvent>(
                   mPnoScanEventService))
           .isOk()) {
    WIFI_LOGE(LOG_TAG, "subscribe pno scan event failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::SetupClientIface(
    const std::string& aIfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  // retrieve wificond handle
  result = InitWificondInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  if (!mWificond->createClientInterface(aIfaceName, &mClientInterface).isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to create client interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  if (!mClientInterface->getWifiScannerImpl(&mScanner).isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to get WificondScannerImpl");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return InitiateScanEvent(aIfaceName, aCallback);
}

Result_t WificondControl::SetupApIface(
    const std::string& aIfaceName,
    const android::sp<WifiEventCallback>& aCallback) {
  Result_t result = nsIWifiResult::ERROR_UNKNOWN;

  result = InitWificondInterface();
  if (result != nsIWifiResult::SUCCESS) {
    return result;
  }

  if (!mWificond->createApInterface(aIfaceName, &mApInterface).isOk()) {
    WIFI_LOGE(LOG_TAG, "Failed to create ap interface");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  mSoftapEventService =
      SoftapEventService::CreateService(aIfaceName, aCallback);
  if (mSoftapEventService == nullptr) {
    WIFI_LOGE(LOG_TAG, "Failed to create softap event service");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }

  bool success = false;
  mApInterface->registerCallback(
      android::interface_cast<android::net::wifi::nl80211::IApInterfaceEventCallback>(
          mSoftapEventService),
      &success);
  return CHECK_SUCCESS(success);
}

Result_t WificondControl::StartSingleScan(ScanSettingsOptions* aScanSettings) {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }
  Wificond::SingleScanSettings settings;
  settings.scan_type_ = aScanSettings->mScanType;

  std::vector<Wificond::ChannelSettings> channels;
  for (auto& freq : aScanSettings->mChannels) {
    Wificond::ChannelSettings channel;
    channel.frequency_ = freq;
    channels.push_back(channel);
  }

  std::vector<Wificond::HiddenNetwork> hiddenNetworks;
  if (!aScanSettings->mHiddenNetworks.IsEmpty()) {
    for (auto& net : aScanSettings->mHiddenNetworks) {
      Wificond::HiddenNetwork hidden;

      std::string ssidStr = NS_ConvertUTF16toUTF8(net).get();
      Dequote(ssidStr);
      std::vector<uint8_t> ssid(ssidStr.begin(), ssidStr.end());
      hidden.ssid_ = ssid;
      hiddenNetworks.push_back(hidden);
    }
  }
  settings.channel_settings_ = channels;
  settings.hidden_networks_ = hiddenNetworks;

  bool success = false;
  mScanner->scan(settings, &success);
  return CHECK_SUCCESS(success);
}

Result_t WificondControl::StopSingleScan() {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }
  return CHECK_SUCCESS(mScanner->abortScan().isOk());
}

Result_t WificondControl::StartPnoScan(
    PnoScanSettingsOptions* aPnoScanSettings) {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  Wificond::PnoSettings settings;
  settings.interval_ms_ = aPnoScanSettings->mInterval;
  settings.min_2g_rssi_ = aPnoScanSettings->mMin2gRssi;
  settings.min_5g_rssi_ = aPnoScanSettings->mMin5gRssi;

  std::vector<Wificond::PnoNetwork> pnoNetworks;
  if (!aPnoScanSettings->mPnoNetworks.IsEmpty()) {
    for (auto& pno : aPnoScanSettings->mPnoNetworks) {
      Wificond::PnoNetwork network;
      network.is_hidden_ = pno.mIsHidden;

      std::string ssidStr = NS_ConvertUTF16toUTF8(pno.mSsid).get();
      Dequote(ssidStr);
      std::vector<uint8_t> ssid(ssidStr.begin(), ssidStr.end());
      network.ssid_ = ssid;

      for (int32_t freq : pno.mFrequencies) {
        network.frequencies_.push_back(freq);
      }
      pnoNetworks.push_back(network);
    }
  }
  settings.pno_networks_ = pnoNetworks;

  bool success = false;
  mScanner->startPnoScan(settings, &success);
  return CHECK_SUCCESS(success);
}

Result_t WificondControl::StopPnoScan() {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  bool success = false;
  mScanner->stopPnoScan(&success);
  return CHECK_SUCCESS(success);
}

Result_t WificondControl::GetSingleScanResults(
    std::vector<Wificond::NativeScanResult>& aScanResults) {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  if (!mScanner->getScanResults(&aScanResults).isOk()) {
    WIFI_LOGE(LOG_TAG, "Get scan results failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::GetPnoScanResults(
    std::vector<Wificond::NativeScanResult>& aPnoScanResults) {
  if (mScanner == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi scanner interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  if (!mScanner->getPnoScanResults(&aPnoScanResults).isOk()) {
    WIFI_LOGE(LOG_TAG, "Get pno scan results failed");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::GetChannelsForBand(uint32_t aBandMask,
                                             std::vector<int32_t>& aChannels) {
  if (mWificond == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  std::unique_ptr<std::vector<int32_t>> channels;
  if (aBandMask & nsIScanSettings::BAND_2_4_GHZ) {
    mWificond->getAvailable2gChannels(&channels);
    if (channels != nullptr) {
      aChannels.insert(aChannels.end(), (*channels).begin(), (*channels).end());
    }
  }

  if (aBandMask & nsIScanSettings::BAND_5_GHZ) {
    mWificond->getAvailable5gNonDFSChannels(&channels);
    if (channels != nullptr) {
      aChannels.insert(aChannels.end(), (*channels).begin(), (*channels).end());
    }
  }

  if (aBandMask & nsIScanSettings::BAND_5_GHZ_DFS) {
    mWificond->getAvailableDFSChannels(&channels);
    if (channels != nullptr) {
      aChannels.insert(aChannels.end(), (*channels).begin(), (*channels).end());
    }
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::SignalPoll(std::vector<int32_t>& aPollResult) {
  if (mClientInterface == nullptr) {
    WIFI_LOGE(LOG_TAG, "Invalid wifi client interface");
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  if (!mClientInterface->signalPoll(&aPollResult).isOk() ||
      aPollResult.size() == 0) {
    WIFI_LOGE(LOG_TAG, "Failed to get signal poll results");
    return nsIWifiResult::ERROR_COMMAND_FAILED;
  }
  return nsIWifiResult::SUCCESS;
}

Result_t WificondControl::GetSoftapStations(uint32_t& aNumStations) {
  if (mApInterface == nullptr) {
    return nsIWifiResult::ERROR_INVALID_INTERFACE;
  }

  //TODO FIXME
  //int32_t stations;
  //mApInterface->getNumberOfAssociatedStations(&stations);
  //aNumStations = (stations < 0) ? 0 : stations;
  //return CHECK_SUCCESS(stations >= 0);
  return nsIWifiResult::SUCCESS;
}
