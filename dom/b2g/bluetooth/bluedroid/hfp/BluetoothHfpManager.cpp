/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothHfpManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsCharSeparatedTokenizer.h"  // for nsCCharSeparatedTokenizer
#include "nsContentUtils.h"
#include "nsIIccInfo.h"
#include "nsIIccService.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileConnectionService.h"
#include "nsIMobileNetworkInfo.h"
#include "nsIMobileSignalStrength.h"
#include "nsIObserverService.h"
#include "nsITelephonyService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/SVGContentUtils.h"  // for ParseInteger

#define AUDIO_VOLUME_BT_SCO_ID u"audio.volume.bt_sco"_ns

/**
 * Dispatch task with arguments to main thread.
 */
using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

// Declared for using nsISettingsManager
class SettingsGetResponse;
class SidlResponse;

namespace {
StaticRefPtr<BluetoothHfpManager> sBluetoothHfpManager;
static BluetoothHandsfreeInterface* sBluetoothHfpInterface = nullptr;

// Wait for 3 seconds for Dialer processing event 'BLDN'. '3' seconds is a
// magic number. The mechanism should be revised once we can get call history.
static int sWaitingForDialingInterval = 3000;  // unit: ms

// Wait 3.7 seconds until Dialer stops playing busy tone. '3' seconds is the
// time window set in Dialer and the extra '0.7' second is a magic number.
// The mechanism should be revised once we know the exact time at which
// Dialer stops playing.
static int sBusyToneInterval = 3700;  // unit: ms

// StaticRefPtr used by nsISettingsManager::Get()
StaticRefPtr<SettingsGetResponse> sSettingsGetResponse;

// StaticRefPtr used by nsISettingsManager::AddObserver()
StaticRefPtr<SidlResponse> sSidlResponse;
}  // namespace

bool BluetoothHfpManager::sInShutdown = false;
const int BluetoothHfpManager::MAX_NUM_CLIENTS = 1;

class SettingsGetResponse final : public nsISettingsGetResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISETTINGSGETRESPONSE

 protected:
  ~SettingsGetResponse() = default;
};
NS_IMETHODIMP SettingsGetResponse::Resolve(nsISettingInfo* info) {
  if (info && sBluetoothHfpManager) {
    nsString value;
    info->GetValue(value);
    sBluetoothHfpManager->HandleVolumeChanged(value);
  }
  return NS_OK;
}
NS_IMETHODIMP SettingsGetResponse::Reject(
    [[maybe_unused]] nsISettingError* aSettingError) {
  BT_WARNING("Failed to get setting 'audio.volume.bt_sco'");
  return NS_OK;
}
NS_IMPL_ISUPPORTS(SettingsGetResponse, nsISettingsGetResponse)

class SidlResponse final : public nsISidlDefaultResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIDLDEFAULTRESPONSE

 protected:
  ~SidlResponse() = default;
};
NS_IMETHODIMP SidlResponse::Resolve() { return NS_OK; }
NS_IMETHODIMP SidlResponse::Reject() {
  BT_WARNING("Failed to observe setting 'audio.volume.bt_sco'");
  return NS_ERROR_FAILURE;
}
NS_IMPL_ISUPPORTS(SidlResponse, nsISidlDefaultResponse)

static bool IsValidDtmf(const char aChar) {
  // Valid DTMF: [*#0-9ABCD]
  return (aChar == '*' || aChar == '#') || (aChar >= '0' && aChar <= '9') ||
         (aChar >= 'A' && aChar <= 'D');
}

static bool IsSupportedChld(const int aChld) {
  // We currently only support CHLD=0~3.
  return (aChld >= 0 && aChld <= 3);
}

class BluetoothHfpManager::CloseScoTask : public Runnable {
 public:
  CloseScoTask() : Runnable("CloseScoTask") { MOZ_ASSERT(NS_IsMainThread()); }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sBluetoothHfpManager);

    sBluetoothHfpManager->DisconnectSco();

    return NS_OK;
  }
};

class BluetoothHfpManager::CloseScoRunnable : public Runnable {
 public:
  CloseScoRunnable() : Runnable("CloseScoRunnable") {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<CloseScoTask> task = new CloseScoTask();
    MessageLoop::current()->PostDelayedTask(task.forget(), sBusyToneInterval);

    return NS_OK;
  }
};

class BluetoothHfpManager::RespondToBLDNTask : public Runnable {
 public:
  RespondToBLDNTask() : Runnable("RespondToBLDNTask") {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sBluetoothHfpManager);

    if (!sBluetoothHfpManager->mDialingRequestProcessed) {
      sBluetoothHfpManager->mDialingRequestProcessed = true;
      sBluetoothHfpManager->SendResponse(HFP_AT_RESPONSE_ERROR);
    }

    return NS_OK;
  }
};

/**
 *  Call
 */
Call::Call() { Reset(); }

void Call::Set(const nsAString& aNumber, const bool aIsOutgoing) {
  mNumber = aNumber;
  mDirection =
      (aIsOutgoing) ? HFP_CALL_DIRECTION_OUTGOING : HFP_CALL_DIRECTION_INCOMING;
  // Same logic as implementation in ril_worker.js
  if (aNumber.Length() && aNumber[0] == '+') {
    mType = HFP_CALL_ADDRESS_TYPE_INTERNATIONAL;
  } else {
    mType = HFP_CALL_ADDRESS_TYPE_UNKNOWN;
  }
}

void Call::Reset() {
  mState = nsITelephonyService::CALL_STATE_DISCONNECTED;
  mDirection = HFP_CALL_DIRECTION_OUTGOING;
  mNumber.Truncate();
  mType = HFP_CALL_ADDRESS_TYPE_UNKNOWN;
}

bool Call::IsActive() {
  return (mState == nsITelephonyService::CALL_STATE_CONNECTED);
}

/**
 *  BluetoothHfpManager
 */
BluetoothHfpManager::BluetoothHfpManager() : mPhoneType(PhoneType::NONE) {
  Reset();
}

void BluetoothHfpManager::ResetCallArray() {
  mCurrentCallArray.Clear();
  // Append a call object at the beginning of mCurrentCallArray since call
  // index from RIL starts at 1.
  Call call;
  mCurrentCallArray.AppendElement(call);

  if (mPhoneType == PhoneType::CDMA) {
    mCdmaSecondCall.Reset();
  }
}

void BluetoothHfpManager::Cleanup() {
  mReceiveVgsFlag = false;
  mDialingRequestProcessed = true;

  mConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mPrevConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mBattChg = 5;
  mService = HFP_NETWORK_STATE_NOT_AVAILABLE;
  mRoam = HFP_SERVICE_TYPE_HOME;
  mSignal = 0;
  mNrecEnabled = HFP_NREC_STARTED;
  mWbsEnabled = HFP_WBS_NONE;

  mController = nullptr;
}

