/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilResponse.h"
#include "nsRilWorker.h"

/* Logging related */
#undef LOG_TAG
#define LOG_TAG "nsRilResponse"

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ##args)
#define ERROR(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ##args)
#define DEBUG(args...)                                         \
  do {                                                         \
    if (gRilDebug_isLoggingEnabled) {                          \
      __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ##args); \
    }                                                          \
  } while (0)

CardStatus cardStatus;

/**
 *
 */
nsRilResponse::nsRilResponse(nsRilWorker* aRil) {
  DEBUG("init nsRilResponse");
  mRIL = aRil;
}

nsRilResponse::~nsRilResponse() {
  DEBUG("~nsRilResponse");
  mRIL = nullptr;
  MOZ_ASSERT(!mRIL);
}

Return<void> nsRilResponse::getIccCardStatusResponse(
    const RadioResponseInfo& info, const CardStatus& card_status) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getICCStatus"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
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
          NS_ConvertUTF8toUTF16(
              card_status.applications[i].appLabelPtr.c_str()),
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
                         card_status.imsSubscriptionAppIndex, applications);
    result->updateIccCardStatus(cardStatus);
  } else {
    DEBUG("getICCStatus error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::supplyIccPinForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"enterICCPIN"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPinForAppResponse error = %d , retries = %d", rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::supplyIccPukForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"enterICCPUK"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPukForAppResponse error = %d , retries = %d", rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::supplyIccPin2ForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"enterICCPIN2"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error, remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::supplyIccPuk2ForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"enterICCPUK2"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("supplyIccPuk2ForAppResponse error = %d , retries = %d",
          rspInfo.error, remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::changeIccPinForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"changeICCPIN"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("changeIccPinForAppResponse error = %d , retries = %d", rspInfo.error,
          remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::changeIccPin2ForAppResponse(
    const RadioResponseInfo& info, int32_t remainingRetries) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"changeICCPIN2"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("changeIccPin2ForAppResponse error = %d , retries = %d",
          rspInfo.error, remainingRetries);
    result->updateRemainRetries(remainingRetries);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::supplyNetworkDepersonalizationResponse(
    const RadioResponseInfo& info, int32_t /*remainingRetries*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getCurrentCallsResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<Call>& calls) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getCurrentCalls"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
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
            convertUusType(calls[i].uusInfo[j].uusType),
            convertUusDcs(calls[i].uusInfo[j].uusDcs),
            NS_ConvertUTF8toUTF16(calls[i].uusInfo[j].uusData.c_str()));

        aUusInfos.AppendElement(uusinfo);
      }

      DEBUG("getCurrentCalls index= %d  state=%d", calls[i].index,
            convertCallState(calls[i].state));
      RefPtr<nsCall> call = new nsCall(
          convertCallState(calls[i].state), calls[i].index, calls[i].toa,
          calls[i].isMpty, calls[i].isMT, int32_t(calls[i].als),
          calls[i].isVoice, calls[i].isVoicePrivacy,
          NS_ConvertUTF8toUTF16(calls[i].number.c_str()),
          convertCallPresentation(calls[i].numberPresentation),
          NS_ConvertUTF8toUTF16(calls[i].name.c_str()),
          convertCallPresentation(calls[i].namePresentation), aUusInfos);
      aCalls.AppendElement(call);
    }
    result->updateCurrentCalls(aCalls);
  } else {
    DEBUG("getCurrentCalls error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::dialResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"dial"_ns);
  return Void();
}

Return<void> nsRilResponse::getIMSIForAppResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& imsi) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"getIMSI"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateIMSI(NS_ConvertUTF8toUTF16(imsi.c_str()));
  } else {
    DEBUG("getIMSIForAppResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::hangupConnectionResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpCall"_ns);
  return Void();
}

Return<void> nsRilResponse::hangupWaitingOrBackgroundResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpBackground"_ns);
  return Void();
}

Return<void> nsRilResponse::hangupForegroundResumeBackgroundResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"hangUpForeground"_ns);
  return Void();
}

Return<void> nsRilResponse::switchWaitingOrHoldingAndActiveResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"switchActiveCall"_ns);
  return Void();
}

Return<void> nsRilResponse::conferenceResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"conferenceCall"_ns);
  return Void();
}

Return<void> nsRilResponse::rejectCallResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"udub"_ns);
  return Void();
}

Return<void> nsRilResponse::getLastCallFailCauseResponse(
    const RadioResponseInfo& info, const LastCallFailCauseInfo& failCauseInfo) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getFailCause"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateFailCause(
        covertLastCallFailCause(failCauseInfo.causeCode),
        NS_ConvertUTF8toUTF16(failCauseInfo.vendorCause.c_str()));
  } else {
    DEBUG("getLastCallFailCauseResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::getSignalStrengthResponse(
    const RadioResponseInfo& info, const SignalStrength& sig_strength) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getSignalStrength"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSignalStrength> signalStrength =
        result->convertSignalStrength(sig_strength);
    result->updateSignalStrength(signalStrength);
  } else {
    DEBUG("getSignalStrength error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::getVoiceRegistrationStateResponse(
    const RadioResponseInfo& info,
    const VoiceRegStateResult& voiceRegResponse) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getVoiceRegistrationState"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("getVoiceRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
        result->convertCellIdentity(voiceRegResponse.cellIdentity);
    RefPtr<nsVoiceRegState> voiceRegState = new nsVoiceRegState(
        convertRegState(voiceRegResponse.regState), voiceRegResponse.rat,
        voiceRegResponse.cssSupported, voiceRegResponse.roamingIndicator,
        voiceRegResponse.systemIsInPrl,
        voiceRegResponse.defaultRoamingIndicator,
        voiceRegResponse.reasonForDenial, cellIdentity);
    result->updateVoiceRegStatus(voiceRegState);
  } else {
    DEBUG("getVoiceRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::getDataRegistrationStateResponse(
    const RadioResponseInfo& info, const DataRegStateResult& dataRegResponse) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getDataRegistrationState"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    DEBUG("getDataRegistrationState success.");
    RefPtr<nsCellIdentity> cellIdentity =
        result->convertCellIdentity(dataRegResponse.cellIdentity);
    RefPtr<nsDataRegState> dataRegState = new nsDataRegState(
        convertRegState(dataRegResponse.regState), dataRegResponse.rat,
        dataRegResponse.reasonDataDenied, dataRegResponse.maxDataCalls,
        cellIdentity);
    result->updateDataRegStatus(dataRegState);
  } else {
    DEBUG("getDataRegistrationState error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::getOperatorResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& longName,
    const ::android::hardware::hidl_string& shortName,
    const ::android::hardware::hidl_string& numeric) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"getOperator"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsOperatorInfo> operatorInfo =
        new nsOperatorInfo(NS_ConvertUTF8toUTF16(longName.c_str()),
                           NS_ConvertUTF8toUTF16(shortName.c_str()),
                           NS_ConvertUTF8toUTF16(numeric.c_str()), 0);
    result->updateOperator(operatorInfo);
  } else {
    DEBUG("getOperator error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setRadioPowerResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setRadioEnabled"_ns);
  return Void();
}

Return<void> nsRilResponse::sendDtmfResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendTone"_ns);
  return Void();
}

Return<void> nsRilResponse::sendSmsResponse(const RadioResponseInfo& info,
                                            const SendSmsResult& sms) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"sendSMS"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSendSmsResult> smsResult = new nsSendSmsResult(
        sms.messageRef, NS_ConvertUTF8toUTF16(sms.ackPDU.c_str()),
        sms.errorCode);
    result->updateSendSmsResponse(smsResult);
  } else {
    DEBUG("sendSMS error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::sendSMSExpectMoreResponse(
    const RadioResponseInfo& info, const SendSmsResult& sms) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // sendSmsResult = sms;

  return Void();
}

Return<void> nsRilResponse::setupDataCallResponse(
    const RadioResponseInfo& info, const SetupDataCallResult& dcResponse) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"setupDataCall"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsSetupDataCallResult> datacallresponse =
        result->convertDcResponse(dcResponse);
    result->updateDataCallResponse(datacallresponse);
  } else {
    DEBUG("setupDataCall error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::iccIOForAppResponse(const RadioResponseInfo& info,
                                                const IccIoResult& iccIo) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"iccIO"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::sendUssdResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendUSSD"_ns);
  return Void();
}

Return<void> nsRilResponse::cancelPendingUssdResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"cancelUSSD"_ns);
  return Void();
}

Return<void> nsRilResponse::getClirResponse(const RadioResponseInfo& info,
                                            int32_t n, int32_t m) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"getCLIR"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateClir(n, m);
  } else {
    DEBUG("getClirResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::setClirResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCLIR"_ns);
  return Void();
}

Return<void> nsRilResponse::getCallForwardStatusResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<CallForwardInfo>& callForwardInfos) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"queryCallForwardStatus"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numCallForwardInfo = callForwardInfos.size();
    nsTArray<RefPtr<nsCallForwardInfo>> aCallForwardInfoLists(
        numCallForwardInfo);

    for (uint32_t i = 0; i < numCallForwardInfo; i++) {
      RefPtr<nsCallForwardInfo> callForwardInfo = new nsCallForwardInfo(
          convertCallForwardState(callForwardInfos[i].status),
          callForwardInfos[i].reason, callForwardInfos[i].serviceClass,
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

Return<void> nsRilResponse::setCallForwardResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCallForward"_ns);
  return Void();
}

Return<void> nsRilResponse::getCallWaitingResponse(
    const RadioResponseInfo& info, bool enable, int32_t serviceClass) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"queryCallWaiting"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateCallWaiting(enable, serviceClass);
  } else {
    DEBUG("getCallWaitingResponse error.");
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::setCallWaitingResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCallWaiting"_ns);
  return Void();
}

Return<void> nsRilResponse::acknowledgeLastIncomingGsmSmsResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"ackSMS"_ns);
  return Void();
}

Return<void> nsRilResponse::acceptCallResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"answerCall"_ns);
  return Void();
}

Return<void> nsRilResponse::deactivateDataCallResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"deactivateDataCall"_ns);
  return Void();
}

Return<void> nsRilResponse::getFacilityLockForAppResponse(
    const RadioResponseInfo& info, int32_t response) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"queryICCFacilityLock"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateServiceClass(response);
  } else {
    DEBUG("setFacilityLockForAppResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setFacilityLockForAppResponse(
    const RadioResponseInfo& info, int32_t retry) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"setICCFacilityLock"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error != RadioError::NONE) {
    DEBUG("setFacilityLockForAppResponse error = %d , retries = %d",
          rspInfo.error, retry);
    result->updateRemainRetries(retry);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setBarringPasswordResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"changeCallBarringPassword"_ns);
  return Void();
}

Return<void> nsRilResponse::getNetworkSelectionModeResponse(
    const RadioResponseInfo& info, bool manual) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getNetworkSelectionMode"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  result->updateNetworkSelectionMode(manual);
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::setNetworkSelectionModeAutomaticResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"selectNetworkAuto"_ns);
  return Void();
}

Return<void> nsRilResponse::setNetworkSelectionModeManualResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"selectNetwork"_ns);
  return Void();
}

Return<void> nsRilResponse::getAvailableNetworksResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<OperatorInfo>& networkInfos) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getAvailableNetworks"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
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

Return<void> nsRilResponse::startDtmfResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"startTone"_ns);
  return Void();
}

Return<void> nsRilResponse::stopDtmfResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stopTone"_ns);
  return Void();
}

Return<void> nsRilResponse::getBasebandVersionResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& version) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getBasebandVersion"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateBasebandVersion(NS_ConvertUTF8toUTF16(version.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::separateConnectionResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"separateCall"_ns);
  return Void();
}

Return<void> nsRilResponse::setMuteResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setMute"_ns);
  return Void();
}

Return<void> nsRilResponse::getMuteResponse(const RadioResponseInfo& info,
                                            bool enable) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"getMute"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateMute(enable);
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::getClipResponse(const RadioResponseInfo& info,
                                            ClipStatus status) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      u"queryCLIP"_ns, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateClip(convertClipState(status));
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::getDataCallListResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<SetupDataCallResult>& dcResponse) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getDataCallList"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
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

Return<void> nsRilResponse::sendOemRilRequestRawResponse(
    const RadioResponseInfo& /*info*/,
    const ::android::hardware::hidl_vec<uint8_t>& /*data*/) {
  return Void();
}

Return<void> nsRilResponse::sendOemRilRequestStringsResponse(
    const RadioResponseInfo& /*info*/,
    const ::android::hardware::hidl_vec<
        ::android::hardware::hidl_string>& /*data*/) {
  return Void();
}

Return<void> nsRilResponse::setSuppServiceNotificationsResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setSuppServiceNotifications"_ns);
  return Void();
}

Return<void> nsRilResponse::writeSmsToSimResponse(const RadioResponseInfo& info,
                                                  int32_t index) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // writeSmsToSimIndex = index;

  return Void();
}

Return<void> nsRilResponse::deleteSmsOnSimResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setBandModeResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getAvailableBandModesResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<RadioBandMode>& /*bandModes*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendEnvelopeResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& /*commandResponse*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendEnvelopeResponse"_ns);
  return Void();
}

Return<void> nsRilResponse::sendTerminalResponseToSimResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendStkTerminalResponse"_ns);
  return Void();
}

Return<void> nsRilResponse::handleStkCallSetupRequestFromSimResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stkHandleCallSetup"_ns);
  return Void();
}

Return<void> nsRilResponse::explicitCallTransferResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"explicitCallTransfer"_ns);
  return Void();
}

Return<void> nsRilResponse::setPreferredNetworkTypeResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setPreferredNetworkType"_ns);
  return Void();
}

Return<void> nsRilResponse::getPreferredNetworkTypeResponse(
    const RadioResponseInfo& info, PreferredNetworkType nw_type) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getPreferredNetworkType"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updatePreferredNetworkType(convertPreferredNetworkType(nw_type));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::getNeighboringCidsResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<NeighboringCell>& cells) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getNeighboringCellIds"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
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

Return<void> nsRilResponse::setLocationUpdatesResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setCdmaSubscriptionSourceResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setCdmaRoamingPreferenceResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getCdmaRoamingPreferenceResponse(
    const RadioResponseInfo& info, CdmaRoamingType /*type*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setTTYModeResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setTtyMode"_ns);
  return Void();
}

Return<void> nsRilResponse::getTTYModeResponse(const RadioResponseInfo& info,
                                               TtyMode mode) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"queryTtyMode"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateTtyMode(convertTtyMode(mode));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::setPreferredVoicePrivacyResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setVoicePrivacyMode"_ns);
  return Void();
}

Return<void> nsRilResponse::getPreferredVoicePrivacyResponse(
    const RadioResponseInfo& info, bool enable) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"queryVoicePrivacyMode"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateVoicePrivacy(enable);
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::sendCDMAFeatureCodeResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendBurstDtmfResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendCdmaSmsResponse(const RadioResponseInfo& info,
                                                const SendSmsResult& sms) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // sendSmsResult = sms;

  return Void();
}

Return<void> nsRilResponse::acknowledgeLastIncomingCdmaSmsResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getGsmBroadcastConfigResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<
        GsmBroadcastSmsConfigInfo>& /*configs*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setGsmBroadcastConfigResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setGsmBroadcastConfig"_ns);
  return Void();
}

Return<void> nsRilResponse::setGsmBroadcastActivationResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setGsmBroadcastActivation"_ns);
  return Void();
}

Return<void> nsRilResponse::getCdmaBroadcastConfigResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<
        CdmaBroadcastSmsConfigInfo>& /*configs*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setCdmaBroadcastConfigResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setCdmaBroadcastActivationResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getCDMASubscriptionResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& /*mdn*/,
    const ::android::hardware::hidl_string& /*hSid*/,
    const ::android::hardware::hidl_string& /*hNid*/,
    const ::android::hardware::hidl_string& /*min*/,
    const ::android::hardware::hidl_string& /*prl*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::writeSmsToRuimResponse(
    const RadioResponseInfo& info, uint32_t index) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // writeSmsToRuimIndex = index;

  return Void();
}

Return<void> nsRilResponse::deleteSmsOnRuimResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getDeviceIdentityResponse(
    const RadioResponseInfo& info, const ::android::hardware::hidl_string& imei,
    const ::android::hardware::hidl_string& imeisv,
    const ::android::hardware::hidl_string& esn,
    const ::android::hardware::hidl_string& meid) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getDeviceIdentity"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateDeviceIdentity(NS_ConvertUTF8toUTF16(imei.c_str()),
                                 NS_ConvertUTF8toUTF16(imeisv.c_str()),
                                 NS_ConvertUTF8toUTF16(esn.c_str()),
                                 NS_ConvertUTF8toUTF16(meid.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::exitEmergencyCallbackModeResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"sendExitEmergencyCbModeRequest"_ns);
  return Void();
}

Return<void> nsRilResponse::getSmscAddressResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& smsc) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getSmscAddress"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateSmscAddress(NS_ConvertUTF8toUTF16(smsc.c_str()));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::setSmscAddressResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::reportSmsMemoryStatusResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::reportStkServiceIsRunningResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"reportStkServiceIsRunning"_ns);
  return Void();
}

Return<void> nsRilResponse::getCdmaSubscriptionSourceResponse(
    const RadioResponseInfo& info, CdmaSubscriptionSource /*source*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::requestIsimAuthenticationResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& /*response*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::acknowledgeIncomingGsmSmsWithPduResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendEnvelopeWithStatusResponse(
    const RadioResponseInfo& info, const IccIoResult& /*iccIo*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getVoiceRadioTechnologyResponse(
    const RadioResponseInfo& info, RadioTechnology rat) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getVoiceRadioTechnology"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    result->updateVoiceRadioTechnology(
        nsRilResult::convertRadioTechnology(rat));
  }
  mRIL->sendRilResponseResult(result);

  return Void();
}

Return<void> nsRilResponse::getCellInfoListResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<CellInfo>& cellInfo) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getCellInfoList"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
  if (rspInfo.error == RadioError::NONE) {
    uint32_t numCellInfo = cellInfo.size();
    nsTArray<RefPtr<nsRilCellInfo>> aCellInfoLists(numCellInfo);

    for (uint32_t i = 0; i < numCellInfo; i++) {
      RefPtr<nsRilCellInfo> cell = result->convertRilCellInfo(cellInfo[i]);
      aCellInfoLists.AppendElement(cell);
    }
    result->updateCellInfoList(aCellInfoLists);
  } else {
    DEBUG("getDataCallListResponse error.");
  }
  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setCellInfoListRateResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setCellInfoListRate"_ns);
  return Void();
}

Return<void> nsRilResponse::setInitialAttachApnResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setInitialAttachApn"_ns);
  return Void();
}

Return<void> nsRilResponse::getImsRegistrationStateResponse(
    const RadioResponseInfo& info, bool /*isRegistered*/,
    RadioTechnologyFamily /*ratFamily*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendImsSmsResponse(const RadioResponseInfo& info,
                                               const SendSmsResult& sms) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);
  // sendSmsResult = sms;

  return Void();
}

Return<void> nsRilResponse::iccTransmitApduBasicChannelResponse(
    const RadioResponseInfo& info, const IccIoResult& result) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::iccOpenLogicalChannelResponse(
    const RadioResponseInfo& info, int32_t channelId,
    const ::android::hardware::hidl_vec<int8_t>& /*selectResponse*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::iccCloseLogicalChannelResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::iccTransmitApduLogicalChannelResponse(
    const RadioResponseInfo& info, const IccIoResult& result) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::nvReadItemResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_string& /*result*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::nvWriteItemResponse(const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::nvWriteCdmaPrlResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::nvResetConfigResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setUiccSubscriptionResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setUiccSubscription"_ns);
  return Void();
}

Return<void> nsRilResponse::setDataAllowedResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setDataRegistration"_ns);
  return Void();
}

Return<void> nsRilResponse::getHardwareConfigResponse(
    const RadioResponseInfo& info,
    const ::android::hardware::hidl_vec<HardwareConfig>& /*config*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::requestIccSimAuthenticationResponse(
    const RadioResponseInfo& info, const IccIoResult& iccIo) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getIccAuthentication"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));
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

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setDataProfileResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setDataProfile"_ns);
  return Void();
}

Return<void> nsRilResponse::requestShutdownResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getRadioCapabilityResponse(
    const RadioResponseInfo& info, const RadioCapability& rc) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  RefPtr<nsRilResponseResult> result =
      new nsRilResponseResult(u"getRadioCapability"_ns, rspInfo.serial,
                              convertRadioErrorToNum(rspInfo.error));

  if (rspInfo.error == RadioError::NONE) {
    RefPtr<nsRadioCapability> radioCapability = new nsRadioCapability(
        rc.session, static_cast<int32_t>(rc.phase), rc.raf,
        NS_ConvertUTF8toUTF16(rc.logicalModemUuid.c_str()),
        static_cast<int32_t>(rc.status));
    result->updateRadioCapability(radioCapability);
  } else {
    DEBUG("getRadioCapability error.");
  }

  mRIL->sendRilResponseResult(result);
  return Void();
}

Return<void> nsRilResponse::setRadioCapabilityResponse(
    const RadioResponseInfo& info, const RadioCapability& /*rc*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::startLceServiceResponse(
    const RadioResponseInfo& info, const LceStatusInfo& /*statusInfo*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::stopLceServiceResponse(
    const RadioResponseInfo& info, const LceStatusInfo& /*statusInfo*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::pullLceDataResponse(
    const RadioResponseInfo& info, const LceDataInfo& /*lceInfo*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getModemActivityInfoResponse(
    const RadioResponseInfo& info, const ActivityStatsInfo& /*activityInfo*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setAllowedCarriersResponse(
    const RadioResponseInfo& info, int32_t /*numAllowed*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::getAllowedCarriersResponse(
    const RadioResponseInfo& info, bool /*allAllowed*/,
    const CarrierRestrictions& /*carriers*/) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::sendDeviceStateResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setIndicationFilterResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"setUnsolResponseFilter"_ns);
  return Void();
}

