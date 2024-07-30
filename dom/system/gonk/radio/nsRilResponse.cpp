/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilResponse.h"
#include "nsRadioTypes.h"
#include "nsRilResult.h"
#include "nsRilWorker.h"
/* Logging related */
#undef LOG_TAG
#define LOG_TAG "nsRilResponse"

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
nsRilResponse::nsRilResponse(nsRilWorker* aRil)
{
  DEBUG("init nsRilResponse");
  mRIL = aRil;
}

nsRilResponse::~nsRilResponse()
{
  DEBUG("~nsRilResponse");
  mRIL = nullptr;
  MOZ_ASSERT(!mRIL);
}

Return<void>
nsRilResponse::getIccCardStatusResponse(const RadioResponseInfo_V1_0& info,
                                        const CardStatus_V1_0& card_status)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getICCStatus"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getICCStatus success.");
    uint32_t numApplications = card_status.applications.size();

    // limit to maximum allowed applications
    if (numApplications > nsIRilResponseResult::CARD_MAX_APPS) {
      numApplications = nsIRilResponseResult::CARD_MAX_APPS;
    }

    nsTArray<RefPtr<nsAppStatus>> applications(numApplications);

    for (uint32_t i = 0; i < numApplications; i++) {
      RefPtr<nsAppStatus> application = new nsAppStatus(
        convertAppType(card_status.applications[i].appType),
        convertAppState(card_status.applications[i].appState),
        convertPersoSubstate(card_status.applications[i].persoSubstate),
        NS_ConvertUTF8toUTF16(card_status.applications[i].aidPtr.c_str()),
        NS_ConvertUTF8toUTF16(card_status.applications[i].appLabelPtr.c_str()),
        card_status.applications[i].pin1Replaced,
        convertPinState(card_status.applications[i].pin1),
        convertPinState(card_status.applications[i].pin2));

      applications.AppendElement(application);
    }

    RefPtr<nsCardStatus> cardStatus =
      new nsCardStatus(convertCardState(card_status.cardState),
                       convertPinState(card_status.universalPinState),
                       card_status.gsmUmtsSubscriptionAppIndex,
                       card_status.cdmaSubscriptionAppIndex,
                       card_status.imsSubscriptionAppIndex,
                       applications,
                       -1,
                       NS_ConvertUTF8toUTF16(nullptr),
                       NS_ConvertUTF8toUTF16(nullptr),
                       NS_ConvertUTF8toUTF16(""));
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::supplyIccPinForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"enterICCPIN"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("supplyIccPinForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::supplyIccPukForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"enterICCPUK"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("supplyIccPukForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::supplyIccPin2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"enterICCPIN2"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("supplyIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::supplyIccPuk2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"enterICCPUK2"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("supplyIccPuk2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::changeIccPinForAppResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"changeICCPIN"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("changeIccPinForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::changeIccPin2ForAppResponse(const RadioResponseInfo_V1_0& info,
                                           int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"changeICCPIN2"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("changeIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::supplyNetworkDepersonalizationResponse(
  const RadioResponseInfo_V1_0& info,
  int32_t /*remainingRetries*/)
{
  rspInfo = info;
  nsString forwardedNumber;

  return Void();
}

Return<void>
nsRilResponse::getCurrentCallsResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<Call_V1_0>& calls)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCurrentCalls"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCalls = calls.size();
    DEBUG("getCurrentCalls numCalls= %d", numCalls);
    nsTArray<RefPtr<nsCall>> aCalls(numCalls);

    for (uint32_t i = 0; i < numCalls; i++) {
      uint32_t numUusInfo = calls[i].uusInfo.size();
      DEBUG("getCurrentCalls numUusInfo= %d", numUusInfo);
      nsTArray<RefPtr<nsUusInfo>> aUusInfos(numUusInfo);

      for (uint32_t j = 0; j < numUusInfo; j++) {
        RefPtr<nsUusInfo> uusinfo = new nsUusInfo(
          convertUusType(calls[i].uusInfo[j].uusType),
          convertUusDcs(calls[i].uusInfo[j].uusDcs),
          NS_ConvertUTF8toUTF16(calls[i].uusInfo[j].uusData.c_str()));

        aUusInfos.AppendElement(uusinfo);
      }

      DEBUG("getCurrentCalls index= %d  state=%d",
            calls[i].index,
            convertCallState(calls[i].state));
      nsString forwardedNumber;
      forwardedNumber.AssignASCII("");
      RefPtr<nsCall> call =
        new nsCall(convertCallState(calls[i].state),
                   calls[i].index,
                   calls[i].toa,
                   calls[i].isMpty,
                   calls[i].isMT,
                   int32_t(calls[i].als),
                   calls[i].isVoice,
                   calls[i].isVoicePrivacy,
                   NS_ConvertUTF8toUTF16(calls[i].number.c_str()),
                   convertCallPresentation(calls[i].numberPresentation),
                   NS_ConvertUTF8toUTF16(calls[i].name.c_str()),
                   convertCallPresentation(calls[i].namePresentation),
                   aUusInfos,
                   0,
                   forwardedNumber);
      aCalls.AppendElement(call);
    }
    result->updateCurrentCalls(aCalls);
  } else {
    DEBUG("getCurrentCalls error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::dialResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"dial"_ns);
  return Void();
}

Return<void>
nsRilResponse::getIMSIForAppResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& imsi)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getIMSI"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateIMSI(NS_ConvertUTF8toUTF16(imsi.c_str()));
  } else {
    DEBUG("getIMSIForAppResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::hangupConnectionResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::hangupWaitingOrBackgroundResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpBackground"_ns);
  return Void();
}

Return<void>
nsRilResponse::hangupForegroundResumeBackgroundResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpForeground"_ns);
  return Void();
}

Return<void>
nsRilResponse::switchWaitingOrHoldingAndActiveResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"switchActiveCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::conferenceResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"conferenceCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::rejectCallResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"udub"_ns);
  return Void();
}

Return<void>
nsRilResponse::getLastCallFailCauseResponse(
  const RadioResponseInfo_V1_0& info,
  const LastCallFailCauseInfo_V1_0& failCauseInfo)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getFailCause"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateFailCause(
      covertLastCallFailCause(failCauseInfo.causeCode),
      NS_ConvertUTF8toUTF16(failCauseInfo.vendorCause.c_str()));
  } else {
    DEBUG("getLastCallFailCauseResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::getSignalStrengthResponse(const RadioResponseInfo_V1_0& info,
                                         const SignalStrength& sig_strength)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSignalStrength"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSignalStrength> signalStrength =
      result->convertSignalStrength(sig_strength);
    result->updateSignalStrength(signalStrength);
  } else {
    DEBUG("getSignalStrength error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getVoiceRegistrationStateResponse(
  const RadioResponseInfo_V1_0& info,
  const VoiceRegStateResult_V1_0& voiceRegResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getVoiceRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity(&voiceRegResponse.cellIdentity);
    nsString rplmn;
    rplmn.AssignASCII("");
    RefPtr<nsVoiceRegState> voiceRegState =
      new nsVoiceRegState(convertRegState(voiceRegResponse.regState),
                          voiceRegResponse.rat,
                          voiceRegResponse.cssSupported,
                          voiceRegResponse.roamingIndicator,
                          voiceRegResponse.systemIsInPrl,
                          voiceRegResponse.defaultRoamingIndicator,
                          voiceRegResponse.reasonForDenial,
                          cellIdentity,
                          rplmn);
    result->updateVoiceRegStatus(voiceRegState);
  } else {
    DEBUG("getVoiceRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataRegistrationStateResponse(
  const RadioResponseInfo_V1_0& info,
  const DataRegStateResult_V1_0& dataRegResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity(&dataRegResponse.cellIdentity);
    nsString rplmn;
    rplmn.AssignASCII("");
    RefPtr<nsDataRegState> dataRegState =
      new nsDataRegState(convertRegState(dataRegResponse.regState),
                         dataRegResponse.rat,
                         dataRegResponse.reasonDataDenied,
                         dataRegResponse.maxDataCalls,
                         cellIdentity,
                         nullptr,
                         nullptr,
                         rplmn,
                         nullptr);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getOperatorResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& longName,
  const ::android::hardware::hidl_string& shortName,
  const ::android::hardware::hidl_string& numeric)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getOperator"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsOperatorInfo> operatorInfo =
      new nsOperatorInfo(NS_ConvertUTF8toUTF16(longName.c_str()),
                         NS_ConvertUTF8toUTF16(shortName.c_str()),
                         NS_ConvertUTF8toUTF16(numeric.c_str()),
                         0);
    result->updateOperator(operatorInfo);
  } else {
    DEBUG("getOperator error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setRadioPowerResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setRadioEnabled"_ns);
  return Void();
}

Return<void>
nsRilResponse::sendDtmfResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendTone"_ns);
  return Void();
}

Return<void>
nsRilResponse::sendSmsResponse(const RadioResponseInfo_V1_0& info,
                               const SendSmsResult_V1_0& sms)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"sendSMS"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSMS error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendSMSExpectMoreResponse(const RadioResponseInfo_V1_0& info,
                                         const SendSmsResult_V1_0& sms)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setupDataCallResponse(const RadioResponseInfo_V1_0& info,
                                     const SetupDataCallResult_V1_0& dcResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"setupDataCall"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
      result->convertDcResponse(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::iccIOForAppResponse(const RadioResponseInfo_V1_0& info,
                                   const IccIoResult_V1_0& iccIo)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"iccIO"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsIccIoResult> iccIoResult = new nsIccIoResult(
      iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(iccIo.simResponse.c_str()));
    result->updateIccIoResult(iccIoResult);
  } else {
    DEBUG("iccIOForApp error.");
    RefPtr<nsIccIoResult> iccIoResult =
      new nsIccIoResult(iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(""));
    result->updateIccIoResult(iccIoResult);
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendUssdResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendUSSD"_ns);
  return Void();
}

Return<void>
nsRilResponse::cancelPendingUssdResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"cancelUSSD"_ns);
  return Void();
}

Return<void>
nsRilResponse::getClirResponse(const RadioResponseInfo_V1_0& info,
                               int32_t n,
                               int32_t m)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getCLIR"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateClir(n, m);
  } else {
    DEBUG("getClirResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setClirResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCLIR"_ns);
  return Void();
}

Return<void>
nsRilResponse::getCallForwardStatusResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<CallForwardInfo_V1_0>& callForwardInfos)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryCallForwardStatus"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCallForwardInfo = callForwardInfos.size();
    nsTArray<RefPtr<nsCallForwardInfo>> aCallForwardInfoLists(
      numCallForwardInfo);

    for (uint32_t i = 0; i < numCallForwardInfo; i++) {
      RefPtr<nsCallForwardInfo> callForwardInfo = new nsCallForwardInfo(
        convertCallForwardState(callForwardInfos[i].status),
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
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setCallForwardResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCallForward"_ns);
  return Void();
}

Return<void>
nsRilResponse::getCallWaitingResponse(const RadioResponseInfo_V1_0& info,
                                      bool enable,
                                      int32_t serviceClass)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryCallWaiting"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateCallWaiting(enable, serviceClass);
  } else {
    DEBUG("getCallWaitingResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setCallWaitingResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCallWaiting"_ns);
  return Void();
}

Return<void>
nsRilResponse::acknowledgeLastIncomingGsmSmsResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"ackSMS"_ns);
  return Void();
}

Return<void>
nsRilResponse::acceptCallResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"answerCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::deactivateDataCallResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"deactivateDataCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::getFacilityLockForAppResponse(const RadioResponseInfo_V1_0& info,
                                             int32_t response)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryICCFacilityLock"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateServiceClass(response);
  } else {
    DEBUG("setFacilityLockForAppResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setFacilityLockForAppResponse(const RadioResponseInfo_V1_0& info,
                                             int32_t retry)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"setICCFacilityLock"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError_V1_0::NONE) {
    DEBUG("setFacilityLockForAppResponse error = %d , retries = %d",
          rspInfo.error,
          retry);
    result->updateRemainRetries(retry);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setBarringPasswordResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"changeCallBarringPassword"_ns);
  return Void();
}

Return<void>
nsRilResponse::getNetworkSelectionModeResponse(
  const RadioResponseInfo_V1_0& info,
  bool manual)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getNetworkSelectionMode"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  result->updateNetworkSelectionMode(manual);
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setNetworkSelectionModeAutomaticResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"selectNetworkAuto"_ns);
  return Void();
}

Return<void>
nsRilResponse::setNetworkSelectionModeManualResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"selectNetwork"_ns);
  return Void();
}

Return<void>
nsRilResponse::getAvailableNetworksResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<OperatorInfo_V1_0>& networkInfos)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getAvailableNetworks"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numNetworks = networkInfos.size();
    DEBUG("getAvailableNetworks numNetworks= %d", numNetworks);
    nsTArray<RefPtr<nsOperatorInfo>> aNetworks(numNetworks);

    for (uint32_t i = 0; i < numNetworks; i++) {
      RefPtr<nsOperatorInfo> network = new nsOperatorInfo(
        NS_ConvertUTF8toUTF16(networkInfos[i].alphaLong.c_str()),
        NS_ConvertUTF8toUTF16(networkInfos[i].alphaShort.c_str()),
        NS_ConvertUTF8toUTF16(networkInfos[i].operatorNumeric.c_str()),
        convertOperatorState(networkInfos[i].status));
      aNetworks.AppendElement(network);
    }
    result->updateAvailableNetworks(aNetworks);
  } else {
    DEBUG("getAvailableNetworksResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::startDtmfResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"startTone"_ns);
  return Void();
}

Return<void>
nsRilResponse::stopDtmfResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stopTone"_ns);
  return Void();
}

