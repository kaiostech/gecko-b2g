/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiHalManager_H
#define WifiHalManager_H

// NAN define conflict
#ifdef NAN
#  undef NAN
#endif

#include "WifiCommon.h"

#include <android/hidl/manager/1.0/IServiceManager.h>
#include <android/hidl/manager/1.0/IServiceNotification.h>
#include <android/hardware/wifi/1.0/IWifi.h>
#include <android/hardware/wifi/1.0/IWifiChip.h>
#include <android/hardware/wifi/1.0/IWifiStaIface.h>
#include <android/hardware/wifi/1.3/IWifiStaIface.h>
#include <android/hardware/wifi/1.0/IWifiEventCallback.h>
#include <android/hardware/wifi/1.2/IWifiChipEventCallback.h>
#include <android/hardware/wifi/1.4/types.h>

#include "mozilla/Mutex.h"

#define START_HAL_RETRY_TIMES 3

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::wifi::V1_0::ChipId;
using ::android::hardware::wifi::V1_0::IWifi;
using ::android::hardware::wifi::V1_0::IWifiApIface;
using ::android::hardware::wifi::V1_0::IWifiChip;
using ::android::hardware::wifi::V1_0::IWifiIface;
using ::android::hardware::wifi::V1_0::IWifiP2pIface;
using ::android::hardware::wifi::V1_0::IWifiStaIface;
using ::android::hardware::wifi::V1_0::StaRoamingCapabilities;
using ::android::hardware::wifi::V1_0::StaRoamingConfig;
using ::android::hardware::wifi::V1_0::StaRoamingState;
using ::android::hardware::wifi::V1_0::StaScanData;
using ::android::hardware::wifi::V1_0::StaScanResult;
using ::android::hardware::wifi::V1_0::WifiDebugRingBufferStatus;
using ::android::hardware::wifi::V1_0::WifiStatus;
using ::android::hardware::wifi::V1_0::WifiStatusCode;
using ::android::hardware::wifi::V1_2::IWifiChipEventCallback;
using ::android::hidl::base::V1_0::IBase;

namespace wifiNameSpaceV1_0 = ::android::hardware::wifi::V1_0;
namespace wifiNameSpaceV1_3 = ::android::hardware::wifi::V1_3;
namespace wifiNameSpaceV1_4 = ::android::hardware::wifi::V1_4;

BEGIN_WIFI_NAMESPACE

