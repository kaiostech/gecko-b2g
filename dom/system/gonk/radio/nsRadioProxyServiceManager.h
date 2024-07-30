/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRADIOPRXOYSERVICEMANAGER_H
#define NSRADIOPRXOYSERVICEMANAGER_H
#include "nsIRadioProxyServiceManager.h"
#include "nsRilWorker.h"
#include <aidl/android/hardware/radio/RadioAccessFamily.h>
#include <aidl/android/hardware/radio/RadioError.h>
#include <aidl/android/hardware/radio/config/BnRadioConfigIndication.h>
#include <aidl/android/hardware/radio/config/BnRadioConfigResponse.h>
#include <aidl/android/hardware/radio/config/IRadioConfig.h>
#include <aidl/android/hardware/radio/data/BnRadioDataIndication.h>
#include <aidl/android/hardware/radio/data/BnRadioDataResponse.h>
#include <aidl/android/hardware/radio/data/IRadioData.h>
#include <aidl/android/hardware/radio/messaging/BnRadioMessagingIndication.h>
#include <aidl/android/hardware/radio/messaging/BnRadioMessagingResponse.h>
#include <aidl/android/hardware/radio/messaging/IRadioMessaging.h>
#include <aidl/android/hardware/radio/modem/BnRadioModemIndication.h>
#include <aidl/android/hardware/radio/modem/BnRadioModemResponse.h>
#include <aidl/android/hardware/radio/modem/IRadioModem.h>
#include <aidl/android/hardware/radio/network/BnRadioNetworkIndication.h>
#include <aidl/android/hardware/radio/network/BnRadioNetworkResponse.h>
#include <aidl/android/hardware/radio/network/IRadioNetwork.h>
#include <aidl/android/hardware/radio/sim/BnRadioSimIndication.h>
#include <aidl/android/hardware/radio/sim/BnRadioSimResponse.h>
#include <aidl/android/hardware/radio/sim/IRadioSim.h>
#include <aidl/android/hardware/radio/voice/BnRadioVoiceIndication.h>
#include <aidl/android/hardware/radio/voice/BnRadioVoiceResponse.h>
#include <aidl/android/hardware/radio/voice/IRadioVoice.h>
#include <cstdint>
#include <nsISupportsImpl.h>

using namespace aidl::android::hardware::radio;
using namespace aidl::android::hardware::radio::data;
using namespace aidl::android::hardware::radio::voice;
using namespace aidl::android::hardware::radio::sim;
using namespace aidl::android::hardware::radio::config;
using aidl_KeepaliveStatus =
  aidl::android::hardware::radio::data::KeepaliveStatus;
using namespace aidl::android::hardware::radio::messaging;
using namespace aidl::android::hardware::radio::modem;
using ::aidl::android::hardware::radio::network::BnRadioNetworkIndication;
using ::aidl::android::hardware::radio::network::BnRadioNetworkResponse;
using ::aidl::android::hardware::radio::network::IRadioNetwork;
using CellIdentity_aidl =
  ::aidl::android::hardware::radio::network::CellIdentity;
using BarringInfo_aidl = ::aidl::android::hardware::radio::network::BarringInfo;
using CellInfo_aidl = ::aidl::android::hardware::radio::network::CellInfo;
using LinkCapacityEstimate_aidl =
  ::aidl::android::hardware::radio::network::LinkCapacityEstimate;
using PhysicalChannelConfig_aidl =
  ::aidl::android::hardware::radio::network::PhysicalChannelConfig;
using SignalStrength_aidl =
  ::aidl::android::hardware::radio::network::SignalStrength;
using NetworkScanResult_aidl =
  ::aidl::android::hardware::radio::network::NetworkScanResult;
using SuppSvcNotification_aidl =
  ::aidl::android::hardware::radio::network::SuppSvcNotification;
using OperatorInfo_aidl =
  ::aidl::android::hardware::radio::network::OperatorInfo;
using RegStateResult_aidl =
  ::aidl::android::hardware::radio::network::RegStateResult;
using RadioAccessSpecifier_aidl =
  ::aidl::android::hardware::radio::network::RadioAccessSpecifier;
// reg
using RegState_aidl = ::aidl::android::hardware::radio::network::RegState;
using RegistrationFailCause_aidl =
  ::aidl::android::hardware::radio::network::RegistrationFailCause;
using NrIndicators_aidl =
  ::aidl::android::hardware::radio::network::NrIndicators;
using LteVopsInfo_aidl = ::aidl::android::hardware::radio::network::LteVopsInfo;
using NrVopsInfo_aidl = ::aidl::android::hardware::radio::network::NrVopsInfo;
// SignalStrength
using GsmSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::GsmSignalStrength;
using CdmaSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::CdmaSignalStrength;
using EvdoSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::EvdoSignalStrength;
using LteSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::LteSignalStrength;
using TdscdmaSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::TdscdmaSignalStrength;
using WcdmaSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::WcdmaSignalStrength;
using NrSignalStrength_aidl =
  ::aidl::android::hardware::radio::network::NrSignalStrength;
using SignalThresholdInfo_aidl =
  ::aidl::android::hardware::radio::network::SignalThresholdInfo;
// CellIdentity
using ClosedSubscriberGroupInfo_aidl =
  ::aidl::android::hardware::radio::network::ClosedSubscriberGroupInfo;
using CellIdentityGsm_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityGsm;
using CellIdentityCdma_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityCdma;
using CellIdentityLte_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityLte;
using CellIdentityTdscdma_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityTdscdma;
using CellIdentityWcdma_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityWcdma;
using CellIdentityNr_aidl =
  ::aidl::android::hardware::radio::network::CellIdentityNr;