Return<void>
nsRilResponse::getBasebandVersionResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& version)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getBasebandVersion"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateBasebandVersion(NS_ConvertUTF8toUTF16(version.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::separateConnectionResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"separateCall"_ns);
  return Void();
}

Return<void>
nsRilResponse::setMuteResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setMute"_ns);
  return Void();
}

Return<void>
nsRilResponse::getMuteResponse(const RadioResponseInfo_V1_0& info, bool enable)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getMute"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateMute(enable);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getClipResponse(const RadioResponseInfo_V1_0& info,
                               ClipStatus_V1_0 status)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"queryCLIP"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateClip(convertClipState(status));
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataCallListResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<SetupDataCallResult_V1_0>& dcResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataCallList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numDataCall = dcResponse.size();
    nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

    for (uint32_t i = 0; i < numDataCall; i++) {
      RefPtr<nsSetupDataCallResult> datcall =
        result->convertDcResponse(dcResponse[i]);
      aDcLists.AppendElement(datcall);
    }
    result->updateDcList(aDcLists);
  } else {
    DEBUG("getDataCallListResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendOemRilRequestRawResponse(
  const RadioResponseInfo_V1_0& /*info*/,
  const ::android::hardware::hidl_vec<uint8_t>& /*data*/)
{
  return Void();
}

Return<void>
nsRilResponse::sendOemRilRequestStringsResponse(
  const RadioResponseInfo_V1_0& /*info*/,
  const ::android::hardware::hidl_vec<
    ::android::hardware::hidl_string>& /*data*/)
{
  return Void();
}

Return<void>
nsRilResponse::setSuppServiceNotificationsResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setSuppServiceNotifications"_ns);
  return Void();
}

Return<void>
nsRilResponse::writeSmsToSimResponse(const RadioResponseInfo_V1_0& info,
                                     int32_t index)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // writeSmsToSimIndex = index;

  return Void();
}

Return<void>
nsRilResponse::deleteSmsOnSimResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setBandModeResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getAvailableBandModesResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<RadioBandMode_V1_0>& /*bandModes*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::sendEnvelopeResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& /*commandResponse*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendEnvelopeResponse"_ns);
  return Void();
}

Return<void>
nsRilResponse::sendTerminalResponseToSimResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendStkTerminalResponse"_ns);
  return Void();
}

Return<void>
nsRilResponse::handleStkCallSetupRequestFromSimResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stkHandleCallSetup"_ns);
  return Void();
}

Return<void>
nsRilResponse::explicitCallTransferResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"explicitCallTransfer"_ns);
  return Void();
}

Return<void>
nsRilResponse::setPreferredNetworkTypeResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setPreferredNetworkType"_ns);
  return Void();
}

Return<void>
nsRilResponse::getPreferredNetworkTypeResponse(
  const RadioResponseInfo_V1_0& info,
  PreferredNetworkType_V1_0 nw_type)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getPreferredNetworkType"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updatePreferredNetworkType(convertPreferredNetworkType(nw_type));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::getNeighboringCidsResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<NeighboringCell_V1_0>& cells)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getNeighboringCellIds"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCells = cells.size();
    nsTArray<RefPtr<nsNeighboringCell>> aNeighboringCells(numCells);

    for (uint32_t i = 0; i < numCells; i++) {
      RefPtr<nsNeighboringCell> neighboringCell = new nsNeighboringCell(
        NS_ConvertUTF8toUTF16(cells[i].cid.c_str()), cells[i].rssi);
      aNeighboringCells.AppendElement(neighboringCell);
    }
    result->updateNeighboringCells(aNeighboringCells);
  } else {
    DEBUG("getNeighboringCidsResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setLocationUpdatesResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setCdmaSubscriptionSourceResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setCdmaRoamingPreferenceResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getCdmaRoamingPreferenceResponse(
  const RadioResponseInfo_V1_0& info,
  CdmaRoamingType_V1_0 /*type*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setTTYModeResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setTtyMode"_ns);
  return Void();
}

Return<void>
nsRilResponse::getTTYModeResponse(const RadioResponseInfo_V1_0& info,
                                  TtyMode_V1_0 mode)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"queryTtyMode"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateTtyMode(convertTtyMode(mode));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setPreferredVoicePrivacyResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setVoicePrivacyMode"_ns);
  return Void();
}

Return<void>
nsRilResponse::getPreferredVoicePrivacyResponse(
  const RadioResponseInfo_V1_0& info,
  bool enable)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"queryVoicePrivacyMode"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateVoicePrivacy(enable);
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::sendCDMAFeatureCodeResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::sendBurstDtmfResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::sendCdmaSmsResponse(const RadioResponseInfo_V1_0& info,
                                   const SendSmsResult_V1_0& sms)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  return Void();
}

Return<void>
nsRilResponse::acknowledgeLastIncomingCdmaSmsResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getGsmBroadcastConfigResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<
    GsmBroadcastSmsConfigInfo_V1_0>& /*configs*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setGsmBroadcastConfigResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setGsmBroadcastConfig"_ns);
  return Void();
}

Return<void>
nsRilResponse::setGsmBroadcastActivationResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setGsmBroadcastActivation"_ns);
  return Void();
}

Return<void>
nsRilResponse::getCdmaBroadcastConfigResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<
    CdmaBroadcastSmsConfigInfo_V1_0>& /*configs*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setCdmaBroadcastConfigResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setCdmaBroadcastActivationResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getCDMASubscriptionResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& /*mdn*/,
  const ::android::hardware::hidl_string& /*hSid*/,
  const ::android::hardware::hidl_string& /*hNid*/,
  const ::android::hardware::hidl_string& /*min*/,
  const ::android::hardware::hidl_string& /*prl*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::writeSmsToRuimResponse(const RadioResponseInfo_V1_0& info,
                                      uint32_t index)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  return Void();
}

Return<void>
nsRilResponse::deleteSmsOnRuimResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getDeviceIdentityResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& imei,
  const ::android::hardware::hidl_string& imeisv,
  const ::android::hardware::hidl_string& esn,
  const ::android::hardware::hidl_string& meid)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDeviceIdentity"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateDeviceIdentity(NS_ConvertUTF8toUTF16(imei.c_str()),
                                 NS_ConvertUTF8toUTF16(imeisv.c_str()),
                                 NS_ConvertUTF8toUTF16(esn.c_str()),
                                 NS_ConvertUTF8toUTF16(meid.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::exitEmergencyCallbackModeResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendExitEmergencyCbModeRequest"_ns);
  return Void();
}

Return<void>
nsRilResponse::getSmscAddressResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& smsc)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSmscAddress"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateSmscAddress(NS_ConvertUTF8toUTF16(smsc.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::setSmscAddressResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::reportSmsMemoryStatusResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::reportStkServiceIsRunningResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"reportStkServiceIsRunning"_ns);
  return Void();
}

Return<void>
nsRilResponse::getCdmaSubscriptionSourceResponse(
  const RadioResponseInfo_V1_0& info,
  CdmaSubscriptionSource_V1_0 /*source*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::requestIsimAuthenticationResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& /*response*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::acknowledgeIncomingGsmSmsWithPduResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::sendEnvelopeWithStatusResponse(
  const RadioResponseInfo_V1_0& info,
  const IccIoResult_V1_0& /*iccIo*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getVoiceRadioTechnologyResponse(
  const RadioResponseInfo_V1_0& info,
  RadioTechnology_V1_0 rat)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getVoiceRadioTechnology"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateVoiceRadioTechnology(
      nsRilResult::convertRadioTechnology(rat));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void>
nsRilResponse::getCellInfoListResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<CellInfo_V1_0>& cellInfo)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCellInfoList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell = result->convertRilCellInfo(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setCellInfoListRateResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCellInfoListRate"_ns);
  return Void();
}

Return<void>
nsRilResponse::setInitialAttachApnResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setInitialAttachApn"_ns);
  return Void();
}

Return<void>
nsRilResponse::getImsRegistrationStateResponse(
  const RadioResponseInfo_V1_0& info,
  bool /*isRegistered*/,
  RadioTechnologyFamily_V1_0 /*ratFamily*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::sendImsSmsResponse(const RadioResponseInfo_V1_0& info,
                                  const SendSmsResult_V1_0& sms)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::iccTransmitApduBasicChannelResponse(
  const RadioResponseInfo_V1_0& info,
  const IccIoResult_V1_0& result)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::iccOpenLogicalChannelResponse(
  const RadioResponseInfo_V1_0& info,
  int32_t channelId,
  const ::android::hardware::hidl_vec<int8_t>& /*selectResponse*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::iccCloseLogicalChannelResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::iccTransmitApduLogicalChannelResponse(
  const RadioResponseInfo_V1_0& info,
  const IccIoResult_V1_0& result)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::nvReadItemResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_string& /*result*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::nvWriteItemResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::nvWriteCdmaPrlResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::nvResetConfigResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setUiccSubscriptionResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setUiccSubscription"_ns);
  return Void();
}

Return<void>
nsRilResponse::setDataAllowedResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setDataRegistration"_ns);
  return Void();
}

Return<void>
nsRilResponse::getHardwareConfigResponse(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<HardwareConfig_V1_0>& /*config*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::requestIccSimAuthenticationResponse(
  const RadioResponseInfo_V1_0& info,
  const IccIoResult_V1_0& iccIo)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getIccAuthentication"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsIccIoResult> iccIoResult = new nsIccIoResult(
      iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(iccIo.simResponse.c_str()));
    result->updateIccIoResult(iccIoResult);
  } else {
    DEBUG("getIccAuthentication error.");
    RefPtr<nsIccIoResult> iccIoResult =
      new nsIccIoResult(iccIo.sw1, iccIo.sw2, NS_ConvertUTF8toUTF16(""));
    result->updateIccIoResult(iccIoResult);
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setDataProfileResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setDataProfile"_ns);
  return Void();
}

Return<void>
nsRilResponse::requestShutdownResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  return Void();
}

Return<void>
nsRilResponse::getRadioCapabilityResponse(const RadioResponseInfo_V1_0& info,
                                          const RadioCapability_V1_0& rc)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getRadioCapability"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setRadioCapabilityResponse(const RadioResponseInfo_V1_0& info,
                                          const RadioCapability_V1_0& /*rc*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::startLceServiceResponse(const RadioResponseInfo_V1_0& info,
                                       const LceStatusInfo_V1_0& /*statusInfo*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::stopLceServiceResponse(const RadioResponseInfo_V1_0& info,
                                      const LceStatusInfo_V1_0& /*statusInfo*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::pullLceDataResponse(const RadioResponseInfo_V1_0& info,
                                   const LceDataInfo_V1_0& /*lceInfo*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::getModemActivityInfoResponse(
  const RadioResponseInfo_V1_0& info,
  const ActivityStatsInfo_V1_0& /*activityInfo*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setAllowedCarriersResponse(const RadioResponseInfo_V1_0& info,
                                          int32_t /*numAllowed*/)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  return Void();
}

