/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilResponse_H
#define nsRilResponse_H
#if ANDROID_VERSION >= 33
#include <android/hardware/radio/1.6/IRadioResponse.h>
#else
#include <android/hardware/radio/1.1/IRadioResponse.h>
#endif
#include <nsISupportsImpl.h>
#include <nsTArray.h>

#if ANDROID_VERSION <= 33
#include <android/hardware/radio/1.1/IRadioResponse.h>
#endif

using namespace ::android::hardware::radio::V1_1;
using ::android::sp;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ActivityStatsInfo_V1_0 =
  ::android::hardware::radio::V1_0::ActivityStatsInfo;
using AppState_V1_0 = ::android::hardware::radio::V1_0::AppState;
using AppType_V1_0 = ::android::hardware::radio::V1_0::AppType;
using Call_V1_0 = ::android::hardware::radio::V1_0::Call;
using CallForwardInfo_V1_0 = ::android::hardware::radio::V1_0::CallForwardInfo;
using CallForwardInfoStatus_V1_0 =
  ::android::hardware::radio::V1_0::CallForwardInfoStatus;
using CallPresentation_V1_0 =
  ::android::hardware::radio::V1_0::CallPresentation;
using CallState_V1_0 = ::android::hardware::radio::V1_0::CallState;
using CardState_V1_0 = ::android::hardware::radio::V1_0::CardState;
using CardStatus_V1_0 = ::android::hardware::radio::V1_0::CardStatus;
using CarrierRestrictions_V1_0 =
  ::android::hardware::radio::V1_0::CarrierRestrictions;
using CdmaBroadcastSmsConfigInfo_V1_0 =
  ::android::hardware::radio::V1_0::CdmaBroadcastSmsConfigInfo;
using CdmaRoamingType_V1_0 = ::android::hardware::radio::V1_0::CdmaRoamingType;
using CdmaSubscriptionSource_V1_0 =
  ::android::hardware::radio::V1_0::CdmaSubscriptionSource;
using CellInfo_V1_0 = ::android::hardware::radio::V1_0::CellInfo;
using ClipStatus_V1_0 = ::android::hardware::radio::V1_0::ClipStatus;
using DataRegStateResult_V1_0 =
  ::android::hardware::radio::V1_0::DataRegStateResult;
using GsmBroadcastSmsConfigInfo_V1_0 =
  ::android::hardware::radio::V1_0::GsmBroadcastSmsConfigInfo;
using HardwareConfig_V1_0 = ::android::hardware::radio::V1_0::HardwareConfig;
using IccIoResult_V1_0 = ::android::hardware::radio::V1_0::IccIoResult;
using LastCallFailCause_V1_0 =
  ::android::hardware::radio::V1_0::LastCallFailCause;
using LastCallFailCauseInfo_V1_0 =
  ::android::hardware::radio::V1_0::LastCallFailCauseInfo;
using LceDataInfo_V1_0 = ::android::hardware::radio::V1_0::LceDataInfo;
using LceStatusInfo_V1_0 = ::android::hardware::radio::V1_0::LceStatusInfo;
using NeighboringCell_V1_0 = ::android::hardware::radio::V1_0::NeighboringCell;
using OperatorInfo_V1_0 = ::android::hardware::radio::V1_0::OperatorInfo;
using OperatorStatus_V1_0 = ::android::hardware::radio::V1_0::OperatorStatus;
using PersoSubstate_V1_0 = ::android::hardware::radio::V1_0::PersoSubstate;
using PinState_V1_0 = ::android::hardware::radio::V1_0::PinState;
using PreferredNetworkType_V1_0 =
  ::android::hardware::radio::V1_0::PreferredNetworkType;
using RadioBandMode_V1_0 = ::android::hardware::radio::V1_0::RadioBandMode;
using RadioCapability_V1_0 = ::android::hardware::radio::V1_0::RadioCapability;
using RadioError_V1_0 = ::android::hardware::radio::V1_0::RadioError;
using RadioTechnology_V1_0 = ::android::hardware::radio::V1_0::RadioTechnology;
using RadioTechnologyFamily_V1_0 =
  ::android::hardware::radio::V1_0::RadioTechnologyFamily;