// CellInfo
using CellConnectionStatus_aidl =
  ::aidl::android::hardware::radio::network::CellConnectionStatus;
using CellInfoGsm_aidl = ::aidl::android::hardware::radio::network::CellInfoGsm;
using CellInfoCdma_aidl =
  ::aidl::android::hardware::radio::network::CellInfoCdma;
using CellInfoLte_aidl = ::aidl::android::hardware::radio::network::CellInfoLte;
using CellInfoTdscdma_aidl =
  ::aidl::android::hardware::radio::network::CellInfoTdscdma;
using CellInfoWcdma_aidl =
  ::aidl::android::hardware::radio::network::CellInfoWcdma;
using CellInfoNr_aidl = ::aidl::android::hardware::radio::network::CellInfoNr;
using ::aidl::android::hardware::radio::AccessNetwork;
using ::aidl::android::hardware::radio::RadioAccessFamily;
using ::aidl::android::hardware::radio::RadioTechnology;
using ::aidl::android::hardware::radio::RadioTechnologyFamily;
using ::aidl::android::hardware::radio::network::AccessTechnologySpecificInfo;
using ::aidl::android::hardware::radio::network::BarringTypeSpecificInfo;
using ::aidl::android::hardware::radio::network::CdmaRoamingType;
using ::aidl::android::hardware::radio::network::PhoneRestrictedState;
using ::aidl::android::hardware::radio::network::RadioBandMode;
using ::aidl::android::hardware::radio::network::UsageSetting;

class nsRadioProxyServiceManager;

typedef enum
{
  DATA,
  VOICE,
  SIM,
  MODEM,
  NETWORK,
  MESSAGE,
  CONFIG
} SERVICE_TYPE;

/* Callback class for radio network indication */
class RadioNetworkIndication : public BnRadioNetworkIndication
{
protected:
  nsRadioProxyServiceManager& parent_network;

public:
  RadioNetworkIndication(nsRadioProxyServiceManager& parent_network);
  ~RadioNetworkIndication() = default;

  ndk::ScopedAStatus barringInfoChanged(
    RadioIndicationType type,
    const CellIdentity_aidl& cellIdentity,
    const std::vector<BarringInfo_aidl>& barringInfos) override;

  ndk::ScopedAStatus cdmaPrlChanged(RadioIndicationType type,
                                    int32_t version) override;

  ndk::ScopedAStatus cellInfoList(
    RadioIndicationType type,
    const std::vector<CellInfo_aidl>& records) override;

  ndk::ScopedAStatus currentLinkCapacityEstimate(
    RadioIndicationType type,
    const LinkCapacityEstimate_aidl& lce) override;

  ndk::ScopedAStatus currentPhysicalChannelConfigs(
    RadioIndicationType type,
    const std::vector<PhysicalChannelConfig_aidl>& configs) override;

  ndk::ScopedAStatus currentSignalStrength(
    RadioIndicationType type,
    const SignalStrength_aidl& signalStrength) override;

  ndk::ScopedAStatus imsNetworkStateChanged(RadioIndicationType type) override;

  ndk::ScopedAStatus networkScanResult(
    RadioIndicationType type,
    const NetworkScanResult_aidl& result) override;

  ndk::ScopedAStatus networkStateChanged(RadioIndicationType type) override;

  ndk::ScopedAStatus nitzTimeReceived(RadioIndicationType type,
                                      const std::string& nitzTime,
                                      int64_t receivedTime,
                                      int64_t ageMs) override;

  ndk::ScopedAStatus registrationFailed(RadioIndicationType type,
                                        const CellIdentity_aidl& cellIdentity,
                                        const std::string& chosenPlmn,
                                        int32_t domain,
                                        int32_t causeCode,
                                        int32_t additionalCauseCode) override;

  ndk::ScopedAStatus restrictedStateChanged(
    RadioIndicationType type,
    PhoneRestrictedState state) override;

  ndk::ScopedAStatus suppSvcNotify(
    RadioIndicationType type,
    const SuppSvcNotification_aidl& suppSvc) override;

  ndk::ScopedAStatus voiceRadioTechChanged(RadioIndicationType type,
                                           const RadioTechnology rat) override;
};

/* Callback class for radio network response */
class RadioNetworkResponse : public BnRadioNetworkResponse
{
protected:
  nsRadioProxyServiceManager& parent_network;

public:
  RadioNetworkResponse(nsRadioProxyServiceManager& parent_network);
  ~RadioNetworkResponse() = default;

  RadioResponseInfo rspInfo;

  ndk::ScopedAStatus acknowledgeRequest(int32_t serial) override;

  ndk::ScopedAStatus getAllowedNetworkTypesBitmapResponse(
    const RadioResponseInfo& info,
    int32_t networkTypeBitmap) override;

  ndk::ScopedAStatus getAvailableBandModesResponse(
    const RadioResponseInfo& info,
    const std::vector<RadioBandMode>& bandModes) override;

  ndk::ScopedAStatus getAvailableNetworksResponse(
    const RadioResponseInfo& info,
    const std::vector<OperatorInfo_aidl>& networkInfos) override;

  ndk::ScopedAStatus getBarringInfoResponse(
    const RadioResponseInfo& info,
    const CellIdentity_aidl& cellIdentity,
    const std::vector<BarringInfo_aidl>& barringInfos) override;

  ndk::ScopedAStatus getCdmaRoamingPreferenceResponse(
    const RadioResponseInfo& info,
    const CdmaRoamingType type) override;

  ndk::ScopedAStatus getCellInfoListResponse(
    const RadioResponseInfo& info,
    const std::vector<CellInfo_aidl>& cellInfo) override;

  ndk::ScopedAStatus getDataRegistrationStateResponse(
    const RadioResponseInfo& info,
    const RegStateResult_aidl& dataRegResponse) override;