void BluetoothHfpManager::Reset() {
  // Phone & Device CIND
  ResetCallArray();
  // Clear Sco state
  mAudioState = HFP_AUDIO_STATE_DISCONNECTED;
  Cleanup();
}

bool BluetoothHfpManager::Init() {
  // The function must run at b2g process since it would access SettingsService.
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add observers!");
    return false;
  }

  hal::RegisterBatteryObserver(this);
  // Update to the latest battery level
  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);
  Notify(batteryInfo);

  mListener = MakeUnique<BluetoothRilListener>();
  NS_ENSURE_TRUE(mListener->Listen(true), false);

  nsCOMPtr<nsISettingsManager> settings =
      do_GetService("@mozilla.org/sidl-native/settings;1");
  if (settings) {
    if (!sSettingsGetResponse) {
      sSettingsGetResponse = new SettingsGetResponse();
    }
    if (!sSidlResponse) {
      sSidlResponse = new SidlResponse();
    }
    settings->Get(AUDIO_VOLUME_BT_SCO_ID, sSettingsGetResponse.get());
    settings->AddObserver(AUDIO_VOLUME_BT_SCO_ID, this, sSidlResponse.get());
  }

  return true;
}

void BluetoothHfpManager::Uninit() {
  if (!mListener->Listen(false)) {
    BT_WARNING("Failed to stop listening RIL");
  }
  mListener = nullptr;

  hal::UnregisterBatteryObserver(this);

  nsCOMPtr<nsISettingsManager> settings =
      do_GetService("@mozilla.org/sidl-native/settings;1");
  if (settings && sSidlResponse) {
    settings->RemoveObserver(AUDIO_VOLUME_BT_SCO_ID, this, sSidlResponse.get());
  }
  sSidlResponse = nullptr;
  sSettingsGetResponse = nullptr;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove observers!");
  }
}