Return<void>
nsRilResponse::getAllowedCarriersResponse(
  const RadioResponseInfo_V1_0& info,
  bool allAllowed,
  const CarrierRestrictions_V1_0& carriers)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getAllowedCarriers"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
    nsTArray<RefPtr<nsCarrier>> excludedCarriers;
    nsTArray<RefPtr<nsCarrier>> allowedCarriers;
    if (!allAllowed) {
      for (uint32_t i = 0; i < carriers.allowedCarriers.size(); i++) {
        nsString mnc =
          NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].mcc.c_str());
        nsString mcc =
          NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].mnc.c_str());
        nsString matchData =
          NS_ConvertUTF8toUTF16(carriers.allowedCarriers[i].matchData.c_str());
        RefPtr<nsCarrier> allowedCr =
          new nsCarrier(mnc,
                        mcc,
                        (uint8_t)(carriers.allowedCarriers[i].matchType),
                        matchData);
        allowedCarriers.AppendElement(allowedCr);
      }

      for (uint32_t i = 0; i < carriers.excludedCarriers.size(); i++) {
        nsString mnc =
          NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].mcc.c_str());
        nsString mcc =
          NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].mnc.c_str());
        nsString matchData =
          NS_ConvertUTF8toUTF16(carriers.excludedCarriers[i].matchData.c_str());
        RefPtr<nsCarrier> excluedCr =
          new nsCarrier(mcc,
                        mnc,
                        (uint8_t)(carriers.excludedCarriers[i].matchType),
                        matchData);
        excludedCarriers.AppendElement(excluedCr);
      }
    }
    RefPtr<nsCarrierRestrictionsWithPriority> crp =
      new nsCarrierRestrictionsWithPriority(
        allowedCarriers, excludedCarriers, allAllowed);
    RefPtr<nsAllowedCarriers> allowedCrs =
      new nsAllowedCarriers(crp, nsAllowedCarriers::NO_MULTISIM_POLICY);
    result->updateAllowedCarriers(allowedCrs);
  } else {
    DEBUG("getAllowedCarriersResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendDeviceStateResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setIndicationFilterResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return Void();
}

Return<void>
nsRilResponse::setSimCardPowerResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::acknowledgeRequest(int32_t /*serial*/)
{
  return Void();
}

Return<void>
nsRilResponse::setCarrierInfoForImsiEncryptionResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::setSimCardPowerResponse_1_1(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::startNetworkScanResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void>
nsRilResponse::stopNetworkScanResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stopNetworkScan"_ns);
  return Void();
}

Return<void>
nsRilResponse::startKeepaliveResponse(const RadioResponseInfo_V1_0& info,
                                      const KeepaliveStatus_V1_1& status)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"startKeepalive"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
    nsIKeepAliveStatus* aStatus =
      new nsKeepAliveStatus(status.sessionHandle, (int32_t)status.code);
    result->updateKeppAliveStatus(aStatus);

  } else {
    DEBUG("startKeepaliveResponse error: %d", (int32_t)rspInfo.error);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::stopKeepaliveResponse(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  return Void();
}

#if ANDROID_VERSION >= 33
Return<void>
nsRilResponse::getCellInfoListResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const hidl_vec<CellInfo_V1_2>& cellInfo)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCellInfoList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell =
        result->convertRilCellInfo_V1_2(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse_1_2 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getIccCardStatusResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const IRadioCardStatus_V1_2& card_status)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getICCStatus"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getICCStatus success.");
    uint32_t numApplications = card_status.base.applications.size();

    if (numApplications > nsIRilResponseResult::CARD_MAX_APPS) {
      numApplications = nsIRilResponseResult::CARD_MAX_APPS;
    }

    nsTArray<RefPtr<nsAppStatus>> applications(numApplications);

    for (uint32_t i = 0; i < numApplications; i++) {
      RefPtr<nsAppStatus> application = new nsAppStatus(
        convertAppType(card_status.base.applications[i].appType),
        convertAppState(card_status.base.applications[i].appState),
        convertPersoSubstate(card_status.base.applications[i].persoSubstate),
        NS_ConvertUTF8toUTF16(card_status.base.applications[i].aidPtr.c_str()),
        NS_ConvertUTF8toUTF16(
          card_status.base.applications[i].appLabelPtr.c_str()),
        card_status.base.applications[i].pin1Replaced,
        convertPinState(card_status.base.applications[i].pin1),
        convertPinState(card_status.base.applications[i].pin2));

      applications.AppendElement(application);
    }

    RefPtr<nsCardStatus> cardStatus =
      new nsCardStatus(convertCardState(card_status.base.cardState),
                       convertPinState(card_status.base.universalPinState),
                       card_status.base.gsmUmtsSubscriptionAppIndex,
                       card_status.base.cdmaSubscriptionAppIndex,
                       card_status.base.imsSubscriptionAppIndex,
                       applications,
                       card_status.physicalSlotId,
                       NS_ConvertUTF8toUTF16(card_status.atr.c_str()),
                       NS_ConvertUTF8toUTF16(card_status.iccid.c_str()),
                       NS_ConvertUTF8toUTF16(""));
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}
#endif

#if ANDROID_VERSION >= 33
Return<void>
nsRilResponse::setSignalStrengthReportingCriteriaResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setSignalStrengthReportingCriteria"_ns);
  return Void();
}

Return<void>
nsRilResponse::setLinkCapacityReportingCriteriaResponse(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setLinkCapacityReportingCriteria"_ns);
  return Void();
}

Return<void>
nsRilResponse::setLinkCapacityReportingCriteriaResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setLinkCapacityReportingCriteria_1_5"_ns);
  return Void();
}