  ndk::ScopedAStatus getImsRegistrationStateResponse(
    const RadioResponseInfo& info,
    bool isRegistered,
    const RadioTechnologyFamily ratFamily) override;

  ndk::ScopedAStatus getNetworkSelectionModeResponse(
    const RadioResponseInfo& info,
    bool manual) override;

  ndk::ScopedAStatus getOperatorResponse(const RadioResponseInfo& info,
                                         const std::string& longName,
                                         const std::string& shortName,
                                         const std::string& numeric) override;

  ndk::ScopedAStatus getSignalStrengthResponse(
    const RadioResponseInfo& info,
    const SignalStrength_aidl& signalStrength) override;

  ndk::ScopedAStatus getSystemSelectionChannelsResponse(
    const RadioResponseInfo& info,
    const std::vector<RadioAccessSpecifier_aidl>& specifiers) override;

  ndk::ScopedAStatus getVoiceRadioTechnologyResponse(
    const RadioResponseInfo& info,
    const RadioTechnology rat) override;

  ndk::ScopedAStatus getVoiceRegistrationStateResponse(
    const RadioResponseInfo& info,
    const RegStateResult_aidl& voiceRegResponse) override;

  ndk::ScopedAStatus isNrDualConnectivityEnabledResponse(
    const RadioResponseInfo& info,
    bool isEnabled) override;

  ndk::ScopedAStatus setAllowedNetworkTypesBitmapResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setBandModeResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setBarringPasswordResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setCdmaRoamingPreferenceResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setCellInfoListRateResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setIndicationFilterResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setLinkCapacityReportingCriteriaResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setLocationUpdatesResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setNetworkSelectionModeAutomaticResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setNetworkSelectionModeManualResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setNrDualConnectivityStateResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setSignalStrengthReportingCriteriaResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setSuppServiceNotificationsResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setSystemSelectionChannelsResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus startNetworkScanResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus stopNetworkScanResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus supplyNetworkDepersonalizationResponse(
    const RadioResponseInfo& info,
    int32_t remainingRetries) override;

  ndk::ScopedAStatus setUsageSettingResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus getUsageSettingResponse(
    const RadioResponseInfo& info,
    const UsageSetting usageSetting) override;
};

class RadioDataIndication : public BnRadioDataIndication
{
protected:
  nsRadioProxyServiceManager& parent_data;

public:
  RadioDataIndication(nsRadioProxyServiceManager& parent_data);
  ~RadioDataIndication() = default;

  ndk::ScopedAStatus dataCallListChanged(
    RadioIndicationType type,
    const std::vector<SetupDataCallResult>& dcList) override;

  ndk::ScopedAStatus keepaliveStatus(
    RadioIndicationType type,
    const aidl_KeepaliveStatus& status) override;

  ndk::ScopedAStatus pcoData(RadioIndicationType type,
                             const PcoDataInfo& pco) override;

  ndk::ScopedAStatus unthrottleApn(RadioIndicationType type,
                                   const DataProfileInfo& dataProfile) override;
  ndk::ScopedAStatus slicingConfigChanged(
    RadioIndicationType type,
    const SlicingConfig& slicingConfig) override;
};

/* Callback class for radio data response */
class RadioDataResponse : public BnRadioDataResponse
{
protected:
  nsRadioProxyServiceManager& parent_data;

public:
  RadioDataResponse(nsRadioProxyServiceManager& parent_data);
  ~RadioDataResponse() = default;
  void defaultResponse(const RadioResponseInfo&,
                       const nsString& rilmessageType);

  RadioResponseInfo rspInfo;
  int32_t allocatedPduSessionId;
  SetupDataCallResult setupDataCallResult;

  ndk::ScopedAStatus acknowledgeRequest(int32_t serial) override;

  ndk::ScopedAStatus allocatePduSessionIdResponse(const RadioResponseInfo& info,
                                                  int32_t id) override;

  ndk::ScopedAStatus cancelHandoverResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus deactivateDataCallResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus getDataCallListResponse(
    const RadioResponseInfo& info,
    const std::vector<SetupDataCallResult>& dcResponse) override;

  ndk::ScopedAStatus getSlicingConfigResponse(
    const RadioResponseInfo& info,
    const SlicingConfig& slicingConfig) override;

  ndk::ScopedAStatus releasePduSessionIdResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setDataAllowedResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setDataProfileResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setDataThrottlingResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setInitialAttachApnResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setupDataCallResponse(
    const RadioResponseInfo& info,
    const SetupDataCallResult& dcResponse) override;

  ndk::ScopedAStatus startHandoverResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus startKeepaliveResponse(
    const RadioResponseInfo& info,
    const aidl_KeepaliveStatus& status) override;

  ndk::ScopedAStatus stopKeepaliveResponse(
    const RadioResponseInfo& info) override;
};

class RadioMessagingIndication : public BnRadioMessagingIndication
{
protected:
  nsRadioProxyServiceManager& parent_Message;

public:
  RadioMessagingIndication(nsRadioProxyServiceManager& parent_msg);
  ~RadioMessagingIndication() = default;

  ndk::ScopedAStatus cdmaNewSms(RadioIndicationType in_type,
                                const CdmaSmsMessage& in_msg) override;
  ndk::ScopedAStatus cdmaRuimSmsStorageFull(
    RadioIndicationType in_type) override;
  ndk::ScopedAStatus newBroadcastSms(
    RadioIndicationType in_type,
    const std::vector<uint8_t>& in_data) override;
  ndk::ScopedAStatus newSms(RadioIndicationType in_type,
                            const std::vector<uint8_t>& in_pdu) override;
  ndk::ScopedAStatus newSmsOnSim(RadioIndicationType in_type,
                                 int32_t in_recordNumber) override;
  ndk::ScopedAStatus newSmsStatusReport(
    RadioIndicationType in_type,
    const std::vector<uint8_t>& in_pdu) override;
  ndk::ScopedAStatus simSmsStorageFull(RadioIndicationType in_type) override;
};