class BluetoothHfpManager::RegisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  RegisterModuleResultHandler(BluetoothHandsfreeInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
      : mInterface(aInterface), mRes(aRes) {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for HFP: %d",
               (int)aStatus);

    mInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothHfpInterface = mInterface;

    if (mRes) {
      mRes->Init();
    }
  }

 private:
  BluetoothHandsfreeInterface* mInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHfpManager::InitProfileResultHandlerRunnable final
    : public Runnable {
 public:
  InitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                   nsresult aRv)
      : Runnable("InitProfileResultHandlerRunnable"), mRes(aRes), mRv(aRv) {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Init();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void BluetoothHfpManager::InitHfpInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sBluetoothHfpInterface) {
    BT_LOGR("Bluetooth Handsfree interface is already initalized.");
    RefPtr<Runnable> r = new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP Init runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  auto interface = btInf->GetBluetoothHandsfreeInterface();

  if (NS_WARN_IF(!interface)) {
    // If there's no HFP interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  interface->SetNotificationHandler(BluetoothHfpManager::Get());

  setupInterface->RegisterModule(
      SETUP_SERVICE_ID_HANDSFREE, MODE_NARROWBAND_SPEECH, MAX_NUM_CLIENTS,
      new RegisterModuleResultHandler(interface, aRes));
}

BluetoothHfpManager::~BluetoothHfpManager() {}

class BluetoothHfpManager::UnregisterModuleResultHandler final
    : public BluetoothSetupResultHandler {
 public:
  explicit UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
      : mRes(aRes) {}

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for HFP: %d",
               (int)aStatus);

    if (sBluetoothHfpInterface) {
      sBluetoothHfpInterface->SetNotificationHandler(nullptr);
      sBluetoothHfpInterface = nullptr;
    }

    if (sBluetoothHfpManager) {
      sBluetoothHfpManager->Uninit();
      sBluetoothHfpManager = nullptr;
    }

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (sBluetoothHfpInterface) {
      sBluetoothHfpInterface->SetNotificationHandler(nullptr);
      sBluetoothHfpInterface = nullptr;
    }

    if (sBluetoothHfpManager) {
      sBluetoothHfpManager->Uninit();
      sBluetoothHfpManager = nullptr;
    }

    if (mRes) {
      mRes->Deinit();
    }
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHfpManager::DeinitProfileResultHandlerRunnable final
    : public Runnable {
 public:
  DeinitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                     nsresult aRv)
      : Runnable("DeinitProfileResultHandlerRunnable"), mRes(aRes), mRv(aRv) {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Deinit();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

 private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void BluetoothHfpManager::DeinitHfpInterface(
    BluetoothProfileResultHandler* aRes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBluetoothHfpInterface) {
    BT_LOGR("Bluetooth Handsfree interface has not been initialized.");
    RefPtr<Runnable> r = new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP Deinit runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
        new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(SETUP_SERVICE_ID_HANDSFREE,
                                   new UnregisterModuleResultHandler(aRes));
}

// static
BluetoothHfpManager* BluetoothHfpManager::Get() {
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothHfpManager already exists, exit early
  if (sBluetoothHfpManager) {
    return sBluetoothHfpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothHfpManager* manager = new BluetoothHfpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothHfpManager = manager;
  return sBluetoothHfpManager;
}

NS_IMETHODIMP
BluetoothHfpManager::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
  } else {
    MOZ_ASSERT(false, "BluetoothHfpManager got unexpected topic!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void BluetoothHfpManager::Notify(const hal::BatteryInformation& aBatteryInfo) {
  // Range of battery level: [0, 1], double
  // Range of CIND::BATTCHG: [0, 5], int
  mBattChg = (int)round(aBatteryInfo.level() * 5.0);
  if (IsConnected()) {
    UpdateDeviceCIND();
  }
}

void BluetoothHfpManager::NotifyConnectionStateChanged(const nsAString& aType) {
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs =
      do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE_VOID(obs);

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  if (NS_FAILED(obs->NotifyObservers(
          static_cast<BluetoothProfileManagerBase*>(this),
          NS_ConvertUTF16toUTF8(aType).get(), deviceAddressStr.get()))) {
    BT_WARNING("Failed to notify observsers!");
  }

  // Dispatch an event of status change
  bool status;
  nsAutoString eventName;
  if (aType.Equals(BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    status = IsConnected();
    eventName.AssignLiteral(HFP_STATUS_CHANGED_ID);
  } else if (aType.Equals(BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    status = IsScoConnected();
    eventName.AssignLiteral(SCO_STATUS_CHANGED_ID);
  } else {
    MOZ_ASSERT(false);
    return;
  }

  DispatchStatusChangedEvent(eventName, mDeviceAddress, status);

  // Notify profile controller
  if (aType.Equals(BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    if (IsConnected()) {
      MOZ_ASSERT(mListener);

      // Enumerate current calls
      mListener->EnumerateCalls();

      OnConnect(EmptyString());
    } else if (mConnectionState == HFP_CONNECTION_STATE_DISCONNECTED) {
      // Reset battery level when it's disconnected
      if (mDeviceBatteryLevel != -1) {
        mDeviceBatteryLevel = -1;
        DispatchHfBatteryChangedEvent(mDeviceAddress, -1);
      }

      mDeviceAddress.Clear();
      if (mPrevConnectionState == HFP_CONNECTION_STATE_DISCONNECTED) {
        // Bug 979160: This implies the outgoing connection failure.
        // When the outgoing hfp connection fails, state changes to disconnected
        // state. Since bluedroid would not report connecting state, but only
        // report connected/disconnected.
        OnConnect(ERR_CONNECTION_FAILED);
      } else {
        OnDisconnect(EmptyString());
      }
      Cleanup();
    }
  }
}

void BluetoothHfpManager::NotifyDialer(const nsAString& aCommand) {
  constexpr auto type = u"bluetooth-dialer-command"_ns;
  nsTArray<BluetoothNamedValue> parameters;

  AppendNamedValue(parameters, "command", nsString(aCommand));

  BT_LOGR("bluetooth-dialer-command with command [%s]",
          NS_ConvertUTF16toUTF8(aCommand).get());

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);
}

class BluetoothHfpManager::VolumeControlResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::VolumeControl failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::HandleVolumeChanged(const nsAString& aVolume) {
  MOZ_ASSERT(NS_IsMainThread());

  int32_t volume = 0;
  if (SVGContentUtils::ParseInteger(aVolume, volume) == false) {
    BT_WARNING("'audio.volume.bt_sco' is not a number!");
    return;
  }

  mCurrentVgs = volume;

  // Adjust volume by headset and we don't have to send volume back to headset
  if (IsConnected()) {
    BT_LOGR("AT+VGS=%d", mCurrentVgs);
    NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
    sBluetoothHfpInterface->VolumeControl(HFP_VOLUME_TYPE_SPEAKER, mCurrentVgs,
                                          mDeviceAddress,
                                          new VolumeControlResultHandler());
  }
}

void BluetoothHfpManager::HandleVoiceConnectionChanged(uint32_t aClientId) {
  nsCOMPtr<nsIMobileConnectionService> mcService =
      do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(mcService);

  nsCOMPtr<nsIMobileConnection> connection;
  mcService->GetItemByServiceId(aClientId, getter_AddRefs(connection));
  NS_ENSURE_TRUE_VOID(connection);

  nsCOMPtr<nsIMobileConnectionInfo> voiceInfo;
  connection->GetVoice(getter_AddRefs(voiceInfo));
  NS_ENSURE_TRUE_VOID(voiceInfo);

  nsString type;
  voiceInfo->GetType(type);
  mPhoneType = GetPhoneType(type);

  // Roam
  bool roaming;
  voiceInfo->GetRoaming(&roaming);
  mRoam = (roaming) ? HFP_SERVICE_TYPE_ROAMING : HFP_SERVICE_TYPE_HOME;

  // Service
  nsString regState;
  voiceInfo->GetState(regState);

  BluetoothHandsfreeNetworkState service =
      (regState.EqualsLiteral("registered")) ? HFP_NETWORK_STATE_AVAILABLE
                                             : HFP_NETWORK_STATE_NOT_AVAILABLE;
  if (service != mService) {
    // Notify BluetoothRilListener of service change
    mListener->ServiceChanged(aClientId, service);
  }
  mService = service;

  // Signal
  nsCOMPtr<nsIMobileSignalStrength> signalStrength;
  connection->GetSignalStrength(getter_AddRefs(signalStrength));
  // Level of signal bars ranges from -1 to 4.
  int16_t signalLevel;
  signalStrength->GetLevel(&signalLevel);
  // HFP Signal Strength indicator ranges from 0 to 5
  mSignal = signalLevel + 1;

  if (IsConnected()) {
    UpdateDeviceCIND();
  }

  // Operator name
  nsCOMPtr<nsIMobileNetworkInfo> network;
  voiceInfo->GetNetwork(getter_AddRefs(network));
  if (!network) {
    BT_LOGD("Unable to get network information");
    return;
  }
  network->GetLongName(mOperatorName);

  // According to GSM 07.07, "<format> indicates if the format is alphanumeric
  // or numeric; long alphanumeric format can be upto 16 characters long and
  // short format up to 8 characters (refer GSM MoU SE.13 [9])..."
  // However, we found that the operator name may sometimes be longer than 16
  // characters. After discussion, we decided to fix this here but not in RIL
  // or modem.
  //
  // Please see Bug 871366 for more information.
  if (mOperatorName.Length() > 16) {
    BT_WARNING("The operator name was longer than 16 characters. We cut it.");
    mOperatorName.Left(mOperatorName, 16);
  }
}

void BluetoothHfpManager::HandleIccInfoChanged(uint32_t aClientId) {
  nsCOMPtr<nsIIccService> service = do_GetService(ICC_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  nsCOMPtr<nsIIcc> icc;
  service->GetIccByServiceId(aClientId, getter_AddRefs(icc));
  NS_ENSURE_TRUE_VOID(icc);

  nsCOMPtr<nsIIccInfo> iccInfo;
  icc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_TRUE_VOID(iccInfo);

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo = do_QueryInterface(iccInfo);
  NS_ENSURE_TRUE_VOID(gsmIccInfo);
  gsmIccInfo->GetMsisdn(mMsisdn);
}

void BluetoothHfpManager::HandleShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  DisconnectSco();
  sBluetoothHfpManager = nullptr;

  nsCOMPtr<nsISettingsManager> settings =
      do_GetService("@mozilla.org/sidl-native/settings;1");
  if (settings && sSidlResponse) {
    settings->RemoveObserver(AUDIO_VOLUME_BT_SCO_ID, this, sSidlResponse.get());
  }
  sSidlResponse = nullptr;
  sSettingsGetResponse = nullptr;
}

class BluetoothHfpManager::ClccResponseResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::ClccResponse failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::SendCLCC(Call& aCall, int aIndex) {
  NS_ENSURE_TRUE_VOID(aCall.mState !=
                      nsITelephonyService::CALL_STATE_DISCONNECTED);
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  BluetoothHandsfreeCallState callState =
      ConvertToBluetoothHandsfreeCallState(aCall.mState);

  if (mPhoneType == PhoneType::CDMA && aIndex == 1 && aCall.IsActive()) {
    callState = (mCdmaSecondCall.IsActive()) ? HFP_CALL_STATE_HELD
                                             : HFP_CALL_STATE_ACTIVE;
  }

  if (callState == HFP_CALL_STATE_INCOMING &&
      FindFirstCall(nsITelephonyService::CALL_STATE_CONNECTED)) {
    callState = HFP_CALL_STATE_WAITING;
  }

  sBluetoothHfpInterface->ClccResponse(
      aIndex, aCall.mDirection, callState, HFP_CALL_MODE_VOICE,
      HFP_CALL_MPTY_TYPE_SINGLE, aCall.mNumber, aCall.mType, mDeviceAddress,
      new ClccResponseResultHandler());
}

class BluetoothHfpManager::FormattedAtResponseResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::FormattedAtResponse failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::SendLine(const char* aMessage) {
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->FormattedAtResponse(
      aMessage, mDeviceAddress, new FormattedAtResponseResultHandler());
}

class BluetoothHfpManager::AtResponseResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::AtResponse failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::SendResponse(
    BluetoothHandsfreeAtResponse aResponseCode) {
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->AtResponse(aResponseCode, 0, mDeviceAddress,
                                     new AtResponseResultHandler());
}

class BluetoothHfpManager::PhoneStateChangeResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::PhoneStateChange failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::UpdatePhoneCIND(uint32_t aCallIndex) {
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  BluetoothHandsfreeCallState callSetupState =
      ConvertToBluetoothHandsfreeCallState(GetCallSetupState());
  BluetoothHandsfreeCallAddressType type = mCurrentCallArray[aCallIndex].mType;

  BT_LOGR("[%d] state %d => BTHF: active[%d] held[%d] setupstate[%d]",
          aCallIndex, mCurrentCallArray[aCallIndex].mState, numActive, numHeld,
          callSetupState);

  sBluetoothHfpInterface->PhoneStateChange(
      numActive, numHeld, callSetupState, mCurrentCallArray[aCallIndex].mNumber,
      type, new PhoneStateChangeResultHandler());
}

class BluetoothHfpManager::DeviceStatusNotificationResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING(
        "BluetoothHandsfreeInterface::DeviceStatusNotification failed: %d",
        (int)aStatus);
  }
};

void BluetoothHfpManager::UpdateDeviceCIND() {
  if (sBluetoothHfpInterface) {
    sBluetoothHfpInterface->DeviceStatusNotification(
        mService, mRoam, mSignal, mBattChg,
        new DeviceStatusNotificationResultHandler());
  }
}

uint32_t BluetoothHfpManager::FindFirstCall(uint16_t aState) {
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    if (mCurrentCallArray[i].mState == aState) {
      return i;
    }
  }

  return 0;
}

uint32_t BluetoothHfpManager::GetNumberOfCalls(uint16_t aState) {
  uint32_t num = 0;
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    if (mCurrentCallArray[i].mState == aState) {
      ++num;
    }
  }

  return num;
}

uint16_t BluetoothHfpManager::GetCdmaSecondCallSetupState() {
  /*
   * In CDMA case, the phone calls use the same channel, and when
   * there's a second incoming call, the TelephonyListener::HandleCallInfo()
   * will not be called, so the HandleCallStateChanged() will not be called.
   * However, the TelephonyListener::NotifyCdmaCallWaiting() will be called
   * to notify there's a second phone call waiting, so that
   * UpdateSecondNumber() will be called.
   *
   * When the CDMA second incoming phone call disconnect from remote party,
   * the CDMA phone will not be notified, since the phone calls use the
   * same channel, and there's still a connected phone call (the first one).
   *
   * In order to send HF the +CCWA result code, we will call
   * sBluetoothHfpInterface->PhoneStateChange() and pass the "call setup"
   * status of the second incoming call.
   */
  switch (mCdmaSecondCall.mState) {
    case nsITelephonyService::CALL_STATE_INCOMING:
    case nsITelephonyService::CALL_STATE_DIALING:
    case nsITelephonyService::CALL_STATE_ALERTING:
      return mCdmaSecondCall.mState;
    default:
      break;
  }

  return nsITelephonyService::CALL_STATE_DISCONNECTED;
}

uint16_t BluetoothHfpManager::GetCallSetupState() {
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    switch (mCurrentCallArray[i].mState) {
      case nsITelephonyService::CALL_STATE_INCOMING:
      case nsITelephonyService::CALL_STATE_DIALING:
      case nsITelephonyService::CALL_STATE_ALERTING:
        return mCurrentCallArray[i].mState;
      default:
        break;
    }
  }

  return nsITelephonyService::CALL_STATE_DISCONNECTED;
}

