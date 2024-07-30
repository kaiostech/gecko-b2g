/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilResult_H
#define nsRilResult_H

#include "nsCOMArray.h"
#include "nsIRilResult.h"
#include "nsISupports.h"
#include "nsRadioTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#if ANDROID_VERSION >= 33
#include <aidl/android/hardware/radio/config/BnRadioConfigIndication.h>
#include <aidl/android/hardware/radio/config/BnRadioConfigResponse.h>
#include <aidl/android/hardware/radio/config/IRadioConfig.h>
#include <aidl/android/hardware/radio/data/BnRadioDataIndication.h>
#include <aidl/android/hardware/radio/data/BnRadioDataResponse.h>
#include <aidl/android/hardware/radio/data/IRadioData.h>
#include <android/hardware/radio/1.6/IRadio.h>
#else
#include <android/hardware/radio/1.1/IRadio.h>
#endif
#include <cstdint>
#include <nsISupportsImpl.h>

using ::android::sp;
using ::android::hardware::hidl_vec;
using CdmaSignalStrength_V1_0 =
  ::android::hardware::radio::V1_0::CdmaSignalStrength;
using CellIdentity_V1_0 = ::android::hardware::radio::V1_0::CellIdentity;
using CellIdentityCdma_V1_0 =
  ::android::hardware::radio::V1_0::CellIdentityCdma;
using CellIdentityGsm_V1_0 = ::android::hardware::radio::V1_0::CellIdentityGsm;
using CellIdentityLte_V1_0 = ::android::hardware::radio::V1_0::CellIdentityLte;
using CellIdentityTdscdma_V1_0 =
  ::android::hardware::radio::V1_0::CellIdentityTdscdma;
using CellIdentityWcdma_V1_0 =
  ::android::hardware::radio::V1_0::CellIdentityWcdma;
using CellInfo_V1_0 = ::android::hardware::radio::V1_0::CellInfo;
using CellInfoCdma_V1_0 = ::android::hardware::radio::V1_0::CellInfoCdma;
using CellInfoGsm_V1_0 = ::android::hardware::radio::V1_0::CellInfoGsm;
using CellInfoLte_V1_0 = ::android::hardware::radio::V1_0::CellInfoLte;
using CellInfoTdscdma_V1_0 = ::android::hardware::radio::V1_0::CellInfoTdscdma;
using DataProfileInfo_V1_0 = ::android::hardware::radio::V1_0::DataProfileInfo;
using DataCallFailCause_V1_0 =
  ::android::hardware::radio::V1_0::DataCallFailCause;
using ::android::hardware::radio::V1_0::EvdoSignalStrength;
using ::android::hardware::radio::V1_0::GsmSignalStrength;
using ::android::hardware::radio::V1_0::LteSignalStrength;
using RadioTechnology_V1_0 = ::android::hardware::radio::V1_0::RadioTechnology;
using SetupDataCallResult_V1_0 =
  ::android::hardware::radio::V1_0::SetupDataCallResult;
using ::android::hardware::radio::V1_0::SignalStrength;
using ::android::hardware::radio::V1_0::TdScdmaSignalStrength;
using ::android::hardware::radio::V1_0::TimeStampType;
using ::android::hardware::radio::V1_0::WcdmaSignalStrength;
using CellInfoWcdma_V1_0 = ::android::hardware::radio::V1_0::CellInfoWcdma;
using ::android::hardware::radio::V1_0::CellInfoType;

#if ANDROID_VERSION >= 33
using CellConnectionStatus_V1_2 =
  ::android::hardware::radio::V1_2::CellConnectionStatus;
using AudioQuality_V1_2 = ::android::hardware::radio::V1_2::AudioQuality;
using IRadioCellIdentity_V1_2 = android::hardware::radio::V1_2::CellIdentity;
using IRadioCellIdentityCdma_V1_2 =
  android::hardware::radio::V1_2::CellIdentityCdma;
using IRadioCellIdentityGsm_V1_2 =
  android::hardware::radio::V1_2::CellIdentityGsm;
using IRadioCellIdentityLte_V1_2 =
  android::hardware::radio::V1_2::CellIdentityLte;
using IRadioCellIdentityTdscdma_V1_2 =
  android::hardware::radio::V1_2::CellIdentityTdscdma;
using IRadioCellIdentityWcdma_V1_2 =
  android::hardware::radio::V1_2::CellIdentityWcdma;
using IRadioCellInfo_V1_2 = android::hardware::radio::V1_2::CellInfo;
using IRadioCellInfoCdma_V1_2 = android::hardware::radio::V1_2::CellInfoCdma;
using IRadioCellInfoGsm_V1_2 = android::hardware::radio::V1_2::CellInfoGsm;
using IRadioCellInfoLte_V1_2 = android::hardware::radio::V1_2::CellInfoLte;
using IRadioCellInfoTdscdma_V1_2 =
  android::hardware::radio::V1_2::CellInfoTdscdma;
using IRadioCellInfoWcdma_V1_2 = android::hardware::radio::V1_2::CellInfoWcdma;
using IRadioSignalStrength_V1_2 =
  android::hardware::radio::V1_2::SignalStrength;
using IRadioWcdmaSignalStrength_V1_2 =
  android::hardware::radio::V1_2::WcdmaSignalStrength;
using IRadioTdScdmaSignalStrength_V1_2 =
  android::hardware::radio::V1_2::TdscdmaSignalStrength;
using CellIdentityOperatorNames_V1_2 =
  android::hardware::radio::V1_2::CellIdentityOperatorNames;
using ISignalStrength_V1_4 = android::hardware::radio::V1_4::SignalStrength;
using LteVopsInfo_V1_4 = ::android::hardware::radio::V1_4::LteVopsInfo;
using NrIndicators_V1_4 = ::android::hardware::radio::V1_4::NrIndicators;
using SetupDataCallResult_V1_4 =
  ::android::hardware::radio::V1_4::SetupDataCallResult;
using hidl_discriminator_V1_4 =
  ::android::hardware::radio::V1_4::CellInfo::Info::hidl_discriminator;
using IRadioCellInfoNr_V1_4 = ::android::hardware::radio::V1_4::CellInfoNr;
using IRadioCellIdentityNr_V1_4 =
  ::android::hardware::radio::V1_4::CellIdentityNr;
using IRadioNrSignalStrength_V1_4 =
  ::android::hardware::radio::V1_4::NrSignalStrength;
using IRadioCellConfigLte_V1_4 =
  ::android::hardware::radio::V1_4::CellConfigLte;

using IRadioCellInfoLte_V1_4 = ::android::hardware::radio::V1_4::CellInfoLte;
using IRadioCellInfo_V1_4 = ::android::hardware::radio::V1_4::CellInfo;
using RadioTechnology_V1_4 = ::android::hardware::radio::V1_4::RadioTechnology;
using DataCallFailCause_V1_4 =
  ::android::hardware::radio::V1_4::DataCallFailCause;
using CellIdentity_V1_5 = ::android::hardware::radio::V1_5::CellIdentity;
using SetupDataCallResult_V1_5 =
  ::android::hardware::radio::V1_5::SetupDataCallResult;
using LinkAddress_V1_5 = ::android::hardware::radio::V1_5::LinkAddress;
using AppStatus_V1_5 = ::android::hardware::radio::V1_5::AppStatus;
using CellIdentityNr_V1_5 = android::hardware::radio::V1_5::CellIdentityNr;
using CellIdentityGsm_V1_5 = android::hardware::radio::V1_5::CellIdentityGsm;
using CellIdentityLte_V1_5 = android::hardware::radio::V1_5::CellIdentityLte;
using CellIdentityTdscdma_V1_5 =
  android::hardware::radio::V1_5::CellIdentityTdscdma;
using CellIdentityWcdma_V1_5 =
  android::hardware::radio::V1_5::CellIdentityWcdma;
using CellType_V1_5 =
  ::android::hardware::radio::V1_5::CellIdentity::hidl_discriminator;
using OptionalCsgInfo_V1_5 = ::android::hardware::radio::V1_5::OptionalCsgInfo;
using CellInfo_V1_5 = ::android::hardware::radio::V1_5::CellInfo;
using CellInfoType_V1_5 = ::android::hardware::radio::V1_5::CellInfo::
  CellInfoRatSpecificInfo::hidl_discriminator;
using CellInfoGsm_V1_5 = ::android::hardware::radio::V1_5::CellInfoGsm;
using CellInfoWcdma_V1_5 = ::android::hardware::radio::V1_5::CellInfoWcdma;
using CellInfoTdscdma_V1_5 = ::android::hardware::radio::V1_5::CellInfoTdscdma;
using CellInfoLte_V1_5 = ::android::hardware::radio::V1_5::CellInfoLte;
using CellInfoNr_V1_5 = ::android::hardware::radio::V1_5::CellInfoNr;
using IRadioCellInfo_V1_6 = ::android::hardware::radio::V1_6::CellInfo;
using PhonebookCapacity_V1_6 =
  ::android::hardware::radio::V1_6::PhonebookCapacity;
using SlicingConfig_V1_6 = ::android::hardware::radio::V1_6::SlicingConfig;
using UrspRule_V1_6 = ::android::hardware::radio::V1_6::UrspRule;
using SliceInfo_V1_6 = ::android::hardware::radio::V1_6::SliceInfo;
using TrafficDescriptor_V1_6 =
  ::android::hardware::radio::V1_6::TrafficDescriptor;