/* Callback class for radio data response */
class RadioMessagingResponse : public BnRadioMessagingResponse
{
protected:
  nsRadioProxyServiceManager& parent_Message;

public:
  RadioMessagingResponse(nsRadioProxyServiceManager& parent_msg);
  ~RadioMessagingResponse() = default;

  RadioResponseInfo rspInfo;

  ndk::ScopedAStatus acknowledgeIncomingGsmSmsWithPduResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus acknowledgeLastIncomingCdmaSmsResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus acknowledgeLastIncomingGsmSmsResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus acknowledgeRequest(int32_t in_serial) override;
  ndk::ScopedAStatus deleteSmsOnRuimResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus deleteSmsOnSimResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus getCdmaBroadcastConfigResponse(
    const RadioResponseInfo& in_info,
    const std::vector<CdmaBroadcastSmsConfigInfo>& in_configs) override;
  ndk::ScopedAStatus getGsmBroadcastConfigResponse(
    const RadioResponseInfo& in_info,
    const std::vector<GsmBroadcastSmsConfigInfo>& in_configs) override;
  ndk::ScopedAStatus getSmscAddressResponse(
    const RadioResponseInfo& in_info,
    const std::string& in_smsc) override;
  ndk::ScopedAStatus reportSmsMemoryStatusResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus sendCdmaSmsExpectMoreResponse(
    const RadioResponseInfo& in_info,
    const SendSmsResult& in_sms) override;
  ndk::ScopedAStatus sendCdmaSmsResponse(const RadioResponseInfo& in_info,
                                         const SendSmsResult& in_sms) override;
  ndk::ScopedAStatus sendImsSmsResponse(const RadioResponseInfo& in_info,
                                        const SendSmsResult& in_sms) override;
  ndk::ScopedAStatus sendSmsExpectMoreResponse(
    const RadioResponseInfo& in_info,
    const SendSmsResult& in_sms) override;
  ndk::ScopedAStatus sendSmsResponse(const RadioResponseInfo& in_info,
                                     const SendSmsResult& in_sms) override;
  ndk::ScopedAStatus setCdmaBroadcastActivationResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus setCdmaBroadcastConfigResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus setGsmBroadcastActivationResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus setGsmBroadcastConfigResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus setSmscAddressResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus writeSmsToRuimResponse(const RadioResponseInfo& in_info,
                                            int32_t in_index) override;
  ndk::ScopedAStatus writeSmsToSimResponse(const RadioResponseInfo& in_info,
                                           int32_t in_index) override;
};

class RadioModemIndication : public BnRadioModemIndication
{
protected:
  nsRadioProxyServiceManager& parent_Modem;

public:
  RadioModemIndication(nsRadioProxyServiceManager& parent_md);
  ~RadioModemIndication() = default;

  ndk::ScopedAStatus hardwareConfigChanged(
    RadioIndicationType in_type,
    const std::vector<HardwareConfig>& in_configs) override;
  ndk::ScopedAStatus modemReset(RadioIndicationType in_type,
                                const std::string& in_reason) override;
  ndk::ScopedAStatus radioCapabilityIndication(
    RadioIndicationType in_type,
    const RadioCapability& in_rc) override;
  ndk::ScopedAStatus radioStateChanged(RadioIndicationType in_type,
                                       RadioState in_radioState) override;
  ndk::ScopedAStatus rilConnected(RadioIndicationType in_type) override;
};

/* Callback class for radio data response */
class RadioModemResponse : public BnRadioModemResponse
{
protected:
  nsRadioProxyServiceManager& parent_Modem;

public:
  RadioModemResponse(nsRadioProxyServiceManager& parent_md);
  ~RadioModemResponse() = default;

  RadioResponseInfo rspInfo;

  ndk::ScopedAStatus acknowledgeRequest(int32_t in_serial) override;
  ndk::ScopedAStatus enableModemResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus getBasebandVersionResponse(
    const RadioResponseInfo& in_info,
    const std::string& in_version) override;
  ndk::ScopedAStatus getDeviceIdentityResponse(
    const RadioResponseInfo& in_info,
    const std::string& in_imei,
    const std::string& in_imeisv,
    const std::string& in_esn,
    const std::string& in_meid) override;
  ndk::ScopedAStatus getHardwareConfigResponse(
    const RadioResponseInfo& in_info,
    const std::vector<HardwareConfig>& in_config) override;
  ndk::ScopedAStatus getModemActivityInfoResponse(
    const RadioResponseInfo& in_info,
    const ActivityStatsInfo& in_activityInfo) override;
  ndk::ScopedAStatus getModemStackStatusResponse(
    const RadioResponseInfo& in_info,
    bool in_isEnabled) override;
  ndk::ScopedAStatus getRadioCapabilityResponse(
    const RadioResponseInfo& in_info,
    const RadioCapability& in_rc) override;
  ndk::ScopedAStatus nvReadItemResponse(const RadioResponseInfo& in_info,
                                        const std::string& in_result) override;
  ndk::ScopedAStatus nvResetConfigResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus nvWriteCdmaPrlResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus nvWriteItemResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus requestShutdownResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus sendDeviceStateResponse(
    const RadioResponseInfo& in_info) override;
  ndk::ScopedAStatus setRadioCapabilityResponse(
    const RadioResponseInfo& in_info,
    const RadioCapability& in_rc) override;
  ndk::ScopedAStatus setRadioPowerResponse(
    const RadioResponseInfo& in_info) override;
};