using RegState_V1_0 = ::android::hardware::radio::V1_0::RegState;
using SendSmsResult_V1_0 = ::android::hardware::radio::V1_0::SendSmsResult;
using TtyMode_V1_0 = ::android::hardware::radio::V1_0::TtyMode;
using UusDcs_V1_0 = ::android::hardware::radio::V1_0::UusDcs;
using UusType_V1_0 = ::android::hardware::radio::V1_0::UusType;
using VoiceRegStateResult_V1_0 =
  ::android::hardware::radio::V1_0::VoiceRegStateResult;
using SetupDataCallResult_V1_0 =
  ::android::hardware::radio::V1_0::SetupDataCallResult;
using KeepaliveStatus_V1_1 = ::android::hardware::radio::V1_1::KeepaliveStatus;
using ::android::hardware::radio::V1_1::IRadioResponse;
using RadioResponseInfo_V1_0 =
  ::android::hardware::radio::V1_0::RadioResponseInfo;
using KeepaliveStatusCode_V1_1 =
  ::android::hardware::radio::V1_1::KeepaliveStatusCode;

#if ANDROID_VERSION >= 33
using IRadioCardStatus_V1_2 = android::hardware::radio::V1_2::CardStatus;
using IRadioCall_V1_2 = android::hardware::radio::V1_2::Call;
using IRadioDataRegStateResult_V1_2 =
  android::hardware::radio::V1_2::DataRegStateResult;
using IRadioResponse_V1_2 = android::hardware::radio::V1_2::IRadioResponse;
using ::android::hardware::radio::V1_0::SignalStrength;
using IRadioSignalStrength_V1_2 =
  android::hardware::radio::V1_2::SignalStrength;
using IRadioVoiceRegStateResult_V1_2 =
  android::hardware::radio::V1_2::VoiceRegStateResult;
using CellInfo_V1_2 = ::android::hardware::radio::V1_2::CellInfo;
using IRadioResponse_V1_3 = android::hardware::radio::V1_2::IRadioResponse;
using IRadioResponse_V1_4 = android::hardware::radio::V1_4::IRadioResponse;
using RadioAccessFamily_V1_4 =
  android::hardware::radio::V1_4::RadioAccessFamily;
using CardStatus_V1_4 = ::android::hardware::radio::V1_4::CardStatus;
using SignalStrength_V1_4 = ::android::hardware::radio::V1_4::SignalStrength;
using DataRegStateResult_V1_4 =
  ::android::hardware::radio::V1_4::DataRegStateResult;
using SetupDataCallResult_V1_4 =
  ::android::hardware::radio::V1_4::SetupDataCallResult;
using CellInfo_V1_4 = ::android::hardware::radio::V1_4::CellInfo;
using CarrierRestrictionsWithPriority_V1_4 =
  ::android::hardware::radio::V1_4::CarrierRestrictionsWithPriority;
using SimLockMultiSimPolicy_V1_4 =
  ::android::hardware::radio::V1_4::SimLockMultiSimPolicy;
using PersoSubstate_V1_5 = ::android::hardware::radio::V1_5::PersoSubstate;
using SetupDataCallResult_V1_5 =
  ::android::hardware::radio::V1_5::SetupDataCallResult;
using RegStateResult_V1_5 = ::android::hardware::radio::V1_5::RegStateResult;
using IRadioResponse_V1_5 = android::hardware::radio::V1_5::IRadioResponse;
using ATSIType = ::android::hardware::radio::V1_5::RegStateResult::
  AccessTechnologySpecificInfo::hidl_discriminator;
using CellInfo_V1_5 = ::android::hardware::radio::V1_5::CellInfo;
using CardStatus_V1_5 = ::android::hardware::radio::V1_5::CardStatus;
using RadioAccessSpecifier_V1_5 =
  ::android::hardware::radio::V1_5::RadioAccessSpecifier;
using IRadioResponse_V1_6 = android::hardware::radio::V1_6::IRadioResponse;
using RadioResponseInfo_V1_6 =
  ::android::hardware::radio::V1_6::RadioResponseInfo;
