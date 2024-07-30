/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilIndication_H
#define nsRilIndication_H

#if ANDROID_VERSION >= 33
#include <android/hardware/radio/1.6/IRadioIndication.h>
#else
#include <android/hardware/radio/1.1/IRadioIndication.h>
#endif
#include <nsISupportsImpl.h>
#include <nsTArray.h>

using namespace ::android::hardware::radio::V1_1;
using ::android::sp;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using CdmaCallWaiting_V1_0 = ::android::hardware::radio::V1_0::CdmaCallWaiting;
using CdmaInformationRecords_V1_0 =
  ::android::hardware::radio::V1_0::CdmaInformationRecords;
using CdmaOtaProvisionStatus_V1_0 =
  ::android::hardware::radio::V1_0::CdmaOtaProvisionStatus;
using CdmaSignalInfoRecord_V1_0 =
  ::android::hardware::radio::V1_0::CdmaSignalInfoRecord;
using CdmaSmsMessage_V1_0 = ::android::hardware::radio::V1_0::CdmaSmsMessage;
using CdmaSubscriptionSource_V1_0 =
  ::android::hardware::radio::V1_0::CdmaSubscriptionSource;
using CellInfo_V1_0 = ::android::hardware::radio::V1_0::CellInfo;
using HardwareConfig_V1_0 = ::android::hardware::radio::V1_0::HardwareConfig;
using HardwareConfigState_V1_0 =
  ::android::hardware::radio::V1_0::HardwareConfigState;
using HardwareConfigType_V1_0 =
  ::android::hardware::radio::V1_0::HardwareConfigType;
using LceDataInfo_V1_0 = ::android::hardware::radio::V1_0::LceDataInfo;
using PcoDataInfo_V1_0 = ::android::hardware::radio::V1_0::PcoDataInfo;
using PhoneRestrictedState_V1_0 =
  ::android::hardware::radio::V1_0::PhoneRestrictedState;
using RadioAccessFamily_V1_0 =
  ::android::hardware::radio::V1_0::RadioAccessFamily;
using RadioCapability_V1_0 = ::android::hardware::radio::V1_0::RadioCapability;
using RadioCapabilityPhase_V1_0 =
  ::android::hardware::radio::V1_0::RadioCapabilityPhase;
using RadioCapabilityStatus_V1_0 =
  ::android::hardware::radio::V1_0::RadioCapabilityStatus;
using RadioIndicationType_V1_0 =
  ::android::hardware::radio::V1_0::RadioIndicationType;
using RadioState_V1_0 = ::android::hardware::radio::V1_0::RadioState;
using RadioTechnology_V1_0 = ::android::hardware::radio::V1_0::RadioTechnology;
using SetupDataCallResult_V1_0 =
  ::android::hardware::radio::V1_0::SetupDataCallResult;
using SignalStrength_V1_0 = ::android::hardware::radio::V1_0::SignalStrength;
using SimRefreshResult_V1_0 =
  ::android::hardware::radio::V1_0::SimRefreshResult;
using SimRefreshType_V1_0 = ::android::hardware::radio::V1_0::SimRefreshType;
using SrvccState_V1_0 = ::android::hardware::radio::V1_0::SrvccState;
using StkCcUnsolSsResult_V1_0 =
  ::android::hardware::radio::V1_0::StkCcUnsolSsResult;
using SuppSvcNotification_V1_0 =
  ::android::hardware::radio::V1_0::SuppSvcNotification;
using UssdModeType_V1_0 = ::android::hardware::radio::V1_0::UssdModeType;
using NetworkScanResult_V1_1 =
  ::android::hardware::radio::V1_1::NetworkScanResult;
using KeepaliveStatus_V1_1 = ::android::hardware::radio::V1_1::KeepaliveStatus;
using IRadioIndication_V1_1 = ::android::hardware::radio::V1_1::IRadioIndication;

#if ANDROID_VERSION >= 33
// Switch IradioIndicaiton Verison to v1.2;
using IRadioIndication_V1_2 =
  ::android::hardware::radio::V1_2::IRadioIndication;
using NetworkScanResult_V1_2 =
  ::android::hardware::radio::V1_2::NetworkScanResult;
using CellInfo_V1_2 = ::android::hardware::radio::V1_2::CellInfo;
using LinkCapacityEstimate_V1_2 =
  ::android::hardware::radio::V1_2::LinkCapacityEstimate;
using PhysicalChannelConfig_V1_2 =
  ::android::hardware::radio::V1_2::PhysicalChannelConfig;
using SignalStrength_V1_2 = ::android::hardware::radio::V1_2::SignalStrength;
// No update for v1.3
using IRadioIndication_V1_3 =
  ::android::hardware::radio::V1_3::IRadioIndication;