/* Callback class for radio voice response */
class RadioVoiceResponse : public BnRadioVoiceResponse
{
protected:
  nsRadioProxyServiceManager& parent_voice;

public:
  RadioVoiceResponse(nsRadioProxyServiceManager& parent_voice);
  ~RadioVoiceResponse() = default;

  RadioResponseInfo rspInfo;
  std::vector<Call> currentCalls;

  ndk::ScopedAStatus acceptCallResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus acknowledgeRequest(int32_t serial) override;

  ndk::ScopedAStatus cancelPendingUssdResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus conferenceResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus dialResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus emergencyDialResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus exitEmergencyCallbackModeResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus explicitCallTransferResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus getCallForwardStatusResponse(
    const RadioResponseInfo& info,
    const std::vector<CallForwardInfo>& call_forwardInfos) override;

  ndk::ScopedAStatus getCallWaitingResponse(const RadioResponseInfo& info,
                                            bool enable,
                                            int32_t serviceClass) override;

  ndk::ScopedAStatus getClipResponse(const RadioResponseInfo& info,
                                     ClipStatus status) override;

  ndk::ScopedAStatus getClirResponse(const RadioResponseInfo& info,
                                     int32_t n,
                                     int32_t m) override;

  ndk::ScopedAStatus getCurrentCallsResponse(
    const RadioResponseInfo& info,
    const std::vector<Call>& calls) override;

  ndk::ScopedAStatus getLastCallFailCauseResponse(
    const RadioResponseInfo& info,
    const LastCallFailCauseInfo& failCauseInfo) override;

  ndk::ScopedAStatus getMuteResponse(const RadioResponseInfo& info,
                                     bool enable) override;

  ndk::ScopedAStatus getPreferredVoicePrivacyResponse(
    const RadioResponseInfo& info,
    bool enable) override;

  ndk::ScopedAStatus getTtyModeResponse(const RadioResponseInfo& info,
                                        TtyMode mode) override;

  ndk::ScopedAStatus handleStkCallSetupRequestFromSimResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus hangupConnectionResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus hangupForegroundResumeBackgroundResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus hangupWaitingOrBackgroundResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus isVoNrEnabledResponse(const RadioResponseInfo& info,
                                           bool enable) override;

  ndk::ScopedAStatus rejectCallResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus sendBurstDtmfResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus sendCdmaFeatureCodeResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus sendDtmfResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus sendUssdResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus separateConnectionResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setCallForwardResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setCallWaitingResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setClirResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setMuteResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setPreferredVoicePrivacyResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setTtyModeResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus setVoNrEnabledResponse(
    const RadioResponseInfo& info) override;

  ndk::ScopedAStatus startDtmfResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus stopDtmfResponse(const RadioResponseInfo& info) override;

  ndk::ScopedAStatus switchWaitingOrHoldingAndActiveResponse(
    const RadioResponseInfo& info) override;
};

/* Callback class for radio voice indication */
class RadioVoiceIndication : public BnRadioVoiceIndication
{
protected:
  nsRadioProxyServiceManager& parent_voice;

public:
  RadioVoiceIndication(nsRadioProxyServiceManager& parent_voice);
  ~RadioVoiceIndication() = default;

  ndk::ScopedAStatus callRing(RadioIndicationType type,
                              bool isGsm,
                              const CdmaSignalInfoRecord& record) override;

  ndk::ScopedAStatus callStateChanged(RadioIndicationType type) override;

  ndk::ScopedAStatus cdmaCallWaiting(
    RadioIndicationType type,
    const CdmaCallWaiting& callWaitingRecord) override;

  ndk::ScopedAStatus cdmaInfoRec(
    RadioIndicationType type,
    const std::vector<CdmaInformationRecord>& records) override;

  ndk::ScopedAStatus cdmaOtaProvisionStatus(
    RadioIndicationType type,
    CdmaOtaProvisionStatus status) override;

  ndk::ScopedAStatus currentEmergencyNumberList(
    RadioIndicationType type,
    const std::vector<EmergencyNumber>& emergencyNumberList) override;

  ndk::ScopedAStatus enterEmergencyCallbackMode(
    RadioIndicationType type) override;

  ndk::ScopedAStatus exitEmergencyCallbackMode(
    RadioIndicationType type) override;

  ndk::ScopedAStatus indicateRingbackTone(RadioIndicationType type,
                                          bool start) override;

  ndk::ScopedAStatus onSupplementaryServiceIndication(
    RadioIndicationType type,
    const StkCcUnsolSsResult& ss) override;

  ndk::ScopedAStatus onUssd(RadioIndicationType type,
                            UssdModeType modeType,
                            const std::string& msg) override;

  ndk::ScopedAStatus resendIncallMute(RadioIndicationType type) override;

  ndk::ScopedAStatus srvccStateNotify(RadioIndicationType type,
                                      SrvccState state) override;

  ndk::ScopedAStatus stkCallControlAlphaNotify(
    RadioIndicationType type,
    const std::string& alpha) override;

  ndk::ScopedAStatus stkCallSetup(RadioIndicationType type,
                                  int64_t timeout) override;
};

class SimIndication : public BnRadioSimIndication
{
protected:
  nsRadioProxyServiceManager& mManager;

public:
  SimIndication(nsRadioProxyServiceManager& manager)
    : mManager(manager){};
  ~SimIndication() = default;