using SetupDataCallResult_V1_6 =
  ::android::hardware::radio::V1_6::SetupDataCallResult;
using RadioResponseInfo_V1_6 =
  ::android::hardware::radio::V1_6::RadioResponseInfo;
using IRadioCall_V1_6 = ::android::hardware::radio::V1_6::Call;
using RegStateResult_V1_6 = ::android::hardware::radio::V1_6::RegStateResult;
using SignalStrength_V1_6 = ::android::hardware::radio::V1_6::SignalStrength;
using IRadioCall_V1_6 = ::android::hardware::radio::V1_6::Call;
using RadioError_V1_6 = ::android::hardware::radio::V1_6::RadioError;
using ATSIType_V1_6 = ::android::hardware::radio::V1_6::RegStateResult::
  AccessTechnologySpecificInfo::hidl_discriminator;
using CellInfo_V1_6 = ::android::hardware::radio::V1_6::CellInfo;
using SlicingConfig_V1_6 = ::android::hardware::radio::V1_6::SlicingConfig;
using PhonebookCapacity_V1_6 =
  ::android::hardware::radio::V1_6::PhonebookCapacity;
#endif

class nsRilWorker;
#if ANDROID_VERSION >= 33
class nsRilResponse : public IRadioResponse_V1_6
#else
class nsRilResponse : public IRadioResponse
#endif
{
public:
  explicit nsRilResponse(nsRilWorker* aRil);
  ~nsRilResponse();

  RadioResponseInfo_V1_0 rspInfo;

#if ANDROID_VERSION >= 33
  RadioResponseInfo_V1_6 rspInfoV1_6;
#endif

  Return<void> getIccCardStatusResponse(const RadioResponseInfo_V1_0& info,
                                        const CardStatus_V1_0& cardStatus);

  Return<void> supplyIccPinForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries);

  Return<void> supplyIccPukForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries);

  Return<void> supplyIccPin2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries);

  Return<void> supplyIccPuk2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries);

  Return<void> changeIccPinForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries);

  Return<void> changeIccPin2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries);

  Return<void> supplyNetworkDepersonalizationResponse(
    const RadioResponseInfo_V1_0& info,
    int32_t remainingRetries);

  Return<void> getCurrentCallsResponse(const RadioResponseInfo_V1_0& info,
                                       const hidl_vec<Call_V1_0>& calls);

  Return<void> dialResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getIMSIForAppResponse(const RadioResponseInfo_V1_0& info,
                                     const hidl_string& imsi);

  Return<void> hangupConnectionResponse(const RadioResponseInfo_V1_0& info);

  Return<void> hangupWaitingOrBackgroundResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> hangupForegroundResumeBackgroundResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> switchWaitingOrHoldingAndActiveResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> conferenceResponse(const RadioResponseInfo_V1_0& info);

  Return<void> rejectCallResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getLastCallFailCauseResponse(
    const RadioResponseInfo_V1_0& info,
    const LastCallFailCauseInfo_V1_0& failCauseInfo);

  Return<void> getSignalStrengthResponse(const RadioResponseInfo_V1_0& info,
                                         const SignalStrength& sigStrength);

  Return<void> getVoiceRegistrationStateResponse(
    const RadioResponseInfo_V1_0& info,
    const VoiceRegStateResult_V1_0& voiceRegResponse);

  Return<void> getDataRegistrationStateResponse(
    const RadioResponseInfo_V1_0& info,
    const DataRegStateResult_V1_0& dataRegResponse);

  Return<void> getOperatorResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_string& longName,
    const ::android::hardware::hidl_string& shortName,
    const ::android::hardware::hidl_string& numeric);

  Return<void> setRadioPowerResponse(const RadioResponseInfo_V1_0& info);

  Return<void> sendDtmfResponse(const RadioResponseInfo_V1_0& info);

  Return<void> sendSmsResponse(const RadioResponseInfo_V1_0& info,
                               const SendSmsResult_V1_0& sms);

  Return<void> sendSMSExpectMoreResponse(const RadioResponseInfo_V1_0& info,
                                         const SendSmsResult_V1_0& sms);

  Return<void> setupDataCallResponse(
    const RadioResponseInfo_V1_0& info,
    const SetupDataCallResult_V1_0& dcResponse);

  Return<void> iccIOForAppResponse(const RadioResponseInfo_V1_0& info,
                                   const IccIoResult_V1_0& iccIo);

  Return<void> sendUssdResponse(const RadioResponseInfo_V1_0& info);

  Return<void> cancelPendingUssdResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getClirResponse(const RadioResponseInfo_V1_0& info,
                               int32_t n,
                               int32_t m);

  Return<void> setClirResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getCallForwardStatusResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<CallForwardInfo_V1_0>& call_forwardInfos);

  Return<void> setCallForwardResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getCallWaitingResponse(const RadioResponseInfo_V1_0& info,
                                      bool enable,
                                      int32_t serviceClass);

  Return<void> setCallWaitingResponse(const RadioResponseInfo_V1_0& info);

  Return<void> acknowledgeLastIncomingGsmSmsResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> acceptCallResponse(const RadioResponseInfo_V1_0& info);

  Return<void> deactivateDataCallResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getFacilityLockForAppResponse(const RadioResponseInfo_V1_0& info,
                                             int32_t response);

  Return<void> setFacilityLockForAppResponse(const RadioResponseInfo_V1_0& info,
                                             int32_t retry);

  Return<void> setBarringPasswordResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getNetworkSelectionModeResponse(
    const RadioResponseInfo_V1_0& info,
    bool manual);

  Return<void> setNetworkSelectionModeAutomaticResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> setNetworkSelectionModeManualResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getAvailableNetworksResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<OperatorInfo_V1_0>& networkInfos);

  Return<void> startDtmfResponse(const RadioResponseInfo_V1_0& info);

  Return<void> stopDtmfResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getBasebandVersionResponse(const RadioResponseInfo_V1_0& info,
                                          const hidl_string& version);

  Return<void> separateConnectionResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setMuteResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getMuteResponse(const RadioResponseInfo_V1_0& info, bool enable);

  Return<void> getClipResponse(const RadioResponseInfo_V1_0& info,
                               ClipStatus_V1_0 status);

  Return<void> getDataCallListResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<SetupDataCallResult_V1_0>& dcResponse);

  Return<void> sendOemRilRequestRawResponse(const RadioResponseInfo_V1_0& info,
                                            const hidl_vec<uint8_t>& data);

  Return<void> sendOemRilRequestStringsResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<hidl_string>& data);

  Return<void> setSuppServiceNotificationsResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> writeSmsToSimResponse(const RadioResponseInfo_V1_0& info,
                                     int32_t index);

  Return<void> deleteSmsOnSimResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setBandModeResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getAvailableBandModesResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<RadioBandMode_V1_0>& bandModes);

  Return<void> sendEnvelopeResponse(const RadioResponseInfo_V1_0& info,
                                    const hidl_string& commandResponse);

  Return<void> sendTerminalResponseToSimResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> handleStkCallSetupRequestFromSimResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> explicitCallTransferResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setPreferredNetworkTypeResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getPreferredNetworkTypeResponse(
    const RadioResponseInfo_V1_0& info,
    PreferredNetworkType_V1_0 nwType);

  Return<void> getNeighboringCidsResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<NeighboringCell_V1_0>& cells);

  Return<void> setLocationUpdatesResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setCdmaSubscriptionSourceResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> setCdmaRoamingPreferenceResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getCdmaRoamingPreferenceResponse(
    const RadioResponseInfo_V1_0& info,
    CdmaRoamingType_V1_0 type);

  Return<void> setTTYModeResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getTTYModeResponse(const RadioResponseInfo_V1_0& info,
                                  TtyMode_V1_0 mode);

  Return<void> setPreferredVoicePrivacyResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getPreferredVoicePrivacyResponse(
    const RadioResponseInfo_V1_0& info,
    bool enable);

  Return<void> sendCDMAFeatureCodeResponse(const RadioResponseInfo_V1_0& info);

  Return<void> sendBurstDtmfResponse(const RadioResponseInfo_V1_0& info);

  Return<void> sendCdmaSmsResponse(const RadioResponseInfo_V1_0& info,
                                   const SendSmsResult_V1_0& sms);

  Return<void> acknowledgeLastIncomingCdmaSmsResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getGsmBroadcastConfigResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<GsmBroadcastSmsConfigInfo_V1_0>& configs);

  Return<void> setGsmBroadcastConfigResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> setGsmBroadcastActivationResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getCdmaBroadcastConfigResponse(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<CdmaBroadcastSmsConfigInfo_V1_0>& configs);

  Return<void> setCdmaBroadcastConfigResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> setCdmaBroadcastActivationResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getCDMASubscriptionResponse(const RadioResponseInfo_V1_0& info,
                                           const hidl_string& mdn,
                                           const hidl_string& hSid,
                                           const hidl_string& hNid,
                                           const hidl_string& min,
                                           const hidl_string& prl);

  Return<void> writeSmsToRuimResponse(const RadioResponseInfo_V1_0& info,
                                      uint32_t index);

  Return<void> deleteSmsOnRuimResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getDeviceIdentityResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_string& imei,
    const ::android::hardware::hidl_string& imeisv,
    const ::android::hardware::hidl_string& esn,
    const ::android::hardware::hidl_string& meid);

  Return<void> exitEmergencyCallbackModeResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getSmscAddressResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_string& smsc);

  Return<void> setSmscAddressResponse(const RadioResponseInfo_V1_0& info);

  Return<void> reportSmsMemoryStatusResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> reportStkServiceIsRunningResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getCdmaSubscriptionSourceResponse(
    const RadioResponseInfo_V1_0& info,
    CdmaSubscriptionSource_V1_0 source);

  Return<void> requestIsimAuthenticationResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_string& response);

  Return<void> acknowledgeIncomingGsmSmsWithPduResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> sendEnvelopeWithStatusResponse(
    const RadioResponseInfo_V1_0& info,
    const IccIoResult_V1_0& iccIo);

  Return<void> getVoiceRadioTechnologyResponse(
    const RadioResponseInfo_V1_0& info,
    RadioTechnology_V1_0 rat);

  Return<void> getCellInfoListResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_vec<CellInfo_V1_0>& cellInfo);

  Return<void> setCellInfoListRateResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setInitialAttachApnResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getImsRegistrationStateResponse(
    const RadioResponseInfo_V1_0& info,
    bool isRegistered,
    RadioTechnologyFamily_V1_0 ratFamily);

  Return<void> sendImsSmsResponse(const RadioResponseInfo_V1_0& info,
                                  const SendSmsResult_V1_0& sms);

  Return<void> iccTransmitApduBasicChannelResponse(
    const RadioResponseInfo_V1_0& info,
    const IccIoResult_V1_0& result);

  Return<void> iccOpenLogicalChannelResponse(
    const RadioResponseInfo_V1_0& info,
    int32_t channelId,
    const ::android::hardware::hidl_vec<int8_t>& selectResponse);

  Return<void> iccCloseLogicalChannelResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> iccTransmitApduLogicalChannelResponse(
    const RadioResponseInfo_V1_0& info,
    const IccIoResult_V1_0& result);

  Return<void> nvReadItemResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_string& result);

  Return<void> nvWriteItemResponse(const RadioResponseInfo_V1_0& info);

  Return<void> nvWriteCdmaPrlResponse(const RadioResponseInfo_V1_0& info);

  Return<void> nvResetConfigResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setUiccSubscriptionResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setDataAllowedResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getHardwareConfigResponse(
    const RadioResponseInfo_V1_0& info,
    const ::android::hardware::hidl_vec<HardwareConfig_V1_0>& config);

  Return<void> requestIccSimAuthenticationResponse(
    const RadioResponseInfo_V1_0& info,
    const IccIoResult_V1_0& result);

  Return<void> setDataProfileResponse(const RadioResponseInfo_V1_0& info);

  Return<void> requestShutdownResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getRadioCapabilityResponse(const RadioResponseInfo_V1_0& info,
                                          const RadioCapability_V1_0& rc);

  Return<void> setRadioCapabilityResponse(const RadioResponseInfo_V1_0& info,
                                          const RadioCapability_V1_0& rc);

  Return<void> startLceServiceResponse(const RadioResponseInfo_V1_0& info,
                                       const LceStatusInfo_V1_0& statusInfo);

  Return<void> stopLceServiceResponse(const RadioResponseInfo_V1_0& info,
                                      const LceStatusInfo_V1_0& statusInfo);

  Return<void> pullLceDataResponse(const RadioResponseInfo_V1_0& info,
                                   const LceDataInfo_V1_0& lceInfo);

  Return<void> getModemActivityInfoResponse(
    const RadioResponseInfo_V1_0& info,
    const ActivityStatsInfo_V1_0& activityInfo);

  Return<void> setAllowedCarriersResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t numAllowed);

  Return<void> getAllowedCarriersResponse(
    const RadioResponseInfo_V1_0& info,
    bool allAllowed,
    const CarrierRestrictions_V1_0& carriers);

  Return<void> sendDeviceStateResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setIndicationFilterResponse(const RadioResponseInfo_V1_0& info);

  Return<void> setSimCardPowerResponse(const RadioResponseInfo_V1_0& info);

  Return<void> acknowledgeRequest(int32_t serial);

  Return<void> setCarrierInfoForImsiEncryptionResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> setSimCardPowerResponse_1_1(const RadioResponseInfo_V1_0& info);

  Return<void> startNetworkScanResponse(const RadioResponseInfo_V1_0& info);

  Return<void> stopNetworkScanResponse(const RadioResponseInfo_V1_0& info);

  Return<void> startKeepaliveResponse(const RadioResponseInfo_V1_0& info,
                                      const KeepaliveStatus_V1_1& status);

  Return<void> stopKeepaliveResponse(const RadioResponseInfo_V1_0& info);

