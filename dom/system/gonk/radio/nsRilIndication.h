/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilIndication_H
#define nsRilIndication_H
#include <nsISupportsImpl.h>
#include <nsTArray.h>
#include <android/hardware/radio/1.1/IRadioIndication.h>

using namespace ::android::hardware::radio::V1_1;
using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::radio::V1_0::CdmaCallWaiting;
using ::android::hardware::radio::V1_0::CdmaInformationRecords;
using ::android::hardware::radio::V1_0::CdmaOtaProvisionStatus;
using ::android::hardware::radio::V1_0::CdmaSignalInfoRecord;
using ::android::hardware::radio::V1_0::CdmaSmsMessage;
using ::android::hardware::radio::V1_0::CdmaSubscriptionSource;
using ::android::hardware::radio::V1_0::CellInfo;
using ::android::hardware::radio::V1_0::HardwareConfig;
using ::android::hardware::radio::V1_0::HardwareConfigState;
using ::android::hardware::radio::V1_0::HardwareConfigType;
using ::android::hardware::radio::V1_0::LceDataInfo;
using ::android::hardware::radio::V1_0::PcoDataInfo;
using ::android::hardware::radio::V1_0::PhoneRestrictedState;
using ::android::hardware::radio::V1_0::RadioAccessFamily;
using ::android::hardware::radio::V1_0::RadioCapability;
using ::android::hardware::radio::V1_0::RadioCapabilityPhase;
using ::android::hardware::radio::V1_0::RadioCapabilityStatus;
using ::android::hardware::radio::V1_0::RadioIndicationType;
using ::android::hardware::radio::V1_0::RadioState;
using ::android::hardware::radio::V1_0::RadioTechnology;
using ::android::hardware::radio::V1_0::SetupDataCallResult;
using ::android::hardware::radio::V1_0::SignalStrength;
using ::android::hardware::radio::V1_0::SimRefreshResult;
using ::android::hardware::radio::V1_0::SimRefreshType;
using ::android::hardware::radio::V1_0::SrvccState;
using ::android::hardware::radio::V1_0::StkCcUnsolSsResult;
using ::android::hardware::radio::V1_0::SuppSvcNotification;
using ::android::hardware::radio::V1_0::UssdModeType;
using ::android::hardware::radio::V1_1::IRadioIndication;

class nsRilWorker;

class nsRilIndication : public IRadioIndication {
 public:
  RefPtr<nsRilWorker> mRIL;
  explicit nsRilIndication(nsRilWorker* aRil);
  ~nsRilIndication();

  Return<void> radioStateChanged(RadioIndicationType type,
                                 RadioState radioState);

  Return<void> callStateChanged(RadioIndicationType type);

  Return<void> networkStateChanged(RadioIndicationType type);

  Return<void> newSms(RadioIndicationType type,
                      const ::android::hardware::hidl_vec<uint8_t>& pdu);

  Return<void> newSmsStatusReport(
      RadioIndicationType type,
      const ::android::hardware::hidl_vec<uint8_t>& pdu);

  Return<void> newSmsOnSim(RadioIndicationType type, int32_t recordNumber);

  Return<void> onUssd(RadioIndicationType type, UssdModeType modeType,
                      const ::android::hardware::hidl_string& msg);

  Return<void> nitzTimeReceived(
      RadioIndicationType type,
      const ::android::hardware::hidl_string& nitzTime, uint64_t receivedTime);

  Return<void> currentSignalStrength(RadioIndicationType type,
                                     const SignalStrength& sig_strength);

  Return<void> dataCallListChanged(
      RadioIndicationType type,
      const ::android::hardware::hidl_vec<SetupDataCallResult>& dcList);

  Return<void> suppSvcNotify(RadioIndicationType type,
                             const SuppSvcNotification& suppSvc);

  Return<void> stkSessionEnd(RadioIndicationType type);

  Return<void> stkProactiveCommand(RadioIndicationType type,
                                   const ::android::hardware::hidl_string& cmd);

  Return<void> stkEventNotify(RadioIndicationType type,
                              const ::android::hardware::hidl_string& cmd);

  Return<void> stkCallSetup(RadioIndicationType type, int64_t timeout);