BluetoothHandsfreeCallState
BluetoothHfpManager::ConvertToBluetoothHandsfreeCallState(
    int aCallState) const {
  BluetoothHandsfreeCallState state;

  // Refer to AOSP BluetoothPhoneService.convertCallState
  if (aCallState == nsITelephonyService::CALL_STATE_INCOMING) {
    state = HFP_CALL_STATE_INCOMING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_DIALING) {
    state = HFP_CALL_STATE_DIALING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_ALERTING) {
    state = HFP_CALL_STATE_ALERTING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_CONNECTED) {
    state = HFP_CALL_STATE_ACTIVE;
  } else if (aCallState == nsITelephonyService::CALL_STATE_HELD) {
    state = HFP_CALL_STATE_HELD;
  } else {  // disconnected
    state = HFP_CALL_STATE_IDLE;
  }

  return state;
}

bool BluetoothHfpManager::IsTransitionState(uint16_t aCallState,
                                            bool aIsConference) {
  /**
   * Regard this callstate change as during CHLD=2 transition state if
   * - the call becomes active, and numActive > 1
   * - the call becomes held, and numHeld > 1 or an incoming call exists
   *
   * TODO:
   * 1) handle CHLD=1 transition state
   * 2) handle conference call cases
   */
  if (!aIsConference) {
    switch (aCallState) {
      case nsITelephonyService::CALL_STATE_CONNECTED:
        return (GetNumberOfCalls(aCallState) > 1);
      case nsITelephonyService::CALL_STATE_HELD:
        return (GetNumberOfCalls(aCallState) > 1 ||
                FindFirstCall(nsITelephonyService::CALL_STATE_INCOMING));
      default:
        break;
    }
  }

  return false;
}