  void defaultResponse(const RadioIndicationType type,
                       const nsString& rilmessageType);
  ::ndk::ScopedAStatus carrierInfoForImsiEncryption(
    RadioIndicationType in_info);
  ::ndk::ScopedAStatus cdmaSubscriptionSourceChanged(
    RadioIndicationType in_type,
    sim::CdmaSubscriptionSource in_cdmaSource);
  ::ndk::ScopedAStatus simPhonebookChanged(RadioIndicationType in_type);
  ::ndk::ScopedAStatus simPhonebookRecordsReceived(
    RadioIndicationType in_type,
    sim::PbReceivedStatus in_status,
    const std::vector<sim::PhonebookRecordInfo>& in_records);
  ::ndk::ScopedAStatus simRefresh(
    RadioIndicationType in_type,
    const sim::SimRefreshResult& in_refreshResult);
  ::ndk::ScopedAStatus simStatusChanged(RadioIndicationType in_type);
  ::ndk::ScopedAStatus stkEventNotify(RadioIndicationType in_type,
                                      const std::string& in_cmd);
  ::ndk::ScopedAStatus stkProactiveCommand(RadioIndicationType in_type,
                                           const std::string& in_cmd);
  ::ndk::ScopedAStatus stkSessionEnd(RadioIndicationType in_type);
  ::ndk::ScopedAStatus subscriptionStatusChanged(RadioIndicationType in_type,
                                                 bool in_activate);
  ::ndk::ScopedAStatus uiccApplicationsEnablementChanged(
    RadioIndicationType in_type,
    bool in_enabled);
};

class RadioSimResponse : public BnRadioSimResponse
{
protected:
  nsRadioProxyServiceManager& mManager;

public:
  RadioSimResponse(nsRadioProxyServiceManager& manager);
  ~RadioSimResponse() = default;

public:
  ::ndk::ScopedAStatus acknowledgeRequest(int32_t in_serial)
  {
    return ndk::ScopedAStatus::ok();
  };

  void defaultResponse(const RadioResponseInfo& rspInfo,
                       const nsString& rilmessageType);
  ::ndk::ScopedAStatus areUiccApplicationsEnabledResponse(
    const RadioResponseInfo& in_info,
    bool in_enabled);
  ::ndk::ScopedAStatus changeIccPin2ForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus changeIccPinForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus enableUiccApplicationsResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus getAllowedCarriersResponse(
    const RadioResponseInfo& in_info,
    const sim::CarrierRestrictions& in_carriers,
    sim::SimLockMultiSimPolicy in_multiSimPolicy);
  ::ndk::ScopedAStatus getCdmaSubscriptionResponse(
    const RadioResponseInfo& in_info,
    const std::string& in_mdn,
    const std::string& in_hSid,
    const std::string& in_hNid,
    const std::string& in_min,
    const std::string& in_prl);
  ::ndk::ScopedAStatus getCdmaSubscriptionSourceResponse(
    const RadioResponseInfo& in_info,
    sim::CdmaSubscriptionSource in_source);
  ::ndk::ScopedAStatus getFacilityLockForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_response);
  ::ndk::ScopedAStatus getIccCardStatusResponse(
    const RadioResponseInfo& in_info,
    const sim::CardStatus& in_cardStatus);
  ::ndk::ScopedAStatus getImsiForAppResponse(const RadioResponseInfo& in_info,
                                             const std::string& in_imsi);
  ::ndk::ScopedAStatus getSimPhonebookCapacityResponse(
    const RadioResponseInfo& in_info,
    const sim::PhonebookCapacity& in_capacity);
  ::ndk::ScopedAStatus getSimPhonebookRecordsResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus iccCloseLogicalChannelResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus iccIoForAppResponse(const RadioResponseInfo& in_info,
                                           const sim::IccIoResult& in_iccIo);
  ::ndk::ScopedAStatus iccOpenLogicalChannelResponse(
    const RadioResponseInfo& in_info,
    int32_t in_channelId,
    const std::vector<uint8_t>& in_selectResponse);
  ::ndk::ScopedAStatus iccTransmitApduBasicChannelResponse(
    const RadioResponseInfo& in_info,
    const sim::IccIoResult& in_result);
  ::ndk::ScopedAStatus iccTransmitApduLogicalChannelResponse(
    const RadioResponseInfo& in_info,
    const sim::IccIoResult& in_result);
  ::ndk::ScopedAStatus reportStkServiceIsRunningResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus requestIccSimAuthenticationResponse(
    const RadioResponseInfo& in_info,
    const sim::IccIoResult& in_result);
  ::ndk::ScopedAStatus sendEnvelopeResponse(
    const RadioResponseInfo& in_info,
    const std::string& in_commandResponse);
  ::ndk::ScopedAStatus sendEnvelopeWithStatusResponse(
    const RadioResponseInfo& in_info,
    const sim::IccIoResult& in_iccIo);
  ::ndk::ScopedAStatus sendTerminalResponseToSimResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus setAllowedCarriersResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus setCarrierInfoForImsiEncryptionResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus setCdmaSubscriptionSourceResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus setFacilityLockForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_retry);
  ::ndk::ScopedAStatus setSimCardPowerResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus setUiccSubscriptionResponse(
    const RadioResponseInfo& in_info);
  ::ndk::ScopedAStatus supplyIccPin2ForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus supplyIccPinForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus supplyIccPuk2ForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus supplyIccPukForAppResponse(
    const RadioResponseInfo& in_info,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus supplySimDepersonalizationResponse(
    const RadioResponseInfo& in_info,
    sim::PersoSubstate in_persoType,
    int32_t in_remainingRetries);
  ::ndk::ScopedAStatus updateSimPhonebookRecordsResponse(
    const RadioResponseInfo& in_info,
    int32_t in_updatedRecordIndex);
  ::ndk::ScopedAStatus iccCloseLogicalChannelWithSessionInfoResponse(
    const RadioResponseInfo& in_info);
};