using RouteSelectionDescriptor_V1_6 =
  ::android::hardware::radio::V1_6::RouteSelectionDescriptor;
using OptionalSscMode_V1_6 = android::hardware::radio::V1_6::OptionalSscMode;
using OptionalPdpProtocolType_V1_6 =
  android::hardware::radio::V1_6::OptionalPdpProtocolType;
using OptionalOsAppId_V1_6 = android::hardware::radio::V1_6::OptionalOsAppId;
using OptionalDnn_V1_6 = android::hardware::radio::V1_6::OptionalDnn;
using OptionalSliceInfo_V1_6 =
  android::hardware::radio::V1_6::OptionalSliceInfo;
using Qos_V1_6 = ::android::hardware::radio::V1_6::Qos;
using EpsQos_V1_6 = ::android::hardware::radio::V1_6::EpsQos;
using NrQos_V1_6 = ::android::hardware::radio::V1_6::NrQos;
using QosBandwidth_V1_6 = ::android::hardware::radio::V1_6::QosBandwidth;
using QosSession_V1_6 = ::android::hardware::radio::V1_6::QosSession;
using QosFilter_V1_6 = ::android::hardware::radio::V1_6::QosFilter;
using MaybePort_V1_6 = ::android::hardware::radio::V1_6::MaybePort;
using PortRange_V1_6 = ::android::hardware::radio::V1_6::PortRange;
using TypeOfService_V1_6 =
  ::android::hardware::radio::V1_6::QosFilter::TypeOfService;
using Ipv6FlowLabel_V1_6 =
  ::android::hardware::radio::V1_6::QosFilter::Ipv6FlowLabel;
using IpsecSpi_V1_6 = ::android::hardware::radio::V1_6::QosFilter::IpsecSpi;
using SetupDataCallResult_V1_6 =
  ::android::hardware::radio::V1_6::SetupDataCallResult;
using DataCallFailCause_V1_6 =
  ::android::hardware::radio::V1_6::DataCallFailCause;
using NrVopsInfo = ::android::hardware::radio::V1_6::NrVopsInfo;
using ISignalStrength_V1_6 = android::hardware::radio::V1_6::SignalStrength;
using LteSignalStrength_V1_6 =
  ::android::hardware::radio::V1_6::LteSignalStrength;
using NrSignalStrength_V1_6 =
  ::android::hardware::radio::V1_6::NrSignalStrength;
using CellInfo_V1_6 = ::android::hardware::radio::V1_6::CellInfo;
using CellInfoType_V1_6 = ::android::hardware::radio::V1_6::CellInfo::
  CellInfoRatSpecificInfo::hidl_discriminator;
using CellInfoLte_V1_6 = ::android::hardware::radio::V1_6::CellInfoLte;
using CellInfoNr_V1_6 = ::android::hardware::radio::V1_6::CellInfoNr;
using OsAppId_V1_6 = ::android::hardware::radio::V1_6::OsAppId;
using OptionalDnn_V1_6 = ::android::hardware::radio::V1_6::OptionalDnn;
// aidl
using namespace aidl::android::hardware::radio;
using namespace aidl::android::hardware::radio::data;
using namespace aidl::android::hardware::radio::config;
using TrafficDescriptorA =
  aidl::android::hardware::radio::data::TrafficDescriptor;
using QosSessionA = aidl::android::hardware::radio::data::QosSession;
using QosA = aidl::android::hardware::radio::data::Qos;
using QosBandwidthA = aidl::android::hardware::radio::data::QosBandwidth;
using EpsQosA = aidl::android::hardware::radio::data::EpsQos;
using NrQosA = aidl::android::hardware::radio::data::NrQos;
using QosFilterA = aidl::android::hardware::radio::data::QosFilter;
using PortRangeA = aidl::android::hardware::radio::data::PortRange;
using SliceInfoA = aidl::android::hardware::radio::data::SliceInfo;
using UrspRuleA = aidl::android::hardware::radio::data::UrspRule;
using RouteSelectionDescriptorA =
  aidl::android::hardware::radio::data::RouteSelectionDescriptor;
using SimSlotStatusA = aidl::android::hardware::radio::config::SimSlotStatus;
using SimPortInfoA = aidl::android::hardware::radio::config::SimPortInfo;
#endif
class nsRilResult;
class nsSimPortInfo final : public nsISimPortInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIMPORTINFO
  nsSimPortInfo(nsAString& aIccId, int32_t aLogicalSlotId, bool aPortActive);
#if ANDROID_VERSION >= 33
  nsSimPortInfo(const SimPortInfoA& aPortInfo);
#endif
private:
  ~nsSimPortInfo(){};
  nsString mIccId;
  int32_t mLogicalSlotId;
  bool mPortActive;
};

class nsSimSlotStatus final : public nsISimSlotStatus
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIMSLOTSTATUS
  nsSimSlotStatus(int32_t aCardState,
                  nsAString& aAtr,
                  nsAString& aEid,
                  nsTArray<RefPtr<nsISimPortInfo>>& aPortInfo);
#if ANDROID_VERSION >= 33
  nsSimSlotStatus(const SimSlotStatusA& aStatus);
#endif
private:
  ~nsSimSlotStatus(){};
  int32_t mCardState;
  nsString mAtr;
  nsString mEid;
  nsTArray<RefPtr<nsISimPortInfo>> mPortInfo;
};

class nsBarringTypeSpecificInfo final : public nsIBarringTypeSpecificInfo
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBARRINGTYPESPECIFICINFO
  nsBarringTypeSpecificInfo(int32_t aFactor,
                            int32_t aTimeSeconds,
                            bool aIsBarred);

private:
  ~nsBarringTypeSpecificInfo(){};

  int32_t factor;
  int32_t timeSeconds;
  bool isBarred;
};

class nsBarringInfo final : public nsIBarringInfo
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBARRINGINFO
#if ANDROID_VERSION >= 33
  nsBarringInfo(const BarringInfo_V1_5& aInfo);
#endif
  nsBarringInfo(int32_t aServiceType,
                int32_t aBarringType,
                int32_t aFactor,
                int32_t aTimeSeconds,
                bool aIsBarred);

private:
  ~nsBarringInfo();

  int32_t serviceType;
  int32_t barringType;
  RefPtr<nsIBarringTypeSpecificInfo> barringTypeSpecificInfo;
};

class nsSliceInfo final : public nsISliceInfo
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISLICEINFO
  nsSliceInfo(int32_t aSst,
              int32_t aSliceDifferentiator,
              int32_t aMappedHplmnSst,
              int32_t aMappedHplmnSD,
              int32_t aStatus);
#if ANDROID_VERSION >= 33
  nsSliceInfo(const SliceInfo_V1_6& aSliceInfo);
  nsSliceInfo(const SliceInfoA& aSliceInfo);
#endif
private:
  ~nsSliceInfo(){};

  /**
   * The type of service provided by the slice.
   *
   * see: 3GPP TS 24.501 Section 9.11.2.8.
   */
  int32_t sst;

  /**
   * Slice differentiator is the identifier of a slice that has
   * SliceServiceType as SST. A value of -1 indicates that there is
   * no corresponding SliceInfo of the HPLMN.
   *
   * see: 3GPP TS 24.501 Section 9.11.2.8.
   */
  int32_t sliceDifferentiator;

  /**
   * This SST corresponds to a SliceInfo (S-NSSAI) of the HPLMN; the SST is
   * mapped to this value.
   *
   * see: 3GPP TS 24.501 Section 9.11.2.8.
   */
  int32_t mappedHplmnSst;

  /**
   * Present only if both sliceDifferentiator and mappedHplmnSst are also
   * present. This SD corresponds to a SliceInfo (S-NSSAI) of the HPLMN;
   * sliceDifferentiator is mapped to this value. A value of -1 indicates that
   * there is no corresponding SliceInfo of the HPLMN.
   *
   * see: 3GPP TS 24.501 Section 9.11.2.8.
   */
  int32_t mappedHplmnSD;

  /**
   * Field to indicate the current status of the slice.
   */
  int32_t status;
};

class nsQosBandwidth final : public nsIQosBandwidth
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQOSBANDWIDTH
  nsQosBandwidth(int32_t aMaxBitrateKbps, int32_t aGuaranteedBitrateKbps);
#if ANDROID_VERSION >= 33
  nsQosBandwidth(const QosBandwidth_V1_6& halData);
  nsQosBandwidth(const QosBandwidthA& halData);
#endif
private:
  ~nsQosBandwidth(){};
  int32_t maxBitrateKbps;
  int32_t guaranteedBitrateKbps;
};

class nsEpsQos final : public nsIEpsQos
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEPSQOS
  nsEpsQos(int32_t aQci,
           RefPtr<nsIQosBandwidth> aDow,
           RefPtr<nsIQosBandwidth> aUp);
#if ANDROID_VERSION >= 33
  nsEpsQos(const EpsQos_V1_6& halEpsQos);
  nsEpsQos(const EpsQosA& halEpsQos);
#endif
private:
  ~nsEpsQos(){};
  int32_t qci;
  RefPtr<nsIQosBandwidth> downlink;
  RefPtr<nsIQosBandwidth> uplink;
};

class nsNrQos final : public nsINrQos
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSINRQOS
  nsNrQos(int32_t aFiveQi,
          int32_t aQfi,
          int32_t aAver,
          RefPtr<nsIQosBandwidth> aDow,
          RefPtr<nsIQosBandwidth> aUP);