void BluetoothHfpManager::HandleCallStateChanged(
    uint32_t aCallIndex, uint16_t aCallState, const nsAString& aError,
    const nsAString& aNumber, const bool aIsOutgoing, const bool aIsConference,
    bool aSend) {
  // aCallIndex can be UINT32_MAX for the pending outgoing call state update.
  // aCallIndex will be updated again after real call state changes. See Bug
  // 990467.
  if (aCallIndex == UINT32_MAX) {
    return;
  }

  // We've send Dialer a dialing request and this is the response sent to
  // HF when SLC is connected.
  if (aCallState == nsITelephonyService::CALL_STATE_DIALING && IsConnected() &&
      !mDialingRequestProcessed) {
    SendResponse(HFP_AT_RESPONSE_OK);
    mDialingRequestProcessed = true;
  }

  // Update call state only
  while (aCallIndex >= mCurrentCallArray.Length()) {
    Call call;
    mCurrentCallArray.AppendElement(call);
  }
  mCurrentCallArray[aCallIndex].mState = aCallState;

  // Update call information besides call state
  mCurrentCallArray[aCallIndex].Set(aNumber, aIsOutgoing);

  // When SLC is connected, notify bluedroid of phone state change if this
  // call state change is not during transition state
  if (IsConnected() && !IsTransitionState(aCallState, aIsConference)) {
    UpdatePhoneCIND(aCallIndex);
  }

  if (aCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    // -1 is necessary because call 0 is an invalid (padding) call object.
    if (mCurrentCallArray.Length() - 1 ==
        GetNumberOfCalls(nsITelephonyService::CALL_STATE_DISCONNECTED)) {
      // When SLC is connected, in order to let user hear busy tone via
      // connected Bluetooth headset, we postpone the timing of dropping SCO.
      if (IsConnected() && aError.EqualsLiteral("BusyError")) {
        // FIXME: UpdatePhoneCIND later since it causes SCO close but
        // Dialer is still playing busy tone via HF.
        NS_DispatchToMainThread(new CloseScoRunnable());
      }

      // We need to make sure the ResetCallArray() is executed after
      // UpdatePhoneCIND(), because after resetting mCurrentCallArray,
      // the mCurrentCallArray[aCallIndex] may be meaningless in
      // UpdatePhoneCIND().
      ResetCallArray();
    }
  }
}

PhoneType BluetoothHfpManager::GetPhoneType(const nsAString& aType) {
  // FIXME: Query phone type from RIL after RIL implements new API (bug 912019)
  if (aType.EqualsLiteral("gsm") || aType.EqualsLiteral("gprs") ||
      aType.EqualsLiteral("edge") || aType.EqualsLiteral("umts") ||
      aType.EqualsLiteral("hspa") || aType.EqualsLiteral("hsdpa") ||
      aType.EqualsLiteral("hsupa") || aType.EqualsLiteral("hspa+") ||
      aType.EqualsLiteral("lte")) {
    return PhoneType::GSM;
  } else if (aType.EqualsLiteral("is95a") || aType.EqualsLiteral("is95b") ||
             aType.EqualsLiteral("1xrtt") || aType.EqualsLiteral("evdo0") ||
             aType.EqualsLiteral("evdoa") || aType.EqualsLiteral("evdob") ||
             aType.EqualsLiteral("ehrpd")) {
    return PhoneType::CDMA;
  }

  return PhoneType::NONE;
}

void BluetoothHfpManager::UpdateSecondNumber(const nsAString& aNumber) {
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Always regard second call as incoming call since v1.2 RIL
  // doesn't support outgoing second call in CDMA.
  mCdmaSecondCall.Set(aNumber, false);

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  mCdmaSecondCall.mState = nsITelephonyService::CALL_STATE_INCOMING;
  int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  BluetoothHandsfreeCallState callSetupState =
      ConvertToBluetoothHandsfreeCallState(GetCdmaSecondCallSetupState());
  BluetoothHandsfreeCallAddressType type = mCdmaSecondCall.mType;

  BT_LOGR(
      "CDMA 2nd number state %d => \
          BTHF: active[%d] held[%d] setupstate[%d]",
      mCdmaSecondCall.mState, numActive, numHeld, callSetupState);

  sBluetoothHfpInterface->PhoneStateChange(numActive, numHeld, callSetupState,
                                           aNumber, type,
                                           new PhoneStateChangeResultHandler());
}

void BluetoothHfpManager::AnswerWaitingCall() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  // Pick up second call. First call is held now.
  mCdmaSecondCall.mState = nsITelephonyService::CALL_STATE_CONNECTED;
  uint32_t callLength = mCurrentCallArray.Length();
  for (uint32_t i = 1; i < callLength; ++i) {
    /*
     * Since we answer the second incoming call, the state of the
     * previous calls in mCurrentCallArray need to be changed to HELD
     * if they are CONNECTED before, so that the numbers of CONNECTED
     * and HELD phone calls will be corrected before passing to
     * sBluetoothHfpInterface->PhoneStateChange()
     */
    if (mCurrentCallArray[i].mState ==
        nsITelephonyService::CALL_STATE_CONNECTED) {
      mCurrentCallArray[i].mState = nsITelephonyService::CALL_STATE_HELD;
    }
  }

  /*
   * The GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED)
   * only get the CONNECTED calls in mCurrentCallArray. When calculating
   * the numActive, we need to take mCdmaSecondCall into account which
   * is CONNECTED at this time.
   *
   * We don't modify GetNumberOfCalls() to make this function take
   * mCdmaSecondCall into account because GetNumberOfCalls() is used
   * in other cases. And we won't be notified when the second incoming
   * call is disconnected in CDMA case, so the mCdmaSecondCall state
   * may be wrong if it is disconnected before. In this case, the
   * GetNumberOfCalls() may be wrong if we take mCdmaSecondCall into
   * account.
   */
  int numActive =
      GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED) + 1;
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  BluetoothHandsfreeCallState callSetupState =
      ConvertToBluetoothHandsfreeCallState(GetCdmaSecondCallSetupState());
  BluetoothHandsfreeCallAddressType type = mCdmaSecondCall.mType;

  BT_LOGR(
      "CDMA 2nd number state %d => \
          BTHF: active[%d] held[%d] setupstate[%d]",
      mCdmaSecondCall.mState, numActive, numHeld, callSetupState);

  sBluetoothHfpInterface->PhoneStateChange(numActive, numHeld, callSetupState,
                                           mCdmaSecondCall.mNumber, type,
                                           new PhoneStateChangeResultHandler());
}

void BluetoothHfpManager::IgnoreWaitingCall() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  mCdmaSecondCall.Reset();
  // FIXME: check CDMA + bluedroid
  // UpdateCIND(CINDType::CALLSETUP, CallSetupState::NO_CALLSETUP, true);
}

void BluetoothHfpManager::ToggleCalls() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Toggle acitve and held calls
  mCdmaSecondCall.mState = (mCdmaSecondCall.IsActive())
                               ? nsITelephonyService::CALL_STATE_HELD
                               : nsITelephonyService::CALL_STATE_CONNECTED;
}

