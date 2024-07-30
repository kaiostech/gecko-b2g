/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilWorker.h"
#include "mozilla/Preferences.h"
#if ANDROID_VERSION >= 33
#include "nsRadioProxyServiceManager.h"
#endif
#include <arpa/inet.h>
#include <cstdint>

/* Logging related */
#if !defined(RILWORKER_LOG_TAG)
#define RILWORKER_LOG_TAG "RilWorker"
#endif

#undef INFO
#undef ERROR
#undef DEBUG
#define INFO(args...)                                                          \
  __android_log_print(ANDROID_LOG_INFO, RILWORKER_LOG_TAG, ##args)
#define ERROR(args...)                                                         \
  __android_log_print(ANDROID_LOG_ERROR, RILWORKER_LOG_TAG, ##args)
#define ERROR_NS_OK(args...)                                                   \
  {                                                                            \
    __android_log_print(ANDROID_LOG_ERROR, RILWORKER_LOG_TAG, ##args);         \
    return NS_OK;                                                              \
  }
#define DEBUG(args...)                                                         \
  do {                                                                         \
    if (gRilDebug_isLoggingEnabled) {                                          \
      __android_log_print(ANDROID_LOG_DEBUG, RILWORKER_LOG_TAG, ##args);       \
    }                                                                          \
  } while (0)

NS_IMPL_ISUPPORTS(nsRilWorker, nsIRilWorker)
static hidl_string HIDL_SERVICE_NAME[3] = { "slot1", "slot2", "slot3" };

/**
 *
 */
nsRilWorker::nsRilWorker(uint32_t aClientId)
{
  DEBUG("init nsRilWorker");
  mRadioProxy = nullptr;
#if ANDROID_VERSION >= 33
  mHalVerison = AIDL_VERSION;
#else
  mHalVerison = HAL_VERSION_V1_1;
#endif
  mDeathRecipient = nullptr;
  mRilCallback = nullptr;
  mClientId = aClientId;
  mRilResponse = new nsRilResponse(this);
  mRilIndication = new nsRilIndication(this);
  updateDebug();
}

void
nsRilWorker::updateDebug()
{
  gRilDebug_isLoggingEnabled =
    mozilla::Preferences::GetBool("ril.debugging.enabled", false);
}

/**
 * nsIRadioInterface implementation
 */
NS_IMETHODIMP
nsRilWorker::SendRilRequest(JS::HandleValue message)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::InitRil(nsIRilCallback* callback)
{
  mRilCallback = callback;
  GetRadioProxy();
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetRadioPower(int32_t serial,
                           bool enabled,
                           bool forEmergencyCall,
                           bool preferredForEmergencyCall)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_RADIO_POWER on = %d", serial, enabled);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetRadioPower(
      serial, enabled, forEmergencyCall, preferredForEmergencyCall);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ret = mRadioProxy->setRadioPower_1_6(
        serial, enabled, forEmergencyCall, preferredForEmergencyCall);
    } else if (mHalVerison >= HAL_VERSION_V1_5) {
      ret = mRadioProxy->setRadioPower_1_5(
        serial, enabled, forEmergencyCall, preferredForEmergencyCall);
    } else {
      ret = mRadioProxy->setRadioPower(serial, enabled);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("SetRadioPower Error.");
    }
  }
#else
  ret = mRadioProxy->setRadioPower(serial, enabled);
  if (!ret.isOk()) {
    ERROR_NS_OK("SetRadioPower Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetDeviceIdentity(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DEVICE_IDENTITY", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetDeviceIdentity(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getDeviceIdentity(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getDeviceIdentity Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetVoiceRegistrationState(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_VOICE_REGISTRATION_STATE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetVoiceRegistrationState(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ret = mRadioProxy->getVoiceRegistrationState_1_6(serial);
    } else if (mHalVerison >= HAL_VERSION_V1_5) {
      ret = mRadioProxy->getVoiceRegistrationState_1_5(serial);
    } else {
      ret = mRadioProxy->getVoiceRegistrationState(serial);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("getVoiceRegistrationState Error.");
    }
  }
#else
  ret = mRadioProxy->getVoiceRegistrationState(serial);
  if (!ret.isOk()) {
      ERROR_NS_OK("getVoiceRegistrationState Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetDataRegistrationState(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DATA_REGISTRATION_STATE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetDataRegistrationState(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ret = mRadioProxy->getDataRegistrationState_1_6(serial);
    } else if (mHalVerison >= HAL_VERSION_V1_5) {
      ret = mRadioProxy->getDataRegistrationState_1_5(serial);
    } else {
      ret = mRadioProxy->getDataRegistrationState(serial);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("getDataRegistrationState Error.");
    }
  }
#else
  ret = mRadioProxy->getDataRegistrationState(serial);
  if (!ret.isOk()) {
      ERROR_NS_OK("getDataRegistrationState Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetOperator(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_OPERATOR", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetOperator(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getOperator(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getOperator Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetNetworkSelectionMode(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetNetworkSelectionMode(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getNetworkSelectionMode(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getNetworkSelectionMode Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetSignalStrength(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SIGNAL_STRENGTH", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetSignalStrength(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ret = mRadioProxy->getSignalStrength_1_6(serial);
    } else if (mHalVerison >= HAL_VERSION_V1_4) {
      ret = mRadioProxy->getSignalStrength_1_4(serial);
    } else {
      ret = mRadioProxy->getSignalStrength(serial);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("getSignalStrength Error.");
    }
  }
#else
  ret = mRadioProxy->getSignalStrength(serial);
  if (!ret.isOk()) {
    ERROR_NS_OK("getSignalStrength Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetVoiceRadioTechnology(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_VOICE_RADIO_TECH", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetVoiceRadioTechnology(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getVoiceRadioTechnology(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getVoiceRadioTechnology Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetIccCardStatus(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SIM_STATUS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetIccCardStatus(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getIccCardStatus(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getIccCardStatus Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ReportSmsMemoryStatus(int32_t serial, bool available)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_REPORT_SMS_MEMORY_STATUS available = %d",
    serial,
    available);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ReportSmsMemoryStatus(serial, available);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->reportSmsMemoryStatus(serial, available);

    if (!ret.isOk()) {
      ERROR_NS_OK("reportSmsMemoryStatus Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetCellInfoListRate(int32_t serial, int32_t rateInMillis)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_CELL_INFO_LIST_RATE rateInMillis = "
    "%d",
    serial,
    rateInMillis);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (rateInMillis == 0) {
      rateInMillis = INT32_MAX;
    }
    mRadioProxyServiceManager->SetCellInfoListRate(serial, rateInMillis);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (rateInMillis == 0) {
      rateInMillis = INT32_MAX;
    }
    Return<void> ret = mRadioProxy->setCellInfoListRate(serial, rateInMillis);

    if (!ret.isOk()) {
      ERROR_NS_OK("setCellInfoListRate Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetDataAllowed(int32_t serial, bool allowed)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_ALLOW_DATA allowed = %d", serial, allowed);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetDataAllowed(serial, allowed);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setDataAllowed(serial, allowed);

    if (!ret.isOk()) {
      ERROR_NS_OK("setDataAllowed Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetBasebandVersion(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_BASEBAND_VERSION", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetBasebandVersion(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getBasebandVersion(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getBasebandVersion Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetUiccSubscription(int32_t serial,
                                 int32_t slotId,
                                 int32_t appIndex,
                                 int32_t subId,
                                 int32_t subStatus)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_UICC_SUBSCRIPTION slotId = %d "
        "appIndex = %d subId = %d subStatus = %d",
        serial,
        slotId,
        appIndex,
        subId,
        subStatus);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetUiccSubscription(
      serial, slotId, appIndex, subId, subStatus);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    SelectUiccSub_V1_0 info;
    info.slot = slotId;
    info.appIndex = appIndex;
    info.subType = SubscriptionType_V1_0(subId);
    info.actStatus = UiccSubActStatus_V1_0(subStatus);

    Return<void> ret = mRadioProxy->setUiccSubscription(serial, info);

    if (!ret.isOk()) {
      ERROR_NS_OK("setUiccSubscription Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetMute(int32_t serial, bool enableMute)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_MUTE enableMute = %d",
        serial,
        enableMute);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetMute(serial, enableMute);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setMute(serial, enableMute);

    if (!ret.isOk()) {
      ERROR_NS_OK("setMute Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetMute(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_MUTE ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetMute(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getMute(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getMute Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetSmscAddress(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SMSC_ADDRESS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetSmscAddress(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getSmscAddress(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getSmscAddress Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::RequestDial(int32_t serial,
                         const nsAString& address,
                         int32_t clirMode,
                         int32_t uusType,
                         int32_t uusDcs,
                         const nsAString& uusData)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DIAL", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->RequestDial(
      serial, address, clirMode, uusType, uusDcs, uusData);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    UusInfo_V1_0 info;
    std::vector<UusInfo_V1_0> uusInfo;
    info.uusType = UusType_V1_0(uusType);
    info.uusDcs = UusDcs_V1_0(uusDcs);
    if (uusData.Length() == 0) {
      info.uusData = NULL;
    } else {
      info.uusData = NS_ConvertUTF16toUTF8(uusData).get();
    }
    uusInfo.push_back(info);

    Dial_V1_0 dialInfo;
    dialInfo.address = NS_ConvertUTF16toUTF8(address).get();
    dialInfo.clir = Clir_V1_0(clirMode);
    dialInfo.uusInfo = uusInfo;
    Return<void> ret = mRadioProxy->dial(serial, dialInfo);

    if (!ret.isOk()) {
      ERROR_NS_OK("dial Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetCurrentCalls(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CURRENT_CALLS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetCurrentCalls(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ret = mRadioProxy->getCurrentCalls_1_6(serial);
    } else {
      ret = mRadioProxy->getCurrentCalls(serial);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("getCurrentCalls Error.");
    }
  }
#else
  ret = mRadioProxy->getCurrentCalls(serial);
  if (!ret.isOk()) {
      ERROR_NS_OK("getCurrentCalls Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::HangupConnection(int32_t serial, int32_t callIndex)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_HANGUP callIndex = %d", serial, callIndex);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->HangupConnection(serial, callIndex);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->hangup(serial, callIndex);

    if (!ret.isOk()) {
      ERROR_NS_OK("hangup Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::HangupWaitingOrBackground(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->HangupWaitingOrBackground(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->hangupWaitingOrBackground(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("hangupWaitingOrBackground Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::HangupForegroundResumeBackground(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->HangupForegroundResumeBackground(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->hangupForegroundResumeBackground(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("hangupForegroundResumeBackground Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SwitchWaitingOrHoldingAndActive(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SwitchWaitingOrHoldingAndActive(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->switchWaitingOrHoldingAndActive(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("switchWaitingOrHoldingAndActive Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::Conference(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CONFERENCE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->Conference(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->conference(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("conference Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetLastCallFailCause(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_LAST_CALL_FAIL_CAUSE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetLastCallFailCause(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getLastCallFailCause(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getLastCallFailCause Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::AcceptCall(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ANSWER", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->AcceptCall(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->acceptCall(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("acceptCall Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetPreferredNetworkType(int32_t serial, int32_t networkType)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE "
        "networkType=%d",
        serial,
        networkType);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetPreferredNetworkType(serial, networkType);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setPreferredNetworkType(
      serial, PreferredNetworkType_V1_0(networkType));

    if (!ret.isOk()) {
      ERROR_NS_OK("setPreferredNetworkType Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetPreferredNetworkType(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetPreferredNetworkType(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getPreferredNetworkType(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getPreferredNetworkType Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetNetworkSelectionModeAutomatic(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetNetworkSelectionModeAutomatic(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setNetworkSelectionModeAutomatic(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("setNetworkSelectionModeAutomatic Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetNetworkSelectionModeManual(int32_t serial,
                                           const nsAString& operatorNumeric,
                                           int32_t ran)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL "
        "operatorNumeric = %s",
        serial,
        NS_ConvertUTF16toUTF8(operatorNumeric).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetNetworkSelectionModeManual(
      serial, operatorNumeric, ran);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      ret = mRadioProxy->setNetworkSelectionModeManual_1_5(
        serial,
        NS_ConvertUTF16toUTF8(operatorNumeric).get(),
        RadioAccessNetworks_V1_5(ran));
    } else {
      ret = mRadioProxy->setNetworkSelectionModeManual(
        serial, NS_ConvertUTF16toUTF8(operatorNumeric).get());
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("setNetworkSelectionModeManual Error.");
    }
  }
#else
  ret = mRadioProxy->setNetworkSelectionModeManual(
        serial, NS_ConvertUTF16toUTF8(operatorNumeric).get());
  if (!ret.isOk()) {
      ERROR_NS_OK("setNetworkSelectionModeManual Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetAvailableNetworks(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_AVAILABLE_NETWORKS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetAvailableNetworks(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getAvailableNetworks(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getAvailableNetworks Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetInitialAttachApn(int32_t serial,
                                 nsIDataProfile* profile,
                                 bool isRoaming)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_INITIAL_ATTACH_APN", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetInitialAttachApn(serial, profile, isRoaming);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      ret = mRadioProxy->setInitialAttachApn_1_5(
        serial, convertToHalDataProfile_V1_5(profile));
    } else if (mHalVerison >= HAL_VERSION_V1_4) {
      ret = mRadioProxy->setInitialAttachApn_1_4(
        serial, convertToHalDataProfile_V1_4(profile));
    } else {
      bool modemCognitive;
      profile->GetModemCognitive(&modemCognitive);
      ret = mRadioProxy->setInitialAttachApn(
        serial, convertToHalDataProfile(profile), modemCognitive, isRoaming);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("setInitialAttachApn Error.");
    }
  }
#else
  bool modemCognitive;
  profile->GetModemCognitive(&modemCognitive);
  ret = mRadioProxy->setInitialAttachApn(
      serial, convertToHalDataProfile(profile), modemCognitive, isRoaming);
  if (!ret.isOk()) {
      ERROR_NS_OK("setInitialAttachApn Error.");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetDataProfile(int32_t serial,
                            const nsTArray<RefPtr<nsIDataProfile>>& profileList,
                            bool isRoaming)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_DATA_PROFILE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetDataProfile(serial, profileList, isRoaming);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    Return<void> ret;
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      std::vector<DataProfileInfo_V1_5> dataProfileInfoList;
      for (uint32_t i = 0; i < profileList.Length(); i++) {
        DataProfileInfo_V1_5 profile =
          convertToHalDataProfile_V1_5(profileList[i]);
        dataProfileInfoList.push_back(profile);
      }
      ret = mRadioProxy->setDataProfile_1_5(serial, dataProfileInfoList);
    } else if (mHalVerison >= HAL_VERSION_V1_4) {
      std::vector<DataProfileInfo_V1_4> dataProfileInfoList;
      for (uint32_t i = 0; i < profileList.Length(); i++) {
        DataProfileInfo_V1_4 profile =
          convertToHalDataProfile_V1_4(profileList[i]);
        dataProfileInfoList.push_back(profile);
      }
      ret = mRadioProxy->setDataProfile_1_4(serial, dataProfileInfoList);
    } else {
      std::vector<DataProfileInfo_V1_0> dataProfileInfoList;
      for (uint32_t i = 0; i < profileList.Length(); i++) {
        DataProfileInfo_V1_0 profile = convertToHalDataProfile(profileList[i]);
        dataProfileInfoList.push_back(profile);
      }
      ret = mRadioProxy->setDataProfile(serial, dataProfileInfoList, isRoaming);
    }

    if (!ret.isOk()) {
      ERROR_NS_OK("setDataProfile Error.");
    }
  }
#else
  std::vector<DataProfileInfo_V1_0> dataProfileInfoList;
  for (uint32_t i = 0; i < profileList.Length(); i++) {
    DataProfileInfo_V1_0 profile = convertToHalDataProfile(profileList[i]);
    dataProfileInfoList.push_back(profile);
  }
  ret = mRadioProxy->setDataProfile(serial, dataProfileInfoList, isRoaming);
  if (!ret.isOk()) {
    ERROR_NS_OK("setDataProfile Error.");
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetupDataCall(int32_t serial,
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
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SETUP_DATA_CALL", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetupDataCall(serial,
                                             radioTechnology,
                                             accessNetworkType,
                                             profile,
                                             modemConfig,
                                             allowRoaming,
                                             isRoaming,
                                             reason,
                                             addresses,
                                             dnses,
                                             pduSessionId,
                                             sliceInfo,
                                             trafficDescriptor,
                                             matchAllRuleAllowed);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ::std::vector<LinkAddress_V1_5> linkAddrs;
      for (uint32_t i = 0; i < addresses.Length(); i++) {
        LinkAddress_V1_5 address = convertToHalLinkAddress_V1_5(addresses[i]);
        linkAddrs.push_back(address);
      }

      std::vector<hidl_string> dns;
      for (uint32_t i = 0; i < dnses.Length(); i++) {
        dns.push_back(NS_ConvertUTF16toUTF8(dnses[i]).get());
      }

      mRadioProxy->setupDataCall_1_6(
        serial,
        AccessNetwork_V1_5(accessNetworkType),
        convertToHalDataProfile_V1_5(profile),
        allowRoaming,
        DataRequestReason_V1_2(reason),
        linkAddrs,
        dns,
        pduSessionId,
        convertToHalSliceInfo_V1_6(sliceInfo),
        convertToHalTrafficDescriptor_V1_6(trafficDescriptor),
        matchAllRuleAllowed);
    } else if (mHalVerison >= HAL_VERSION_V1_5) {
      ::std::vector<LinkAddress_V1_5> linkAddrs;
      for (uint32_t i = 0; i < addresses.Length(); i++) {
        LinkAddress_V1_5 address = convertToHalLinkAddress_V1_5(addresses[i]);
        linkAddrs.push_back(address);
      }

      std::vector<hidl_string> dns;
      for (uint32_t i = 0; i < dnses.Length(); i++) {
        dns.push_back(NS_ConvertUTF16toUTF8(dnses[i]).get());
      }
      mRadioProxy->setupDataCall_1_5(serial,
                                     AccessNetwork_V1_5(accessNetworkType),
                                     convertToHalDataProfile_V1_5(profile),
                                     allowRoaming,
                                     DataRequestReason_V1_2(reason),
                                     linkAddrs,
                                     dns);
    } else if (mHalVerison >= HAL_VERSION_V1_2) {
      std::vector<hidl_string> addr;
      for (uint32_t i = 0; i < addresses.Length(); i++) {
        nsString address;
        addresses[i]->GetAddress(address);
        addr.push_back(NS_ConvertUTF16toUTF8(address).get());
      }

      std::vector<hidl_string> dns;
      for (uint32_t i = 0; i < dnses.Length(); i++) {
        dns.push_back(NS_ConvertUTF16toUTF8(dnses[i]).get());
      }
      if (mHalVerison >= HAL_VERSION_V1_4) {
        mRadioProxy->setupDataCall_1_4(serial,
                                       AccessNetwork_V1_4(accessNetworkType),
                                       convertToHalDataProfile_V1_4(profile),
                                       allowRoaming,
                                       DataRequestReason_V1_2(reason),
                                       addr,
                                       dns);
      } else {
        mRadioProxy->setupDataCall_1_2(serial,
                                       AccessNetwork_V1_2(accessNetworkType),
                                       convertToHalDataProfile(profile),
                                       modemConfig,
                                       allowRoaming,
                                       isRoaming,
                                       DataRequestReason_V1_2(reason),
                                       addr,
                                       dns);
      }
    } else {
      mRadioProxy->setupDataCall(serial,
                                 RadioTechnology_V1_0(radioTechnology),
                                 convertToHalDataProfile(profile),
                                 modemConfig,
                                 allowRoaming,
                                 isRoaming);
    }
  }
#else
  mRadioProxy->setupDataCall(serial,
                             RadioTechnology_V1_0(radioTechnology),
                             convertToHalDataProfile(profile),
                             modemConfig,
                             allowRoaming,
                             isRoaming);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::DeactivateDataCall(int32_t serial, int32_t cid, int32_t reason)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DEACTIVATE_DATA_CALL", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->DeactivateDataCall(serial, cid, reason);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_2) {
      mRadioProxy->deactivateDataCall_1_2(
        serial, cid, DataRequestReason_V1_2(reason));
    } else {
      mRadioProxy->deactivateDataCall(
        serial, cid, (reason == 0 ? false : true));
    }
  }
#else
  mRadioProxy->deactivateDataCall(
    serial, cid, (reason == 0 ? false : true));
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetDataCallList(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DATA_CALL_LIST", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetDataCallList(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getDataCallList(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getDataCallList Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetCellInfoList(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CELL_INFO_LIST", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetCellInfoList(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getCellInfoList(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getCellInfoList Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetIMSI(int32_t serial, const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_IMSI aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetIMSI(serial, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->getImsiForApp(serial, NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("getImsiForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::IccIOForApp(int32_t serial,
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
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SIM_IO command = %d, fileId = %d, path "
    "= %s, p1 = %d, p2 = %d, p3 = %d, data = %s, pin2 = %s, aid = %s",
    serial,
    command,
    fileId,
    NS_ConvertUTF16toUTF8(path).get(),
    p1,
    p2,
    p3,
    NS_ConvertUTF16toUTF8(data).get(),
    NS_ConvertUTF16toUTF8(pin2).get(),
    NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->IccIOForApp(
      serial, command, fileId, path, p1, p2, p3, data, pin2, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    IccIo_V1_0 iccIo;
    iccIo.command = command;
    iccIo.fileId = fileId;
    iccIo.path = NS_ConvertUTF16toUTF8(path).get();
    iccIo.p1 = p1;
    iccIo.p2 = p2;
    iccIo.p3 = p3;
    iccIo.data = NS_ConvertUTF16toUTF8(data).get();
    iccIo.pin2 = NS_ConvertUTF16toUTF8(pin2).get();
    iccIo.aid = NS_ConvertUTF16toUTF8(aid).get();

    Return<void> ret = mRadioProxy->iccIOForApp(serial, iccIo);

    if (!ret.isOk()) {
      ERROR_NS_OK("iccIOForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetClir(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_CLIR", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetClir(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getClir(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getClir Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetClir(int32_t serial, int32_t clirMode)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_CLIR clirMode = %d", serial, clirMode);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetClir(serial, clirMode);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setClir(serial, clirMode);

    if (!ret.isOk()) {
      ERROR_NS_OK("setClir Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SendDtmf(int32_t serial, const nsAString& dtmfChar)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF dtmfChar = %s",
        serial,
        NS_ConvertUTF16toUTF8(dtmfChar).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendDtmf(serial, dtmfChar);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->sendDtmf(serial, NS_ConvertUTF16toUTF8(dtmfChar).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("sendDtmf Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::StartDtmf(int32_t serial, const nsAString& dtmfChar)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF_START dtmfChar = %s",
        serial,
        NS_ConvertUTF16toUTF8(dtmfChar).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StartDtmf(serial, dtmfChar);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->startDtmf(serial, NS_ConvertUTF16toUTF8(dtmfChar).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("startDtmf Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::StopDtmf(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_DTMF_STOP", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StopDtmf(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->stopDtmf(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("stopDtmf Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::RejectCall(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_UDUB", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->RejectCall(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->rejectCall(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("rejectCall Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SendUssd(int32_t serial, const nsAString& ussd)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEND_USSD ussd = %s",
        serial,
        NS_ConvertUTF16toUTF8(ussd).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendUssd(serial, ussd);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->sendUssd(serial, NS_ConvertUTF16toUTF8(ussd).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("sendUssd Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::CancelPendingUssd(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CANCEL_USSD", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->CancelPendingUssd(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->cancelPendingUssd(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("cancelPendingUssd Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetCallForwardStatus(int32_t serial,
                                  int32_t cfReason,
                                  int32_t serviceClass,
                                  const nsAString& number,
                                  int32_t toaNumber)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_QUERY_CALL_FORWARD_STATUS cfReason = %d "
    ", serviceClass = %d, number = %s",
    serial,
    cfReason,
    serviceClass,
    NS_ConvertUTF16toUTF8(number).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetCallForwardStatus(
      serial, cfReason, serviceClass, number, toaNumber);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    CallForwardInfo_V1_0 cfInfo;
    cfInfo.reason = cfReason;
    cfInfo.serviceClass = serviceClass;
    cfInfo.toa = toaNumber;
    cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
    cfInfo.timeSeconds = 0;

    Return<void> ret = mRadioProxy->getCallForwardStatus(serial, cfInfo);

    if (!ret.isOk()) {
      ERROR_NS_OK("getCallForwardStatus Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetCallForwardStatus(int32_t serial,
                                  int32_t action,
                                  int32_t cfReason,
                                  int32_t serviceClass,
                                  const nsAString& number,
                                  int32_t toaNumber,
                                  int32_t timeSeconds)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_CALL_FORWARD action = %d, cfReason "
    "= %d , serviceClass = %d, number = %s, timeSeconds = %d",
    serial,
    action,
    cfReason,
    serviceClass,
    NS_ConvertUTF16toUTF8(number).get(),
    timeSeconds);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetCallForwardStatus(
      serial, action, cfReason, serviceClass, number, toaNumber, timeSeconds);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    CallForwardInfo_V1_0 cfInfo;
    cfInfo.status = CallForwardInfoStatus_V1_0(action);
    cfInfo.reason = cfReason;
    cfInfo.serviceClass = serviceClass;
    cfInfo.toa = toaNumber;
    cfInfo.number = NS_ConvertUTF16toUTF8(number).get();
    cfInfo.timeSeconds = timeSeconds;

    Return<void> ret = mRadioProxy->setCallForward(serial, cfInfo);

    if (!ret.isOk()) {
      ERROR_NS_OK("setCallForward Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetCallWaiting(int32_t serial, int32_t serviceClass)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_CALL_WAITING serviceClass = %d",
        serial,
        serviceClass);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetCallWaiting(serial, serviceClass);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getCallWaiting(serial, serviceClass);

    if (!ret.isOk()) {
      ERROR_NS_OK("getCallWaiting Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetCallWaiting(int32_t serial, bool enable, int32_t serviceClass)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_CALL_WAITING enable = %d, "
        "serviceClass = %d",
        serial,
        enable,
        serviceClass);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetCallWaiting(serial, enable, serviceClass);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->setCallWaiting(serial, enable, serviceClass);

    if (!ret.isOk()) {
      ERROR_NS_OK("setCallWaiting Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetBarringPassword(int32_t serial,
                                const nsAString& facility,
                                const nsAString& oldPwd,
                                const nsAString& newPwd)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_CHANGE_BARRING_PASSWORD facility = %s, "
    "oldPwd = %s, newPwd = %s",
    serial,
    NS_ConvertUTF16toUTF8(facility).get(),
    NS_ConvertUTF16toUTF8(oldPwd).get(),
    NS_ConvertUTF16toUTF8(newPwd).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetBarringPassword(
      serial, facility, oldPwd, newPwd);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->setBarringPassword(serial,
                                      NS_ConvertUTF16toUTF8(facility).get(),
                                      NS_ConvertUTF16toUTF8(oldPwd).get(),
                                      NS_ConvertUTF16toUTF8(newPwd).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("setBarringPassword Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SeparateConnection(int32_t serial, int32_t gsmIndex)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEPARATE_CONNECTION gsmIndex = %d",
        serial,
        gsmIndex);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SeparateConnection(serial, gsmIndex);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->separateConnection(serial, gsmIndex);

    if (!ret.isOk()) {
      ERROR_NS_OK("separateConnection Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetClip(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_CLIP", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetClip(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getClip(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getClip Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ExplicitCallTransfer(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_EXPLICIT_CALL_TRANSFER", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ExplicitCallTransfer(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->explicitCallTransfer(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("explicitCallTransfer Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetNeighboringCids(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_NEIGHBORING_CELL_IDS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetNeighboringCids(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getNeighboringCids(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getNeighboringCids Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetTTYMode(int32_t serial, int32_t ttyMode)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_TTY_MODE ttyMode = %d",
        serial,
        ttyMode);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetTTYMode(serial, ttyMode);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setTTYMode(serial, TtyMode_V1_0(ttyMode));

    if (!ret.isOk()) {
      ERROR_NS_OK("setTTYMode Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::QueryTTYMode(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_QUERY_TTY_MODE ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->QueryTTYMode(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getTTYMode(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getTTYMode Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ExitEmergencyCallbackMode(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE ",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ExitEmergencyCallbackMode(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->exitEmergencyCallbackMode(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("exitEmergencyCallbackMode Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SupplyIccPinForApp(int32_t serial,
                                const nsAString& pin,
                                const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PIN pin = %s , aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(pin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SupplyIccPinForApp(serial, pin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->supplyIccPinForApp(serial,
                                      NS_ConvertUTF16toUTF8(pin).get(),
                                      NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("supplyIccPinForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SupplyIccPin2ForApp(int32_t serial,
                                 const nsAString& pin,
                                 const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PIN2 pin = %s , aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(pin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SupplyIccPin2ForApp(serial, pin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->supplyIccPin2ForApp(serial,
                                       NS_ConvertUTF16toUTF8(pin).get(),
                                       NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("supplyIccPin2ForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SupplyIccPukForApp(int32_t serial,
                                const nsAString& puk,
                                const nsAString& newPin,
                                const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PUK puk = %s , newPin = %s "
        ",aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(puk).get(),
        NS_ConvertUTF16toUTF8(newPin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SupplyIccPukForApp(serial, puk, newPin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->supplyIccPukForApp(serial,
                                      NS_ConvertUTF16toUTF8(puk).get(),
                                      NS_ConvertUTF16toUTF8(newPin).get(),
                                      NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("supplyIccPukForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SupplyIccPuk2ForApp(int32_t serial,
                                 const nsAString& puk,
                                 const nsAString& newPin,
                                 const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_PUK2 puk = %s , newPin = %s "
        ",aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(puk).get(),
        NS_ConvertUTF16toUTF8(newPin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SupplyIccPuk2ForApp(serial, puk, newPin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->supplyIccPuk2ForApp(serial,
                                       NS_ConvertUTF16toUTF8(puk).get(),
                                       NS_ConvertUTF16toUTF8(newPin).get(),
                                       NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("supplyIccPuk2ForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetFacilityLockForApp(int32_t serial,
                                   const nsAString& facility,
                                   bool lockState,
                                   const nsAString& password,
                                   int32_t serviceClass,
                                   const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_FACILITY_LOCK ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetFacilityLockForApp(
      serial, facility, lockState, password, serviceClass, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->setFacilityLockForApp(serial,
                                         NS_ConvertUTF16toUTF8(facility).get(),
                                         lockState,
                                         NS_ConvertUTF16toUTF8(password).get(),
                                         serviceClass,
                                         NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("setFacilityLockForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetFacilityLockForApp(int32_t serial,
                                   const nsAString& facility,
                                   const nsAString& password,
                                   int32_t serviceClass,
                                   const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_FACILITY_LOCK ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetFacilityLockForApp(
      serial, facility, password, serviceClass, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->getFacilityLockForApp(serial,
                                         NS_ConvertUTF16toUTF8(facility).get(),
                                         NS_ConvertUTF16toUTF8(password).get(),
                                         serviceClass,
                                         NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("getFacilityLockForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ChangeIccPinForApp(int32_t serial,
                                const nsAString& oldPin,
                                const nsAString& newPin,
                                const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CHANGE_SIM_PIN oldPin = %s , newPin = "
        "%s ,aid = %s",
        serial,
        NS_ConvertUTF16toUTF8(oldPin).get(),
        NS_ConvertUTF16toUTF8(newPin).get(),
        NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ChangeIccPinForApp(serial, oldPin, newPin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->changeIccPinForApp(serial,
                                      NS_ConvertUTF16toUTF8(oldPin).get(),
                                      NS_ConvertUTF16toUTF8(newPin).get(),
                                      NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("changeIccPinForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ChangeIccPin2ForApp(int32_t serial,
                                 const nsAString& oldPin,
                                 const nsAString& newPin,
                                 const nsAString& aid)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_CHANGE_SIM_PIN2 oldPin = %s , newPin = "
    "%s ,aid = %s",
    serial,
    NS_ConvertUTF16toUTF8(oldPin).get(),
    NS_ConvertUTF16toUTF8(newPin).get(),
    NS_ConvertUTF16toUTF8(aid).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ChangeIccPin2ForApp(serial, oldPin, newPin, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->changeIccPin2ForApp(serial,
                                       NS_ConvertUTF16toUTF8(oldPin).get(),
                                       NS_ConvertUTF16toUTF8(newPin).get(),
                                       NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("changeIccPin2ForApp Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::ReportStkServiceIsRunning(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING ",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ReportStkServiceIsRunning(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->reportStkServiceIsRunning(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("reportStkServiceIsRunning Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetGsmBroadcastActivation(int32_t serial, bool activate)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GSM_BROADCAST_ACTIVATION ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetGsmBroadcastActivation(serial, activate);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setGsmBroadcastActivation(serial, activate);

    if (!ret.isOk()) {
      ERROR_NS_OK("setGsmBroadcastActivation Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetGsmBroadcastConfig(int32_t serial,
                                   const nsTArray<int32_t>& ranges)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GSM_SET_BROADCAST_CONFIG ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetGsmBroadcastConfig(serial, ranges);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    std::vector<GsmBroadcastSmsConfigInfo_V1_0> broadcastInfo;
    for (uint32_t i = 0; i < ranges.Length();) {
      GsmBroadcastSmsConfigInfo_V1_0 info;
      // convert [from, to) to [from, to - 1]
      info.fromServiceId = ranges[i++];
      info.toServiceId = ranges[i++] - 1;
      info.fromCodeScheme = 0x00;
      info.toCodeScheme = 0xFF;
      info.selected = 1;
      broadcastInfo.push_back(info);
    }
    Return<void> ret =
      mRadioProxy->setGsmBroadcastConfig(serial, broadcastInfo);

    if (!ret.isOk()) {
      ERROR_NS_OK("setGsmBroadcastConfig Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetPreferredVoicePrivacy(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > "
        "RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE ",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetPreferredVoicePrivacy(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getPreferredVoicePrivacy(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getPreferredVoicePrivacy Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetPreferredVoicePrivacy(int32_t serial, bool enable)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE "
        "enable = %d",
        serial,
        enable);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetPreferredVoicePrivacy(serial, enable);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setPreferredVoicePrivacy(serial, enable);

    if (!ret.isOk()) {
      ERROR_NS_OK("setPreferredVoicePrivacy Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::RequestIccSimAuthentication(int32_t serial,
                                         int32_t authContext,
                                         const nsAString& data,
                                         const nsAString& aid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SIM_AUTHENTICATION ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->RequestIccSimAuthentication(
      serial, authContext, data, aid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->requestIccSimAuthentication(
      serial,
      authContext,
      NS_ConvertUTF16toUTF8(data).get(),
      NS_ConvertUTF16toUTF8(aid).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("requestIccSimAuthentication Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SendSMS(int32_t serial,
                     const nsAString& smsc,
                     const nsAString& pdu)
{
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendSMS(serial, smsc, pdu);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    GsmSmsMessage_V1_0 smsMessage;
    smsMessage.smscPdu = NS_ConvertUTF16toUTF8(smsc).get();
    smsMessage.pdu = NS_ConvertUTF16toUTF8(pdu).get();
    DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SEND_SMS %s",
          serial,
          NS_ConvertUTF16toUTF8(pdu).get());
    Return<void> ret = mRadioProxy->sendSms(serial, smsMessage);

    if (!ret.isOk()) {
      ERROR_NS_OK("sendSms Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::AcknowledgeLastIncomingGsmSms(int32_t serial,
                                           bool success,
                                           int32_t cause)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SMS_ACKNOWLEDGE ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->AcknowledgeLastIncomingGsmSms(
      serial, success, cause);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->acknowledgeLastIncomingGsmSms(
      serial, success, SmsAcknowledgeFailCause_V1_0(cause));

    if (!ret.isOk()) {
      ERROR_NS_OK("acknowledgeLastIncomingGsmSms Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetSuppServiceNotifications(int32_t serial, bool enable)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION "
        "enable = %d",
        serial,
        enable);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetSuppServiceNotifications(serial, enable);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setSuppServiceNotifications(serial, enable);

    if (!ret.isOk()) {
      ERROR_NS_OK("setSuppServiceNotifications Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::HandleStkCallSetupRequestFromSim(int32_t serial, bool accept)
{
  DEBUG("nsRilWorker: [%d] > "
        "RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM on = %d",
        serial,
        accept);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->HandleStkCallSetupRequestFromSim(serial, accept);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->handleStkCallSetupRequestFromSim(serial, accept);

    if (!ret.isOk()) {
      ERROR_NS_OK("handleStkCallSetupRequestFromSim Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SendTerminalResponseToSim(int32_t serial,
                                       const nsAString& contents)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE contents = "
        "%s",
        serial,
        NS_ConvertUTF16toUTF8(contents).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendTerminalResponseToSim(serial, contents);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->sendTerminalResponseToSim(
      serial, NS_ConvertUTF16toUTF8(contents).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("sendTerminalResponseToSim Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SendEnvelope(int32_t serial, const nsAString& contents)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND contents = "
        "%s",
        serial,
        NS_ConvertUTF16toUTF8(contents).get());
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendEnvelope(serial, contents);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret =
      mRadioProxy->sendEnvelope(serial, NS_ConvertUTF16toUTF8(contents).get());

    if (!ret.isOk()) {
      ERROR_NS_OK("sendEnvelope Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::GetRadioCapability(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_RADIO_CAPABILITY", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetRadioCapability(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->getRadioCapability(serial);

    if (!ret.isOk()) {
      ERROR_NS_OK("getRadioCapability Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetIndicationFilter(int32_t serial, int32_t filter)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetIndicationFilter(serial, filter);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      mRadioProxy->setIndicationFilter_1_5(serial, filter);
    } else if (mHalVerison >= HAL_VERSION_V1_2) {
      mRadioProxy->setIndicationFilter_1_2(serial, filter);
    } else {
      mRadioProxy->setIndicationFilter(serial, filter);
    }
  }
#else
  mRadioProxy->setIndicationFilter(serial, filter);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetUnsolResponseFilter(int32_t serial, int32_t filter)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetUnsolResponseFilter(serial, filter);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    Return<void> ret = mRadioProxy->setIndicationFilter(serial, filter);

    if (!ret.isOk()) {
      ERROR_NS_OK("setIndicationFilter Error.");
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetCarrierInfoForImsiEncryption(
  int32_t serial,
  const nsAString& mcc,
  const nsAString& mnc,
  const nsTArray<int32_t>& carrierKey,
  const nsAString& keyIdentifier,
  int32_t expirationTime,
  uint8_t publicKeyType)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_CARRIER_INFO_IMSI_ENCRYPTION",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetCarrierInfoForImsiEncryption(serial,
                                                               mcc,
                                                               mnc,
                                                               carrierKey,
                                                               keyIdentifier,
                                                               expirationTime,
                                                               publicKeyType);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_6) {
      ::android::hardware::radio::V1_6::ImsiEncryptionInfo halImsiInfo;
      halImsiInfo.base.mnc = NS_ConvertUTF16toUTF8(mcc).get();
      halImsiInfo.base.mcc = NS_ConvertUTF16toUTF8(mnc).get();
      halImsiInfo.base.keyIdentifier =
        NS_ConvertUTF16toUTF8(keyIdentifier).get();
      halImsiInfo.base.expirationTime = expirationTime;
      std::vector<uint8_t> keys;
      for (uint32_t i = 0; i < carrierKey.Length(); i++) {
        keys.push_back(carrierKey[i]);
      }
      halImsiInfo.base.carrierKey = keys;
      halImsiInfo.keyType = (PublicKeyType_V1_6)publicKeyType;

      if (mRadioProxy) {
        mRadioProxy->setCarrierInfoForImsiEncryption_1_6(serial, halImsiInfo);
      }
    } else if (mHalVerison >= HAL_VERSION_V1_1) {
#endif
      ::android::hardware::radio::V1_1::ImsiEncryptionInfo enInfo;
      enInfo.mcc = NS_ConvertUTF16toUTF8(mcc).get();
      enInfo.mnc = NS_ConvertUTF16toUTF8(mnc).get();
      std::vector<uint8_t> keys;
      for (uint32_t i = 0; i < carrierKey.Length(); i++) {
        keys.push_back(carrierKey[i]);
      }
      enInfo.carrierKey = keys;
      enInfo.keyIdentifier = NS_ConvertUTF16toUTF8(keyIdentifier).get();
      enInfo.expirationTime = expirationTime;

      if (mRadioProxy) {
        mRadioProxy->setCarrierInfoForImsiEncryption(serial, enInfo);
      }
#if ANDROID_VERSION >= 33
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::StartNetworkScan(int32_t serial, nsINetworkScanRequest* request)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_START_NETWORK_SCAN", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StartNetworkScan(serial, request);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      NetworkScanRequest_V1_5 scanRequest;
      int32_t nsType;
      request->GetType(&nsType);
      scanRequest.type = ScanType(nsType);

      int32_t nsInterval;
      request->GetInterval(&nsInterval);
      scanRequest.interval = nsInterval;

      int32_t nsMaxSearchTime;
      request->GetMaxSearchTime(&nsMaxSearchTime);
      scanRequest.maxSearchTime = nsMaxSearchTime;

      int32_t nsIncrementalResultsPeriodicity;
      request->GetIncrementalResultsPeriodicity(
        &nsIncrementalResultsPeriodicity);
      scanRequest.incrementalResultsPeriodicity =
        nsIncrementalResultsPeriodicity;

      bool nsIncrementalResults;
      request->GetIncrementalResults(&nsIncrementalResults);
      scanRequest.incrementalResults = nsIncrementalResults;

      DEBUG("StartNetworkScan nsType = %d, nsInterval = %d,"
            "nsMaxSearchTime = %d, nsIncrementalResultsPeriodicity = %d, "
            "nsIncrementalResults = %d",
            nsType,
            nsInterval,
            nsMaxSearchTime,
            nsIncrementalResultsPeriodicity,
            nsIncrementalResults);
      std::vector<hidl_string> mccMncs;
      nsTArray<nsString> nsMccMncs;
      request->GetMccMncs(nsMccMncs);
      if (nsMccMncs.IsEmpty()) {
        DEBUG("nsMccMncs is null");
      }

      for (uint32_t i = 0; i < nsMccMncs.Length(); i++) {
        DEBUG("StartNetworkScan nsMccMncs[%d] = %s",
              i,
              NS_ConvertUTF16toUTF8(nsMccMncs[i]).get());
        hidl_string mccmnc = NS_ConvertUTF16toUTF8(nsMccMncs[i]).get();
        mccMncs.push_back(mccmnc);
      }
      scanRequest.mccMncs = mccMncs;

      std::vector<RadioAccessSpecifier_V1_5> specifiers;
      nsTArray<RefPtr<nsIRadioAccessSpecifier>> nsSpecifiers;
      request->GetSpecifiers(nsSpecifiers);
      if (nsSpecifiers.IsEmpty()) {
        DEBUG("nsSpecifiers is null");
      }

      for (uint32_t i = 0; i < nsSpecifiers.Length(); i++) {
        if (nsSpecifiers[i] == nullptr) {
          DEBUG("nsSpecifiers[%d] is null", i);
        } else {
          RadioAccessSpecifier_V1_5 specifier =
            convertToHalRadioAccessSpecifier_1_5(nsSpecifiers[i]);
          specifiers.push_back(specifier);
        }
      }
      scanRequest.specifiers = specifiers;
      DEBUG("StartNetworkScan end");
      mRadioProxy->startNetworkScan_1_5(serial, scanRequest);
    } else if (mHalVerison >= HAL_VERSION_V1_2) {

      NetworkScanRequest_V1_2 scanRequest;
      int32_t nsType;
      request->GetType(&nsType);
      scanRequest.type = ScanType(nsType);

      int32_t nsInterval;
      request->GetInterval(&nsInterval);
      scanRequest.interval = nsInterval;

      std::vector<RadioAccessSpecifier_V1_1> specifiers;
      nsTArray<RefPtr<nsIRadioAccessSpecifier>> nsSpecifiers;
      request->GetSpecifiers(nsSpecifiers);
      for (uint32_t i = 0; i < nsSpecifiers.Length(); i++) {
        RadioAccessSpecifier_V1_1 specifier =
          convertToHalRadioAccessSpecifier(nsSpecifiers[i]);
        scanRequest.specifiers[i] = specifier;
      }
      int32_t nsMaxSearchTime;
      request->GetMaxSearchTime(&nsMaxSearchTime);
      scanRequest.maxSearchTime = nsMaxSearchTime;

      bool nsIncrementalResults;
      request->GetIncrementalResults(&nsIncrementalResults);
      scanRequest.incrementalResults = nsIncrementalResults;

      int32_t nsIncrementalResultsPeriodicity;
      request->GetIncrementalResultsPeriodicity(
        &nsIncrementalResultsPeriodicity);
      scanRequest.incrementalResultsPeriodicity =
        nsIncrementalResultsPeriodicity;

      std::vector<hidl_string> mccMncs;
      nsTArray<nsString> nsMccMncs;
      request->GetMccMncs(nsMccMncs);
      for (uint32_t i = 0; i < nsMccMncs.Length(); i++) {
        hidl_string mccmnc = NS_ConvertUTF16toUTF8(nsMccMncs[i]).get();
        mccMncs.push_back(mccmnc);
      }
      scanRequest.mccMncs = mccMncs;

      if (mHalVerison >= HAL_VERSION_V1_4) {
        mRadioProxy->startNetworkScan_1_4(serial, scanRequest);
      } else {
        mRadioProxy->startNetworkScan_1_2(serial, scanRequest);
      }
    } else if (mHalVerison >= HAL_VERSION_V1_1) {
#endif
      NetworkScanRequest_V1_1 scanRequest;

      int32_t nsType;
      request->GetType(&nsType);
      scanRequest.type = ScanType(nsType);

      int32_t nsInterval;
      request->GetInterval(&nsInterval);
      scanRequest.interval = nsInterval;

      std::vector<RadioAccessSpecifier_V1_1> specifiers;
      nsTArray<RefPtr<nsIRadioAccessSpecifier>> nsSpecifiers;
      request->GetSpecifiers(nsSpecifiers);
      for (uint32_t i = 0; i < nsSpecifiers.Length(); i++) {
        RadioAccessSpecifier_V1_1 specifier =
          convertToHalRadioAccessSpecifier(nsSpecifiers[i]);
        specifiers.push_back(specifier);
      }
      scanRequest.specifiers = specifiers;
      mRadioProxy->startNetworkScan(serial, scanRequest);
#if ANDROID_VERSION >= 33
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::StopNetworkScan(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_STOP_NETWORK_SCAN", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StopNetworkScan(serial);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_1) {
      mRadioProxy->stopNetworkScan(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
#if ANDROID_VERSION >= 33
  }
#endif
}

NS_IMETHODIMP
nsRilWorker::StartNattKeepalive(int32_t serial,
                                int32_t type,
                                const nsAString& sourceAddress,
                                int32_t sourcePort,
                                const nsAString& destinationAddress,
                                int32_t destinationPort,
                                int32_t maxKeepaliveIntervalMillis,
                                int32_t cid)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_START_KEEPALIVE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StartNattKeepalive(serial,
                                                  type,
                                                  sourceAddress,
                                                  sourcePort,
                                                  destinationAddress,
                                                  destinationPort,
                                                  maxKeepaliveIntervalMillis,
                                                  cid);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_1) {
      KeepaliveRequest_V1_1 req;

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

      DEBUG("nsRilWorker: [%d] > RIL_REQUEST_START_KEEPALIVE ", serial);
      if (mRadioProxy) {
        mRadioProxy->startKeepalive(serial, req);
      }
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
#if ANDROID_VERSION >= 33
  }
#endif
}

NS_IMETHODIMP
nsRilWorker::StopNattKeepalive(int32_t serial, int32_t sessionHandle)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_STOP_KEEPALIVE ", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StopNattKeepalive(serial, sessionHandle);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_1) {
      mRadioProxy->stopKeepalive(serial, sessionHandle);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
#if ANDROID_VERSION >= 33
  }
#endif
}

NS_IMETHODIMP
nsRilWorker::SetSimCardPower(int32_t serial, int32_t cardPowerState)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_SIM_CARD_POWER cardPowerState = %d",
    serial,
    cardPowerState);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetSimCardPower(serial, cardPowerState);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_1) {
      mRadioProxy->setSimCardPower_1_1(
        serial,
        ::android::hardware::radio::V1_1::CardPowerState(cardPowerState));
    } else {
      bool enable = (cardPowerState == 1) ? true : false;
      mRadioProxy->setSimCardPower(serial, enable);
    }
#if ANDROID_VERSION >= 33
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetSignalStrengthReportingCriteria(
  int32_t serial,
  uint32_t mrType,
  bool isEnabled,
  int32_t hysteresisMs,
  int32_t hysteresisDb,
  const nsTArray<int32_t>& thresholdsDbm,
  int32_t accessNetwork)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_SIGNALSTRENGTH_REPORTING_CRITERIAL",
    serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetSignalStrengthReportingCriteria(
      serial,
      mrType,
      isEnabled,
      hysteresisMs,
      hysteresisDb,
      thresholdsDbm,
      accessNetwork);
  } else {
#endif
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    std::vector<int32_t> dbms;
    for (uint32_t i = 0; i < thresholdsDbm.Length(); i++) {
      dbms.push_back(thresholdsDbm[i]);
    }
#if ANDROID_VERSION >= 33
    if (mHalVerison >= HAL_VERSION_V1_5) {
      SignalThresholdInfo_V1_5 signalThresholdInfo;
      signalThresholdInfo.signalMeasurement =
        (::android::hardware::radio::V1_5::SignalMeasurementType)mrType;
      signalThresholdInfo.hysteresisMs = hysteresisMs;
      signalThresholdInfo.hysteresisDb = hysteresisDb;
      signalThresholdInfo.thresholds = dbms;
      signalThresholdInfo.isEnabled = isEnabled;

      mRadioProxy->setSignalStrengthReportingCriteria_1_5(
        serial, signalThresholdInfo, (AccessNetwork_V1_5)accessNetwork);
      return NS_OK;
    } else if (mHalVerison >= HAL_VERSION_V1_2) {
      mRadioProxy->setSignalStrengthReportingCriteria(
        serial,
        hysteresisMs,
        hysteresisDb,
        dbms,
        AccessNetwork_V1_2(accessNetwork));
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SetLinkCapacityReportingCriteria(
  int32_t serial,
  int32_t hysteresisMs,
  int32_t hysteresisDlKbps,
  int32_t hysteresisUlKbps,
  const nsTArray<int32_t>& thresholdsDownlinkKbps,
  const nsTArray<int32_t>& thresholdsUplinkKbps,
  int32_t accessNetwork)
{

  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_LINK_CAPACITY_REPORTING_CRITERIAL",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetLinkCapacityReportingCriteria(
      serial,
      hysteresisMs,
      hysteresisDlKbps,
      hysteresisUlKbps,
      thresholdsDownlinkKbps,
      thresholdsUplinkKbps,
      accessNetwork);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    std::vector<int32_t> downLinkKbps;
    for (uint32_t i = 0; i < thresholdsDownlinkKbps.Length(); i++) {
      downLinkKbps.push_back(thresholdsDownlinkKbps[i]);
    }

    std::vector<int32_t> uplinkKbps;
    for (uint32_t i = 0; i < thresholdsUplinkKbps.Length(); i++) {
      uplinkKbps.push_back(thresholdsUplinkKbps[i]);
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {

      mRadioProxy->setLinkCapacityReportingCriteria_1_5(
        serial,
        hysteresisMs,
        hysteresisDlKbps,
        hysteresisUlKbps,
        downLinkKbps,
        uplinkKbps,
        AccessNetwork_V1_5(accessNetwork));

    } else if (mHalVerison >= HAL_VERSION_V1_2) {
      mRadioProxy->setLinkCapacityReportingCriteria(
        serial,
        hysteresisMs,
        hysteresisDlKbps,
        hysteresisUlKbps,
        downLinkKbps,
        uplinkKbps,
        AccessNetwork_V1_2(accessNetwork));
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsRilWorker::SetSystemSelectionChannels(
  int32_t serial,
  bool specifyChannels,
  const nsTArray<RefPtr<nsIRadioAccessSpecifier>>& aSpecifiers)
{

  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_SYSTEM_SELECTION_CHANNELS",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetSystemSelectionChannels(
      serial, specifyChannels, aSpecifiers);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {
      ::std::vector<RadioAccessSpecifier_V1_5> specifiers;
      for (uint32_t i = 0; i < aSpecifiers.Length(); i++) {
        RadioAccessSpecifier_V1_5 specifier =
          convertToHalRadioAccessSpecifier_1_5(aSpecifiers[i]);
        specifiers.push_back(specifier);
      }
      mRadioProxy->setSystemSelectionChannels_1_5(
        serial, specifyChannels, specifiers);

    } else if (mHalVerison >= HAL_VERSION_V1_3) {
      ::std::vector<RadioAccessSpecifier> specifiers;
      for (uint32_t i = 0; i < aSpecifiers.Length(); i++) {
        RadioAccessSpecifier_V1_1 specifier =
          convertToHalRadioAccessSpecifier(aSpecifiers[i]);
        specifiers.push_back(specifier);
      }
      mRadioProxy->setSystemSelectionChannels(
        serial, specifyChannels, specifiers);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::EnableModem(int32_t serial, bool on)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_ENABLE_MODEM enabled = %d", serial, on);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->EnableModem(serial, on);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_3) {
      mRadioProxy->enableModem(serial, on);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetModemStackStatus(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_MODEM_STACK_STATUS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetModemStackStatus(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_3) {
      mRadioProxy->getModemStackStatus(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::EmergencyDial(int32_t serial,
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
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_EMERGENCY_DIAL", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->EmergencyDial(serial,
                                             address,
                                             clirMode,
                                             uusType,
                                             uusDcs,
                                             uusData,
                                             categories,
                                             aUrns,
                                             routMode,
                                             hasKnownUserIntentEmergency,
                                             isTesting);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_4) {
      UusInfo_V1_0 info;
      std::vector<UusInfo_V1_0> uusInfo;
      info.uusType = UusType_V1_0(uusType);
      info.uusDcs = UusDcs_V1_0(uusDcs);
      if (uusData.Length() == 0) {
        info.uusData = NULL;
      } else {
        info.uusData = NS_ConvertUTF16toUTF8(uusData).get();
      }
      uusInfo.push_back(info);

      Dial_V1_0 dialInfo;
      dialInfo.address = NS_ConvertUTF16toUTF8(address).get();
      dialInfo.clir = Clir_V1_0(clirMode);
      dialInfo.uusInfo = uusInfo;

      hidl_vec<hidl_string> urns;
      urns.resize(aUrns.Length());
      for (uint32_t i = 0; i < aUrns.Length(); i++) {
        urns[i] = NS_ConvertUTF16toUTF8(aUrns[i]).get();
      }
      if (mHalVerison >= HAL_VERSION_V1_6) {
        mRadioProxy->emergencyDial_1_6(serial,
                                       dialInfo,
                                       categories,
                                       urns,
                                       (EmergencyCallRouting_V1_4)routMode,
                                       hasKnownUserIntentEmergency,
                                       isTesting);
      } else {
        mRadioProxy->emergencyDial(serial,
                                   dialInfo,
                                   categories,
                                   urns,
                                   (EmergencyCallRouting_V1_4)routMode,
                                   hasKnownUserIntentEmergency,
                                   isTesting);
      }
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SetNrDualConnectivityState(int32_t serial,
                                        int32_t nrDualConnectivityState)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENABLE_NR_DUAL_CONNECTIVITY", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetNrDualConnectivityState(
      serial, nrDualConnectivityState);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->setNrDualConnectivityState(
        serial, (NrDualConnectivityState_V1_6)nrDualConnectivityState);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::IsNrDualConnectivityEnabled(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_IS_NR_DUAL_CONNECTIVITY_ENABLED",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->IsNrDualConnectivityEnabled(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->isNrDualConnectivityEnabled(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::AllocatePduSessionId(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ALLOCATE_PDU_SESSION_ID", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->AllocatePduSessionId(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->allocatePduSessionId(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::ReleasePduSessionId(int32_t serial, int32_t id)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_RELEASE_PDU_SESSION_ID", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->ReleasePduSessionId(serial, id);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->releasePduSessionId(serial, id);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::StartHandover(int32_t serial, int32_t callId)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_START_HANDOVER", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->StartHandover(serial, callId);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->startHandover(serial, callId);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::CancelHandover(int32_t serial, int32_t callId)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CANCEL_HANDOVER", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->CancelHandover(serial, callId);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->cancelHandover(serial, callId);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetSimPhonebookCapacity(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SIM_PHONEBOOK_CAPACITY", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetSimPhonebookCapacity(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->getSimPhonebookCapacity(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetSlicingConfig(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SLICING_CONFIG", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetSlicingConfig(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->getSlicingConfig(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetSystemSelectionChannels(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_SYSTEM_SELECTION_CHANNELS",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetSystemSelectionChannels(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->getSystemSelectionChannels(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetAllowedNetworkTypesBitmap(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetAllowedNetworkTypesBitmap(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->getAllowedNetworkTypesBitmap(serial);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
  return NS_OK;
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::EnableUiccApplications(int32_t serial, bool enable)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENABLE_UICC_APPLICATIONS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->EnableUiccApplications(serial, enable);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {
      mRadioProxy->enableUiccApplications(serial, enable);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

int
convertToHalRadioAccessFamily(int networkTypeBitmask)
{
  int raf = 0;
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_GSM) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::GSM;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_GPRS) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::GPRS;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_EDGE) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::EDGE;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_CDMA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::IS95A;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_1xRTT) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::ONE_X_RTT;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_EVDO_0) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_0;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_EVDO_A) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_A;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_EVDO_B) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::EVDO_B;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_EHRPD) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::EHRPD;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_HSUPA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::HSUPA;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_HSDPA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::HSDPA;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_HSPA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::HSPA;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_HSPAP) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::HSPAP;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_UMTS) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::UMTS;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_TD_SCDMA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::TD_SCDMA;
  }
#if ANDROID_VERSION >= 33
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_IWLAN) != 0) {
    raf |=
      (1 << ((int)::android::hardware::radio::V1_4::RadioTechnology::IWLAN));
  }
#endif
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_LTE) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::LTE;
  }
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_LTE_CA) != 0) {
    raf |= ::android::hardware::radio::V1_0::RadioAccessFamily::LTE_CA;
  }
#if ANDROID_VERSION >= 33
  if ((networkTypeBitmask & NETWORK_TYPE_BITMASK_NR) != 0) {
    raf |= ::android::hardware::radio::V1_4::RadioAccessFamily::NR;
  }
  return (raf == 0)
           ? (int)::android::hardware::radio::V1_4::RadioAccessFamily::UNKNOWN
           : raf;
#else
  return (raf == 0)
           ? (int)::android::hardware::radio::V1_0::RadioAccessFamily::UNKNOWN
           : raf;
#endif
}

NS_IMETHODIMP
nsRilWorker::SetAllowedNetworkTypesBitmap(int32_t serial, int32_t bitmap)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_ALLOWED_NETWORK_TYPES_BITMAP",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetAllowedNetworkTypesBitmap(serial, bitmap);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->setAllowedNetworkTypesBitmap(
        serial, convertToHalRadioAccessFamily(bitmap));
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::UpdateSimPhonebookRecords(int32_t serial,
                                       nsIPhonebookRecordInfo* info)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_UPDATE_SIM_PHONEBOOK_RECORD", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->UpdateSimPhonebookRecords(serial, info);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      PhonebookRecordInfo_V1_6 aMessage;
      ((nsPhonebookRecordInfo*)info)->updateToDestion(aMessage);
      mRadioProxy->updateSimPhonebookRecords(serial, aMessage);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::AreUiccApplicationsEnabled(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_UICC_APPLICATIONS_ENABLEMENT",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->AreUiccApplicationsEnabled(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {
      mRadioProxy->areUiccApplicationsEnabled(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SupplySimDepersonalization(int32_t serial,
                                        int32_t persoType,
                                        const nsAString& controlKey)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_ENTER_SIM_DEPERSONALIZATION", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SupplySimDepersonalization(
      serial, persoType, controlKey);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {
      mRadioProxy->supplySimDepersonalization(
        serial,
        (PersoSubstate_V1_5)persoType,
        NS_ConvertUTF16toUTF8(controlKey).get());
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetBarringInfo(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_BARRING_INFO", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetBarringInfo(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_5) {
      mRadioProxy->getBarringInfo(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SendCdmaSmsExpectMore(int32_t serial, nsICdmaSmsMessage* aMessage)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_CDMA_SEND_SMS_EXPECT_MORE", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SendCdmaSmsExpectMore(serial, aMessage);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }

    CdmaSmsMessage_V1_1 cdmaSmsMessage;
    ((nsCdmaSmsMessage*)aMessage)->updateToDestion(cdmaSmsMessage);

    if (mHalVerison > HAL_VERSION_V1_5) {
      mRadioProxy->sendCdmaSmsExpectMore_1_6(serial, cdmaSmsMessage);
      return NS_OK;
    } else if (mHalVerison == HAL_VERSION_V1_5) {
      mRadioProxy->sendCdmaSmsExpectMore(serial, cdmaSmsMessage);
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetPreferredNetworkTypeBitmap(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_PREFEERED_NETWORK_TYPE_BITMAP",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetPreferredNetworkTypeBitmap(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_4) {
      mRadioProxy->getPreferredNetworkTypeBitmap(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SetPreferredNetworkTypeBitmap(int32_t serial,
                                           uint32_t networkTypeBitmap)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_PREFEERED_NETWORK_TYPE_BITMAP",
        serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetAllowedNetworkTypesBitmap(serial,
                                                            networkTypeBitmap);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_6) {
      mRadioProxy->setAllowedNetworkTypesBitmap(
        serial, convertToHalRadioAccessFamily(networkTypeBitmap));
      return NS_OK;
    } else if (mHalVerison >= HAL_VERSION_V1_4) {
      mRadioProxy->setPreferredNetworkTypeBitmap(
        serial, (RadioAccessFamily_BF_V1_4)networkTypeBitmap);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SetAllowedCarriers(int32_t serial,
                                nsICarrierRestrictionsWithPriority* carriers,
                                uint32_t multiSimPolicy)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_SET_ALLOWED_CARRIERS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->SetAllowedCarriers(
      serial, carriers, multiSimPolicy);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_4) {
      CarrierRestrictionsWithPriority_V1_4 crp;
      nsTArray<RefPtr<nsICarrier>> allowedCarriers;
      carriers->GetAllowedCarriers(allowedCarriers);
      crp.allowedCarriers.resize(allowedCarriers.Length());
      for (uint32_t i = 0; i < allowedCarriers.Length(); i++) {
        Carrier_V1_4 cr;
        nsString mcc;
        allowedCarriers[i]->GetMcc(mcc);
        cr.mcc = NS_ConvertUTF16toUTF8(mcc).get();

        nsString mnc;
        allowedCarriers[i]->GetMnc(mnc);
        cr.mnc = NS_ConvertUTF16toUTF8(mnc).get();
        uint8_t matchType;
        allowedCarriers[i]->GetMatchType(&matchType);
        cr.matchType = (CarrierMatchType_V1_4)matchType;

        nsString matchData;
        allowedCarriers[i]->GetMatchData(matchData);
        cr.matchData = NS_ConvertUTF16toUTF8(matchData).get();

        crp.allowedCarriers[i] = cr;
      }

      nsTArray<RefPtr<nsICarrier>> excludedCarriers;
      carriers->GetExcludedCarriers(excludedCarriers);
      crp.excludedCarriers.resize(excludedCarriers.Length());
      for (uint32_t i = 0; i < excludedCarriers.Length(); i++) {
        Carrier_V1_4 cr;
        nsString mcc;
        excludedCarriers[i]->GetMcc(mcc);
        cr.mcc = NS_ConvertUTF16toUTF8(mcc).get();

        nsString mnc;
        excludedCarriers[i]->GetMnc(mnc);
        cr.mnc = NS_ConvertUTF16toUTF8(mnc).get();

        uint8_t matchType;
        excludedCarriers[i]->GetMatchType(&matchType);
        cr.matchType = (CarrierMatchType_V1_4)matchType;

        nsString matchData;
        excludedCarriers[i]->GetMatchData(matchData);
        cr.matchData = NS_ConvertUTF16toUTF8(matchData).get();

        crp.excludedCarriers[i] = cr;
      }

      bool allowedCarriersPrioritized;
      carriers->GetAllowedCarriersPrioritized(&allowedCarriersPrioritized);
      crp.allowedCarriersPrioritized = allowedCarriersPrioritized;

      mRadioProxy->setAllowedCarriers_1_4(
        serial, crp, SimLockMultiSimPolicy_V1_4(multiSimPolicy));
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::GetAllowedCarriers(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_GET_ALLOWED_CARRIERS", serial);
#if ANDROID_VERSION >= 33
  if (mHalVerison >= AIDL_VERSION) {
    getRadioServiceProxy();
    if (mRadioProxyServiceManager == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    mRadioProxyServiceManager->GetAllowedCarriers(serial);
  } else {
    GetRadioProxy();
    if (mRadioProxy == nullptr) {
      ERROR_NS_OK("No Radio HAL exist");
    }
    if (mHalVerison >= HAL_VERSION_V1_4) {
      mRadioProxy->getAllowedCarriers_1_4(serial);
      return NS_OK;
    } else {
      ERROR_NS_OK("Not support in current radio version");
    }
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::IsVoNrEnabled(int32_t serial)
{
  DEBUG("nsRilWorker: [%d] > RIL_REQUEST_IS_VONR_ENABLED", serial);
#if ANDROID_VERSION >= 33
  getRadioServiceProxy();
  if (mRadioProxyServiceManager == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  if (mHalVerison >= AIDL_VERSION) {
    mRadioProxyServiceManager->IsVoNrEnabled(serial);
    return NS_OK;
  } else {
    ERROR_NS_OK("Not support in current radio version");
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

NS_IMETHODIMP
nsRilWorker::SetVoNrEnabled(int32_t serial, bool enabled)
{
  DEBUG(
    "nsRilWorker: [%d] > RIL_REQUEST_SET_VONR_ENABLED (%d)", serial, enabled);
#if ANDROID_VERSION >= 33
  getRadioServiceProxy();
  if (mRadioProxyServiceManager == nullptr) {
    ERROR_NS_OK("No Radio HAL exist");
  }
  if (mHalVerison >= AIDL_VERSION) {
    mRadioProxyServiceManager->SetVoNrEnabled(serial, enabled);
    return NS_OK;
  } else {
    ERROR_NS_OK("Not support in current radio version");
  }
#else
  ERROR_NS_OK("Not support in current radio version");
#endif
}

nsRilWorker::~nsRilWorker()
{
  DEBUG("Destructor nsRilWorker");
  mRilResponse = nullptr;
  MOZ_ASSERT(!mRilResponse);
  mRilIndication = nullptr;
  MOZ_ASSERT(!mRilIndication);
  mRadioProxy = nullptr;
  MOZ_ASSERT(!mRadioProxy);
  mDeathRecipient = nullptr;
  MOZ_ASSERT(!mDeathRecipient);
}

void
nsRilWorker::RadioProxyDeathRecipient::serviceDied(uint64_t,
                                                   const ::android::wp<IBase>&)
{
  INFO("nsRilWorker HAL died, cleanup instance.");
}

#if ANDROID_VERSION >= 33
void
nsRilWorker::getRadioServiceProxy()
{
  if (mRadioProxyServiceManager != nullptr) {
    return;
  }
  mRadioProxyServiceManager =
    new nsRadioProxyServiceManager(mClientId + 1, this);
}
#endif

void
nsRilWorker::GetRadioProxy()
{
  if (mHalVerison < AIDL_VERSION) {
    if (mRadioProxy != nullptr) {
      return;
    }

    DEBUG("GetRadioProxy");

    if (!mRadioProxy) {
#if ANDROID_VERSION >= 33
      mRadioProxy = ::android::hardware::radio::V1_6::IRadio::getService(
        HIDL_SERVICE_NAME[mClientId]);
#else
      mRadioProxy = ::android::hardware::radio::V1_1::IRadio::getService(
        HIDL_SERVICE_NAME[mClientId]);
#endif
    }

    DEBUG("Radio hal version is %d", mHalVerison);
    if (mRadioProxy != nullptr) {
      if (mDeathRecipient == nullptr) {
        mDeathRecipient = new RadioProxyDeathRecipient();
      }
      if (mDeathRecipient != nullptr) {
        Return<bool> linked =
          mRadioProxy->linkToDeath(mDeathRecipient, 0 /*cookie*/);
        if (!linked || !linked.isOk()) {
          ERROR("Failed to link to radio hal death notifications");
        }
      }
      mRadioProxy->setResponseFunctions(mRilResponse, mRilIndication);
    } else {
      ERROR("Get Radio hal failed");
    }
  }
}

void
nsRilWorker::processIndication(RadioIndicationType_V1_0 indicationType)
{
  DEBUG("processIndication, type= %d", indicationType);
  if (indicationType == RadioIndicationType_V1_0::UNSOLICITED_ACK_EXP) {
    sendAck();
    DEBUG("Unsol response received; Sending ack to ril.cpp");
  } else {
    // ack is not expected to be sent back. Nothing is required to be done here.
  }
}

void
nsRilWorker::processResponse(RadioResponseType_V1_0 responseType)
{
  DEBUG("processResponse, type= %d", responseType);
  if (responseType == RadioResponseType_V1_0::SOLICITED_ACK_EXP) {
    sendAck();
    DEBUG("Solicited response received; Sending ack to ril.cpp");
  } else {
    // ack is not expected to be sent back. Nothing is required to be done here.
  }
}

void
nsRilWorker::sendAck()
{
  DEBUG("sendAck");
  GetRadioProxy();
  if (mRadioProxy != nullptr) {
    mRadioProxy->responseAcknowledgement();
  } else {
    ERROR("sendAck mRadioProxy == nullptr");
  }
}

void
nsRilWorker::sendRilIndicationResult(nsRilIndicationResult* aIndication)
{
  DEBUG("nsRilWorker: [USOL]< %s",
        NS_LossyConvertUTF16toASCII(aIndication->mRilMessageType).get());

  RefPtr<nsRilIndicationResult> indication = aIndication;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "nsRilWorker::sendRilIndicationResult", [this, indication]() {
      if (mRilCallback) {
        mRilCallback->HandleRilIndication(indication);
      } else {
        DEBUG("sendRilIndicationResult: no callback");
      }
    });
  NS_DispatchToMainThread(r);
}

void
nsRilWorker::sendRilResponseResult(nsRilResponseResult* aResponse)
{
  DEBUG("nsRilWorker: [%d] < %s",
        aResponse->mRilMessageToken,
        NS_LossyConvertUTF16toASCII(aResponse->mRilMessageType).get());

  if (aResponse->mRilMessageToken > 0) {
    RefPtr<nsRilResponseResult> response = aResponse;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "nsRilWorker::sendRilResponseResult", [this, response]() {
        if (mRilCallback) {
          mRilCallback->HandleRilResponse(response);
        } else {
          DEBUG("sendRilResponseResult: no callback");
        }
      });
    NS_DispatchToMainThread(r);
  } else {
    DEBUG("ResponseResult internal reqeust.");
  }
}

// Helper function
MvnoType_V1_0
nsRilWorker::convertToHalMvnoType(const nsAString& mvnoType)
{
  if (u"imsi"_ns.Equals(mvnoType)) {
    return MvnoType_V1_0::IMSI;
  } else if (u"gid"_ns.Equals(mvnoType)) {
    return MvnoType_V1_0::GID;
  } else if (u"spn"_ns.Equals(mvnoType)) {
    return MvnoType_V1_0::SPN;
  } else {
    return MvnoType_V1_0::NONE;
  }
}
#if ANDROID_VERSION >= 33
DataProfileInfo_V1_5
nsRilWorker::convertToHalDataProfile_V1_5(nsIDataProfile* profile)
{
  DataProfileInfo_V1_5 dataProfileInfo;

  int32_t profileId;
  profile->GetProfileId(&profileId);
  dataProfileInfo.profileId = (DataProfileId_V1_0)profileId;

  nsString apn;
  profile->GetApn(apn);
  dataProfileInfo.apn = NS_ConvertUTF16toUTF8(apn).get();

  int32_t protocol;
  profile->GetProtocol(&protocol);
  dataProfileInfo.protocol = (PdpProtocolType_V_4)protocol;

  int32_t roamingProtocol;
  profile->GetRoamingProtocol(&roamingProtocol);
  dataProfileInfo.roamingProtocol = (PdpProtocolType_V_4)roamingProtocol;

  int32_t authType;
  profile->GetAuthType(&authType);
  dataProfileInfo.authType = (ApnAuthType_V1_0)authType;

  nsString user;
  profile->GetUser(user);
  dataProfileInfo.user = NS_ConvertUTF16toUTF8(user).get();

  nsString password;
  profile->GetPassword(password);
  dataProfileInfo.password = NS_ConvertUTF16toUTF8(password).get();

  int32_t type;
  profile->GetType(&type);
  dataProfileInfo.type = (DataProfileInfoType_V1_0)type;

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
  dataProfileInfo.supportedApnTypesBitmap =
    (int32_t)ApnTypes(supportedApnTypesBitmap);

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

  return dataProfileInfo;
}

DataProfileInfo_V1_4
nsRilWorker::convertToHalDataProfile_V1_4(nsIDataProfile* profile)
{
  DataProfileInfo_V1_4 dataProfileInfo;
  int32_t profileId;
  profile->GetProfileId(&profileId);
  dataProfileInfo.profileId = DataProfileId_V1_0(profileId);

  nsString apn;
  profile->GetApn(apn);
  dataProfileInfo.apn = NS_ConvertUTF16toUTF8(apn).get();

  int32_t protocol;
  profile->GetProtocol(&protocol);
  dataProfileInfo.protocol = (PdpProtocolType_V_4)protocol;

  int32_t roamingProtocol;
  profile->GetRoamingProtocol(&roamingProtocol);
  dataProfileInfo.roamingProtocol = (PdpProtocolType_V_4)roamingProtocol;

  int32_t authType;
  profile->GetAuthType(&authType);
  dataProfileInfo.authType = (ApnAuthType_V1_0)authType;

  nsString user;
  profile->GetUser(user);
  dataProfileInfo.user = NS_ConvertUTF16toUTF8(user).get();

  nsString password;
  profile->GetPassword(password);
  dataProfileInfo.password = NS_ConvertUTF16toUTF8(password).get();

  int32_t type;
  profile->GetType(&type);
  dataProfileInfo.type = (DataProfileInfoType_V1_0)type;

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
  dataProfileInfo.supportedApnTypesBitmap =
    (int32_t)ApnTypes_V1_0(supportedApnTypesBitmap);

  int32_t bearerBitmap;
  profile->GetBearerBitmap(&bearerBitmap);
  dataProfileInfo.bearerBitmap =
    (int32_t)RadioAccessFamily_BF_V1_4(bearerBitmap);

  int32_t mtu;
  profile->GetMtu(
    &mtu); // According to Android, suggest to replace with GetMtuV4()
  dataProfileInfo.mtu = mtu;

  bool preferred;
  profile->GetPreferred(&preferred);
  dataProfileInfo.preferred = preferred;

  bool persistent;
  profile->GetPersistent(&persistent);
  dataProfileInfo.persistent = persistent;

  return dataProfileInfo;
}
#endif

DataProfileInfo_V1_0
nsRilWorker::convertToHalDataProfile(nsIDataProfile* profile)
{
  DataProfileInfo_V1_0 dataProfileInfo;
  // Mapping index to type string according to PdpProtocolType
  // in types.hal of raido1.4
  const char* PdpProtocolTypeMaps[7] = { "UNKNOWN",     "IP",  "IPV6",
                                         "IPV4V6",      "PPP", "NON_IP",
                                         "UNSTRUCTURED" };

  int32_t profileId;
  profile->GetProfileId(&profileId);
  dataProfileInfo.profileId = (DataProfileId_V1_0)profileId;

  nsString apn;
  profile->GetApn(apn);
  dataProfileInfo.apn = NS_ConvertUTF16toUTF8(apn).get();

  int32_t protocol;
  profile->GetProtocol(&protocol);
  dataProfileInfo.protocol = PdpProtocolTypeMaps[protocol - 1];

  int32_t roamingProtocol;
  profile->GetRoamingProtocol(&roamingProtocol);
  dataProfileInfo.roamingProtocol = PdpProtocolTypeMaps[roamingProtocol];

  int32_t authType;
  profile->GetAuthType(&authType);
  dataProfileInfo.authType = (ApnAuthType_V1_0)authType;

  nsString user;
  profile->GetUser(user);
  dataProfileInfo.user = NS_ConvertUTF16toUTF8(user).get();

  nsString password;
  profile->GetPassword(password);
  dataProfileInfo.password = NS_ConvertUTF16toUTF8(password).get();

  int32_t type;
  profile->GetType(&type);
  dataProfileInfo.type = (DataProfileInfoType_V1_0)type;

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
  dataProfileInfo.supportedApnTypesBitmap =
    (int32_t)ApnTypes_V1_0(supportedApnTypesBitmap);

  int32_t bearerBitmap;
  profile->GetBearerBitmap(&bearerBitmap);
  dataProfileInfo.bearerBitmap =
    (int32_t)RadioAccessFamily_BF_V1_0(bearerBitmap);

  int32_t mtu;
  profile->GetMtu(&mtu);
  dataProfileInfo.mtu = mtu;

  nsString mvnoType;
  profile->GetMvnoType(mvnoType);
  dataProfileInfo.mvnoType = convertToHalMvnoType(mvnoType);

  nsString mvnoMatchData;
  profile->GetMvnoMatchData(mvnoMatchData);
  dataProfileInfo.mvnoMatchData = NS_ConvertUTF16toUTF8(mvnoMatchData).get();

  return dataProfileInfo;
}

// Helper function
RadioAccessSpecifier_V1_1
nsRilWorker::convertToHalRadioAccessSpecifier(
  nsIRadioAccessSpecifier* specifier)
{

  RadioAccessSpecifier_V1_1 value;
  if (mHalVerison >= HAL_VERSION_V1_1 && mHalVerison < HAL_VERSION_V1_5) {
    int32_t nsRadioAccessNetwork;
    specifier->GetRadioAccessNetwork(&nsRadioAccessNetwork);
    value.radioAccessNetwork = RadioAccessNetworks_V1_1(nsRadioAccessNetwork);

    std::vector<GeranBands> geranBands;
    nsTArray<int32_t> nsGeranBands;
    specifier->GetGeranBands(nsGeranBands);
    for (uint32_t i = 0; i < nsGeranBands.Length(); i++) {
      geranBands.push_back(GeranBands(nsGeranBands[i]));
    }
    value.geranBands = geranBands;

    std::vector<UtranBands> utranBands;
    nsTArray<int32_t> nsUtranBands;
    specifier->GetUtranBands(nsUtranBands);
    for (uint32_t i = 0; i < nsUtranBands.Length(); i++) {
      utranBands.push_back(UtranBands(nsUtranBands[i]));
    }
    value.utranBands = utranBands;

    std::vector<EutranBands> eutranBands;
    nsTArray<int32_t> nsEutranBands;
    specifier->GetEutranBands(nsEutranBands);
    for (uint32_t i = 0; i < nsEutranBands.Length(); i++) {
      eutranBands.push_back(EutranBands(nsEutranBands[i]));
    }
    value.eutranBands = eutranBands;

    std::vector<int32_t> channels;
    nsTArray<int32_t> nsChannels;
    specifier->GetChannels(nsChannels);
    for (uint32_t i = 0; i < nsChannels.Length(); i++) {
      channels.push_back(nsChannels[i]);
    }
    value.channels = channels;
  } else {
    DEBUG("Not support in current radio version");
  }
  return value;
}

#if ANDROID_VERSION >= 33
LinkAddress_V1_5
nsRilWorker::convertToHalLinkAddress_V1_5(nsILinkAddress* domLinkAddress)
{
  LinkAddress_V1_5 linkAddress;

  nsString address;
  domLinkAddress->GetAddress(address);
  linkAddress.address = NS_ConvertUTF16toUTF8(address).get();

  int32_t properties;
  domLinkAddress->GetProperties(&properties);
  linkAddress.properties = (int32_t)AddressProperty_V1_5(properties);

  uint64_t deprecationTime;
  domLinkAddress->GetDeprecationTime(&deprecationTime);
  linkAddress.deprecationTime = deprecationTime;

  uint64_t expirationTime;
  domLinkAddress->GetExpirationTime(&expirationTime);
  linkAddress.expirationTime = expirationTime;

  return linkAddress;
}

OptionalSliceInfo_V1_6
nsRilWorker::convertToHalSliceInfo_V1_6(nsISliceInfo* domSliceinfo)
{
  OptionalSliceInfo_V1_6 opInfo;
  SliceInfo_V1_6 info;
  Monostate_V1_6 state;
  if (domSliceinfo == nullptr) {
    opInfo.noinit(state);
    return opInfo;
  }
  int32_t sst;
  domSliceinfo->GetSst(&sst);
  info.sst = SliceServiceType_V1_6(sst);

  int32_t sliceDifferentiator;
  domSliceinfo->GetSliceDifferentiator(&sliceDifferentiator);
  info.sliceDifferentiator = sliceDifferentiator;

  int32_t mappedHplmnSst;
  domSliceinfo->GetMappedHplmnSst(&mappedHplmnSst);
  info.mappedHplmnSst = SliceServiceType_V1_6(mappedHplmnSst);

  int32_t mappedHplmnSD;
  domSliceinfo->GetMappedHplmnSD(&mappedHplmnSD);
  info.mappedHplmnSD = mappedHplmnSD;

  int32_t status;
  domSliceinfo->GetStatus(&status);
  info.status = SliceStatus_V1_6(status);

  opInfo.value(info);
  return opInfo;
}

OptionalTrafficDescriptor_V1_6
nsRilWorker::convertToHalTrafficDescriptor_V1_6(
  nsITrafficDescriptor* domTrafficdes)
{
  OptionalTrafficDescriptor_V1_6 opTrafDes;
  TrafficDescriptor_V1_6 trafDes;
  OptionalDnn_V1_6 opDnn;
  OptionalOsAppId_V1_6 opOsAppId;
  OsAppId_V1_6 tempOsAppId;
  Monostate_V1_6 state;
  if (domTrafficdes == nullptr) {
    opTrafDes.noinit(state);
    return opTrafDes;
  }

  nsString dnn;
  domTrafficdes->GetDnn(dnn);
  if (dnn.IsEmpty()) {
    opDnn.noinit(state);
  } else {
    opDnn.value(NS_ConvertUTF16toUTF8(dnn).get());
  }
  trafDes.dnn = opDnn;

  std::vector<uint8_t> appid;
  nsTArray<int32_t> nsAppId;
  domTrafficdes->GetOsAppId(nsAppId);
  bool isPresent = false;
  for (uint32_t i = 0; i < nsAppId.Length(); i++) {
    if (nsAppId[i] != 0) {
      isPresent = true;
    }
    appid.push_back((uint8_t)nsAppId[i]);
  }
  if (!isPresent) {
    opOsAppId.noinit(state);
  } else {
    tempOsAppId.osAppId = appid;
    opOsAppId.value(tempOsAppId);
  }
  trafDes.osAppId = opOsAppId;

  opTrafDes.value(trafDes);
  return opTrafDes;
}

/**
 * Convert to RadioAccessNetwork defined in radio/1.1/types.hal
 * @param accessNetworkType Access network type
 * @return The converted RadioAccessNetwork
 */
static RadioAccessNetworks_V1_5
convertToHalRadioAccessNetworks(int accessNetworkType)
{
  switch (accessNetworkType) {
    case RadioAccessNetworkTypes::GERAN:
      return RadioAccessNetworks_V1_5::GERAN;
    case RadioAccessNetworkTypes::UTRAN:
      return RadioAccessNetworks_V1_5::UTRAN;
    case RadioAccessNetworkTypes::EUTRAN:
      return RadioAccessNetworks_V1_5::EUTRAN;
    case RadioAccessNetworkTypes::NGRAN:
      return RadioAccessNetworks_V1_5::NGRAN;
    case RadioAccessNetworkTypes::CDMA2000:
      return RadioAccessNetworks_V1_5::CDMA2000;
    case RadioAccessNetworkTypes::UNKNOWN:
    default:
      return RadioAccessNetworks_V1_5::UNKNOWN;
  }
}

RadioAccessSpecifier_V1_5
nsRilWorker::convertToHalRadioAccessSpecifier_1_5(
  nsIRadioAccessSpecifier* specifier)
{
  RadioAccessSpecifier_V1_5 rasInHalFormat;
  RadioAccessSpecifier_V1_5::Bands bandsInHalFormat;

  if (mHalVerison >= HAL_VERSION_V1_5) {

    int32_t nsRadioAccessNetwork;
    specifier->GetRadioAccessNetwork(&nsRadioAccessNetwork);
    DEBUG("convertToHalRadioAccessSpecifier_1_5 nsRadioAccessNetwork %d",
          nsRadioAccessNetwork);
    rasInHalFormat.radioAccessNetwork =
      RadioAccessNetworks_V1_5(nsRadioAccessNetwork);

    std::vector<GeranBands> geranBands;
    nsTArray<int32_t> nsGeranBands;

    std::vector<UtranBands_V1_5> utranBands;
    nsTArray<int32_t> nsUtranBands;

    std::vector<EutranBands_V1_5> eutranBands;
    nsTArray<int32_t> nsEutranBands;

    std::vector<NgranBands_V1_5> ngranBands;
    nsTArray<int32_t> nsNgranBands;

    switch ((RadioAccessNetworkTypes)nsRadioAccessNetwork) {
      case RadioAccessNetworkTypes::GERAN:
        specifier->GetGeranBands(nsGeranBands);
        for (uint32_t i = 0; i < nsGeranBands.Length(); i++) {
          geranBands.push_back((GeranBands)nsGeranBands[i]);
        }
        bandsInHalFormat.geranBands(geranBands);
        break;
      case RadioAccessNetworkTypes::UTRAN:
        specifier->GetUtranBands(nsUtranBands);
        for (uint32_t i = 0; i < nsUtranBands.Length(); i++) {
          utranBands.push_back((UtranBands_V1_5)nsUtranBands[i]);
        }
        bandsInHalFormat.utranBands(utranBands);
        break;
      case RadioAccessNetworkTypes::EUTRAN:
        specifier->GetEutranBands(nsEutranBands);
        for (uint32_t i = 0; i < nsEutranBands.Length(); i++) {
          eutranBands.push_back((EutranBands_V1_5)nsEutranBands[i]);
        }
        bandsInHalFormat.eutranBands(eutranBands);
        break;
      case RadioAccessNetworkTypes::NGRAN:
        specifier->GetNgranBands(nsNgranBands);
        for (uint32_t i = 0; i < nsNgranBands.Length(); i++) {
          DEBUG("convertToHalRadioAccessSpecifier_1_5 nsNgranBands[%d] = %d",
                i,
                nsNgranBands[i]);
          ngranBands.push_back((NgranBands_V1_5)nsNgranBands[i]);
        }
        bandsInHalFormat.ngranBands(ngranBands);
        break;
      default:
        break;
    }

    std::vector<int32_t> channels;
    nsTArray<int32_t> nsChannels;
    specifier->GetChannels(nsChannels);
    for (uint32_t i = 0; i < nsChannels.Length(); i++) {
      DEBUG("convertToHalRadioAccessSpecifier_1_5 nsChannels[%d] = %d",
            i,
            nsChannels[i]);
      channels.push_back(nsChannels[i]);
    }
    rasInHalFormat.channels = channels;
  }
  return rasInHalFormat;
}
#endif