#if ANDROID_VERSION >= 33
  nsNrQos(const NrQos_V1_6& halNrQos);
  nsNrQos(const NrQosA& halNrQos);
#endif

private:
  ~nsNrQos(){};
  int32_t fiveQi;
  int32_t qfi;
  int32_t averagingWindowMs;
  RefPtr<nsIQosBandwidth> downlink;
  RefPtr<nsIQosBandwidth> uplink;
};

class nsQos final : public nsIQos
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQOS
  nsQos(int32_t aType, RefPtr<nsIEpsQos> aEps, RefPtr<nsINrQos> aNr);
#if ANDROID_VERSION >= 33
  nsQos(const Qos_V1_6& halQos);
  nsQos(const QosA& aParm);
#endif
private:
  ~nsQos(){};

  int32_t type;
  RefPtr<nsIEpsQos> eps;
  RefPtr<nsINrQos> nr;
};

class nsPortRange final : public nsIPortRange
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPORTRANGE
  nsPortRange(int32_t aStart, int32_t aEnd);
#if ANDROID_VERSION >= 33
  nsPortRange(const PortRange_V1_6& aRange);
  nsPortRange(const PortRangeA& aRange);
#endif

private:
  ~nsPortRange(){};
  int32_t start;
  int32_t end;
};

class nsQosFilter final : public nsIQosFilter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQOSFILTER
  nsQosFilter(int32_t aDirection,
              int32_t aPrecedence,
              int32_t aProtocol,
              int32_t aTos,
              int32_t aFlowLabel,
              int32_t aSpi,
              nsTArray<nsAString>& aLocalAddr,
              nsTArray<nsAString>& aRemoteAddr,
              RefPtr<nsIPortRange> aLocalPort,
              RefPtr<nsIPortRange> aRemotePort);
#if ANDROID_VERSION >= 33
  nsQosFilter(const QosFilter_V1_6& aFilter);
  nsQosFilter(const QosFilterA& aFilter);
#endif

private:
  ~nsQosFilter(){};

  int32_t direction;
  int32_t precedence;
  int32_t protocol;
  int32_t tos;
  int32_t flowLabel;
  int32_t spi;
  nsTArray<nsString> localAddresses;
  nsTArray<nsString> remoteAddresses;
  RefPtr<nsIPortRange> localPort;
  RefPtr<nsIPortRange> remotePort;
};

class nsQosSession final : public nsIQosSession
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQOSSESSION
  nsQosSession(int32_t aId,
               RefPtr<nsIQos> aQos,
               nsTArray<RefPtr<nsIQosFilter>>& aFilters);
#if ANDROID_VERSION >= 33
  nsQosSession(const QosSession_V1_6& halData);
  nsQosSession(const QosSessionA& halData);
#endif

private:
  ~nsQosSession(){};

  int32_t qosSessionId;
  RefPtr<nsIQos> qos;
  nsTArray<RefPtr<nsIQosFilter>> qosFilters;
};

class nsGetBarringInfoResult final : public nsIGetBarringInfoResult
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGETBARRINGINFORESULT
  nsGetBarringInfoResult(RefPtr<nsICellIdentity> aCellIdentity,
                         nsTArray<RefPtr<nsIBarringInfo>>& aBarringInfoList);

private:
  ~nsGetBarringInfoResult(){};
  RefPtr<nsICellIdentity> mCellIdentity;
  nsTArray<RefPtr<nsIBarringInfo>> mBarringInfos;
};

class nsRouteSelectionDescriptor final : public nsIRouteSelectionDescriptor
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIROUTESELECTIONDESCRIPTOR
  nsRouteSelectionDescriptor(int32_t aPrecedence,
                             int32_t aSessionType,
                             int32_t aSscMode,
                             nsTArray<RefPtr<nsISliceInfo>>& aSliceInfo,
                             nsTArray<nsString>& aDnn);
#if ANDROID_VERSION >= 33
  nsRouteSelectionDescriptor(
    RouteSelectionDescriptor_V1_6& aRouteSelectionDescriptor);
  nsRouteSelectionDescriptor(
    const RouteSelectionDescriptorA& aRouteSelectionDescriptor);
#endif

private:
  ~nsRouteSelectionDescriptor(){};

  int32_t precedence;
  int32_t sessionType;
  int32_t sscMode;
  nsTArray<RefPtr<nsISliceInfo>> sliceInfo;
  nsTArray<nsString> dnn;
};

class nsTrafficDescriptor final : public nsITrafficDescriptor
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRAFFICDESCRIPTOR
  nsTrafficDescriptor(nsAString& aDnn, const nsTArray<int32_t>& aOsAppId);
#if ANDROID_VERSION >= 33
  nsTrafficDescriptor(const TrafficDescriptor_V1_6& aParm);
  nsTrafficDescriptor(const TrafficDescriptorA& aParm);
#endif

private:
  ~nsTrafficDescriptor(){};
  /**
   * DNN stands for Data Network Name and represents an APN as defined in
   * 3GPP TS 23.003.
   */
  nsString mDnn;

  /**
   * Indicates the OsId + OsAppId (used as category in Android).
   */
  nsTArray<int32_t> mOsAppId;
};

class nsUrspRule final : public nsIUrspRule
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURSPRULE
  nsUrspRule(
    int32_t aPrecedence,
    nsTArray<RefPtr<nsITrafficDescriptor>>& aTrafficDescriptors,
    nsTArray<RefPtr<nsIRouteSelectionDescriptor>>& aRouteSelectionDescriptors);
#if ANDROID_VERSION >= 33
  nsUrspRule(const UrspRule_V1_6& aRule);
  nsUrspRule(const UrspRuleA& aRule);
#endif

private:
  ~nsUrspRule(){};
  /**
   * Precedence value in the range of 0 to 255. Higher value has lower
   * precedence.
   */
  int32_t precedence;
  /**
   * Used as a matcher for network requests.
   */
  nsTArray<RefPtr<nsITrafficDescriptor>> trafficDescriptors;
  /**
   * List of routes (connection parameters) that must be used for requests
   * matching a trafficDescriptor.
   */
  nsTArray<RefPtr<nsIRouteSelectionDescriptor>> routeSelectionDescriptor;
};

class nsSlicingConfig final : public nsISlicingConfig
{

  NS_DECL_ISUPPORTS
  NS_DECL_NSISLICINGCONFIG

  nsSlicingConfig(nsTArray<RefPtr<nsIUrspRule>>& aRules,
                  nsTArray<RefPtr<nsISliceInfo>>& aSliceInfos);
#if ANDROID_VERSION >= 33
  nsSlicingConfig(const SlicingConfig_V1_6& aConfig);
#endif
private:
  ~nsSlicingConfig(){};
  nsTArray<RefPtr<nsIUrspRule>> urspRules;
  nsTArray<RefPtr<nsISliceInfo>> sliceInfo;
};

class nsPhonebookCapacity final : public nsIPhonebookCapacity
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPHONEBOOKCAPACITY
  nsPhonebookCapacity(int32_t aMaxAdnRecords,
                      int32_t aUsedAdnRecords,
                      int32_t aMaxEmailRecords,
                      int32_t aUsedEmailRecords,
                      int32_t aMaxAdditionalNumberRecords,
                      int32_t aUsedAdditionalNumberRecords,
                      int32_t aMaxNameLen,
                      int32_t aMaxNumberLen,
                      int32_t aMaxEmailLen,
                      int32_t aMaxAdditionalNumberLen);
#if ANDROID_VERSION >= 33
  nsPhonebookCapacity(const PhonebookCapacity_V1_6& aParam);
#endif
private:
  ~nsPhonebookCapacity(){};
  /**
   * Maximum number of ADN records possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxAdnRecords;

  /**
   * Used ADN records in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t usedAdnRecords;

  /**
   * Maximum email records possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxEmailRecords;

  /**
   * Used email records in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t usedEmailRecords;

  /**
   * Maximum additional number records possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxAdditionalNumberRecords;

  /**
   * Used additional number records in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t usedAdditionalNumberRecords;

  /**
   * Maximum name length possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxNameLen;

  /**
   * Maximum number length possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxNumberLen;

  /**
   * Maximum email length possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxEmailLen;

  /**
   * Maximum additional number length possible in the SIM phonebook
   * Needs to be non-negative
   */
  int32_t maxAdditionalNumberLen;
};

class nsSupplySimDepersonalizationResult final
  : public nsISupplySimDepersonalizationResult
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPLYSIMDEPERSONALIZATIONRESULT

  nsSupplySimDepersonalizationResult(int32_t aType, int32_t aRemainingRetries);

private:
  ~nsSupplySimDepersonalizationResult(){};
  int32_t mPersoType;
  int32_t mRemainingRetries;
};

class nsKeepAliveStatus final : public nsIKeepAliveStatus
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEEPALIVESTATUS
  nsKeepAliveStatus(int32_t aHandle, int32_t aStatus);

private:
  ~nsKeepAliveStatus(){};

  int32_t mKeepAliveStatusCode;
  int32_t mSessionHandle;
};

class nsGsmSignalStrength final : public nsIGsmSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIGSMSIGNALSTRENGTH
  nsGsmSignalStrength(int32_t aSignalStrength,
                      int32_t aBitErrorRate,
                      int32_t aTimingAdvance);

private:
  ~nsGsmSignalStrength(){};
  int32_t mSignalStrength;
  int32_t mBitErrorRate;
  int32_t mTimingAdvance;
};