/*
 * Reset connection state and audio state to DISCONNECTED to handle backend
 * error. The state change triggers UI status bar update as ordinary bluetooth
 * turn-off sequence.
 */
void BluetoothHfpManager::HandleBackendError() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mConnectionState != HFP_CONNECTION_STATE_DISCONNECTED) {
    ConnectionStateNotification(HFP_CONNECTION_STATE_DISCONNECTED,
                                mDeviceAddress);
  }

  if (mAudioState != HFP_AUDIO_STATE_DISCONNECTED) {
    AudioStateNotification(HFP_AUDIO_STATE_DISCONNECTED, mDeviceAddress);
  }
}

class BluetoothHfpManager::ConnectAudioResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::ConnectAudio failed: %d",
               (int)aStatus);
  }
};

bool BluetoothHfpManager::ConnectSco() {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(!sInShutdown, false);
  NS_ENSURE_TRUE(IsConnected() && !IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  sBluetoothHfpInterface->ConnectAudio(mDeviceAddress,
                                       new ConnectAudioResultHandler());

  return true;
}

class BluetoothHfpManager::DisconnectAudioResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::DisconnectAudio failed: %d",
               (int)aStatus);
  }
};

bool BluetoothHfpManager::DisconnectSco() {
  NS_ENSURE_TRUE(IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  sBluetoothHfpInterface->DisconnectAudio(mDeviceAddress,
                                          new DisconnectAudioResultHandler());

  return true;
}

bool BluetoothHfpManager::IsScoConnected() {
  return (mAudioState == HFP_AUDIO_STATE_CONNECTED);
}

bool BluetoothHfpManager::IsConnected() {
  return (mConnectionState == HFP_CONNECTION_STATE_SLC_CONNECTED);
}

bool BluetoothHfpManager::IsNrecEnabled() { return mNrecEnabled; }

bool BluetoothHfpManager::ReplyToConnectionRequest(bool aAccept) {
  MOZ_ASSERT(false,
             "BluetoothHfpManager hasn't implemented this function yet.");
  return false;
}

bool BluetoothHfpManager::IsWbsEnabled() { return mWbsEnabled; }

void BluetoothHfpManager::OnConnectError() {
  MOZ_ASSERT(NS_IsMainThread());

  mController->NotifyCompletion(ERR_CONNECTION_FAILED);

  mController = nullptr;
  mDeviceAddress.Clear();
}

class BluetoothHfpManager::ConnectResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  explicit ConnectResultHandler(BluetoothHfpManager* aHfpManager)
      : mHfpManager(aHfpManager) {
    MOZ_ASSERT(mHfpManager);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::Connect failed: %d", (int)aStatus);
    mHfpManager->OnConnectError();
  }

 private:
  BluetoothHfpManager* mHfpManager;
};

void BluetoothHfpManager::Connect(const BluetoothAddress& aDeviceAddress,
                                  BluetoothProfileController* aController) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aController && !mController);

  if (sInShutdown) {
    aController->NotifyCompletion(ERR_NO_AVAILABLE_RESOURCE);
    return;
  }

  if (!sBluetoothHfpInterface) {
    BT_LOGR("sBluetoothHfpInterface is null");
    aController->NotifyCompletion(ERR_NO_AVAILABLE_RESOURCE);
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  sBluetoothHfpInterface->Connect(mDeviceAddress,
                                  new ConnectResultHandler(this));
}

void BluetoothHfpManager::OnDisconnectError() {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(ERR_CONNECTION_FAILED);
}

class BluetoothHfpManager::DisconnectResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  explicit DisconnectResultHandler(BluetoothHfpManager* aHfpManager)
      : mHfpManager(aHfpManager) {
    MOZ_ASSERT(mHfpManager);
  }

  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::Disconnect failed: %d",
               (int)aStatus);
    mHfpManager->OnDisconnectError();
  }

 private:
  BluetoothHfpManager* mHfpManager;
};

void BluetoothHfpManager::Disconnect(BluetoothProfileController* aController) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  if (!sBluetoothHfpInterface) {
    BT_LOGR("sBluetoothHfpInterface is null");
    if (aController) {
      aController->NotifyCompletion(ERR_NO_AVAILABLE_RESOURCE);
    }
    return;
  }

  mController = aController;

  sBluetoothHfpInterface->Disconnect(mDeviceAddress,
                                     new DisconnectResultHandler(this));
}

void BluetoothHfpManager::OnConnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  mController = nullptr;
}

void BluetoothHfpManager::OnDisconnect(const nsAString& aErrorStr) {
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  mController = nullptr;
}