#if ANDROID_VERSION >= 33
  Return<void> setLinkCapacityReportingCriteriaResponse(
    const RadioResponseInfo_V1_0& info);
#endif
  Return<void> setSystemSelectionChannelsResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> enableModemResponse(const RadioResponseInfo_V1_0& info);

  Return<void> getModemStackStatusResponse(const RadioResponseInfo_V1_0& info,
                                           bool isEnabled);

  Return<void> emergencyDialResponse(const RadioResponseInfo_V1_0& info);
////
  Return<void> setPreferredNetworkTypeBitmapResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> enableUiccApplicationsResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> areUiccApplicationsEnabledResponse(
    const RadioResponseInfo_V1_0& info,
    bool isEnabled);

  Return<void> sendCdmaSmsExpectMoreResponse(const RadioResponseInfo_V1_0& info,
                                             const SendSmsResult_V1_0& sms);

#if ANDROID_VERSION >= 33
  Return<void> setSignalStrengthReportingCriteriaResponse(
    const RadioResponseInfo_V1_0& info);

  Return<void> getCellInfoListResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<CellInfo_V1_2>& cellInfo);

  Return<void> getIccCardStatusResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const IRadioCardStatus_V1_2& card_status);

  Return<void> getCurrentCallsResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<IRadioCall_V1_2>& calls);

  Return<void> getSignalStrengthResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const IRadioSignalStrength_V1_2& signalStrength);

  Return<void> getVoiceRegistrationStateResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const IRadioVoiceRegStateResult_V1_2& voiceRegResponse);

  Return<void> getDataRegistrationStateResponse_1_2(
    const RadioResponseInfo_V1_0& info,
    const IRadioDataRegStateResult_V1_2& dataRegResponse);

  Return<void> startNetworkScanResponse_1_4(const RadioResponseInfo_V1_0& info);

  Return<void> getPreferredNetworkTypeBitmapResponse(
    const RadioResponseInfo_V1_0& info,

    hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap);

  Return<void> getIccCardStatusResponse_1_4(const RadioResponseInfo_V1_0& info,
                                            const CardStatus_V1_4& cardStatus);

  Return<void> getSignalStrengthResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const SignalStrength_V1_4& signalStrength);

  Return<void> getDataRegistrationStateResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const DataRegStateResult_V1_4& dataRegResponse);

  Return<void> setupDataCallResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const SetupDataCallResult_V1_4& dcResponse);

  Return<void> getDataCallListResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<SetupDataCallResult_V1_4>& dcResponse);

  Return<void> getCellInfoListResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<CellInfo_V1_4>& cellInfo);

  Return<void> setAllowedCarriersResponse_1_4(
    const RadioResponseInfo_V1_0& info);
  Return<void> getAllowedCarriersResponse_1_4(
    const RadioResponseInfo_V1_0& info,
    const CarrierRestrictionsWithPriority_V1_4& carriers,
    SimLockMultiSimPolicy_V1_4 multiSimPolicy);

  Return<void> setSignalStrengthReportingCriteriaResponse_1_5(
    const RadioResponseInfo_V1_0& info);
  Return<void> setLinkCapacityReportingCriteriaResponse_1_5(
    const RadioResponseInfo_V1_0& info);
  Return<void> setSystemSelectionChannelsResponse_1_5(
    const RadioResponseInfo_V1_0& info);

  Return<void> getBarringInfoResponse(
    const RadioResponseInfo_V1_0& info,
    const CellIdentity_V1_5& cellIdentity,
    const hidl_vec<BarringInfo_V1_5>& barringInfos);
  Return<void> startNetworkScanResponse_1_5(const RadioResponseInfo_V1_0& info);
  Return<void> setIndicationFilterResponse_1_5(
    const RadioResponseInfo_V1_0& info);

  Return<void> setDataProfileResponse_1_5(const RadioResponseInfo_V1_0& info);

  Return<void> setInitialAttachApnResponse_1_5(
    const RadioResponseInfo_V1_0& info);

  Return<void> setNetworkSelectionModeManualResponse_1_5(
    const RadioResponseInfo_V1_0& info);

  Return<void> setupDataCallResponse_1_5(
    const RadioResponseInfo_V1_0& info,
    const SetupDataCallResult_V1_5& dcResponse);

  Return<void> setRadioPowerResponse_1_5(const RadioResponseInfo_V1_0& info);

  Return<void> getDataRegistrationStateResponse_1_5(
    const RadioResponseInfo_V1_0& info,
    const RegStateResult_V1_5& dataRegResponse);

  Return<void> getVoiceRegistrationStateResponse_1_5(
    const RadioResponseInfo_V1_0& info,
    const RegStateResult_V1_5& voiceRegResponse);

  Return<void> getDataCallListResponse_1_5(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<SetupDataCallResult_V1_5>& dcResponse);
  Return<void> getCellInfoListResponse_1_5(
    const RadioResponseInfo_V1_0& info,
    const hidl_vec<CellInfo_V1_5>& cellInfo);

  Return<void> supplySimDepersonalizationResponse(
    const RadioResponseInfo_V1_0& info,
    PersoSubstate_V1_5 persoType,
    int32_t remainingRetries);

  Return<void> getIccCardStatusResponse_1_5(const RadioResponseInfo_V1_0& info,
                                            const CardStatus_V1_5& cardStatus);

  Return<void> setupDataCallResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const SetupDataCallResult_V1_6& dcResponse);

  Return<void> setRadioPowerResponse_1_6(const RadioResponseInfo_V1_6& info);

  Return<void> getDataRegistrationStateResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const RegStateResult_V1_6& dataRegResponse);

  Return<void> getVoiceRegistrationStateResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const RegStateResult_V1_6& voiceRegResponse);

  Return<void> getSignalStrengthResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const SignalStrength_V1_6& signalStrength);

  Return<void> getCurrentCallsResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const hidl_vec<IRadioCall_V1_6>& calls);

  Return<void> getDataCallListResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const hidl_vec<SetupDataCallResult_V1_6>& dcResponse);

  Return<void> sendSmsResponse_1_6(const RadioResponseInfo_V1_6& info,
                                   const SendSmsResult_V1_0& sms);

  Return<void> sendSmsExpectMoreResponse_1_6(const RadioResponseInfo_V1_6& info,
                                             const SendSmsResult_V1_0& sms);

  Return<void> sendCdmaSmsResponse_1_6(const RadioResponseInfo_V1_6& info,
                                       const SendSmsResult_V1_0& sms);

  Return<void> sendCdmaSmsExpectMoreResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const SendSmsResult_V1_0& sms);

  Return<void> setSimCardPowerResponse_1_6(const RadioResponseInfo_V1_6& info);

  Return<void> setNrDualConnectivityStateResponse(
    const RadioResponseInfo_V1_6& info);

  Return<void> isNrDualConnectivityEnabledResponse(
    const RadioResponseInfo_V1_6& info,
    bool isEnabled);

  Return<void> allocatePduSessionIdResponse(const RadioResponseInfo_V1_6& info,
                                            int32_t id);

  Return<void> releasePduSessionIdResponse(const RadioResponseInfo_V1_6& info);

  Return<void> startHandoverResponse(const RadioResponseInfo_V1_6& info);

  Return<void> cancelHandoverResponse(const RadioResponseInfo_V1_6& info);

  Return<void> setAllowedNetworkTypesBitmapResponse(
    const RadioResponseInfo_V1_6& info);

  Return<void> getAllowedNetworkTypesBitmapResponse(
    const RadioResponseInfo_V1_6& info,
    hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap);

  Return<void> setDataThrottlingResponse(const RadioResponseInfo_V1_6& info);

  Return<void> getSystemSelectionChannelsResponse(
    const RadioResponseInfo_V1_6& info,
    const hidl_vec<RadioAccessSpecifier_V1_5>& specifiers);

  Return<void> getCellInfoListResponse_1_6(
    const RadioResponseInfo_V1_6& info,
    const hidl_vec<CellInfo_V1_6>& cellInfo);

  Return<void> getSlicingConfigResponse(
    const RadioResponseInfo_V1_6& info,
    const SlicingConfig_V1_6& slicingConfig);

  Return<void> getSimPhonebookRecordsResponse(
    const RadioResponseInfo_V1_6& info);

  Return<void> getSimPhonebookCapacityResponse(
    const RadioResponseInfo_V1_6& info,
    const PhonebookCapacity_V1_6& capacity);

  Return<void> updateSimPhonebookRecordsResponse(
    const RadioResponseInfo_V1_6& info,
    int32_t updatedRecordIndex);