class nsWcdmaSignalStrength final : public nsIWcdmaSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWCDMASIGNALSTRENGTH
  nsWcdmaSignalStrength(int32_t aSignalStrength,
                        int32_t aBitErrorRate,
                        int32_t aRscp,
                        int32_t aEcno);

private:
  ~nsWcdmaSignalStrength(){};
  int32_t mSignalStrength;
  int32_t mBitErrorRate;
  int32_t mRscp;
  int32_t mEcno;
};

class nsCdmaSignalStrength final : public nsICdmaSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICDMASIGNALSTRENGTH
  nsCdmaSignalStrength(int32_t aDbm, int32_t aEcio);

private:
  ~nsCdmaSignalStrength(){};
  int32_t mDbm;
  int32_t mEcio;
};

class nsEvdoSignalStrength final : public nsIEvdoSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVDOSIGNALSTRENGTH
  nsEvdoSignalStrength(int32_t aDbm, int32_t aEcio, int32_t aSignalNoiseRatio);

private:
  ~nsEvdoSignalStrength(){};
  int32_t mDbm;
  int32_t mEcio;
  int32_t mSignalNoiseRatio;
};

class nsLteSignalStrength final : public nsILteSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILTESIGNALSTRENGTH
  nsLteSignalStrength(int32_t aSignalStrength,
                      int32_t aRsrp,
                      int32_t aRsrq,
                      int32_t aRssnr,
                      int32_t aCqi,
                      int32_t aTimingAdvance,
                      uint32_t aCqiTableIndex);

private:
  ~nsLteSignalStrength(){};
  int32_t mSignalStrength;
  int32_t mRsrp;
  int32_t mRsrq;
  int32_t mRssnr;
  int32_t mCqi;
  int32_t mTimingAdvance;
  uint32_t mCqiTableIndex;
};

class nsCellConfigLte final : public nsICellConfigLte
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLCONFIGLTE
  nsCellConfigLte(bool isEndcAvailable);

private:
  ~nsCellConfigLte(){};
  bool mIsEndcAvailable;
};

class nsTdScdmaSignalStrength final : public nsITdScdmaSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITDSCDMASIGNALSTRENGTH
  explicit nsTdScdmaSignalStrength(int32_t aSignalStrength,
                                   int32_t aBitErrorRate,
                                   int32_t aRscp);

private:
  ~nsTdScdmaSignalStrength(){};
  int32_t mSignalStrength;
  int32_t mBitErrorRate;
  int32_t mRscp;
};

class nsNrSignalStrength final : public nsINrSignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINRSIGNALSTRENGTH
  explicit nsNrSignalStrength(int32_t csiRsrp,
                              int32_t csiRsrq,
                              int32_t csiSinr,
                              int32_t ssRsrp,
                              int32_t ssRsrq,
                              int32_t ssSinr,
                              uint32_t aCsiCqiTableIndex,
                              const nsTArray<int32_t>& aCsiCqiReport);

private:
  ~nsNrSignalStrength(){};
  int32_t mCsiRsrp;
  int32_t mCsiRsrq;
  int32_t mCsiSinr;
  int32_t mSsRsrp;
  int32_t mSsRsrq;
  int32_t mSsSinr;
  uint32_t mCsiCqiTableIndex;
  nsTArray<int32_t> mCsiCqiReport;
};

class nsSignalStrength final : public nsISignalStrength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIGNALSTRENGTH
  nsSignalStrength(nsGsmSignalStrength* aGsmSignalStrength,
                   nsCdmaSignalStrength* aCdmaSignalStrength,
                   nsEvdoSignalStrength* aEvdoSignalStrength,
                   nsLteSignalStrength* aLteSignalStrength,
                   nsTdScdmaSignalStrength* aTdScdmaSignalStrength,
                   nsWcdmaSignalStrength* aWcdmaSignalStrength,
                   nsNrSignalStrength* aNrSignalStrength);

private:
  ~nsSignalStrength(){};
  RefPtr<nsGsmSignalStrength> mGsmSignalStrength;
  RefPtr<nsCdmaSignalStrength> mCdmaSignalStrength;
  RefPtr<nsEvdoSignalStrength> mEvdoSignalStrength;
  RefPtr<nsLteSignalStrength> mLteSignalStrength;
  RefPtr<nsTdScdmaSignalStrength> mTdScdmaSignalStrength;
  RefPtr<nsWcdmaSignalStrength> mWcdmaSignalStrength;
  RefPtr<nsNrSignalStrength> mNrSignalStrength;
};

class nsLinkAddress final : public nsILinkAddress
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILINKADDRESS

  nsLinkAddress(const nsAString& aAddress,
                int32_t aProperties,
                uint64_t aDeprecationTime,
                uint64_t aExpirationTime);

private:
  ~nsLinkAddress(){};
  nsString mAddress;
  int32_t mProperties;
  uint64_t mDeprecationTime;
  uint64_t mExpirationTime;
};

class nsSetupDataCallResult final : public nsISetupDataCallResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISETUPDATACALLRESULT
  nsSetupDataCallResult(int32_t aFailCause,
                        int64_t aSuggestedRetryTime,
                        int32_t aCid,
                        int32_t aActive,
                        const nsAString& aPdpType,
                        const nsAString& aIfname,
                        const nsTArray<RefPtr<nsLinkAddress>>& aAddresses,
                        const nsAString& aDnses,
                        const nsAString& aGateways,
                        const nsAString& aPcscf,
                        int32_t aMtu,
                        int32_t aMtuV4,
                        int32_t aMtuV6);
  nsSetupDataCallResult(
    int32_t aFailCause,
    int64_t aSuggestedRetryTime,
    int32_t aCid,
    int32_t aActive,
    const nsAString& aPdpType,
    const nsAString& aIfname,
    const nsTArray<RefPtr<nsLinkAddress>>& aAddresses,
    const nsAString& aDnses,
    const nsAString& aGateways,
    const nsAString& aPcscf,
    int32_t aMtu,
    int32_t aMtuV4,
    int32_t aMtuV6,
    int32_t aPduSessionId,
    int32_t aHFailureMode,
    RefPtr<nsISliceInfo> aSliceInfo,
    RefPtr<nsIQos> aQos,
    nsTArray<RefPtr<nsITrafficDescriptor>>& aTrafficDescriptors,
    nsTArray<RefPtr<nsIQosSession>>& aQosSessions);

private:
  ~nsSetupDataCallResult(){};
  int32_t mFailCause;
  int64_t mSuggestedRetryTime;
  int32_t mCid;
  int32_t mActive;
  nsString mPdpType;
  nsString mIfname;
  nsTArray<RefPtr<nsLinkAddress>> mAddresses;
  nsString mDnses;
  nsString mGateways;
  nsString mPcscf;
  int32_t mMtu;
  int32_t mMtuV4;
  int32_t mMtuV6;

  int32_t pduSessionId;
  int32_t handoverFailureMode;
  RefPtr<nsISliceInfo> sliceInfo;
  RefPtr<nsIQos> defaultQos;
  nsTArray<RefPtr<nsITrafficDescriptor>> trafficDescriptors;
  nsTArray<RefPtr<nsIQosSession>> qosSessions;
};

class nsSuppSvcNotification final : public nsISuppSvcNotification
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISUPPSVCNOTIFICATION
  nsSuppSvcNotification(bool aNotificationType,
                        int32_t aCode,
                        int32_t aIndex,
                        int32_t aType,
                        const nsAString& aNumber);

private:
  ~nsSuppSvcNotification(){};
  bool mNotificationType;
  int32_t mCode;
  int32_t mIndex;
  int32_t mType;
  nsString mNumber;
};

class nsSimRefreshResult final : public nsISimRefreshResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIMREFRESHRESULT
  nsSimRefreshResult(int32_t aType, int32_t aEfId, const nsAString& aAid);

private:
  ~nsSimRefreshResult(){};
  int32_t mType;
  int32_t mEfId;
  nsString mAid;
};

class nsCellIdentityOperatorNames final : public nsICellIdentityOperatorNames
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYOPERATORNAMES
  nsCellIdentityOperatorNames(const nsAString& aAlphaLong,
                              const nsAString& aAlphaShort);
#if ANDROID_VERSION >= 33
  nsCellIdentityOperatorNames(CellIdentityOperatorNames_V1_2& aMessage);
#endif
private:
  ~nsCellIdentityOperatorNames(){};
  nsString mAlphaLong;
  nsString mAlphaShort;
};

class nsCellIdentityCsgInfo final : public nsICellIdentityCsgInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYCSGINFO
  nsCellIdentityCsgInfo(bool aCsgIndication,
                        const nsAString& aHomeNodebName,
                        int32_t aCsgIdentity);

private:
  ~nsCellIdentityCsgInfo(){};
  bool mCsgIndication;
  nsString mHomeNodebName;
  int32_t mCsgIdentity;
};

class nsCellIdentityGsm final : public nsICellIdentityGsm
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYGSM
  nsCellIdentityGsm(const nsAString& aMcc,
                    const nsAString& aMnc,
                    int32_t aLac,
                    int32_t aCid,
                    int32_t aArfcn,
                    int32_t aBsic,
                    nsCellIdentityOperatorNames* aOperatorNames,
                    nsTArray<nsString>& aAdditionalPlmns);