void BluetoothHfpManager::OnUpdateSdpRecords(
    const BluetoothAddress& aDeviceAddress) {
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

void BluetoothHfpManager::OnGetServiceChannel(
    const BluetoothAddress& aDeviceAddress, const BluetoothUuid& aServiceUuid,
    int aChannel) {
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

void BluetoothHfpManager::GetAddress(BluetoothAddress& aDeviceAddress) {
  aDeviceAddress = mDeviceAddress;
}

//
// Bluetooth notifications
//

void BluetoothHfpManager::ConnectionStateNotification(
    BluetoothHandsfreeConnectionState aState,
    const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("[HFP] state %d", aState);

  mPrevConnectionState = mConnectionState;
  mConnectionState = aState;

  if (aState == HFP_CONNECTION_STATE_SLC_CONNECTED) {
    mDeviceAddress = aBdAddress;
    NotifyConnectionStateChanged(BLUETOOTH_HFP_STATUS_CHANGED_ID);

  } else if (aState == HFP_CONNECTION_STATE_DISCONNECTED) {
    DisconnectSco();
    NotifyConnectionStateChanged(BLUETOOTH_HFP_STATUS_CHANGED_ID);

  } else if (aState == HFP_CONNECTION_STATE_CONNECTED) {
    // Once RFCOMM is connected, enable NREC before each new SLC connection
    NRECNotification(HFP_NREC_STARTED, mDeviceAddress);
  }
}

void BluetoothHfpManager::AudioStateNotification(
    BluetoothHandsfreeAudioState aState, const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("state %d", aState);

  mAudioState = aState;

  if (aState == HFP_AUDIO_STATE_CONNECTED ||
      aState == HFP_AUDIO_STATE_DISCONNECTED) {
    NotifyConnectionStateChanged(BLUETOOTH_SCO_STATUS_CHANGED_ID);
  }
}

void BluetoothHfpManager::AnswerCallNotification(
    const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NotifyDialer(u"ATA"_ns);
}

void BluetoothHfpManager::HangupCallNotification(
    const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NotifyDialer(u"CHUP"_ns);
}

void BluetoothHfpManager::VolumeNotification(
    BluetoothHandsfreeVolumeType aType, int aVolume,
    const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aVolume >= 0 && aVolume <= 15);

  if (aType == HFP_VOLUME_TYPE_MICROPHONE) {
    mCurrentVgm = aVolume;
  } else if (aType == HFP_VOLUME_TYPE_SPEAKER) {
    mReceiveVgsFlag = true;

    if (aVolume == mCurrentVgs) {
      // Keep current volume
      return;
    }

    nsString data;
    data.AppendInt(aVolume);

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    NS_ENSURE_TRUE_VOID(os);

    BT_LOGR("bluetooth-volume-change: %s", NS_ConvertUTF16toUTF8(data).get());
    os->NotifyObservers(nullptr, "bluetooth-volume-change", data.get());
  }
}

void BluetoothHfpManager::DtmfNotification(char aDtmf,
                                           const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(IsValidDtmf(aDtmf));

  nsAutoCString message("VTS=");
  message += aDtmf;
  NotifyDialer(NS_ConvertUTF8toUTF16(message));
}

/**
 * NREC status will be set when:
 * 1. Get an AT command from HF device.
 *    (Bluetooth HFP spec v1.6 merely defines for the "Disable" part.)
 * 2. Once RFCOMM is connected, enable NREC before each new SLC connection.
 */
void BluetoothHfpManager::NRECNotification(BluetoothHandsfreeNRECState aNrec,
                                           const BluetoothAddress& aBdAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  mNrecEnabled = static_cast<bool>(aNrec);

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  // Notify audio manager
  if (NS_FAILED(obs->NotifyObservers(
          static_cast<BluetoothProfileManagerBase*>(this),
          BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID, deviceAddressStr.get()))) {
    BT_WARNING(
        "Failed to notify bluetooth-hfp-nrec-status-changed observsers!");
  }
}

void BluetoothHfpManager::WbsNotification(BluetoothHandsfreeWbsConfig aWbs,
                                          const BluetoothAddress& aBdAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  mWbsEnabled = (aWbs == HFP_WBS_YES);

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  // Notify audio manager
  if (NS_FAILED(obs->NotifyObservers(
          static_cast<BluetoothProfileManagerBase*>(this),
          BLUETOOTH_HFP_WBS_STATUS_CHANGED_ID, deviceAddressStr.get()))) {
    BT_WARNING("Failed to notify bluetooth-hfp-wbs-status-changed observsers!");
  }
}

void BluetoothHfpManager::CallHoldNotification(
    BluetoothHandsfreeCallHoldType aChld, const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsSupportedChld((int)aChld)) {
    // We currently don't support Enhanced Call Control.
    // AT+CHLD=1x and AT+CHLD=2x will be ignored
    SendResponse(HFP_AT_RESPONSE_ERROR);
    return;
  }

  SendResponse(HFP_AT_RESPONSE_OK);

  nsAutoCString message("CHLD=");
  message.AppendInt((int)aChld);
  NotifyDialer(NS_ConvertUTF8toUTF16(message));

  if (mPhoneType == PhoneType::CDMA &&
      aChld == BluetoothHandsfreeCallHoldType::HFP_CALL_HOLD_RELEASEHELD) {
    /*
     * After notifying dialer CHLD=0 above, AG should release all held calls
     * according to Bluetooth HFP 1.6. But in CDMA case, the first incoming call
     * and second incoming call use the same channel, dialer app cannot hangup
     * the second waiting call. However, the second incoming waiting call should
     * be in disconnected state at this time.
     */
    mCdmaSecondCall.mState = nsITelephonyService::CALL_STATE_DISCONNECTED;
    int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
    int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
    BluetoothHandsfreeCallState callSetupState =
        ConvertToBluetoothHandsfreeCallState(GetCdmaSecondCallSetupState());
    BluetoothHandsfreeCallAddressType type = mCdmaSecondCall.mType;

    BT_LOGR(
        "CDMA 2nd number state %d => \
            BTHF: active[%d] held[%d] setupstate[%d]",
        mCdmaSecondCall.mState, numActive, numHeld, callSetupState);

    sBluetoothHfpInterface->PhoneStateChange(
        numActive, numHeld, callSetupState, mCdmaSecondCall.mNumber, type,
        new PhoneStateChangeResultHandler());
  }
}

void BluetoothHfpManager::DialCallNotification(
    const nsAString& aNumber, const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCString message = NS_ConvertUTF16toUTF8(aNumber);

  // There are three cases based on aNumber,
  // 1) Empty value:    Redial, BLDN
  // 2) >xxx:           Memory dial, ATD>xxx
  // 3) xxx:            Normal dial, ATDxxx
  // We need to respond OK/Error for dial requests for every case listed above,
  // 1) and 2):         Respond in either RespondToBLDNTask or
  //                    HandleCallStateChanged()
  // 3):                Respond here
  if (message.IsEmpty()) {
    mDialingRequestProcessed = false;
    NotifyDialer(u"BLDN"_ns);

    RefPtr<RespondToBLDNTask> task = new RespondToBLDNTask();
    MessageLoop::current()->PostDelayedTask(task.forget(),
                                            sWaitingForDialingInterval);
  } else if (message[0] == '>') {
    mDialingRequestProcessed = false;

    nsAutoCString newMsg("ATD");
    newMsg += StringHead(message, message.Length() - 1);
    NotifyDialer(NS_ConvertUTF8toUTF16(newMsg));

    RefPtr<RespondToBLDNTask> task = new RespondToBLDNTask();
    MessageLoop::current()->PostDelayedTask(task.forget(),
                                            sWaitingForDialingInterval);
  } else {
    SendResponse(HFP_AT_RESPONSE_OK);

    nsAutoCString newMsg("ATD");
    newMsg += StringHead(message, message.Length() - 1);
    NotifyDialer(NS_ConvertUTF8toUTF16(newMsg));
  }
}

void BluetoothHfpManager::CnumNotification(const BluetoothAddress& aBdAddress) {
  static const uint8_t sAddressType[]{
      [HFP_CALL_ADDRESS_TYPE_UNKNOWN] = 0x81,
      [HFP_CALL_ADDRESS_TYPE_INTERNATIONAL] = 0x91  // for completeness
  };

  MOZ_ASSERT(NS_IsMainThread());

  if (!mMsisdn.IsEmpty()) {
    nsAutoCString message("+CNUM: ,\"");
    message.Append(NS_ConvertUTF16toUTF8(mMsisdn).get());
    message.AppendLiteral("\",");
    message.AppendInt(sAddressType[HFP_CALL_ADDRESS_TYPE_UNKNOWN]);
    message.AppendLiteral(",,4");

    SendLine(message.get());
  }

  SendResponse(HFP_AT_RESPONSE_OK);
}