#endif

private:
  void defaultResponse(const RadioResponseInfo_V1_0& rspInfo,
                       const nsString& rilmessageType);
#if ANDROID_VERSION >= 33
  void defaultResponse(const RadioResponseInfo_V1_6& rspInfo,
                       const nsString& rilmessageType);

  int32_t convertRadioErrorToNum(RadioError_V1_6 error);
#endif
  int32_t convertRadioErrorToNum(RadioError_V1_0 error);

  int32_t covertLastCallFailCause(LastCallFailCause_V1_0 cause);

  int32_t convertAppType(AppType_V1_0 type);

  int32_t convertAppState(AppState_V1_0 state);
#if ANDROID_VERSION >= 33
  int32_t convertPersoSubstate(PersoSubstate_V1_5 state);
#endif
  int32_t convertPersoSubstate(PersoSubstate_V1_0 state);

  int32_t convertPinState(PinState_V1_0 state);

  int32_t convertCardState(CardState_V1_0 state);

  int32_t convertRegState(RegState_V1_0 state);

  int32_t convertUusType(UusType_V1_0 type);

  int32_t convertUusDcs(UusDcs_V1_0 dcs);

  int32_t convertCallPresentation(CallPresentation_V1_0 state);

  int32_t convertCallState(CallState_V1_0 state);

  int32_t convertPreferredNetworkType(PreferredNetworkType_V1_0 type);

  int32_t convertOperatorState(OperatorStatus_V1_0 status);

  int32_t convertCallForwardState(CallForwardInfoStatus_V1_0 status);

  int32_t convertClipState(ClipStatus_V1_0 status);

  int32_t convertTtyMode(TtyMode_V1_0 mode);
#if ANDROID_VERSION >= 33
  int32_t convertHalNetworkTypeBitMask_V1_4(
    hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap);
#endif
  RefPtr<nsRilWorker> mRIL;
};
#endif