private:
  ~nsCellIdentityGsm(){};
  nsString mMcc;
  nsString mMnc;
  int32_t mLac;
  int32_t mCid;
  int32_t mArfcn;
  int32_t mBsic;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
  nsTArray<nsString> mAdditionalPlmns;
};

class nsCellIdentityCdma final : public nsICellIdentityCdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYCDMA
  nsCellIdentityCdma(int32_t aNetworkId,
                     int32_t aSystemId,
                     int32_t aBaseStationId,
                     int32_t aLongitude,
                     int32_t aLatitude,
                     nsCellIdentityOperatorNames* aOperatorNames);

private:
  ~nsCellIdentityCdma(){};
  int32_t mNetworkId;
  int32_t mSystemId;
  int32_t mBaseStationId;
  int32_t mLongitude;
  int32_t mLatitude;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
};

class nsCellIdentityLte final : public nsICellIdentityLte
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYLTE
  nsCellIdentityLte(const nsAString& aMcc,
                    const nsAString& aMnc,
                    int32_t aCi,
                    int32_t aPci,
                    int32_t aTac,
                    int32_t aEarfcn,
                    nsCellIdentityOperatorNames* aOperatorNames,
                    int32_t aBandwidth,
                    nsTArray<nsString>& aAdditionalPlmns,
                    nsCellIdentityCsgInfo* aCsgInfo,
                    nsTArray<int32_t>& aBands);

private:
  ~nsCellIdentityLte(){};
  nsString mMcc;
  nsString mMnc;
  int32_t mCi;
  int32_t mPci;
  int32_t mTac;
  int32_t mEarfcn;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
  int32_t mBandwidth;
  nsTArray<nsString> mAdditionalPlmns;
  RefPtr<nsCellIdentityCsgInfo> mCsgInfo;
  nsTArray<int32_t> mBands;
};

class nsCellIdentityWcdma final : public nsICellIdentityWcdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYWCDMA
  nsCellIdentityWcdma(const nsAString& aMcc,
                      const nsAString& aMnc,
                      int32_t aLac,
                      int32_t aCid,
                      int32_t aPsc,
                      int32_t aUarfcn,
                      nsCellIdentityOperatorNames* aOperatorNames,
                      nsTArray<nsString>& aAdditionalPlmns,
                      nsCellIdentityCsgInfo* aCsgInfo);

private:
  ~nsCellIdentityWcdma(){};
  nsString mMcc;
  nsString mMnc;
  int32_t mLac;
  int32_t mCid;
  int32_t mPsc;
  int32_t mUarfcn;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
  nsTArray<nsString> mAdditionalPlmns;
  RefPtr<nsCellIdentityCsgInfo> mCsgInfo;
};

class nsCellIdentityTdScdma final : public nsICellIdentityTdScdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYTDSCDMA
  nsCellIdentityTdScdma(const nsAString& aMcc,
                        const nsAString& aMnc,
                        int32_t aLac,
                        int32_t aCid,
                        int32_t aCpid,
                        nsCellIdentityOperatorNames* aOperatorNames,
                        int32_t aUarfcn,
                        nsTArray<nsString>& aAdditionalPlmns,
                        nsCellIdentityCsgInfo* aCsgInfo);

private:
  ~nsCellIdentityTdScdma(){};
  nsString mMcc;
  nsString mMnc;
  int32_t mLac;
  int32_t mCid;
  int32_t mCpid;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
  int32_t mUarfcn;
  nsTArray<nsString> mAdditionalPlmns;
  RefPtr<nsCellIdentityCsgInfo> mCsgInfo;
};

class nsCellIdentityNr final : public nsICellIdentityNr
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITYNR
  nsCellIdentityNr(const nsAString& aMcc,
                   const nsAString& aMnc,
                   uint64_t aNci,
                   uint32_t aPci,
                   int32_t aTac,
                   int32_t aNrarfcn,
                   nsCellIdentityOperatorNames* aOperatorNames,
                   nsTArray<nsString>& aAdditionalPlmns,
                   nsTArray<int32_t>& aBands);

private:
  ~nsCellIdentityNr(){};
  nsString mMcc;
  nsString mMnc;
  uint64_t mNci;
  uint32_t mPci;
  int32_t mTac;
  int32_t mNrarfcn;
  RefPtr<nsCellIdentityOperatorNames> mOperatorNames;
  nsTArray<nsString> mAdditionalPlmns;
  nsTArray<int32_t> mBands;
};

class nsCellIdentity final : public nsICellIdentity
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLIDENTITY
  nsCellIdentity(int32_t aCellInfoType,
                 nsCellIdentityGsm* aCellIdentityGsm,
                 nsCellIdentityWcdma* aCellIdentityWcdma,
                 nsCellIdentityCdma* aCellIdentityCdma,
                 nsCellIdentityLte* aCellIdentityLte,
                 nsCellIdentityTdScdma* aCellIdentityTdScdma);
  nsCellIdentity(int32_t aCellInfoType, nsCellIdentityGsm* aCellIdentityGsm);
  nsCellIdentity(int32_t aCellInfoType,
                 nsCellIdentityWcdma* aCellIdentityWcdma);
  nsCellIdentity(int32_t aCellInfoType, nsCellIdentityCdma* aCellIdentityCdma);
  nsCellIdentity(int32_t aCellInfoType, nsCellIdentityLte* aCellIdentityLte);
  nsCellIdentity(int32_t aCellInfoType,
                 nsCellIdentityTdScdma* aCellIdentityTdScdma);
  nsCellIdentity(int32_t aCellInfoType, nsCellIdentityNr* aCellIdentityNr);

private:
  ~nsCellIdentity(){};

  int32_t mCellInfoType;
  RefPtr<nsCellIdentityGsm> mCellIdentityGsm;
  RefPtr<nsCellIdentityWcdma> mCellIdentityWcdma;
  RefPtr<nsCellIdentityCdma> mCellIdentityCdma;
  RefPtr<nsCellIdentityLte> mCellIdentityLte;
  RefPtr<nsCellIdentityTdScdma> mCellIdentityTdScdma;
  RefPtr<nsCellIdentityNr> mCellIdentityNr;
};

class nsCellInfoGsm final : public nsICellInfoGsm
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOGSM
  nsCellInfoGsm(nsCellIdentityGsm* aCellIdentityGsm,
                nsGsmSignalStrength* aSignalStrengthGsm);

private:
  ~nsCellInfoGsm(){};
  RefPtr<nsCellIdentityGsm> mCellIdentityGsm;
  RefPtr<nsGsmSignalStrength> mSignalStrengthGsm;
};

class nsCellInfoCdma final : public nsICellInfoCdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOCDMA
  nsCellInfoCdma(nsCellIdentityCdma* aCellIdentityCdma,
                 nsCdmaSignalStrength* aSignalStrengthCdma,
                 nsEvdoSignalStrength* aSignalStrengthEvdo);

private:
  ~nsCellInfoCdma(){};
  RefPtr<nsCellIdentityCdma> mCellIdentityCdma;
  RefPtr<nsCdmaSignalStrength> mSignalStrengthCdma;
  RefPtr<nsEvdoSignalStrength> mSignalStrengthEvdo;
};

class nsCellInfoLte final : public nsICellInfoLte
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOLTE
  nsCellInfoLte(nsCellIdentityLte* aCellIdentityLte,
                nsLteSignalStrength* aSignalStrengthLte,
                nsCellConfigLte* aCellConfigLte);

private:
  ~nsCellInfoLte(){};
  RefPtr<nsCellIdentityLte> mCellIdentityLte;
  RefPtr<nsLteSignalStrength> mSignalStrengthLte;
  RefPtr<nsCellConfigLte> mCellConfigLte;
};

class nsCellInfoWcdma final : public nsICellInfoWcdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOWCDMA
  nsCellInfoWcdma(nsCellIdentityWcdma* aCellIdentityWcdma,
                  nsWcdmaSignalStrength* aSignalStrengthWcdma);

private:
  ~nsCellInfoWcdma(){};
  RefPtr<nsCellIdentityWcdma> mCellIdentityWcdma;
  RefPtr<nsWcdmaSignalStrength> mSignalStrengthWcdma;
};

class nsCellInfoTdScdma final : public nsICellInfoTdScdma
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOTDSCDMA
  nsCellInfoTdScdma(nsCellIdentityTdScdma* aCellIdentityTdScdma,
                    nsTdScdmaSignalStrength* aSignalStrengthTdScdma);

private:
  ~nsCellInfoTdScdma(){};
  RefPtr<nsCellIdentityTdScdma> mCellIdentityTdScdma;
  RefPtr<nsTdScdmaSignalStrength> mSignalStrengthTdScdma;
};

class nsCellInfoNr final : public nsICellInfoNr
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFONR
  nsCellInfoNr(nsCellIdentityNr* aCellIdentityNr,
               nsNrSignalStrength* aSignalStrengthNr);

private:
  ~nsCellInfoNr(){};
  RefPtr<nsCellIdentityNr> mCellIdentityNr;
  RefPtr<nsNrSignalStrength> mSignalStrengthNr;
};

class nsRilCellInfo final : public nsIRilCellInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRILCELLINFO
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoGsm* aGsm,
                nsCellInfoCdma* aCdma,
                nsCellInfoLte* aLte,
                nsCellInfoWcdma* aWcdma,
                nsCellInfoTdScdma* aTdScdma,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoGsm* aGsm,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoWcdma* aWcdma,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoCdma* aCdma,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoLte* aLte,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoTdScdma* aTdScdma,
                int32_t aConnectionStatus);
  nsRilCellInfo(int32_t aCellInfoType,
                bool aRegistered,
                int32_t aTimeStampType,
                uint64_t aTimeStamp,
                nsCellInfoNr* aNr,
                int32_t aConnectionStatus);

