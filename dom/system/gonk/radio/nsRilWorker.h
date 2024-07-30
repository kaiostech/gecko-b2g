/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilWorker_H
#define nsRilWorker_H
#if ANDROID_VERSION >= 33
#include "nsIRadioProxyServiceManager.h"
#include <android/hardware/radio/1.6/IRadio.h>
#else
#include <android/hardware/radio/1.1/IRadio.h>
#endif
#include "nsRadioTypes.h"
#include "nsRilIndication.h"
#include "nsRilIndicationResult.h"
#include "nsRilResponse.h"
#include "nsRilResponseResult.h"
#include "nsRilResult.h"
#include <nsIRilWorkerService.h>
#include <nsISupportsImpl.h>
#include <nsTArray.h>
#include <nsThreadUtils.h>

using ::android::sp;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::radio::V1_0::ApnAuthType;
using ::android::hardware::radio::V1_0::ApnTypes;
using CallForwardInfo_V1_0 = ::android::hardware::radio::V1_0::CallForwardInfo;
using ApnAuthType_V1_0 = ::android::hardware::radio::V1_0::ApnAuthType;
using ApnTypes_V1_0 = ::android::hardware::radio::V1_0::ApnTypes;
using CallForwardInfo_V1_0 = ::android::hardware::radio::V1_0::CallForwardInfo;
using CallForwardInfoStatus_V1_0 =
  ::android::hardware::radio::V1_0::CallForwardInfoStatus;
using Clir_V1_0 = ::android::hardware::radio::V1_0::Clir;
using DataProfileId_V1_0 = ::android::hardware::radio::V1_0::DataProfileId;
using DataProfileInfo_V1_0 = ::android::hardware::radio::V1_0::DataProfileInfo;
using DataProfileInfoType_V1_0 =
  ::android::hardware::radio::V1_0::DataProfileInfoType;
using Dial_V1_0 = ::android::hardware::radio::V1_0::Dial;
using GsmSmsMessage_V1_0 = ::android::hardware::radio::V1_0::GsmSmsMessage;
using IccIo_V1_0 = ::android::hardware::radio::V1_0::IccIo;
using MvnoType_V1_0 = ::android::hardware::radio::V1_0::MvnoType;
using PreferredNetworkType_V1_0 =
  ::android::hardware::radio::V1_0::PreferredNetworkType;
using RadioResponseType_V1_0 =
  ::android::hardware::radio::V1_0::RadioResponseType;
using RadioTechnology_V1_0 = ::android::hardware::radio::V1_0::RadioTechnology;
using RadioAccessFamily_BF_V1_0 = ::android::hardware::hidl_bitfield<
  ::android::hardware::radio::V1_0::RadioAccessFamily>;
using SelectUiccSub_V1_0 = ::android::hardware::radio::V1_0::SelectUiccSub;
using SmsAcknowledgeFailCause_V1_0 =
  ::android::hardware::radio::V1_0::SmsAcknowledgeFailCause;
using SubscriptionType_V1_0 =
  ::android::hardware::radio::V1_0::SubscriptionType;
using TtyMode_V1_0 = ::android::hardware::radio::V1_0::TtyMode;
using UiccSubActStatus_V1_0 =
  ::android::hardware::radio::V1_0::UiccSubActStatus;
using UusInfo_V1_0 = ::android::hardware::radio::V1_0::UusInfo;
using IRadio_V1_0 = ::android::hardware::radio::V1_0::IRadio;
using GsmBroadcastSmsConfigInfo_V1_0 =
  ::android::hardware::radio::V1_0::GsmBroadcastSmsConfigInfo;
using NetworkScanRequest_V1_1 =
  ::android::hardware::radio::V1_1::NetworkScanRequest;
using RadioAccessNetworks_V1_1 =
  ::android::hardware::radio::V1_1::RadioAccessNetworks;
using RadioAccessSpecifier_V1_1 =
  ::android::hardware::radio::V1_1::RadioAccessSpecifier;
using ScanType_V1_1 = ::android::hardware::radio::V1_1::ScanType;
using KeepaliveRequest_V1_1 =
  ::android::hardware::radio::V1_1::KeepaliveRequest;