Return<void>
nsRilResponse::getCurrentCallsResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const ::android::hardware::hidl_vec<IRadioCall_V1_2>& calls)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCurrentCalls"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCalls = calls.size();
    DEBUG("getCurrentCalls_1_2 numCalls= %d", numCalls);
    nsTArray<RefPtr<nsCall>> aCalls(numCalls);

    for (uint32_t i = 0; i < numCalls; i++) {
      uint32_t numUusInfo = calls[i].base.uusInfo.size();
      DEBUG("getCurrentCalls_1_2 numUusInfo= %d", numUusInfo);
      nsTArray<RefPtr<nsUusInfo>> aUusInfos(numUusInfo);

      for (uint32_t j = 0; j < numUusInfo; j++) {
        RefPtr<nsUusInfo> uusinfo = new nsUusInfo(
          convertUusType(calls[i].base.uusInfo[j].uusType),
          convertUusDcs(calls[i].base.uusInfo[j].uusDcs),
          NS_ConvertUTF8toUTF16(calls[i].base.uusInfo[j].uusData.c_str()));
        aUusInfos.AppendElement(uusinfo);
      }

      DEBUG("getCurrentCalls_1_2 index= %d  state=%d",
            calls[i].base.index,
            convertCallState(calls[i].base.state));
      nsString forwardedNumber;
      forwardedNumber.AssignASCII("");
      RefPtr<nsCall> call =
        new nsCall(convertCallState(calls[i].base.state),
                   calls[i].base.index,
                   calls[i].base.toa,
                   calls[i].base.isMpty,
                   calls[i].base.isMT,
                   int32_t(calls[i].base.als),
                   calls[i].base.isVoice,
                   calls[i].base.isVoicePrivacy,
                   NS_ConvertUTF8toUTF16(calls[i].base.number.c_str()),
                   convertCallPresentation(calls[i].base.numberPresentation),
                   NS_ConvertUTF8toUTF16(calls[i].base.name.c_str()),
                   convertCallPresentation(calls[i].base.namePresentation),
                   aUusInfos,
                   nsRilResult::convertAudioQuality(calls[i].audioQuality),
                   forwardedNumber);
      aCalls.AppendElement(call);
    }
    result->updateCurrentCalls(aCalls);
  } else {
    DEBUG("getCurrentCalls error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getSignalStrengthResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const IRadioSignalStrength_V1_2& sig_strength)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSignalStrength"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSignalStrength> signalStrength =
      result->convertSignalStrength_V1_2(sig_strength);
    result->updateSignalStrength(signalStrength);
  } else {
    DEBUG("getSignalStrength error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getVoiceRegistrationStateResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const IRadioVoiceRegStateResult_V1_2& voiceRegResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getVoiceRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_2(&voiceRegResponse.cellIdentity);
    nsString rplmn;
    rplmn.AssignASCII("");
    RefPtr<nsVoiceRegState> voiceRegState =
      new nsVoiceRegState(convertRegState(voiceRegResponse.regState),
                          voiceRegResponse.rat,
                          voiceRegResponse.cssSupported,
                          voiceRegResponse.roamingIndicator,
                          voiceRegResponse.systemIsInPrl,
                          voiceRegResponse.defaultRoamingIndicator,
                          voiceRegResponse.reasonForDenial,
                          cellIdentity,
                          rplmn);
    result->updateVoiceRegStatus(voiceRegState);
  } else {
    DEBUG("getVoiceRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataRegistrationStateResponse_1_2(
  const RadioResponseInfo_V1_0& info,
  const IRadioDataRegStateResult_V1_2& dataRegResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_2(&dataRegResponse.cellIdentity);
    nsString rplmn;
    rplmn.AssignASCII("");
    RefPtr<nsDataRegState> dataRegState =
      new nsDataRegState(convertRegState(dataRegResponse.regState),
                         dataRegResponse.rat,
                         dataRegResponse.reasonDataDenied,
                         dataRegResponse.maxDataCalls,
                         cellIdentity,
                         nullptr,
                         nullptr,
                         rplmn,
                         nullptr);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setSystemSelectionChannelsResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  DEBUG("setSystemSelectionChannelsResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"setSystemSelectionChannels"_ns);
  return Void();
}
#endif

Return<void>
nsRilResponse::setSystemSelectionChannelsResponse(
  const RadioResponseInfo_V1_0& info)
{
  DEBUG("setSystemSelectionChannelsResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"setSystemSelectionChannels"_ns);
  return Void();
}

Return<void>
nsRilResponse::enableUiccApplicationsResponse(
  const RadioResponseInfo_V1_0& info)
{
  DEBUG("enableUiccApplicationsResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"enableUiccApplications"_ns);
  return Void();
}

Return<void>
nsRilResponse::areUiccApplicationsEnabledResponse(
  const RadioResponseInfo_V1_0& info,
  bool isEnabled)
{
  DEBUG("areUiccApplicationsEnabledResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"areUiccApplicationsEnabled"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateUiccAppEnabledReponse(isEnabled);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::enableModemResponse(const RadioResponseInfo_V1_0& info)
{
  DEBUG("enableModemResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"enableModem"_ns);
  return Void();
}

Return<void>
nsRilResponse::getModemStackStatusResponse(const RadioResponseInfo_V1_0& info,
                                           bool isEnabled)
{
  DEBUG("getModemStackStatusResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getModemStackStatus"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateModemStackStatusReponse(isEnabled);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::emergencyDialResponse(const RadioResponseInfo_V1_0& info)
{
  DEBUG("emergencyDialResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"emergencyDial"_ns);
  return Void();
}

Return<void>
nsRilResponse::setPreferredNetworkTypeBitmapResponse(
  const RadioResponseInfo_V1_0& info)
{
  DEBUG("setPreferredNetworkTypeBitmapResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"setPreferredNetworkTypeBitmap"_ns);
  return Void();
}

#if ANDROID_VERSION >= 33
Return<void>
nsRilResponse::startNetworkScanResponse_1_4(const RadioResponseInfo_V1_0& info)
{
  DEBUG("startNetworkScanResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"startNetworkScan"_ns);
  return Void();
}

Return<void>
nsRilResponse::startNetworkScanResponse_1_5(const RadioResponseInfo_V1_0& info)
{
  DEBUG("startNetworkScanResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"startNetworkScan"_ns);
  return Void();
}

Return<void>
nsRilResponse::getPreferredNetworkTypeBitmapResponse(
  const RadioResponseInfo_V1_0& info,
  hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap)
{
  DEBUG("getPreferredNetworkTypeBitmapResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getPreferredNetworkTypeBitmap"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    result->updateAllowedNetworkTypesBitmask(
      convertHalNetworkTypeBitMask_V1_4(networkTypeBitmap));
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getIccCardStatusResponse_1_4(const RadioResponseInfo_V1_0& info,
                                            const CardStatus_V1_4& aCardStatus)
{
  DEBUG("getIccCardStatusResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getICCStatus"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getICCStatus success.");
    uint32_t numApplications = aCardStatus.base.base.applications.size();

    if (numApplications > nsIRilResponseResult::CARD_MAX_APPS) {
      numApplications = nsIRilResponseResult::CARD_MAX_APPS;
    }

    nsTArray<RefPtr<nsAppStatus>> applications(numApplications);

    for (uint32_t i = 0; i < numApplications; i++) {
      RefPtr<nsAppStatus> application = new nsAppStatus(
        convertAppType(aCardStatus.base.base.applications[i].appType),
        convertAppState(aCardStatus.base.base.applications[i].appState),
        convertPersoSubstate(
          aCardStatus.base.base.applications[i].persoSubstate),
        NS_ConvertUTF8toUTF16(
          aCardStatus.base.base.applications[i].aidPtr.c_str()),
        NS_ConvertUTF8toUTF16(
          aCardStatus.base.base.applications[i].appLabelPtr.c_str()),
        aCardStatus.base.base.applications[i].pin1Replaced,
        convertPinState(aCardStatus.base.base.applications[i].pin1),
        convertPinState(aCardStatus.base.base.applications[i].pin2));

      applications.AppendElement(application);
    }

    RefPtr<nsCardStatus> cardStatus =
      new nsCardStatus(convertCardState(aCardStatus.base.base.cardState),
                       convertPinState(aCardStatus.base.base.universalPinState),
                       aCardStatus.base.base.gsmUmtsSubscriptionAppIndex,
                       aCardStatus.base.base.cdmaSubscriptionAppIndex,
                       aCardStatus.base.base.imsSubscriptionAppIndex,
                       applications,
                       aCardStatus.base.physicalSlotId,
                       NS_ConvertUTF8toUTF16(aCardStatus.base.atr.c_str()),
                       NS_ConvertUTF8toUTF16(aCardStatus.base.iccid.c_str()),
                       NS_ConvertUTF8toUTF16(aCardStatus.eid.c_str()));
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getSignalStrengthResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const SignalStrength_V1_4& signalStrength)
{
  DEBUG("getSignalStrengthResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSignalStrength"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSignalStrength> ss =
      result->convertSignalStrength_V1_4(signalStrength);
    result->updateSignalStrength(ss);
  } else {
    DEBUG("getSignalStrength error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataRegistrationStateResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const DataRegStateResult_V1_4& dataRegResponse)
{
  DEBUG("getDataRegistrationStateResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_2(&dataRegResponse.base.cellIdentity);
    RefPtr<nsNrIndicators> nrIndicators =
      result->convertNrIndicators(&dataRegResponse.nrIndicators);
    RefPtr<nsLteVopsInfo> lteVopsInfo = nullptr;
    if ((uint8_t)(dataRegResponse.vopsInfo.getDiscriminator()) ==
        nsDataRegState::LTEVOPSINFOR) {
      lteVopsInfo =
        result->convertVopsInfo(&dataRegResponse.vopsInfo.lteVopsInfo());
    }
    nsString rplmn;
    rplmn.AssignASCII("");
    RefPtr<nsDataRegState> dataRegState =
      new nsDataRegState(convertRegState(dataRegResponse.base.regState),
                         dataRegResponse.base.rat,
                         dataRegResponse.base.reasonDataDenied,
                         dataRegResponse.base.maxDataCalls,
                         cellIdentity,
                         lteVopsInfo,
                         nrIndicators,
                         rplmn,
                         nullptr);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setupDataCallResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const SetupDataCallResult_V1_4& dcResponse)
{
  DEBUG("setupDataCallResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"setupDataCall"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
      result->convertDcResponse_V1_4(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataCallListResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const hidl_vec<SetupDataCallResult_V1_4>& dcResponse)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataCallList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numDataCall = dcResponse.size();
    nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

    for (uint32_t i = 0; i < numDataCall; i++) {
      RefPtr<nsSetupDataCallResult> datcall =
        result->convertDcResponse_V1_4(dcResponse[i]);
      aDcLists.AppendElement(datcall);
    }
    result->updateDcList(aDcLists);
  } else {
    DEBUG("getDataCallListResponse_1_4 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setAllowedCarriersResponse_1_4(
  const RadioResponseInfo_V1_0& info)
{
  DEBUG("setAllowedCarriersResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  defaultResponse(rspInfo, u"setAllowedCarriers"_ns);
  return Void();
}

Return<void>
nsRilResponse::getBarringInfoResponse(
  const RadioResponseInfo_V1_0& info,
  const CellIdentity_V1_5& cellIdentity,
  const hidl_vec<BarringInfo_V1_5>& barringInfos)
{
  DEBUG("getBarringInfoResponse");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getBarringInfo"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {

    nsTArray<RefPtr<nsIBarringInfo>> data;
    for (uint32_t i = 0; i < barringInfos.size(); i++) {
      RefPtr<nsIBarringInfo> info = new nsBarringInfo(barringInfos[i]);
      data.AppendElement(info);
    }

    RefPtr<nsCellIdentity> rv = result->convertCellIdentity_V1_5(&cellIdentity);

    result->updateGetBarringInfoResult(new nsGetBarringInfoResult(rv, data));
  } else {
    DEBUG("getBarringInfoResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getCellInfoListResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const hidl_vec<CellInfo_V1_4>& cellInfo)
{
  DEBUG("getCellInfoListResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCellInfoList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell =
        result->convertRilCellInfo_V1_4(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse_1_4 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getAllowedCarriersResponse_1_4(
  const RadioResponseInfo_V1_0& info,
  const CarrierRestrictionsWithPriority_V1_4& carriers,
  SimLockMultiSimPolicy_V1_4 multiSimPolicy)
{
  DEBUG("getAllowedCarriersResponse_1_4");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getAllowedCarriers"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
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
    DEBUG("getAllowedCarriersResponse_1_4 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}
#endif

// Helper function
void
nsRilResponse::defaultResponse(const RadioResponseInfo_V1_0& rspInfo,
                               const nsString& rilmessageType)
{
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    rilmessageType, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  mRIL->sendRilResponseResult(result);
}

#if ANDROID_VERSION >= 33
void
nsRilResponse::defaultResponse(const RadioResponseInfo_V1_6& rspInfo,
                               const nsString& rilmessageType)
{
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    rilmessageType, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  mRIL->sendRilResponseResult(result);
}

Return<void>
nsRilResponse::setIndicationFilterResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return Void();
}

Return<void>
nsRilResponse::setDataProfileResponse_1_5(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setDataProfile"_ns);
  return Void();
}

Return<void>
nsRilResponse::setInitialAttachApnResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setInitialAttachApn"_ns);
  return Void();
}

Return<void>
nsRilResponse::setNetworkSelectionModeManualResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"selectNetwork"_ns);
  return Void();
}

Return<void>
nsRilResponse::setupDataCallResponse_1_5(
  const RadioResponseInfo_V1_0& info,
  const SetupDataCallResult_V1_5& dcResponse)
{
  DEBUG("setupDataCallResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"setupDataCall"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
      result->convertDcResponse_V1_5(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall V1.5 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setRadioPowerResponse_1_5(const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setRadioEnabled"_ns);
  return Void();
}

Return<void>
nsRilResponse::getDataRegistrationStateResponse_1_5(
  const RadioResponseInfo_V1_0& info,
  const RegStateResult_V1_5& dataRegResponse)
{
  DEBUG("getDataRegistrationStateResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_5(&dataRegResponse.cellIdentity);

    RefPtr<nsNrIndicators> nrIndicators = nullptr;
    RefPtr<nsLteVopsInfo> lteVopsInfo = nullptr;
    if (dataRegResponse.accessTechnologySpecificInfo.getDiscriminator() ==
        ATSIType::eutranInfo) {
      nrIndicators = result->convertNrIndicators(
        &dataRegResponse.accessTechnologySpecificInfo.eutranInfo()
           .nrIndicators);
      lteVopsInfo = result->convertVopsInfo(
        &dataRegResponse.accessTechnologySpecificInfo.eutranInfo().lteVopsInfo);
    }
    RefPtr<nsDataRegState> dataRegState = new nsDataRegState(
      convertRegState(dataRegResponse.regState),
      (int32_t)dataRegResponse.rat,
      (int32_t)dataRegResponse.reasonForDenial,
      nsDataRegState::MAX_DATA_CALLS,
      cellIdentity,
      lteVopsInfo,
      nrIndicators,
      NS_ConvertUTF8toUTF16(dataRegResponse.registeredPlmn.c_str()),
      nullptr);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getVoiceRegistrationStateResponse_1_5(
  const RadioResponseInfo_V1_0& info,
  const RegStateResult_V1_5& voiceRegResponse)
{
  DEBUG("getVoiceRegistrationStateResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getVoiceRegistrationState"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_5(&voiceRegResponse.cellIdentity);
    bool cssSupported = false;
    int32_t roamingIndicator = 0;
    int32_t systemIsInPrl = 0;
    int32_t defaultRoamingIndicator = 0;

    if (voiceRegResponse.accessTechnologySpecificInfo.getDiscriminator() ==
        ATSIType::cdmaInfo) {
      cssSupported =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo().cssSupported;
      roamingIndicator =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .roamingIndicator;
      systemIsInPrl =
        (int32_t)voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .systemIsInPrl;
      defaultRoamingIndicator =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .defaultRoamingIndicator;
    }
    RefPtr<nsVoiceRegState> voiceRegState = new nsVoiceRegState(
      convertRegState(voiceRegResponse.regState),
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setSignalStrengthReportingCriteriaResponse_1_5(
  const RadioResponseInfo_V1_0& info)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setSignalStrengthReportingCriteria"_ns);
  return Void();
}

Return<void>
nsRilResponse::getDataCallListResponse_1_5(
  const RadioResponseInfo_V1_0& info,
  const hidl_vec<SetupDataCallResult_V1_5>& dcResponse)
{
  DEBUG("getDataCallListResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataCallList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numDataCall = dcResponse.size();
    nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

    for (uint32_t i = 0; i < numDataCall; i++) {
      RefPtr<nsSetupDataCallResult> datcall =
        result->convertDcResponse_V1_5(dcResponse[i]);
      aDcLists.AppendElement(datcall);
    }
    result->updateDcList(aDcLists);
  } else {
    DEBUG("getDataCallListResponse_1_5 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getCellInfoListResponse_1_5(
  const RadioResponseInfo_V1_0& info,
  const hidl_vec<CellInfo_V1_5>& cellInfo)
{
  DEBUG("getCellInfoListResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCellInfoList"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell =
        result->convertRilCellInfo_V1_5(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse_1_5 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}
#endif

Return<void>
nsRilResponse::sendCdmaSmsExpectMoreResponse(const RadioResponseInfo_V1_0& info,
                                             const SendSmsResult_V1_0& sms)
{}

#if ANDROID_VERSION >= 33
Return<void>
nsRilResponse::supplySimDepersonalizationResponse(
  const RadioResponseInfo_V1_0& info,
  PersoSubstate_V1_5 persoType,
  int32_t remainingRetries)
{
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"supplySimDepersonalization"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError_V1_0::NONE) {
    RefPtr<nsSupplySimDepersonalizationResult> aResult =
      new nsSupplySimDepersonalizationResult((int32_t)persoType,
                                             remainingRetries);
    result->updateSimDepersonalizationResult(aResult);
  } else {
    DEBUG("supplySimDepersonalization error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getIccCardStatusResponse_1_5(const RadioResponseInfo_V1_0& info,
                                            const CardStatus_V1_5& aCardStatus)
{
  DEBUG("getIccCardStatusResponse_1_5");
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
    u"getICCStatus"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError_V1_0::NONE) {
    DEBUG("getICCStatus success.");
    uint32_t numApplications = aCardStatus.applications.size();

    if (numApplications > nsIRilResponseResult::CARD_MAX_APPS) {
      numApplications = nsIRilResponseResult::CARD_MAX_APPS;
    }

    nsTArray<RefPtr<nsAppStatus>> applications(numApplications);

    for (uint32_t i = 0; i < numApplications; i++) {
      RefPtr<nsAppStatus> application = new nsAppStatus(
        convertAppType(aCardStatus.applications[i].base.appType),
        convertAppState(aCardStatus.applications[i].base.appState),
        convertPersoSubstate(aCardStatus.applications[i].persoSubstate),
        NS_ConvertUTF8toUTF16(aCardStatus.applications[i].base.aidPtr.c_str()),
        NS_ConvertUTF8toUTF16(
          aCardStatus.applications[i].base.appLabelPtr.c_str()),
        aCardStatus.applications[i].base.pin1Replaced,
        convertPinState(aCardStatus.applications[i].base.pin1),
        convertPinState(aCardStatus.applications[i].base.pin2));

      applications.AppendElement(application);
    }

    RefPtr<nsCardStatus> cardStatus = new nsCardStatus(
      convertCardState(aCardStatus.base.base.base.cardState),
      convertPinState(aCardStatus.base.base.base.universalPinState),
      aCardStatus.base.base.base.gsmUmtsSubscriptionAppIndex,
      aCardStatus.base.base.base.cdmaSubscriptionAppIndex,
      aCardStatus.base.base.base.imsSubscriptionAppIndex,
      applications,
      aCardStatus.base.base.physicalSlotId,
      NS_ConvertUTF8toUTF16(aCardStatus.base.base.atr.c_str()),
      NS_ConvertUTF8toUTF16(aCardStatus.base.base.iccid.c_str()),
      NS_ConvertUTF8toUTF16(aCardStatus.base.eid.c_str()));
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setupDataCallResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const SetupDataCallResult_V1_6& dcResponse)
{
  DEBUG("setupDataCallResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"setupDataCall"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
      result->convertDcResponse_V1_6(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall V1.6 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setRadioPowerResponse_1_6(const RadioResponseInfo_V1_6& info)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"setRadioEnabled"_ns);
  return Void();
}

Return<void>
nsRilResponse::getDataRegistrationStateResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const RegStateResult_V1_6& dataRegResponse)
{
  DEBUG("getDataRegistrationStateResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataRegistrationState"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_5(&dataRegResponse.cellIdentity);

    RefPtr<nsNrIndicators> nrIndicators = nullptr;
    RefPtr<nsLteVopsInfo> lteVopsInfo = nullptr;
    RefPtr<nsNrVopsInfo> nrVopsInfo = nullptr;
    if (dataRegResponse.accessTechnologySpecificInfo.getDiscriminator() ==
        ATSIType_V1_6::eutranInfo) {
      nrIndicators = result->convertNrIndicators(
        &dataRegResponse.accessTechnologySpecificInfo.eutranInfo()
           .nrIndicators);
      lteVopsInfo = result->convertVopsInfo(
        &dataRegResponse.accessTechnologySpecificInfo.eutranInfo().lteVopsInfo);
    } else if (dataRegResponse.accessTechnologySpecificInfo
                 .getDiscriminator() == ATSIType_V1_6::ngranNrVopsInfo) {
      nrVopsInfo = result->convertNrVopsInfo(
        &dataRegResponse.accessTechnologySpecificInfo.ngranNrVopsInfo());
    }
    RefPtr<nsDataRegState> dataRegState = new nsDataRegState(
      convertRegState(dataRegResponse.regState),
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getVoiceRegistrationStateResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const RegStateResult_V1_6& voiceRegResponse)
{
  DEBUG("getVoiceRegistrationStateResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getVoiceRegistrationState"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
      result->convertCellIdentity_V1_5(&voiceRegResponse.cellIdentity);
    bool cssSupported = false;
    int32_t roamingIndicator = 0;
    int32_t systemIsInPrl = 0;
    int32_t defaultRoamingIndicator = 0;

    if (voiceRegResponse.accessTechnologySpecificInfo.getDiscriminator() ==
        ATSIType_V1_6::cdmaInfo) {
      cssSupported =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo().cssSupported;
      roamingIndicator =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .roamingIndicator;
      systemIsInPrl =
        (int32_t)voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .systemIsInPrl;
      defaultRoamingIndicator =
        voiceRegResponse.accessTechnologySpecificInfo.cdmaInfo()
          .defaultRoamingIndicator;
    } else if (voiceRegResponse.accessTechnologySpecificInfo
                 .getDiscriminator() == ATSIType_V1_6::geranDtmSupported) {
      cssSupported =
        voiceRegResponse.accessTechnologySpecificInfo.geranDtmSupported();
    }
    RefPtr<nsVoiceRegState> voiceRegState = new nsVoiceRegState(
      convertRegState(voiceRegResponse.regState),
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getSignalStrengthResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const SignalStrength_V1_6& signalStrength)
{
  DEBUG("getSignalStrengthResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSignalStrength"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSignalStrength> ss =
      result->convertSignalStrength_V1_6(signalStrength);
    result->updateSignalStrength(ss);
  } else {
    DEBUG("getSignalStrength error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getCurrentCallsResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const hidl_vec<IRadioCall_V1_6>& calls)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCurrentCalls"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    uint32_t numCalls = calls.size();
    DEBUG("getCurrentCalls_1_6 numCalls= %d", numCalls);
    nsTArray<RefPtr<nsCall>> aCalls(numCalls);

    for (uint32_t i = 0; i < numCalls; i++) {
      uint32_t numUusInfo = calls[i].base.base.uusInfo.size();
      DEBUG("getCurrentCalls_1_6 numUusInfo= %d", numUusInfo);
      nsTArray<RefPtr<nsUusInfo>> aUusInfos(numUusInfo);

      for (uint32_t j = 0; j < numUusInfo; j++) {
        RefPtr<nsUusInfo> uusinfo = new nsUusInfo(
          convertUusType(calls[i].base.base.uusInfo[j].uusType),
          convertUusDcs(calls[i].base.base.uusInfo[j].uusDcs),
          NS_ConvertUTF8toUTF16(calls[i].base.base.uusInfo[j].uusData.c_str()));

        aUusInfos.AppendElement(uusinfo);
      }

      DEBUG("getCurrentCalls_1_6 index= %d  state=%d",
            calls[i].base.base.index,
            convertCallState(calls[i].base.base.state));
      RefPtr<nsCall> call = new nsCall(
        convertCallState(calls[i].base.base.state),
        calls[i].base.base.index,
        calls[i].base.base.toa,
        calls[i].base.base.isMpty,
        calls[i].base.base.isMT,
        int32_t(calls[i].base.base.als),
        calls[i].base.base.isVoice,
        calls[i].base.base.isVoicePrivacy,
        NS_ConvertUTF8toUTF16(calls[i].base.base.number.c_str()),
        convertCallPresentation(calls[i].base.base.numberPresentation),
        NS_ConvertUTF8toUTF16(calls[i].base.base.name.c_str()),
        convertCallPresentation(calls[i].base.base.namePresentation),
        aUusInfos,
        nsRilResult::convertAudioQuality(calls[i].base.audioQuality),
        NS_ConvertUTF8toUTF16(calls[i].forwardedNumber.c_str()));
      aCalls.AppendElement(call);
    }
    result->updateCurrentCalls(aCalls);
  } else {
    DEBUG("getCurrentCalls error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getDataCallListResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const hidl_vec<SetupDataCallResult_V1_6>& dcResponse)
{
  DEBUG("getDataCallListResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getDataCallList"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    uint32_t numDataCall = dcResponse.size();
    nsTArray<RefPtr<nsSetupDataCallResult>> aDcLists(numDataCall);

    for (uint32_t i = 0; i < numDataCall; i++) {
      RefPtr<nsSetupDataCallResult> datcall =
        result->convertDcResponse_V1_6(dcResponse[i]);
      aDcLists.AppendElement(datcall);
    }
    result->updateDcList(aDcLists);
  } else {
    DEBUG("getDataCallListResponse_1_6 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendSmsResponse_1_6(const RadioResponseInfo_V1_6& info,
                                   const SendSmsResult_V1_0& sms)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"sendSMS"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));

  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSmsResponse_1_6 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendSmsExpectMoreResponse_1_6(const RadioResponseInfo_V1_6& info,
                                             const SendSmsResult_V1_0& sms)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"sendSMS"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));

  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSmsExpectMoreResponse_1_6 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendCdmaSmsResponse_1_6(const RadioResponseInfo_V1_6& info,
                                       const SendSmsResult_V1_0& sms)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"sendSMS"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));

  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendCdmaSmsResponse_1_6 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::sendCdmaSmsExpectMoreResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const SendSmsResult_V1_0& sms)
{
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"sendSMS"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));

  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
      sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()), sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendCdmaSmsExpectMoreResponse_1_6 error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setSimCardPowerResponse_1_6(const RadioResponseInfo_V1_6& info)
{
  DEBUG("setSimCardPowerResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"setSimCardPowerResponse"_ns);
  return Void();
}

Return<void>
nsRilResponse::setNrDualConnectivityStateResponse(
  const RadioResponseInfo_V1_6& info)
{
  DEBUG("setNrDualConnectivityStateResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"setNrDualConnectivityState"_ns);
  return Void();
}

Return<void>
nsRilResponse::isNrDualConnectivityEnabledResponse(
  const RadioResponseInfo_V1_6& info,
  bool isEnabled)
{
  DEBUG("isNrDualConnectivityEnabledResponse");

  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"isNrDualConnectivityEnabled"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    result->updateNrDualConnectivityEnabled(isEnabled);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::allocatePduSessionIdResponse(const RadioResponseInfo_V1_6& info,
                                            int32_t id)
{
  DEBUG("allocatePduSessionIdResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"allocatePduSessionId"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    result->updateAllocatedId(id);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::releasePduSessionIdResponse(const RadioResponseInfo_V1_6& info)
{
  DEBUG("releasePduSessionIdResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"releasePduSessionId"_ns);
  return Void();
}

Return<void>
nsRilResponse::startHandoverResponse(const RadioResponseInfo_V1_6& info)
{
  DEBUG("startHandoverResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"startHandover"_ns);
  return Void();
}

Return<void>
nsRilResponse::cancelHandoverResponse(const RadioResponseInfo_V1_6& info)
{

  DEBUG("cancelHandoverResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"cancelHandover"_ns);
  return Void();
}

Return<void>
nsRilResponse::setAllowedNetworkTypesBitmapResponse(
  const RadioResponseInfo_V1_6& info)
{
  DEBUG("setAllowedNetworkTypesBitmapResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"setPreferredNetworkTypeBitmap"_ns);
  return Void();
}

Return<void>
nsRilResponse::getAllowedNetworkTypesBitmapResponse(
  const RadioResponseInfo_V1_6& info,
  ::android::hardware::hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap)
{
  DEBUG("getAllowedNetworkTypesBitmapResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getAllowedNetworkTypesBitmap"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    result->updateAllowedNetworkTypesBitmask(
      convertHalNetworkTypeBitMask_V1_4(networkTypeBitmap));
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::setDataThrottlingResponse(const RadioResponseInfo_V1_6& info)
{
  DEBUG("setDataThrottlingResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"setDataThrottling"_ns);
  return Void();
}

Return<void>
nsRilResponse::getSystemSelectionChannelsResponse(
  const RadioResponseInfo_V1_6& info,
  const hidl_vec<RadioAccessSpecifier_V1_5>& specifiers)
{
  DEBUG("getSystemSelectionChannelsResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  nsTArray<RefPtr<nsIRadioAccessSpecifier>> specifiersArray;
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSystemSelectionChannels"_ns,
                            rspInfo.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    for (uint32_t i = 0; i < specifiers.size(); i++) {
      RefPtr<nsIRadioAccessSpecifier> specifier =
        new nsRadioAccessSpecifier(specifiers[i]);
      specifiersArray.AppendElement(specifier);
    }
    result->updateRadioAccessSpecifiers(specifiersArray);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getCellInfoListResponse_1_6(
  const RadioResponseInfo_V1_6& info,
  const hidl_vec<CellInfo_V1_6>& cellInfo)
{
  DEBUG("getCellInfoListResponse_1_6");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getCellInfoList"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell =
        result->convertRilCellInfo_V1_6(&cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getCellInfoListResponse_1_6 error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getSlicingConfigResponse(const RadioResponseInfo_V1_6& info,
                                        const SlicingConfig_V1_6& slicingConfig)
{
  DEBUG("getSlicingConfigResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);
  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSlicingConfig"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    nsTArray<RefPtr<nsISliceInfo>> aSliceInfos;
    for (uint32_t i = 0; i < slicingConfig.sliceInfo.size(); i++) {
      SliceInfo_V1_6 aSlice = slicingConfig.sliceInfo[i];
      RefPtr<nsISliceInfo> data = new nsSliceInfo(aSlice);
      aSliceInfos.AppendElement(data);
    }

    nsTArray<RefPtr<nsIUrspRule>> aRules;
    for (uint32_t i = 0; i < slicingConfig.urspRules.size(); i++) {
      UrspRule_V1_6 aUrspRule = slicingConfig.urspRules[i];
      RefPtr<nsIUrspRule> data = new nsUrspRule(aUrspRule);
      aRules.AppendElement(data);
    }
    RefPtr<nsISlicingConfig> config = new nsSlicingConfig(aRules, aSliceInfos);
    result->updateSlicingConfig(config);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::getSimPhonebookRecordsResponse(
  const RadioResponseInfo_V1_6& info)
{
  DEBUG("getSimPhonebookRecordsResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  defaultResponse(rspInfoV1_6, u"getSimPhonebookRecords"_ns);
  return Void();
}

Return<void>
nsRilResponse::getSimPhonebookCapacityResponse(
  const RadioResponseInfo_V1_6& info,
  const PhonebookCapacity_V1_6& capacity)
{
  DEBUG("getSimPhonebookCapacityResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"getSimPhonebookCapacity"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    nsIPhonebookCapacity* phoneBookCapacity = new nsPhonebookCapacity(capacity);
    result->updatePhonebookCapacity(phoneBookCapacity);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void>
nsRilResponse::updateSimPhonebookRecordsResponse(
  const RadioResponseInfo_V1_6& info,
  int32_t updatedRecordIndex)
{
  DEBUG("updateSimPhonebookRecordsResponse");
  rspInfoV1_6 = info;
  mRIL->processResponse(rspInfoV1_6.type);

  RefPtr<nsRilResponseResult> result =
    new nsRilResponseResult(u"updateSimPhonebookRecords"_ns,
                            rspInfoV1_6.serial,
                            convertRadioErrorToNum(rspInfoV1_6.error));
  if (rspInfoV1_6.error == RadioError_V1_6::NONE) {
    result->updateUpdatedRecordIndex(updatedRecordIndex);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

int32_t
nsRilResponse::convertHalNetworkTypeBitMask_V1_4(
  hidl_bitfield<RadioAccessFamily_V1_4> networkTypeBitmap)
{
  int32_t networkTypeRaf = 0;
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::GSM) != 0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_GSM - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::GPRS) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_GPRS - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::EDGE) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EDGE - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::IS95A) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IS95A - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::IS95B) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IS95B - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::ONE_X_RTT) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_1XRTT - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_0) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDO0 - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_A) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_B) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOB - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::EHRPD) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_EHRPD - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::HSUPA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSUPA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::HSDPA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSDPA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::HSPA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSPA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::HSPAP) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_HSPAP - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::UMTS) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_UMTS - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::TD_SCDMA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_TD_SCDMA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::LTE) != 0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_LTE - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioAccessFamily::LTE_CA) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_LTE_CA - 1));
  }
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_4::RadioAccessFamily::NR) != 0) {
    networkTypeRaf |= (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_NR));
  }
  // Android use "if ((raf & (1 << ServiceState.RIL_RADIO_TECHNOLOGY_IWLAN)) !=
  // 0): to check bitmap. Is it  ok to use RadioAccessFamily.IWLAN? We need to
  // check whether it is ok on QCOM device.
  if ((networkTypeBitmap &
       ::android::hardware::radio::V1_0::RadioTechnology::IWLAN) != 0) {
    networkTypeRaf |=
      (1 << (nsIRadioTechnologyState::RADIO_CREG_TECH_IWLAN - 1));
  }
  return (networkTypeRaf == 0)
           ? nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN
           : networkTypeRaf;
}
#endif

int32_t
nsRilResponse::convertRadioErrorToNum(RadioError_V1_0 error)
{
  switch (error) {
    case RadioError_V1_0::NONE:
      return nsIRilResponseResult::RADIO_ERROR_NONE;
    case RadioError_V1_0::RADIO_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_NOT_AVAILABLE;
    case RadioError_V1_0::GENERIC_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
    case RadioError_V1_0::PASSWORD_INCORRECT:
      return nsIRilResponseResult::RADIO_ERROR_PASSWOR_INCORRECT;
    case RadioError_V1_0::SIM_PIN2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PIN2;
    case RadioError_V1_0::SIM_PUK2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PUK2;
    case RadioError_V1_0::REQUEST_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_NOT_SUPPORTED;
    case RadioError_V1_0::CANCELLED:
      return nsIRilResponseResult::RADIO_ERROR_CANCELLED;
    case RadioError_V1_0::OP_NOT_ALLOWED_DURING_VOICE_CALL:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_DURING_VOICE_CALL;
    case RadioError_V1_0::OP_NOT_ALLOWED_BEFORE_REG_TO_NW:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
    case RadioError_V1_0::SMS_SEND_FAIL_RETRY:
      return nsIRilResponseResult::RADIO_ERROR_SMS_SEND_FAIL_RETRY;
    case RadioError_V1_0::SIM_ABSENT:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ABSENT;
    case RadioError_V1_0::SUBSCRIPTION_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_AVAILABLE;
    case RadioError_V1_0::MODE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_MODE_NOT_SUPPORTED;
    case RadioError_V1_0::FDN_CHECK_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_FDN_CHECK_FAILURE;
    case RadioError_V1_0::ILLEGAL_SIM_OR_ME:
      return nsIRilResponseResult::RADIO_ERROR_ILLEGAL_SIM_OR_ME;
    case RadioError_V1_0::MISSING_RESOURCE:
      return nsIRilResponseResult::RADIO_ERROR_MISSING_RESOURCE;
    case RadioError_V1_0::NO_SUCH_ELEMENT:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ELEMENT;
    case RadioError_V1_0::DIAL_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_USSD;
    case RadioError_V1_0::DIAL_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_SS;
    case RadioError_V1_0::DIAL_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_DIAL;
    case RadioError_V1_0::USSD_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_DIAL;
    case RadioError_V1_0::USSD_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_SS;
    case RadioError_V1_0::USSD_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_USSD;
    case RadioError_V1_0::SS_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_DIAL;
    case RadioError_V1_0::SS_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_USSD;
    case RadioError_V1_0::SUBSCRIPTION_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_SUPPORTED;
    case RadioError_V1_0::SS_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_SS;
    case RadioError_V1_0::LCE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_LCE_NOT_SUPPORTED;
    case RadioError_V1_0::NO_MEMORY:
      return nsIRilResponseResult::RADIO_ERROR_NO_MEMORY;
    case RadioError_V1_0::INTERNAL_ERR:
      return nsIRilResponseResult::RADIO_ERROR_INTERNAL_ERR;
    case RadioError_V1_0::SYSTEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SYSTEM_ERR;
    case RadioError_V1_0::MODEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_MODEM_ERR;
    case RadioError_V1_0::INVALID_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_STATE;
    case RadioError_V1_0::NO_RESOURCES:
      return nsIRilResponseResult::RADIO_ERROR_NO_RESOURCES;
    case RadioError_V1_0::SIM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ERR;
    case RadioError_V1_0::INVALID_ARGUMENTS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_ARGUMENTS;
    case RadioError_V1_0::INVALID_SIM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SIM_STATE;
    case RadioError_V1_0::INVALID_MODEM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_MODEM_STATE;
    case RadioError_V1_0::INVALID_CALL_ID:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_CALL_ID;
    case RadioError_V1_0::NO_SMS_TO_ACK:
      return nsIRilResponseResult::RADIO_ERROR_NO_SMS_TO_ACK;
    case RadioError_V1_0::NETWORK_ERR:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_ERR;
    case RadioError_V1_0::REQUEST_RATE_LIMITED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_RATE_LIMITED;
    case RadioError_V1_0::SIM_BUSY:
      return nsIRilResponseResult::RADIO_ERROR_SIM_BUSY;
    case RadioError_V1_0::SIM_FULL:
      return nsIRilResponseResult::RADIO_ERROR_SIM_FULL;
    case RadioError_V1_0::NETWORK_REJECT:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_REJECT;
    case RadioError_V1_0::OPERATION_NOT_ALLOWED:
      return nsIRilResponseResult::RADIO_ERROR_OPERATION_NOT_ALLOWED;
    case RadioError_V1_0::EMPTY_RECORD:
      return nsIRilResponseResult::RADIO_ERROR_EMPTY_RECORD;
    case RadioError_V1_0::INVALID_SMS_FORMAT:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMS_FORMAT;
    case RadioError_V1_0::ENCODING_ERR:
      return nsIRilResponseResult::RADIO_ERROR_ENCODING_ERR;
    case RadioError_V1_0::INVALID_SMSC_ADDRESS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMSC_ADDRESS;
    case RadioError_V1_0::NO_SUCH_ENTRY:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ENTRY;
    case RadioError_V1_0::NETWORK_NOT_READY:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_NOT_READY;
    case RadioError_V1_0::NOT_PROVISIONED:
      return nsIRilResponseResult::RADIO_ERROR_NOT_PROVISIONED;
    case RadioError_V1_0::NO_SUBSCRIPTION:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUBSCRIPTION;
    case RadioError_V1_0::NO_NETWORK_FOUND:
      return nsIRilResponseResult::RADIO_ERROR_NO_NETWORK_FOUND;
    case RadioError_V1_0::DEVICE_IN_USE:
      return nsIRilResponseResult::RADIO_ERROR_DEVICE_IN_USE;
    case RadioError_V1_0::ABORTED:
      return nsIRilResponseResult::RADIO_ERROR_ABORTED;
    case RadioError_V1_0::INVALID_RESPONSE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_RESPONSE;
    default:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
  }
}

#if ANDROID_VERSION >= 33
int32_t
nsRilResponse::convertRadioErrorToNum(RadioError_V1_6 error)
{
  switch (error) {
    case RadioError_V1_6::NONE:
      return nsIRilResponseResult::RADIO_ERROR_NONE;
    case RadioError_V1_6::RADIO_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_NOT_AVAILABLE;
    case RadioError_V1_6::GENERIC_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
    case RadioError_V1_6::PASSWORD_INCORRECT:
      return nsIRilResponseResult::RADIO_ERROR_PASSWOR_INCORRECT;
    case RadioError_V1_6::SIM_PIN2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PIN2;
    case RadioError_V1_6::SIM_PUK2:
      return nsIRilResponseResult::RADIO_ERROR_SIM_PUK2;
    case RadioError_V1_6::REQUEST_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_NOT_SUPPORTED;
    case RadioError_V1_6::CANCELLED:
      return nsIRilResponseResult::RADIO_ERROR_CANCELLED;
    case RadioError_V1_6::OP_NOT_ALLOWED_DURING_VOICE_CALL:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_DURING_VOICE_CALL;
    case RadioError_V1_6::OP_NOT_ALLOWED_BEFORE_REG_TO_NW:
      return nsIRilResponseResult::RADIO_ERROR_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
    case RadioError_V1_6::SMS_SEND_FAIL_RETRY:
      return nsIRilResponseResult::RADIO_ERROR_SMS_SEND_FAIL_RETRY;
    case RadioError_V1_6::SIM_ABSENT:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ABSENT;
    case RadioError_V1_6::SUBSCRIPTION_NOT_AVAILABLE:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_AVAILABLE;
    case RadioError_V1_6::MODE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_MODE_NOT_SUPPORTED;
    case RadioError_V1_6::FDN_CHECK_FAILURE:
      return nsIRilResponseResult::RADIO_ERROR_FDN_CHECK_FAILURE;
    case RadioError_V1_6::ILLEGAL_SIM_OR_ME:
      return nsIRilResponseResult::RADIO_ERROR_ILLEGAL_SIM_OR_ME;
    case RadioError_V1_6::MISSING_RESOURCE:
      return nsIRilResponseResult::RADIO_ERROR_MISSING_RESOURCE;
    case RadioError_V1_6::NO_SUCH_ELEMENT:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ELEMENT;
    case RadioError_V1_6::DIAL_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_USSD;
    case RadioError_V1_6::DIAL_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_SS;
    case RadioError_V1_6::DIAL_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_DIAL_MODIFIED_TO_DIAL;
    case RadioError_V1_6::USSD_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_DIAL;
    case RadioError_V1_6::USSD_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_SS;
    case RadioError_V1_6::USSD_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_USSD_MODIFIED_TO_USSD;
    case RadioError_V1_6::SS_MODIFIED_TO_DIAL:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_DIAL;
    case RadioError_V1_6::SS_MODIFIED_TO_USSD:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_USSD;
    case RadioError_V1_6::SUBSCRIPTION_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_SUBSCRIPTION_NOT_SUPPORTED;
    case RadioError_V1_6::SS_MODIFIED_TO_SS:
      return nsIRilResponseResult::RADIO_ERROR_SS_MODIFIED_TO_SS;
    case RadioError_V1_6::LCE_NOT_SUPPORTED:
      return nsIRilResponseResult::RADIO_ERROR_LCE_NOT_SUPPORTED;
    case RadioError_V1_6::NO_MEMORY:
      return nsIRilResponseResult::RADIO_ERROR_NO_MEMORY;
    case RadioError_V1_6::INTERNAL_ERR:
      return nsIRilResponseResult::RADIO_ERROR_INTERNAL_ERR;
    case RadioError_V1_6::SYSTEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SYSTEM_ERR;
    case RadioError_V1_6::MODEM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_MODEM_ERR;
    case RadioError_V1_6::INVALID_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_STATE;
    case RadioError_V1_6::NO_RESOURCES:
      return nsIRilResponseResult::RADIO_ERROR_NO_RESOURCES;
    case RadioError_V1_6::SIM_ERR:
      return nsIRilResponseResult::RADIO_ERROR_SIM_ERR;
    case RadioError_V1_6::INVALID_ARGUMENTS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_ARGUMENTS;
    case RadioError_V1_6::INVALID_SIM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SIM_STATE;
    case RadioError_V1_6::INVALID_MODEM_STATE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_MODEM_STATE;
    case RadioError_V1_6::INVALID_CALL_ID:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_CALL_ID;
    case RadioError_V1_6::NO_SMS_TO_ACK:
      return nsIRilResponseResult::RADIO_ERROR_NO_SMS_TO_ACK;
    case RadioError_V1_6::NETWORK_ERR:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_ERR;
    case RadioError_V1_6::REQUEST_RATE_LIMITED:
      return nsIRilResponseResult::RADIO_ERROR_REQUEST_RATE_LIMITED;
    case RadioError_V1_6::SIM_BUSY:
      return nsIRilResponseResult::RADIO_ERROR_SIM_BUSY;
    case RadioError_V1_6::SIM_FULL:
      return nsIRilResponseResult::RADIO_ERROR_SIM_FULL;
    case RadioError_V1_6::NETWORK_REJECT:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_REJECT;
    case RadioError_V1_6::OPERATION_NOT_ALLOWED:
      return nsIRilResponseResult::RADIO_ERROR_OPERATION_NOT_ALLOWED;
    case RadioError_V1_6::EMPTY_RECORD:
      return nsIRilResponseResult::RADIO_ERROR_EMPTY_RECORD;
    case RadioError_V1_6::INVALID_SMS_FORMAT:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMS_FORMAT;
    case RadioError_V1_6::ENCODING_ERR:
      return nsIRilResponseResult::RADIO_ERROR_ENCODING_ERR;
    case RadioError_V1_6::INVALID_SMSC_ADDRESS:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_SMSC_ADDRESS;
    case RadioError_V1_6::NO_SUCH_ENTRY:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUCH_ENTRY;
    case RadioError_V1_6::NETWORK_NOT_READY:
      return nsIRilResponseResult::RADIO_ERROR_NETWORK_NOT_READY;
    case RadioError_V1_6::NOT_PROVISIONED:
      return nsIRilResponseResult::RADIO_ERROR_NOT_PROVISIONED;
    case RadioError_V1_6::NO_SUBSCRIPTION:
      return nsIRilResponseResult::RADIO_ERROR_NO_SUBSCRIPTION;
    case RadioError_V1_6::NO_NETWORK_FOUND:
      return nsIRilResponseResult::RADIO_ERROR_NO_NETWORK_FOUND;
    case RadioError_V1_6::DEVICE_IN_USE:
      return nsIRilResponseResult::RADIO_ERROR_DEVICE_IN_USE;
    case RadioError_V1_6::ABORTED:
      return nsIRilResponseResult::RADIO_ERROR_ABORTED;
    case RadioError_V1_6::INVALID_RESPONSE:
      return nsIRilResponseResult::RADIO_ERROR_INVALID_RESPONSE;
    case RadioError_V1_6::SIMULTANEOUS_SMS_AND_CALL_NOT_ALLOWED:
      return nsIRilResponseResult::
        RADIO_ERROR_SIMULTANEOUS_SMS_AND_CALL_NOT_ALLOWED;
    case RadioError_V1_6::ACCESS_BARRED:
      return nsIRilResponseResult::RADIO_ERROR_ACCESS_BARRED;
    case RadioError_V1_6::BLOCKED_DUE_TO_CALL:
      return nsIRilResponseResult::RADIO_ERROR_BLOCKED_DUE_TO_CALL;
    case RadioError_V1_6::RF_HARDWARE_ISSUE:
      return nsIRilResponseResult::RADIO_ERROR_RF_HARDWARE_ISSUE;
    case RadioError_V1_6::NO_RF_CALIBRATION_INFO:
      return nsIRilResponseResult::RADIO_ERROR_NO_RF_CALIBRATION_INFO;
    default:
      return nsIRilResponseResult::RADIO_ERROR_GENERIC_FAILURE;
  }
}
#endif

int32_t
nsRilResponse::covertLastCallFailCause(LastCallFailCause_V1_0 cause)
{
  switch (cause) {
    case LastCallFailCause_V1_0::UNOBTAINABLE_NUMBER:
      return nsILastCallFailCause::CALL_FAIL_UNOBTAINABLE_NUMBER;
    case LastCallFailCause_V1_0::NO_ROUTE_TO_DESTINATION:
      return nsILastCallFailCause::CALL_FAIL_NO_ROUTE_TO_DESTINATION;
    case LastCallFailCause_V1_0::CHANNEL_UNACCEPTABLE:
      return nsILastCallFailCause::CALL_FAIL_CHANNEL_UNACCEPTABLE;
    case LastCallFailCause_V1_0::OPERATOR_DETERMINED_BARRING:
      return nsILastCallFailCause::CALL_FAIL_OPERATOR_DETERMINED_BARRING;
    case LastCallFailCause_V1_0::NORMAL:
      return nsILastCallFailCause::CALL_FAIL_NORMAL;
    case LastCallFailCause_V1_0::BUSY:
      return nsILastCallFailCause::CALL_FAIL_BUSY;
    case LastCallFailCause_V1_0::NO_USER_RESPONDING:
      return nsILastCallFailCause::CALL_FAIL_NO_USER_RESPONDING;
    case LastCallFailCause_V1_0::NO_ANSWER_FROM_USER:
      return nsILastCallFailCause::CALL_FAIL_NO_ANSWER_FROM_USER;
    case LastCallFailCause_V1_0::CALL_REJECTED:
      return nsILastCallFailCause::CALL_FAIL_CALL_REJECTED;
    case LastCallFailCause_V1_0::NUMBER_CHANGED:
      return nsILastCallFailCause::CALL_FAIL_NUMBER_CHANGED;
    case LastCallFailCause_V1_0::PREEMPTION:
      return nsILastCallFailCause::CALL_FAIL_PREEMPTION;
    case LastCallFailCause_V1_0::DESTINATION_OUT_OF_ORDER:
      return nsILastCallFailCause::CALL_FAIL_DESTINATION_OUT_OF_ORDER;
    case LastCallFailCause_V1_0::INVALID_NUMBER_FORMAT:
      return nsILastCallFailCause::CALL_FAIL_INVALID_NUMBER_FORMAT;
    case LastCallFailCause_V1_0::FACILITY_REJECTED:
      return nsILastCallFailCause::CALL_FAIL_FACILITY_REJECTED;
    case LastCallFailCause_V1_0::RESP_TO_STATUS_ENQUIRY:
      return nsILastCallFailCause::CALL_FAIL_RESP_TO_STATUS_ENQUIRY;
    case LastCallFailCause_V1_0::NORMAL_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_NORMAL_UNSPECIFIED;
    case LastCallFailCause_V1_0::CONGESTION:
      return nsILastCallFailCause::CALL_FAIL_CONGESTION;
    case LastCallFailCause_V1_0::NETWORK_OUT_OF_ORDER:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_OUT_OF_ORDER;
    case LastCallFailCause_V1_0::TEMPORARY_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_TEMPORARY_FAILURE;
    case LastCallFailCause_V1_0::SWITCHING_EQUIPMENT_CONGESTION:
      return nsILastCallFailCause::CALL_FAIL_SWITCHING_EQUIPMENT_CONGESTION;
    case LastCallFailCause_V1_0::ACCESS_INFORMATION_DISCARDED:
      return nsILastCallFailCause::CALL_FAIL_ACCESS_INFORMATION_DISCARDED;
    case LastCallFailCause_V1_0::REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE:
      return nsILastCallFailCause::
        CALL_FAIL_REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE;
    case LastCallFailCause_V1_0::RESOURCES_UNAVAILABLE_OR_UNSPECIFIED:
      return nsILastCallFailCause::
        CALL_FAIL_RESOURCES_UNAVAILABLE_OR_UNSPECIFIED;
    case LastCallFailCause_V1_0::QOS_UNAVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_QOS_UNAVAILABLE;
    case LastCallFailCause_V1_0::REQUESTED_FACILITY_NOT_SUBSCRIBED:
      return nsILastCallFailCause::CALL_FAIL_REQUESTED_FACILITY_NOT_SUBSCRIBED;
    case LastCallFailCause_V1_0::INCOMING_CALLS_BARRED_WITHIN_CUG:
      return nsILastCallFailCause::CALL_FAIL_INCOMING_CALLS_BARRED_WITHIN_CUG;
    case LastCallFailCause_V1_0::BEARER_CAPABILITY_NOT_AUTHORIZED:
      return nsILastCallFailCause::CALL_FAIL_BEARER_CAPABILITY_NOT_AUTHORIZED;
    case LastCallFailCause_V1_0::BEARER_CAPABILITY_UNAVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_BEARER_CAPABILITY_UNAVAILABLE;
    case LastCallFailCause_V1_0::SERVICE_OPTION_NOT_AVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_SERVICE_OPTION_NOT_AVAILABLE;
    case LastCallFailCause_V1_0::BEARER_SERVICE_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_BEARER_SERVICE_NOT_IMPLEMENTED;
    case LastCallFailCause_V1_0::ACM_LIMIT_EXCEEDED:
      return nsILastCallFailCause::CALL_FAIL_ACM_LIMIT_EXCEEDED;
    case LastCallFailCause_V1_0::REQUESTED_FACILITY_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_REQUESTED_FACILITY_NOT_IMPLEMENTED;
    case LastCallFailCause_V1_0::ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE:
      return nsILastCallFailCause::
        CALL_FAIL_ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE;
    case LastCallFailCause_V1_0::SERVICE_OR_OPTION_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_SERVICE_OR_OPTION_NOT_IMPLEMENTED;
    case LastCallFailCause_V1_0::INVALID_TRANSACTION_IDENTIFIER:
      return nsILastCallFailCause::CALL_FAIL_INVALID_TRANSACTION_IDENTIFIER;
    case LastCallFailCause_V1_0::USER_NOT_MEMBER_OF_CUG:
      return nsILastCallFailCause::CALL_FAIL_USER_NOT_MEMBER_OF_CUG;
    case LastCallFailCause_V1_0::INCOMPATIBLE_DESTINATION:
      return nsILastCallFailCause::CALL_FAIL_INCOMPATIBLE_DESTINATION;
    case LastCallFailCause_V1_0::INVALID_TRANSIT_NW_SELECTION:
      return nsILastCallFailCause::CALL_FAIL_INVALID_TRANSIT_NW_SELECTION;
    case LastCallFailCause_V1_0::SEMANTICALLY_INCORRECT_MESSAGE:
      return nsILastCallFailCause::CALL_FAIL_SEMANTICALLY_INCORRECT_MESSAGE;
    case LastCallFailCause_V1_0::INVALID_MANDATORY_INFORMATION:
      return nsILastCallFailCause::CALL_FAIL_INVALID_MANDATORY_INFORMATION;
    case LastCallFailCause_V1_0::MESSAGE_TYPE_NON_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_MESSAGE_TYPE_NON_IMPLEMENTED;
    case LastCallFailCause_V1_0::
      MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
      return nsILastCallFailCause::
        CALL_FAIL_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
    case LastCallFailCause_V1_0::INFORMATION_ELEMENT_NON_EXISTENT:
      return nsILastCallFailCause::CALL_FAIL_INFORMATION_ELEMENT_NON_EXISTENT;
    case LastCallFailCause_V1_0::CONDITIONAL_IE_ERROR:
      return nsILastCallFailCause::CALL_FAIL_CONDITIONAL_IE_ERROR;
    case LastCallFailCause_V1_0::MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
      return nsILastCallFailCause::
        CALL_FAIL_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
    case LastCallFailCause_V1_0::RECOVERY_ON_TIMER_EXPIRED:
      return nsILastCallFailCause::CALL_FAIL_RECOVERY_ON_TIMER_EXPIRED;
    case LastCallFailCause_V1_0::PROTOCOL_ERROR_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_PROTOCOL_ERROR_UNSPECIFIED;
    case LastCallFailCause_V1_0::INTERWORKING_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_INTERWORKING_UNSPECIFIED;
    case LastCallFailCause_V1_0::CALL_BARRED:
      return nsILastCallFailCause::CALL_FAIL_CALL_BARRED;
    case LastCallFailCause_V1_0::FDN_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_FDN_BLOCKED;
    case LastCallFailCause_V1_0::IMSI_UNKNOWN_IN_VLR:
      return nsILastCallFailCause::CALL_FAIL_IMSI_UNKNOWN_IN_VLR;
    case LastCallFailCause_V1_0::IMEI_NOT_ACCEPTED:
      return nsILastCallFailCause::CALL_FAIL_IMEI_NOT_ACCEPTED;
    case LastCallFailCause_V1_0::DIAL_MODIFIED_TO_USSD:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_USSD;
    case LastCallFailCause_V1_0::DIAL_MODIFIED_TO_SS:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_SS;
    case LastCallFailCause_V1_0::DIAL_MODIFIED_TO_DIAL:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_DIAL;
    case LastCallFailCause_V1_0::RADIO_OFF:
      return nsILastCallFailCause::CALL_FAIL_RADIO_OFF;
    case LastCallFailCause_V1_0::OUT_OF_SERVICE:
      return nsILastCallFailCause::CALL_FAIL_OUT_OF_SERVICE;
    case LastCallFailCause_V1_0::NO_VALID_SIM:
      return nsILastCallFailCause::CALL_FAIL_NO_VALID_SIM;
    case LastCallFailCause_V1_0::RADIO_INTERNAL_ERROR:
      return nsILastCallFailCause::CALL_FAIL_RADIO_INTERNAL_ERROR;
    case LastCallFailCause_V1_0::NETWORK_RESP_TIMEOUT:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_RESP_TIMEOUT;
    case LastCallFailCause_V1_0::NETWORK_REJECT:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_REJECT;
    case LastCallFailCause_V1_0::RADIO_ACCESS_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_ACCESS_FAILURE;
    case LastCallFailCause_V1_0::RADIO_LINK_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_LINK_FAILURE;
    case LastCallFailCause_V1_0::RADIO_LINK_LOST:
      return nsILastCallFailCause::CALL_FAIL_RADIO_LINK_LOST;
    case LastCallFailCause_V1_0::RADIO_UPLINK_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_UPLINK_FAILURE;
    case LastCallFailCause_V1_0::RADIO_SETUP_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_SETUP_FAILURE;
    case LastCallFailCause_V1_0::RADIO_RELEASE_NORMAL:
      return nsILastCallFailCause::CALL_FAIL_RADIO_RELEASE_NORMAL;
    case LastCallFailCause_V1_0::RADIO_RELEASE_ABNORMAL:
      return nsILastCallFailCause::CALL_FAIL_RADIO_RELEASE_ABNORMAL;
    case LastCallFailCause_V1_0::ACCESS_CLASS_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_ACCESS_CLASS_BLOCKED;
    case LastCallFailCause_V1_0::NETWORK_DETACH:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_DETACH;
    case LastCallFailCause_V1_0::CDMA_LOCKED_UNTIL_POWER_CYCLE:
      return nsILastCallFailCause::CALL_FAIL_CDMA_LOCKED_UNTIL_POWER_CYCLE;
    case LastCallFailCause_V1_0::CDMA_DROP:
      return nsILastCallFailCause::CALL_FAIL_CDMA_DROP;
    case LastCallFailCause_V1_0::CDMA_INTERCEPT:
      return nsILastCallFailCause::CALL_FAIL_CDMA_INTERCEPT;
    case LastCallFailCause_V1_0::CDMA_REORDER:
      return nsILastCallFailCause::CALL_FAIL_CDMA_REORDER;
    case LastCallFailCause_V1_0::CDMA_SO_REJECT:
      return nsILastCallFailCause::CALL_FAIL_CDMA_SO_REJECT;
    case LastCallFailCause_V1_0::CDMA_RETRY_ORDER:
      return nsILastCallFailCause::CALL_FAIL_CDMA_RETRY_ORDER;
    case LastCallFailCause_V1_0::CDMA_ACCESS_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_CDMA_ACCESS_FAILURE;
    case LastCallFailCause_V1_0::CDMA_PREEMPTED:
      return nsILastCallFailCause::CALL_FAIL_CDMA_PREEMPTED;
    case LastCallFailCause_V1_0::CDMA_NOT_EMERGENCY:
      return nsILastCallFailCause::CALL_FAIL_CDMA_NOT_EMERGENCY;
    case LastCallFailCause_V1_0::CDMA_ACCESS_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_CDMA_ACCESS_BLOCKED;
    default:
      return nsILastCallFailCause::CALL_FAIL_ERROR_UNSPECIFIED;
  }
}

int32_t
nsRilResponse::convertAppType(AppType_V1_0 type)
{
  switch (type) {
    case AppType_V1_0::UNKNOWN:
      return nsIRilResponseResult::CARD_APPTYPE_UNKNOWN;
    case AppType_V1_0::SIM:
      return nsIRilResponseResult::CARD_APPTYPE_SIM;
    case AppType_V1_0::USIM:
      return nsIRilResponseResult::CARD_APPTYPE_USIM;
    case AppType_V1_0::RUIM:
      return nsIRilResponseResult::CARD_APPTYPE_RUIM;
    case AppType_V1_0::CSIM:
      return nsIRilResponseResult::CARD_APPTYPE_CSIM;
    case AppType_V1_0::ISIM:
      return nsIRilResponseResult::CARD_APPTYPE_ISIM;
    default:
      return nsIRilResponseResult::CARD_APPTYPE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertAppState(AppState_V1_0 state)
{
  switch (state) {
    case AppState_V1_0::UNKNOWN:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
    case AppState_V1_0::DETECTED:
      return nsIRilResponseResult::CARD_APPSTATE_DETECTED;
    case AppState_V1_0::PIN:
      return nsIRilResponseResult::CARD_APPSTATE_PIN;
    case AppState_V1_0::PUK:
      return nsIRilResponseResult::CARD_APPSTATE_PUK;
    case AppState_V1_0::SUBSCRIPTION_PERSO:
      return nsIRilResponseResult::CARD_APPSTATE_SUBSCRIPTION_PERSO;
    case AppState_V1_0::READY:
      return nsIRilResponseResult::CARD_APPSTATE_READY;
    default:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertPersoSubstate(PersoSubstate_V1_0 state)
{
  switch (state) {
    case PersoSubstate_V1_0::UNKNOWN:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_UNKNOWN;
    case PersoSubstate_V1_0::IN_PROGRESS:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_IN_PROGRESS;
    case PersoSubstate_V1_0::READY:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_READY;
    case PersoSubstate_V1_0::SIM_NETWORK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK;
    case PersoSubstate_V1_0::SIM_NETWORK_SUBSET:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET;
    case PersoSubstate_V1_0::SIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE;
    case PersoSubstate_V1_0::SIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER;
    case PersoSubstate_V1_0::SIM_SIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM;
    case PersoSubstate_V1_0::SIM_NETWORK_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_PUK;
    case PersoSubstate_V1_0::SIM_NETWORK_SUBSET_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK;
    case PersoSubstate_V1_0::SIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK;
    case PersoSubstate_V1_0::SIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate_V1_0::SIM_SIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM_PUK;
    case PersoSubstate_V1_0::RUIM_NETWORK1:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1;
    case PersoSubstate_V1_0::RUIM_NETWORK2:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2;
    case PersoSubstate_V1_0::RUIM_HRPD:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD;
    case PersoSubstate_V1_0::RUIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE;
    case PersoSubstate_V1_0::RUIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER;
    case PersoSubstate_V1_0::RUIM_RUIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM;
    case PersoSubstate_V1_0::RUIM_NETWORK1_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK;
    case PersoSubstate_V1_0::RUIM_NETWORK2_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK;
    case PersoSubstate_V1_0::RUIM_HRPD_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD_PUK;
    case PersoSubstate_V1_0::RUIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK;
    case PersoSubstate_V1_0::RUIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate_V1_0::RUIM_RUIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM_PUK;
    default:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
  }
}

#if ANDROID_VERSION >= 33
int32_t
nsRilResponse::convertPersoSubstate(PersoSubstate_V1_5 state)
{
  switch (state) {
    case PersoSubstate_V1_5::UNKNOWN:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_UNKNOWN;
    case PersoSubstate_V1_5::IN_PROGRESS:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_IN_PROGRESS;
    case PersoSubstate_V1_5::READY:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_READY;
    case PersoSubstate_V1_5::SIM_NETWORK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK;
    case PersoSubstate_V1_5::SIM_NETWORK_SUBSET:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET;
    case PersoSubstate_V1_5::SIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE;
    case PersoSubstate_V1_5::SIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER;
    case PersoSubstate_V1_5::SIM_SIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM;
    case PersoSubstate_V1_5::SIM_NETWORK_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_PUK;
    case PersoSubstate_V1_5::SIM_NETWORK_SUBSET_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK;
    case PersoSubstate_V1_5::SIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK;
    case PersoSubstate_V1_5::SIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate_V1_5::SIM_SIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM_PUK;
    case PersoSubstate_V1_5::RUIM_NETWORK1:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1;
    case PersoSubstate_V1_5::RUIM_NETWORK2:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2;
    case PersoSubstate_V1_5::RUIM_HRPD:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD;
    case PersoSubstate_V1_5::RUIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE;
    case PersoSubstate_V1_5::RUIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER;
    case PersoSubstate_V1_5::RUIM_RUIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM;
    case PersoSubstate_V1_5::RUIM_NETWORK1_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK;
    case PersoSubstate_V1_5::RUIM_NETWORK2_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK;
    case PersoSubstate_V1_5::RUIM_HRPD_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD_PUK;
    case PersoSubstate_V1_5::RUIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK;
    case PersoSubstate_V1_5::RUIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate_V1_5::RUIM_RUIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM_PUK;
    case PersoSubstate_V1_5::SIM_SPN:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SPN;
    case PersoSubstate_V1_5::SIM_SPN_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SPN_PUK;
    case PersoSubstate_V1_5::SIM_SP_EHPLMN:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SP_EHPLMN;
    case PersoSubstate_V1_5::SIM_SP_EHPLMN_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SP_EHPLMN_PUK;
    case PersoSubstate_V1_5::SIM_ICCID:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_ICCID;
    case PersoSubstate_V1_5::SIM_ICCID_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_ICCID_PUK;
    case PersoSubstate_V1_5::SIM_IMPI:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_IMPI;
    case PersoSubstate_V1_5::SIM_IMPI_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_IMPI_PUK;
    case PersoSubstate_V1_5::SIM_NS_SP:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NS_SP;
    case PersoSubstate_V1_5::SIM_NS_SP_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NS_SP_PUK;
    default:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
  }
}
#endif

int32_t
nsRilResponse::convertPinState(PinState_V1_0 state)
{
  switch (state) {
    case PinState_V1_0::UNKNOWN:
      return nsIRilResponseResult::CARD_PIN_STATE_UNKNOWN;
    case PinState_V1_0::ENABLED_NOT_VERIFIED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_NOT_VERIFIED;
    case PinState_V1_0::ENABLED_VERIFIED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_VERIFIED;
    case PinState_V1_0::DISABLED:
      return nsIRilResponseResult::CARD_PIN_STATE_DISABLED;
    case PinState_V1_0::ENABLED_BLOCKED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_BLOCKED;
    case PinState_V1_0::ENABLED_PERM_BLOCKED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_PERM_BLOCKED;
    default:
      return nsIRilResponseResult::CARD_PIN_STATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertCardState(CardState_V1_0 state)
{
  switch (state) {
    case CardState_V1_0::ABSENT:
      return nsIRilResponseResult::CARD_STATE_ABSENT;
    case CardState_V1_0::PRESENT:
      return nsIRilResponseResult::CARD_STATE_PRESENT;
    case CardState_V1_0::ERROR:
      return nsIRilResponseResult::CARD_STATE_ERROR;
    case CardState_V1_0::RESTRICTED:
      return nsIRilResponseResult::CARD_STATE_RESTRICTED;
    default:
      return nsIRilResponseResult::CARD_STATE_ERROR;
  }
}

int32_t
nsRilResponse::convertRegState(RegState_V1_0 state)
{
  switch (state) {
    case RegState_V1_0::NOT_REG_MT_NOT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP;
    case RegState_V1_0::REG_HOME:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_HOME;
    case RegState_V1_0::NOT_REG_MT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP;
    case RegState_V1_0::REG_DENIED:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED;
    case RegState_V1_0::UNKNOWN:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
    case RegState_V1_0::REG_ROAMING:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_ROAMING;
    case RegState_V1_0::NOT_REG_MT_NOT_SEARCHING_OP_EM:
      return nsIRilResponseResult::
        RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP_EM;
    case RegState_V1_0::NOT_REG_MT_SEARCHING_OP_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP_EM;
    case RegState_V1_0::REG_DENIED_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED_EM;
    case RegState_V1_0::UNKNOWN_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN_EM;
    default:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertUusType(UusType_V1_0 type)
{
  switch (type) {
    case UusType_V1_0::TYPE1_IMPLICIT:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
    case UusType_V1_0::TYPE1_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_REQUIRED;
    case UusType_V1_0::TYPE1_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_NOT_REQUIRED;
    case UusType_V1_0::TYPE2_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_REQUIRED;
    case UusType_V1_0::TYPE2_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_NOT_REQUIRED;
    case UusType_V1_0::TYPE3_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_REQUIRED;
    case UusType_V1_0::TYPE3_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_NOT_REQUIRED;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
  }
}

int32_t
nsRilResponse::convertUusDcs(UusDcs_V1_0 dcs)
{
  switch (dcs) {
    case UusDcs_V1_0::USP:
      return nsIRilResponseResult::CALL_UUSDCS_USP;
    case UusDcs_V1_0::OSIHLP:
      return nsIRilResponseResult::CALL_UUSDCS_OSIHLP;
    case UusDcs_V1_0::X244:
      return nsIRilResponseResult::CALL_UUSDCS_X244;
    case UusDcs_V1_0::RMCF:
      return nsIRilResponseResult::CALL_UUSDCS_RMCF;
    case UusDcs_V1_0::IA5C:
      return nsIRilResponseResult::CALL_UUSDCS_IA5C;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSDCS_USP;
  }
}

int32_t
nsRilResponse::convertCallPresentation(CallPresentation_V1_0 state)
{
  switch (state) {
    case CallPresentation_V1_0::ALLOWED:
      return nsIRilResponseResult::CALL_PRESENTATION_ALLOWED;
    case CallPresentation_V1_0::RESTRICTED:
      return nsIRilResponseResult::CALL_PRESENTATION_RESTRICTED;
    case CallPresentation_V1_0::UNKNOWN:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
    case CallPresentation_V1_0::PAYPHONE:
      return nsIRilResponseResult::CALL_PRESENTATION_PAYPHONE;
    default:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertCallState(CallState_V1_0 state)
{
  switch (state) {
    case CallState_V1_0::ACTIVE:
      return nsIRilResponseResult::CALL_STATE_ACTIVE;
    case CallState_V1_0::HOLDING:
      return nsIRilResponseResult::CALL_STATE_HOLDING;
    case CallState_V1_0::DIALING:
      return nsIRilResponseResult::CALL_STATE_DIALING;
    case CallState_V1_0::ALERTING:
      return nsIRilResponseResult::CALL_STATE_ALERTING;
    case CallState_V1_0::INCOMING:
      return nsIRilResponseResult::CALL_STATE_INCOMING;
    case CallState_V1_0::WAITING:
      return nsIRilResponseResult::CALL_STATE_WAITING;
    default:
      return nsIRilResponseResult::CALL_STATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertPreferredNetworkType(PreferredNetworkType_V1_0 type)
{
  switch (type) {
    case PreferredNetworkType_V1_0::GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_WCDMA;
    case PreferredNetworkType_V1_0::GSM_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_ONLY;
    case PreferredNetworkType_V1_0::WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_WCDMA;
    case PreferredNetworkType_V1_0::GSM_WCDMA_AUTO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_WCDMA_AUTO;
    case PreferredNetworkType_V1_0::CDMA_EVDO_AUTO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_CDMA_EVDO_AUTO;
    case PreferredNetworkType_V1_0::CDMA_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_CDMA_ONLY;
    case PreferredNetworkType_V1_0::EVDO_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_EVDO_ONLY;
    case PreferredNetworkType_V1_0::GSM_WCDMA_CDMA_EVDO_AUTO:
      return nsIRilResponseResult::
        PREFERRED_NETWORK_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO;
    case PreferredNetworkType_V1_0::LTE_CDMA_EVDO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_CDMA_EVDO;
    case PreferredNetworkType_V1_0::LTE_GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_GSM_WCDMA;
    case PreferredNetworkType_V1_0::LTE_CMDA_EVDO_GSM_WCDMA:
      return nsIRilResponseResult::
        PREFERRED_NETWORK_TYPE_LTE_CMDA_EVDO_GSM_WCDMA;
    case PreferredNetworkType_V1_0::LTE_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_ONLY;
    case PreferredNetworkType_V1_0::LTE_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_WCDMA;
    case PreferredNetworkType_V1_0::TD_SCDMA_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_ONLY;
    case PreferredNetworkType_V1_0::TD_SCDMA_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_WCDMA;
    case PreferredNetworkType_V1_0::TD_SCDMA_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_LTE;
    case PreferredNetworkType_V1_0::TD_SCDMA_GSM:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM;
    case PreferredNetworkType_V1_0::TD_SCDMA_GSM_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_LTE;
    case PreferredNetworkType_V1_0::TD_SCDMA_GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA;
    case PreferredNetworkType_V1_0::TD_SCDMA_WCDMA_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_WCDMA_LTE;
    case PreferredNetworkType_V1_0::TD_SCDMA_GSM_WCDMA_LTE:
      return nsIRilResponseResult::
        PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA_LTE;
    case PreferredNetworkType_V1_0::TD_SCDMA_GSM_WCDMA_CDMA_EVDO_AUTO:
      return nsIRilResponseResult::
        PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA_CDMA_EVDO_AUTO;
    case PreferredNetworkType_V1_0::TD_SCDMA_LTE_CDMA_EVDO_GSM_WCDMA:
      return nsIRilResponseResult::
        PREFERRED_NETWORK_TYPE_TD_SCDMA_LTE_CDMA_EVDO_GSM_WCDMA;
    default:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_GSM_WCDMA;
  }
}

int32_t
nsRilResponse::convertOperatorState(OperatorStatus_V1_0 status)
{
  switch (status) {
    case OperatorStatus_V1_0::UNKNOWN:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
    case OperatorStatus_V1_0::AVAILABLE:
      return nsIRilResponseResult::QAN_STATE_AVAILABLE;
    case OperatorStatus_V1_0::CURRENT:
      return nsIRilResponseResult::QAN_STATE_CURRENT;
    case OperatorStatus_V1_0::FORBIDDEN:
      return nsIRilResponseResult::QAN_STATE_FORBIDDEN;
    default:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertCallForwardState(CallForwardInfoStatus_V1_0 status)
{
  switch (status) {
    case CallForwardInfoStatus_V1_0::DISABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
    case CallForwardInfoStatus_V1_0::ENABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ENABLE;
    case CallForwardInfoStatus_V1_0::INTERROGATE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_INTERROGATE;
    case CallForwardInfoStatus_V1_0::REGISTRATION:
      return nsIRilResponseResult::CALL_FORWARD_STATE_REGISTRATION;
    case CallForwardInfoStatus_V1_0::ERASURE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ERASURE;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
  }
}

int32_t
nsRilResponse::convertClipState(ClipStatus_V1_0 status)
{
  switch (status) {
    case ClipStatus_V1_0::CLIP_PROVISIONED:
      return nsIRilResponseResult::CLIP_STATE_PROVISIONED;
    case ClipStatus_V1_0::CLIP_UNPROVISIONED:
      return nsIRilResponseResult::CLIP_STATE_UNPROVISIONED;
    case ClipStatus_V1_0::UNKNOWN:
      return nsIRilResponseResult::CLIP_STATE_UNKNOWN;
    default:
      return nsIRilResponseResult::CLIP_STATE_UNKNOWN;
  }
}

int32_t
nsRilResponse::convertTtyMode(TtyMode_V1_0 mode)
{
  switch (mode) {
    case TtyMode_V1_0::OFF:
      return nsIRilResponseResult::TTY_MODE_OFF;
    case TtyMode_V1_0::FULL:
      return nsIRilResponseResult::TTY_MODE_FULL;
    case TtyMode_V1_0::HCO:
      return nsIRilResponseResult::TTY_MODE_HCO;
    case TtyMode_V1_0::VCO:
      return nsIRilResponseResult::TTY_MODE_VCO;
    default:
      return nsIRilResponseResult::TTY_MODE_OFF;
  }
}