private:
  ~nsRilCellInfo(){};
  int32_t mCellInfoType;
  bool mRegistered;
  int32_t mTimeStampType;
  uint64_t mTimeStamp;
  RefPtr<nsCellInfoGsm> mGsm;
  RefPtr<nsCellInfoCdma> mCdma;
  RefPtr<nsCellInfoLte> mLte;
  RefPtr<nsCellInfoWcdma> mWcdma;
  RefPtr<nsCellInfoTdScdma> mTdScdma;
  RefPtr<nsCellInfoNr> mNr;
  int32_t mConnectionStatus;
};

class nsHardwareConfig final : public nsIHardwareConfig
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHARDWARECONFIG
  nsHardwareConfig(int32_t aType,
                   const nsAString& aUuid,
                   int32_t aState,
                   nsIHardwareConfigModem* aModem,
                   nsIHardwareConfigSim* aSim);

private:
  ~nsHardwareConfig(){};
  int32_t mType;
  nsString mUuid;
  int32_t mState;
  RefPtr<nsIHardwareConfigModem> mModem;
  RefPtr<nsIHardwareConfigSim> mSim;
};

class nsHardwareConfigModem final : public nsIHardwareConfigModem
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHARDWARECONFIGMODEM
  nsHardwareConfigModem(int32_t aRilModel,
                        int32_t aRat,
                        int32_t aMaxVoice,
                        int32_t aMaxData,
                        int32_t aMaxStandby);

private:
  ~nsHardwareConfigModem(){};
  int32_t mRilModel;
  int32_t mRat;
  int32_t mMaxVoice;
  int32_t mMaxData;
  int32_t mMaxStandby;
};

class nsHardwareConfigSim final : public nsIHardwareConfigSim
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHARDWARECONFIGSIM
  explicit nsHardwareConfigSim(const nsAString& aModemUuid);

private:
  ~nsHardwareConfigSim(){};
  nsString mModemUuid;
};

class nsRadioCapability final : public nsIRadioCapability
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRADIOCAPABILITY
  nsRadioCapability(int32_t aSession,
                    int32_t aPhase,
                    int32_t aRaf,
                    const nsAString& aLogicalModemUuid,
                    int32_t aStatus);

private:
  ~nsRadioCapability(){};
  int32_t mSession;
  int32_t mPhase;
  int32_t mRaf;
  nsString mLogicalModemUuid;
  int32_t mStatus;
};

class nsLceStatusInfo final : public nsILceStatusInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILCESTATUSINFO
  nsLceStatusInfo(int32_t aLceStatus, int32_t aActualIntervalMs);

private:
  ~nsLceStatusInfo(){};
  int32_t mLceStatus;
  int32_t mActualIntervalMs;
};

class nsLceDataInfo final : public nsILceDataInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILCEDATAINFO
  nsLceDataInfo(int32_t aLastHopCapacityKbps,
                int32_t aConfidenceLevel,
                bool aLceSuspended);

private:
  ~nsLceDataInfo(){};
  int32_t mLastHopCapacityKbps;
  int32_t mConfidenceLevel;
  bool mLceSuspended;
};

class nsPcoDataInfo final : public nsIPcoDataInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPCODATAINFO
  nsPcoDataInfo(int32_t aCid,
                const nsAString& aBearerProto,
                bool aPcoId,
                nsTArray<int32_t>& aContents);

private:
  ~nsPcoDataInfo(){};
  int32_t mCid;
  nsString mBearerProto;
  bool mPcoId;
  nsTArray<int32_t> mContents;
};

class nsAppStatus final : public nsIAppStatus
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAPPSTATUS
  nsAppStatus(int32_t aAppType,
              int32_t aAppState,
              int32_t aPersoSubstate,
              const nsAString& aAidPtr,
              const nsAString& aAppLabelPtr,
              int32_t aPin1Replaced,
              int32_t aPin1,
              int32_t aPin2);

private:
  ~nsAppStatus(){};
  int32_t mAppType;
  int32_t mAppState;
  int32_t mPersoSubstate;
  nsString mAidPtr;
  nsString mAppLabelPtr;
  int32_t mPin1Replaced;
  int32_t mPin1;
  int32_t mPin2;
};

class nsCardStatus final : public nsICardStatus
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICARDSTATUS
  nsCardStatus(int32_t aCardState,
               int32_t aUniversalPinState,
               int32_t aGsmUmtsSubscriptionAppIndex,
               int32_t aCdmaSubscriptionAppIndex,
               int32_t aImsSubscriptionAppIndex,
               nsTArray<RefPtr<nsAppStatus>>& aApplications,
               int32_t aPhysicalSlotId,
               const nsAString& aAtr,
               const nsAString& aIccid,
               const nsAString& aEid);

private:
  ~nsCardStatus(){};
  int32_t mCardState;
  int32_t mUniversalPinState;
  int32_t mGsmUmtsSubscriptionAppIndex;
  int32_t mCdmaSubscriptionAppIndex;
  int32_t mImsSubscriptionAppIndex;
  nsTArray<RefPtr<nsAppStatus>> mApplications;
  int32_t mPhysicalSlotId;
  nsString mAtr;
  nsString mIccid;
  nsString mEid;
};

class nsVoiceRegState final : public nsIVoiceRegState
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVOICEREGSTATE
  nsVoiceRegState(int32_t aRegState,
                  int32_t aRat,
                  bool aCssSupported,
                  int32_t aRoamingIndicator,
                  int32_t aSystemIsInPrl,
                  int32_t aDefaultRoamingIndicator,
                  int32_t aReasonForDenial,
                  nsCellIdentity* aCellIdentity,
                  const nsAString& aRegisteredPlmn);

private:
  ~nsVoiceRegState(){};
  int32_t mRegState;
  int32_t mRat;
  bool mCssSupported;
  int32_t mRoamingIndicator;
  int32_t mSystemIsInPrl;
  int32_t mDefaultRoamingIndicator;
  int32_t mReasonForDenial;
  RefPtr<nsCellIdentity> mCellIdentity;
  nsString mRegisteredPlmn;
};

class nsNrIndicators final : public nsINrIndicators
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINRINDICATORS
  nsNrIndicators(bool isEndcAvailable,
                 bool isDcNrRestricted,
                 bool isNrAvailable);

private:
  ~nsNrIndicators(){};
  bool mIsEndcAvailable;
  bool mIsDcNrRestricted;
  bool mIsNrAvailable;
};

class nsLteVopsInfo final : public nsILteVopsInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILTEVOPSINFO
  nsLteVopsInfo(bool isVopsSupported, bool isEmcBearerSupported);

private:
  ~nsLteVopsInfo(){};
  bool mIsVopsSupported;
  bool mIsEmcBearerSupported;
};

class nsNrVopsInfo final : public nsINrVopsInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINRVOPSINFO
  nsNrVopsInfo(int32_t aVopsSupported,
               int32_t aEmcSupported,
               int32_t aEmfSupported);

private:
  ~nsNrVopsInfo(){};
  int32_t mVopsSupported;
  int32_t mEmcSupported;
  int32_t mEmfSupported;
};

class nsDataRegState final : public nsIDataRegState
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDATAREGSTATE
  nsDataRegState(int32_t aRegState,
                 int32_t aRat,
                 int32_t aReasonDataDenied,
                 int32_t aMaxDataCalls,
                 nsCellIdentity* aCellIdentity,
                 nsLteVopsInfo* aVopsInfo,
                 nsNrIndicators* aNrIndicators,
                 const nsAString& aRegisteredPlmn,
                 nsNrVopsInfo* aNrVopsInfo);

private:
  ~nsDataRegState(){};
  int32_t mRegState;
  int32_t mRat;
  int32_t mReasonDataDenied;
  int32_t mMaxDataCalls;
  RefPtr<nsCellIdentity> mCellIdentity;
  RefPtr<nsLteVopsInfo> mVopsInfo;
  RefPtr<nsNrIndicators> mNrIndicators;
  nsString mRegisteredPlmn;
  RefPtr<nsNrVopsInfo> mNrVopsInfo;
};

class nsSendSmsResult final : public nsISendSmsResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISENDSMSRESULT
  nsSendSmsResult(int32_t aMessageRef,
                  const nsAString& aAckPDU,
                  int32_t aErrorCode);

private:
  ~nsSendSmsResult(){};
  int32_t mMessageRef;
  nsString mAckPDU;
  int32_t mErrorCode;
};

class nsOperatorInfo final : public nsIOperatorInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOPERATORINFO
  nsOperatorInfo(const nsAString& aAlphaLong,
                 const nsAString& aAlphaShort,
                 const nsAString& aOperatorNumeric,
                 int32_t aStatus);

private:
  ~nsOperatorInfo(){};
  nsString mAlphaLong;
  nsString mAlphaShort;
  nsString mOperatorNumeric;
  int32_t mStatus;
};

class nsNeighboringCell final : public nsINeighboringCell
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINEIGHBORINGCELL
  nsNeighboringCell(const nsAString& aCid, int32_t aRssi);

private:
  ~nsNeighboringCell(){};
  nsString mCid;
  int32_t mRssi;
};

