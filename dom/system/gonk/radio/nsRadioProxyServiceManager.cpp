/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRadioProxyServiceManager.h"
#include "mozilla/Preferences.h"
#include <android/binder_manager.h>
#include <arpa/inet.h>

/* Logging related */
#if !defined(RILPROXY_LOG_TAG)
#define RILPROXY_LOG_TAG "nsRadioProxyServiceManager"
#endif

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...)                                                          \
  __android_log_print(ANDROID_LOG_INFO, RILPROXY_LOG_TAG, ##args)
#define ERROR(args...)                                                         \
  __android_log_print(ANDROID_LOG_ERROR, RILPROXY_LOG_TAG, ##args)
#define ERROR_NS_OK(args...)                                                   \
  {                                                                            \
    __android_log_print(ANDROID_LOG_ERROR, RILPROXY_LOG_TAG, ##args);          \
    return NS_OK;                                                              \
  }
#define DEBUG(args...)                                                         \
  do {                                                                         \
    if (gRilDebug_isLoggingEnabled) {                                          \
      __android_log_print(ANDROID_LOG_DEBUG, RILPROXY_LOG_TAG, ##args);        \
    }                                                                          \
  } while (0)

using namespace android;
using namespace mozilla;

nsString
convertPdpProtocolType(int32_t pdpType)
{
  const char* PdpProtocolTypeMaps[6] = { "IP",  "IPV6",   "IPV4V6",
                                         "PPP", "NON_IP", "UNSTRUCTURED" };
  nsString type;
  type.Assign(NS_ConvertUTF8toUTF16("UNKNOWN"));
  if (pdpType >= 0 && pdpType <= 5) {
    type.Assign(NS_ConvertUTF8toUTF16(PdpProtocolTypeMaps[pdpType]));
  }
  return type;
}

RefPtr<nsLinkAddress>
convertLinkAddress(
  const aidl::android::hardware::radio::data::LinkAddress& aLinkaddress)
{
  RefPtr<nsLinkAddress> linkAddress =
    new nsLinkAddress(NS_ConvertUTF8toUTF16(aLinkaddress.address.c_str()),
                      (int32_t)aLinkaddress.addressProperties,
                      aLinkaddress.deprecationTime,
                      aLinkaddress.expirationTime);
  return linkAddress;
}

RefPtr<nsSetupDataCallResult>
convertDcResponse(
  const aidl::android::hardware::radio::data::SetupDataCallResult& aDcResponse)
{
  nsTArray<RefPtr<nsLinkAddress>> addresses;
  uint32_t addrLen = aDcResponse.addresses.size();
  for (uint32_t i = 0; i < addrLen; i++) {
    RefPtr<nsLinkAddress> linkAddress =
      convertLinkAddress(aDcResponse.addresses[i]);
    addresses.AppendElement(linkAddress);
  }
  nsCString dnses;
  uint32_t dnsLen = aDcResponse.dnses.size();
  for (uint32_t i = 0; i < dnsLen; i++) {
    dnses.Append(aDcResponse.dnses[i].c_str());
    if (i <= dnsLen - 2) {
      dnses.AppendLiteral(" ");
    }
  }
  nsCString gateways;
  uint32_t gatewLen = aDcResponse.gateways.size();
  for (uint32_t i = 0; i < gatewLen; i++) {
    gateways.Append(aDcResponse.gateways[i].c_str());
    if (i <= gatewLen - 2) {
      gateways.AppendLiteral(" ");
    }
  }
  nsCString pcscf;
  uint32_t pcscfLen = aDcResponse.pcscf.size();
  for (uint32_t i = 0; i < pcscfLen; i++) {
    pcscf.Append(aDcResponse.pcscf[i].c_str());
    if (i <= pcscfLen - 2) {
      pcscf.AppendLiteral(" ");
    }
  }
  int32_t mtu = std::max(aDcResponse.mtuV4, aDcResponse.mtuV6);

  RefPtr<nsSliceInfo> sliceInfo = nullptr;

  if (aDcResponse.sliceInfo.has_value()) {
    sliceInfo = new nsSliceInfo(aDcResponse.sliceInfo.value());
  }

  nsTArray<RefPtr<nsITrafficDescriptor>> trafficDescriptors;
  for (uint32_t i = 0; i < aDcResponse.trafficDescriptors.size(); i++) {
    RefPtr<nsITrafficDescriptor> value =
      new nsTrafficDescriptor(aDcResponse.trafficDescriptors[i]);
    trafficDescriptors.AppendElement(value);
  }

  nsTArray<RefPtr<nsIQosSession>> qosSessions;
  for (uint32_t i = 0; i < aDcResponse.qosSessions.size(); i++) {
    RefPtr<nsIQosSession> value = new nsQosSession(aDcResponse.qosSessions[i]);
    qosSessions.AppendElement(value);
  }

  RefPtr<nsSetupDataCallResult> dcResponse =
    new nsSetupDataCallResult((int32_t)aDcResponse.cause,
                              aDcResponse.suggestedRetryTime,
                              aDcResponse.cid,
                              (int32_t) static_cast<int>(aDcResponse.active),
                              convertPdpProtocolType((int32_t)aDcResponse.type),
                              NS_ConvertUTF8toUTF16(aDcResponse.ifname.c_str()),
                              addresses,
                              NS_ConvertUTF8toUTF16(dnses),
                              NS_ConvertUTF8toUTF16(gateways),
                              NS_ConvertUTF8toUTF16(pcscf),
                              mtu,
                              aDcResponse.mtuV4,
                              aDcResponse.mtuV6,
                              aDcResponse.pduSessionId,
                              (int32_t)aDcResponse.handoverFailureMode,
                              sliceInfo,
                              new nsQos(aDcResponse.defaultQos),
                              trafficDescriptors,
                              qosSessions);

  return dcResponse;
}

void
RadioDataProxyDeathNotifier(void* cookie)
{
  nsRadioProxyServiceManager* iface =
    static_cast<nsRadioProxyServiceManager*>(cookie);
  if (iface) {
    iface->onRadioDataBinderDied();
  }
}

void
RadioVoiceProxyDeathNotifier(void* cookie)
{
  nsRadioProxyServiceManager* iface =
    static_cast<nsRadioProxyServiceManager*>(cookie);
  if (iface) {
    iface->onRadioVoiceBinderDied();
  }
}

void
RadioSimProxyDeathNotifier(void* cookie)
{
  nsRadioProxyServiceManager* iface =
    static_cast<nsRadioProxyServiceManager*>(cookie);
  if (iface) {
    iface->onRadioSimBinderDied();
  }
}

void
RadioConfigProxyDeathNotifier(void* cookie)
{
  nsRadioProxyServiceManager* iface =
    static_cast<nsRadioProxyServiceManager*>(cookie);
  if (iface) {
    iface->onRadioConfigBinderDied();
  }
}

/**
 * Implementation of RadioDataIndication
 */
RadioDataIndication::RadioDataIndication(nsRadioProxyServiceManager& parent)
  : parent_data(parent)
{}

ndk::ScopedAStatus
RadioDataIndication::dataCallListChanged(
  RadioIndicationType type,
  const std::vector<SetupDataCallResult>& dcList)
{

  DEBUG("dataCallListChanged");
  parent_data.sendIndAck(type, SERVICE_TYPE::DATA);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"datacalllistchanged"_ns);
  uint32_t numDataCall = dcList.size();
  DEBUG("getDataCallListResponse numDataCall= %d", numDataCall);
  nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

  for (uint32_t i = 0; i < numDataCall; i++) {
    RefPtr<nsSetupDataCallResult> datcall = convertDcResponse(dcList[i]);
    aDcLists.AppendElement(datcall);
  }
  result->updateDataCallListChanged(aDcLists);

  parent_data.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataIndication::keepaliveStatus(RadioIndicationType /*type*/,
                                     const aidl_KeepaliveStatus& /*status*/)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataIndication::pcoData(RadioIndicationType type, const PcoDataInfo& pco)
{
  parent_data.sendIndAck(type, SERVICE_TYPE::DATA);
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
  parent_data.mRIL->sendRilIndicationResult(result);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataIndication::unthrottleApn(RadioIndicationType type,
                                   const DataProfileInfo& dataProfileInfo)
{
  DEBUG("unthrottleApn");
  parent_data.sendIndAck(type, SERVICE_TYPE::DATA);

  nsString rilmessageType(u"unthrottleApn"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateUnthrottleApn(
    NS_ConvertUTF8toUTF16(dataProfileInfo.apn.c_str()));
  parent_data.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataIndication::slicingConfigChanged(RadioIndicationType /*type*/,
                                          const SlicingConfig& slicingConfig)
{
  DEBUG("slicingConfigChanged");
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"slicingConfigChanged"_ns);
  uint32_t urspRulesL = slicingConfig.urspRules.size();
  uint32_t sliceInfoL = slicingConfig.sliceInfo.size();
  nsTArray<RefPtr<nsIUrspRule>> urspRules(urspRulesL);
  nsTArray<RefPtr<nsISliceInfo>> sliceInfos(sliceInfoL);

  for (uint32_t i = 0; i < urspRulesL; i++) {
    uint32_t trafficDescriptorsL =
      slicingConfig.urspRules[i].trafficDescriptors.size();
    uint32_t routeSelectionDescriptorL =
      slicingConfig.urspRules[i].routeSelectionDescriptor.size();
    nsTArray<RefPtr<nsITrafficDescriptor>> trafficDescriptors(
      trafficDescriptorsL);
    nsTArray<RefPtr<nsIRouteSelectionDescriptor>> routeSelectionDescriptors(
      routeSelectionDescriptorL);
    for (uint32_t m = 0; m < trafficDescriptorsL; m++) {
      nsString dnnNsAString;
      if (slicingConfig.urspRules[i].trafficDescriptors[m].dnn.has_value()) {
        dnnNsAString.Assign(NS_ConvertUTF8toUTF16(slicingConfig.urspRules[i]
                                                    .trafficDescriptors[m]
                                                    .dnn.value()
                                                    .c_str()));
      }
      nsTArray<int32_t> osAppId;
      if (slicingConfig.urspRules[i]
            .trafficDescriptors[m]
            .osAppId.has_value()) {
        for (uint32_t v = 0; v < slicingConfig.urspRules[i]
                                   .trafficDescriptors[m]
                                   .osAppId.value()
                                   .osAppId.size();
             v++) {
          osAppId.AppendElement((int32_t)slicingConfig.urspRules[i]
                                  .trafficDescriptors[m]
                                  .osAppId.value()
                                  .osAppId[v]);
        }
      }
      RefPtr<nsITrafficDescriptor> trafficDescriptor =
        new nsTrafficDescriptor(dnnNsAString, osAppId);
      trafficDescriptors.AppendElement(trafficDescriptor);
    }
    for (uint32_t n = 0; n < routeSelectionDescriptorL; n++) {
      uint32_t sliceInfoL =
        slicingConfig.urspRules[i].routeSelectionDescriptor[n].sliceInfo.size();
      nsTArray<RefPtr<nsISliceInfo>> sliceInfos(sliceInfoL);
      for (uint32_t k = 0; k < sliceInfoL; k++) {
        RefPtr<nsISliceInfo> sliceInfo =
          new nsSliceInfo(slicingConfig.urspRules[i]
                            .routeSelectionDescriptor[n]
                            .sliceInfo[k]
                            .sliceServiceType,
                          slicingConfig.urspRules[i]
                            .routeSelectionDescriptor[n]
                            .sliceInfo[k]
                            .sliceDifferentiator,
                          slicingConfig.urspRules[i]
                            .routeSelectionDescriptor[n]
                            .sliceInfo[k]
                            .mappedHplmnSst,
                          slicingConfig.urspRules[i]
                            .routeSelectionDescriptor[n]
                            .sliceInfo[k]
                            .mappedHplmnSd,
                          slicingConfig.urspRules[i]
                            .routeSelectionDescriptor[n]
                            .sliceInfo[k]
                            .status);
        sliceInfos.AppendElement(sliceInfo);
      }
      nsTArray<nsString> aDnns;
      for (uint32_t l = 0;
           l <
           slicingConfig.urspRules[i].routeSelectionDescriptor[n].dnn.size();
           l++) {
        nsString aDnnNsAString;
        aDnnNsAString.Assign(
          NS_ConvertUTF8toUTF16(slicingConfig.urspRules[i]
                                  .routeSelectionDescriptor[n]
                                  .dnn[l]
                                  .c_str()));
        aDnns.AppendElement(aDnnNsAString);
      }

      RefPtr<nsIRouteSelectionDescriptor> routeSelectionDescriptor =
        new nsRouteSelectionDescriptor(
          slicingConfig.urspRules[i].routeSelectionDescriptor[n].precedence,
          (uint32_t)slicingConfig.urspRules[i]
            .routeSelectionDescriptor[n]
            .sessionType,
          slicingConfig.urspRules[i].routeSelectionDescriptor[n].sscMode,
          sliceInfos,
          aDnns);
      routeSelectionDescriptors.AppendElement(routeSelectionDescriptor);
    }
    RefPtr<nsIUrspRule> urspRule =
      new nsUrspRule(slicingConfig.urspRules[i].precedence,
                     trafficDescriptors,
                     routeSelectionDescriptors);

    urspRules.AppendElement(urspRule);
  }
  for (uint32_t i = 0; i < sliceInfoL; i++) {
    RefPtr<nsSliceInfo> sliceInfo =
      new nsSliceInfo(slicingConfig.sliceInfo[i].sliceServiceType,
                      slicingConfig.sliceInfo[i].sliceDifferentiator,
                      slicingConfig.sliceInfo[i].mappedHplmnSst,
                      slicingConfig.sliceInfo[i].mappedHplmnSd,
                      slicingConfig.sliceInfo[i].status);
    sliceInfos.AppendElement(sliceInfo);
  }

  RefPtr<nsSlicingConfig> slicingCon =
    new nsSlicingConfig(urspRules, sliceInfos);
  result->updateSlicingConfig(slicingCon);

  parent_data.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioDataIndication
 */
RadioDataResponse::RadioDataResponse(nsRadioProxyServiceManager& parent)
  : parent_data(parent)
{}

ndk::ScopedAStatus RadioDataResponse::acknowledgeRequest(int32_t /*serial*/)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::allocatePduSessionIdResponse(const RadioResponseInfo& info,
                                                int32_t id)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"allocatePduSessionId"_ns,
                            info.serial,
                            parent_data.convertRadioErrorToNum(info.error));
  if (info.error == RadioError::NONE) {
    result->updateAllocatedId(id);
  }
  parent_data.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

void
RadioDataResponse::defaultResponse(const RadioResponseInfo& rspInfo,
                                   const nsString& rilmessageType)
{
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(rilmessageType,
                            rspInfo.serial,
                            parent_data.convertRadioErrorToNum(rspInfo.error));
  parent_data.mRIL->sendRilResponseResult(result);
}

ndk::ScopedAStatus
RadioDataResponse::cancelHandoverResponse(const RadioResponseInfo& info)
{
  DEBUG("cancelHandoverResponse");
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"cancelHandover"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::deactivateDataCallResponse(const RadioResponseInfo& info)
{
  DEBUG("deactivateDataCallResponse");
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"deactivateDataCall"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::getDataCallListResponse(
  const RadioResponseInfo& info,
  const std::vector<SetupDataCallResult>& dcResponse)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataCallList"_ns,
                            info.serial,
                            parent_data.convertRadioErrorToNum(info.error));
  if (info.error == RadioError::NONE) {
    uint32_t numDataCall = dcResponse.size();
    nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);
    for (uint32_t i = 0; i < numDataCall; i++) {
      RefPtr<nsSetupDataCallResult> datcall = convertDcResponse(dcResponse[i]);
      aDcLists.AppendElement(datcall);
    }
    result->updateDcList(aDcLists);
  } else {
    DEBUG("getDataCallListResponse error.");
  }
  parent_data.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::getSlicingConfigResponse(const RadioResponseInfo& info,
                                            const SlicingConfig& slicingConfig)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSlicingConfig"_ns,
                            info.serial,
                            parent_data.convertRadioErrorToNum(info.error));
  if (info.error == RadioError::NONE) {

    nsTArray<RefPtr<nsISliceInfo>> aSliceInfos;
    for (uint32_t i = 0; i < slicingConfig.sliceInfo.size(); i++) {
      RefPtr<nsISliceInfo> data = new nsSliceInfo(slicingConfig.sliceInfo[i]);
      aSliceInfos.AppendElement(data);
    }

    nsTArray<RefPtr<nsIUrspRule>> aRules;
    for (uint32_t i = 0; i < slicingConfig.urspRules.size(); i++) {
      RefPtr<nsIUrspRule> data = new nsUrspRule(slicingConfig.urspRules[i]);
      aRules.AppendElement(data);
    }
    RefPtr<nsISlicingConfig> config = new nsSlicingConfig(aRules, aSliceInfos);
    result->updateSlicingConfig(config);
  }
  parent_data.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::releasePduSessionIdResponse(const RadioResponseInfo& info)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"releasePduSessionId"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::setDataAllowedResponse(const RadioResponseInfo& info)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"setDataRegistration"_ns);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::setDataProfileResponse(const RadioResponseInfo& info)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"setDataProfile"_ns);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::setDataThrottlingResponse(const RadioResponseInfo& info)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"setDataThrottling"_ns);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::setInitialAttachApnResponse(const RadioResponseInfo& info)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(rspInfo, u"setInitialAttachApn"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::setupDataCallResponse(const RadioResponseInfo& info,
                                         const SetupDataCallResult& dcResponse)
{
  rspInfo = info;
  setupDataCallResult = dcResponse;
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"setupDataCall"_ns,
                            rspInfo.serial,
                            parent_data.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
      convertDcResponse(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall error.");
  }

  parent_data.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::startHandoverResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  defaultResponse(info, u"startHandover"_ns);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::startKeepaliveResponse(const RadioResponseInfo& info,
                                          const aidl_KeepaliveStatus& status)
{
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"startKeepalive"_ns,
                            rspInfo.serial,
                            parent_data.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    nsIKeepAliveStatus* aStatus =
      new nsKeepAliveStatus(status.sessionHandle, (int32_t)status.code);
    result->updateKeppAliveStatus(aStatus);

  } else {
    DEBUG("startKeepaliveResponse error: %d", (int32_t)rspInfo.error);
  }
  parent_data.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioDataResponse::stopKeepaliveResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_data.sendRspAck(info, info.serial, SERVICE_TYPE::DATA);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioVoiceIndication
 *
 * @param parent
 */
RadioVoiceIndication::RadioVoiceIndication(nsRadioProxyServiceManager& parent)
  : parent_voice(parent)
{}

ndk::ScopedAStatus
RadioVoiceIndication::callRing(RadioIndicationType type,
                               bool /*isGsm*/,
                               const CdmaSignalInfoRecord& /*record*/)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);

  // TODO impelment CDMA later.
  nsString rilmessageType(u"callRing"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  parent_voice.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::callStateChanged(RadioIndicationType type)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(type, u"callStateChanged"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::cdmaCallWaiting(
  RadioIndicationType /*type*/,
  const CdmaCallWaiting& /*callWaitingRecord*/)
{
  DEBUG("Not implement cdmaCallWaiting");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::cdmaInfoRec(
  RadioIndicationType /*type*/,
  const std::vector<CdmaInformationRecord>& /*records*/)
{
  DEBUG("Not implement cdmaInfoRec");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RadioVoiceIndication::cdmaOtaProvisionStatus(
  RadioIndicationType /*type*/,
  CdmaOtaProvisionStatus /*status*/)
{
  DEBUG("Not implement cdmaOtaProvisionStatus");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::currentEmergencyNumberList(
  RadioIndicationType type,
  const std::vector<EmergencyNumber>& emergencyNumberList)
{
  DEBUG("currentEmergencyNumberList");
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
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
  parent_voice.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::enterEmergencyCallbackMode(RadioIndicationType type)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(type, u"enterEmergencyCbMode"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::exitEmergencyCallbackMode(RadioIndicationType type)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(type, u"exitEmergencyCbModev"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::indicateRingbackTone(RadioIndicationType type, bool start)
{
  DEBUG("ringbackTone");
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);

  nsString rilmessageType(u"ringbackTone"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateIndicateRingbackTone(start);
  parent_voice.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::onSupplementaryServiceIndication(
  RadioIndicationType /*type*/,
  const StkCcUnsolSsResult& /*ss*/)
{
  DEBUG("Not implement onSupplementaryServiceIndication");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::onUssd(RadioIndicationType type,
                             UssdModeType modeType,
                             const std::string& msg)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
  nsString rilmessageType(u"ussdreceived"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateOnUssd(parent_voice.convertUssdModeType(modeType),
                       NS_ConvertUTF8toUTF16(msg.c_str()));
  parent_voice.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::resendIncallMute(RadioIndicationType type)
{
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(type, u"resendIncallMute"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::srvccStateNotify(RadioIndicationType type,
                                       SrvccState state)
{
  DEBUG("srvccStateNotify");
  parent_voice.sendIndAck(type, SERVICE_TYPE::VOICE);

  nsString rilmessageType(u"srvccStateNotify"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateSrvccStateNotify(parent_voice.convertSrvccState(state));
  parent_voice.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceIndication::stkCallControlAlphaNotify(RadioIndicationType /*type*/,
                                                const std::string& /*alpha*/)
{
  DEBUG("Not implement stkCallControlAlphaNotify");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RadioVoiceIndication::stkCallSetup(
  RadioIndicationType /*type*/,
  int64_t /*timeout*/)
{
  DEBUG("Not implement stkCallSetup");
  return ndk::ScopedAStatus::ok();
}

RadioVoiceResponse::RadioVoiceResponse(nsRadioProxyServiceManager& parent)
  : parent_voice(parent)
{}

ndk::ScopedAStatus
RadioVoiceResponse::acceptCallResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"answerCall"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RadioVoiceResponse::acknowledgeRequest(int32_t /*serial*/)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::cancelPendingUssdResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"cancelUSSD"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::conferenceResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"conferenceCall"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::dialResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"dial"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::emergencyDialResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"emergencyDial"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::exitEmergencyCallbackModeResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"sendExitEmergencyCbModeRequest"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::explicitCallTransferResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"explicitCallTransfer"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getCallForwardStatusResponse(
  const RadioResponseInfo& info,
  const std::vector<CallForwardInfo>& callForwardInfos)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryCallForwardStatus"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numCallForwardInfo = callForwardInfos.size();
    nsTArray<RefPtr<nsCallForwardInfo>> aCallForwardInfoLists(
      numCallForwardInfo);

    for (uint32_t i = 0; i < numCallForwardInfo; i++) {
      RefPtr<nsCallForwardInfo> callForwardInfo = new nsCallForwardInfo(
        parent_voice.convertCallForwardState(callForwardInfos[i].status),
        callForwardInfos[i].reason,
        callForwardInfos[i].serviceClass,
        callForwardInfos[i].toa,
        NS_ConvertUTF8toUTF16(callForwardInfos[i].number.c_str()),
        callForwardInfos[i].timeSeconds);
      aCallForwardInfoLists.AppendElement(callForwardInfo);
    }
    result->updateCallForwardStatusList(aCallForwardInfoLists);
  } else {
    DEBUG("getCallForwardStatusResponse error.");
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getCallWaitingResponse(const RadioResponseInfo& info,
                                           bool enable,
                                           int32_t serviceClass)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryCallWaiting"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateCallWaiting(enable, serviceClass);
  } else {
    DEBUG("getCallWaitingResponse error.");
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getClipResponse(const RadioResponseInfo& info,
                                    ClipStatus status)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryCLIP"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateClip(parent_voice.convertClipState(status));
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getClirResponse(const RadioResponseInfo& info,
                                    int32_t n,
                                    int32_t m)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCLIR"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateClir(n, m);
  } else {
    DEBUG("getClirResponse error.");
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getCurrentCallsResponse(const RadioResponseInfo& info,
                                            const std::vector<Call>& calls)
{
  rspInfo = info;
  currentCalls = calls;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCurrentCalls"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numCalls = calls.size();
    DEBUG("getCurrentCalls numCalls= %d", numCalls);
    nsTArray<RefPtr<nsCall>> aCalls(numCalls);

    for (uint32_t i = 0; i < numCalls; i++) {
      uint32_t numUusInfo = calls[i].uusInfo.size();
      DEBUG("getCurrentCalls numUusInfo= %d", numUusInfo);
      nsTArray<RefPtr<nsUusInfo>> aUusInfos(numUusInfo);

      for (uint32_t j = 0; j < numUusInfo; j++) {
        RefPtr<nsUusInfo> uusinfo = new nsUusInfo(
          parent_voice.convertUusType(calls[i].uusInfo[j].uusType),
          parent_voice.convertUusDcs(calls[i].uusInfo[j].uusDcs),
          NS_ConvertUTF8toUTF16(calls[i].uusInfo[j].uusData.c_str()));

        aUusInfos.AppendElement(uusinfo);
      }

      DEBUG("getCurrentCalls index= %d  state=%d",
            calls[i].index,
            parent_voice.convertCallState(calls[i].state));
      RefPtr<nsCall> call = new nsCall(
        parent_voice.convertCallState(calls[i].state),
        calls[i].index,
        calls[i].toa,
        calls[i].isMpty,
        calls[i].isMT,
        int32_t(calls[i].als),
        calls[i].isVoice,
        calls[i].isVoicePrivacy,
        NS_ConvertUTF8toUTF16(calls[i].number.c_str()),
        parent_voice.convertCallPresentation(calls[i].numberPresentation),
        NS_ConvertUTF8toUTF16(calls[i].name.c_str()),
        parent_voice.convertCallPresentation(calls[i].namePresentation),
        aUusInfos,
        parent_voice.convertAudioQuality(calls[i].audioQuality),
        NS_ConvertUTF8toUTF16(calls[i].forwardedNumber.c_str()));
      aCalls.AppendElement(call);
    }
    result->updateCurrentCalls(aCalls);
  } else {
    DEBUG("getCurrentCalls error.");
  }

  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getLastCallFailCauseResponse(
  const RadioResponseInfo& info,
  const LastCallFailCauseInfo& failCauseInfo)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getFailCause"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateFailCause(
      // parent_voice.covertLastCallFailCause(failCauseInfo.causeCode),
      (int32_t)(failCauseInfo.causeCode),
      NS_ConvertUTF8toUTF16(failCauseInfo.vendorCause.c_str()));
  } else {
    DEBUG("getLastCallFailCauseResponse error.");
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getMuteResponse(const RadioResponseInfo& info, bool enable)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getMute"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateMute(enable);
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getPreferredVoicePrivacyResponse(
  const RadioResponseInfo& info,
  bool enable)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryVoicePrivacyMode"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateVoicePrivacy(enable);
  }
  parent_voice.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::getTtyModeResponse(const RadioResponseInfo& info,
                                       TtyMode /*mode*/)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  DEBUG("Not support getTtyModeResponse at present");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::handleStkCallSetupRequestFromSimResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"stkHandleCallSetup"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::hangupConnectionResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"hangUpCall"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::hangupForegroundResumeBackgroundResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"hangUpForeground"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::hangupWaitingOrBackgroundResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"hangUpBackground"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::isVoNrEnabledResponse(const RadioResponseInfo& info,
                                          bool enabled)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"isVoNrEnabled"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("isVoNrEnabledResponse with vonr enabled= %b", enabled);
    result->updateVoNREnabled(enabled);
  } else {
    DEBUG("isVoNrEnabledResponse error %d", rspInfo.error);
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::rejectCallResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"udub"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::sendBurstDtmfResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::sendCdmaFeatureCodeResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::sendDtmfResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"sendTone"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::sendUssdResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"sendUSSD"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::separateConnectionResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"separateCall"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setCallForwardResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"setCallForward"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setCallWaitingResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"setCallWaiting"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setClirResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"setCLIR"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setMuteResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"setMute"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setPreferredVoicePrivacyResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"setVoicePrivacyMode"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setTtyModeResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::setVoNrEnabledResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"setVoNrEnabled"_ns,
                            rspInfo.serial,
                            parent_voice.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("setVoNrEnabled successfully");
  } else {
    DEBUG("setVoNrEnabled failed with error %d", rspInfo.error);
  }
  parent_voice.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::startDtmfResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"startTone"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::stopDtmfResponse(const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"stopTone"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioVoiceResponse::switchWaitingOrHoldingAndActiveResponse(
  const RadioResponseInfo& info)
{
  rspInfo = info;
  parent_voice.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::VOICE);
  parent_voice.defaultResponse(rspInfo, u"switchActiveCall"_ns);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioMessagingIndication
 */
RadioMessagingIndication::RadioMessagingIndication(
  nsRadioProxyServiceManager& parent)
  : parent_Message(parent)
{}
ndk::ScopedAStatus
RadioMessagingIndication::cdmaNewSms(RadioIndicationType in_type,
                                     const CdmaSmsMessage& in_msg)
{
  DEBUG("Not implement cdmaNewSms");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::cdmaRuimSmsStorageFull(RadioIndicationType in_type)
{
  DEBUG("Not implement cdmaRuimSmsStorageFull");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::newBroadcastSms(RadioIndicationType in_type,
                                          const std::vector<uint8_t>& in_data)
{
  parent_Message.sendIndAck(in_type, SERVICE_TYPE::MESSAGE);

  nsString rilmessageType(u"cellbroadcast-received"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = in_data.size();

  nsTArray<int32_t> aData;
  for (uint32_t i = 0; i < size; i++) {
    aData.AppendElement(in_data[i]);
  }
  result->updateNewBroadcastSms(aData);

  parent_Message.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::newSms(RadioIndicationType in_type,
                                 const std::vector<uint8_t>& in_pdu)
{
  parent_Message.sendIndAck(in_type, SERVICE_TYPE::MESSAGE);

  nsString rilmessageType(u"sms-received"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = in_pdu.size();

  nsTArray<int32_t> pdu_u32;
  for (uint32_t i = 0; i < size; i++) {
    pdu_u32.AppendElement(in_pdu[i]);
  }
  result->updateOnSms(pdu_u32);

  parent_Message.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::newSmsOnSim(RadioIndicationType in_type,
                                      int32_t in_recordNumber)
{
  parent_Message.sendIndAck(in_type, SERVICE_TYPE::MESSAGE);

  nsString rilmessageType(u"smsOnSim"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateNewSmsOnSim(in_recordNumber);

  parent_Message.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::newSmsStatusReport(RadioIndicationType in_type,
                                             const std::vector<uint8_t>& in_pdu)
{
  parent_Message.sendIndAck(in_type, SERVICE_TYPE::MESSAGE);

  nsString rilmessageType(u"smsstatusreport"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  uint32_t size = in_pdu.size();

  nsTArray<int32_t> pdu_u32;
  for (uint32_t i = 0; i < size; i++) {
    pdu_u32.AppendElement(in_pdu[i]);
  }
  result->updateOnSms(pdu_u32);

  parent_Message.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingIndication::simSmsStorageFull(RadioIndicationType in_type)
{
  parent_Message.sendIndAck(in_type, SERVICE_TYPE::MESSAGE);
  parent_Message.defaultResponse(in_type, u"simSmsStorageFull"_ns);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioMessagingResponse
 */
RadioMessagingResponse::RadioMessagingResponse(
  nsRadioProxyServiceManager& parent)
  : parent_Message(parent)
{}

ndk::ScopedAStatus
RadioMessagingResponse::acknowledgeIncomingGsmSmsWithPduResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::acknowledgeLastIncomingCdmaSmsResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::acknowledgeLastIncomingGsmSmsResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  parent_Message.defaultResponse(rspInfo, u"ackSMS"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::acknowledgeRequest(int32_t in_serial)
{
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus
RadioMessagingResponse::deleteSmsOnRuimResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::deleteSmsOnSimResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::getCdmaBroadcastConfigResponse(
  const RadioResponseInfo& in_info,
  const std::vector<CdmaBroadcastSmsConfigInfo>& in_configs)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::getGsmBroadcastConfigResponse(
  const RadioResponseInfo& in_info,
  const std::vector<GsmBroadcastSmsConfigInfo>& in_configs)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::getSmscAddressResponse(const RadioResponseInfo& in_info,
                                               const std::string& in_smsc)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getSmscAddress"_ns,
    rspInfo.serial,
    parent_Message.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateSmscAddress(NS_ConvertUTF8toUTF16(in_smsc.c_str()));
  }
  parent_Message.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::reportSmsMemoryStatusResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::sendCdmaSmsExpectMoreResponse(
  const RadioResponseInfo& in_info,
  const SendSmsResult& in_sms)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::sendCdmaSmsResponse(const RadioResponseInfo& in_info,
                                            const SendSmsResult& sms)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"sendSMS"_ns,
    rspInfo.serial,
    parent_Message.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendCdmaSmsResponse error.");
  }

  parent_Message.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::sendImsSmsResponse(const RadioResponseInfo& in_info,
                                           const SendSmsResult& in_sms)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::sendSmsExpectMoreResponse(
  const RadioResponseInfo& in_info,
  const SendSmsResult& sms)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"sendSMS"_ns,
    rspInfo.serial,
    parent_Message.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSmsExpectMoreResponse error.");
  }

  parent_Message.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::sendSmsResponse(const RadioResponseInfo& in_info,
                                        const SendSmsResult& sms)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"sendSMS"_ns,
    rspInfo.serial,
    parent_Message.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSmsResponse error.");
  }

  parent_Message.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::setCdmaBroadcastActivationResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::setCdmaBroadcastConfigResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::setGsmBroadcastActivationResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  parent_Message.defaultResponse(rspInfo, u"setGsmBroadcastActivation"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::setGsmBroadcastConfigResponse(
  const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);

  parent_Message.defaultResponse(rspInfo, u"setGsmBroadcastConfig"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::setSmscAddressResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::writeSmsToRuimResponse(const RadioResponseInfo& in_info,
                                               int32_t in_index)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioMessagingResponse::writeSmsToSimResponse(const RadioResponseInfo& in_info,
                                              int32_t in_index)
{
  rspInfo = in_info;
  parent_Message.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MESSAGE);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioModemIndication
 */
RadioModemIndication::RadioModemIndication(nsRadioProxyServiceManager& parent)
  : parent_Modem(parent)
{}
ndk::ScopedAStatus
RadioModemIndication::hardwareConfigChanged(
  RadioIndicationType in_type,
  const std::vector<HardwareConfig>& configs)
{
  DEBUG("hardwareConfigChanged");
  parent_Modem.sendIndAck(in_type, SERVICE_TYPE::MODEM);

  nsString rilmessageType(u"hardwareConfigChanged"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);

  uint32_t numConfigs = configs.size();
  nsTArray<RefPtr<nsHardwareConfig>> aHWConfigLists(numConfigs);

  for (uint32_t i = 0; i < numConfigs; i++) {
    int32_t type = parent_Modem.convertHardwareConfigType(configs[i].type);
    RefPtr<nsHardwareConfig> hwConfig = nullptr;
    RefPtr<nsHardwareConfigModem> hwConfigModem = nullptr;
    RefPtr<nsHardwareConfigSim> hwConfigSim = nullptr;

    if (type == nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM) {
      hwConfigModem =
        new nsHardwareConfigModem(configs[i].modem[0].rilModel,
                                  (int32_t)configs[i].modem[0].rat,
                                  configs[i].modem[0].maxVoiceCalls,
                                  configs[i].modem[0].maxDataCalls,
                                  configs[i].modem[0].maxStandby);
    } else {
      hwConfigSim = new nsHardwareConfigSim(
        NS_ConvertUTF8toUTF16(configs[i].sim[0].modemUuid.c_str()));
    }

    hwConfig = new nsHardwareConfig(
      type,
      NS_ConvertUTF8toUTF16(configs[i].uuid.c_str()),
      parent_Modem.convertHardwareConfigState(configs[i].state),
      hwConfigModem,
      hwConfigSim);
    aHWConfigLists.AppendElement(hwConfig);
  }
  result->updateHardwareConfigChanged(aHWConfigLists);
  parent_Modem.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemIndication::modemReset(RadioIndicationType in_type,
                                 const std::string& reason)
{
  DEBUG("modemReset");
  parent_Modem.sendIndAck(in_type, SERVICE_TYPE::MODEM);

  nsString rilmessageType(u"modemReset"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateModemReset(NS_ConvertUTF8toUTF16(reason.c_str()));
  parent_Modem.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemIndication::radioCapabilityIndication(RadioIndicationType in_type,
                                                const RadioCapability& rc)
{
  DEBUG("radioCapabilityIndication");
  parent_Modem.sendIndAck(in_type, SERVICE_TYPE::MODEM);

  nsString rilmessageType(u"radioCapabilityIndication"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsRadioCapability> capability = new nsRadioCapability(
    rc.session,
    parent_Modem.convertRadioCapabilityPhase(rc.phase),
    parent_Modem.convertRadioAccessFamily(RadioAccessFamily(rc.raf)),
    NS_ConvertUTF8toUTF16(rc.logicalModemUuid.c_str()),
    parent_Modem.convertRadioCapabilityStatus(rc.status));
  result->updateRadioCapabilityIndication(capability);
  parent_Modem.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemIndication::radioStateChanged(RadioIndicationType in_type,
                                        RadioState in_radioState)
{
  DEBUG("radioStateChanged");
  parent_Modem.sendIndAck(in_type, SERVICE_TYPE::MODEM);

  nsString rilmessageType(u"radiostatechange"_ns);
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateRadioStateChanged(
    parent_Modem.convertRadioStateToNum(in_radioState));
  parent_Modem.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemIndication::rilConnected(RadioIndicationType in_type)
{
  parent_Modem.sendIndAck(in_type, SERVICE_TYPE::MODEM);
  parent_Modem.defaultResponse(in_type, u"rilconnected"_ns);
  return ndk::ScopedAStatus::ok();
}

/*
 * Implementation of RadioModemResponse
 */
RadioModemResponse::RadioModemResponse(nsRadioProxyServiceManager& parent)
  : parent_Modem(parent)
{}
ndk::ScopedAStatus
RadioModemResponse::acknowledgeRequest(int32_t in_serial)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::enableModemResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  parent_Modem.defaultResponse(rspInfo, u"enableModem"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getBasebandVersionResponse(const RadioResponseInfo& in_info,
                                               const std::string& version)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getBasebandVersion"_ns,
                            rspInfo.serial,
                            parent_Modem.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateBasebandVersion(NS_ConvertUTF8toUTF16(version.c_str()));
  }
  parent_Modem.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getDeviceIdentityResponse(const RadioResponseInfo& in_info,
                                              const std::string& imei,
                                              const std::string& imeisv,
                                              const std::string& esn,
                                              const std::string& meid)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDeviceIdentity"_ns,
                            rspInfo.serial,
                            parent_Modem.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateDeviceIdentity(NS_ConvertUTF8toUTF16(imei.c_str()),
                                 NS_ConvertUTF8toUTF16(imeisv.c_str()),
                                 NS_ConvertUTF8toUTF16(esn.c_str()),
                                 NS_ConvertUTF8toUTF16(meid.c_str()));
  }
  parent_Modem.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getHardwareConfigResponse(
  const RadioResponseInfo& in_info,
  const std::vector<HardwareConfig>& in_config)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getModemActivityInfoResponse(
  const RadioResponseInfo& in_info,
  const ActivityStatsInfo& in_activityInfo)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getModemStackStatusResponse(
  const RadioResponseInfo& in_info,
  bool isEnabled)
{
  DEBUG("getModemStackStatusResponse");
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getModemStackStatus"_ns,
                            rspInfo.serial,
                            parent_Modem.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateModemStackStatusReponse(isEnabled);
  }
  parent_Modem.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::getRadioCapabilityResponse(const RadioResponseInfo& in_info,
                                               const RadioCapability& rc)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getRadioCapability"_ns,
                            rspInfo.serial,
                            parent_Modem.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsRadioCapability> radioCapability =
      new nsRadioCapability(rc.session,
                            static_cast<int32_t>(rc.phase),
                            rc.raf,
                            NS_ConvertUTF8toUTF16(rc.logicalModemUuid.c_str()),
                            static_cast<int32_t>(rc.status));
    result->updateRadioCapability(radioCapability);
  } else {
    DEBUG("getRadioCapability error.");
  }

  parent_Modem.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::nvReadItemResponse(const RadioResponseInfo& in_info,
                                       const std::string& in_result)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::nvResetConfigResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::nvWriteCdmaPrlResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::nvWriteItemResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::requestShutdownResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::sendDeviceStateResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::setRadioCapabilityResponse(const RadioResponseInfo& in_info,
                                               const RadioCapability& in_rc)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioModemResponse::setRadioPowerResponse(const RadioResponseInfo& in_info)
{
  rspInfo = in_info;
  parent_Modem.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::MODEM);
  parent_Modem.defaultResponse(rspInfo, u"setRadioEnabled"_ns);
  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioNetworkIndication
 */
RadioNetworkIndication::RadioNetworkIndication(
  nsRadioProxyServiceManager& parent)
  : parent_network(parent)
{}

ndk::ScopedAStatus
RadioNetworkIndication::barringInfoChanged(
  RadioIndicationType type,
  const CellIdentity_aidl& cellIdentity,
  const std::vector<BarringInfo_aidl>& barringInfos)
{
  DEBUG("barringInfoChanged");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"barringInfoChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsCellIdentity> rilCellIdentity =
    parent_network.convertCellIdentity(&cellIdentity);
  nsTArray<RefPtr<nsBarringInfo>> barrInfos;
  int32_t factor = -1;
  int32_t timeSeconds = -1;
  bool isBarred = false;
  int32_t serviceType, barringType;
  uint32_t size = barringInfos.size();
  for (uint32_t i = 0; i < size; i++) {
    serviceType = (int32_t)barringInfos[i].serviceType;
    barringType = (int32_t)barringInfos[i].barringType;
    if (barringInfos[i].barringTypeSpecificInfo.has_value()) {
      factor = barringInfos[i].barringTypeSpecificInfo.value().factor;
      timeSeconds = barringInfos[i].barringTypeSpecificInfo.value().timeSeconds;
      isBarred = barringInfos[i].barringTypeSpecificInfo.value().isBarred;
    }
    RefPtr<nsBarringInfo> barrinfo = new nsBarringInfo(
      serviceType, barringType, factor, timeSeconds, isBarred);
    barrInfos.AppendElement(barrinfo);
  }

  RefPtr<nsBarringInfoChanged> barrInfoEvent =
    new nsBarringInfoChanged(rilCellIdentity, barrInfos);
  result->updateBarringInfoChanged(barrInfoEvent);
  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::cdmaPrlChanged(RadioIndicationType type,
                                       int32_t version)
{
  DEBUG("Not implement cdmaPrlChanged");
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::cellInfoList(RadioIndicationType type,
                                     const std::vector<CellInfo_aidl>& records)
{
  DEBUG("cellInfoList");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"cellInfoList"_ns);
  uint32_t numCellInfo = records.size();
  DEBUG("cellInfoList numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      parent_network.convertRilCellInfo(&records[i]);
    aCellInfoLists.AppendElement(cellInfo);
  }
  result->updateCellInfoList(aCellInfoLists);

  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::currentLinkCapacityEstimate(
  RadioIndicationType type,
  const LinkCapacityEstimate_aidl& lce)
{
  DEBUG("currentLinkCapacityEstimate");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"currentLinkCapacityEstimate"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsLinkCapacityEstimate> linkCapacityEstimate =
    new nsLinkCapacityEstimate(lce.downlinkCapacityKbps,
                               lce.uplinkCapacityKbps,
                               lce.secondaryDownlinkCapacityKbps,
                               lce.secondaryUplinkCapacityKbps);
  result->updateCurrentLinkCapacityEstimate(linkCapacityEstimate);

  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::currentPhysicalChannelConfigs(
  RadioIndicationType type,
  const std::vector<PhysicalChannelConfig_aidl>& configs)
{
  DEBUG("currentPhysicalChannelConfigs");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

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
    int32_t ran_discriminator = -1;

    aidl::android::hardware::radio::network::PhysicalChannelConfigBand::Tag
      ran_tag = configs[i].band.getTag();
    if (ran_tag == aidl::android::hardware::radio::network::
                     PhysicalChannelConfigBand::Tag::geranBand) {
      geranBand = (int32_t)configs[i]
                    .band.get<aidl::android::hardware::radio::network::
                                PhysicalChannelConfigBand::Tag::geranBand>();
      ran_discriminator = 0;
    } else if (ran_tag == aidl::android::hardware::radio::network::
                            PhysicalChannelConfigBand::Tag::utranBand) {
      utranBand = (int32_t)configs[i]
                    .band.get<aidl::android::hardware::radio::network::
                                PhysicalChannelConfigBand::Tag::utranBand>();
      ran_discriminator = 1;
    } else if (ran_tag == aidl::android::hardware::radio::network::
                            PhysicalChannelConfigBand::Tag::eutranBand) {
      eutranBand = (int32_t)configs[i]
                     .band.get<aidl::android::hardware::radio::network::
                                 PhysicalChannelConfigBand::Tag::eutranBand>();
      ran_discriminator = 2;
    } else if (ran_tag == aidl::android::hardware::radio::network::
                            PhysicalChannelConfigBand::Tag::ngranBand) {
      ngranBand = (int32_t)configs[i]
                    .band.get<aidl::android::hardware::radio::network::
                                PhysicalChannelConfigBand::Tag::ngranBand>();
      ran_discriminator = 3;
    }
    RefPtr<nsPhysicalChannelConfig> config = new nsPhysicalChannelConfig(
      parent_network.convertConnectionStatus(configs[i].status),
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

  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::currentSignalStrength(
  RadioIndicationType type,
  const SignalStrength_aidl& aSignalStrength)
{
  DEBUG("currentSignalStrength");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"signalstrengthchange"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSignalStrength> signalStrength =
    parent_network.convertSignalStrength(aSignalStrength);
  result->updateCurrentSignalStrength(signalStrength);
  parent_network.mRIL->sendRilIndicationResult(result);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::imsNetworkStateChanged(RadioIndicationType type)
{
  parent_network.defaultResponse(type, u"imsNetworkStateChanged"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::networkScanResult(RadioIndicationType type,
                                          const NetworkScanResult_aidl& aResult)
{
  DEBUG("networkScanResult");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(u"networkScanResult"_ns);

  uint32_t numCellInfo = aResult.networkInfos.size();
  DEBUG("networkScanResult numCellInfo= %d", numCellInfo);
  nsTArray<RefPtr<nsRilCellInfo>> networkInfos;

  for (uint32_t i = 0; i < numCellInfo; i++) {
    RefPtr<nsRilCellInfo> cellInfo =
      parent_network.convertRilCellInfo(&aResult.networkInfos[i]);
    networkInfos.AppendElement(cellInfo);
  }
  RefPtr<nsNetworkScanResult> scanResult = new nsNetworkScanResult(
    (int32_t)(aResult.status), (int32_t)(aResult.error), networkInfos);
  result->updateScanResult(scanResult);

  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::networkStateChanged(RadioIndicationType type)
{
  parent_network.defaultResponse(type, u"networkStateChanged"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::nitzTimeReceived(RadioIndicationType type,
                                         const std::string& nitzTime,
                                         int64_t receivedTime,
                                         int64_t ageMs)
{
  DEBUG("nitzTimeReceived");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"nitzTimeReceived"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateNitzTimeReceived(NS_ConvertUTF8toUTF16(nitzTime.c_str()),
                                 receivedTime);
  parent_network.mRIL->sendRilIndicationResult(result);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::registrationFailed(
  RadioIndicationType type,
  const CellIdentity_aidl& cellIdentity,
  const std::string& chosenPlmn,
  int32_t domain,
  int32_t causeCode,
  int32_t additionalCauseCode)
{
  DEBUG("registrationFailed");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"registrationFailed"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsCellIdentity> rilCellIdentity =
    parent_network.convertCellIdentity(&cellIdentity);
  RefPtr<nsRegistrationFailedEvent> regFailedEvent =
    new nsRegistrationFailedEvent(rilCellIdentity,
                                  NS_ConvertUTF8toUTF16(chosenPlmn.c_str()),
                                  (int32_t)domain,
                                  causeCode,
                                  additionalCauseCode);
  result->updateRegistrationFailed(regFailedEvent);
  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::restrictedStateChanged(RadioIndicationType type,
                                               const PhoneRestrictedState state)
{
  DEBUG("restrictedStateChanged");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"restrictedStateChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateRestrictedStateChanged(
    parent_network.convertPhoneRestrictedState(state));
  parent_network.mRIL->sendRilIndicationResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::suppSvcNotify(RadioIndicationType type,
                                      const SuppSvcNotification_aidl& suppSvc)
{
  DEBUG("suppSvcNotification");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

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
  parent_network.mRIL->sendRilIndicationResult(result);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkIndication::voiceRadioTechChanged(RadioIndicationType type,
                                              const RadioTechnology rat)
{
  DEBUG("voiceRadioTechChanged");
  parent_network.sendIndAck(type, SERVICE_TYPE::NETWORK);

  nsString rilmessageType(u"voiceRadioTechChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateVoiceRadioTechChanged((int32_t)rat);
  parent_network.mRIL->sendRilIndicationResult(result);

  return ndk::ScopedAStatus::ok();
}

/**
 * Implementation of RadioNetworkResponse
 */
RadioNetworkResponse::RadioNetworkResponse(nsRadioProxyServiceManager& parent)
  : parent_network(parent)
{}

ndk::ScopedAStatus RadioNetworkResponse::acknowledgeRequest(int32_t /*serial*/)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getAllowedNetworkTypesBitmapResponse(
  const RadioResponseInfo& info,
  int32_t networkTypeBitmap)
{
  DEBUG("getAllowedNetworkTypesBitmapResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getAllowedNetworkTypesBitmap"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateAllowedNetworkTypesBitmask(
      parent_network.convertHalNetworkTypeBitMask(networkTypeBitmap));
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getAvailableBandModesResponse(
  const RadioResponseInfo& info,
  const std::vector<RadioBandMode>& bandModes)
{
  DEBUG("getAvailableBandModesResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getAvailableNetworksResponse(
  const RadioResponseInfo& info,
  const std::vector<OperatorInfo_aidl>& networkInfos)
{
  DEBUG("getAvailableNetworksResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getAvailableNetworks"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numNetworks = networkInfos.size();
    DEBUG("getAvailableNetworks numNetworks= %d", numNetworks);
    nsTArray<RefPtr<nsOperatorInfo>> aNetworks(numNetworks);

    for (uint32_t i = 0; i < numNetworks; i++) {
      RefPtr<nsOperatorInfo> network = new nsOperatorInfo(
        NS_ConvertUTF8toUTF16(networkInfos[i].alphaLong.c_str()),
        NS_ConvertUTF8toUTF16(networkInfos[i].alphaShort.c_str()),
        NS_ConvertUTF8toUTF16(networkInfos[i].operatorNumeric.c_str()),
        parent_network.convertOperatorState((int32_t)networkInfos[i].status));
      aNetworks.AppendElement(network);
    }
    result->updateAvailableNetworks(aNetworks);
  } else {
    DEBUG("getAvailableNetworksResponse error.");
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getBarringInfoResponse(
  const RadioResponseInfo& info,
  const CellIdentity_aidl& cellIdentity,
  const std::vector<BarringInfo_aidl>& barringInfos)
{
  DEBUG("getBarringInfoResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getBarringInfo"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {

    nsTArray<RefPtr<nsIBarringInfo>> data;
    int32_t factor = -1;
    int32_t timeSeconds = -1;
    bool isBarred = false;
    for (uint32_t i = 0; i < barringInfos.size(); i++) {
      if (barringInfos[i].barringTypeSpecificInfo.has_value()) {
        factor = barringInfos[i].barringTypeSpecificInfo.value().factor;
        timeSeconds =
          barringInfos[i].barringTypeSpecificInfo.value().timeSeconds;
        isBarred = barringInfos[i].barringTypeSpecificInfo.value().isBarred;
      }
      RefPtr<nsIBarringInfo> info =
        new nsBarringInfo(barringInfos[i].serviceType,
                          barringInfos[i].barringType,
                          factor,
                          timeSeconds,
                          isBarred);
      data.AppendElement(info);
    }

    RefPtr<nsCellIdentity> rv =
      parent_network.convertCellIdentity(&cellIdentity);

    result->updateGetBarringInfoResult(new nsGetBarringInfoResult(rv, data));
  } else {
    DEBUG("getBarringInfoResponse error.");
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getCdmaRoamingPreferenceResponse(
  const RadioResponseInfo& info,
  const CdmaRoamingType type)
{
  DEBUG("getCdmaRoamingPreferenceResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getCellInfoListResponse(
  const RadioResponseInfo& info,
  const std::vector<CellInfo_aidl>& cellInfo)
{
  DEBUG("getCellInfoListResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getCellInfoList"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell =
        parent_network.convertRilCellInfo(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse error.");
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getDataRegistrationStateResponse(
  const RadioResponseInfo& info,
  const RegStateResult_aidl& dataRegResponse)
{
  DEBUG("getDataRegistrationStateResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getDataRegistrationState"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      parent_network.convertCellIdentity(&dataRegResponse.cellIdentity);

    RefPtr<nsNrIndicators> nrIndicators = nullptr;
    RefPtr<nsLteVopsInfo> lteVopsInfo = nullptr;
    RefPtr<nsNrVopsInfo> nrVopsInfo = nullptr;
    if (dataRegResponse.accessTechnologySpecificInfo.getTag() ==
        aidl::android::hardware::radio::network::AccessTechnologySpecificInfo::
          Tag::eutranInfo) {
      nrIndicators = parent_network.convertNrIndicators(
        &dataRegResponse.accessTechnologySpecificInfo
           .get<aidl::android::hardware::radio::network::
                  AccessTechnologySpecificInfo::Tag::eutranInfo>()
           .nrIndicators);
      lteVopsInfo = parent_network.convertVopsInfo(
        &dataRegResponse.accessTechnologySpecificInfo
           .get<aidl::android::hardware::radio::network::
                  AccessTechnologySpecificInfo::Tag::eutranInfo>()
           .lteVopsInfo);
    } else if (dataRegResponse.accessTechnologySpecificInfo.getTag() ==
               aidl::android::hardware::radio::network::
                 AccessTechnologySpecificInfo::Tag::ngranNrVopsInfo) {
      nrVopsInfo = parent_network.convertNrVopsInfo(
        &dataRegResponse.accessTechnologySpecificInfo
           .get<aidl::android::hardware::radio::network::
                  AccessTechnologySpecificInfo::Tag::ngranNrVopsInfo>());
    }

    RefPtr<nsDataRegState> dataRegState = new nsDataRegState(
      parent_network.convertRegState(dataRegResponse.regState),
      (int32_t)dataRegResponse.rat,
      (int32_t)dataRegResponse.reasonForDenial,
      nsDataRegState::MAX_DATA_CALLS,
      cellIdentity,
      lteVopsInfo,
      nrIndicators,
      NS_ConvertUTF8toUTF16(dataRegResponse.registeredPlmn.c_str()),
      nrVopsInfo);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getImsRegistrationStateResponse(
  const RadioResponseInfo& info,
  bool isRegistered,
  const RadioTechnologyFamily ratFamily)
{
  DEBUG("getImsRegistrationStateResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getNetworkSelectionModeResponse(
  const RadioResponseInfo& info,
  bool manual)
{
  DEBUG("getNetworkSelectionModeResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getNetworkSelectionMode"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  result->updateNetworkSelectionMode(manual);
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getOperatorResponse(const RadioResponseInfo& info,
                                          const std::string& longName,
                                          const std::string& shortName,
                                          const std::string& numeric)
{
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getOperator"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsOperatorInfo> operatorInfo =
      new nsOperatorInfo(NS_ConvertUTF8toUTF16(longName.c_str()),
                         NS_ConvertUTF8toUTF16(shortName.c_str()),
                         NS_ConvertUTF8toUTF16(numeric.c_str()),
                         0);
    result->updateOperator(operatorInfo);
  } else {
    DEBUG("getOperator error.");
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getSignalStrengthResponse(
  const RadioResponseInfo& info,
  const SignalStrength_aidl& signalStrength)
{
  DEBUG("getSignalStrengthResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getSignalStrength"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSignalStrength> ss =
      parent_network.convertSignalStrength(signalStrength);
    result->updateSignalStrength(ss);
  } else {
    DEBUG("getSignalStrength error.");
  }

  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getSystemSelectionChannelsResponse(
  const RadioResponseInfo& info,
  const std::vector<RadioAccessSpecifier_aidl>& specifiers)
{
  DEBUG("getSystemSelectionChannelsResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  nsTArray<RefPtr<nsIRadioAccessSpecifier>> specifiersArray;
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getSystemSelectionChannels"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    for (uint32_t i = 0; i < specifiers.size(); i++) {

      int32_t radioAccessNetwork;
      nsTArray<int32_t> geranBands;
      nsTArray<int32_t> utranBands;
      nsTArray<int32_t> eutranBands;
      nsTArray<int32_t> ngranBands;
      nsTArray<int32_t> channels;

      radioAccessNetwork = (int32_t)specifiers[i].accessNetwork;
      switch (specifiers[i].bands.getTag()) {
        case aidl::android::hardware::radio::network::
          RadioAccessSpecifierBands::Tag::geranBands:
          for (uint32_t i = 0;
               i < specifiers[i]
                     .bands
                     .get<aidl::android::hardware::radio::network::
                            RadioAccessSpecifierBands::Tag::geranBands>()
                     .size();
               i++) {
            geranBands[i] =
              (int32_t)specifiers[i]
                .bands.get<aidl::android::hardware::radio::network::
                             RadioAccessSpecifierBands::Tag::geranBands>()[i];
          }
          break;
        case aidl::android::hardware::radio::network::
          RadioAccessSpecifierBands::Tag::utranBands:
          for (uint32_t i = 0;
               i < specifiers[i]
                     .bands
                     .get<aidl::android::hardware::radio::network::
                            RadioAccessSpecifierBands::Tag::utranBands>()
                     .size();
               i++) {
            utranBands[i] =
              (int32_t)specifiers[i]
                .bands.get<aidl::android::hardware::radio::network::
                             RadioAccessSpecifierBands::Tag::utranBands>()[i];
          }
          break;
        case aidl::android::hardware::radio::network::
          RadioAccessSpecifierBands::Tag::eutranBands:
          for (uint32_t i = 0;
               i < specifiers[i]
                     .bands
                     .get<aidl::android::hardware::radio::network::
                            RadioAccessSpecifierBands::Tag::eutranBands>()
                     .size();
               i++) {
            eutranBands[i] =
              (int32_t)specifiers[i]
                .bands.get<aidl::android::hardware::radio::network::
                             RadioAccessSpecifierBands::Tag::eutranBands>()[i];
          }
          break;
        case aidl::android::hardware::radio::network::
          RadioAccessSpecifierBands::Tag::ngranBands:
          for (uint32_t i = 0;
               i < specifiers[i]
                     .bands
                     .get<aidl::android::hardware::radio::network::
                            RadioAccessSpecifierBands::Tag::ngranBands>()
                     .size();
               i++) {
            ngranBands[i] =
              (int32_t)specifiers[i]
                .bands.get<aidl::android::hardware::radio::network::
                             RadioAccessSpecifierBands::Tag::ngranBands>()[i];
          }
          break;
        default:
          break;
      }

      for (uint32_t j = 0; i < specifiers[i].channels.size(); j++) {
        channels.AppendElement(specifiers[i].channels[j]);
      }

      RefPtr<nsIRadioAccessSpecifier> specifier =
        new nsRadioAccessSpecifier(radioAccessNetwork,
                                   geranBands.Clone(),
                                   utranBands.Clone(),
                                   eutranBands.Clone(),
                                   ngranBands.Clone(),
                                   channels.Clone());
      specifiersArray.AppendElement(specifier);
    }
    result->updateRadioAccessSpecifiers(specifiersArray);
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getVoiceRadioTechnologyResponse(
  const RadioResponseInfo& info,
  const RadioTechnology rat)
{
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getVoiceRadioTechnology"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateVoiceRadioTechnology(
      parent_network.convertRadioTechnology(rat));
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getVoiceRegistrationStateResponse(
  const RadioResponseInfo& info,
  const RegStateResult_aidl& voiceRegResponse)
{
  DEBUG("getVoiceRegistrationStateResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getVoiceRegistrationState"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      parent_network.convertCellIdentity(&voiceRegResponse.cellIdentity);
    bool cssSupported = false;
    int32_t roamingIndicator = 0;
    int32_t systemIsInPrl = 0;
    int32_t defaultRoamingIndicator = 0;

    if (voiceRegResponse.accessTechnologySpecificInfo.getTag() ==
        aidl::android::hardware::radio::network::AccessTechnologySpecificInfo::
          Tag::cdmaInfo) {
      cssSupported = voiceRegResponse.accessTechnologySpecificInfo
                       .get<aidl::android::hardware::radio::network::
                              AccessTechnologySpecificInfo::Tag::cdmaInfo>()
                       .cssSupported;
      roamingIndicator = voiceRegResponse.accessTechnologySpecificInfo
                           .get<aidl::android::hardware::radio::network::
                                  AccessTechnologySpecificInfo::Tag::cdmaInfo>()
                           .roamingIndicator;
      systemIsInPrl = (int32_t)voiceRegResponse.accessTechnologySpecificInfo
                        .get<aidl::android::hardware::radio::network::
                               AccessTechnologySpecificInfo::Tag::cdmaInfo>()
                        .systemIsInPrl;
      defaultRoamingIndicator =
        voiceRegResponse.accessTechnologySpecificInfo
          .get<aidl::android::hardware::radio::network::
                 AccessTechnologySpecificInfo::Tag::cdmaInfo>()
          .defaultRoamingIndicator;
    } else if (voiceRegResponse.accessTechnologySpecificInfo.getTag() ==
               aidl::android::hardware::radio::network::
                 AccessTechnologySpecificInfo::Tag::geranDtmSupported) {
      cssSupported =
        voiceRegResponse.accessTechnologySpecificInfo
          .get<aidl::android::hardware::radio::network::
                 AccessTechnologySpecificInfo::Tag::geranDtmSupported>();
    }
    RefPtr<nsVoiceRegState> voiceRegState = new nsVoiceRegState(
      parent_network.convertRegState(voiceRegResponse.regState),
      (int32_t)voiceRegResponse.rat,
      cssSupported,
      roamingIndicator,
      systemIsInPrl,
      defaultRoamingIndicator,
      (int32_t)voiceRegResponse.reasonForDenial,
      cellIdentity,
      NS_ConvertUTF8toUTF16(voiceRegResponse.registeredPlmn.c_str()));
    result->updateVoiceRegStatus(voiceRegState);
  } else {
    DEBUG("getVoiceRegistrationState error.");
  }

  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::isNrDualConnectivityEnabledResponse(
  const RadioResponseInfo& info,
  bool isEnabled)
{
  DEBUG("isNrDualConnectivityEnabledResponse");

  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"areUiccApplicationsEnabled"_ns,
    rspInfo.serial,
    parent_network.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateUiccAppEnabledReponse(isEnabled);
  }
  parent_network.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setAllowedNetworkTypesBitmapResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setAllowedNetworkTypesBitmapResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setPreferredNetworkTypeBitmap"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setBandModeResponse(const RadioResponseInfo& info)
{
  DEBUG("setBandModeResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setBarringPasswordResponse(const RadioResponseInfo& info)
{
  DEBUG("setBarringPasswordResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"changeCallBarringPassword"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setCdmaRoamingPreferenceResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setCdmaRoamingPreferenceResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setCellInfoListRateResponse(const RadioResponseInfo& info)
{
  DEBUG("setCellInfoListRateResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setCellInfoListRate"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setIndicationFilterResponse(const RadioResponseInfo& info)
{
  DEBUG("setIndicationFilterResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setLinkCapacityReportingCriteriaResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setLinkCapacityReportingCriteriaResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo,
                                 u"setLinkCapacityReportingCriteria"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setLocationUpdatesResponse(const RadioResponseInfo& info)
{
  DEBUG("setLocationUpdatesResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setNetworkSelectionModeAutomaticResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setNetworkSelectionModeAutomaticResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"selectNetworkAuto"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setNetworkSelectionModeManualResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setNetworkSelectionModeManualResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setNrDualConnectivityStateResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setNrDualConnectivityStateResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setSignalStrengthReportingCriteriaResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setSignalStrengthReportingCriteriaResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo,
                                 u"setSignalStrengthReportingCriteria"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setSuppServiceNotificationsResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setSuppServiceNotificationsResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setSuppServiceNotifications"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setSystemSelectionChannelsResponse(
  const RadioResponseInfo& info)
{
  DEBUG("setSystemSelectionChannelsResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"setSystemSelectionChannels"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::startNetworkScanResponse(const RadioResponseInfo& info)
{
  DEBUG("startNetworkScanResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"startNetworkScan"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::stopNetworkScanResponse(const RadioResponseInfo& info)
{
  DEBUG("stopNetworkScanResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  parent_network.defaultResponse(rspInfo, u"stopNetworkScan"_ns);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::supplyNetworkDepersonalizationResponse(
  const RadioResponseInfo& info,
  int32_t remainingRetries)
{
  DEBUG("supplyNetworkDepersonalizationResponse");
  rspInfo = info;
  parent_network.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::NETWORK);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::setUsageSettingResponse(const RadioResponseInfo& info)
{
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus
RadioNetworkResponse::getUsageSettingResponse(const RadioResponseInfo& info,
                                              const UsageSetting usageSetting)
{
  return ndk::ScopedAStatus::ok();
}

nsRadioProxyServiceManager::nsRadioProxyServiceManager(int32_t clientId,
                                                       nsRilWorker* aRil)
{
  mClientId = clientId;
  mRIL = aRil;
  updateDebug();
  loadRadioModemProxy();
  loadRadioMessagingProxy();
  loadRadioDataProxy();
  loadRadioVoiceProxy();
  loadRadioNetworkProxy();
  loadRadioSimProxy();
  loadRadioConfigProxy();
}

nsRadioProxyServiceManager::~nsRadioProxyServiceManager() {}

void
nsRadioProxyServiceManager::updateDebug()
{
  gRilDebug_isLoggingEnabled =
    mozilla::Preferences::GetBool("ril.debugging.enabled", false);
}

void
nsRadioProxyServiceManager::sendAck(int32_t aToken, uint8_t aServiceType)
{
  DEBUG("sendAck %d", aToken);
  switch (aServiceType) {
    case SERVICE_TYPE::DATA:
      if (mRadioDataProxy.get()) {
        mRadioDataProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioDataProxy is null");
      }
      break;
    case SERVICE_TYPE::VOICE:
      if (mRadioVoiceProxy.get()) {
        mRadioVoiceProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioVoiceProxy is null");
      }
      break;
    case SERVICE_TYPE::MESSAGE:
      if (mRadioMessagingProxy.get()) {
        mRadioMessagingProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioMessagingProxy is null");
      }
      break;
    case SERVICE_TYPE::MODEM:
      if (mRadioMessagingProxy.get()) {
        mRadioMessagingProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioMessagingProxy is null");
      }
      break;
    case SERVICE_TYPE::NETWORK:
      if (mRadioNetworkProxy.get()) {
        mRadioNetworkProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioNetworkProxy is null");
      }
      break;
    case SERVICE_TYPE::SIM:
      if (mRadioSimProxy.get()) {
        mRadioSimProxy->responseAcknowledgement();
      } else {
        DEBUG("mRadioSimProxy is null");
      }
      break;
    default:
      break;
  }
}

void
nsRadioProxyServiceManager::onRadioDataBinderDied()
{
  DEBUG("RadioDataBinderDied died");
  mRadioDataProxy = nullptr;
  mRadioRsp_data = nullptr;
  mRadioInd_data = nullptr;
}

void
nsRadioProxyServiceManager::onRadioVoiceBinderDied()
{
  DEBUG("RadioDataBinderDied died");
  mRadioVoiceProxy = nullptr;
  mRadioRsp_voice = nullptr;
  mRadioInd_voice = nullptr;
}

void
nsRadioProxyServiceManager::sendRspAck(const RadioResponseInfo& rspInfo,
                                       int32_t aToken,
                                       uint8_t aServiceType)
{
  DEBUG("sendAck %d", aToken);
  if (rspInfo.type != RadioResponseType::SOLICITED_ACK_EXP)
    return;
  sendAck(aToken, aServiceType);
}

void
nsRadioProxyServiceManager::sendIndAck(const RadioIndicationType& type,
                                       uint8_t aServiceType)
{
  DEBUG("sendIndAck with type %d", (int)type);
  if (type != RadioIndicationType::UNSOLICITED_ACK_EXP)
    return;
  sendAck(0, aServiceType);
}

void
nsRadioProxyServiceManager::onRadioConfigBinderDied()
{
  DEBUG("RadioConfigBinderDied died");
  mRadioConfigProxy = nullptr;
  mRadioRsp_config = nullptr;
  mRadioInd_config = nullptr;
}

void
nsRadioProxyServiceManager::onRadioSimBinderDied()
{
  DEBUG("onRadioSimBinderDied died");
  mRadioSimProxy = nullptr;
  mRadioRsp_sim = nullptr;
  mRadioInd_sim = nullptr;
}

NS_IMETHODIMP
nsRadioProxyServiceManager::AreUiccApplicationsEnabled(int32_t token)
{
  DEBUG("AreUiccApplicationsEnabled token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->areUiccApplicationsEnabled(token);
    if (ret.isOk()) {
      DEBUG("areUiccApplicationsEnabled successfully");
      return NS_OK;
    } else {
      DEBUG("areUiccApplicationsEnabled fail %s", ret.getMessage());
      ERROR_NS_OK("areUiccApplicationsEnabled failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ChangeIccPin2ForApp(int32_t token,
                                                const nsAString& oldPin,
                                                const nsAString& newPin,
                                                const nsAString& aid)
{
  DEBUG("ChangeIccPin2ForApp token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->changeIccPin2ForApp(token,
                                          NS_ConvertUTF16toUTF8(oldPin).get(),
                                          NS_ConvertUTF16toUTF8(newPin).get(),
                                          NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("changeIccPin2ForApp successfully");
      return NS_OK;
    } else {
      DEBUG("changeIccPin2ForApp fail %s", ret.getMessage());
      ERROR_NS_OK("changeIccPin2ForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ChangeIccPinForApp(int32_t token,
                                               const nsAString& oldPin,
                                               const nsAString& newPin,
                                               const nsAString& aid)
{
  DEBUG("ChangeIccPinForApp token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->changeIccPinForApp(token,
                                         NS_ConvertUTF16toUTF8(oldPin).get(),
                                         NS_ConvertUTF16toUTF8(newPin).get(),
                                         NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("changeIccPinForApp successfully");
      return NS_OK;
    } else {
      DEBUG("changeIccPinForApp fail %s", ret.getMessage());
      ERROR_NS_OK("changeIccPinForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::EnableUiccApplications(int32_t token, bool enabled)
{
  DEBUG("EnableUiccApplications token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->enableUiccApplications(token, enabled);
    if (ret.isOk()) {
      DEBUG("enableUiccApplications successfully");
      return NS_OK;
    } else {
      DEBUG("enableUiccApplications fail %s", ret.getMessage());
      ERROR_NS_OK("enableUiccApplications failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetAllowedCarriers(int32_t token)
{
  DEBUG("GetAllowedCarriers token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getAllowedCarriers(token);
    if (ret.isOk()) {
      DEBUG("getAllowedCarriers successfully");
      return NS_OK;
    } else {
      DEBUG("getAllowedCarriers fail %s", ret.getMessage());
      ERROR_NS_OK("getAllowedCarriers failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetCdmaSubscription(int32_t token)
{
  DEBUG("getCdmaSubscriGetCdmaSubscriptionption token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getCdmaSubscription(token);
    if (ret.isOk()) {
      DEBUG("getCdmaSubscription successfully");
      return NS_OK;
    } else {
      DEBUG("getCdmaSubscription fail %s", ret.getMessage());
      ERROR_NS_OK("getCdmaSubscription failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetFacilityLockForApp(int32_t token,
                                                  const nsAString& facility,
                                                  const nsAString& password,
                                                  int32_t serviceClass,
                                                  const nsAString& aid)
{
  DEBUG("GetFacilityLockForApp token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getFacilityLockForApp(
      token,
      NS_ConvertUTF16toUTF8(facility).get(),
      NS_ConvertUTF16toUTF8(password).get(),
      serviceClass,
      NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("getFacilityLockForApp successfully");
      return NS_OK;
    } else {
      DEBUG("getFacilityLockForApp fail %s", ret.getMessage());
      ERROR_NS_OK("getFacilityLockForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetIccCardStatus(int32_t token)
{
  DEBUG("GetIccCardStatus token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getIccCardStatus(token);
    if (ret.isOk()) {
      DEBUG("getIccCardStatus successfully");
      return NS_OK;
    } else {
      DEBUG("getIccCardStatus fail %s", ret.getMessage());
      ERROR_NS_OK("getIccCardStatus failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetImsiForApp(int32_t token, const nsAString& aid)
{
  DEBUG("GetImsiForApp token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->getImsiForApp(token, NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("getImsiForApp successfully");
      return NS_OK;
    } else {
      DEBUG("GetImsiForApp fail %s", ret.getMessage());
      ERROR_NS_OK("getImsiForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetIMSI(int32_t serial, const nsAString& aid)
{
  DEBUG("getIMSI token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->getImsiForApp(serial, NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("getIMSI successfully");
      return NS_OK;
    } else {
      DEBUG("getIMSI fail %s", ret.getMessage());
      ERROR_NS_OK("getIMSI failed");
    }
  }
  ERROR_NS_OK("mRadioSimProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSimPhonebookCapacity(int32_t token)
{
  DEBUG("GetSimPhonebookCapacity token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getSimPhonebookCapacity(token);
    if (ret.isOk()) {
      DEBUG("getSimPhonebookCapacity successfully");
      return NS_OK;
    } else {
      DEBUG("getSimPhonebookCapacity fail %s", ret.getMessage());
      ERROR_NS_OK("getSimPhonebookCapacity failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSimPhonebookRecords(int32_t token)
{
  DEBUG("GetSimPhonebookRecords token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->getSimPhonebookRecords(token);
    if (ret.isOk()) {
      DEBUG("getSimPhonebookRecords successfully");
      return NS_OK;
    } else {
      DEBUG("getSimPhonebookRecords fail %s", ret.getMessage());
      ERROR_NS_OK("getSimPhonebookRecords failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::IccCloseLogicalChannel(int32_t token,
                                                   int32_t channelId)
{
  DEBUG("IccCloseLogicalChannel token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->iccCloseLogicalChannel(token, channelId);
    if (ret.isOk()) {
      DEBUG("iccCloseLogicalChannel successfully");
      return NS_OK;
    } else {
      DEBUG("iccCloseLogicalChannel fail %s", ret.getMessage());
      ERROR_NS_OK("iccCloseLogicalChannel failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::IccIOForApp(int32_t token,
                                        int32_t command,
                                        int32_t fileId,
                                        const nsAString& path,
                                        int32_t p1,
                                        int32_t p2,
                                        int32_t p3,
                                        const nsAString& data,
                                        const nsAString& pin2,
                                        const nsAString& aid)
{
  DEBUG("IccIOForApp token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::aidl::android::hardware::radio::sim::IccIo iccIo;
    iccIo.command = command;
    iccIo.fileId = fileId;
    iccIo.path = NS_ConvertUTF16toUTF8(path).get();
    iccIo.p1 = p1;
    iccIo.p2 = p2;
    iccIo.p3 = p3;
    iccIo.data = NS_ConvertUTF16toUTF8(data).get();
    iccIo.pin2 = NS_ConvertUTF16toUTF8(pin2).get();
    iccIo.aid = NS_ConvertUTF16toUTF8(aid).get();

    ::ndk::ScopedAStatus ret = mRadioSimProxy->iccIoForApp(token, iccIo);
    if (ret.isOk()) {
      DEBUG("iccIOForApp successfully");
      return NS_OK;
    } else {
      DEBUG("iccIOForApp fail %s", ret.getMessage());
      ERROR_NS_OK("iccIOForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::IccOpenLogicalChannel(int32_t token,
                                                  const nsAString& aid,
                                                  int32_t pin2)
{
  DEBUG("IccOpenLogicalChannel token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {

    ::ndk::ScopedAStatus ret = mRadioSimProxy->iccOpenLogicalChannel(
      token, NS_ConvertUTF16toUTF8(aid).get(), pin2);
    if (ret.isOk()) {
      DEBUG("iccOpenLogicalChannel successfully");
      return NS_OK;
    } else {
      DEBUG("iccOpenLogicalChannel fail %s", ret.getMessage());
      ERROR_NS_OK("iccOpenLogicalChannel failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ReportStkServiceIsRunning(int32_t token)
{
  DEBUG("ReportStkServiceIsRunning token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->reportStkServiceIsRunning(token);
    if (ret.isOk()) {
      DEBUG("reportStkServiceIsRunning successfully");
      return NS_OK;
    } else {
      DEBUG("reportStkServiceIsRunning fail %s", ret.getMessage());
      ERROR_NS_OK("reportStkServiceIsRunning failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::RequestIccSimAuthentication(int32_t token,
                                                        int32_t authContext,
                                                        const nsAString& data,
                                                        const nsAString& aid)
{
  DEBUG("RequestIccSimAuthentication token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->requestIccSimAuthentication(
      token,
      authContext,
      NS_ConvertUTF16toUTF8(data).get(),
      NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("RequestIccSimAuthentication successfully");
      return NS_OK;
    } else {
      DEBUG("RequestIccSimAuthentication fail %s", ret.getMessage());
      ERROR_NS_OK("RequestIccSimAuthentication failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendEnvelope(int32_t token,
                                         const nsAString& contents)
{
  DEBUG("SendEnvelope token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->sendEnvelope(
      token, NS_ConvertUTF16toUTF8(contents).get());
    if (ret.isOk()) {
      DEBUG("SendEnvelope successfully");
      return NS_OK;
    } else {
      DEBUG("SendEnvelope fail %s", ret.getMessage());
      ERROR_NS_OK("SendEnvelope failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendEnvelopeWithStatus(int32_t token,
                                                   const nsAString& contents)
{
  DEBUG("sendEnvelopeWithStatus token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->sendEnvelopeWithStatus(
      token, NS_ConvertUTF16toUTF8(contents).get());
    if (ret.isOk()) {
      DEBUG("SendEnvelopeWithStatus successfully");
      return NS_OK;
    } else {
      DEBUG("SendEnvelopeWithStatus fail %s", ret.getMessage());
      ERROR_NS_OK("SendEnvelopeWithStatus failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendTerminalResponseToSim(int32_t token,
                                                      const nsAString& contents)
{
  DEBUG("SendTerminalResponseToSim token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->sendTerminalResponseToSim(
      token, NS_ConvertUTF16toUTF8(contents).get());
    if (ret.isOk()) {
      DEBUG("SendTerminalResponseToSim successfully");
      return NS_OK;
    } else {
      DEBUG("SendTerminalResponseToSim fail %s", ret.getMessage());
      ERROR_NS_OK("SendTerminalResponseToSim failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetAllowedCarriers(
  int32_t token,
  nsICarrierRestrictionsWithPriority* carriers,
  uint32_t multiSimPolicy)
{
  DEBUG("SetAllowedCarriers token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    aidl::android::hardware::radio::sim::CarrierRestrictions crp;
    nsTArray<RefPtr<nsICarrier>> allowedCarriers;
    carriers->GetAllowedCarriers(allowedCarriers);
    crp.allowedCarriers.resize(allowedCarriers.Length());
    for (uint32_t i = 0; i < allowedCarriers.Length(); i++) {

      aidl::android::hardware::radio::sim::Carrier cr;
      nsString mcc;
      allowedCarriers[i]->GetMcc(mcc);
      cr.mcc = NS_ConvertUTF16toUTF8(mcc).get();

      nsString mnc;
      allowedCarriers[i]->GetMnc(mnc);
      cr.mnc = NS_ConvertUTF16toUTF8(mnc).get();
      uint8_t matchType;
      allowedCarriers[i]->GetMatchType(&matchType);
      cr.matchType = matchType;

      nsString matchData;
      allowedCarriers[i]->GetMatchData(matchData);
      cr.matchData = NS_ConvertUTF16toUTF8(matchData).get();

      crp.allowedCarriers[i] = cr;
    }

    nsTArray<RefPtr<nsICarrier>> excludedCarriers;
    carriers->GetExcludedCarriers(excludedCarriers);
    crp.excludedCarriers.resize(excludedCarriers.Length());
    for (uint32_t i = 0; i < excludedCarriers.Length(); i++) {
      aidl::android::hardware::radio::sim::Carrier cr;
      nsString mcc;
      excludedCarriers[i]->GetMcc(mcc);
      cr.mcc = NS_ConvertUTF16toUTF8(mcc).get();

      nsString mnc;
      excludedCarriers[i]->GetMnc(mnc);
      cr.mnc = NS_ConvertUTF16toUTF8(mnc).get();

      uint8_t matchType;
      excludedCarriers[i]->GetMatchType(&matchType);
      cr.matchType = matchType;

      nsString matchData;
      excludedCarriers[i]->GetMatchData(matchData);
      cr.matchData = NS_ConvertUTF16toUTF8(matchData).get();

      crp.excludedCarriers[i] = cr;
    }

    bool allowedCarriersPrioritized;
    carriers->GetAllowedCarriersPrioritized(&allowedCarriersPrioritized);
    crp.allowedCarriersPrioritized = allowedCarriersPrioritized;

    ::ndk::ScopedAStatus ret = mRadioSimProxy->setAllowedCarriers(
      token,
      crp,
      ::aidl::android::hardware::radio::sim::SimLockMultiSimPolicy(
        multiSimPolicy));
    if (ret.isOk()) {
      DEBUG("SetAllowedCarriers successfully");
      return NS_OK;
    } else {
      DEBUG("SetAllowedCarriers fail %s", ret.getMessage());
      ERROR_NS_OK("SetAllowedCarriers failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetCarrierInfoForImsiEncryption(
  int32_t serial,
  const nsAString& mcc,
  const nsAString& mnc,
  const nsTArray<int32_t>& carrierKey,
  const nsAString& keyIdentifier,
  int32_t expirationTime,
  uint8_t publicKeyType)
{
  DEBUG("SetCarrierInfoForImsiEncryption token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::aidl::android::hardware::radio::sim::ImsiEncryptionInfo halImsiInfo;
    halImsiInfo.mnc = NS_ConvertUTF16toUTF8(mcc).get();
    halImsiInfo.mcc = NS_ConvertUTF16toUTF8(mnc).get();
    halImsiInfo.keyIdentifier = NS_ConvertUTF16toUTF8(keyIdentifier).get();
    halImsiInfo.expirationTime = expirationTime;
    std::vector<uint8_t> keys;
    for (uint32_t i = 0; i < carrierKey.Length(); i++) {
      keys.push_back(carrierKey[i]);
    }
    halImsiInfo.carrierKey = keys;
    halImsiInfo.keyType = publicKeyType;

    if (mRadioSimProxy) {
      ::ndk::ScopedAStatus ret =
        mRadioSimProxy->setCarrierInfoForImsiEncryption(serial, halImsiInfo);
      if (ret.isOk()) {
        DEBUG("SetCarrierInfoForImsiEncryption successfully");
        return NS_OK;
      } else {
        DEBUG("SetCarrierInfoForImsiEncryption fail %s", ret.getMessage());
        ERROR_NS_OK("SetCarrierInfoForImsiEncryption failed");
      }
    } else {
      ERROR_NS_OK("mRadioSimProxy is null");
    }
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetFacilityLockForApp(int32_t serial,
                                                  const nsAString& facility,
                                                  bool lockState,
                                                  const nsAString& password,
                                                  int32_t serviceClass,
                                                  const nsAString& aid)
{
  DEBUG("setFacilityLockForApp token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->setFacilityLockForApp(
      serial,
      NS_ConvertUTF16toUTF8(facility).get(),
      lockState,
      NS_ConvertUTF16toUTF8(password).get(),
      serviceClass,
      NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("setFacilityLockForApp successfully");
      return NS_OK;
    } else {
      DEBUG("setFacilityLockForApp fail %s", ret.getMessage());
      ERROR_NS_OK("setFacilityLockForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetSimCardPower(int32_t token,
                                            int32_t cardPowerState)
{
  DEBUG("setSimCardPower token:%d", token);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->setSimCardPower(
      token,
      ::aidl::android::hardware::radio::sim::CardPowerState(cardPowerState));
    if (ret.isOk()) {
      DEBUG("setSimCardPower successfully");
      return NS_OK;
    } else {
      DEBUG("setSimCardPower fail %s", ret.getMessage());
      ERROR_NS_OK("setSimCardPower failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetUiccSubscription(int32_t serial,
                                                int32_t slotId,
                                                int32_t appIndex,
                                                int32_t subId,
                                                int32_t subStatus)
{
  DEBUG("setSimCardPower token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {

    ::aidl::android::hardware::radio::sim::SelectUiccSub info;
    info.slot = slotId;
    info.appIndex = appIndex;
    info.subType = subId;
    info.actStatus = subStatus;

    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->setUiccSubscription(serial, info);
    if (ret.isOk()) {
      DEBUG("setSimCardPower successfully");
      return NS_OK;
    } else {
      DEBUG("setSimCardPower fail %s", ret.getMessage());
      ERROR_NS_OK("setSimCardPower failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SupplyIccPin2ForApp(int32_t serial,
                                                const nsAString& pin,
                                                const nsAString& aid)
{
  DEBUG("SupplyIccPin2ForApp token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->supplyIccPin2ForApp(serial,
                                          NS_ConvertUTF16toUTF8(pin).get(),
                                          NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("SupplyIccPin2ForApp successfully");
      return NS_OK;
    } else {
      DEBUG("SupplyIccPin2ForApp fail %s", ret.getMessage());
      ERROR_NS_OK("SupplyIccPin2ForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SupplyIccPinForApp(int32_t serial,
                                               const nsAString& pin,
                                               const nsAString& aid)
{
  DEBUG("SupplyIccPinForApp token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->supplyIccPinForApp(serial,
                                         NS_ConvertUTF16toUTF8(pin).get(),
                                         NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("SupplyIccPinForApp successfully");
      return NS_OK;
    } else {
      DEBUG("SupplyIccPinForApp fail %s", ret.getMessage());
      ERROR_NS_OK("SupplyIccPinForApp failed");
    }
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SupplyIccPuk2ForApp(int32_t serial,
                                                const nsAString& puk,
                                                const nsAString& newPin,
                                                const nsAString& aid)
{
  DEBUG("supplyIccPuk2ForApp token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->supplyIccPuk2ForApp(serial,
                                          NS_ConvertUTF16toUTF8(puk).get(),
                                          NS_ConvertUTF16toUTF8(newPin).get(),
                                          NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("SupplyIccPinForApp successfully");
      return NS_OK;
    } else {
      DEBUG("SupplyIccPinForApp fail %s", ret.getMessage());
      ERROR_NS_OK("SupplyIccPinForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SupplyIccPukForApp(int32_t serial,
                                               const nsAString& puk,
                                               const nsAString& newPin,
                                               const nsAString& aid)
{
  DEBUG("supplyIccPukForApp token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->supplyIccPukForApp(serial,
                                         NS_ConvertUTF16toUTF8(puk).get(),
                                         NS_ConvertUTF16toUTF8(newPin).get(),
                                         NS_ConvertUTF16toUTF8(aid).get());
    if (ret.isOk()) {
      DEBUG("supplyIccPukForApp successfully");
      return NS_OK;
    } else {
      DEBUG("supplyIccPukForApp fail %s", ret.getMessage());
      ERROR_NS_OK("supplyIccPukForApp failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SupplySimDepersonalization(
  int32_t serial,
  int32_t persoType,
  const nsAString& controlKey)
{
  DEBUG("SupplySimDepersonalization token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioSimProxy->supplySimDepersonalization(
      serial,
      (::aidl::android::hardware::radio::sim::PersoSubstate)persoType,
      NS_ConvertUTF16toUTF8(controlKey).get());
    if (ret.isOk()) {
      DEBUG("SupplySimDepersonalization successfully");
      return NS_OK;
    } else {
      DEBUG("SupplySimDepersonalization fail %s", ret.getMessage());
      ERROR_NS_OK("SupplySimDepersonalization failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::UpdateSimPhonebookRecords(
  int32_t serial,
  nsIPhonebookRecordInfo* info)
{
  DEBUG("UpdateSimPhonebookRecords token:%d", serial);
  if (mRadioSimProxy.get() == nullptr) {
    loadRadioSimProxy();
  }
  if (mRadioSimProxy.get() != nullptr) {
    ::aidl::android::hardware::radio::sim::PhonebookRecordInfo aMessage;
    ((nsPhonebookRecordInfo*)info)->updateToDestion(aMessage);
    ::ndk::ScopedAStatus ret =
      mRadioSimProxy->updateSimPhonebookRecords(serial, aMessage);

    if (ret.isOk()) {
      DEBUG("UpdateSimPhonebookRecords successfully");
      return NS_OK;
    } else {
      DEBUG("UpdateSimPhonebookRecords fail %s", ret.getMessage());
      ERROR_NS_OK("UpdateSimPhonebookRecords failed");
    }
  } else {
    ERROR_NS_OK("mRadioSimProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetVoNrEnabled(int32_t token, bool enable)
{
  DEBUG("setVoNrEnabled token:%d, enable:%d", token, enable);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->setVoNrEnabled(token, enable);
    if (ret.isOk()) {
      DEBUG("setVoNrEnabled successfully");
      return NS_OK;
    } else {
      DEBUG("setVoNrEnabled fail %s", ret.getMessage());
      ERROR_NS_OK("setVoNrEnabled failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::IsVoNrEnabled(int32_t token)
{
  DEBUG("IsVoNrEnabled token:%d", token);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->isVoNrEnabled(token);
    if (ret.isOk()) {
      DEBUG("IsVoNrEnabled successfully");
      return NS_OK;
    } else {
      DEBUG("IsVoNrEnabled fail %s", ret.getMessage());
      ERROR_NS_OK("IsVoNrEnabled failed");
    }
  }
  ERROR_NS_OK("mRadioVoiceProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::AcceptCall(int32_t serial)
{
  DEBUG("AcceptCall serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->acceptCall(serial);
    if (ret.isOk()) {
      DEBUG("acceptCall successfully");
      return NS_OK;
    } else {
      DEBUG("acceptCall fail %s", ret.getMessage());
      ERROR_NS_OK("acceptCall failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::CancelPendingUssd(int32_t serial)
{
  DEBUG("CancelPendingUssd serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->cancelPendingUssd(serial);
    if (ret.isOk()) {
      DEBUG("CancelPendingUssd successfully");
      return NS_OK;
    } else {
      DEBUG("CancelPendingUssd fail %s", ret.getMessage());
      ERROR_NS_OK("CancelPendingUssd failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::Conference(int32_t serial)
{
  DEBUG("Conference serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->conference(serial);
    if (ret.isOk()) {
      DEBUG("conference successfully");
      return NS_OK;
    } else {
      DEBUG("conference fail %s", ret.getMessage());
      ERROR_NS_OK("conference failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::RequestDial(int32_t serial,
                                        const nsAString& address,
                                        int32_t clirMode,
                                        int32_t uusType,
                                        int32_t uusDcs,
                                        const nsAString& uusData)
{
  DEBUG("RequestDial serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    std::vector<UusInfo> uusInfo;
    UusInfo info;
    info.uusType = uusType;
    info.uusDcs = uusDcs;
    if (uusData.Length() == 0) {
      // info.uusData = NULL;
    } else {
      info.uusData = NS_ConvertUTF16toUTF8(uusData).get();
    }
    uusInfo.push_back(info);

    Dial dialInfo;
    dialInfo.address = NS_ConvertUTF16toUTF8(address).get();
    dialInfo.clir = clirMode;
    dialInfo.uusInfo = uusInfo;

    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->dial(serial, dialInfo);
    if (ret.isOk()) {
      DEBUG("RequestDial successfully");
      return NS_OK;
    } else {
      DEBUG("RequestDial fail %s", ret.getMessage());
      ERROR_NS_OK("RequestDial failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::EmergencyDial(int32_t serial,
                                          const nsAString& address,
                                          uint32_t clirMode,
                                          uint32_t uusType,
                                          uint32_t uusDcs,
                                          const nsAString& uusData,
                                          uint32_t categories,
                                          const nsTArray<nsString>& aUrns,
                                          uint8_t routMode,
                                          bool hasKnownUserIntentEmergency,
                                          bool isTesting)
{
  DEBUG("EmergencyDial serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    std::vector<UusInfo> uusInfo;
    UusInfo info;
    info.uusType = uusType;
    info.uusDcs = uusDcs;
    if (uusData.Length() == 0) {
      // info.uusData = "";
    } else {
      info.uusData = NS_ConvertUTF16toUTF8(uusData).get();
    }
    uusInfo.push_back(info);

    Dial dialInfo;
    dialInfo.address = NS_ConvertUTF16toUTF8(address).get();
    dialInfo.clir = clirMode;
    dialInfo.uusInfo = uusInfo;

    std::vector<std::string> urns;
    urns.resize(aUrns.Length());
    for (uint32_t i = 0; i < aUrns.Length(); i++) {
      urns[i] = NS_ConvertUTF16toUTF8(aUrns[i]).get();
    }
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->emergencyDial(serial,
                                      dialInfo,
                                      categories,
                                      urns,
                                      (EmergencyCallRouting)routMode,
                                      hasKnownUserIntentEmergency,
                                      isTesting);
    if (ret.isOk()) {
      DEBUG("EmergencyDial successfully");
      return NS_OK;
    } else {
      DEBUG("EmergencyDial fail %s", ret.getMessage());
      ERROR_NS_OK("EmergencyDial failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ExitEmergencyCallbackMode(int32_t serial)
{
  DEBUG("ExitEmergencyCallbackMode serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->exitEmergencyCallbackMode(serial);
    if (ret.isOk()) {
      DEBUG("exitEmergencyCallbackMode successfully");
      return NS_OK;
    } else {
      DEBUG("exitEmergencyCallbackMode fail %s", ret.getMessage());
      ERROR_NS_OK("exitEmergencyCallbackMode failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ExplicitCallTransfer(int32_t serial)
{
  DEBUG("explicitCallTransfer serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->explicitCallTransfer(serial);
    if (ret.isOk()) {
      DEBUG("explicitCallTransfer successfully");
      return NS_OK;
    } else {
      DEBUG("explicitCallTransfer fail %s", ret.getMessage());
      ERROR_NS_OK("explicitCallTransfer failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetCallForwardStatus(int32_t serial,
                                                 int32_t cfReason,
                                                 int32_t serviceClass,
                                                 const nsAString& number,
                                                 int32_t toaNumber)
{
  DEBUG("GetCallForwardStatus serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    CallForwardInfo cfInfo;
    cfInfo.reason = cfReason;
    cfInfo.serviceClass = serviceClass;
    cfInfo.toa = toaNumber;
    cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
    cfInfo.timeSeconds = 0;
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->getCallForwardStatus(serial, cfInfo);
    if (ret.isOk()) {
      DEBUG("GetCallForwardStatus successfully");
      return NS_OK;
    } else {
      DEBUG("GetCallForwardStatus fail %s", ret.getMessage());
      ERROR_NS_OK("GetCallForwardStatus failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetCallWaiting(int32_t serial, int32_t serviceClass)
{
  DEBUG("GetCallWaiting serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->getCallWaiting(serial, serviceClass);
    if (ret.isOk()) {
      DEBUG("getCallWaiting successfully");
      return NS_OK;
    } else {
      DEBUG("getCallWaiting fail %s", ret.getMessage());
      ERROR_NS_OK("getCallWaiting failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetClip(int32_t serial)
{
  DEBUG("getClip serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getClip(serial);
    if (ret.isOk()) {
      DEBUG("getClip successfully");
      return NS_OK;
    } else {
      DEBUG("getClip fail %s", ret.getMessage());
      ERROR_NS_OK("getClip failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetClir(int32_t serial)
{
  DEBUG("getClir serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getClir(serial);
    if (ret.isOk()) {
      DEBUG("getClir successfully");
      return NS_OK;
    } else {
      DEBUG("getClir fail %s", ret.getMessage());
      ERROR_NS_OK("getClir failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetCurrentCalls(int32_t serial)
{
  DEBUG("GetCurrentCalls serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getCurrentCalls(serial);
    if (ret.isOk()) {
      DEBUG("getCurrentCalls successfully");
      return NS_OK;
    } else {
      DEBUG("getCurrentCalls fail %s", ret.getMessage());
      ERROR_NS_OK("getCurrentCalls failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetLastCallFailCause(int32_t serial)
{
  DEBUG("GetCurrentCalls serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getLastCallFailCause(serial);
    if (ret.isOk()) {
      DEBUG("getLastCallFailCause successfully");
      return NS_OK;
    } else {
      DEBUG("getLastCallFailCause fail %s", ret.getMessage());
      ERROR_NS_OK("getLastCallFailCause failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetMute(int32_t serial)
{
  DEBUG("GetMute serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getMute(serial);
    if (ret.isOk()) {
      DEBUG("getMute successfully");
      return NS_OK;
    } else {
      DEBUG("getMute fail %s", ret.getMessage());
      ERROR_NS_OK("getMute failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetPreferredVoicePrivacy(int32_t serial)
{
  DEBUG("getPreferredVoicePrivacy serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->getPreferredVoicePrivacy(serial);
    if (ret.isOk()) {
      DEBUG("getPreferredVoicePrivacy successfully");
      return NS_OK;
    } else {
      DEBUG("getPreferredVoicePrivacy fail %s", ret.getMessage());
      ERROR_NS_OK("getPreferredVoicePrivacy failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::QueryTTYMode(int32_t serial)
{
  DEBUG("QueryTTYMode serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->getTtyMode(serial);
    if (ret.isOk()) {
      DEBUG("getTtyMode successfully");
      return NS_OK;
    } else {
      DEBUG("getTtyMode fail %s", ret.getMessage());
      ERROR_NS_OK("getTtyMode failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::HandleStkCallSetupRequestFromSim(int32_t serial,
                                                             bool accept)
{
  DEBUG("handleStkCallSetupRequestFromSim serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->handleStkCallSetupRequestFromSim(serial, accept);
    if (ret.isOk()) {
      DEBUG("handleStkCallSetupRequestFromSim successfully");
      return NS_OK;
    } else {
      DEBUG("handleStkCallSetupRequestFromSim fail %s", ret.getMessage());
      ERROR_NS_OK("handleStkCallSetupRequestFromSim failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::HangupConnection(int32_t serial, int32_t callIndex)
{
  DEBUG("HangupConnection serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->hangup(serial, callIndex);
    if (ret.isOk()) {
      DEBUG("HangupConnection successfully");
      return NS_OK;
    } else {
      DEBUG("HangupConnection fail %s", ret.getMessage());
      ERROR_NS_OK("HangupConnection failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::HangupForegroundResumeBackground(int32_t serial)
{
  DEBUG("hangupForegroundResumeBackground serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->hangupForegroundResumeBackground(serial);
    if (ret.isOk()) {
      DEBUG("hangupForegroundResumeBackground successfully");
      return NS_OK;
    } else {
      DEBUG("hangupForegroundResumeBackground fail %s", ret.getMessage());
      ERROR_NS_OK("hangupForegroundResumeBackground failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::HangupWaitingOrBackground(int32_t serial)
{
  DEBUG("hangupWaitingOrBackground serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->hangupWaitingOrBackground(serial);
    if (ret.isOk()) {
      DEBUG("hangupWaitingOrBackground successfully");
      return NS_OK;
    } else {
      DEBUG("hangupWaitingOrBackground fail %s", ret.getMessage());
      ERROR_NS_OK("hangupWaitingOrBackground failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::RejectCall(int32_t serial)
{
  DEBUG("rejectCall serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->rejectCall(serial);
    if (ret.isOk()) {
      DEBUG("rejectCall successfully");
      return NS_OK;
    } else {
      DEBUG("rejectCall fail %s", ret.getMessage());
      ERROR_NS_OK("rejectCall failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendDtmf(int32_t serial, const nsAString& dtmfChar)
{
  DEBUG("sendDtmf serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->sendDtmf(serial, NS_ConvertUTF16toUTF8(dtmfChar).get());
    if (ret.isOk()) {
      DEBUG("sendDtmf successfully");
      return NS_OK;
    } else {
      DEBUG("sendDtmf fail %s", ret.getMessage());
      ERROR_NS_OK("sendDtmf failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendUssd(int32_t serial, const nsAString& ussd)
{
  DEBUG("sendUssd serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->sendUssd(serial, NS_ConvertUTF16toUTF8(ussd).get());
    if (ret.isOk()) {
      DEBUG("sendUssd successfully");
      return NS_OK;
    } else {
      DEBUG("sendUssd fail %s", ret.getMessage());
      ERROR_NS_OK("sendUssd failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SeparateConnection(int32_t serial, int32_t gsmIndex)
{
  DEBUG("separateConnection serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->separateConnection(serial, gsmIndex);
    if (ret.isOk()) {
      DEBUG("separateConnection successfully");
      return NS_OK;
    } else {
      DEBUG("separateConnection fail %s", ret.getMessage());
      ERROR_NS_OK("separateConnection failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetCallForwardStatus(int32_t serial,
                                                 int32_t action,
                                                 int32_t cfReason,
                                                 int32_t serviceClass,
                                                 const nsAString& number,
                                                 int32_t toaNumber,
                                                 int32_t timeSeconds)
{
  DEBUG("setCallForward serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    CallForwardInfo cfInfo;
    cfInfo.status = action;
    cfInfo.reason = cfReason;
    cfInfo.serviceClass = serviceClass;
    cfInfo.toa = toaNumber;
    cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
    cfInfo.timeSeconds = timeSeconds;
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->setCallForward(serial, cfInfo);
    if (ret.isOk()) {
      DEBUG("setCallForward successfully");
      return NS_OK;
    } else {
      DEBUG("setCallForward fail %s", ret.getMessage());
      ERROR_NS_OK("setCallForward failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetCallWaiting(int32_t serial,
                                           bool enable,
                                           int32_t serviceClass)
{
  DEBUG("SetCallWaiting serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->setCallWaiting(serial, enable, serviceClass);
    if (ret.isOk()) {
      DEBUG("setCallWaiting successfully");
      return NS_OK;
    } else {
      DEBUG("setCallWaiting fail %s", ret.getMessage());
      ERROR_NS_OK("setCallWaiting failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetClir(int32_t serial, int32_t clirMode)
{
  DEBUG("SetClir serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->setClir(serial, clirMode);
    if (ret.isOk()) {
      DEBUG("SetClir successfully");
      return NS_OK;
    } else {
      DEBUG("SetClir fail %s", ret.getMessage());
      ERROR_NS_OK("SetClir failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetMute(int32_t serial, bool enableMute)
{
  DEBUG("setMute serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->setMute(serial, enableMute);
    if (ret.isOk()) {
      DEBUG("setMute successfully");
      return NS_OK;
    } else {
      DEBUG("setMute fail %s", ret.getMessage());
      ERROR_NS_OK("setMute failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetPreferredVoicePrivacy(int32_t serial,
                                                     bool enable)
{
  DEBUG("setPreferredVoicePrivacy serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->setPreferredVoicePrivacy(serial, enable);
    if (ret.isOk()) {
      DEBUG("setPreferredVoicePrivacy successfully");
      return NS_OK;
    } else {
      DEBUG("setPreferredVoicePrivacy fail %s", ret.getMessage());
      ERROR_NS_OK("setPreferredVoicePrivacy failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetTTYMode(int32_t serial, int32_t ttyMode)
{
  DEBUG("SetTTYMode serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->setTtyMode(serial, (TtyMode)ttyMode);
    if (ret.isOk()) {
      DEBUG("SetTTYMode successfully");
      return NS_OK;
    } else {
      DEBUG("SetTTYMode fail %s", ret.getMessage());
      ERROR_NS_OK("SetTTYMode failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StartDtmf(int32_t serial, const nsAString& dtmfChar)
{
  DEBUG("startDtmf serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->startDtmf(
      serial, NS_ConvertUTF16toUTF8(dtmfChar).get());
    if (ret.isOk()) {
      DEBUG("startDtmf successfully");
      return NS_OK;
    } else {
      DEBUG("startDtmf fail %s", ret.getMessage());
      ERROR_NS_OK("startDtmf failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StopDtmf(int32_t serial)
{
  DEBUG("StopDtmf serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioVoiceProxy->stopDtmf(serial);
    if (ret.isOk()) {
      DEBUG("stopDtmf successfully");
      return NS_OK;
    } else {
      DEBUG("stopDtmf fail %s", ret.getMessage());
      ERROR_NS_OK("stopDtmf failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SwitchWaitingOrHoldingAndActive(int32_t serial)
{
  DEBUG("switchWaitingOrHoldingAndActive serial:%d", serial);
  if (mRadioVoiceProxy.get() == nullptr) {
    loadRadioVoiceProxy();
  }
  if (mRadioVoiceProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioVoiceProxy->switchWaitingOrHoldingAndActive(serial);
    if (ret.isOk()) {
      DEBUG("switchWaitingOrHoldingAndActive successfully");
      return NS_OK;
    } else {
      DEBUG("switchWaitingOrHoldingAndActive fail %s", ret.getMessage());
      ERROR_NS_OK("switchWaitingOrHoldingAndActive failed");
    }
  } else {
    ERROR_NS_OK("mRadioVoiceProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetRadioPower(int32_t serial,
                                          bool enabled,
                                          bool forEmergencyCall,
                                          bool preferredForEmergencyCall)
{
  DEBUG("SetRadioPower serial:%d, enabled:%d", serial, enabled);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->setRadioPower(
      serial, enabled, forEmergencyCall, preferredForEmergencyCall);
    if (ret.isOk()) {
      DEBUG("setRadioPower successfully");
      return NS_OK;
    } else {
      DEBUG("setRadioPower fail %s", ret.getMessage());
      ERROR_NS_OK("setRadioPower failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::EnableModem(int32_t serial, bool on)
{
  DEBUG("EnableModem serial:%d, on:%d", serial, on);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->enableModem(serial, on);
    if (ret.isOk()) {
      DEBUG("enableModem successfully");
      return NS_OK;
    } else {
      DEBUG("enableModem fail %s", ret.getMessage());
      ERROR_NS_OK("enableModem failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetBasebandVersion(int32_t serial)
{
  DEBUG("getBasebandVersion serial:%d", serial);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->getBasebandVersion(serial);
    if (ret.isOk()) {
      DEBUG("getBasebandVersion successfully");
      return NS_OK;
    } else {
      DEBUG("getBasebandVersion fail %s", ret.getMessage());
      ERROR_NS_OK("getBasebandVersion failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetDeviceIdentity(int32_t serial)
{
  DEBUG("GetDeviceIdentity serial:%d", serial);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->getDeviceIdentity(serial);
    if (ret.isOk()) {
      DEBUG("getDeviceIdentity successfully");
      return NS_OK;
    } else {
      DEBUG("getDeviceIdentity fail %s", ret.getMessage());
      ERROR_NS_OK("getDeviceIdentity failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetModemStackStatus(int32_t serial)
{
  DEBUG("getModemStackStatus serial:%d", serial);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->getModemStackStatus(serial);
    if (ret.isOk()) {
      DEBUG("getModemStackStatus successfully");
      return NS_OK;
    } else {
      DEBUG("getModemStackStatus fail %s", ret.getMessage());
      ERROR_NS_OK("getModemStackStatus failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetRadioCapability(int32_t serial)
{
  DEBUG("getRadioCapability serial:%d", serial);
  if (mRadioModemProxy.get() == nullptr) {
    loadRadioModemProxy();
  }
  if (mRadioModemProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioModemProxy->getRadioCapability(serial);
    if (ret.isOk()) {
      DEBUG("getRadioCapability successfully");
      return NS_OK;
    } else {
      DEBUG("getRadioCapability fail %s", ret.getMessage());
      ERROR_NS_OK("getRadioCapability failed");
    }
  } else {
    ERROR_NS_OK("mRadioModemProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::AcknowledgeLastIncomingGsmSms(int32_t serial,
                                                          bool success,
                                                          int32_t cause)
{
  DEBUG("acknowledgeLastIncomingGsmSms serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioMessagingProxy->acknowledgeLastIncomingGsmSms(
        serial, success, (SmsAcknowledgeFailCause)cause);
    if (ret.isOk()) {
      DEBUG("acknowledgeLastIncomingGsmSms successfully");
      return NS_OK;
    } else {
      DEBUG("acknowledgeLastIncomingGsmSms fail %s", ret.getMessage());
      ERROR_NS_OK("acknowledgeLastIncomingGsmSms failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSmscAddress(int32_t serial)
{
  DEBUG("GetSmscAddress serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioMessagingProxy->getSmscAddress(serial);
    if (ret.isOk()) {
      DEBUG("GetSmscAddress successfully");
      return NS_OK;
    } else {
      DEBUG("GetSmscAddress fail %s", ret.getMessage());
      ERROR_NS_OK("GetSmscAddress failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ReportSmsMemoryStatus(int32_t serial,
                                                  bool available)
{
  DEBUG("ReportSmsMemoryStatus serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioMessagingProxy->reportSmsMemoryStatus(serial, available);
    if (ret.isOk()) {
      DEBUG("ReportSmsMemoryStatus successfully");
      return NS_OK;
    } else {
      DEBUG("ReportSmsMemoryStatus fail %s", ret.getMessage());
      ERROR_NS_OK("ReportSmsMemoryStatus failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendCdmaSmsExpectMore(int32_t serial,
                                                  nsICdmaSmsMessage* aMessage)
{
  DEBUG("sendCdmaSmsExpectMore serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    CdmaSmsMessage msg;
    convertToCdmaSmsMessage(aMessage, msg);
    ::ndk::ScopedAStatus ret =
      mRadioMessagingProxy->sendCdmaSmsExpectMore(serial, msg);
    if (ret.isOk()) {
      DEBUG("sendCdmaSmsExpectMore successfully");
      return NS_OK;
    } else {
      DEBUG("sendCdmaSmsExpectMore fail %s", ret.getMessage());
      ERROR_NS_OK("sendCdmaSmsExpectMore failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SendSMS(int32_t serial,
                                    const nsAString& smsc,
                                    const nsAString& pdu)
{
  DEBUG("SendSMS serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    GsmSmsMessage msg;
    msg.smscPdu = NS_ConvertUTF16toUTF8(smsc).get();
    msg.pdu = NS_ConvertUTF16toUTF8(pdu).get();
    ::ndk::ScopedAStatus ret = mRadioMessagingProxy->sendSms(serial, msg);
    if (ret.isOk()) {
      DEBUG("SendSMS successfully");
      return NS_OK;
    } else {
      DEBUG("SendSMS fail %s", ret.getMessage());
      ERROR_NS_OK("SendSMS failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetGsmBroadcastActivation(int32_t serial,
                                                      bool activate)
{
  DEBUG("SetGsmBroadcastActivation serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioMessagingProxy->setGsmBroadcastActivation(serial, activate);
    if (ret.isOk()) {
      DEBUG("SetGsmBroadcastActivation successfully");
      return NS_OK;
    } else {
      DEBUG("SetGsmBroadcastActivation fail %s", ret.getMessage());
      ERROR_NS_OK("SetGsmBroadcastActivation failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetGsmBroadcastConfig(
  int32_t serial,
  const nsTArray<int32_t>& ranges)
{
  DEBUG("SetGsmBroadcastConfig serial:%d", serial);
  if (mRadioMessagingProxy.get() == nullptr) {
    loadRadioMessagingProxy();
  }
  if (mRadioMessagingProxy.get() != nullptr) {
    std::vector<GsmBroadcastSmsConfigInfo> broadcastInfo;
    for (uint32_t i = 0; i < ranges.Length();) {
      GsmBroadcastSmsConfigInfo info;
      // convert [from, to) to [from, to - 1]
      info.fromServiceId = ranges[i++];
      info.toServiceId = ranges[i++] - 1;
      info.fromCodeScheme = 0x00;
      info.toCodeScheme = 0xFF;
      info.selected = 1;
      broadcastInfo.push_back(info);
    }
    ::ndk::ScopedAStatus ret =
      mRadioMessagingProxy->setGsmBroadcastConfig(serial, broadcastInfo);
    if (ret.isOk()) {
      DEBUG("SetGsmBroadcastConfig successfully");
      return NS_OK;
    } else {
      DEBUG("SetGsmBroadcastConfig fail %s", ret.getMessage());
      ERROR_NS_OK("SetGsmBroadcastConfig failed");
    }
  } else {
    ERROR_NS_OK("mRadioMessagingProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::AllocatePduSessionId(int32_t serial)
{
  DEBUG("allocatePduSessionId serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->allocatePduSessionId(serial);
    if (ret.isOk()) {
      DEBUG("allocatePduSessionId successfully");
      return NS_OK;
    } else {
      DEBUG("allocatePduSessionId fail %s", ret.getMessage());
      ERROR_NS_OK("allocatePduSessionId failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::CancelHandover(int32_t serial, int32_t callId)
{
  DEBUG("CancelHandover serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->cancelHandover(serial, callId);
    if (ret.isOk()) {
      DEBUG("cancelHandover successfully");
      return NS_OK;
    } else {
      DEBUG("cancelHandover fail %s", ret.getMessage());
      ERROR_NS_OK("cancelHandover failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::DeactivateDataCall(int32_t serial,
                                               int32_t cid,
                                               int32_t reason)
{
  DEBUG("DeactivateDataCall serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->deactivateDataCall(
      serial, cid, (DataRequestReason)reason);
    if (ret.isOk()) {
      DEBUG("deactivateDataCall successfully");
      return NS_OK;
    } else {
      DEBUG("deactivateDataCall fail %s", ret.getMessage());
      ERROR_NS_OK("deactivateDataCall failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetDataCallList(int32_t serial)
{
  DEBUG("getDataCallList serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->getDataCallList(serial);
    if (ret.isOk()) {
      DEBUG("getDataCallList successfully");
      return NS_OK;
    } else {
      DEBUG("getDataCallList fail %s", ret.getMessage());
      ERROR_NS_OK("getDataCallList failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSlicingConfig(int32_t serial)
{
  DEBUG("getSlicingConfig serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->getSlicingConfig(serial);
    if (ret.isOk()) {
      DEBUG("getSlicingConfig successfully");
      return NS_OK;
    } else {
      DEBUG("getSlicingConfig fail %s", ret.getMessage());
      ERROR_NS_OK("getSlicingConfig failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::ReleasePduSessionId(int32_t serial, int32_t id)
{
  DEBUG("releasePduSessionId serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->releasePduSessionId(serial, id);
    if (ret.isOk()) {
      DEBUG("releasePduSessionId successfully");
      return NS_OK;
    } else {
      DEBUG("releasePduSessionId fail %s", ret.getMessage());
      ERROR_NS_OK("releasePduSessionId failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetDataAllowed(int32_t serial, bool allowed)
{
  DEBUG("setDataAllowed serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->setDataAllowed(serial, allowed);
    if (ret.isOk()) {
      DEBUG("setDataAllowed successfully");
      return NS_OK;
    } else {
      DEBUG("setDataAllowed fail %s", ret.getMessage());
      ERROR_NS_OK("setDataAllowed failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetDataProfile(
  int32_t serial,
  const nsTArray<RefPtr<nsIDataProfile>>& profileList,
  bool isRoaming)
{
  DEBUG("setDataProfile serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    std::vector<DataProfileInfo> dataProfileInfoList;
    for (uint32_t i = 0; i < profileList.Length(); i++) {
      DataProfileInfo profile =
        convertToHalDataProfile(profileList[i], nullptr);
      dataProfileInfoList.push_back(profile);
    }

    ::ndk::ScopedAStatus ret =
      mRadioDataProxy->setDataProfile(serial, dataProfileInfoList);
    if (ret.isOk()) {
      DEBUG("setDataProfile successfully");
      return NS_OK;
    } else {
      DEBUG("setDataProfile fail %s", ret.getMessage());
      ERROR_NS_OK("setDataProfile failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetInitialAttachApn(int32_t serial,
                                                nsIDataProfile* profile,
                                                bool isRoaming)
{
  DEBUG("setInitialAttachApn serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->setInitialAttachApn(
      serial, convertToHalDataProfile(profile, nullptr));
    if (ret.isOk()) {
      DEBUG("setInitialAttachApn successfully");
      return NS_OK;
    } else {
      DEBUG("setInitialAttachApn fail %s", ret.getMessage());
      ERROR_NS_OK("setInitialAttachApn failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetupDataCall(
  int32_t serial,
  int32_t radioTechnology,
  int32_t accessNetworkType,
  nsIDataProfile* profile,
  bool modemConfig,
  bool allowRoaming,
  bool isRoaming,
  int32_t reason,
  const nsTArray<RefPtr<nsILinkAddress>>& addresses,
  const nsTArray<nsString>& dnses,
  int32_t pduSessionId,
  nsISliceInfo* sliceInfo,
  nsITrafficDescriptor* trafficDescriptor,
  bool matchAllRuleAllowed)
{
  DEBUG("setupDataCall serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::std::vector<LinkAddress> linkAddrs;
    for (uint32_t i = 0; i < addresses.Length(); i++) {
      LinkAddress address = convertToHalLinkAddress(addresses[i]);
      linkAddrs.push_back(address);
    }

    std::vector<std::string> dns;
    for (uint32_t i = 0; i < dnses.Length(); i++) {
      dns.push_back(NS_ConvertUTF16toUTF8(dnses[i]).get());
    }
    ::ndk::ScopedAStatus ret = mRadioDataProxy->setupDataCall(
      serial,
      AccessNetwork(accessNetworkType),
      convertToHalDataProfile(profile, trafficDescriptor),
      allowRoaming,
      DataRequestReason(reason),
      linkAddrs,
      dns,
      pduSessionId,
      convertToHalSliceInfo(sliceInfo),
      matchAllRuleAllowed);
    if (ret.isOk()) {
      DEBUG("setupDataCall successfully");
      return NS_OK;
    } else {
      DEBUG("setupDataCall fail %s", ret.getMessage());
      ERROR_NS_OK("setupDataCall failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StartHandover(int32_t serial, int32_t callId)
{
  DEBUG("startHandover serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioDataProxy->startHandover(serial, callId);
    if (ret.isOk()) {
      DEBUG("startHandover successfully");
      return NS_OK;
    } else {
      DEBUG("startHandover fail %s", ret.getMessage());
      ERROR_NS_OK("startHandover failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StartNattKeepalive(
  int32_t serial,
  int32_t type,
  const nsAString& sourceAddress,
  int32_t sourcePort,
  const nsAString& destinationAddress,
  int32_t destinationPort,
  int32_t maxKeepaliveIntervalMillis,
  int32_t cid)
{
  DEBUG("StartNattKeepalive serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    aidl::android::hardware::radio::data::KeepaliveRequest req;

    // Handle sourceAddress
    unsigned char ipv4SourceAddrNetwork[sourceAddress.Length()];
    unsigned char ipv6SourceAddrNetwork[sourceAddress.Length()];
    unsigned char* sourceAddr = ipv4SourceAddrNetwork;
    if (inet_pton(AF_INET,
                  NS_ConvertUTF16toUTF8(sourceAddress).get(),
                  ipv4SourceAddrNetwork) < 1) {
      sourceAddr = &(ipv6SourceAddrNetwork[0]);
      if (inet_pton(AF_INET6,
                    NS_ConvertUTF16toUTF8(sourceAddress).get(),
                    ipv6SourceAddrNetwork) < 1) {
        DEBUG("Failed to transfer Ipv4v6 souorce address to binary");
        return NS_OK;
      }
    }

    std::vector<uint8_t> source;
    for (uint32_t i = 0; i < sizeof(sourceAddr); i++) {
      source.push_back(sourceAddr[i]);
    }
    req.sourceAddress = source;
    req.sourcePort = sourcePort;

    // Handle destinationAddress
    unsigned char ipv4DesAddrNetwork[destinationAddress.Length()];
    unsigned char ipv6DesAddrNetwork[destinationAddress.Length()];
    unsigned char* destinationAddr = ipv4DesAddrNetwork;
    if (inet_pton(AF_INET,
                  NS_ConvertUTF16toUTF8(destinationAddress).get(),
                  ipv4DesAddrNetwork) < 1) {
      destinationAddr = &(ipv6DesAddrNetwork[0]);
      if (inet_pton(AF_INET6,
                    NS_ConvertUTF16toUTF8(destinationAddress).get(),
                    ipv6DesAddrNetwork) < 1) {
        DEBUG("Failed to transfer Ipv4v6 destination address to binary");
        return NS_OK;
      }
    }

    std::vector<uint8_t> destination;
    for (uint32_t i = 0; i < sizeof(destinationAddr); i++) {
      destination.push_back(destinationAddr[i]);
    }
    req.destinationAddress = destination;
    req.destinationPort = destinationPort;

    req.maxKeepaliveIntervalMillis = maxKeepaliveIntervalMillis;
    req.cid = cid;
    ::ndk::ScopedAStatus ret = mRadioDataProxy->startKeepalive(serial, req);
    if (ret.isOk()) {
      DEBUG("StartNattKeepalive successfully");
      return NS_OK;
    } else {
      DEBUG("StartNattKeepalive fail %s", ret.getMessage());
      ERROR_NS_OK("StartNattKeepalive failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StopNattKeepalive(int32_t serial,
                                              int32_t sessionHandle)
{
  DEBUG("StopNattKeepalive serial:%d", serial);
  if (mRadioDataProxy.get() == nullptr) {
    loadRadioDataProxy();
  }
  if (mRadioDataProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioDataProxy->stopKeepalive(serial, sessionHandle);
    if (ret.isOk()) {
      DEBUG("StopNattKeepalive successfully");
      return NS_OK;
    } else {
      DEBUG("StopNattKeepalive fail %s", ret.getMessage());
      ERROR_NS_OK("StopNattKeepalive failed");
    }
  } else {
    ERROR_NS_OK("mRadioDataProxy is null");
  }
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetBarringPassword(int32_t serial,
                                               const nsAString& facility,
                                               const nsAString& oldPwd,
                                               const nsAString& newPwd)
{
  DEBUG("setBarringPassword token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->setBarringPassword(
      serial,
      NS_ConvertUTF16toUTF8(facility).get(),
      NS_ConvertUTF16toUTF8(oldPwd).get(),
      NS_ConvertUTF16toUTF8(newPwd).get());
    if (ret.isOk()) {
      DEBUG("setBarringPassword successfully");
      return NS_OK;
    } else {
      DEBUG("setBarringPassword fail %s", ret.getMessage());
      ERROR_NS_OK("setBarringPassword failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetVoiceRegistrationState(int32_t serial)
{
  DEBUG("getVoiceRegistrationState token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getVoiceRegistrationState(serial);
    if (ret.isOk()) {
      DEBUG("getVoiceRegistrationState successfully");
      return NS_OK;
    } else {
      DEBUG("getVoiceRegistrationState fail %s", ret.getMessage());
      ERROR_NS_OK("getVoiceRegistrationState failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetDataRegistrationState(int32_t serial)
{
  DEBUG("getDataRegistrationState token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getDataRegistrationState(serial);
    if (ret.isOk()) {
      DEBUG("getDataRegistrationState successfully");
      return NS_OK;
    } else {
      DEBUG("getDataRegistrationState fail %s", ret.getMessage());
      ERROR_NS_OK("getDataRegistrationState failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetOperator(int32_t serial)
{
  DEBUG("getOperator token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->getOperator(serial);
    if (ret.isOk()) {
      DEBUG("getOperator successfully");
      return NS_OK;
    } else {
      DEBUG("getOperator fail %s", ret.getMessage());
      ERROR_NS_OK("getOperator failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetNetworkSelectionMode(int32_t serial)
{
  DEBUG("getNetworkSelectionMode token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getNetworkSelectionMode(serial);
    if (ret.isOk()) {
      DEBUG("getNetworkSelectionMode successfully");
      return NS_OK;
    } else {
      DEBUG("getNetworkSelectionMode fail %s", ret.getMessage());
      ERROR_NS_OK("getNetworkSelectionMode failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSignalStrength(int32_t serial)
{
  DEBUG("getSignalStrength token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->getSignalStrength(serial);
    if (ret.isOk()) {
      DEBUG("getSignalStrength successfully");
      return NS_OK;
    } else {
      DEBUG("getSignalStrength fail %s", ret.getMessage());
      ERROR_NS_OK("getSignalStrength failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetVoiceRadioTechnology(int32_t serial)
{
  DEBUG("getVoiceRadioTechnology token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getVoiceRadioTechnology(serial);
    if (ret.isOk()) {
      DEBUG("getVoiceRadioTechnology successfully");
      return NS_OK;
    } else {
      DEBUG("getVoiceRadioTechnology fail %s", ret.getMessage());
      ERROR_NS_OK("getVoiceRadioTechnology failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetCellInfoListRate(int32_t serial,
                                                int32_t rateInMillis)
{
  DEBUG("setCellInfoListRate token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setCellInfoListRate(serial, rateInMillis);
    if (ret.isOk()) {
      DEBUG("setCellInfoListRate successfully");
      return NS_OK;
    } else {
      DEBUG("setCellInfoListRate fail %s", ret.getMessage());
      ERROR_NS_OK("setCellInfoListRate failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetNetworkSelectionModeAutomatic(int32_t serial)
{
  DEBUG("setNetworkSelectionModeAutomatic token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setNetworkSelectionModeAutomatic(serial);
    if (ret.isOk()) {
      DEBUG("setNetworkSelectionModeAutomatic successfully");
      return NS_OK;
    } else {
      DEBUG("setNetworkSelectionModeAutomatic fail %s", ret.getMessage());
      ERROR_NS_OK("setNetworkSelectionModeAutomatic failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetNetworkSelectionModeManual(
  int32_t serial,
  const nsAString& operatorNumeric,
  int32_t ran)
{
  DEBUG("setNetworkSelectionModeManual token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setNetworkSelectionModeManual(
        serial,
        NS_ConvertUTF16toUTF8(operatorNumeric).get(),
        (aidl::android::hardware::radio::AccessNetwork)ran);
    if (ret.isOk()) {
      DEBUG("setNetworkSelectionModeManual successfully");
      return NS_OK;
    } else {
      DEBUG("setNetworkSelectionModeManual fail %s", ret.getMessage());
      ERROR_NS_OK("setNetworkSelectionModeManual failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetAvailableNetworks(int32_t serial)
{
  DEBUG("getAvailableNetworks token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->getAvailableNetworks(serial);
    if (ret.isOk()) {
      DEBUG("getAvailableNetworks successfully");
      return NS_OK;
    } else {
      DEBUG("getAvailableNetworks fail %s", ret.getMessage());
      ERROR_NS_OK("getAvailableNetworks failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetCellInfoList(int32_t serial)
{
  DEBUG("getCellInfoList token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->getCellInfoList(serial);
    if (ret.isOk()) {
      DEBUG("getCellInfoList successfully");
      return NS_OK;
    } else {
      DEBUG("getCellInfoList fail %s", ret.getMessage());
      ERROR_NS_OK("getCellInfoList failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StopNetworkScan(int32_t serial)
{
  DEBUG("stopNetworkScan token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->stopNetworkScan(serial);
    if (ret.isOk()) {
      DEBUG("stopNetworkScan successfully");
      return NS_OK;
    } else {
      DEBUG("stopNetworkScan fail %s", ret.getMessage());
      ERROR_NS_OK("stopNetworkScan failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetSuppServiceNotifications(int32_t serial,
                                                        bool enable)
{
  DEBUG("setSuppServiceNotifications token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setSuppServiceNotifications(serial, enable);
    if (ret.isOk()) {
      DEBUG("setSuppServiceNotifications successfully");
      return NS_OK;
    } else {
      DEBUG("setSuppServiceNotifications fail %s", ret.getMessage());
      ERROR_NS_OK("setSuppServiceNotifications failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetIndicationFilter(int32_t serial, int32_t filter)
{
  DEBUG("setIndicationFilter token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setIndicationFilter(serial, filter);
    if (ret.isOk()) {
      DEBUG("setIndicationFilter successfully");
      return NS_OK;
    } else {
      DEBUG("setIndicationFilter fail %s", ret.getMessage());
      ERROR_NS_OK("setIndicationFilter failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetNrDualConnectivityState(
  int32_t serial,
  int32_t nrDualConnectivityState)
{
  DEBUG("setNrDualConnectivityState token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->setNrDualConnectivityState(
      serial,
      (aidl::android::hardware::radio::network::NrDualConnectivityState)
        nrDualConnectivityState);
    if (ret.isOk()) {
      DEBUG("setNrDualConnectivityState successfully");
      return NS_OK;
    } else {
      DEBUG("setNrDualConnectivityState fail %s", ret.getMessage());
      ERROR_NS_OK("setNrDualConnectivityState failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetBarringInfo(int32_t serial)
{
  DEBUG("getBarringInfo token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->getBarringInfo(serial);
    if (ret.isOk()) {
      DEBUG("getBarringInfo successfully");
      return NS_OK;
    } else {
      DEBUG("getBarringInfo fail %s", ret.getMessage());
      ERROR_NS_OK("getBarringInfo failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetUnsolResponseFilter(int32_t serial,
                                                   int32_t filter)
{
  DEBUG("setUnsolResponseFilter token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setIndicationFilter(serial, filter);
    if (ret.isOk()) {
      DEBUG("setUnsolResponseFilter successfully");
      return NS_OK;
    } else {
      DEBUG("setUnsolResponseFilter fail %s", ret.getMessage());
      ERROR_NS_OK("setUnsolResponseFilter failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetPreferredNetworkType(int32_t serial,
                                                    int32_t networkType)
{
  DEBUG("setPreferredNetworkType token:%d", serial);
  ERROR_NS_OK("setPreferredNetworkType is not supported in aidl");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetPreferredNetworkType(int32_t serial)
{
  DEBUG("getPreferredNetworkType token:%d", serial);
  ERROR_NS_OK("getPreferredNetworkType is not supported in aidl");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetNeighboringCids(int32_t serial)
{
  DEBUG("getNeighboringCids token:%d", serial);
  ERROR_NS_OK("getNeighboringCids is not supported in aidl");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::StartNetworkScan(int32_t serial,
                                             nsINetworkScanRequest* request)
{
  DEBUG("startNetworkScan token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  aidl::android::hardware::radio::network::NetworkScanRequest scanRequest;
  int32_t nsType;
  request->GetType(&nsType);
  scanRequest.type = nsType;

  int32_t nsInterval;
  request->GetInterval(&nsInterval);
  scanRequest.interval = nsInterval;

  int32_t nsMaxSearchTime;
  request->GetMaxSearchTime(&nsMaxSearchTime);
  scanRequest.maxSearchTime = nsMaxSearchTime;

  int32_t nsIncrementalResultsPeriodicity;
  request->GetMaxSearchTime(&nsIncrementalResultsPeriodicity);
  scanRequest.incrementalResultsPeriodicity = nsIncrementalResultsPeriodicity;

  bool nsIncrementalResults;
  request->GetIncrementalResults(&nsIncrementalResults);
  scanRequest.incrementalResults = nsIncrementalResults;

  nsTArray<nsString> nsMccMncs;
  request->GetMccMncs(nsMccMncs);
  for (uint32_t i = 0; i < nsMccMncs.Length(); i++) {
    scanRequest.mccMncs[i] = NS_ConvertUTF16toUTF8(nsMccMncs[i]).get();
  }

  ::std::vector<RadioAccessSpecifier_aidl> specifiers;
  nsTArray<RefPtr<nsIRadioAccessSpecifier>> nsSpecifiers;
  request->GetSpecifiers(nsSpecifiers);
  for (uint32_t i = 0; i < nsSpecifiers.Length(); i++) {
    if (nsSpecifiers[i] == nullptr) {
      DEBUG("nsSpecifiers[%d] is null", i);
    } else {
      RadioAccessSpecifier_aidl specifier =
        convertToaidlRadioAccessSpecifier(nsSpecifiers[i]);
      specifiers.push_back(specifier);
    }
  }
  scanRequest.specifiers = specifiers;

  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->startNetworkScan(serial, scanRequest);
    if (ret.isOk()) {
      DEBUG("startNetworkScan successfully");
      return NS_OK;
    } else {
      DEBUG("startNetworkScan fail %s", ret.getMessage());
      ERROR_NS_OK("startNetworkScan failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetLinkCapacityReportingCriteria(
  int32_t serial,
  int32_t hysteresisMs,
  int32_t hysteresisDlKbps,
  int32_t hysteresisUlKbps,
  const nsTArray<int32_t>& thresholdsDownlinkKbps,
  const nsTArray<int32_t>& thresholdsUplinkKbps,
  int32_t accessNetwork)
{
  DEBUG("setLinkCapacityReportingCriteria token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }

  ::std::vector<int32_t> downLinkKbps;
  for (uint32_t i = 0; i < thresholdsDownlinkKbps.Length(); i++) {
    downLinkKbps.push_back(thresholdsDownlinkKbps[i]);
  }

  ::std::vector<int32_t> uplinkKbps;
  for (uint32_t i = 0; i < thresholdsUplinkKbps.Length(); i++) {
    uplinkKbps.push_back(thresholdsUplinkKbps[i]);
  }

  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setLinkCapacityReportingCriteria(
        serial,
        hysteresisMs,
        hysteresisDlKbps,
        hysteresisUlKbps,
        downLinkKbps,
        uplinkKbps,
        (AccessNetwork)accessNetwork);
    if (ret.isOk()) {
      DEBUG("SetLinkCapacityReportingCriteria successfully");
      return NS_OK;
    } else {
      DEBUG("SetLinkCapacityReportingCriteria fail %s", ret.getMessage());
      ERROR_NS_OK("SetLinkCapacityReportingCriteria failed");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetSystemSelectionChannels(
  int32_t serial,
  bool specifyChannels,
  const nsTArray<RefPtr<nsIRadioAccessSpecifier>>& specifiers)
{
  DEBUG("setSystemSelectionChannels token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }

  ::std::vector<RadioAccessSpecifier_aidl> specifiers_array;
  for (uint32_t i = 0; i < specifiers.Length(); i++) {
    RadioAccessSpecifier_aidl specifier =
      convertToaidlRadioAccessSpecifier(specifiers[i]);
    specifiers_array.push_back(specifier);
  }

  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->setSystemSelectionChannels(
      serial, specifyChannels, specifiers_array);
    if (ret.isOk()) {
      DEBUG("SetSystemSelectionChannels successfully");
      return NS_OK;
    } else {
      DEBUG("SetSystemSelectionChannels fail %s", ret.getMessage());
      ERROR_NS_OK("SetSystemSelectionChannels failed");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetSignalStrengthReportingCriteria(
  int32_t serial,
  uint32_t mrType,
  bool isEnabled,
  int32_t hysteresisMs,
  int32_t hysteresisDb,
  const nsTArray<int32_t>& thresholdsDbm,
  int32_t accessNetwork)
{
  DEBUG("setSignalStrengthReportingCriteria token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }

  SignalThresholdInfo_aidl signalThresholdInfo;
  ::std::vector<SignalThresholdInfo_aidl> signalThresholdInfo_array;

  ::std::vector<int32_t> dbms;
  for (uint32_t i = 0; i < thresholdsDbm.Length(); i++) {
    dbms.push_back(thresholdsDbm[i]);
  }
  signalThresholdInfo.signalMeasurement = mrType;
  signalThresholdInfo.hysteresisMs = hysteresisMs;
  signalThresholdInfo.hysteresisDb = hysteresisDb;
  signalThresholdInfo.thresholds = dbms;
  signalThresholdInfo.isEnabled = isEnabled;
  signalThresholdInfo.ran = (AccessNetwork)accessNetwork;

  signalThresholdInfo_array.push_back(signalThresholdInfo);

  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->setSignalStrengthReportingCriteria(
        serial, signalThresholdInfo_array);
    if (ret.isOk()) {
      DEBUG("SetSignalStrengthReportingCriteria successfully");
      return NS_OK;
    } else {
      DEBUG("SetSignalStrengthReportingCriteria fail %s", ret.getMessage());
      ERROR_NS_OK("SetSignalStrengthReportingCriteria failed");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetPreferredNetworkTypeBitmap(int32_t serial)
{
  DEBUG("getPreferredNetworkTypeBitmap token:%d", serial);
  ERROR_NS_OK("GetPreferredNetworkTypeBitmap is not supported in aidl");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetPreferredNetworkTypeBitmap(
  int32_t serial,
  uint32_t networkTypeBitmap)
{
  DEBUG("setPreferredNetworkTypeBitmap token:%d", serial);
  ERROR_NS_OK("SetPreferredNetworkTypeBitmap is not supported in aidl");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::IsNrDualConnectivityEnabled(int32_t serial)
{
  DEBUG("isNrDualConnectivityEnabled token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->isNrDualConnectivityEnabled(serial);
    if (ret.isOk()) {
      DEBUG("isNrDualConnectivityEnabled successfully");
      return NS_OK;
    } else {
      DEBUG("isNrDualConnectivityEnabled fail %s", ret.getMessage());
      ERROR_NS_OK("isNrDualConnectivityEnabled failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetSystemSelectionChannels(int32_t serial)
{
  DEBUG("getSystemSelectionChannels token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getSystemSelectionChannels(serial);
    if (ret.isOk()) {
      DEBUG("getSystemSelectionChannels successfully");
      return NS_OK;
    } else {
      DEBUG("getSystemSelectionChannels fail %s", ret.getMessage());
      ERROR_NS_OK("getSystemSelectionChannels failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::GetAllowedNetworkTypesBitmap(int32_t serial)
{
  DEBUG("getAllowedNetworkTypesBitmap token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret =
      mRadioNetworkProxy->getAllowedNetworkTypesBitmap(serial);
    if (ret.isOk()) {
      DEBUG("getAllowedNetworkTypesBitmap successfully");
      return NS_OK;
    } else {
      DEBUG("getAllowedNetworkTypesBitmap fail %s", ret.getMessage());
      ERROR_NS_OK("getAllowedNetworkTypesBitmap failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::SetAllowedNetworkTypesBitmap(
  int32_t serial,
  int32_t networkTypeBitmask)
{
  DEBUG("setAllowedNetworkTypesBitmap token:%d", serial);
  if (mRadioNetworkProxy.get() == nullptr) {
    loadRadioNetworkProxy();
  }
  if (mRadioNetworkProxy.get() != nullptr) {
    ::ndk::ScopedAStatus ret = mRadioNetworkProxy->setAllowedNetworkTypesBitmap(
      serial, networkTypeBitmask);
    if (ret.isOk()) {
      DEBUG("setAllowedNetworkTypesBitmap successfully");
      return NS_OK;
    } else {
      DEBUG("setAllowedNetworkTypesBitmap fail %s", ret.getMessage());
      ERROR_NS_OK("setAllowedNetworkTypesBitmap failed");
    }
  }
  ERROR_NS_OK("mRadioNetworkProxy is null");
}

NS_IMETHODIMP
nsRadioProxyServiceManager::OnBinderDied(uint8_t serviceType)
{
  DEBUG("OnBinderDied serviceType:%d", serviceType);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsRadioProxyServiceManager, nsIRadioProxyServiceManager)

SliceInfo
nsRadioProxyServiceManager::convertToHalSliceInfo(nsISliceInfo* domSliceinfo)
{
  SliceInfo info;

  int32_t sst;
  domSliceinfo->GetSst(&sst);
  info.sliceServiceType = (uint8_t)sst;

  int32_t sliceDifferentiator;
  domSliceinfo->GetSliceDifferentiator(&sliceDifferentiator);
  info.sliceDifferentiator = sliceDifferentiator;

  int32_t mappedHplmnSst;
  domSliceinfo->GetMappedHplmnSst(&mappedHplmnSst);
  info.mappedHplmnSst = (uint8_t)(mappedHplmnSst);

  int32_t mappedHplmnSD;
  domSliceinfo->GetMappedHplmnSD(&mappedHplmnSD);
  info.mappedHplmnSd = mappedHplmnSD;

  int32_t status;
  domSliceinfo->GetStatus(&status);
  info.status = (uint8_t)(status);

  return info;
}

LinkAddress
nsRadioProxyServiceManager::convertToHalLinkAddress(
  nsILinkAddress* domLinkAddress)
{
  LinkAddress linkAddress;

  nsString address;
  domLinkAddress->GetAddress(address);
  linkAddress.address = NS_ConvertUTF16toUTF8(address).get();

  int32_t properties;
  domLinkAddress->GetProperties(&properties);
  linkAddress.addressProperties = properties;

  uint64_t deprecationTime;
  domLinkAddress->GetDeprecationTime(&deprecationTime);
  linkAddress.deprecationTime = deprecationTime;

  uint64_t expirationTime;
  domLinkAddress->GetExpirationTime(&expirationTime);
  linkAddress.expirationTime = expirationTime;

  return linkAddress;
}

DataProfileInfo
nsRadioProxyServiceManager::convertToHalDataProfile(
  nsIDataProfile* profile,
  nsITrafficDescriptor* trafficDescriptor)
{
  DataProfileInfo dataProfileInfo;

  int32_t profileId;
  profile->GetProfileId(&profileId);
  dataProfileInfo.profileId = profileId;

  nsString apn;
  profile->GetApn(apn);
  dataProfileInfo.apn = NS_ConvertUTF16toUTF8(apn).get();

  int32_t protocol;
  profile->GetProtocol(&protocol);
  dataProfileInfo.protocol = (PdpProtocolType)protocol;

  int32_t roamingProtocol;
  profile->GetRoamingProtocol(&roamingProtocol);
  dataProfileInfo.roamingProtocol = (PdpProtocolType)roamingProtocol;

  int32_t authType;
  profile->GetAuthType(&authType);
  dataProfileInfo.authType =
    (aidl::android::hardware::radio::data::ApnAuthType)authType;

  nsString user;
  profile->GetUser(user);
  dataProfileInfo.user = NS_ConvertUTF16toUTF8(user).get();

  nsString password;
  profile->GetPassword(password);
  dataProfileInfo.password = NS_ConvertUTF16toUTF8(password).get();

  int32_t type;
  profile->GetType(&type);
  dataProfileInfo.type = type;

  int32_t maxConnsTime;
  profile->GetMaxConnsTime(&maxConnsTime);
  dataProfileInfo.maxConnsTime = maxConnsTime;

  int32_t maxConns;
  profile->GetMaxConns(&maxConns);
  dataProfileInfo.maxConns = maxConns;

  int32_t waitTime;
  profile->GetWaitTime(&waitTime);
  dataProfileInfo.waitTime = waitTime;

  bool enabled;
  profile->GetEnabled(&enabled);
  dataProfileInfo.enabled = enabled;

  int32_t supportedApnTypesBitmap;
  profile->GetSupportedApnTypesBitmap(&supportedApnTypesBitmap);
  dataProfileInfo.supportedApnTypesBitmap = supportedApnTypesBitmap;

  int32_t bearerBitmap;
  profile->GetBearerBitmap(&bearerBitmap);
  dataProfileInfo.bearerBitmap = bearerBitmap;

  int32_t mtuV4;
  profile->GetMtuV4(&mtuV4);
  dataProfileInfo.mtuV4 = mtuV4;

  int32_t mtuV6;
  profile->GetMtuV6(&mtuV6);
  dataProfileInfo.mtuV6 = mtuV6;

  bool preferred;
  profile->GetPreferred(&preferred);
  dataProfileInfo.preferred = preferred;

  bool persistent;
  profile->GetPersistent(&persistent);
  dataProfileInfo.persistent = persistent;

  dataProfileInfo.alwaysOn = false;

  if (trafficDescriptor != nullptr) {
    nsString dnn;
    trafficDescriptor->GetDnn(dnn);
    dataProfileInfo.trafficDescriptor.dnn = NS_ConvertUTF16toUTF8(dnn).get();
    std::vector<uint8_t> appid;
    nsTArray<int32_t> nsAppId;
    trafficDescriptor->GetOsAppId(nsAppId);
    for (uint32_t i = 0; i < nsAppId.Length(); i++) {
      appid.push_back((uint8_t)nsAppId[i]);
    }
    dataProfileInfo.trafficDescriptor.osAppId->osAppId = appid;
  }

  return dataProfileInfo;
}

void
nsRadioProxyServiceManager::convertToCdmaSmsMessage(nsICdmaSmsMessage* inMsg,
                                                    CdmaSmsMessage& aMessage)
{

  int32_t teleserviceId;
  inMsg->GetTeleserviceId(&teleserviceId);
  aMessage.teleserviceId = teleserviceId;

  int32_t serviceCategory;
  inMsg->GetServiceCategory(&serviceCategory);
  aMessage.serviceCategory = serviceCategory;

  bool isServicePresent;
  inMsg->GetIsServicePresent(&isServicePresent);
  aMessage.isServicePresent = isServicePresent;

  nsICdmaSmsAddress* cdmaSmsAddress;
  inMsg->GetAddress(&cdmaSmsAddress);

  int32_t aDigitMode;
  cdmaSmsAddress->GetDigitMode(&aDigitMode);
  aMessage.address.digitMode = aDigitMode;

  int32_t aNumberMode;
  cdmaSmsAddress->GetNumberMode(&aNumberMode);
  aMessage.address.isNumberModeDataNetwork = (aNumberMode == 1);

  int32_t aNumberType;
  cdmaSmsAddress->GetNumberType(&aNumberType);
  aMessage.address.numberType = aNumberType;

  int32_t aNumberPlan;
  cdmaSmsAddress->GetNumberPlan(&aNumberPlan);
  aMessage.address.numberPlan = aNumberPlan;

  nsTArray<uint8_t> aAddressDigits;
  cdmaSmsAddress->GetDigits(aAddressDigits);
  for (uint32_t i = 0; i < aAddressDigits.Length(); i++) {
    aMessage.address.digits[i] = aAddressDigits[i];
  }

  nsICdmaSmsSubAddress* cdmaSmsSubAddress;
  inMsg->GetSubAddress(&cdmaSmsSubAddress);

  int32_t aSubAddressType;
  cdmaSmsSubAddress->GetSubAddressType(&aSubAddressType);
  aMessage.subAddress.subaddressType = aSubAddressType;

  bool aOdd;
  cdmaSmsSubAddress->GetOdd(&aOdd);
  aMessage.subAddress.odd = aOdd;

  nsTArray<uint8_t> aSubAddressDigits;
  cdmaSmsSubAddress->GetDigits(aSubAddressDigits);
  for (uint32_t i = 0; i < aSubAddressDigits.Length(); i++) {
    aMessage.subAddress.digits[i] = aSubAddressDigits[i];
  }

  nsTArray<uint8_t> bearerData;
  inMsg->GetBearerData(bearerData);
  for (uint32_t i = 0; i < bearerData.Length(); i++) {
    aMessage.bearerData[i] = bearerData[i];
  }
}

int32_t
nsRadioProxyServiceManager::convertAudioQuality(AudioQuality type)
{
  switch (type) {
    case AudioQuality::AMR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_AMR;
    case AudioQuality::AMR_WB:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_AMR_WB;
    case AudioQuality::GSM_EFR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_EFR;
    case AudioQuality::GSM_FR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_FR;
    case AudioQuality::GSM_HR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_HR;
    case AudioQuality::EVRC:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC;
    case AudioQuality::EVRC_B:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_B;
    case AudioQuality::EVRC_WB:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_WB;
    case AudioQuality::EVRC_NW:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_NW;
    default:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_UNSPECIFIED;
  }
}

int32_t
nsRadioProxyServiceManager::convertCallPresentation(int32_t state)
{
  switch (state) {
    case Call::PRESENTATION_ALLOWED:
      return nsIRilResponseResult::CALL_PRESENTATION_ALLOWED;
    case Call::PRESENTATION_RESTRICTED:
      return nsIRilResponseResult::CALL_PRESENTATION_RESTRICTED;
    case Call::PRESENTATION_UNKNOWN:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
    case Call::PRESENTATION_PAYPHONE:
      return nsIRilResponseResult::CALL_PRESENTATION_PAYPHONE;
    default:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertCallState(int32_t state)
{
  switch (state) {
    case Call::STATE_ACTIVE:
      return nsIRilResponseResult::CALL_STATE_ACTIVE;
    case Call::STATE_HOLDING:
      return nsIRilResponseResult::CALL_STATE_HOLDING;
    case Call::STATE_DIALING:
      return nsIRilResponseResult::CALL_STATE_DIALING;
    case Call::STATE_ALERTING:
      return nsIRilResponseResult::CALL_STATE_ALERTING;
    case Call::STATE_INCOMING:
      return nsIRilResponseResult::CALL_STATE_INCOMING;
    case Call::STATE_WAITING:
      return nsIRilResponseResult::CALL_STATE_WAITING;
    default:
      return nsIRilResponseResult::CALL_STATE_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertUusDcs(int32_t dcs)
{
  switch (dcs) {
    case UusInfo::UUS_DCS_USP:
      return nsIRilResponseResult::CALL_UUSDCS_USP;
    case UusInfo::UUS_DCS_OSIHLP:
      return nsIRilResponseResult::CALL_UUSDCS_OSIHLP;
    case UusInfo::UUS_DCS_X244:
      return nsIRilResponseResult::CALL_UUSDCS_X244;
    case UusInfo::UUS_DCS_RMCF:
      return nsIRilResponseResult::CALL_UUSDCS_RMCF;
    case UusInfo::UUS_DCS_IA5C:
      return nsIRilResponseResult::CALL_UUSDCS_IA5C;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSDCS_USP;
  }
}

int32_t
nsRadioProxyServiceManager::convertUusType(int32_t type)
{
  switch (type) {
    case UusInfo::UUS_TYPE_TYPE1_IMPLICIT:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
    case UusInfo::UUS_TYPE_TYPE1_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_REQUIRED;
    case UusInfo::UUS_TYPE_TYPE1_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_NOT_REQUIRED;
    case UusInfo::UUS_TYPE_TYPE2_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_REQUIRED;
    case UusInfo::UUS_TYPE_TYPE2_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_NOT_REQUIRED;
    case UusInfo::UUS_TYPE_TYPE3_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_REQUIRED;
    case UusInfo::UUS_TYPE_TYPE3_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_NOT_REQUIRED;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
  }
}

int32_t
nsRadioProxyServiceManager::convertClipState(ClipStatus status)
{
  switch (status) {
    case ClipStatus::CLIP_PROVISIONED:
      return nsIRilResponseResult::CLIP_STATE_PROVISIONED;
    case ClipStatus::CLIP_UNPROVISIONED:
      return nsIRilResponseResult::CLIP_STATE_UNPROVISIONED;
    case ClipStatus::UNKNOWN:
      return nsIRilResponseResult::CLIP_STATE_UNKNOWN;
    default:
      return nsIRilResponseResult::CLIP_STATE_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertCallForwardState(int32_t status)
{
  switch (status) {
    case CallForwardInfo::STATUS_DISABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
    case CallForwardInfo::STATUS_ENABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ENABLE;
    case CallForwardInfo::STATUS_INTERROGATE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_INTERROGATE;
    case CallForwardInfo::STATUS_REGISTRATION:
      return nsIRilResponseResult::CALL_FORWARD_STATE_REGISTRATION;
    case CallForwardInfo::STATUS_ERASURE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ERASURE;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
  }
}

int32_t
nsRadioProxyServiceManager::convertSrvccState(SrvccState state)
{
  switch (state) {
    case SrvccState::HANDOVER_STARTED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_STARTED;
    case SrvccState::HANDOVER_COMPLETED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_COMPLETED;
    case SrvccState::HANDOVER_FAILED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_FAILED;
    case SrvccState::HANDOVER_CANCELED:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_CANCELED;
    default:
      return nsIRilIndicationResult::SRVCC_STATE_HANDOVER_CANCELED;
  }
}

int32_t
nsRadioProxyServiceManager::convertUssdModeType(UssdModeType type)
{
  switch (type) {
    case UssdModeType::NOTIFY:
      return nsIRilIndicationResult::USSD_MODE_NOTIFY;
    case UssdModeType::REQUEST:
      return nsIRilIndicationResult::USSD_MODE_REQUEST;
    case UssdModeType::NW_RELEASE:
      return nsIRilIndicationResult::USSD_MODE_NW_RELEASE;
    case UssdModeType::LOCAL_CLIENT:
      return nsIRilIndicationResult::USSD_MODE_LOCAL_CLIENT;
    case UssdModeType::NOT_SUPPORTED:
      return nsIRilIndicationResult::USSD_MODE_NOT_SUPPORTED;
    case UssdModeType::NW_TIMEOUT:
      return nsIRilIndicationResult::USSD_MODE_NW_TIMEOUT;
    default:
      // TODO need confirmed the default value.
      return nsIRilIndicationResult::USSD_MODE_NW_TIMEOUT;
  }
}

int32_t
nsRadioProxyServiceManager::convertHardwareConfigType(int32_t type)
{
  switch (type) {
    case HardwareConfig::TYPE_MODEM:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM;
    case HardwareConfig::TYPE_SIM:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_SIM;
    default:
      return nsIRilIndicationResult::HW_CONFIG_TYPE_MODEM;
  }
}

int32_t
nsRadioProxyServiceManager::convertHardwareConfigState(int32_t state)
{
  switch (state) {
    case HardwareConfig::STATE_ENABLED:
      return nsIRilIndicationResult::HW_CONFIG_STATE_ENABLED;
    case HardwareConfig::STATE_STANDBY:
      return nsIRilIndicationResult::HW_CONFIG_STATE_STANDBY;
    case HardwareConfig::STATE_DISABLED:
      return nsIRilIndicationResult::HW_CONFIG_STATE_DISABLED;
    default:
      return nsIRilIndicationResult::HW_CONFIG_STATE_DISABLED;
  }
}

int32_t
nsRadioProxyServiceManager::convertRadioStateToNum(RadioState state)
{
  switch (state) {
    case RadioState::OFF:
      return nsIRilIndicationResult::RADIOSTATE_DISABLED;
    case RadioState::UNAVAILABLE:
      return nsIRilIndicationResult::RADIOSTATE_UNKNOWN;
    case RadioState::ON:
      return nsIRilIndicationResult::RADIOSTATE_ENABLED;
    default:
      return nsIRilIndicationResult::RADIOSTATE_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertRadioCapabilityPhase(int32_t value)
{
  switch (value) {
    case RadioCapability::PHASE_CONFIGURED:
      return nsIRilIndicationResult::RADIO_CP_CONFIGURED;
    case RadioCapability::PHASE_START:
      return nsIRilIndicationResult::RADIO_CP_START;
    case RadioCapability::PHASE_APPLY:
      return nsIRilIndicationResult::RADIO_CP_APPLY;
    case RadioCapability::PHASE_UNSOL_RSP:
      return nsIRilIndicationResult::RADIO_CP_UNSOL_RSP;
    case RadioCapability::PHASE_FINISH:
      return nsIRilIndicationResult::RADIO_CP_FINISH;
    default:
      return nsIRilIndicationResult::RADIO_CP_FINISH;
  }
}

int32_t
nsRadioProxyServiceManager::convertRadioAccessFamily(RadioAccessFamily value)
{
  switch (value) {
    case RadioAccessFamily::UNKNOWN:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UNKNOWN;
    case RadioAccessFamily::GPRS:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_GPRS;
    case RadioAccessFamily::EDGE:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EDGE;
    case RadioAccessFamily::UMTS:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UMTS;
    case RadioAccessFamily::IS95A:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_IS95A;
    case RadioAccessFamily::IS95B:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_IS95B;
    case RadioAccessFamily::ONE_X_RTT:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_ONE_X_RTT;
    case RadioAccessFamily::EVDO_0:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_0;
    case RadioAccessFamily::EVDO_A:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_A;
    case RadioAccessFamily::HSDPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSDPA;
    case RadioAccessFamily::HSUPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSUPA;
    case RadioAccessFamily::HSPA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSPA;
    case RadioAccessFamily::EVDO_B:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EVDO_B;
    case RadioAccessFamily::EHRPD:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_EHRPD;
    case RadioAccessFamily::LTE:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_LTE;
    case RadioAccessFamily::HSPAP:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_HSPAP;
    case RadioAccessFamily::GSM:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_GSM;
    case RadioAccessFamily::TD_SCDMA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_TD_SCDMA;
    case RadioAccessFamily::LTE_CA:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_LTE_CA;
    case RadioAccessFamily::NR:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_NR;
    default:
      return nsIRilIndicationResult::RADIO_ACCESS_FAMILY_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertRadioCapabilityStatus(int32_t state)
{
  switch (state) {
    case RadioCapability::STATUS_NONE:
      return nsIRilIndicationResult::RADIO_CP_STATUS_NONE;
    case RadioCapability::STATUS_SUCCESS:
      return nsIRilIndicationResult::RADIO_CP_STATUS_SUCCESS;
    case RadioCapability::STATUS_FAIL:
      return nsIRilIndicationResult::RADIO_CP_STATUS_FAIL;
    default:
      return nsIRilIndicationResult::RADIO_CP_STATUS_NONE;
  }
}
int32_t
nsRadioProxyServiceManager::convertRadioErrorToNum(RadioError error)
{
  switch (error) {
    case RadioError::NONE:
      return nsIRilResponseResult::RADIO_ERROR_NONE;
    case RadioError::RADIO_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_NOT_AVAILABLE;
    case RadioError::GENERIC_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
    case RadioError::PASSWORD_INCORRECT:
      return nsIRilResponseResult::RADIO_ERROR_PASSWOR_INCORRECT;
    case RadioError::SIM_PIN2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PIN2;
    case RadioError::SIM_PUK2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PUK2;
    case RadioError::REQUEST_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_NOT_SUPPORTED;
    case RadioError::CANCELLED:
      return nsIRilResponseResult::RADIO_ERROR_CANCELLED;
    case RadioError::OP_NOT_ALLOWED_DURING_VOICE_CALL:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_DURING_VOICE_CALL;
    case RadioError::OP_NOT_ALLOWED_BEFORE_REG_TO_NW:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
    case RadioError::SMS_SEND_FAIL_RETRY:
      return nsIRilResponseResult::RADIO_ERROR_SMS_SEND_FAIL_RETRY;
    case RadioError::SIM_ABSENT:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ABSENT;
    case RadioError::SUBSCRIPTION_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_AVAILABLE;
    case RadioError::MODE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_MODE_NOT_SUPPORTED;
    case RadioError::FDN_CHECK_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_FDN_CHECK_FAILURE;
    case RadioError::ILLEGAL_SIM_OR_ME:
      return nsIRilResponseResult::RADIO_ERROR_ILLEGAL_SIM_OR_ME;
    case RadioError::MISSING_RESOURCE:
      return nsIRilResponseResult::RADIO_ERROR_MISSING_RESOURCE;
    case RadioError::NO_SUCH_ELEMENT:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ELEMENT;
    case RadioError::DIAL_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_USSD;
    case RadioError::DIAL_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_SS;
    case RadioError::DIAL_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_DIAL;
    case RadioError::USSD_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_DIAL;
    case RadioError::USSD_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_SS;
    case RadioError::USSD_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_USSD;
    case RadioError::SS_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_DIAL;
    case RadioError::SS_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_USSD;
    case RadioError::SUBSCRIPTION_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_SUPPORTED;
    case RadioError::SS_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_SS;
    case RadioError::LCE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_LCE_NOT_SUPPORTED;
    case RadioError::NO_MEMORY:
      return nsIRilResponseResult::RADIO_ERROR_NO_MEMORY;
    case RadioError::INTERNAL_ERR:
      return nsIRilResponseResult::RADIO_ERROR_INTERNAL_ERR;
    case RadioError::SYSTEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SYSTEM_ERR;
    case RadioError::MODEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_MODEM_ERR;
    case RadioError::INVALID_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_STATE;
    case RadioError::NO_RESOURCES:
      return nsIRilResponseResult::RADIO_ERROR_NO_RESOURCES;
    case RadioError::SIM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ERR;
    case RadioError::INVALID_ARGUMENTS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_ARGUMENTS;
    case RadioError::INVALID_SIM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SIM_STATE;
    case RadioError::INVALID_MODEM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_MODEM_STATE;
    case RadioError::INVALID_CALL_ID:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_CALL_ID;
    case RadioError::NO_SMS_TO_ACK:
      return nsIRilResponseResult::RADIO_ERROR_NO_SMS_TO_ACK;
    case RadioError::NETWORK_ERR:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_ERR;
    case RadioError::REQUEST_RATE_LIMITED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_RATE_LIMITED;
    case RadioError::SIM_BUSY:
      return nsIRilResponseResult::RADIO_ERROR_SIM_BUSY;
    case RadioError::SIM_FULL:
      return nsIRilResponseResult::RADIO_ERROR_SIM_FULL;
    case RadioError::NETWORK_REJECT:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_REJECT;
    case RadioError::OPERATION_NOT_ALLOWED:
      return nsIRilResponseResult::RADIO_ERROR_OPERATION_NOT_ALLOWED;
    case RadioError::EMPTY_RECORD:
      return nsIRilResponseResult::RADIO_ERROR_EMPTY_RECORD;
    case RadioError::INVALID_SMS_FORMAT:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMS_FORMAT;
    case RadioError::ENCODING_ERR:
      return nsIRilResponseResult::RADIO_ERROR_ENCODING_ERR;
    case RadioError::INVALID_SMSC_ADDRESS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMSC_ADDRESS;
    case RadioError::NO_SUCH_ENTRY:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ENTRY;
    case RadioError::NETWORK_NOT_READY:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_NOT_READY;
    case RadioError::NOT_PROVISIONED:
      return nsIRilResponseResult::RADIO_ERROR_NOT_PROVISIONED;
    case RadioError::NO_SUBSCRIPTION:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUBSCRIPTION;
    case RadioError::NO_NETWORK_FOUND:
      return nsIRilResponseResult::RADIO_ERROR_NO_NETWORK_FOUND;
    case RadioError::DEVICE_IN_USE:
      return nsIRilResponseResult::RADIO_ERROR_DEVICE_IN_USE;
    case RadioError::ABORTED:
      return nsIRilResponseResult::RADIO_ERROR_ABORTED;
    case RadioError::INVALID_RESPONSE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_RESPONSE;
    default:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
  }
}

void
nsRadioProxyServiceManager::loadRadioDataProxy()
{
  DEBUG("try to load mRadioDataProxy");
  if (mRadioDataProxy == nullptr) {
    DEBUG("new mRadioDataProxy");
    const std::string instance = std::string() + IRadioData::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioDataProxy = IRadioData::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioDataProxy == nullptr) {
      mRadioDataProxy = IRadioData::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }
    if (mRadioDataDeathRecipient == nullptr) {
      mRadioDataDeathRecipient =
        AIBinder_DeathRecipient_new(&RadioDataProxyDeathNotifier);
    }
    binder_status_t binder_status =
      AIBinder_linkToDeath(mRadioDataProxy->asBinder().get(),
                           mRadioDataDeathRecipient,
                           reinterpret_cast<void*>(this));
    if (binder_status != STATUS_OK) {
      DEBUG("Failed to link to death, status %d",
            static_cast<int>(binder_status));
      mRadioDataProxy = nullptr;
      return;
    }
    mRadioInd_data = ndk::SharedRefBase::make<RadioDataIndication>(*this);
    mRadioRsp_data = ndk::SharedRefBase::make<RadioDataResponse>(*this);
    mRadioDataProxy->setResponseFunctions(mRadioRsp_data, mRadioInd_data);
  } else {
    DEBUG("mRadioDataProxy is ready.");
  }
}

int32_t
nsRadioProxyServiceManager::convertRadioTechnology(RadioTechnology rat)
{
  switch (rat) {
    case RadioTechnology::UNKNOWN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
    case RadioTechnology::GPRS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GPRS;
    case RadioTechnology::EDGE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EDGE;
    case RadioTechnology::UMTS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UMTS;
    case RadioTechnology::IS95A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95A;
    case RadioTechnology::IS95B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95B;
    case RadioTechnology::ONE_X_RTT:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_1XRTT;
    case RadioTechnology::EVDO_0:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDO0;
    case RadioTechnology::EVDO_A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOA;
    case RadioTechnology::HSDPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSDPA;
    case RadioTechnology::HSUPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSUPA;
    case RadioTechnology::HSPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPA;
    case RadioTechnology::EVDO_B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOB;
    case RadioTechnology::EHRPD:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EHRPD;
    case RadioTechnology::LTE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE;
    case RadioTechnology::HSPAP:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPAP;
    case RadioTechnology::GSM:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GSM;
    case RadioTechnology::TD_SCDMA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_TD_SCDMA;
    case RadioTechnology::IWLAN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IWLAN;
    case RadioTechnology::LTE_CA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE_CA;
    case RadioTechnology::NR:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_NR;
    default:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertRegState(RegState_aidl state)
{
  switch (state) {
    case RegState_aidl::NOT_REG_MT_NOT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP;
    case RegState_aidl::REG_HOME:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_HOME;
    case RegState_aidl::NOT_REG_MT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP;
    case RegState_aidl::REG_DENIED:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED;
    case RegState_aidl::UNKNOWN:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
    case RegState_aidl::REG_ROAMING:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_ROAMING;
    case RegState_aidl::NOT_REG_MT_NOT_SEARCHING_OP_EM:
      return nsIRilResponseResult::
        RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP_EM;
    case RegState_aidl::NOT_REG_MT_SEARCHING_OP_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP_EM;
    case RegState_aidl::REG_DENIED_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED_EM;
    case RegState_aidl::UNKNOWN_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN_EM;
    default:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
  }
}

RefPtr<nsNrIndicators>
nsRadioProxyServiceManager::convertNrIndicators(
  const NrIndicators_aidl* aNrIndicators)
{
  RefPtr<nsNrIndicators> nrIndicators =
    new nsNrIndicators(aNrIndicators->isEndcAvailable,
                       aNrIndicators->isDcNrRestricted,
                       aNrIndicators->isNrAvailable);
  return nrIndicators;
}

RefPtr<nsLteVopsInfo>
nsRadioProxyServiceManager::convertVopsInfo(const LteVopsInfo_aidl* aVopsInfo)
{
  RefPtr<nsLteVopsInfo> vopsInfo = new nsLteVopsInfo(
    aVopsInfo->isVopsSupported, aVopsInfo->isEmcBearerSupported);
  return vopsInfo;
}

RefPtr<nsNrVopsInfo>
nsRadioProxyServiceManager::convertNrVopsInfo(
  const NrVopsInfo_aidl* aNrVopsInfo)
{
  RefPtr<nsNrVopsInfo> vopsInfo =
    new nsNrVopsInfo((int32_t)aNrVopsInfo->vopsSupported,
                     (int32_t)aNrVopsInfo->emcSupported,
                     (int32_t)aNrVopsInfo->emfSupported);
  return vopsInfo;
}

int32_t
nsRadioProxyServiceManager::convertPhoneRestrictedState(
  PhoneRestrictedState state)
{
  switch (state) {
    case PhoneRestrictedState::NONE:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_NONE;
    case PhoneRestrictedState::CS_EMERGENCY:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_EMERGENCY;
    case PhoneRestrictedState::CS_NORMAL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_NORMAL;
    case PhoneRestrictedState::CS_ALL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_CS_ALL;
    case PhoneRestrictedState::PS_ALL:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_PS_ALL;
    default:
      return nsIRilIndicationResult::PHONE_RESTRICTED_STATE_NONE;
  }
}

int32_t
nsRadioProxyServiceManager::convertOperatorState(int32_t status)
{
  switch (status) {
    case OperatorInfo_aidl::STATUS_UNKNOWN:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
    case OperatorInfo_aidl::STATUS_AVAILABLE:
      return nsIRilResponseResult::QAN_STATE_AVAILABLE;
    case OperatorInfo_aidl::STATUS_CURRENT:
      return nsIRilResponseResult::QAN_STATE_CURRENT;
    case OperatorInfo_aidl::STATUS_FORBIDDEN:
      return nsIRilResponseResult::QAN_STATE_FORBIDDEN;
    default:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
  }
}

int32_t
nsRadioProxyServiceManager::convertHalNetworkTypeBitMask(
  int32_t networkTypeBitmap)
{
  int32_t networkTypeRaf = 0;
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::GSM) !=
      0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_GSM - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::GPRS) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_GPRS - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::EDGE) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EDGE - 1));
  }
  // convert both IS95A/IS95B to CDMA as network mode doesn't support CDMA
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::IS95A) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IS95A - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::IS95B) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IS95B - 1));
  }
  if ((networkTypeBitmap & (int32_t)::aidl::android::hardware::radio::
                             RadioAccessFamily::ONE_X_RTT) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_1XRTT - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::EVDO_0) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDO0 - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::EVDO_A) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::EVDO_B) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOB - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::EHRPD) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EHRPD - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::HSUPA) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSUPA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::HSDPA) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSDPA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::HSPA) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSPA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::HSPAP) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSPAP - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::UMTS) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_UMTS - 1));
  }
  if ((networkTypeBitmap & (int32_t)::aidl::android::hardware::radio::
                             RadioAccessFamily::TD_SCDMA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_TD_SCDMA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::LTE) !=
      0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_LTE - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::LTE_CA) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_LTE_CA - 1));
  }
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::NR) != 0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_NR));
  }
  // Android use "if ((raf & (1 << ServiceState.RIL_RADIO_TECHNOLOGY_IWLAN)) !=
  // 0): to check bitmap. Is it  ok to use RadioAccessFamily.IWLAN? We need to
  // check whether it is ok on QCOM device.
  if ((networkTypeBitmap &
       (int32_t)::aidl::android::hardware::radio::RadioAccessFamily::IWLAN) !=
      0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IWLAN - 1));
  }
  return (networkTypeRaf == 0)
           ? nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN
           : networkTypeRaf;
}

int32_t
nsRadioProxyServiceManager::convertCellIdentitytoCellInfoType(
  aidl::android::hardware::radio::network::CellIdentity::Tag tag)
{
  switch (tag) {
    case aidl::android::hardware::radio::network::CellIdentity::Tag::gsm:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case aidl::android::hardware::radio::network::CellIdentity::Tag::lte:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case aidl::android::hardware::radio::network::CellIdentity::Tag::wcdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case aidl::android::hardware::radio::network::CellIdentity::Tag::tdscdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    case aidl::android::hardware::radio::network::CellIdentity::Tag::cdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case aidl::android::hardware::radio::network::CellIdentity::Tag::nr:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_NR;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

RefPtr<nsCellIdentity>
nsRadioProxyServiceManager::convertCellIdentity(
  const CellIdentity_aidl* aCellIdentity)
{
  int32_t cellInfoType =
    convertCellIdentitytoCellInfoType(aCellIdentity->getTag());
  if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM) {
    RefPtr<nsCellIdentityGsm> cellIdentityGsm = convertCellIdentityGsm(
      &(aCellIdentity->get<
        aidl::android::hardware::radio::network::CellIdentity::Tag::gsm>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityGsm);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE) {
    RefPtr<nsCellIdentityLte> cellIdentityLte = convertCellIdentityLte(
      &(aCellIdentity->get<
        aidl::android::hardware::radio::network::CellIdentity::Tag::lte>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityLte);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA) {
    RefPtr<nsCellIdentityWcdma> cellIdentityWcdma = convertCellIdentityWcdma(
      &(aCellIdentity->get<
        aidl::android::hardware::radio::network::CellIdentity::Tag::wcdma>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityWcdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA) {
    RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
      convertCellIdentityTdScdma(
        &(aCellIdentity->get<aidl::android::hardware::radio::network::
                               CellIdentity::Tag::tdscdma>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityTdScdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA) {
    RefPtr<nsCellIdentityCdma> cellIdentityCdma = convertCellIdentityCdma(
      &(aCellIdentity->get<
        aidl::android::hardware::radio::network::CellIdentity::Tag::cdma>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityCdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_NR) {
    RefPtr<nsCellIdentityNr> cellIdentityNr = convertCellIdentityNr(
      &(aCellIdentity->get<
        aidl::android::hardware::radio::network::CellIdentity::Tag::nr>()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityNr);
    return cellIdentity;
  }
  return nullptr;
}

RefPtr<nsCellIdentityOperatorNames>
nsRadioProxyServiceManager::convertCellIdentityOperatorNames(
  const OperatorInfo_aidl& aOperatorNames)
{
  RefPtr<nsCellIdentityOperatorNames> names = new nsCellIdentityOperatorNames(
    NS_ConvertUTF8toUTF16(aOperatorNames.alphaLong.c_str()),
    NS_ConvertUTF8toUTF16(aOperatorNames.alphaShort.c_str()));
  return names;
}

RefPtr<nsCellIdentityGsm>
nsRadioProxyServiceManager::convertCellIdentityGsm(
  const CellIdentityGsm_aidl* aCellIdentityGsm)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->mnc.c_str());
  int32_t lac = aCellIdentityGsm->lac;
  int32_t cid = aCellIdentityGsm->cid;
  int32_t arfcn = aCellIdentityGsm->arfcn;
  int32_t bsic = (int32_t)aCellIdentityGsm->bsic;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityGsm->operatorNames);
  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityGsm->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityGsm->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityGsm> gsm = new nsCellIdentityGsm(
    mcc, mnc, lac, cid, arfcn, bsic, operatorNames, addlPlmns);
  return gsm;
}

RefPtr<nsCellIdentityCsgInfo>
nsRadioProxyServiceManager::convertCsgInfo(
  const ClosedSubscriberGroupInfo_aidl* aCsgInfo)
{
  if (aCsgInfo != nullptr) {
    RefPtr<nsCellIdentityCsgInfo> csgInfo = new nsCellIdentityCsgInfo(
      aCsgInfo->csgIndication,
      NS_ConvertUTF8toUTF16(aCsgInfo->homeNodebName.c_str()),
      aCsgInfo->csgIdentity);
    return csgInfo;
  }
  return nullptr;
}

RefPtr<nsCellIdentityLte>
nsRadioProxyServiceManager::convertCellIdentityLte(
  const CellIdentityLte_aidl* aCellIdentityLte)
{
  nsString mcc;
  nsString mnc;
  int32_t ci;
  int32_t pci;
  int32_t tac;
  int32_t earfcn;
  int32_t bandwidth = 0;
  RefPtr<nsCellIdentityOperatorNames> operatorNames;

  mcc = NS_ConvertUTF8toUTF16(aCellIdentityLte->mcc.c_str());
  mnc = NS_ConvertUTF8toUTF16(aCellIdentityLte->mnc.c_str());
  ci = aCellIdentityLte->ci;
  pci = aCellIdentityLte->pci;
  tac = aCellIdentityLte->tac;
  earfcn = aCellIdentityLte->earfcn;
  operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityLte->operatorNames);
  bandwidth = aCellIdentityLte->bandwidth;

  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityLte->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityLte->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo = nullptr;
  if (aCellIdentityLte->csgInfo.has_value()) {
    csgInfo = convertCsgInfo(&aCellIdentityLte->csgInfo.value());
  }
  nsTArray<int32_t> bands;
  uint32_t bandsLen = aCellIdentityLte->bands.size();
  for (uint32_t i = 0; i < bandsLen; i++) {
    int32_t band = (int32_t)aCellIdentityLte->bands[i];
    bands.AppendElement(band);
  }

  RefPtr<nsCellIdentityLte> lte = new nsCellIdentityLte(mcc,
                                                        mnc,
                                                        ci,
                                                        pci,
                                                        tac,
                                                        earfcn,
                                                        operatorNames,
                                                        bandwidth,
                                                        addlPlmns,
                                                        csgInfo,
                                                        bands);
  return lte;
}

RefPtr<nsCellIdentityWcdma>
nsRadioProxyServiceManager::convertCellIdentityWcdma(
  const CellIdentityWcdma_aidl* aCellIdentityWcdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->mnc.c_str());
  int32_t lac = aCellIdentityWcdma->lac;
  int32_t cid = aCellIdentityWcdma->cid;
  int32_t psc = aCellIdentityWcdma->psc;
  int32_t uarfcn = aCellIdentityWcdma->uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityWcdma->operatorNames);
  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityWcdma->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityWcdma->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo = nullptr;
  if (aCellIdentityWcdma->csgInfo.has_value()) {
    csgInfo = convertCsgInfo(&aCellIdentityWcdma->csgInfo.value());
  }

  RefPtr<nsCellIdentityWcdma> wcdma = new nsCellIdentityWcdma(
    mcc, mnc, lac, cid, psc, uarfcn, operatorNames, addlPlmns, csgInfo);
  return wcdma;
}

RefPtr<nsCellIdentityTdScdma>
nsRadioProxyServiceManager::convertCellIdentityTdScdma(
  const CellIdentityTdscdma_aidl* aCellIdentityTdScdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->mnc.c_str());
  int32_t lac = aCellIdentityTdScdma->lac;
  int32_t cid = aCellIdentityTdScdma->cid;
  int32_t cpid = aCellIdentityTdScdma->cpid;
  int32_t uarfcn = aCellIdentityTdScdma->uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityTdScdma->operatorNames);

  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityTdScdma->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo = nullptr;
  if (aCellIdentityTdScdma->csgInfo.has_value()) {
    csgInfo = convertCsgInfo(&aCellIdentityTdScdma->csgInfo.value());
  }

  RefPtr<nsCellIdentityTdScdma> tdscdma = new nsCellIdentityTdScdma(
    mcc, mnc, lac, cid, cpid, operatorNames, uarfcn, addlPlmns, csgInfo);
  return tdscdma;
}

RefPtr<nsCellIdentityCdma>
nsRadioProxyServiceManager::convertCellIdentityCdma(
  const CellIdentityCdma_aidl* aCellIdentityCdma)
{

  int32_t networkId = aCellIdentityCdma->networkId;
  int32_t systemId = aCellIdentityCdma->systemId;
  int32_t baseStationId = aCellIdentityCdma->baseStationId;
  int32_t longitude = aCellIdentityCdma->longitude;
  int32_t latitude = aCellIdentityCdma->latitude;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityCdma->operatorNames);

  RefPtr<nsCellIdentityCdma> cdma = new nsCellIdentityCdma(
    networkId, systemId, baseStationId, longitude, latitude, operatorNames);
  return cdma;
}

RefPtr<nsCellIdentityNr>
nsRadioProxyServiceManager::convertCellIdentityNr(
  const CellIdentityNr_aidl* aCellIdentityNr)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityNr->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityNr->mnc.c_str());
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityNr->operatorNames);

  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityNr->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityNr->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  nsTArray<int32_t> bands;
  uint32_t bandsLen = aCellIdentityNr->bands.size();
  for (uint32_t i = 0; i < bandsLen; i++) {
    int32_t band = (int32_t)aCellIdentityNr->bands[i];
    bands.AppendElement(band);
  }

  RefPtr<nsCellIdentityNr> nr = new nsCellIdentityNr(mcc,
                                                     mnc,
                                                     aCellIdentityNr->nci,
                                                     aCellIdentityNr->pci,
                                                     aCellIdentityNr->tac,
                                                     aCellIdentityNr->nrarfcn,
                                                     operatorNames,
                                                     addlPlmns,
                                                     bands);
  return nr;
}

int32_t
nsRadioProxyServiceManager::convertCellInfoTypeToRil(
  aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag tag)
{
  switch (tag) {
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      gsm:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      cdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      lte:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      wcdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      tdscdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    case aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::Tag::
      nr:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_NR;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

int32_t
nsRadioProxyServiceManager::convertConnectionStatus(
  CellConnectionStatus_aidl status)
{
  switch (status) {
    case CellConnectionStatus_aidl::NONE:
      return nsICellConnectionStatus::RADIO_CELL_CONNECTION_STATUS_NONE;
    case CellConnectionStatus_aidl::PRIMARY_SERVING:
      return nsICellConnectionStatus::
        RADIO_CELL_CONNECTION_STATUS_PRIMARY_SERVING;
    case CellConnectionStatus_aidl::SECONDARY_SERVING:
      return nsICellConnectionStatus::
        RADIO_CELL_CONNECTION_STATUS_SECONDARY_SERVING;
    default:
      return -1;
  }
}

RefPtr<nsRilCellInfo>
nsRadioProxyServiceManager::convertRilCellInfo(const CellInfo_aidl* aCellInfo)
{
  int32_t cellInfoType =
    convertCellInfoTypeToRil(aCellInfo->ratSpecificInfo.getTag());
  bool registered = aCellInfo->registered;
  int32_t connectionStatus =
    convertConnectionStatus(aCellInfo->connectionStatus);
  int32_t timeStampType = -1;
  uint64_t timeStamp = 0;

  if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM) {
    RefPtr<nsCellInfoGsm> cellInfoGsm = convertCellInfoGsm(
      &aCellInfo->ratSpecificInfo.get<aidl::android::hardware::radio::network::
                                        CellInfoRatSpecificInfo::Tag::gsm>());
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoGsm,
                                                             connectionStatus);
    return rilCellInfoGsm;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE) {
    RefPtr<nsCellInfoLte> cellInfoLte = convertCellInfoLte(
      &aCellInfo->ratSpecificInfo.get<aidl::android::hardware::radio::network::
                                        CellInfoRatSpecificInfo::Tag::lte>());
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoLte,
                                                             connectionStatus);
    return rilCellInfoLte;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma = convertCellInfoWcdma(
      &aCellInfo->ratSpecificInfo.get<aidl::android::hardware::radio::network::
                                        CellInfoRatSpecificInfo::Tag::wcdma>());
    RefPtr<nsRilCellInfo> rilCellInfoWcdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoWcdma,
                        connectionStatus);
    return rilCellInfoWcdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma = convertCellInfoTdScdma(
      &aCellInfo->ratSpecificInfo
         .get<aidl::android::hardware::radio::network::CellInfoRatSpecificInfo::
                Tag::tdscdma>());
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoTdScdma,
                        connectionStatus);
    return rilCellInfoTdScdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA) {
    RefPtr<nsCellInfoCdma> cellInfoCdma = convertCellInfoCdma(
      &aCellInfo->ratSpecificInfo.get<aidl::android::hardware::radio::network::
                                        CellInfoRatSpecificInfo::Tag::cdma>());
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(cellInfoType,
                                                              registered,
                                                              timeStampType,
                                                              timeStamp,
                                                              cellInfoCdma,
                                                              connectionStatus);
    return rilCellInfoCdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_NR) {
    RefPtr<nsCellInfoNr> cellInfoNr = convertCellInfoNr(
      &aCellInfo->ratSpecificInfo.get<aidl::android::hardware::radio::network::
                                        CellInfoRatSpecificInfo::Tag::nr>());
    RefPtr<nsRilCellInfo> rilCellInfoNr = new nsRilCellInfo(cellInfoType,
                                                            registered,
                                                            timeStampType,
                                                            timeStamp,
                                                            cellInfoNr,
                                                            connectionStatus);
    return rilCellInfoNr;
  } else {
    return nullptr;
  }
}

RefPtr<nsCellInfoGsm>
nsRadioProxyServiceManager::convertCellInfoGsm(
  const CellInfoGsm_aidl* aCellInfoGsm)
{
  RefPtr<nsCellIdentityGsm> cellIdentityGsm =
    convertCellIdentityGsm(&aCellInfoGsm->cellIdentityGsm);
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aCellInfoGsm->signalStrengthGsm);
  RefPtr<nsCellInfoGsm> cellInfoGsm =
    new nsCellInfoGsm(cellIdentityGsm, gsmSignalStrength);

  return cellInfoGsm;
}

RefPtr<nsCellInfoLte>
nsRadioProxyServiceManager::convertCellInfoLte(
  const CellInfoLte_aidl* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte(&aCellInfoLte->cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aCellInfoLte->signalStrengthLte);
  RefPtr<nsCellConfigLte> cellConfigLte = nullptr;
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, cellConfigLte);

  return cellInfoLte;
}

RefPtr<nsCellInfoWcdma>
nsRadioProxyServiceManager::convertCellInfoWcdma(
  const CellInfoWcdma_aidl* aCellInfoWcdma)
{
  RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
    convertCellIdentityWcdma(&aCellInfoWcdma->cellIdentityWcdma);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength(aCellInfoWcdma->signalStrengthWcdma);
  RefPtr<nsCellInfoWcdma> cellInfoWcdma =
    new nsCellInfoWcdma(cellIdentityWcdma, wcdmaSignalStrength);

  return cellInfoWcdma;
}

RefPtr<nsCellInfoTdScdma>
nsRadioProxyServiceManager::convertCellInfoTdScdma(
  const CellInfoTdscdma_aidl* aCellInfoTdscdma)
{
  RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
    convertCellIdentityTdScdma(&aCellInfoTdscdma->cellIdentityTdscdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength(aCellInfoTdscdma->signalStrengthTdscdma);
  RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
    new nsCellInfoTdScdma(cellIdentityTdScdma, tdscdmaSignalStrength);

  return cellInfoTdScdma;
}

RefPtr<nsCellInfoCdma>
nsRadioProxyServiceManager::convertCellInfoCdma(
  const CellInfoCdma_aidl* aCellInfoCdma)
{
  RefPtr<nsCellIdentityCdma> cellIdentityCdma =
    convertCellIdentityCdma(&aCellInfoCdma->cellIdentityCdma);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aCellInfoCdma->signalStrengthCdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aCellInfoCdma->signalStrengthEvdo);
  RefPtr<nsCellInfoCdma> cellInfoCdma = new nsCellInfoCdma(
    cellIdentityCdma, cdmaSignalStrength, evdoSignalStrength);

  return cellInfoCdma;
}

RefPtr<nsCellInfoNr>
nsRadioProxyServiceManager::convertCellInfoNr(
  const CellInfoNr_aidl* aCellInfoNr)
{
  RefPtr<nsCellIdentityNr> cellIdentityNr =
    convertCellIdentityNr(&aCellInfoNr->cellIdentityNr);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength(&aCellInfoNr->signalStrengthNr);
  RefPtr<nsCellInfoNr> cellInfoNr =
    new nsCellInfoNr(cellIdentityNr, nrSignalStrength);

  return cellInfoNr;
}

RefPtr<nsSignalStrength>
nsRadioProxyServiceManager::convertSignalStrength(
  const SignalStrength_aidl& aSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aSignalStrength.gsm);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aSignalStrength.cdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aSignalStrength.evdo);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength(aSignalStrength.wcdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength(aSignalStrength.tdscdma);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aSignalStrength.lte);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength(&aSignalStrength.nr);

  RefPtr<nsSignalStrength> signalStrength =
    new nsSignalStrength(gsmSignalStrength,
                         cdmaSignalStrength,
                         evdoSignalStrength,
                         lteSignalStrength,
                         tdscdmaSignalStrength,
                         wcdmaSignalStrength,
                         nrSignalStrength);

  return signalStrength;
}

RefPtr<nsGsmSignalStrength>
nsRadioProxyServiceManager::convertGsmSignalStrength(
  const GsmSignalStrength_aidl& aGsmSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    new nsGsmSignalStrength(aGsmSignalStrength.signalStrength,
                            aGsmSignalStrength.bitErrorRate,
                            aGsmSignalStrength.timingAdvance);

  return gsmSignalStrength;
}

RefPtr<nsCdmaSignalStrength>
nsRadioProxyServiceManager::convertCdmaSignalStrength(
  const CdmaSignalStrength_aidl& aCdmaSignalStrength)
{
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    new nsCdmaSignalStrength(aCdmaSignalStrength.dbm, aCdmaSignalStrength.ecio);

  return cdmaSignalStrength;
}

RefPtr<nsEvdoSignalStrength>
nsRadioProxyServiceManager::convertEvdoSignalStrength(
  const EvdoSignalStrength_aidl& aEvdoSignalStrength)
{
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    new nsEvdoSignalStrength(aEvdoSignalStrength.dbm,
                             aEvdoSignalStrength.ecio,
                             aEvdoSignalStrength.signalNoiseRatio);

  return evdoSignalStrength;
}

RefPtr<nsWcdmaSignalStrength>
nsRadioProxyServiceManager::convertWcdmaSignalStrength(
  const WcdmaSignalStrength_aidl& aWcdmaSignalStrength)
{
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    new nsWcdmaSignalStrength(aWcdmaSignalStrength.signalStrength,
                              aWcdmaSignalStrength.bitErrorRate,
                              aWcdmaSignalStrength.rscp,
                              aWcdmaSignalStrength.ecno);

  return wcdmaSignalStrength;
}

RefPtr<nsTdScdmaSignalStrength>
nsRadioProxyServiceManager::convertTdScdmaSignalStrength(
  const TdscdmaSignalStrength_aidl& aTdScdmaSignalStrength)
{
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    new nsTdScdmaSignalStrength(aTdScdmaSignalStrength.signalStrength,
                                aTdScdmaSignalStrength.bitErrorRate,
                                aTdScdmaSignalStrength.rscp);

  return tdscdmaSignalStrength;
}

RefPtr<nsLteSignalStrength>
nsRadioProxyServiceManager::convertLteSignalStrength(
  const LteSignalStrength_aidl& aLteSignalStrength)
{
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    new nsLteSignalStrength(aLteSignalStrength.signalStrength,
                            aLteSignalStrength.rsrp,
                            aLteSignalStrength.rsrq,
                            aLteSignalStrength.rssnr,
                            aLteSignalStrength.cqi,
                            aLteSignalStrength.timingAdvance,
                            aLteSignalStrength.cqiTableIndex);

  return lteSignalStrength;
}

RefPtr<nsNrSignalStrength>
nsRadioProxyServiceManager::convertNrSignalStrength(
  const NrSignalStrength_aidl* aNrSignalStrength)
{
  nsTArray<int32_t> csiCqiReport;
  uint32_t csiLen = aNrSignalStrength->csiCqiReport.size();
  for (uint32_t i = 0; i < csiLen; i++) {
    int32_t cqi = (int32_t)aNrSignalStrength->csiCqiReport[i];
    csiCqiReport.AppendElement(cqi);
  }
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    new nsNrSignalStrength(aNrSignalStrength->csiRsrp,
                           aNrSignalStrength->csiRsrq,
                           aNrSignalStrength->csiSinr,
                           aNrSignalStrength->ssRsrp,
                           aNrSignalStrength->ssRsrq,
                           aNrSignalStrength->ssSinr,
                           aNrSignalStrength->csiCqiTableIndex,
                           csiCqiReport);

  return nrSignalStrength;
}

RadioAccessSpecifier_aidl
nsRadioProxyServiceManager::convertToaidlRadioAccessSpecifier(
  nsIRadioAccessSpecifier* specifier)
{
  RadioAccessSpecifier_aidl rasInAidlFormat;

  int32_t nsRadioAccessNetwork;
  specifier->GetRadioAccessNetwork(&nsRadioAccessNetwork);
  rasInAidlFormat.accessNetwork = (AccessNetwork)nsRadioAccessNetwork;

  nsTArray<int32_t> nsGeranBands;
  nsTArray<int32_t> nsUtranBands;
  nsTArray<int32_t> nsEutranBands;
  nsTArray<int32_t> nsNgranBands;
  ::std::vector<::aidl::android::hardware::radio::network::GeranBands>
    geranBands;
  ::std::vector<::aidl::android::hardware::radio::network::UtranBands>
    utranBands;
  ::std::vector<::aidl::android::hardware::radio::network::EutranBands>
    eutranBands;
  ::std::vector<::aidl::android::hardware::radio::network::NgranBands>
    ngranBands;

  switch ((AccessNetwork)nsRadioAccessNetwork) {
    case AccessNetwork::GERAN:
      specifier->GetGeranBands(nsGeranBands);
      for (uint32_t i = 0; i < nsGeranBands.Length(); i++) {
        geranBands[i] = (::aidl::android::hardware::radio::network::GeranBands)
          nsGeranBands[i];
      }
      rasInAidlFormat.bands.set<aidl::android::hardware::radio::network::
                                  RadioAccessSpecifierBands::Tag::geranBands>(
        geranBands);
      break;
    case AccessNetwork::UTRAN:
      specifier->GetUtranBands(nsUtranBands);
      for (uint32_t i = 0; i < nsUtranBands.Length(); i++) {
        utranBands[i] = (::aidl::android::hardware::radio::network::UtranBands)
          nsUtranBands[i];
      }
      rasInAidlFormat.bands.set<aidl::android::hardware::radio::network::
                                  RadioAccessSpecifierBands::Tag::utranBands>(
        utranBands);
      break;
    case AccessNetwork::EUTRAN:
      specifier->GetEutranBands(nsEutranBands);
      for (uint32_t i = 0; i < nsEutranBands.Length(); i++) {
        eutranBands[i] =
          (::aidl::android::hardware::radio::network::EutranBands)
            nsEutranBands[i];
      }
      rasInAidlFormat.bands.set<aidl::android::hardware::radio::network::
                                  RadioAccessSpecifierBands::Tag::eutranBands>(
        eutranBands);
      break;
    case AccessNetwork::NGRAN:
      specifier->GetNgranBands(nsNgranBands);
      for (uint32_t i = 0; i < nsNgranBands.Length(); i++) {
        ngranBands[i] = (::aidl::android::hardware::radio::network::NgranBands)
          nsNgranBands[i];
      }
      rasInAidlFormat.bands.set<aidl::android::hardware::radio::network::
                                  RadioAccessSpecifierBands::Tag::ngranBands>(
        ngranBands);
      break;
    default:
      break;
  }

  nsTArray<int32_t> nsChannels;
  ::std::vector<int32_t> channels;
  specifier->GetChannels(nsChannels);
  for (uint32_t i = 0; i < nsChannels.Length(); i++) {
    channels.push_back(nsChannels[i]);
  }
  rasInAidlFormat.channels = channels;
  return rasInAidlFormat;
}

void
nsRadioProxyServiceManager::defaultResponse(const RadioResponseInfo& rspInfo,
                                            const nsString& rilmessageType)
{
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    rilmessageType, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  mRIL->sendRilResponseResult(result);
}

void
nsRadioProxyServiceManager::defaultResponse(const RadioIndicationType& type,
                                            const nsString& rilmessageType)
{
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  mRIL->sendRilIndicationResult(result);
}

void
nsRadioProxyServiceManager::loadRadioMessagingProxy()
{
  DEBUG("try to load RadioMessagingProxy");
  if (mRadioMessagingProxy.get() == nullptr) {
    DEBUG("new mRadioMessagingProxy");
    const std::string instance = std::string() + IRadioMessaging::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioMessagingProxy = IRadioMessaging::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioMessagingProxy.get() == nullptr) {
      mRadioMessagingProxy = IRadioMessaging::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }
    mRadioInd_messaging =
      ndk::SharedRefBase::make<RadioMessagingIndication>(*this);
    mRadioRsp_messaging =
      ndk::SharedRefBase::make<RadioMessagingResponse>(*this);
    mRadioMessagingProxy->setResponseFunctions(mRadioRsp_messaging,
                                               mRadioInd_messaging);
  } else {
    DEBUG("RadioMessagingProxy is ready.");
  }
}

void
nsRadioProxyServiceManager::loadRadioModemProxy()
{
  DEBUG("try to load RadioModemProxy");
  if (mRadioModemProxy.get() == nullptr) {
    DEBUG("new mRadioModemProxy");
    const std::string instance = std::string() + IRadioModem::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioModemProxy = IRadioModem::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioModemProxy.get() == nullptr) {
      mRadioModemProxy = IRadioModem::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }
    mRadioInd_modem = ndk::SharedRefBase::make<RadioModemIndication>(*this);
    mRadioRsp_modem = ndk::SharedRefBase::make<RadioModemResponse>(*this);
    mRadioModemProxy->setResponseFunctions(mRadioRsp_modem, mRadioInd_modem);
  } else {
    DEBUG("RadioModemProxy is ready.");
  }
}

void
nsRadioProxyServiceManager::loadRadioNetworkProxy()
{
  DEBUG("try to load mRadioNetworkProxy");
  if (mRadioNetworkProxy.get() == nullptr) {
    DEBUG("new mRadioNetworkProxy");
    const std::string instance = std::string() + IRadioNetwork::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioNetworkProxy = IRadioNetwork::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioNetworkProxy.get() == nullptr) {
      mRadioNetworkProxy = IRadioNetwork::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }
    mRadioInd_network = ndk::SharedRefBase::make<RadioNetworkIndication>(*this);
    mRadioRsp_network = ndk::SharedRefBase::make<RadioNetworkResponse>(*this);
    mRadioNetworkProxy->setResponseFunctions(mRadioRsp_network,
                                             mRadioInd_network);
  } else {
    DEBUG("mRadioNetworkProxy is ready.");
  }
}

void
nsRadioProxyServiceManager::loadRadioVoiceProxy()
{
  DEBUG("try to load mRadioVoiceProxy");
  if (mRadioVoiceProxy == nullptr) {
    DEBUG("new mRadioVoiceProxy");
    const std::string instance = std::string() + IRadioVoice::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioVoiceProxy = IRadioVoice::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioVoiceProxy == nullptr) {
      mRadioVoiceProxy = IRadioVoice::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }
    if (mRadioVoiceDeathRecipient == nullptr) {
      mRadioVoiceDeathRecipient =
        AIBinder_DeathRecipient_new(&RadioVoiceProxyDeathNotifier);
    }
    binder_status_t binder_status =
      AIBinder_linkToDeath(mRadioDataProxy->asBinder().get(),
                           mRadioVoiceDeathRecipient,
                           reinterpret_cast<void*>(this));
    if (binder_status != STATUS_OK) {
      DEBUG("Failed to link to death, status %d",
            static_cast<int>(binder_status));
      mRadioVoiceProxy = nullptr;
      return;
    }
    mRadioInd_voice = ndk::SharedRefBase::make<RadioVoiceIndication>(*this);
    mRadioRsp_voice = ndk::SharedRefBase::make<RadioVoiceResponse>(*this);
    mRadioVoiceProxy->setResponseFunctions(mRadioRsp_voice, mRadioInd_voice);
  } else {
    DEBUG("mRadioVoiceProxy is ready.");
  }
}

void
nsRadioProxyServiceManager::loadRadioConfigProxy()
{
  if (mRadioConfigProxy == nullptr) {
    DEBUG("new mRadioConfigProxy");
    const std::string instance =
      std::string() + IRadioConfig::descriptor + "/default";
    mRadioConfigProxy = IRadioConfig::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioConfigProxy == nullptr) {
      mRadioConfigProxy = IRadioConfig::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }

    if (mRadioConfigDeathRecipient == nullptr) {
      mRadioConfigDeathRecipient =
        AIBinder_DeathRecipient_new(&RadioConfigProxyDeathNotifier);
    }
    binder_status_t binder_status =
      AIBinder_linkToDeath(mRadioConfigProxy->asBinder().get(),
                           mRadioConfigDeathRecipient,
                           reinterpret_cast<void*>(this));
    if (binder_status != STATUS_OK) {
      DEBUG("Failed to link to death, status %d",
            static_cast<int>(binder_status));
      mRadioConfigProxy = nullptr;
      return;
    }
    mRadioInd_config = ndk::SharedRefBase::make<RadioConfigIndication>(*this);
    mRadioRsp_config = ndk::SharedRefBase::make<RadioConfigResponse>(*this);
    mRadioConfigProxy->setResponseFunctions(mRadioRsp_config, mRadioInd_config);
  } else {
    DEBUG("mRadioConfigProxy is ready.");
  }
}

void
nsRadioProxyServiceManager::loadRadioSimProxy()
{
  DEBUG("try to load loadRadioSimProxy");
  if (mRadioSimProxy == nullptr) {
    DEBUG("new mRadioSimProxy");
    const std::string instance = std::string() + IRadioSim::descriptor +
                                 "/slot" + std::to_string(mClientId);
    mRadioSimProxy = IRadioSim::fromBinder(
      ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
    if (mRadioSimProxy == nullptr) {
      mRadioSimProxy = IRadioSim::fromBinder(
        ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    }

    if (mRadioSimDeathRecipient == nullptr) {
      mRadioSimDeathRecipient =
        AIBinder_DeathRecipient_new(&RadioSimProxyDeathNotifier);
    }
    binder_status_t binder_status =
      AIBinder_linkToDeath(mRadioSimProxy->asBinder().get(),
                           mRadioSimDeathRecipient,
                           reinterpret_cast<void*>(this));
    if (binder_status != STATUS_OK) {
      DEBUG("Failed to link to death, status %d",
            static_cast<int>(binder_status));
      mRadioSimProxy = nullptr;
      return;
    }
    mRadioInd_sim = ndk::SharedRefBase::make<SimIndication>(*this);
    mRadioRsp_sim = ndk::SharedRefBase::make<RadioSimResponse>(*this);
    mRadioSimProxy->setResponseFunctions(mRadioRsp_sim, mRadioInd_sim);
  } else {
    DEBUG("mRadioSimProxy is ready.");
  }
}

RadioSimResponse::RadioSimResponse(nsRadioProxyServiceManager& serviceManager)
  : mManager(serviceManager)
{}

::ndk::ScopedAStatus
RadioSimResponse::areUiccApplicationsEnabledResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& rspInfo,
  bool isEnabled)
{
  DEBUG("RadioSimResponse::areUiccApplicationsEnabledResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"areUiccApplicationsEnabled"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateUiccAppEnabledReponse(isEnabled);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::changeIccPin2ForAppResponse(const RadioResponseInfo& rspInfo,
                                              int32_t remainingRetries)
{
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"changeICCPIN2"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("changeIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::changeIccPinForAppResponse(const RadioResponseInfo& rspInfo,
                                             int32_t remainingRetries)
{
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"changeICCPIN"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("changeIccPinForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }

  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

void
RadioSimResponse::defaultResponse(const RadioResponseInfo& rspInfo,
                                  const nsString& rilmessageType)
{
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(rilmessageType,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  mManager.mRIL->sendRilResponseResult(result);
}
::ndk::ScopedAStatus
RadioSimResponse::enableUiccApplicationsResponse(
  const RadioResponseInfo& rspInfo)
{
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  defaultResponse(rspInfo, u"enableUiccApplicationsResponse"_ns);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getAllowedCarriersResponse(
  const RadioResponseInfo& rspInfo,
  const sim::CarrierRestrictions& carriers,
  sim::SimLockMultiSimPolicy multiSimPolicy)
{
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getAllowedCarriers"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    nsTArray<RefPtr<nsCarrier>> allowedCarriers;
    for (uint32_t i = 0; i < carriers.allowedCarriers.size(); i++) {
      nsString mcc =
        NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].mcc.c_str());
      nsString mnc =
        NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].mnc.c_str());
      nsString matchData =
        NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].matchData.c_str());
      RefPtr<nsCarrier> allowedCr = new nsCarrier(
        mcc, mcc, (uint8_t)carriers.allowedCarriers[i].matchType, matchData);
      allowedCarriers.AppendElement(allowedCr);
    }
    nsTArray<RefPtr<nsCarrier>> excludedCarriers;
    for (uint32_t i = 0; i < carriers.excludedCarriers.size(); i++) {
      nsString mcc =
        NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].mcc.c_str());
      nsString mnc =
        NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].mnc.c_str());
      nsString matchData =
        NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].matchData.c_str());
      RefPtr<nsCarrier> excluedCr = new nsCarrier(
        mcc, mnc, (uint8_t)carriers.excludedCarriers[i].matchType, matchData);
      excludedCarriers.AppendElement(excluedCr);
    }
    RefPtr<nsCarrierRestrictionsWithPriority> crp =
      new nsCarrierRestrictionsWithPriority(
        allowedCarriers, excludedCarriers, carriers.allowedCarriersPrioritized);
    RefPtr<nsAllowedCarriers> allowedCar =
      new nsAllowedCarriers(crp, (int32_t)multiSimPolicy);
    result->updateAllowedCarriers(allowedCar);
  } else {
    DEBUG("getAllowedCarriersResponse error.");
  }
  mManager.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getCdmaSubscriptionResponse(const RadioResponseInfo& rspInfo,
                                              const std::string& in_mdn,
                                              const std::string& in_hSid,
                                              const std::string& in_hNid,
                                              const std::string& in_min,
                                              const std::string& in_prl)
{
  DEBUG("getCdmaSubscriptionResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getCdmaSubscriptionSourceResponse(
  const RadioResponseInfo& rspInfo,
  sim::CdmaSubscriptionSource in_source)
{
  DEBUG("getCdmaSubscriptionSourceResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getFacilityLockForAppResponse(
  const RadioResponseInfo& rspInfo,
  int32_t response)
{
  DEBUG("getFacilityLockForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryICCFacilityLock"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateServiceClass(response);
  } else {
    DEBUG("setFacilityLockForAppResponse error.");
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getIccCardStatusResponse(const RadioResponseInfo& rspInfo,
                                           const sim::CardStatus& aCardStatus)
{
  DEBUG("getIccCardStatusResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getICCStatus"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("getICCStatus success.");
    uint32_t numApplications = aCardStatus.applications.size();

    // limit to maximum allowed applications
    if (numApplications > nsIRilResponseResult::CARD_MAX_APPS) {
      numApplications = nsIRilResponseResult::CARD_MAX_APPS;
    }

    nsTArray<RefPtr<nsAppStatus>> applications(numApplications);

    for (uint32_t i = 0; i < numApplications; i++) {
      RefPtr<nsAppStatus> application = new nsAppStatus(
        aCardStatus.applications[i].appType,
        aCardStatus.applications[i].appState,
        (int32_t)aCardStatus.applications[i].persoSubstate,
        NS_ConvertUTF8toUTF16(aCardStatus.applications[i].aidPtr.c_str()),
        NS_ConvertUTF8toUTF16(aCardStatus.applications[i].appLabelPtr.c_str()),
        aCardStatus.applications[i].pin1Replaced,
        (int32_t)aCardStatus.applications[i].pin1,
        (int32_t)aCardStatus.applications[i].pin2);

      applications.AppendElement(application);
    }

    RefPtr<nsCardStatus> cardStatus =
      new nsCardStatus(aCardStatus.cardState,
                       (int32_t)aCardStatus.universalPinState,
                       aCardStatus.gsmUmtsSubscriptionAppIndex,
                       aCardStatus.cdmaSubscriptionAppIndex,
                       aCardStatus.imsSubscriptionAppIndex,
                       applications,
                       aCardStatus.slotMap.physicalSlotId,
                       NS_ConvertUTF8toUTF16(aCardStatus.atr.c_str()),
                       NS_ConvertUTF8toUTF16(aCardStatus.iccid.c_str()),
                       NS_ConvertUTF8toUTF16(aCardStatus.eid.c_str()));
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }
  mManager.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getImsiForAppResponse(const RadioResponseInfo& rspInfo,
                                        const std::string& imsi)
{
  DEBUG("getImsiForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getIMSI"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateIMSI(NS_ConvertUTF8toUTF16(imsi.c_str()));
  } else {
    DEBUG("getIMSIForAppResponse error.");
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getSimPhonebookCapacityResponse(
  const RadioResponseInfo& rspInfo,
  const sim::PhonebookCapacity& capacity)
{
  DEBUG("getSimPhonebookCapacityResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSimPhonebookCapacity"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    nsIPhonebookCapacity* phoneBookCapacity =
      new nsPhonebookCapacity(capacity.maxAdnRecords,
                              capacity.usedAdnRecords,
                              capacity.maxEmailRecords,
                              capacity.usedEmailRecords,
                              capacity.maxAdditionalNumberRecords,
                              capacity.usedAdditionalNumberRecords,
                              capacity.maxNameLen,
                              capacity.maxNumberLen,
                              capacity.maxEmailLen,
                              capacity.maxAdditionalNumberLen);
    result->updatePhonebookCapacity(phoneBookCapacity);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::getSimPhonebookRecordsResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("getSimPhonebookRecordsResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  defaultResponse(rspInfo, u"getSimPhonebookRecords"_ns);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccCloseLogicalChannelResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("iccCloseLogicalChannelResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccIoForAppResponse(const RadioResponseInfo& rspInfo,
                                      const sim::IccIoResult& iccIo)
{
  DEBUG("iccIoForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"iccIO"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsIccIoResult> iccIoResult = new nsIccIoResult(
      iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(iccIo.simResponse.c_str()));
    result->updateIccIoResult(iccIoResult);
  } else {
    DEBUG("iccIOForApp error.");
    RefPtr<nsIccIoResult> iccIoResult =
      new nsIccIoResult(iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(""));
    result->updateIccIoResult(iccIoResult);
  }

  mManager.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccOpenLogicalChannelResponse(
  const RadioResponseInfo& rspInfo,
  int32_t in_channelId,
  const std::vector<uint8_t>& in_selectResponse)
{
  DEBUG("iccOpenLogicalChannelResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccTransmitApduBasicChannelResponse(
  const RadioResponseInfo& rspInfo,
  const sim::IccIoResult& in_result)
{
  DEBUG("iccTransmitApduBasicChannelResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccTransmitApduLogicalChannelResponse(
  const RadioResponseInfo& rspInfo,
  const sim::IccIoResult& in_result)
{
  DEBUG("iccTransmitApduLogicalChannelResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::reportStkServiceIsRunningResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("reportStkServiceIsRunningResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  defaultResponse(rspInfo, u"reportStkServiceIsRunning"_ns);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::requestIccSimAuthenticationResponse(
  const RadioResponseInfo& rspInfo,
  const sim::IccIoResult& iccIo)
{
  DEBUG("requestIccSimAuthenticationResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getIccAuthentication"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsIccIoResult> iccIoResult = new nsIccIoResult(
      iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(iccIo.simResponse.c_str()));
    result->updateIccIoResult(iccIoResult);
  } else {
    DEBUG("getIccAuthentication error.");
    RefPtr<nsIccIoResult> iccIoResult =
      new nsIccIoResult(iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(""));
    result->updateIccIoResult(iccIoResult);
  }

  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::sendEnvelopeResponse(const RadioResponseInfo& rspInfo,
                                       const std::string& in_commandResponse)
{
  DEBUG("sendEnvelopeResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  defaultResponse(rspInfo, u"sendEnvelopeResponse"_ns);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::sendEnvelopeWithStatusResponse(
  const RadioResponseInfo& rspInfo,
  const sim::IccIoResult& in_iccIo)
{
  DEBUG("sendEnvelopeResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::sendTerminalResponseToSimResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("sendTerminalResponseToSimResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  defaultResponse(rspInfo, u"sendStkTerminalResponse"_ns);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setAllowedCarriersResponse(const RadioResponseInfo& rspInfo)
{
  DEBUG("setAllowedCarriersResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setCarrierInfoForImsiEncryptionResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("setCarrierInfoForImsiEncryptionResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setCdmaSubscriptionSourceResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("setCdmaSubscriptionSourceResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setFacilityLockForAppResponse(
  const RadioResponseInfo& rspInfo,
  int32_t retry)
{
  DEBUG("setFacilityLockForAppResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"setICCFacilityLock"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("setFacilityLockForAppResponse error = %d , retries = %d",
          rspInfo.error,
          retry);
    result->updateRemainRetries(retry);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setSimCardPowerResponse(const RadioResponseInfo& rspInfo)
{
  DEBUG("setSimCardPowerResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  defaultResponse(rspInfo, u"setSimCardPowerResponse"_ns);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::setUiccSubscriptionResponse(const RadioResponseInfo& rspInfo)
{
  DEBUG("setUiccSubscriptionResponse");

  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  defaultResponse(rspInfo, u"setUiccSubscriptionResponse"_ns);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::supplyIccPin2ForAppResponse(const RadioResponseInfo& rspInfo,
                                              int32_t remainingRetries)
{
  DEBUG("supplyIccPin2ForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"enterICCPIN2"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mManager.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::supplyIccPinForAppResponse(const RadioResponseInfo& rspInfo,
                                             int32_t remainingRetries)
{
  DEBUG("supplyIccPinForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"enterICCPIN"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPinForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::supplyIccPuk2ForAppResponse(const RadioResponseInfo& rspInfo,
                                              int32_t remainingRetries)
{
  DEBUG("supplyIccPuk2ForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"enterICCPUK2"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPuk2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mManager.mRIL->sendRilResponseResult(result);

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::supplyIccPukForAppResponse(const RadioResponseInfo& rspInfo,
                                             int32_t remainingRetries)
{
  DEBUG("supplyIccPukForAppResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"enterICCPUK"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPukForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::supplySimDepersonalizationResponse(
  const RadioResponseInfo& rspInfo,
  sim::PersoSubstate persoType,
  int32_t remainingRetries)
{
  DEBUG("supplySimDepersonalizationResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"supplySimDepersonalization"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSupplySimDepersonalizationResult> aResult =
      new nsSupplySimDepersonalizationResult((int32_t)persoType,
                                             remainingRetries);
    result->updateSimDepersonalizationResult(aResult);
  } else {
    DEBUG("supplySimDepersonalization error.");
  }

  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::updateSimPhonebookRecordsResponse(
  const RadioResponseInfo& rspInfo,
  int32_t updatedRecordIndex)
{
  DEBUG("updateSimPhonebookRecordsResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"updateSimPhonebookRecords"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateUpdatedRecordIndex(updatedRecordIndex);
  }
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
RadioSimResponse::iccCloseLogicalChannelWithSessionInfoResponse(
  const RadioResponseInfo& rspInfo)
{
  DEBUG("iccCloseLogicalChannelWithSessionInfoResponse");
  mManager.sendRspAck(rspInfo, rspInfo.serial, SERVICE_TYPE::SIM);
  // TODO:
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
SimIndication::carrierInfoForImsiEncryption(RadioIndicationType type)
{
  DEBUG("carrierInfoForImsiEncryption");
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::cdmaSubscriptionSourceChanged(
  RadioIndicationType type,
  sim::CdmaSubscriptionSource in_cdmaSource)
{
  DEBUG("cdmaSubscriptionSourceChanged");
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
SimIndication::simPhonebookChanged(RadioIndicationType type)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::simPhonebookRecordsReceived(
  RadioIndicationType type,
  sim::PbReceivedStatus status,
  const std::vector<sim::PhonebookRecordInfo>& aRecords)
{
  DEBUG("simPhonebookRecordsReceived");
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);

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

  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::simRefresh(RadioIndicationType type,
                          const sim::SimRefreshResult& refreshResult)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  nsString rilmessageType(u"simRefresh"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  RefPtr<nsSimRefreshResult> simRefresh =
    new nsSimRefreshResult(refreshResult.type,
                           refreshResult.efId,
                           NS_ConvertUTF8toUTF16(refreshResult.aid.c_str()));
  result->updateSimRefresh(simRefresh);
  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::simStatusChanged(RadioIndicationType type)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);

  defaultResponse(type, u"simStatusChanged"_ns);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::stkEventNotify(RadioIndicationType type, const std::string& cmd)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);

  nsString rilmessageType(u"stkEventNotify"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateStkEventNotify(NS_ConvertUTF8toUTF16(cmd.c_str()));
  mManager.mRIL->sendRilIndicationResult(result);

  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::stkProactiveCommand(RadioIndicationType type,
                                   const std::string& cmd)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  nsString rilmessageType(u"stkProactiveCommand"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateStkProactiveCommand(NS_ConvertUTF8toUTF16(cmd.c_str()));
  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::stkSessionEnd(RadioIndicationType type)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  defaultResponse(type, u"stkSessionEnd"_ns);
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
SimIndication::subscriptionStatusChanged(RadioIndicationType type,
                                         bool activate)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  nsString rilmessageType(u"subscriptionStatusChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateSubscriptionStatusChanged(activate);
  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus
SimIndication::uiccApplicationsEnablementChanged(RadioIndicationType type,
                                                 bool enabled)
{
  mManager.sendIndAck(type, SERVICE_TYPE::SIM);
  nsString rilmessageType(u"uiccApplicationsEnablementChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  result->updateUiccApplicationsEnabled(enabled);
  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}

void
SimIndication::defaultResponse(const RadioIndicationType type,
                               const nsString& rilmessageType)
{
  DEBUG("SimIndication::defaultResponse");
  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);
  mManager.mRIL->sendRilIndicationResult(result);
}

RadioConfigIndication::RadioConfigIndication(
  nsRadioProxyServiceManager& aManager)
  : mManager(aManager)
{}
::ndk::ScopedAStatus
RadioConfigIndication::simSlotsStatusChanged(
  RadioIndicationType type,
  const std::vector<::aidl::android::hardware::radio::config::SimSlotStatus>&
    aSlotStatus)
{
  nsString rilmessageType(u"simSlotsStatusChanged"_ns);

  RefPtr<nsRilIndicationResult> result =
    new nsRilIndicationResult(rilmessageType);

  nsTArray<RefPtr<nsISimSlotStatus>> simSlotStatus;
  for (uint32_t i = 0; i < aSlotStatus.size(); i++) {
    RefPtr<nsISimSlotStatus> status = new nsSimSlotStatus(aSlotStatus[i]);
    simSlotStatus.AppendElement(status);
  }

  result->updateSimSlotStatus(simSlotStatus);
  mManager.mRIL->sendRilIndicationResult(result);
  return ::ndk::ScopedAStatus::ok();
}

RadioConfigResponse::RadioConfigResponse(nsRadioProxyServiceManager& manager)
  : mManager(manager)
{}

::ndk::ScopedAStatus
RadioConfigResponse::getHalDeviceCapabilitiesResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
  bool modemReducedFeatureSet1)
{
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::getNumOfLiveModemsResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
  int8_t numOfLiveModems)
{
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::getPhoneCapabilityResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info,
  const ::aidl::android::hardware::radio::config::PhoneCapability&
    phoneCapability)
{
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::getSimSlotsStatusResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& rspInfo,
  const std::vector<::aidl::android::hardware::radio::config::SimSlotStatus>&
    aSlotStatus)
{
  DEBUG("getSimSlotsStatusResponse");
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSimSlotsStatusResponse"_ns,
                            rspInfo.serial,
                            mManager.convertRadioErrorToNum(rspInfo.error));
  nsTArray<RefPtr<nsISimSlotStatus>> simSlotStatus;
  for (uint32_t i = 0; i < aSlotStatus.size(); i++) {
    RefPtr<nsISimSlotStatus> status = new nsSimSlotStatus(aSlotStatus[i]);
    simSlotStatus.AppendElement(status);
  }

  result->updateSimSlotStatus(simSlotStatus);
  mManager.mRIL->sendRilResponseResult(result);
  return ndk::ScopedAStatus::ok();
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::setNumOfLiveModemsResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
{
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::setPreferredDataModemResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
{
  return ::ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus
RadioConfigResponse::setSimSlotsMappingResponse(
  const ::aidl::android::hardware::radio::RadioResponseInfo& in_info)
{
  return ::ndk::ScopedAStatus::ok();
}