  Return<void> simSmsStorageFull(RadioIndicationType type);

  Return<void> simRefresh(RadioIndicationType type,
                          const SimRefreshResult& refreshResult);

  Return<void> callRing(RadioIndicationType type, bool isGsm,
                        const CdmaSignalInfoRecord& record);

  Return<void> simStatusChanged(RadioIndicationType type);

  Return<void> cdmaNewSms(RadioIndicationType type, const CdmaSmsMessage& msg);

  Return<void> newBroadcastSms(
      RadioIndicationType type,
      const ::android::hardware::hidl_vec<uint8_t>& data);

  Return<void> cdmaRuimSmsStorageFull(RadioIndicationType type);

  Return<void> restrictedStateChanged(RadioIndicationType type,
                                      PhoneRestrictedState state);

  Return<void> enterEmergencyCallbackMode(RadioIndicationType type);

  Return<void> cdmaCallWaiting(RadioIndicationType type,
                               const CdmaCallWaiting& callWaitingRecord);

  Return<void> cdmaOtaProvisionStatus(RadioIndicationType type,
                                      CdmaOtaProvisionStatus status);

  Return<void> cdmaInfoRec(RadioIndicationType type,
                           const CdmaInformationRecords& records);

  Return<void> indicateRingbackTone(RadioIndicationType type, bool start);

  Return<void> resendIncallMute(RadioIndicationType type);

  Return<void> cdmaSubscriptionSourceChanged(RadioIndicationType type,
                                             CdmaSubscriptionSource cdmaSource);

  Return<void> cdmaPrlChanged(RadioIndicationType type, int32_t version);

  Return<void> exitEmergencyCallbackMode(RadioIndicationType type);

  Return<void> rilConnected(RadioIndicationType type);

  Return<void> voiceRadioTechChanged(RadioIndicationType type,
                                     RadioTechnology rat);

  Return<void> cellInfoList(
      RadioIndicationType type,
      const ::android::hardware::hidl_vec<CellInfo>& records);

  Return<void> imsNetworkStateChanged(RadioIndicationType type);

  Return<void> subscriptionStatusChanged(RadioIndicationType type,
                                         bool activate);

  Return<void> srvccStateNotify(RadioIndicationType type, SrvccState state);

  Return<void> hardwareConfigChanged(
      RadioIndicationType type,
      const ::android::hardware::hidl_vec<HardwareConfig>& configs);

  Return<void> radioCapabilityIndication(RadioIndicationType type,
                                         const RadioCapability& rc);

  Return<void> onSupplementaryServiceIndication(RadioIndicationType type,
                                                const StkCcUnsolSsResult& ss);

  Return<void> stkCallControlAlphaNotify(
      RadioIndicationType type, const ::android::hardware::hidl_string& alpha);

  Return<void> lceData(RadioIndicationType type, const LceDataInfo& lce);

  Return<void> pcoData(RadioIndicationType type, const PcoDataInfo& pco);

  Return<void> modemReset(RadioIndicationType type,
                          const ::android::hardware::hidl_string& reason);

  Return<void> carrierInfoForImsiEncryption(RadioIndicationType type);

  Return<void> networkScanResult(RadioIndicationType type,
                                 const NetworkScanResult& result);

  Return<void> keepaliveStatus(RadioIndicationType type,
                               const KeepaliveStatus& status);

 private:
  void defaultResponse(const RadioIndicationType type,
                       const nsString& rilmessageType);
  int32_t convertRadioStateToNum(RadioState state);
  int32_t convertSimRefreshType(SimRefreshType type);
  int32_t convertPhoneRestrictedState(PhoneRestrictedState state);
  int32_t convertUssdModeType(UssdModeType type);
  int32_t convertSrvccState(SrvccState state);
  int32_t convertHardwareConfigType(HardwareConfigType state);
  int32_t convertHardwareConfigState(HardwareConfigState state);

  int32_t convertRadioCapabilityPhase(RadioCapabilityPhase value);
  int32_t convertRadioAccessFamily(RadioAccessFamily value);
  int32_t convertRadioCapabilityStatus(RadioCapabilityStatus state);
};

#endif