// For Iradioindication v1.4
using EmergencyNumber_V1_4 = ::android::hardware::radio::V1_4::EmergencyNumber;
using IRadioIndication_V1_4 =
  ::android::hardware::radio::V1_4::IRadioIndication;
using PhysicalChannelConfig_V1_4 =
  ::android::hardware::radio::V1_4::PhysicalChannelConfig;
using SignalStrength_V1_4 = ::android::hardware::radio::V1_4::SignalStrength;
using SetupDataCallResult_V1_4 =
  ::android::hardware::radio::V1_4::SetupDataCallResult;
using CellInfo_V1_4 = ::android::hardware::radio::V1_4::CellInfo;
using NetworkScanResult_V1_4 =
  ::android::hardware::radio::V1_4::NetworkScanResult;
using RadioTechnology_V1_4 = ::android::hardware::radio::V1_4::RadioTechnology;

// For Iradioindication v1.5 and v1.6
using SetupDataCallResult_V1_5 =
  ::android::hardware::radio::V1_5::SetupDataCallResult;
using CellIdentity_V1_5 = ::android::hardware::radio::V1_5::CellIdentity;
using NetworkScanResult_V1_5 =
  ::android::hardware::radio::V1_5::NetworkScanResult;
using BarringInfo_V1_5 = ::android::hardware::radio::V1_5::BarringInfo;
using Domain_V1_5 = ::android::hardware::radio::V1_5::Domain;
using CellInfo_V1_5 = ::android::hardware::radio::V1_5::CellInfo;

using IRadioIndication_V1_6 =
  ::android::hardware::radio::V1_6::IRadioIndication;
using SetupDataCallResult_V1_6 =
  ::android::hardware::radio::V1_6::SetupDataCallResult;
using NetworkScanResult_V1_6 =
  ::android::hardware::radio::V1_6::NetworkScanResult;
using CellInfo_V1_6 = ::android::hardware::radio::V1_6::CellInfo;
using LinkCapacityEstimate_V1_6 =
  ::android::hardware::radio::V1_6::LinkCapacityEstimate;
using SignalStrength_V1_6 = ::android::hardware::radio::V1_6::SignalStrength;
using PhysicalChannelConfig_V1_6 =
  ::android::hardware::radio::V1_6::PhysicalChannelConfig;
using PbReceivedStatus_V1_6 =
  ::android::hardware::radio::V1_6::PbReceivedStatus;
using PhonebookRecordInfo_V1_6 =
  ::android::hardware::radio::V1_6::PhonebookRecordInfo;
#endif

class nsRilWorker;