class RadioConfigIndication : public BnRadioConfigIndication
{
public:
  nsRadioProxyServiceManager& mManager;
  RadioConfigIndication(nsRadioProxyServiceManager&);
  ~RadioConfigIndication() = default;

  ::ndk::ScopedAStatus simSlotsStatusChanged(
    ::aidl::android::hardware::radio::RadioIndicationType in_type,
    const std::vector<::aidl::android::hardware::radio::config::SimSlotStatus>&
      in_slotStatus) override;
};

class RadioConfigResponse : public BnRadioConfigResponse
{
public:
  nsRadioProxyServiceManager& mManager;
  RadioConfigResponse(nsRadioProxyServiceManager&);
  ~RadioConfigResponse() = default;
  ::ndk::ScopedAStatus getHalDeviceCapabilitiesResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
    bool in_modemReducedFeatureSet1) override;
  ::ndk::ScopedAStatus getNumOfLiveModemsResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
    int8_t in_numOfLiveModems) override;
  ::ndk::ScopedAStatus getPhoneCapabilityResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
    const ::aidl::android::hardware::radio::config::PhoneCapability&
      in_phoneCapability) override;
  ::ndk::ScopedAStatus getSimSlotsStatusResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
    const std::vector<::aidl::android::hardware::radio::config::SimSlotStatus>&
      in_slotStatus) override;
  ::ndk::ScopedAStatus setNumOfLiveModemsResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
    override;
  ::ndk::ScopedAStatus setPreferredDataModemResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
    override;
  ::ndk::ScopedAStatus setSimSlotsMappingResponse(
    const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
    override;
};

class nsRadioProxyServiceManager final : public nsIRadioProxyServiceManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRADIOPROXYSERVICEMANAGER

  nsRadioProxyServiceManager(int32_t clientId, nsRilWorker* aRil);
  void sendAck(int32_t aToken, uint8_t aServiceType);
  void defaultResponse(const RadioResponseInfo& rspInfo,
                       const nsString& rilmessageType);
  void defaultResponse(const RadioIndicationType& rspInfo,
                       const nsString& rilmessageType);
  void sendRspAck(const RadioResponseInfo& rspInfo,
                  int32_t aToken,
                  uint8_t aServiceType);
  void sendIndAck(const RadioIndicationType& type, uint8_t aServiceType);
  RefPtr<nsRilWorker> mRIL;
  int32_t convertRadioErrorToNum(RadioError error);
  int32_t convertHardwareConfigType(int32_t type);
  int32_t convertRadioCapabilityPhase(int32_t value);
  int32_t convertRadioAccessFamily(RadioAccessFamily value);
  int32_t convertRadioCapabilityStatus(int32_t state);
  int32_t convertHardwareConfigState(int32_t state);
  int32_t convertRadioStateToNum(RadioState state);
  int32_t convertUssdModeType(UssdModeType type);
  int32_t convertSrvccState(SrvccState state);
  int32_t convertCallForwardState(int32_t status);
  int32_t convertClipState(ClipStatus status);
  int32_t convertUusType(int32_t type);
  int32_t convertUusDcs(int32_t dcs);
  int32_t convertCallState(int32_t state);
  int32_t convertCallPresentation(int32_t state);
  int32_t convertAudioQuality(AudioQuality type);
  void convertToCdmaSmsMessage(nsICdmaSmsMessage* in_msg,
                               CdmaSmsMessage& aMessage);
  DataProfileInfo convertToHalDataProfile(
    nsIDataProfile* profile,
    nsITrafficDescriptor* trafficDescriptor);
  LinkAddress convertToHalLinkAddress(nsILinkAddress* linkAddress);
  SliceInfo convertToHalSliceInfo(nsISliceInfo* sliceinfo);
  void onRadioDataBinderDied();
  void onRadioVoiceBinderDied();
  int32_t convertRadioTechnology(RadioTechnology rat);
  int32_t convertPhoneRestrictedState(PhoneRestrictedState state);
  int32_t convertHalNetworkTypeBitMask(int32_t networkTypeBitmap);
  int32_t convertOperatorState(int32_t status);
  // reg
  int32_t convertRegState(RegState_aidl state);
  RefPtr<nsNrIndicators> convertNrIndicators(
    const NrIndicators_aidl* aNrIndicators);
  RefPtr<nsLteVopsInfo> convertVopsInfo(const LteVopsInfo_aidl* aVopsInfo);
  RefPtr<nsNrVopsInfo> convertNrVopsInfo(const NrVopsInfo_aidl* aNrVopsInfo);
  // SignalStrength
  RefPtr<nsSignalStrength> convertSignalStrength(
    const SignalStrength_aidl& aSignalStrength);
  RefPtr<nsGsmSignalStrength> convertGsmSignalStrength(
    const GsmSignalStrength_aidl& aGsmSignalStrength);
  RefPtr<nsCdmaSignalStrength> convertCdmaSignalStrength(
    const CdmaSignalStrength_aidl& aCdmaSignalStrength);
  RefPtr<nsEvdoSignalStrength> convertEvdoSignalStrength(
    const EvdoSignalStrength_aidl& aEvdoSignalStrength);
  RefPtr<nsWcdmaSignalStrength> convertWcdmaSignalStrength(
    const WcdmaSignalStrength_aidl& aWcdmaSignalStrength);
  RefPtr<nsTdScdmaSignalStrength> convertTdScdmaSignalStrength(
    const TdscdmaSignalStrength_aidl& aTdScdmaSignalStrength);
  RefPtr<nsLteSignalStrength> convertLteSignalStrength(
    const LteSignalStrength_aidl& aLteSignalStrength);
  RefPtr<nsNrSignalStrength> convertNrSignalStrength(
    const NrSignalStrength_aidl* aNrSignalStrength);
  // CellIdentity
  int32_t convertCellIdentitytoCellInfoType(
    aidl::android::hardware::radio::network::CellIdentity::Tag tag);
  RefPtr<nsCellIdentity> convertCellIdentity(
    const CellIdentity_aidl* aCellIdentity);
  RefPtr<nsCellIdentityOperatorNames> convertCellIdentityOperatorNames(
    const OperatorInfo_aidl& aOperatorNames);
  RefPtr<nsCellIdentityGsm> convertCellIdentityGsm(
    const CellIdentityGsm_aidl* aCellIdentityGsm);
  RefPtr<nsCellIdentityCsgInfo> convertCsgInfo(
    const ClosedSubscriberGroupInfo_aidl* aCsgInfo);
  RefPtr<nsCellIdentityLte> convertCellIdentityLte(
    const CellIdentityLte_aidl* aCellIdentityLte);
  RefPtr<nsCellIdentityWcdma> convertCellIdentityWcdma(
    const CellIdentityWcdma_aidl* aCellIdentityWcdma);
  RefPtr<nsCellIdentityTdScdma> convertCellIdentityTdScdma(
    const CellIdentityTdscdma_aidl* aCellIdentityTdScdma);
  RefPtr<nsCellIdentityCdma> convertCellIdentityCdma(
    const CellIdentityCdma_aidl* aCellIdentityCdma);
  RefPtr<nsCellIdentityNr> convertCellIdentityNr(
    const CellIdentityNr_aidl* aCellIdentityNr);
  // CellInfo
  int32_t convertCellInfoTypeToRil(
    aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag tag);
  int32_t convertConnectionStatus(CellConnectionStatus_aidl status);
  RefPtr<nsRilCellInfo> convertRilCellInfo(const CellInfo_aidl* aCellInfo);
  RefPtr<nsCellInfoGsm> convertCellInfoGsm(
    const CellInfoGsm_aidl* aCellInfoGsm);
  RefPtr<nsCellInfoLte> convertCellInfoLte(
    const CellInfoLte_aidl* aCellInfoLte);
  RefPtr<nsCellInfoWcdma> convertCellInfoWcdma(
    const CellInfoWcdma_aidl* aCellInfoWcdma);
  RefPtr<nsCellInfoTdScdma> convertCellInfoTdScdma(
    const CellInfoTdscdma_aidl* aCellInfoTdscdma);
  RefPtr<nsCellInfoCdma> convertCellInfoCdma(
    const CellInfoCdma_aidl* aCellInfoCdma);
  RefPtr<nsCellInfoNr> convertCellInfoNr(const CellInfoNr_aidl* aCellInfoNr);
  RadioAccessSpecifier_aidl convertToaidlRadioAccessSpecifier(
    nsIRadioAccessSpecifier* specifier);
  void onRadioSimBinderDied();
  void onRadioConfigBinderDied();