class nsUusInfo final : public nsIUusInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUUSINFO
  nsUusInfo(int32_t aUusType, int32_t aUusDcs, const nsAString& aUusData);

private:
  ~nsUusInfo(){};
  int32_t mUusType;
  int32_t mUusDcs;
  nsString mUusData;
};

class nsCall final : public nsICall
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICALL
  nsCall(int32_t aState,
         int32_t aIndex,
         int32_t aToa,
         bool aIsMpty,
         bool aIsMT,
         int32_t aAls,
         bool aIsVoice,
         bool aIsVoicePrivacy,
         const nsAString& aNumber,
         int32_t aNumberPresentation,
         const nsAString& aName,
         int32_t aNamePresentation,
         nsTArray<RefPtr<nsUusInfo>>& aUusInfo,
         int32_t aAudioQuality,
         const nsAString& aForwardedNumber);

private:
  ~nsCall(){};
  int32_t mState;
  int32_t mIndex;
  int32_t mToa;
  bool mIsMpty;
  bool mIsMT;
  int32_t mAls;
  bool mIsVoice;
  bool mIsVoicePrivacy;
  nsString mNumber;
  int32_t mNumberPresentation;
  nsString mName;
  int32_t mNamePresentation;
  nsTArray<RefPtr<nsUusInfo>> mUusInfo;
  int32_t mAudioQuality;
  nsString mForwardedNumber;
};

class nsIccIoResult final : public nsIIccIoResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIICCIORESULT
  nsIccIoResult(int32_t aSw1, int32_t aSw2, const nsAString& aSimResponse);

private:
  ~nsIccIoResult(){};
  int32_t mSw1;
  int32_t mSw2;
  nsString mSimResponse;
};

class nsCallForwardInfo final : public nsICallForwardInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICALLFORWARDINFO
  nsCallForwardInfo(int32_t aStatus,
                    int32_t aReason,
                    int32_t aServiceClass,
                    int32_t aToa,
                    const nsAString& aNumber,
                    int32_t aTimeSeconds);

private:
  ~nsCallForwardInfo(){};
  int32_t mStatus;
  int32_t mReason;
  int32_t mServiceClass;
  int32_t mToa;
  nsString mNumber;
  int32_t mTimeSeconds;
};

class nsLinkCapacityEstimate final : public nsILinkCapacityEstimate
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILINKCAPACITYESTIMATE
  nsLinkCapacityEstimate(int32_t aDownlinkCapacityKbps,
                         int32_t aUplinkCapacityKbps,
                         uint32_t aSecondaryDownlinkCapacityKbps = 0,
                         uint32_t aSecondaryUplinkCapacityKbps = 0);

private:
  ~nsLinkCapacityEstimate(){};
  int32_t mDownlinkCapacityKbps;
  int32_t mUplinkCapacityKbps;
  uint32_t mSecondaryDownlinkCapacityKbps;
  uint32_t mSecondaryUplinkCapacityKbps;
};

class nsEmergencyNumber final : public nsIEmergencyNumber
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEMERGENCYNUMBER
  nsEmergencyNumber(const nsAString& number,
                    const nsAString& mcc,
                    const nsAString& mnc,
                    uint32_t categories,
                    nsTArray<nsString>& urns,
                    uint32_t sources);

private:
  ~nsEmergencyNumber(){};
  nsString mNumber;
  nsString mMcc;
  nsString mMnc;
  uint32_t mCategories;
  nsTArray<nsString> mUrns;
  uint32_t mSources;
};

class nsPhysicalChannelConfig final : public nsIPhysicalChannelConfig
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPHYSICALCHANNELCONFIG
  nsPhysicalChannelConfig(const int32_t status,
                          const uint32_t cellBandwidthDownlink,
                          const int32_t rat,
                          const uint8_t discriminator,
                          const int32_t frequencyRange,
                          const int32_t channelNumber,
                          const nsTArray<int32_t>& contextIds,
                          const uint32_t physicalCellId,
                          int32_t aDownlinkChannelNumber,
                          int32_t aUplinkChannelNumber,
                          int32_t aCellBandwidthUplinkKhz,
                          int32_t aRan_discriminator,
                          int32_t aGeranBand,
                          int32_t aUtranBand,
                          int32_t aEutranBand,
                          int32_t aNgranBand);

private:
  ~nsPhysicalChannelConfig(){};
  int32_t mStatus;
  uint32_t mCellBandwidthDownlink;
  int32_t mRat;
  uint8_t mDiscriminator;
  int32_t mFrequencyRange;
  int32_t mChannelNumber;
  nsTArray<int32_t> mContextIds;
  uint32_t mPhysicalCellId;
  int32_t mDownlinkChannelNumber;
  int32_t mUplinkChannelNumber;
  int32_t mCellBandwidthUplinkKhz;
  int32_t mRan_discriminator;
  int32_t mGeranBand;
  int32_t mUtranBand;
  int32_t mEutranBand;
  int32_t mNgranBand;
};

class nsBarringInfoChanged final : public nsIBarringInfoChanged
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIBARRINGINFOCHANGED
  nsBarringInfoChanged(nsCellIdentity* aCellIdentity,
                       nsTArray<RefPtr<nsBarringInfo>>& aBarringInfos);

private:
  ~nsBarringInfoChanged(){};
  RefPtr<nsCellIdentity> mCellIdentity;
  nsTArray<RefPtr<nsBarringInfo>> mBarringInfos;
};

class nsRegistrationFailedEvent final : public nsIRegistrationFailedEvent
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREGISTRATIONFAILEDEVENT
  nsRegistrationFailedEvent(nsCellIdentity* aCellIdentity,
                            const nsAString& aChosenPlmn,
                            int32_t aDomain,
                            int32_t aCauseCode,
                            int32_t aAdditionalCauseCode);

private:
  ~nsRegistrationFailedEvent(){};
  RefPtr<nsCellIdentity> mCellIdentity;
  nsString mChosenPlmn;
  int32_t mDomain;
  int32_t mCauseCode;
  int32_t mAdditionalCauseCode;
};

class nsAllowedCarriers final : public nsIAllowedCarriers
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIALLOWEDCARRIERS
  nsAllowedCarriers(nsCarrierRestrictionsWithPriority* crp,
                    const int32_t simLockMultiSimPolicy);

private:
  ~nsAllowedCarriers(){};
  RefPtr<nsCarrierRestrictionsWithPriority> mCrp;
  int32_t mSimLockMultiSimPolicy;
};

class nsNetworkScanResult final : public nsINetworkScanResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINETWORKSCANRESULT
  nsNetworkScanResult(const int32_t status,
                      const int32_t error,
                      nsTArray<RefPtr<nsRilCellInfo>>& networkInfos);

private:
  ~nsNetworkScanResult(){};
  int32_t mStatus;
  int32_t mError;
  nsTArray<RefPtr<nsRilCellInfo>> mNetworkInfos;
};

class nsRilResult
{
public:
  explicit nsRilResult(const nsAString& aRilMessageType);
  nsRilResult(const nsAString& aRilMessageType,
              int32_t aRilMessageToken,
              int32_t aErrorMsg);
  RefPtr<nsRilCellInfo> convertRilCellInfo(const CellInfo_V1_0* aCellInfo);
  RefPtr<nsCellInfoGsm> convertCellInfoGsm(
    const CellInfoGsm_V1_0* aCellInfoGsm);
  RefPtr<nsCellInfoCdma> convertCellInfoCdma(
    const CellInfoCdma_V1_0* aCellInfoCdma);
  RefPtr<nsCellInfoWcdma> convertCellInfoWcdma(
    const CellInfoWcdma_V1_0* aCellInfoWcdma);
  RefPtr<nsCellInfoLte> convertCellInfoLte(
    const CellInfoLte_V1_0* aCellInfoLte);
  RefPtr<nsCellInfoTdScdma> convertCellInfoTdScdma(
    const CellInfoTdscdma_V1_0* aCellInfoTdscdma);

#if ANDROID_VERSION >= 33
  RefPtr<nsRilCellInfo> convertRilCellInfo_V1_2(
    const IRadioCellInfo_V1_2* aCellInfo);
  RefPtr<nsCellInfoGsm> convertCellInfoGsm_V1_2(
    const IRadioCellInfoGsm_V1_2* aCellInfoGsm);
  RefPtr<nsCellInfoCdma> convertCellInfoCdma_V1_2(
    const IRadioCellInfoCdma_V1_2* aCellInfoCdma);
  RefPtr<nsCellInfoWcdma> convertCellInfoWcdma_V1_2(
    const IRadioCellInfoWcdma_V1_2* aCellInfoWcdma);
  RefPtr<nsCellInfoLte> convertCellInfoLte_V1_2(
    const IRadioCellInfoLte_V1_2* aCellInfoLte);
  RefPtr<nsCellInfoTdScdma> convertCellInfoTdScdma_V1_2(
    const IRadioCellInfoTdscdma_V1_2* aCellInfoTdscdma);
#endif