using ::android::hidl::base::V1_0::IBase;
#if ANDROID_VERSION >= 33
using ::android::hidl::safe_union::V1_0::Monostate;
// For IRadio interface v1.2
using NetworkScanRequest_V1_2 =
  ::android::hardware::radio::V1_2::NetworkScanRequest;
using DataRequestReason_V1_2 =
  android::hardware::radio::V1_2::DataRequestReason;
using AccessNetwork_V1_2 = android::hardware::radio::V1_2::AccessNetwork;

// For Iradio interface v1.4
using ::android::hardware::radio::V1_1::ImsiEncryptionInfo;
using IRadio_V1_4 = ::android::hardware::radio::V1_4::IRadio;
using AccessNetwork_V1_4 = android::hardware::radio::V1_4::AccessNetwork;
using CarrierRestrictionsWithPriority_V1_4 =
  android::hardware::radio::V1_4::CarrierRestrictionsWithPriority;
using SimLockMultiSimPolicy_V1_4 =
  android::hardware::radio::V1_4::SimLockMultiSimPolicy;
using Carrier_V1_4 = android::hardware::radio::V1_0::Carrier;
using DataProfileInfo_V1_4 = android::hardware::radio::V1_4::DataProfileInfo;
using EmergencyCallRouting_V1_4 =
  ::android::hardware::radio::V1_4::EmergencyCallRouting;
using RadioAccessFamily_BF_V1_4 = ::android::hardware::hidl_bitfield<
  ::android::hardware::radio::V1_4::RadioAccessFamily>;
using CarrierMatchType_V1_4 =
  ::android::hardware::radio::V1_0::CarrierMatchType;
using PdpProtocolType_V_4 = ::android::hardware::radio::V1_4::PdpProtocolType;

// For Iradio interface v1.5
using IRadio_V1_5 = ::android::hardware::radio::V1_5::IRadio;
using DataProfileInfo_V1_5 = ::android::hardware::radio::V1_5::DataProfileInfo;
using LinkAddress_V1_5 = ::android::hardware::radio::V1_5::LinkAddress;
using AddressProperty_V1_5 = ::android::hardware::radio::V1_5::AddressProperty;
using RadioAccessNetworks_V1_5 =
  ::android::hardware::radio::V1_5::RadioAccessNetworks;
using AccessNetwork_V1_5 = ::android::hardware::radio::V1_5::AccessNetwork;
using SignalThresholdInfo_V1_5 =
  ::android::hardware::radio::V1_5::SignalThresholdInfo;
using PersoSubstate_V1_5 = ::android::hardware::radio::V1_5::PersoSubstate;
using UtranBands_V1_5 = ::android::hardware::radio::V1_5::UtranBands;
using NgranBands_V1_5 = ::android::hardware::radio::V1_5::NgranBands;
using EutranBands_V1_5 = ::android::hardware::radio::V1_5::EutranBands;
using NetworkScanRequest_V1_5 =
  ::android::hardware::radio::V1_5::NetworkScanRequest;

// For Iradio 1.6
using PublicKeyType_V1_6 = ::android::hardware::radio::V1_6::PublicKeyType;
using IRadio_V1_6 = ::android::hardware::radio::V1_6::IRadio;
using NrDualConnectivityState_V1_6 =
  ::android::hardware::radio::V1_6::NrDualConnectivityState;
using NgranBands_V1_6 = ::android::hardware::radio::V1_6::NgranBands;
using OptionalTrafficDescriptor_V1_6 =
  ::android::hardware::radio::V1_6::OptionalTrafficDescriptor;
using SliceInfo_V1_6 = ::android::hardware::radio::V1_6::SliceInfo;
using sliceinfo_hidl_discriminator_V1_6 =
  ::android::hardware::radio::V1_6::OptionalSliceInfo::hidl_discriminator;
using traffic_hidl_discriminator_V1_6 = ::android::hardware::radio::V1_6::
  OptionalTrafficDescriptor::hidl_discriminator;
using Monostate_V1_6 = ::android::hidl::safe_union::V1_0::Monostate;
using SliceServiceType_V1_6 =
  ::android::hardware::radio::V1_6::SliceServiceType;
using SliceStatus_V1_6 = ::android::hardware::radio::V1_6::SliceStatus;
using TrafficDescriptor_V1_6 =
  ::android::hardware::radio::V1_6::TrafficDescriptor;
