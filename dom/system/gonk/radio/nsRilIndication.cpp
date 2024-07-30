/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilIndication.h"
#include "nsRilWorker.h"
#include <cstdint>

/* Logging related */
#undef LOG_TAG
#define LOG_TAG "RilIndication"

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ##args)
#define ERROR(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ##args)
#define DEBUG(args...)                                                         \
  do {                                                                         \
    if (gRilDebug_isLoggingEnabled) {                                          \
      __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ##args);                 \
    }                                                                          \
  } while (0)

/**
 *
 */
nsRilIndication::nsRilIndication(nsRilWorker* aRil)
{
  DEBUG("init nsRilIndication");
  mRIL = aRil;
  DEBUG("init nsRilIndication done");
}

nsRilIndication::~nsRilIndication()
{
  DEBUG("Destructor nsRilIndication");
  mRIL = nullptr;
  MOZ_ASSERT(!mRIL);
}

Return<void>
nsRilIndication::radioStateChanged(RadioIndicationType_V1_0 type,
                                   RadioState_V1_0 radioState)
{
  DEBUG("radioStateChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"radiostatechange"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateRadioStateChanged(convertRadioStateToNum(radioState));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::callStateChanged(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"callStateChanged"_ns);
  return Void();
}

Return<void>
nsRilIndication::networkStateChanged(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"networkStateChanged"_ns);
  return Void();
}