  RefPtr<nsCellIdentity> convertCellIdentity(
    const CellIdentity_V1_0* aCellIdentity);
  RefPtr<nsCellIdentityGsm> convertCellIdentityGsm(
    const CellIdentityGsm_V1_0* aCellIdentityGsm);
  RefPtr<nsCellIdentityWcdma> convertCellIdentityWcdma(
    const CellIdentityWcdma_V1_0* aCellIdentityWcdma);
  RefPtr<nsCellIdentityCdma> convertCellIdentityCdma(
    const CellIdentityCdma_V1_0* aCellIdentityCdma);
  RefPtr<nsCellIdentityLte> convertCellIdentityLte(
    const CellIdentityLte_V1_0* aCellIdentityLte);
  RefPtr<nsCellIdentityTdScdma> convertCellIdentityTdScdma(
    const CellIdentityTdscdma_V1_0* aCellIdentityTdScdma);
#if ANDROID_VERSION >= 33
  RefPtr<nsCellIdentity> convertCellIdentity_V1_2(
    const IRadioCellIdentity_V1_2* aCellIdentity);
  RefPtr<nsCellIdentityGsm> convertCellIdentityGsm_V1_2(
    const IRadioCellIdentityGsm_V1_2* aCellIdentityGsm);
  RefPtr<nsCellIdentityWcdma> convertCellIdentityWcdma_V1_2(
    const IRadioCellIdentityWcdma_V1_2* aCellIdentityWcdma);
  RefPtr<nsCellIdentityCdma> convertCellIdentityCdma_V1_2(
    const IRadioCellIdentityCdma_V1_2* aCellIdentityCdma);
  RefPtr<nsCellIdentityLte> convertCellIdentityLte_V1_2(
    const IRadioCellIdentityLte_V1_2* aCellIdentityLte);
  RefPtr<nsCellIdentityTdScdma> convertCellIdentityTdScdma_V1_2(
    const IRadioCellIdentityTdscdma_V1_2* aCellIdentityTdScdma);
  static RefPtr<nsCellIdentityOperatorNames> convertCellIdentityOperatorNames(
    const CellIdentityOperatorNames_V1_2& aOperatorNames);
#endif
  RefPtr<nsSignalStrength> convertSignalStrength(
    const SignalStrength& aSignalStrength);
  RefPtr<nsGsmSignalStrength> convertGsmSignalStrength(
    const GsmSignalStrength& aGsmSignalStrength);
  RefPtr<nsWcdmaSignalStrength> convertWcdmaSignalStrength(
    const WcdmaSignalStrength& aWcdmaSignalStrength);
  RefPtr<nsCdmaSignalStrength> convertCdmaSignalStrength(
    const CdmaSignalStrength_V1_0& aCdmaSignalStrength);
  RefPtr<nsEvdoSignalStrength> convertEvdoSignalStrength(
    const EvdoSignalStrength& aEvdoSignalStrength);
  RefPtr<nsLteSignalStrength> convertLteSignalStrength(
    const LteSignalStrength& aLteSignalStrength);
  RefPtr<nsTdScdmaSignalStrength> convertTdScdmaSignalStrength(
    const TdScdmaSignalStrength& aTdScdmaSignalStrength);
  RefPtr<nsSetupDataCallResult> convertDcResponse(
    const SetupDataCallResult_V1_0& aDcResponse);
#if ANDROID_VERSION >= 33
  RefPtr<nsSignalStrength> convertSignalStrength_V1_2(
    const IRadioSignalStrength_V1_2& aSignalStrength);
  RefPtr<nsWcdmaSignalStrength> convertWcdmaSignalStrength_V1_2(
    const IRadioWcdmaSignalStrength_V1_2& aWcdmaSignalStrength);
  RefPtr<nsTdScdmaSignalStrength> convertTdScdmaSignalStrength_V1_2(
    const IRadioTdScdmaSignalStrength_V1_2& aTdScdmaSignalStrength);
#endif
  static int32_t convertRadioTechnology(RadioTechnology_V1_0 aRat);
  static int32_t convertDataCallFailCause(DataCallFailCause_V1_0 aCause);
  static int32_t convertCellInfoType(CellInfoType type);
  static int32_t convertTimeStampType(TimeStampType type);
  static int32_t convertCellTypeToRil(uint8_t type);
  static int32_t convertCellInfoTypeToRil(uint8_t type);
#if ANDROID_VERSION >= 33
  static int32_t convertConnectionStatus(CellConnectionStatus_V1_2 status);
  static int32_t convertAudioQuality(AudioQuality_V1_2 aType);
  RefPtr<nsNrSignalStrength> convertNrSignalStrength(
    const IRadioNrSignalStrength_V1_4* aNrSignalStrength);
  RefPtr<nsLteVopsInfo> convertVopsInfo(const LteVopsInfo_V1_4* aCellIdentity);
  RefPtr<nsNrIndicators> convertNrIndicators(
    const NrIndicators_V1_4* aCellIdentity);
  RefPtr<nsSetupDataCallResult> convertDcResponse_V1_4(
    const SetupDataCallResult_V1_4& aDcResponse);
  RefPtr<nsCellInfoLte> convertCellInfoLte_V1_4(
    const IRadioCellInfoLte_V1_4* aCellInfoLte);
  RefPtr<nsCellInfoNr> convertCellInfoNr(
    const IRadioCellInfoNr_V1_4* aCellInfoNr);
  RefPtr<nsCellIdentityNr> convertCellIdentityNr(
    const IRadioCellIdentityNr_V1_4* aCellIdentityNr);
  RefPtr<nsCellConfigLte> convertCellConfigLte(
    const IRadioCellConfigLte_V1_4& aCellConfig);
  RefPtr<nsRilCellInfo> convertRilCellInfo_V1_4(
    const IRadioCellInfo_V1_4* aCellInfo);
  RefPtr<nsSignalStrength> convertSignalStrength_V1_4(
    const ISignalStrength_V1_4& aSignalStrength);
  static int32_t convertRadioTechnology_V1_4(RadioTechnology_V1_4 aRat);
  nsString convertPdpProtocolType_V1_4(int32_t pdpType);
  static int32_t convertDataCallFailCause_1_4(DataCallFailCause_V1_4 aCause);
  RefPtr<nsSetupDataCallResult> convertDcResponse_V1_5(
    const SetupDataCallResult_V1_5& aDcResponse);
  RefPtr<nsLinkAddress> convertLinkAddress_V1_5(
    const LinkAddress_V1_5& aLinkaddress);
  RefPtr<nsCellIdentity> convertCellIdentity_V1_5(
    const CellIdentity_V1_5* aCellIdentity);
  RefPtr<nsCellIdentityGsm> convertCellIdentityGsm_V1_5(
    const CellIdentityGsm_V1_5* aCellIdentityGsm);
  RefPtr<nsCellIdentityWcdma> convertCellIdentityWcdma_V1_5(
    const CellIdentityWcdma_V1_5* aCellIdentityWcdma);
  RefPtr<nsCellIdentityNr> convertCellIdentityNr_V1_5(
    const CellIdentityNr_V1_5* aCellIdentityNr);
  RefPtr<nsCellIdentityLte> convertCellIdentityLte_V1_5(
    const CellIdentityLte_V1_5* aCellIdentityLte);
  RefPtr<nsCellIdentityTdScdma> convertCellIdentityTdScdma_V1_5(
    const CellIdentityTdscdma_V1_5* aCellIdentityTdScdma);
  RefPtr<nsCellIdentityCsgInfo> convertCsgInfo(
    const OptionalCsgInfo_V1_5* aCsgInfo);
  RefPtr<nsRilCellInfo> convertRilCellInfo_V1_5(const CellInfo_V1_5* aCellInfo);
  RefPtr<nsCellInfoGsm> convertCellInfoGsm_V1_5(
    const CellInfoGsm_V1_5* aCellInfoGsm);
  RefPtr<nsCellInfoWcdma> convertCellInfoWcdma_V1_5(
    const CellInfoWcdma_V1_5* aCellInfoWcdma);
  RefPtr<nsCellInfoLte> convertCellInfoLte_V1_5(
    const CellInfoLte_V1_5* aCellInfoLte);
  RefPtr<nsCellInfoNr> convertCellInfoNr_V1_5(
    const CellInfoNr_V1_5* aCellInfoNr);
  RefPtr<nsCellInfoTdScdma> convertCellInfoTdScdma_V1_5(
    const CellInfoTdscdma_V1_5* aCellInfoTdscdma);
  static int32_t convertCellInfoTypeToRil_V1_6(uint8_t type);
  RefPtr<nsSetupDataCallResult> convertDcResponse_V1_6(
    const SetupDataCallResult_V1_6& aDcResponse);
  static int32_t convertDataCallFailCause_1_6(DataCallFailCause_V1_6 aCause);
  RefPtr<nsNrVopsInfo> convertNrVopsInfo(const NrVopsInfo* aNrVopsInfo);
  RefPtr<nsSignalStrength> convertSignalStrength_V1_6(
    const ISignalStrength_V1_6& aSignalStrength);
  RefPtr<nsLteSignalStrength> convertLteSignalStrength_V1_6(
    const LteSignalStrength_V1_6& aLteSignalStrength);
  RefPtr<nsNrSignalStrength> convertNrSignalStrength_V1_6(
    const NrSignalStrength_V1_6* aNrSignalStrength);
  RefPtr<nsRilCellInfo> convertRilCellInfo_V1_6(const CellInfo_V1_6* aCellInfo);
  RefPtr<nsCellInfoLte> convertCellInfoLte_V1_6(
    const CellInfoLte_V1_6* aCellInfoLte);
  RefPtr<nsCellInfoNr> convertCellInfoNr_V1_6(
    const CellInfoNr_V1_6* aCellInfoNr);
#endif
  nsString mRilMessageType;
  int32_t mRilMessageToken;
  int32_t mErrorMsg;

  ~nsRilResult();
};
#endif // nsRilResult_H