#if ANDROID_VERSION >= 33
class nsRilIndication : public IRadioIndication_V1_6
#else
class nsRilIndication : public IRadioIndication
#endif
{
public:
  RefPtr<nsRilWorker> mRIL;
  explicit nsRilIndication(nsRilWorker* aRil);
  ~nsRilIndication();

  Return<void> radioStateChanged(RadioIndicationType_V1_0 type,
                                 RadioState_V1_0 radioState);

  Return<void> callStateChanged(RadioIndicationType_V1_0 type);

  Return<void> networkStateChanged(RadioIndicationType_V1_0 type);

  Return<void> newSms(RadioIndicationType_V1_0 type,
                      const hidl_vec<uint8_t>& pdu);

  Return<void> newSmsStatusReport(RadioIndicationType_V1_0 type,
                                  const hidl_vec<uint8_t>& pdu);

  Return<void> newSmsOnSim(RadioIndicationType_V1_0 type, int32_t recordNumber);

  Return<void> onUssd(RadioIndicationType_V1_0 type,
                      UssdModeType_V1_0 modeType,
                      const hidl_string& msg);

  Return<void> nitzTimeReceived(RadioIndicationType_V1_0 type,
                                const hidl_string& nitzTime,
                                uint64_t receivedTime);

  Return<void> currentSignalStrength(RadioIndicationType_V1_0 type,
                                     const SignalStrength_V1_0& sig_strength);

  Return<void> dataCallListChanged(
    RadioIndicationType_V1_0 type,
    const hidl_vec<SetupDataCallResult_V1_0>& dcList);

  Return<void> suppSvcNotify(RadioIndicationType_V1_0 type,
                             const SuppSvcNotification_V1_0& suppSvc);

  Return<void> stkSessionEnd(RadioIndicationType_V1_0 type);

  Return<void> stkProactiveCommand(RadioIndicationType_V1_0 type,
                                   const hidl_string& cmd);

  Return<void> stkEventNotify(RadioIndicationType_V1_0 type,
                              const hidl_string& cmd);

  Return<void> stkCallSetup(RadioIndicationType_V1_0 type, int64_t timeout);

  Return<void> simSmsStorageFull(RadioIndicationType_V1_0 type);

  Return<void> simRefresh(RadioIndicationType_V1_0 type,
                          const SimRefreshResult_V1_0& refreshResult);

  Return<void> callRing(RadioIndicationType_V1_0 type,
                        bool isGsm,
                        const CdmaSignalInfoRecord_V1_0& record);

  Return<void> simStatusChanged(RadioIndicationType_V1_0 type);

  Return<void> cdmaNewSms(RadioIndicationType_V1_0 type,
                          const CdmaSmsMessage_V1_0& msg);

  Return<void> newBroadcastSms(
    RadioIndicationType_V1_0 type,
    const ::android::hardware::hidl_vec<uint8_t>& data);

  Return<void> cdmaRuimSmsStorageFull(RadioIndicationType_V1_0 type);

  Return<void> restrictedStateChanged(RadioIndicationType_V1_0 type,
                                      PhoneRestrictedState_V1_0 state);

  Return<void> enterEmergencyCallbackMode(RadioIndicationType_V1_0 type);

  Return<void> cdmaCallWaiting(RadioIndicationType_V1_0 type,
                               const CdmaCallWaiting_V1_0& callWaitingRecord);

  Return<void> cdmaOtaProvisionStatus(RadioIndicationType_V1_0 type,
                                      CdmaOtaProvisionStatus_V1_0 status);

  Return<void> cdmaInfoRec(RadioIndicationType_V1_0 type,
                           const CdmaInformationRecords_V1_0& records);

  Return<void> indicateRingbackTone(RadioIndicationType_V1_0 type, bool start);

  Return<void> resendIncallMute(RadioIndicationType_V1_0 type);

  Return<void> cdmaSubscriptionSourceChanged(
    RadioIndicationType_V1_0 type,
    CdmaSubscriptionSource_V1_0 cdmaSource);

  Return<void> cdmaPrlChanged(RadioIndicationType_V1_0 type, int32_t version);

  Return<void> exitEmergencyCallbackMode(RadioIndicationType_V1_0 type);

  Return<void> rilConnected(RadioIndicationType_V1_0 type);

  Return<void> voiceRadioTechChanged(RadioIndicationType_V1_0 type,
                                     RadioTechnology_V1_0 rat);

  Return<void> cellInfoList(RadioIndicationType_V1_0 type,
                            const hidl_vec<CellInfo_V1_0>& records);

  Return<void> imsNetworkStateChanged(RadioIndicationType_V1_0 type);

  Return<void> subscriptionStatusChanged(RadioIndicationType_V1_0 type,
                                         bool activate);

  Return<void> srvccStateNotify(RadioIndicationType_V1_0 type,
                                SrvccState_V1_0 state);

  Return<void> hardwareConfigChanged(
    RadioIndicationType_V1_0 type,
    const ::android::hardware::hidl_vec<HardwareConfig_V1_0>& configs);

  Return<void> radioCapabilityIndication(RadioIndicationType_V1_0 type,
                                         const RadioCapability_V1_0& rc);

  Return<void> onSupplementaryServiceIndication(
    RadioIndicationType_V1_0 type,
    const StkCcUnsolSsResult_V1_0& ss);

  Return<void> stkCallControlAlphaNotify(RadioIndicationType_V1_0 type,
                                         const hidl_string& alpha);

  Return<void> lceData(RadioIndicationType_V1_0 type,
                       const LceDataInfo_V1_0& lce);

  Return<void> pcoData(RadioIndicationType_V1_0 type,
                       const PcoDataInfo_V1_0& pco);

  Return<void> modemReset(RadioIndicationType_V1_0 type,
                          const hidl_string& reason);

  // Radio Indication v1.1
  Return<void> carrierInfoForImsiEncryption(RadioIndicationType_V1_0 type);

  Return<void> networkScanResult(RadioIndicationType_V1_0 type,
                                 const NetworkScanResult_V1_1& result);

  Return<void> keepaliveStatus(RadioIndicationType_V1_0 type,
                               const KeepaliveStatus_V1_1& status);

#if ANDROID_VERSION >= 33
  // Radio Indication v1.2
  Return<void> networkScanResult_1_2(RadioIndicationType_V1_0 type,
                                     const NetworkScanResult_V1_2& result);

  Return<void> cellInfoList_1_2(RadioIndicationType_V1_0 type,
                                const hidl_vec<CellInfo_V1_2>& records);

  Return<void> currentLinkCapacityEstimate(
    RadioIndicationType_V1_0 type,
    const LinkCapacityEstimate_V1_2& lce);

  Return<void> currentPhysicalChannelConfigs(
    RadioIndicationType_V1_0 type,
    const hidl_vec<PhysicalChannelConfig_V1_2>& configs);

  Return<void> currentSignalStrength_1_2(
    RadioIndicationType_V1_0 type,
    const SignalStrength_V1_2& sig_strength);

  // Radio indication v1.4
  Return<void> currentEmergencyNumberList(
    RadioIndicationType_V1_0 type,
    const hidl_vec<EmergencyNumber_V1_4>& emergencyNumberList);
  Return<void> currentPhysicalChannelConfigs_1_4(
    RadioIndicationType_V1_0 type,
    const hidl_vec<PhysicalChannelConfig_V1_4>& configs);
  Return<void> currentSignalStrength_1_4(
    RadioIndicationType_V1_0 type,
    const SignalStrength_V1_4& signalStrength);
  Return<void> dataCallListChanged_1_4(
    RadioIndicationType_V1_0 type,
    const hidl_vec<SetupDataCallResult_V1_4>& dcList);
  Return<void> cellInfoList_1_4(RadioIndicationType_V1_0 type,
                                const hidl_vec<CellInfo_V1_4>& records);
  Return<void> networkScanResult_1_4(RadioIndicationType_V1_0 type,
                                     const NetworkScanResult_V1_4& result);
  // Radio indication V1.5
  Return<void> uiccApplicationsEnablementChanged(RadioIndicationType_V1_0 type,
                                                 bool enabled);
  Return<void> registrationFailed(RadioIndicationType_V1_0 type,
                                  const CellIdentity_V1_5& cellIdentity,
                                  const hidl_string& chosenPlmn,
                                  hidl_bitfield<Domain_V1_5> domain,
                                  int32_t causeCode,
                                  int32_t additionalCauseCode);
  Return<void> barringInfoChanged(
    RadioIndicationType_V1_0 type,
    const CellIdentity_V1_5& cellIdentity,
    const hidl_vec<BarringInfo_V1_5>& barringInfos);
  Return<void> cellInfoList_1_5(RadioIndicationType_V1_0 type,
                                const hidl_vec<CellInfo_V1_5>& records);
  Return<void> networkScanResult_1_5(RadioIndicationType_V1_0 type,
                                     const NetworkScanResult_V1_5& result);
  Return<void> dataCallListChanged_1_5(
    RadioIndicationType_V1_0 type,
    const hidl_vec<SetupDataCallResult_V1_5>& dcList);

  // Radio indication V1.6
  Return<void> dataCallListChanged_1_6(
    RadioIndicationType_V1_0 type,
    const hidl_vec<SetupDataCallResult_V1_6>& dcList);
  Return<void> unthrottleApn(RadioIndicationType_V1_0 type,
                             const hidl_string& apn);
  Return<void> currentLinkCapacityEstimate_1_6(
    RadioIndicationType_V1_0 type,
    const LinkCapacityEstimate_V1_6& lce);
  Return<void> currentSignalStrength_1_6(
    RadioIndicationType_V1_0 type,
    const SignalStrength_V1_6& signalStrength);
  Return<void> cellInfoList_1_6(RadioIndicationType_V1_0 type,
                                const hidl_vec<CellInfo_V1_6>& records);
  Return<void> networkScanResult_1_6(RadioIndicationType_V1_0 type,
                                     const NetworkScanResult_V1_6& result);
  Return<void> currentPhysicalChannelConfigs_1_6(
    RadioIndicationType_V1_0 type,
    const hidl_vec<PhysicalChannelConfig_V1_6>& configs);
  Return<void> simPhonebookChanged(RadioIndicationType_V1_0 type);
  Return<void> simPhonebookRecordsReceived(
    RadioIndicationType_V1_0 type,
    PbReceivedStatus_V1_6 status,
    const hidl_vec<PhonebookRecordInfo_V1_6>& records);
#endif

private:
  void defaultResponse(const RadioIndicationType_V1_0 type,
                       const nsString& rilmessageType);
  int32_t convertRadioStateToNum(RadioState_V1_0 state);
  int32_t convertSimRefreshType(SimRefreshType_V1_0 type);
  int32_t convertPhoneRestrictedState(PhoneRestrictedState_V1_0 state);
  int32_t convertUssdModeType(UssdModeType_V1_0 type);
  int32_t convertSrvccState(SrvccState_V1_0 state);
  int32_t convertHardwareConfigType(HardwareConfigType_V1_0 state);
  int32_t convertHardwareConfigState(HardwareConfigState_V1_0 state);
  int32_t convertRadioCapabilityPhase(RadioCapabilityPhase_V1_0 value);
  int32_t convertRadioAccessFamily(RadioAccessFamily_V1_0 value);
  int32_t convertRadioCapabilityStatus(RadioCapabilityStatus_V1_0 state);
};

#endif