private:
  ~nsRadioProxyServiceManager();

  void updateDebug();
  void loadRadioDataProxy();
  void loadRadioVoiceProxy();
  void loadRadioSimProxy();
  void loadRadioConfigProxy();
  void loadRadioMessagingProxy();
  void loadRadioModemProxy();
  void loadRadioNetworkProxy();
  int32_t mClientId;
  AIBinder_DeathRecipient* mRadioDataDeathRecipient{ nullptr };
  AIBinder_DeathRecipient* mRadioVoiceDeathRecipient{ nullptr };
  AIBinder_DeathRecipient* mRadioSimDeathRecipient{ nullptr };
  AIBinder_DeathRecipient* mRadioConfigDeathRecipient{ nullptr };
  std::shared_ptr<IRadioSim> mRadioSimProxy;
  std::shared_ptr<RadioSimResponse> mRadioRsp_sim;
  /* radio data indication handle */
  std::shared_ptr<SimIndication> mRadioInd_sim;
  std::shared_ptr<IRadioConfig> mRadioConfigProxy;
  std::shared_ptr<RadioConfigResponse> mRadioRsp_config;
  /* radio data indication handle */
  std::shared_ptr<RadioConfigIndication> mRadioInd_config;
  std::shared_ptr<IRadioData> mRadioDataProxy;
  /* radio data response handle */
  std::shared_ptr<RadioDataResponse> mRadioRsp_data;
  /* radio data indication handle */
  std::shared_ptr<RadioDataIndication> mRadioInd_data;
  std::shared_ptr<IRadioVoice> mRadioVoiceProxy;
  /* radio voice response handle */
  std::shared_ptr<RadioVoiceResponse> mRadioRsp_voice;
  /* radio voice indication handle */
  std::shared_ptr<RadioVoiceIndication> mRadioInd_voice;
  std::shared_ptr<IRadioMessaging> mRadioMessagingProxy;
  std::shared_ptr<RadioMessagingResponse> mRadioRsp_messaging;
  std::shared_ptr<RadioMessagingIndication> mRadioInd_messaging;
  std::shared_ptr<IRadioModem> mRadioModemProxy;
  std::shared_ptr<RadioModemResponse> mRadioRsp_modem;
  std::shared_ptr<RadioModemIndication> mRadioInd_modem;
  /* radio network response handle */
  std::shared_ptr<IRadioNetwork> mRadioNetworkProxy;
  /* radio network response handle */
  std::shared_ptr<RadioNetworkResponse> mRadioRsp_network;
  /* radio network indication handle */
  std::shared_ptr<RadioNetworkIndication> mRadioInd_network;
};
#endif /* NSRADIOPRXOYSERVICEMANAGER_H */