using OptionalDnn_V1_6 = ::android::hardware::radio::V1_6::OptionalDnn;
using OptionalOsAppId_V1_6 = ::android::hardware::radio::V1_6::OptionalOsAppId;
using OsAppId_V1_6 = ::android::hardware::radio::V1_6::OsAppId;
#else
using IRadio_V1_1 = ::android::hardware::radio::V1_1::IRadio;
#endif

static bool gRilDebug_isLoggingEnabled = false;

typedef enum
{
  HAL_VERSION_V1_0,
  HAL_VERSION_V1_1,
  HAL_VERSION_V1_2,
  HAL_VERSION_V1_3,
  HAL_VERSION_V1_4,
  HAL_VERSION_V1_5,
  HAL_VERSION_V1_6,
  AIDL_VERSION
} HAL_VERIOSN;

#if ANDROID_VERSION >= 33
enum RadioAccessNetworkTypes
{
  UNKNOWN = (uint32_t)RadioAccessNetworks_V1_5::UNKNOWN,
  GERAN = (uint32_t)RadioAccessNetworks_V1_5::GERAN,
  UTRAN = (uint32_t)RadioAccessNetworks_V1_5::UTRAN,
  EUTRAN = (uint32_t)RadioAccessNetworks_V1_5::EUTRAN,
  NGRAN = (uint32_t)RadioAccessNetworks_V1_5::NGRAN,
  CDMA2000 = (uint32_t)RadioAccessNetworks_V1_5::CDMA2000
};
#endif

class nsRilWorker;

class nsRilWorker final : public nsIRilWorker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRILWORKER

  explicit nsRilWorker(uint32_t aClientId);

  struct RadioProxyDeathRecipient : public hidl_death_recipient
  {
    virtual void serviceDied(uint64_t cookie,
                             const ::android::wp<IBase>& who) override;
  };

  void GetRadioProxy();
#if ANDROID_VERSION >= 33
  void getRadioServiceProxy();
#endif
  void processIndication(RadioIndicationType_V1_0 indicationType);
  void processResponse(RadioResponseType_V1_0 responseType);
  void sendAck();
  void sendRilIndicationResult(nsRilIndicationResult* aIndication);
  void sendRilResponseResult(nsRilResponseResult* aResponse);
  void updateDebug();
  nsCOMPtr<nsIRilCallback> mRilCallback;

private:
  ~nsRilWorker();
  int32_t mClientId;
#if ANDROID_VERSION >= 33
  sp<IRadio_V1_6> mRadioProxy;
  sp<IRadioResponse_V1_6> mRilResponse;
  RefPtr<nsIRadioProxyServiceManager> mRadioProxyServiceManager;
  sp<IRadioIndication_V1_6> mRilIndication;
#else
  sp<IRadio_V1_1> mRadioProxy;
  sp<IRadioResponse> mRilResponse;
  sp<IRadioIndication> mRilIndication;
#endif
  HAL_VERIOSN mHalVerison;
  sp<RadioProxyDeathRecipient> mDeathRecipient;
  MvnoType_V1_0 convertToHalMvnoType(const nsAString& mvnoType);
  DataProfileInfo_V1_0 convertToHalDataProfile(nsIDataProfile* profile);
  RadioAccessSpecifier_V1_1 convertToHalRadioAccessSpecifier(
    nsIRadioAccessSpecifier* specifier);
#if ANDROID_VERSION >= 33
  DataProfileInfo_V1_4 convertToHalDataProfile_V1_4(nsIDataProfile* profile);
  DataProfileInfo_V1_5 convertToHalDataProfile_V1_5(nsIDataProfile* profile);
  RadioAccessSpecifier_V1_5 convertToHalRadioAccessSpecifier_1_5(
    nsIRadioAccessSpecifier* specifier);
  LinkAddress_V1_5 convertToHalLinkAddress_V1_5(nsILinkAddress* linkAddress);
  OptionalSliceInfo_V1_6 convertToHalSliceInfo_V1_6(nsISliceInfo* sliceinfo);
  OptionalTrafficDescriptor_V1_6 convertToHalTrafficDescriptor_V1_6(
    nsITrafficDescriptor* trafficdes);
#endif
};
#endif