class BluetoothHfpManager::CindResponseResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::CindResponse failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::CindNotification(const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  /*
   * When counting the numbers of CONNECTED and HELD calls, we should take
   * mCdmaSecondCall into account
   */
  int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  if (mCdmaSecondCall.mState == nsITelephonyService::CALL_STATE_CONNECTED) {
    ++numActive;
  } else if (mCdmaSecondCall.mState == nsITelephonyService::CALL_STATE_HELD) {
    ++numHeld;
  }

  BluetoothHandsfreeCallState callState =
      ConvertToBluetoothHandsfreeCallState(GetCallSetupState());

  sBluetoothHfpInterface->CindResponse(mService, numActive, numHeld, callState,
                                       mSignal, mRoam, mBattChg, aBdAddress,
                                       new CindResponseResultHandler());
}

class BluetoothHfpManager::CopsResponseResultHandler final
    : public BluetoothHandsfreeResultHandler {
 public:
  void OnError(BluetoothStatus aStatus) override {
    BT_WARNING("BluetoothHandsfreeInterface::CopsResponse failed: %d",
               (int)aStatus);
  }
};

void BluetoothHfpManager::CopsNotification(const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->CopsResponse(
      NS_ConvertUTF16toUTF8(mOperatorName).get(), aBdAddress,
      new CopsResponseResultHandler());
}

void BluetoothHfpManager::ClccNotification(const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t callNumbers = mCurrentCallArray.Length();
  uint32_t i;
  for (i = 1; i < callNumbers; i++) {
    SendCLCC(mCurrentCallArray[i], i);
  }

  if (!mCdmaSecondCall.mNumber.IsEmpty()) {
    MOZ_ASSERT(mPhoneType == PhoneType::CDMA);
    MOZ_ASSERT(i == 2);

    SendCLCC(mCdmaSecondCall, 2);
  }

  SendResponse(HFP_AT_RESPONSE_OK);
}

void BluetoothHfpManager::UnknownAtNotification(
    const nsACString& aAtString, const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("[%s]", nsCString(aAtString).get());

  // Directly handle AT+IPHONEACCEV without checking AT+XAPL
  if (nsAutoCString(aAtString).Find("+IPHONEACCEV=") != kNotFound) {
    if (HandleIphoneAccev(aAtString, aBdAddress)) {
      SendResponse(HFP_AT_RESPONSE_OK);
      return;
    } else {
      BT_WARNING("Failed to parse AT+IPHONEACCEV command");
    }
  }

  SendResponse(HFP_AT_RESPONSE_ERROR);
}

// Handle AT+IPHONEACCEV command. Currently, only battery level is used.
bool BluetoothHfpManager::HandleIphoneAccev(
    const nsACString& aAtString, const BluetoothAddress& aBdAddress) {
  // Format:
  //   +IPHONEACCEV=Number of key/value pairs,key1,val1,key2,val2,...
  // Key/value pairs:
  //   key: the type of change being reported:
  //     - 1 = Battery Level
  //     - 2 = Dock State
  //   val: the value of the change:
  //     - Battery Level: string value between '0' and '9'
  //     - Dock State: 0 = undocked, 1 = docked
  nsCCharSeparatedTokenizer exposeTokens(aAtString, ',');
  if (!exposeTokens.hasMoreTokens()) {
    return false;
  }
  // Skip the 1st token "+IPHONEACCEV="
  exposeTokens.nextToken();

  while (exposeTokens.hasMoreTokens()) {
    const nsDependentCSubstring& key = exposeTokens.nextToken();
    if (!exposeTokens.hasMoreTokens()) {
      return false;
    }
    const nsDependentCSubstring& value = exposeTokens.nextToken();
    if (key.EqualsLiteral("1")) {
      if (value.Length() != 1 || value[0] < '0' || value[0] > '9') {
        return false;
      }
      mDeviceBatteryLevel = (value[0] - '0') * 10;
      DispatchHfBatteryChangedEvent(aBdAddress, mDeviceBatteryLevel);
      return true;
    }
  }
  return true;
}

void BluetoothHfpManager::KeyPressedNotification(
    const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  bool hasActiveCall =
      (FindFirstCall(nsITelephonyService::CALL_STATE_CONNECTED) > 0);

  // Refer to AOSP HeadsetStateMachine.processKeyPressed
  if (FindFirstCall(nsITelephonyService::CALL_STATE_INCOMING) &&
      !hasActiveCall) {
    /*
     * Bluetooth HSP spec 4.2.2
     * There is an incoming call, notify Dialer to pick up the phone call
     * and SCO will be established after we get the CallStateChanged event
     * indicating the call is answered successfully.
     */
    NotifyDialer(u"ATA"_ns);
  } else if (hasActiveCall) {
    if (!IsScoConnected()) {
      /*
       * Bluetooth HSP spec 4.3
       * If there's no SCO, set up a SCO link.
       */
      ConnectSco();
    } else {
      /*
       * Bluetooth HSP spec 4.5
       * There are two ways to release SCO: sending CHUP to dialer or closing
       * SCO socket directly. We notify dialer only if there is at least one
       * active call.
       */
      NotifyDialer(u"CHUP"_ns);
    }
  } else {
    // BLDN

    NotifyDialer(u"BLDN"_ns);

    RefPtr<RespondToBLDNTask> task = new RespondToBLDNTask();
    MessageLoop::current()->PostDelayedTask(task.forget(),
                                            sWaitingForDialingInterval);
  }
}

void BluetoothHfpManager::BindNotification(const nsACString& aAtString,
                                           const BluetoothAddress& aBdAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGD("[%s]", nsCString(aAtString).get());
}

void BluetoothHfpManager::BievNotification(BluetoothHandsfreeHfIndType aType,
                                           int aValue,
                                           const BluetoothAddress& aBdAddr) {
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGD("type: %d, value: %d", static_cast<int>(aType), aValue);

  if (aType == HFP_HF_IND_BATTERY_LEVEL_STATUS) {
    if (aValue >= 0 && aValue <= 100) {
      mDeviceBatteryLevel = aValue;
      DispatchHfBatteryChangedEvent(aBdAddr, mDeviceBatteryLevel);
    } else {
      BT_WARNING("The value of HF battery level is abnormal");
    }
  }
}

// Implements nsISettingsObserver::ObserveSetting
NS_IMETHODIMP BluetoothHfpManager::ObserveSetting(nsISettingInfo* info) {
  if (info) {
    // Currently, AUDIO_VOLUME_BT_SCO_ID is the only setting we observe
    nsString value;
    info->GetValue(value);
    HandleVolumeChanged(value);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(BluetoothHfpManager, nsIObserver, nsISettingsObserver)