Return<void>
nsRilIndication::newSms(RadioIndicationType_V1_0 type,
                        const ::android::hardware::hidl_vec<uint8_t>& pdu)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"sms-received"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = pdu.size();

  nsTArray<int32_t> pdu_u32;
  for (uint32_t i = 0; i < size; i++) {
    pdu_u32.AppendElement(pdu[i]);
  }
  result->updateOnSms(pdu_u32);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::newSmsStatusReport(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_vec<uint8_t>& pdu)
{
  nsString rilmessageType(u"smsstatusreport"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = pdu.size();

  nsTArray<int32_t> pdu_u32;
  for (uint32_t i = 0; i < size; i++) {
    pdu_u32.AppendElement(pdu[i]);
  }
  result->updateOnSms(pdu_u32);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::newSmsOnSim(RadioIndicationType_V1_0 type,
                             int32_t recordNumber)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"smsOnSim"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateNewSmsOnSim(recordNumber);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::onUssd(RadioIndicationType_V1_0 type,
                        UssdModeType_V1_0 modeType,
                        const ::android::hardware::hidl_string& msg)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"ussdreceived"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateOnUssd(convertUssdModeType(modeType),
                       NS_ConvertUTF8toUTF16(msg.c_str()));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::nitzTimeReceived(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_string& nitzTime,
  uint64_t receivedTime)
{
  DEBUG("nitzTimeReceived");
  mRIL->processIndication(type);

  nsString rilmessageType(u"nitzTimeReceived"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateNitzTimeReceived(NS_ConvertUTF8toUTF16(nitzTime.c_str()),
                                 receivedTime);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::currentSignalStrength(RadioIndicationType_V1_0 type,
                                       const SignalStrength_V1_0& sig_strength)
{
  DEBUG("currentSignalStrength");
  mRIL->processIndication(type);

  nsString rilmessageType(u"signalstrengthchange"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSignalStrength> signalStrength =
    result->convertSignalStrength(sig_strength);
  result->updateCurrentSignalStrength(signalStrength);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::dataCallListChanged(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_vec<SetupDataCallResult_V1_0>& dcList)
{
  DEBUG("dataCallListChanged");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"datacalllistchanged"_ns);
  uint32_t numDataCall = dcList.size();
  DEBUG("getDataCallListResponse numDataCall= %d", numDataCall);
  nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

  for (uint32_t i = 0; i < numDataCall; i++) {
    RefPtr<nsSetupDataCallResult> datcall =
      result->convertDcResponse(dcList[i]);
    aDcLists.AppendElement(datcall);
  }
  result->updateDataCallListChanged(aDcLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::suppSvcNotify(RadioIndicationType_V1_0 type,
                               const SuppSvcNotification_V1_0& suppSvc)
{
  DEBUG("suppSvcNotification");
  mRIL->processIndication(type);

  nsString rilmessageType(u"suppSvcNotification"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSuppSvcNotification> notify =
    new nsSuppSvcNotification(suppSvc.isMT,
                              suppSvc.code,
                              suppSvc.index,
                              suppSvc.type,
                              NS_ConvertUTF8toUTF16(suppSvc.number.c_str()));
  result->updateSuppSvcNotify(notify);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::stkSessionEnd(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"stksessionend"_ns);
  return Void();
}

Return<void>
nsRilIndication::stkProactiveCommand(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_string& cmd)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"stkProactiveCommand"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateStkProactiveCommand(NS_ConvertUTF8toUTF16(cmd.c_str()));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::stkEventNotify(RadioIndicationType_V1_0 type,
                                const ::android::hardware::hidl_string& cmd)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"stkEventNotify"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateStkEventNotify(NS_ConvertUTF8toUTF16(cmd.c_str()));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void> nsRilIndication::stkCallSetup(RadioIndicationType_V1_0 /*type*/,
                                           int64_t /*timeout*/)
{
  DEBUG("Not implement stkCallSetup");
  return Void();
}

Return<void>
nsRilIndication::simSmsStorageFull(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"simSmsStorageFull"_ns);
  return Void();
}

Return<void>
nsRilIndication::simRefresh(RadioIndicationType_V1_0 type,
                            const SimRefreshResult_V1_0& refreshResult)
{
  DEBUG("simRefresh");
  mRIL->processIndication(type);

  nsString rilmessageType(u"simRefresh"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSimRefreshResult> simRefresh =
    new nsSimRefreshResult(convertSimRefreshType(refreshResult.type),
                           refreshResult.efId,
                           NS_ConvertUTF8toUTF16(refreshResult.aid.c_str()));
  result->updateSimRefresh(simRefresh);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::callRing(RadioIndicationType_V1_0 type,
                          bool isGsm,
                          const CdmaSignalInfoRecord_V1_0& record)
{
  mRIL->processIndication(type);

  // TODO impelment CDMA later.

  nsString rilmessageType(u"callRing"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::simStatusChanged(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"simStatusChanged"_ns);
  return Void();
}

Return<void>
nsRilIndication::cdmaNewSms(RadioIndicationType_V1_0 /*type*/,
                            const CdmaSmsMessage_V1_0& /*msg*/)
{
  DEBUG("Not implement cdmaNewSms");
  return Void();
}

Return<void>
nsRilIndication::newBroadcastSms(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_vec<uint8_t>& data)
{
  mRIL->processIndication(type);

  nsString rilmessageType(u"cellbroadcast-received"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = data.size();

  nsTArray<int32_t> aData;
  for (uint32_t i = 0; i < size; i++) {
    aData.AppendElement(data[i]);
  }
  result->updateNewBroadcastSms(aData);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void> nsRilIndication::cdmaRuimSmsStorageFull(
  RadioIndicationType_V1_0 /*type*/)
{
  DEBUG("Not implement cdmaRuimSmsStorageFull");
  return Void();
}

Return<void>
nsRilIndication::restrictedStateChanged(RadioIndicationType_V1_0 type,
                                        PhoneRestrictedState_V1_0 state)
{
  DEBUG("restrictedStateChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"restrictedStateChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateRestrictedStateChanged(convertPhoneRestrictedState(state));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::enterEmergencyCallbackMode(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"enterEmergencyCbMode"_ns);
  return Void();
}

Return<void>
nsRilIndication::cdmaCallWaiting(
  RadioIndicationType_V1_0 /*type*/,
  const CdmaCallWaiting_V1_0& /*callWaitingRecord*/)
{
  DEBUG("Not implement cdmaCallWaiting");
  return Void();
}

Return<void> nsRilIndication::cdmaOtaProvisionStatus(
  RadioIndicationType_V1_0 /*type*/,
  CdmaOtaProvisionStatus_V1_0 /*status*/)
{
  DEBUG("Not implement cdmaOtaProvisionStatus");
  return Void();
}

Return<void>
nsRilIndication::cdmaInfoRec(RadioIndicationType_V1_0 /*type*/,
                             const CdmaInformationRecords_V1_0& /*records*/)
{
  DEBUG("Not implement cdmaInfoRec");
  return Void();
}

Return<void>
nsRilIndication::indicateRingbackTone(RadioIndicationType_V1_0 type, bool start)
{
  DEBUG("ringbackTone");
  mRIL->processIndication(type);

  nsString rilmessageType(u"ringbackTone"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateIndicateRingbackTone(start);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::resendIncallMute(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"resendIncallMute"_ns);
  return Void();
}

Return<void> nsRilIndication::cdmaSubscriptionSourceChanged(
  RadioIndicationType_V1_0 /*type*/,
  CdmaSubscriptionSource_V1_0 /*cdmaSource*/)
{
  DEBUG("Not implement cdmaSubscriptionSourceChanged");
  return Void();
}

Return<void> nsRilIndication::cdmaPrlChanged(RadioIndicationType_V1_0 /*type*/,
                                             int32_t /*version*/)
{
  DEBUG("Not implement cdmaPrlChanged");
  return Void();
}

Return<void>
nsRilIndication::exitEmergencyCallbackMode(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"exitEmergencyCbMode"_ns);
  return Void();
}

Return<void>
nsRilIndication::rilConnected(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"rilconnected"_ns);
  return Void();
}

Return<void>
nsRilIndication::voiceRadioTechChanged(RadioIndicationType_V1_0 type,
                                       RadioTechnology_V1_0 rat)
{
  DEBUG("voiceRadioTechChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"voiceRadioTechChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateVoiceRadioTechChanged((int32_t)rat);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::cellInfoList(RadioIndicationType_V1_0 type,
                              const hidl_vec<CellInfo_V1_0>& records)
{
  DEBUG("cellInfoList");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo = result->convertRilCellInfo(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::keepaliveStatus(RadioIndicationType_V1_0 /*type*/,
                                 const KeepaliveStatus_V1_1& /*status*/)
{
  DEBUG("Not implement keepaliveStatus");
  return Void();
}

#if ANDROID_VERSION >= 33
Return<void>
nsRilIndication::networkScanResult_1_2(RadioIndicationType_V1_0 type,
                                       const NetworkScanResult_V1_2& result)
{
  DEBUG("Not implement networkScanResult_1_2");
  return Void();
}

Return<void>
nsRilIndication::cellInfoList_1_2(RadioIndicationType_V1_0 type,
                                  const hidl_vec<CellInfo_V1_2>& records)
{
  DEBUG("cellInfoList_1_2");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList_1_2 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_2(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentLinkCapacityEstimate(
  RadioIndicationType_V1_0 type,
  const LinkCapacityEstimate_V1_2& lce)
{
  DEBUG("currentLinkCapacityEstimate");
  mRIL->processIndication(type);

  nsString rilmessageType(u"currentLinkCapacityEstimate"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsLinkCapacityEstimate> linkCapacityEstimate =
    new nsLinkCapacityEstimate(lce.downlinkCapacityKbps,
                               lce.uplinkCapacityKbps);
  result->updateCurrentLinkCapacityEstimate(linkCapacityEstimate);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentPhysicalChannelConfigs(
  RadioIndicationType_V1_0 type,
  const hidl_vec<PhysicalChannelConfig_V1_2>& configs)
{
  DEBUG("currentPhysicalChannelConfigs");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"currentPhysicalChannelConfigs"_ns);
  uint32_t numConfig = configs.size();
  DEBUG("currentPhysicalChannelConfigs numConfig= %d", numConfig);
  nsTArray<RefPtr<nsPhysicalChannelConfig>> aConfigs(numConfig);
  const nsTArray<int32_t> contextIds;
  for (uint32_t i = 0; i < numConfig; i++) {
    RefPtr<nsPhysicalChannelConfig> config = new nsPhysicalChannelConfig(
      nsRilResult::convertConnectionStatus(configs[i].status),
      configs[i].cellBandwidthDownlink,
      0,
      0,
      0,
      0,
      contextIds,
      0,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1);
    aConfigs.AppendElement(config);
  }
  result->updatePhysicalChannelConfig(aConfigs);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentSignalStrength_1_2(
  RadioIndicationType_V1_0 type,
  const SignalStrength_V1_2& sig_strength)
{
  DEBUG("currentSignalStrength_1_2");
  mRIL->processIndication(type);

  nsString rilmessageType(u"signalstrengthchange"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSignalStrength> signalStrength =
    result->convertSignalStrength_V1_2(sig_strength);
  result->updateCurrentSignalStrength(signalStrength);
  mRIL->sendRilIndicationResult(result);

  return Void();
}
#endif

Return<void>
nsRilIndication::imsNetworkStateChanged(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"imsNetworkStateChanged"_ns);
  return Void();
}

Return<void>
nsRilIndication::subscriptionStatusChanged(RadioIndicationType_V1_0 type,
                                           bool activate)
{
  DEBUG("subscriptionStatusChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"subscriptionStatusChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateSubscriptionStatusChanged(activate);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::srvccStateNotify(RadioIndicationType_V1_0 type,
                                  SrvccState_V1_0 state)
{
  DEBUG("srvccStateNotify");
  mRIL->processIndication(type);

  nsString rilmessageType(u"srvccStateNotify"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateSrvccStateNotify(convertSrvccState(state));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::hardwareConfigChanged(
  RadioIndicationType_V1_0 type,
  const ::android::hardware::hidl_vec<HardwareConfig_V1_0>& configs)
{
  DEBUG("hardwareConfigChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"hardwareConfigChanged"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);

  uint32_t numConfigs = configs.size();
  nsTArray<RefPtr<nsHardwareConfig>> aHWConfigLists(numConfigs);

  for (uint32_t i = 0; i < numConfigs; i++) {
    int32_t type = convertHardwareConfigType(configs[i].type);
    RefPtr<nsHardwareConfig> hwConfig = nullptr;
    RefPtr<nsHardwareConfigModem> hwConfigModem = nullptr;
    RefPtr<nsHardwareConfigSim> hwConfigSim = nullptr;

    if (type == nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM) {
      hwConfigModem = new nsHardwareConfigModem(configs[i].modem[0].rilModel,
                                                configs[i].modem[0].rat,
                                                configs[i].modem[0].maxVoice,
                                                configs[i].modem[0].maxData,
                                                configs[i].modem[0].maxStandby);
    } else {
      hwConfigSim = new nsHardwareConfigSim(
        NS_ConvertUTF8toUTF16(configs[i].sim[0].modemUuid.c_str()));
    }

    hwConfig =
      new nsHardwareConfig(type,
                           NS_ConvertUTF8toUTF16(configs[i].uuid.c_str()),
                           convertHardwareConfigState(configs[i].state),
                           hwConfigModem,
                           hwConfigSim);
    aHWConfigLists.AppendElement(hwConfig);
  }
  result->updateHardwareConfigChanged(aHWConfigLists);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::radioCapabilityIndication(RadioIndicationType_V1_0 type,
                                           const RadioCapability_V1_0& rc)
{
  DEBUG("radioCapabilityIndication");
  mRIL->processIndication(type);

  nsString rilmessageType(u"radioCapabilityIndication"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsRadioCapability> capability = new nsRadioCapability(
    rc.session,
    convertRadioCapabilityPhase(rc.phase),
    convertRadioAccessFamily(RadioAccessFamily_V1_0(rc.raf)),
    NS_ConvertUTF8toUTF16(rc.logicalModemUuid.c_str()),
    convertRadioCapabilityStatus(rc.status));
  result->updateRadioCapabilityIndication(capability);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::onSupplementaryServiceIndication(
  RadioIndicationType_V1_0 /*type*/,
  const StkCcUnsolSsResult_V1_0& /*ss*/)
{
  DEBUG("Not implement onSupplementaryServiceIndication");
  return Void();
}

Return<void>
nsRilIndication::stkCallControlAlphaNotify(
  RadioIndicationType_V1_0 /*type*/,
  const ::android::hardware::hidl_string& /*alpha*/)
{
  DEBUG("Not implement stkCallControlAlphaNotify");
  return Void();
}

Return<void>
nsRilIndication::lceData(RadioIndicationType_V1_0 type,
                         const LceDataInfo_V1_0& lce)
{
  DEBUG("lceData");
  mRIL->processIndication(type);

  nsString rilmessageType(u"lceData"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsLceDataInfo> lceInfo = new nsLceDataInfo(
    lce.lastHopCapacityKbps, lce.confidenceLevel, lce.lceSuspended);
  result->updateLceData(lceInfo);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::pcoData(RadioIndicationType_V1_0 type,
                         const PcoDataInfo_V1_0& pco)
{
  DEBUG("pcoData");
  mRIL->processIndication(type);

  nsString rilmessageType(u"pcoData"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);

  uint32_t numContents = pco.contents.size();
  nsTArray<int32_t> pcoContents(numContents);
  for (uint32_t i = 0; i < numContents; i++) {
    pcoContents.AppendElement((int32_t)pco.contents[i]);
  }

  RefPtr<nsPcoDataInfo> pcoInfo =
    new nsPcoDataInfo(pco.cid,
                      NS_ConvertUTF8toUTF16(pco.bearerProto.c_str()),
                      pco.pcoId,
                      pcoContents);
  result->updatePcoData(pcoInfo);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::modemReset(RadioIndicationType_V1_0 type,
                            const ::android::hardware::hidl_string& reason)
{
  DEBUG("modemReset");
  mRIL->processIndication(type);

  nsString rilmessageType(u"modemReset"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateModemReset(NS_ConvertUTF8toUTF16(reason.c_str()));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void> nsRilIndication::carrierInfoForImsiEncryption(
  RadioIndicationType_V1_0 /*info*/)
{
  DEBUG("Not implement carrierInfoForImsiEncryption");
  return Void();
}

Return<void>
nsRilIndication::networkScanResult(RadioIndicationType_V1_0 type,
                                   const NetworkScanResult_V1_1& result)
{
  DEBUG("Not implement networkScanResult");
  return Void();
}

#if ANDROID_VERSION >= 33
Return<void>
nsRilIndication::currentEmergencyNumberList(
  RadioIndicationType_V1_0 type,
  const hidl_vec<EmergencyNumber_V1_4>& emergencyNumberList)
{
  DEBUG("currentEmergencyNumberList");
  mRIL->processIndication(type);
  nsString rilmessageType(u"currentEmergencyNumberList"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  nsTArray<RefPtr<nsEmergencyNumber>> list;
  for (uint32_t i = 0; i < emergencyNumberList.size(); i++) {
    nsTArray<nsString> urns;
    for (uint32_t j = 0; j < emergencyNumberList[i].urns.size(); j++) {
      nsString urn;
      urn.Assign(NS_ConvertUTF8toUTF16(emergencyNumberList[i].urns[j].c_str()));
      urns.AppendElement(urn);
    }
    RefPtr<nsEmergencyNumber> emergencyNumber = new nsEmergencyNumber(
      NS_ConvertUTF8toUTF16(emergencyNumberList[i].number.c_str()),
      NS_ConvertUTF8toUTF16(emergencyNumberList[i].mcc.c_str()),
      NS_ConvertUTF8toUTF16(emergencyNumberList[i].mnc.c_str()),
      emergencyNumberList[i].categories,
      urns,
      emergencyNumberList[i].sources);
    list.AppendElement(emergencyNumber);
  }
  result->updateCurrentEmergencyNumberList(list);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentPhysicalChannelConfigs_1_4(
  RadioIndicationType_V1_0 type,
  const hidl_vec<PhysicalChannelConfig_V1_4>& configs)
{
  DEBUG("currentPhysicalChannelConfigs_1_4");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"currentPhysicalChannelConfigs"_ns);
  uint32_t numConfig = configs.size();
  DEBUG("currentPhysicalChannelConfigs numConfig= %d", numConfig);
  nsTArray<RefPtr<nsPhysicalChannelConfig>> aConfigs(numConfig);
  for (uint32_t i = 0; i < numConfig; i++) {
    nsTArray<int32_t> contextIds;
    for (uint32_t k = 0; k < configs[i].contextIds.size(); k++) {
      contextIds.AppendElement(configs[i].contextIds[k]);
    }
    RefPtr<nsPhysicalChannelConfig> config = new nsPhysicalChannelConfig(
      nsRilResult::convertConnectionStatus(configs[i].base.status),
      configs[i].base.cellBandwidthDownlink,
      static_cast<int32_t>(configs[i].rat),
      static_cast<uint8_t>(configs[i].rfInfo.getDiscriminator()),
      static_cast<int32_t>(configs[i].rfInfo.range()),
      configs[i].rfInfo.channelNumber(),
      contextIds,
      configs[i].physicalCellId,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1);
    aConfigs.AppendElement(config);
  }
  result->updatePhysicalChannelConfig(aConfigs);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentSignalStrength_1_4(
  RadioIndicationType_V1_0 type,
  const SignalStrength_V1_4& aSignalStrength)
{
  DEBUG("currentSignalStrength_1_4");
  mRIL->processIndication(type);

  nsString rilmessageType(u"signalstrengthchange"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSignalStrength> signalStrength =
    result->convertSignalStrength_V1_4(aSignalStrength);
  result->updateCurrentSignalStrength(signalStrength);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::dataCallListChanged_1_4(
  RadioIndicationType_V1_0 type,
  const hidl_vec<SetupDataCallResult_V1_4>& dcList)
{
  DEBUG("dataCallListChanged_1_4");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"datacalllistchanged"_ns);
  uint32_t numDataCall = dcList.size();
  DEBUG("getDataCallListResponse numDataCall= %d", numDataCall);
  nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

  for (uint32_t i = 0; i < numDataCall; i++) {
    RefPtr<nsSetupDataCallResult> datcall =
      result->convertDcResponse_V1_4(dcList[i]);
    aDcLists.AppendElement(datcall);
  }
  result->updateDataCallListChanged(aDcLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::cellInfoList_1_4(RadioIndicationType_V1_0 type,
                                  const hidl_vec<CellInfo_V1_4>& records)
{
  DEBUG("cellInfoList_1_4");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList_1_4 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_4(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::networkScanResult_1_4(RadioIndicationType_V1_0 type,
                                       const NetworkScanResult_V1_4& aResult)
{
  DEBUG("networkScanResult_1_4");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"networkScanResult"_ns);

  uint32_t numCellInfo = aResult.networkInfos.size();
  DEBUG("networkScanResult_1_4 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> networkInfos;

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_4(&aResult.networkInfos[i]);
    networkInfos.AppendElement(cellInfo);
  }
  RefPtr<nsNetworkScanResult> scanResult =
    new nsNetworkScanResult(static_cast<int32_t>(aResult.status),
                            static_cast<int32_t>(aResult.error),
                            networkInfos);
  result->updateScanResult(scanResult);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::uiccApplicationsEnablementChanged(
  RadioIndicationType_V1_0 type,
  bool enabled)
{
  DEBUG("uiccApplicationsEnablementChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"uiccApplicationsEnablementChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateUiccApplicationsEnabled(enabled);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::registrationFailed(RadioIndicationType_V1_0 type,
                                    const CellIdentity_V1_5& cellIdentity,
                                    const hidl_string& chosenPlmn,
                                    hidl_bitfield<Domain_V1_5> domain,
                                    int32_t causeCode,
                                    int32_t additionalCauseCode)
{
  DEBUG("registrationFailed");
  mRIL->processIndication(type);

  nsString rilmessageType(u"registrationFailed"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsCellIdentity> rilCellIdentity =
    result->convertCellIdentity_V1_5(&cellIdentity);
  RefPtr<nsRegistrationFailedEvent> regFailedEvent =
    new nsRegistrationFailedEvent(rilCellIdentity,
                                  NS_ConvertUTF8toUTF16(chosenPlmn.c_str()),
                                  (int32_t)domain,
                                  causeCode,
                                  additionalCauseCode);
  result->updateRegistrationFailed(regFailedEvent);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::barringInfoChanged(
  RadioIndicationType_V1_0 type,
  const CellIdentity_V1_5& cellIdentity,
  const hidl_vec<BarringInfo_V1_5>& barringInfos)
{
  DEBUG("barringInfoChanged");
  mRIL->processIndication(type);

  nsString rilmessageType(u"barringInfoChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsCellIdentity> rilCellIdentity =
    result->convertCellIdentity_V1_5(&cellIdentity);
  nsTArray<RefPtr<nsBarringInfo>> barrInfos;
  int32_t factor = -1;
  int32_t timeSeconds = -1;
  bool isBarred = false;
  int32_t serviceType, barringType;
  uint32_t size = barringInfos.size();
  for (uint32_t i = 0; i < size; i++) {
    serviceType = (int32_t)barringInfos[i].serviceType;
    barringType = (int32_t)barringInfos[i].barringType;
    if (barringInfos[i].barringTypeSpecificInfo.getDiscriminator() ==
        BarringInfo_V1_5::BarringTypeSpecificInfo::hidl_discriminator::
          conditional) {
      factor = barringInfos[i].barringTypeSpecificInfo.conditional().factor;
      timeSeconds =
        barringInfos[i].barringTypeSpecificInfo.conditional().timeSeconds;
      isBarred = barringInfos[i].barringTypeSpecificInfo.conditional().isBarred;
    }
    RefPtr<nsBarringInfo> barrinfo = new nsBarringInfo(
      serviceType, barringType, factor, timeSeconds, isBarred);
    barrInfos.AppendElement(barrinfo);
  }

  RefPtr<nsBarringInfoChanged> barrInfoEvent =
    new nsBarringInfoChanged(rilCellIdentity, barrInfos);
  result->updateBarringInfoChanged(barrInfoEvent);
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::cellInfoList_1_5(RadioIndicationType_V1_0 type,
                                  const hidl_vec<CellInfo_V1_5>& records)
{
  DEBUG("cellInfoList_1_5");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList_1_5 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_5(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::networkScanResult_1_5(RadioIndicationType_V1_0 type,
                                       const NetworkScanResult_V1_5& aResult)
{
  DEBUG("networkScanResult_1_5");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"networkScanResult"_ns);

  uint32_t numCellInfo = aResult.networkInfos.size();
  DEBUG("networkScanResult_1_5 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> networkInfos;

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_5(&aResult.networkInfos[i]);
    networkInfos.AppendElement(cellInfo);
  }
  RefPtr<nsNetworkScanResult> scanResult = new nsNetworkScanResult(
    (int32_t)(aResult.status), (int32_t)(aResult.error), networkInfos);
  result->updateScanResult(scanResult);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::dataCallListChanged_1_5(
  RadioIndicationType_V1_0 type,
  const hidl_vec<SetupDataCallResult_V1_5>& dcList)
{
  DEBUG("dataCallListChanged_1_5");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"datacalllistchanged"_ns);
  uint32_t numDataCall = dcList.size();
  DEBUG("getDataCallListResponse numDataCall= %d", numDataCall);
  nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

  for (uint32_t i = 0; i < numDataCall; i++) {
    RefPtr<nsSetupDataCallResult> datcall =
      result->convertDcResponse_V1_5(dcList[i]);
    aDcLists.AppendElement(datcall);
  }
  result->updateDataCallListChanged(aDcLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::dataCallListChanged_1_6(
  RadioIndicationType_V1_0 type,
  const hidl_vec<SetupDataCallResult_V1_6>& dcList)
{
  DEBUG("dataCallListChanged_1_6");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"datacalllistchanged"_ns);
  uint32_t numDataCall = dcList.size();
  DEBUG("getDataCallListResponse numDataCall= %d", numDataCall);
  nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

  for (uint32_t i = 0; i < numDataCall; i++) {
    RefPtr<nsSetupDataCallResult> datcall =
      result->convertDcResponse_V1_6(dcList[i]);
    aDcLists.AppendElement(datcall);
  }
  result->updateDataCallListChanged(aDcLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::unthrottleApn(RadioIndicationType_V1_0 type,
                               const hidl_string& apn)
{
  DEBUG("unthrottleApn");
  mRIL->processIndication(type);

  nsString rilmessageType(u"unthrottleApn"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateUnthrottleApn(NS_ConvertUTF8toUTF16(apn.c_str()));
  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentLinkCapacityEstimate_1_6(
  RadioIndicationType_V1_0 type,
  const LinkCapacityEstimate_V1_6& lce)
{
  DEBUG("currentLinkCapacityEstimate_1_6");
  mRIL->processIndication(type);

  nsString rilmessageType(u"currentLinkCapacityEstimate"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsLinkCapacityEstimate> linkCapacityEstimate =
    new nsLinkCapacityEstimate(lce.downlinkCapacityKbps,
                               lce.uplinkCapacityKbps,
                               lce.secondaryDownlinkCapacityKbps,
                               lce.secondaryUplinkCapacityKbps);
  result->updateCurrentLinkCapacityEstimate(linkCapacityEstimate);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentSignalStrength_1_6(
  RadioIndicationType_V1_0 type,
  const SignalStrength_V1_6& aSignalStrength)
{
  DEBUG("currentSignalStrength_1_6");
  mRIL->processIndication(type);

  nsString rilmessageType(u"signalstrengthchange"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSignalStrength> signalStrength =
    result->convertSignalStrength_V1_6(aSignalStrength);
  result->updateCurrentSignalStrength(signalStrength);
  mRIL->sendRilIndicationResult(result);

  return Void();
}

Return<void>
nsRilIndication::cellInfoList_1_6(RadioIndicationType_V1_0 type,
                                  const hidl_vec<CellInfo_V1_6>& records)
{
  DEBUG("cellInfoList_1_6");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList_1_6 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_6(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::networkScanResult_1_6(RadioIndicationType_V1_0 type,
                                       const NetworkScanResult_V1_6& aResult)
{
  DEBUG("networkScanResult_1_6");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"networkScanResult"_ns);

  uint32_t numCellInfo = aResult.networkInfos.size();
  DEBUG("networkScanResult_1_6 numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> networkInfos;

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      result->convertRilCellInfo_V1_6(&aResult.networkInfos[i]);
    networkInfos.AppendElement(cellInfo);
  }
  RefPtr<nsNetworkScanResult> scanResult = new nsNetworkScanResult(
    (int32_t)(aResult.status), (int32_t)(aResult.error), networkInfos);
  result->updateScanResult(scanResult);

  mRIL->sendRilIndicationResult(result);
  return Void();
}

Return<void>
nsRilIndication::currentPhysicalChannelConfigs_1_6(
  RadioIndicationType_V1_0 type,
  const hidl_vec<PhysicalChannelConfig_V1_6>& configs)
{
  DEBUG("currentPhysicalChannelConfigs_1_6");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"currentPhysicalChannelConfigs"_ns);
  uint32_t numConfig = configs.size();
  DEBUG("currentPhysicalChannelConfigs numConfig= %d", numConfig);
  nsTArray<RefPtr<nsPhysicalChannelConfig>> aConfigs(numConfig);
  for (uint32_t i = 0; i < numConfig; i++) {
    nsTArray<int32_t> contextIds;
    for (uint32_t k = 0; k < configs[i].contextIds.size(); k++) {
      contextIds.AppendElement(configs[i].contextIds[k]);
    }
    int32_t geranBand = -1;
    int32_t utranBand = -1;
    int32_t eutranBand = -1;
    int32_t ngranBand = -1;

    int32_t ran_discriminator = (int32_t)configs[i].band.getDiscriminator();
    if (ran_discriminator ==
        (int32_t)
          PhysicalChannelConfig_V1_6::Band::hidl_discriminator::geranBand) {
      geranBand = (int32_t)configs[i].band.geranBand();
    } else if (ran_discriminator == (int32_t)PhysicalChannelConfig_V1_6::Band::
                                      hidl_discriminator::utranBand) {
      utranBand = (int32_t)configs[i].band.utranBand();
    } else if (ran_discriminator == (int32_t)PhysicalChannelConfig_V1_6::Band::
                                      hidl_discriminator::eutranBand) {
      eutranBand = (int32_t)configs[i].band.eutranBand();
    } else if (ran_discriminator == (int32_t)PhysicalChannelConfig_V1_6::Band::
                                      hidl_discriminator::ngranBand) {
      ngranBand = (int32_t)configs[i].band.ngranBand();
    }
    RefPtr<nsPhysicalChannelConfig> config = new nsPhysicalChannelConfig(
      nsRilResult::convertConnectionStatus(configs[i].status),
      configs[i].cellBandwidthDownlinkKhz,
      static_cast<int32_t>(configs[i].rat),
      -1,
      0,
      -1,
      contextIds,
      configs[i].physicalCellId,
      configs[i].downlinkChannelNumber,
      configs[i].uplinkChannelNumber,
      configs[i].cellBandwidthUplinkKhz,
      ran_discriminator,
      geranBand,
      utranBand,
      eutranBand,
      ngranBand);
    aConfigs.AppendElement(config);
  }
  result->updatePhysicalChannelConfig(aConfigs);

  mRIL->sendRilIndicationResult(result);
  return Void();
}
Return<void>
nsRilIndication::simPhonebookChanged(RadioIndicationType_V1_0 type)
{
  defaultResponse(type, u"simPhonebookChanged"_ns);
  return Void();
}
Return<void>
nsRilIndication::simPhonebookRecordsReceived(
  RadioIndicationType_V1_0 type,
  PbReceivedStatus_V1_6 status,
  const hidl_vec<PhonebookRecordInfo_V1_6>& aRecords)
{
  DEBUG("simPhonebookRecordsReceived");
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"simPhonebookRecordsReceived"_ns);

  uint32_t numRecords = aRecords.size();
  DEBUG("simPhonebookRecordsReceived numRecords= %d", numRecords);
  nsTArray<RefPtr<nsPhonebookRecordInfo>> pbRecordInfos;

  for (uint32_t i = 0; i < numRecords; i++) {
    nsTArray<nsString> aEmails;
    nsTArray<nsString> aAadditionalNumbers;
    uint32_t numEmails = aRecords[i].emails.size();
    for (uint32_t j = 0; j < numEmails; j++) {
      aEmails.AppendElement(
        NS_ConvertUTF8toUTF16(aRecords[i].emails[j].c_str()));
    }
    uint32_t numAddNumber = aRecords[i].additionalNumbers.size();
    for (uint32_t j = 0; j < numAddNumber; j++) {
      aAadditionalNumbers.AppendElement(
        NS_ConvertUTF8toUTF16(aRecords[i].additionalNumbers[j].c_str()));
    }
    RefPtr<nsPhonebookRecordInfo> pbRecord = new nsPhonebookRecordInfo(
      aRecords[i].recordId,
      NS_ConvertUTF8toUTF16(aRecords[i].name.c_str()),
      NS_ConvertUTF8toUTF16(aRecords[i].number.c_str()),
      aEmails,
      aAadditionalNumbers);
    pbRecordInfos.AppendElement(pbRecord);
  }
  RefPtr<nsSimPhonebookRecordsEvent> simPBRecordsEvent =
    new nsSimPhonebookRecordsEvent((int32_t)status, pbRecordInfos);
  result->updateSimPhonebookRecords(simPBRecordsEvent);

  mRIL->sendRilIndicationResult(result);
  return Void();
}
#endif

// Helper function
void
nsRilIndication::defaultResponse(const RadioIndicationType_V1_0 type,
                                 const nsString& rilmessageType)
{
  mRIL->processIndication(type);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  mRIL->sendRilIndicationResult(result);
}
int32_t
nsRilIndication::convertRadioStateToNum(RadioState_V1_0 state)
{
  switch (state) {
    case RadioState_V1_0::OFF:
      return nsIRilIndicationResult::RADIOSTATE_DISABLED;
    case RadioState_V1_0::UNAVAILABLE:
      return nsIRilIndicationResult::RADIOSTATE_UNKNOWN;
    case RadioState_V1_0::ON:
      return nsIRilIndicationResult::RADIOSTATE_ENABLED;
    default:
      return nsIRilIndicationResult::RADIOSTATE_UNKNOWN;
  }
}

int32_t
nsRilIndication::convertSimRefreshType(SimRefreshType_V1_0 type)
{
  switch (type) {
    case SimRefreshType_V1_0::SIM_FILE_UPDATE:
      return nsIRilIndicationResult::SIM_REFRESH_FILE_UPDATE;
    case SimRefreshType_V1_0::SIM_INIT:
      return nsIRilIndicationResult::SIM_REFRESH_INIT;
    case SimRefreshType_V1_0::SIM_RESET:
      return nsIRilIndicationResult::SIM_REFRESH_RESET;
    default:
      return nsIRilIndicationResult::SIM_REFRESH_UNKNOW;
  }
}

int32_t
nsRilIndication::convertPhoneRestrictedState(PhoneRestrictedState_V1_0 state)
{
  switch (state) {
    case PhoneRestrictedState_V1_0::NONE:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_NONE;
    case PhoneRestrictedState_V1_0::CS_EMERGENCY:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_EMERGENCY;
    case PhoneRestrictedState_V1_0::CS_NORMAL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_NORMAL;
    case PhoneRestrictedState_V1_0::CS_ALL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_ALL;
    case PhoneRestrictedState_V1_0::PS_ALL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_PS_ALL;
    default:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_NONE;
  }
}

int32_t
nsRilIndication::convertUssdModeType(UssdModeType_V1_0 type)
{
  switch (type) {
    case UssdModeType_V1_0::NOTIFY:
      return nsIRilIndicationResult::USSD_MODE_NOTIFY;
    case UssdModeType_V1_0::REQUEST:
      return nsIRilIndicationResult::USSD_MODE_REQUEST;
    case UssdModeType_V1_0::NW_RELEASE:
      return nsIRilIndicationResult::USSD_MODE_NW_RELEASE;
    case UssdModeType_V1_0::LOCAL_CLIENT:
      return nsIRilIndicationResult::USSD_MODE_LOCAL_CLIENT;
    case UssdModeType_V1_0::NOT_SUPPORTED:
      return nsIRilIndicationResult::USSD_MODE_NOT_SUPPORTED;
    case UssdModeType_V1_0::NW_TIMEOUT:
      return nsIRilIndicationResult::USSD_MODE_NW_TIMEOUT;
    default:
      // TODO need confirmed the default value.
      return nsIRilIndicationResult::USSD_MODE_NW_TIMEOUT;
  }
}

int32_t
nsRilIndication::convertSrvccState(SrvccState_V1_0 state)
{
  switch (state) {
    case SrvccState_V1_0::HANDOVER_STARTED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_STARTED;
    case SrvccState_V1_0::HANDOVER_COMPLETED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_COMPLETED;
    case SrvccState_V1_0::HANDOVER_FAILED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_FAILED;
    case SrvccState_V1_0::HANDOVER_CANCELED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_CANCELED;
    default:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_CANCELED;
  }
}

int32_t
nsRilIndication::convertHardwareConfigType(HardwareConfigType_V1_0 state)
{
  switch (state) {
    case HardwareConfigType_V1_0::MODEM:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM;
    case HardwareConfigType_V1_0::SIM:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_SIM;
    default:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM;
  }
}

int32_t
nsRilIndication::convertHardwareConfigState(HardwareConfigState_V1_0 state)
{
  switch (state) {
    case HardwareConfigState_V1_0::ENABLED:
      return nsIRilIndicationResult::HW_CONFIG_STATE_ENABLED;
    case HardwareConfigState_V1_0::STANDBY:
      return nsIRilIndicationResult::HW_CONFIG_STATE_STANDBY;
    case HardwareConfigState_V1_0::DISABLED:
      return nsIRilIndicationResult::HW_CONFIG_STATE_DISABLED;
    default:
      return nsIRilIndicationResult::HW_CONFIG_STATE_DISABLED;
  }
}

int32_t
nsRilIndication::convertRadioCapabilityPhase(RadioCapabilityPhase_V1_0 value)
{
  switch (value) {
    case RadioCapabilityPhase_V1_0::CONFIGURED:
      return nsIRilIndicationResult::RADIO_CP_CONFIGURED;
    case RadioCapabilityPhase_V1_0::START:
      return nsIRilIndicationResult::RADIO_CP_START;
    case RadioCapabilityPhase_V1_0::APPLY:
      return nsIRilIndicationResult::RADIO_CP_APPLY;
    case RadioCapabilityPhase_V1_0::UNSOL_RSP:
      return nsIRilIndicationResult::RADIO_CP_UNSOL_RSP;
    case RadioCapabilityPhase_V1_0::FINISH:
      return nsIRilIndicationResult::RADIO_CP_FINISH;
    default:
      return nsIRilIndicationResult::RADIO_CP_FINISH;
  }
}

int32_t
nsRilIndication::convertRadioAccessFamily(RadioAccessFamily_V1_0 value)
{
  switch (value) {
    case RadioAccessFamily_V1_0::UNKNOWN:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UNKNOWN;
    case RadioAccessFamily_V1_0::GPRS:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_GPRS;
    case RadioAccessFamily_V1_0::EDGE:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EDGE;
    case RadioAccessFamily_V1_0::UMTS:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UMTS;
    case RadioAccessFamily_V1_0::IS95A:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_IS95A;
    case RadioAccessFamily_V1_0::IS95B:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_IS95B;
    case RadioAccessFamily_V1_0::ONE_X_RTT:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_ONE_X_RTT;
    case RadioAccessFamily_V1_0::EVDO_0:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_0;
    case RadioAccessFamily_V1_0::EVDO_A:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_A;
    case RadioAccessFamily_V1_0::HSDPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSDPA;
    case RadioAccessFamily_V1_0::HSUPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSUPA;
    case RadioAccessFamily_V1_0::HSPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSPA;
    case RadioAccessFamily_V1_0::EVDO_B:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_B;
    case RadioAccessFamily_V1_0::EHRPD:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EHRPD;
    case RadioAccessFamily_V1_0::LTE:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_LTE;
    case RadioAccessFamily_V1_0::HSPAP:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSPAP;
    case RadioAccessFamily_V1_0::GSM:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_GSM;
    case RadioAccessFamily_V1_0::TD_SCDMA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_TD_SCDMA;
    case RadioAccessFamily_V1_0::LTE_CA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_LTE_CA;
    default:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UNKNOWN;
  }
}

int32_t
nsRilIndication::convertRadioCapabilityStatus(RadioCapabilityStatus_V1_0 state)
{
  switch (state) {
    case RadioCapabilityStatus_V1_0::NONE:
      return nsIRilIndicationResult::RADIO_CP_STATUS_NONE;
    case RadioCapabilityStatus_V1_0::SUCCESS:
      return nsIRilIndicationResult::RADIO_CP_STATUS_SUCCESS;
    case RadioCapabilityStatus_V1_0::FAIL:
      return nsIRilIndicationResult::RADIO_CP_STATUS_FAIL;
    default:
      return nsIRilIndicationResult::RADIO_CP_STATUS_NONE;
  }
}