class WifiHal
    : virtual public android::hidl::manager::V1_0::IServiceNotification,
      virtual public android::hardware::wifi::V1_0::IWifiEventCallback,
      virtual public android::hardware::wifi::V1_0::IWifiStaIfaceEventCallback,
      virtual public android::hardware::wifi::V1_2::IWifiChipEventCallback {
 public:
  static WifiHal* Get();
  static void CleanUp();

  Return<bool> CheckWifiStarted();
  Result_t InitHalInterface();
  Result_t TearDownInterface(const wifiNameSpaceV1_0::IfaceType& aType);
  Result_t GetSupportedFeatures(uint32_t& aSupportedFeatures);
  Result_t GetDriverModuleInfo(nsAString& aDriverVersion,
                               nsAString& aFirmwareVersion);
  Result_t SetLowLatencyMode(bool aEnable);

  Result_t StartWifiModule();
  Result_t StopWifiModule();
  Result_t ConfigChipAndCreateIface(const wifiNameSpaceV1_0::IfaceType& aType,
                                    std::string& aIfaceName);
  Result_t EnableLinkLayerStats();
  Result_t GetLinkLayerStats(wifiNameSpaceV1_3::StaLinkLayerStats& aStats);
  Result_t SetSoftapCountryCode(std::string aCountryCode);
  Result_t SetFirmwareRoaming(bool aEnable);
  Result_t ConfigureFirmwareRoaming(
      RoamingConfigurationOptions* mRoamingConfig);

  std::string GetInterfaceName(const wifiNameSpaceV1_0::IfaceType& aType);

  virtual ~WifiHal() {}

  // IServiceNotification::onRegistration
  virtual Return<void> onRegistration(const hidl_string& fqName,
                                      const hidl_string& name,
                                      bool preexisting) override;

 private:
  //...................... IWifiEventCallback ......................../
  /**
   * Called in response to a call to start indicating that the operation
   * completed. After this callback the HAL must be fully operational.
   */
  Return<void> onStart() override;
  /**
   * Called in response to a call to stop indicating that the operation
   * completed. When this event is received all IWifiChip objects retrieved
   * after the last call to start will be considered invalid.
   */
  Return<void> onStop() override;
  /**
   * Called when the Wi-Fi system failed in a way that caused it be disabled.
   * Calling start again must restart Wi-Fi as if stop then start was called
   * (full state reset). When this event is received all IWifiChip & IWifiIface
   * objects retrieved after the last call to start will be considered invalid.
   *
   * @param status Failure reason code.
   */
  Return<void> onFailure(const WifiStatus& status) override;

  //..................... IWifiChipEventCallback ......................./
  /**
   * Callback indicating that the chip has been reconfigured successfully. At
   * this point the interfaces available in the mode must be able to be
   * configured. When this is called any previous iface objects must be
   * considered invalid.
   *
   * @param modeId The mode that the chip switched to, corresponding to the id
   *        property of the target ChipMode.
   */
  Return<void> onChipReconfigured(uint32_t modeId) override;
  /**
   * Callback indicating that a chip reconfiguration failed. This is a fatal
   * error and any iface objects available previously must be considered
   * invalid. The client can attempt to recover by trying to reconfigure the
   * chip again using |IWifiChip.configureChip|.
   *
   * @param status Failure reason code.
   */
  Return<void> onChipReconfigureFailure(const WifiStatus& status) override;
  /**
   * Callback indicating that a new iface has been added to the chip.
   *
   * @param type Type of iface added.
   * @param name Name of iface added.
   */
  Return<void> onIfaceAdded(wifiNameSpaceV1_0::IfaceType type,
                            const hidl_string& name) override;
  /**
   * Callback indicating that an existing iface has been removed from the chip.
   *
   * @param type Type of iface removed.
   * @param name Name of iface removed.
   */
  Return<void> onIfaceRemoved(wifiNameSpaceV1_0::IfaceType type,
                              const hidl_string& name) override;
  /**
   * Callbacks for reporting debug ring buffer data.
   *
   * The ring buffer data collection is event based:
   * - Driver calls this callback when new records are available, the
   *   |WifiDebugRingBufferStatus| passed up to framework in the callback
   *   indicates to framework if more data is available in the ring buffer.
   *   It is not expected that driver will necessarily always empty the ring
   *   immediately as data is available, instead driver will report data
   *   every X seconds or if N bytes are available based on the parameters
   *   set via |startLoggingToDebugRingBuffer|.
   * - In the case where a bug report has to be captured, framework will
   *   require driver to upload all data immediately. This is indicated to
   *   driver when framework calls |forceDumpToDebugRingBuffer|.  The driver
   *   will start sending all available data in the indicated ring by repeatedly
   *   invoking this callback.
   *
   * @return status Status of the corresponding ring buffer. This should
   *         contain the name of the ring buffer on which the data is
   *         available.
   * @return data Raw bytes of data sent by the driver. Must be dumped
   *         out to a bugreport and post processed.
   */
  Return<void> onDebugRingBufferDataAvailable(
      const WifiDebugRingBufferStatus& status,
      const hidl_vec<uint8_t>& data) override;
  /**
   * Callback indicating that the chip has encountered a fatal error.
   * Client must not attempt to parse either the errorCode or debugData.
   * Must only be captured in a bugreport.
   *
   * @param errorCode Vendor defined error code.
   * @param debugData Vendor defined data used for debugging.
   */
  Return<void> onDebugErrorAlert(int32_t errorCode,
                                 const hidl_vec<uint8_t>& debugData) override;
  /**
   * Asynchronous callback indicating a radio mode change.
   * Radio mode change could be a result of:
   * a) Bringing up concurrent interfaces (For ex: STA + AP).
   * b) Change in operating band of one of the concurrent interfaces (For ex:
   * STA connection moved from 2.4G to 5G)
   *
   * @param radioModeInfos List of RadioModeInfo structures for each
   * radio chain (hardware MAC) on the device.
   */
  Return<void> onRadioModeChange(
      const hidl_vec<IWifiChipEventCallback::RadioModeInfo>& radioModeInfos)
      override;

  //.................... IWifiStaIfaceEventCallback ....................../
  /**
   * Callback indicating that an ongoing background scan request has failed.
   * The background scan needs to be restarted to continue scanning.
   *
   * @param cmdId command ID corresponding to the request.
   */
  Return<void> onBackgroundScanFailure(uint32_t cmdId) override;
  /**
   * Called for each received beacon/probe response for a scan with the
   * |REPORT_EVENTS_FULL_RESULTS| flag set in
   * |StaBackgroundScanBucketParameters.eventReportScheme|.
   *
   * @param cmdId command ID corresponding to the request.
   * @param bucketsScanned Bitset where each bit indicates if the bucket with
   *        that index (starting at 0) was scanned.
   * @parm result Full scan result for an AP.
   */
  Return<void> onBackgroundFullScanResult(uint32_t cmdId,
                                          uint32_t bucketsScanned,
                                          const StaScanResult& result) override;
  /**
   * Called when the |StaBackgroundScanBucketParameters.eventReportScheme| flags
   * for at least one bucket that was just scanned was
   * |REPORT_EVENTS_EACH_SCAN| or one of the configured thresholds was
   * breached.
   *
   * @param cmdId command ID corresponding to the request.
   * @parm scanDatas List of scan result for all AP's seen since last callback.
   */
  Return<void> onBackgroundScanResults(
      uint32_t cmdId,
      const ::android::hardware::hidl_vec<StaScanData>& scanDatas) override;
  /**
   * Called when the RSSI of the currently connected access point goes beyond
   * the thresholds set via |IWifiStaIface.startRssiMonitoring|.
   *
   * @param cmdId command ID corresponding to the request.
   * @param currBssid BSSID of the currently connected access point.
   * @param currRssi RSSI of the currently connected access point.
   */
  Return<void> onRssiThresholdBreached(
      uint32_t cmdId,
      const ::android::hardware::hidl_array<uint8_t, 6>& currBssid,
      int32_t currRssi) override;

  struct ServiceManagerDeathRecipient : public hidl_death_recipient {
    explicit ServiceManagerDeathRecipient(WifiHal* aOuter) : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    WifiHal* mOuter;
  };

  struct WifiServiceDeathRecipient : public hidl_death_recipient {
    explicit WifiServiceDeathRecipient(WifiHal* aOuter) : mOuter(aOuter) {}
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;

   private:
    WifiHal* mOuter;
  };

  WifiHal();
  Result_t InitServiceManager();
  Result_t InitWifiInterface();
  Result_t GetVendorCapabilities();
  Result_t ConfigChipByType(const android::sp<IWifiChip>& aChip,
                            const wifiNameSpaceV1_0::IfaceType& aType);
  Result_t RemoveInterfaceInternal(const wifiNameSpaceV1_0::IfaceType& aType);
  std::string QueryInterfaceName(const android::sp<IWifiIface>& aIface);
  android::sp<wifiNameSpaceV1_3::IWifiStaIface> GetWifiStaIfaceV1_3();

  static mozilla::Mutex sLock;

  android::sp<::android::hidl::manager::V1_0::IServiceManager> mServiceManager;
  android::sp<ServiceManagerDeathRecipient> mServiceManagerDeathRecipient;
  android::sp<IWifi> mWifi;
  android::sp<IWifiChip> mWifiChip;
  android::sp<WifiServiceDeathRecipient> mDeathRecipient;
  android::sp<IWifiStaIface> mStaIface;
  android::sp<IWifiP2pIface> mP2pIface;
  android::sp<IWifiApIface> mApIface;

  std::unordered_map<wifiNameSpaceV1_0::IfaceType, std::string> mIfaceNameMap;
  uint32_t mCapabilities;

  DISALLOW_COPY_AND_ASSIGN(WifiHal);
};

END_WIFI_NAMESPACE

#endif  // WifiHalManager_H