Return<void> nsRilResponse::setSimCardPowerResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::acknowledgeRequest(int32_t /*serial*/) {
  return Void();
}

Return<void> nsRilResponse::setCarrierInfoForImsiEncryptionResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::setSimCardPowerResponse_1_1(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::startNetworkScanResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::stopNetworkScanResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  defaultResponse(rspInfo, u"stopNetworkScan"_ns);
  return Void();
}

Return<void> nsRilResponse::startKeepaliveResponse(
    const RadioResponseInfo& info, const KeepaliveStatus& status) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

Return<void> nsRilResponse::stopKeepaliveResponse(
    const RadioResponseInfo& info) {
  rspInfo = info;
  mRIL->processResponse(rspInfo.type);

  return Void();
}

// Helper function
void nsRilResponse::defaultResponse(const RadioResponseInfo& rspInfo,
                                    const nsString& rilmessageType) {
  RefPtr<nsRilResponseResult> result = new nsRilResponseResult(
      rilmessageType, rspInfo.serial, convertRadioErrorToNum(rspInfo.error));
  mRIL->sendRilResponseResult(result);
}

int32_t nsRilResponse::convertRadioErrorToNum(RadioError error) {
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

int32_t nsRilResponse::covertLastCallFailCause(LastCallFailCause cause) {
  switch (cause) {
    case LastCallFailCause::UNOBTAINABLE_NUMBER:
      return nsILastCallFailCause::CALL_FAIL_UNOBTAINABLE_NUMBER;
    case LastCallFailCause::NO_ROUTE_TO_DESTINATION:
      return nsILastCallFailCause::CALL_FAIL_NO_ROUTE_TO_DESTINATION;
    case LastCallFailCause::CHANNEL_UNACCEPTABLE:
      return nsILastCallFailCause::CALL_FAIL_CHANNEL_UNACCEPTABLE;
    case LastCallFailCause::OPERATOR_DETERMINED_BARRING:
      return nsILastCallFailCause::CALL_FAIL_OPERATOR_DETERMINED_BARRING;
    case LastCallFailCause::NORMAL:
      return nsILastCallFailCause::CALL_FAIL_NORMAL;
    case LastCallFailCause::BUSY:
      return nsILastCallFailCause::CALL_FAIL_BUSY;
    case LastCallFailCause::NO_USER_RESPONDING:
      return nsILastCallFailCause::CALL_FAIL_NO_USER_RESPONDING;
    case LastCallFailCause::NO_ANSWER_FROM_USER:
      return nsILastCallFailCause::CALL_FAIL_NO_ANSWER_FROM_USER;
    case LastCallFailCause::CALL_REJECTED:
      return nsILastCallFailCause::CALL_FAIL_CALL_REJECTED;
    case LastCallFailCause::NUMBER_CHANGED:
      return nsILastCallFailCause::CALL_FAIL_NUMBER_CHANGED;
    case LastCallFailCause::PREEMPTION:
      return nsILastCallFailCause::CALL_FAIL_PREEMPTION;
    case LastCallFailCause::DESTINATION_OUT_OF_ORDER:
      return nsILastCallFailCause::CALL_FAIL_DESTINATION_OUT_OF_ORDER;
    case LastCallFailCause::INVALID_NUMBER_FORMAT:
      return nsILastCallFailCause::CALL_FAIL_INVALID_NUMBER_FORMAT;
    case LastCallFailCause::FACILITY_REJECTED:
      return nsILastCallFailCause::CALL_FAIL_FACILITY_REJECTED;
    case LastCallFailCause::RESP_TO_STATUS_ENQUIRY:
      return nsILastCallFailCause::CALL_FAIL_RESP_TO_STATUS_ENQUIRY;
    case LastCallFailCause::NORMAL_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_NORMAL_UNSPECIFIED;
    case LastCallFailCause::CONGESTION:
      return nsILastCallFailCause::CALL_FAIL_CONGESTION;
    case LastCallFailCause::NETWORK_OUT_OF_ORDER:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_OUT_OF_ORDER;
    case LastCallFailCause::TEMPORARY_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_TEMPORARY_FAILURE;
    case LastCallFailCause::SWITCHING_EQUIPMENT_CONGESTION:
      return nsILastCallFailCause::CALL_FAIL_SWITCHING_EQUIPMENT_CONGESTION;
    case LastCallFailCause::ACCESS_INFORMATION_DISCARDED:
      return nsILastCallFailCause::CALL_FAIL_ACCESS_INFORMATION_DISCARDED;
    case LastCallFailCause::REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE:
      return nsILastCallFailCause::
          CALL_FAIL_REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE;
    case LastCallFailCause::RESOURCES_UNAVAILABLE_OR_UNSPECIFIED:
      return nsILastCallFailCause::
          CALL_FAIL_RESOURCES_UNAVAILABLE_OR_UNSPECIFIED;
    case LastCallFailCause::QOS_UNAVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_QOS_UNAVAILABLE;
    case LastCallFailCause::REQUESTED_FACILITY_NOT_SUBSCRIBED:
      return nsILastCallFailCause::CALL_FAIL_REQUESTED_FACILITY_NOT_SUBSCRIBED;
    case LastCallFailCause::INCOMING_CALLS_BARRED_WITHIN_CUG:
      return nsILastCallFailCause::CALL_FAIL_INCOMING_CALLS_BARRED_WITHIN_CUG;
    case LastCallFailCause::BEARER_CAPABILITY_NOT_AUTHORIZED:
      return nsILastCallFailCause::CALL_FAIL_BEARER_CAPABILITY_NOT_AUTHORIZED;
    case LastCallFailCause::BEARER_CAPABILITY_UNAVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_BEARER_CAPABILITY_UNAVAILABLE;
    case LastCallFailCause::SERVICE_OPTION_NOT_AVAILABLE:
      return nsILastCallFailCause::CALL_FAIL_SERVICE_OPTION_NOT_AVAILABLE;
    case LastCallFailCause::BEARER_SERVICE_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_BEARER_SERVICE_NOT_IMPLEMENTED;
    case LastCallFailCause::ACM_LIMIT_EXCEEDED:
      return nsILastCallFailCause::CALL_FAIL_ACM_LIMIT_EXCEEDED;
    case LastCallFailCause::REQUESTED_FACILITY_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_REQUESTED_FACILITY_NOT_IMPLEMENTED;
    case LastCallFailCause::ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE:
      return nsILastCallFailCause::
          CALL_FAIL_ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE;
    case LastCallFailCause::SERVICE_OR_OPTION_NOT_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_SERVICE_OR_OPTION_NOT_IMPLEMENTED;
    case LastCallFailCause::INVALID_TRANSACTION_IDENTIFIER:
      return nsILastCallFailCause::CALL_FAIL_INVALID_TRANSACTION_IDENTIFIER;
    case LastCallFailCause::USER_NOT_MEMBER_OF_CUG:
      return nsILastCallFailCause::CALL_FAIL_USER_NOT_MEMBER_OF_CUG;
    case LastCallFailCause::INCOMPATIBLE_DESTINATION:
      return nsILastCallFailCause::CALL_FAIL_INCOMPATIBLE_DESTINATION;
    case LastCallFailCause::INVALID_TRANSIT_NW_SELECTION:
      return nsILastCallFailCause::CALL_FAIL_INVALID_TRANSIT_NW_SELECTION;
    case LastCallFailCause::SEMANTICALLY_INCORRECT_MESSAGE:
      return nsILastCallFailCause::CALL_FAIL_SEMANTICALLY_INCORRECT_MESSAGE;
    case LastCallFailCause::INVALID_MANDATORY_INFORMATION:
      return nsILastCallFailCause::CALL_FAIL_INVALID_MANDATORY_INFORMATION;
    case LastCallFailCause::MESSAGE_TYPE_NON_IMPLEMENTED:
      return nsILastCallFailCause::CALL_FAIL_MESSAGE_TYPE_NON_IMPLEMENTED;
    case LastCallFailCause::MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
      return nsILastCallFailCause::
          CALL_FAIL_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
    case LastCallFailCause::INFORMATION_ELEMENT_NON_EXISTENT:
      return nsILastCallFailCause::CALL_FAIL_INFORMATION_ELEMENT_NON_EXISTENT;
    case LastCallFailCause::CONDITIONAL_IE_ERROR:
      return nsILastCallFailCause::CALL_FAIL_CONDITIONAL_IE_ERROR;
    case LastCallFailCause::MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
      return nsILastCallFailCause::
          CALL_FAIL_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
    case LastCallFailCause::RECOVERY_ON_TIMER_EXPIRED:
      return nsILastCallFailCause::CALL_FAIL_RECOVERY_ON_TIMER_EXPIRED;
    case LastCallFailCause::PROTOCOL_ERROR_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_PROTOCOL_ERROR_UNSPECIFIED;
    case LastCallFailCause::INTERWORKING_UNSPECIFIED:
      return nsILastCallFailCause::CALL_FAIL_INTERWORKING_UNSPECIFIED;
    case LastCallFailCause::CALL_BARRED:
      return nsILastCallFailCause::CALL_FAIL_CALL_BARRED;
    case LastCallFailCause::FDN_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_FDN_BLOCKED;
    case LastCallFailCause::IMSI_UNKNOWN_IN_VLR:
      return nsILastCallFailCause::CALL_FAIL_IMSI_UNKNOWN_IN_VLR;
    case LastCallFailCause::IMEI_NOT_ACCEPTED:
      return nsILastCallFailCause::CALL_FAIL_IMEI_NOT_ACCEPTED;
    case LastCallFailCause::DIAL_MODIFIED_TO_USSD:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_USSD;
    case LastCallFailCause::DIAL_MODIFIED_TO_SS:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_SS;
    case LastCallFailCause::DIAL_MODIFIED_TO_DIAL:
      return nsILastCallFailCause::CALL_FAIL_DIAL_MODIFIED_TO_DIAL;
    case LastCallFailCause::RADIO_OFF:
      return nsILastCallFailCause::CALL_FAIL_RADIO_OFF;
    case LastCallFailCause::OUT_OF_SERVICE:
      return nsILastCallFailCause::CALL_FAIL_OUT_OF_SERVICE;
    case LastCallFailCause::NO_VALID_SIM:
      return nsILastCallFailCause::CALL_FAIL_NO_VALID_SIM;
    case LastCallFailCause::RADIO_INTERNAL_ERROR:
      return nsILastCallFailCause::CALL_FAIL_RADIO_INTERNAL_ERROR;
    case LastCallFailCause::NETWORK_RESP_TIMEOUT:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_RESP_TIMEOUT;
    case LastCallFailCause::NETWORK_REJECT:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_REJECT;
    case LastCallFailCause::RADIO_ACCESS_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_ACCESS_FAILURE;
    case LastCallFailCause::RADIO_LINK_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_LINK_FAILURE;
    case LastCallFailCause::RADIO_LINK_LOST:
      return nsILastCallFailCause::CALL_FAIL_RADIO_LINK_LOST;
    case LastCallFailCause::RADIO_UPLINK_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_UPLINK_FAILURE;
    case LastCallFailCause::RADIO_SETUP_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_RADIO_SETUP_FAILURE;
    case LastCallFailCause::RADIO_RELEASE_NORMAL:
      return nsILastCallFailCause::CALL_FAIL_RADIO_RELEASE_NORMAL;
    case LastCallFailCause::RADIO_RELEASE_ABNORMAL:
      return nsILastCallFailCause::CALL_FAIL_RADIO_RELEASE_ABNORMAL;
    case LastCallFailCause::ACCESS_CLASS_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_ACCESS_CLASS_BLOCKED;
    case LastCallFailCause::NETWORK_DETACH:
      return nsILastCallFailCause::CALL_FAIL_NETWORK_DETACH;
    case LastCallFailCause::CDMA_LOCKED_UNTIL_POWER_CYCLE:
      return nsILastCallFailCause::CALL_FAIL_CDMA_LOCKED_UNTIL_POWER_CYCLE;
    case LastCallFailCause::CDMA_DROP:
      return nsILastCallFailCause::CALL_FAIL_CDMA_DROP;
    case LastCallFailCause::CDMA_INTERCEPT:
      return nsILastCallFailCause::CALL_FAIL_CDMA_INTERCEPT;
    case LastCallFailCause::CDMA_REORDER:
      return nsILastCallFailCause::CALL_FAIL_CDMA_REORDER;
    case LastCallFailCause::CDMA_SO_REJECT:
      return nsILastCallFailCause::CALL_FAIL_CDMA_SO_REJECT;
    case LastCallFailCause::CDMA_RETRY_ORDER:
      return nsILastCallFailCause::CALL_FAIL_CDMA_RETRY_ORDER;
    case LastCallFailCause::CDMA_ACCESS_FAILURE:
      return nsILastCallFailCause::CALL_FAIL_CDMA_ACCESS_FAILURE;
    case LastCallFailCause::CDMA_PREEMPTED:
      return nsILastCallFailCause::CALL_FAIL_CDMA_PREEMPTED;
    case LastCallFailCause::CDMA_NOT_EMERGENCY:
      return nsILastCallFailCause::CALL_FAIL_CDMA_NOT_EMERGENCY;
    case LastCallFailCause::CDMA_ACCESS_BLOCKED:
      return nsILastCallFailCause::CALL_FAIL_CDMA_ACCESS_BLOCKED;
    default:
      return nsILastCallFailCause::CALL_FAIL_ERROR_UNSPECIFIED;
  }
}

int32_t nsRilResponse::convertAppType(AppType type) {
  switch (type) {
    case AppType::UNKNOWN:
      return nsIRilResponseResult::CARD_APPTYPE_UNKNOWN;
    case AppType::SIM:
      return nsIRilResponseResult::CARD_APPTYPE_SIM;
    case AppType::USIM:
      return nsIRilResponseResult::CARD_APPTYPE_USIM;
    case AppType::RUIM:
      return nsIRilResponseResult::CARD_APPTYPE_RUIM;
    case AppType::CSIM:
      return nsIRilResponseResult::CARD_APPTYPE_CSIM;
    case AppType::ISIM:
      return nsIRilResponseResult::CARD_APPTYPE_ISIM;
    default:
      return nsIRilResponseResult::CARD_APPTYPE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertAppState(AppState state) {
  switch (state) {
    case AppState::UNKNOWN:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
    case AppState::DETECTED:
      return nsIRilResponseResult::CARD_APPSTATE_DETECTED;
    case AppState::PIN:
      return nsIRilResponseResult::CARD_APPSTATE_PIN;
    case AppState::PUK:
      return nsIRilResponseResult::CARD_APPSTATE_PUK;
    case AppState::SUBSCRIPTION_PERSO:
      return nsIRilResponseResult::CARD_APPSTATE_SUBSCRIPTION_PERSO;
    case AppState::READY:
      return nsIRilResponseResult::CARD_APPSTATE_READY;
    default:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertPersoSubstate(PersoSubstate state) {
  switch (state) {
    case PersoSubstate::UNKNOWN:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_UNKNOWN;
    case PersoSubstate::IN_PROGRESS:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_IN_PROGRESS;
    case PersoSubstate::READY:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_READY;
    case PersoSubstate::SIM_NETWORK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK;
    case PersoSubstate::SIM_NETWORK_SUBSET:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET;
    case PersoSubstate::SIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE;
    case PersoSubstate::SIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER;
    case PersoSubstate::SIM_SIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM;
    case PersoSubstate::SIM_NETWORK_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_PUK;
    case PersoSubstate::SIM_NETWORK_SUBSET_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK;
    case PersoSubstate::SIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK;
    case PersoSubstate::SIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate::SIM_SIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_SIM_SIM_PUK;
    case PersoSubstate::RUIM_NETWORK1:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1;
    case PersoSubstate::RUIM_NETWORK2:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2;
    case PersoSubstate::RUIM_HRPD:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD;
    case PersoSubstate::RUIM_CORPORATE:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE;
    case PersoSubstate::RUIM_SERVICE_PROVIDER:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER;
    case PersoSubstate::RUIM_RUIM:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM;
    case PersoSubstate::RUIM_NETWORK1_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK;
    case PersoSubstate::RUIM_NETWORK2_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK;
    case PersoSubstate::RUIM_HRPD_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_HRPD_PUK;
    case PersoSubstate::RUIM_CORPORATE_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK;
    case PersoSubstate::RUIM_SERVICE_PROVIDER_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK;
    case PersoSubstate::RUIM_RUIM_PUK:
      return nsIRilResponseResult::CARD_PERSOSUBSTATE_RUIM_RUIM_PUK;
    default:
      return nsIRilResponseResult::CARD_APPSTATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertPinState(PinState state) {
  switch (state) {
    case PinState::UNKNOWN:
      return nsIRilResponseResult::CARD_PIN_STATE_UNKNOWN;
    case PinState::ENABLED_NOT_VERIFIED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_NOT_VERIFIED;
    case PinState::ENABLED_VERIFIED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_VERIFIED;
    case PinState::DISABLED:
      return nsIRilResponseResult::CARD_PIN_STATE_DISABLED;
    case PinState::ENABLED_BLOCKED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_BLOCKED;
    case PinState::ENABLED_PERM_BLOCKED:
      return nsIRilResponseResult::CARD_PIN_STATE_ENABLED_PERM_BLOCKED;
    default:
      return nsIRilResponseResult::CARD_PIN_STATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertCardState(CardState state) {
  switch (state) {
    case CardState::ABSENT:
      return nsIRilResponseResult::CARD_STATE_ABSENT;
    case CardState::PRESENT:
      return nsIRilResponseResult::CARD_STATE_PRESENT;
    case CardState::ERROR:
      return nsIRilResponseResult::CARD_STATE_ERROR;
    case CardState::RESTRICTED:
      return nsIRilResponseResult::CARD_STATE_RESTRICTED;
    default:
      return nsIRilResponseResult::CARD_STATE_ERROR;
  }
}

int32_t nsRilResponse::convertRegState(RegState state) {
  switch (state) {
    case RegState::NOT_REG_MT_NOT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP;
    case RegState::REG_HOME:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_HOME;
    case RegState::NOT_REG_MT_SEARCHING_OP:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP;
    case RegState::REG_DENIED:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED;
    case RegState::UNKNOWN:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
    case RegState::REG_ROAMING:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_ROAMING;
    case RegState::NOT_REG_MT_NOT_SEARCHING_OP_EM:
      return nsIRilResponseResult::
          RADIO_REG_STATE_NOT_REG_MT_NOT_SEARCHING_OP_EM;
    case RegState::NOT_REG_MT_SEARCHING_OP_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_NOT_REG_MT_SEARCHING_OP_EM;
    case RegState::REG_DENIED_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_REG_DENIED_EM;
    case RegState::UNKNOWN_EM:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN_EM;
    default:
      return nsIRilResponseResult::RADIO_REG_STATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertUusType(UusType type) {
  switch (type) {
    case UusType::TYPE1_IMPLICIT:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
    case UusType::TYPE1_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_REQUIRED;
    case UusType::TYPE1_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_NOT_REQUIRED;
    case UusType::TYPE2_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_REQUIRED;
    case UusType::TYPE2_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE2_NOT_REQUIRED;
    case UusType::TYPE3_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_REQUIRED;
    case UusType::TYPE3_NOT_REQUIRED:
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE3_NOT_REQUIRED;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSTYPE_TYPE1_IMPLICIT;
  }
}

int32_t nsRilResponse::convertUusDcs(UusDcs dcs) {
  switch (dcs) {
    case UusDcs::USP:
      return nsIRilResponseResult::CALL_UUSDCS_USP;
    case UusDcs::OSIHLP:
      return nsIRilResponseResult::CALL_UUSDCS_OSIHLP;
    case UusDcs::X244:
      return nsIRilResponseResult::CALL_UUSDCS_X244;
    case UusDcs::RMCF:
      return nsIRilResponseResult::CALL_UUSDCS_RMCF;
    case UusDcs::IA5C:
      return nsIRilResponseResult::CALL_UUSDCS_IA5C;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_UUSDCS_USP;
  }
}

int32_t nsRilResponse::convertCallPresentation(CallPresentation state) {
  switch (state) {
    case CallPresentation::ALLOWED:
      return nsIRilResponseResult::CALL_PRESENTATION_ALLOWED;
    case CallPresentation::RESTRICTED:
      return nsIRilResponseResult::CALL_PRESENTATION_RESTRICTED;
    case CallPresentation::UNKNOWN:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
    case CallPresentation::PAYPHONE:
      return nsIRilResponseResult::CALL_PRESENTATION_PAYPHONE;
    default:
      return nsIRilResponseResult::CALL_PRESENTATION_UNKNOWN;
  }
}

int32_t nsRilResponse::convertCallState(CallState state) {
  switch (state) {
    case CallState::ACTIVE:
      return nsIRilResponseResult::CALL_STATE_ACTIVE;
    case CallState::HOLDING:
      return nsIRilResponseResult::CALL_STATE_HOLDING;
    case CallState::DIALING:
      return nsIRilResponseResult::CALL_STATE_DIALING;
    case CallState::ALERTING:
      return nsIRilResponseResult::CALL_STATE_ALERTING;
    case CallState::INCOMING:
      return nsIRilResponseResult::CALL_STATE_INCOMING;
    case CallState::WAITING:
      return nsIRilResponseResult::CALL_STATE_WAITING;
    default:
      return nsIRilResponseResult::CALL_STATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertPreferredNetworkType(PreferredNetworkType type) {
  switch (type) {
    case PreferredNetworkType::GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_WCDMA;
    case PreferredNetworkType::GSM_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_ONLY;
    case PreferredNetworkType::WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_WCDMA;
    case PreferredNetworkType::GSM_WCDMA_AUTO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_GSM_WCDMA_AUTO;
    case PreferredNetworkType::CDMA_EVDO_AUTO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_CDMA_EVDO_AUTO;
    case PreferredNetworkType::CDMA_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_CDMA_ONLY;
    case PreferredNetworkType::EVDO_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_EVDO_ONLY;
    case PreferredNetworkType::GSM_WCDMA_CDMA_EVDO_AUTO:
      return nsIRilResponseResult::
          PREFERRED_NETWORK_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO;
    case PreferredNetworkType::LTE_CDMA_EVDO:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_CDMA_EVDO;
    case PreferredNetworkType::LTE_GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_GSM_WCDMA;
    case PreferredNetworkType::LTE_CMDA_EVDO_GSM_WCDMA:
      return nsIRilResponseResult::
          PREFERRED_NETWORK_TYPE_LTE_CMDA_EVDO_GSM_WCDMA;
    case PreferredNetworkType::LTE_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_ONLY;
    case PreferredNetworkType::LTE_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_WCDMA;
    case PreferredNetworkType::TD_SCDMA_ONLY:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_ONLY;
    case PreferredNetworkType::TD_SCDMA_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_WCDMA;
    case PreferredNetworkType::TD_SCDMA_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_LTE;
    case PreferredNetworkType::TD_SCDMA_GSM:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM;
    case PreferredNetworkType::TD_SCDMA_GSM_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_LTE;
    case PreferredNetworkType::TD_SCDMA_GSM_WCDMA:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA;
    case PreferredNetworkType::TD_SCDMA_WCDMA_LTE:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_TD_SCDMA_WCDMA_LTE;
    case PreferredNetworkType::TD_SCDMA_GSM_WCDMA_LTE:
      return nsIRilResponseResult::
          PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA_LTE;
    case PreferredNetworkType::TD_SCDMA_GSM_WCDMA_CDMA_EVDO_AUTO:
      return nsIRilResponseResult::
          PREFERRED_NETWORK_TYPE_TD_SCDMA_GSM_WCDMA_CDMA_EVDO_AUTO;
    case PreferredNetworkType::TD_SCDMA_LTE_CDMA_EVDO_GSM_WCDMA:
      return nsIRilResponseResult::
          PREFERRED_NETWORK_TYPE_TD_SCDMA_LTE_CDMA_EVDO_GSM_WCDMA;
    default:
      return nsIRilResponseResult::PREFERRED_NETWORK_TYPE_LTE_GSM_WCDMA;
  }
}

int32_t nsRilResponse::convertOperatorState(OperatorStatus status) {
  switch (status) {
    case OperatorStatus::UNKNOWN:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
    case OperatorStatus::AVAILABLE:
      return nsIRilResponseResult::QAN_STATE_AVAILABLE;
    case OperatorStatus::CURRENT:
      return nsIRilResponseResult::QAN_STATE_CURRENT;
    case OperatorStatus::FORBIDDEN:
      return nsIRilResponseResult::QAN_STATE_FORBIDDEN;
    default:
      return nsIRilResponseResult::QAN_STATE_UNKNOWN;
  }
}

int32_t nsRilResponse::convertCallForwardState(CallForwardInfoStatus status) {
  switch (status) {
    case CallForwardInfoStatus::DISABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
    case CallForwardInfoStatus::ENABLE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ENABLE;
    case CallForwardInfoStatus::INTERROGATE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_INTERROGATE;
    case CallForwardInfoStatus::REGISTRATION:
      return nsIRilResponseResult::CALL_FORWARD_STATE_REGISTRATION;
    case CallForwardInfoStatus::ERASURE:
      return nsIRilResponseResult::CALL_FORWARD_STATE_ERASURE;
    default:
      // TODO need confirmed the default value.
      return nsIRilResponseResult::CALL_FORWARD_STATE_DISABLE;
  }
}

int32_t nsRilResponse::convertClipState(ClipStatus status) {
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

int32_t nsRilResponse::convertTtyMode(TtyMode mode) {
  switch (mode) {
    case TtyMode::OFF:
      return nsIRilResponseResult::TTY_MODE_OFF;
    case TtyMode::FULL:
      return nsIRilResponseResult::TTY_MODE_FULL;
    case TtyMode::HCO:
      return nsIRilResponseResult::TTY_MODE_HCO;
    case TtyMode::VCO:
      return nsIRilResponseResult::TTY_MODE_VCO;
    default:
      return nsIRilResponseResult::TTY_MODE_OFF;
  }
}
